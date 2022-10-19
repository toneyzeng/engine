// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/entity/entity_pass.h"

#include <memory>
#include <variant>

#include "flutter/fml/logging.h"
#include "flutter/fml/macros.h"
#include "flutter/fml/trace_event.h"
#include "impeller/base/validation.h"
#include "impeller/entity/contents/content_context.h"
#include "impeller/entity/contents/filters/color_filter_contents.h"
#include "impeller/entity/contents/filters/inputs/filter_input.h"
#include "impeller/entity/contents/texture_contents.h"
#include "impeller/entity/entity.h"
#include "impeller/entity/inline_pass_context.h"
#include "impeller/geometry/path_builder.h"
#include "impeller/renderer/allocator.h"
#include "impeller/renderer/command.h"
#include "impeller/renderer/command_buffer.h"
#include "impeller/renderer/formats.h"
#include "impeller/renderer/render_pass.h"
#include "impeller/renderer/texture.h"

namespace impeller {

EntityPass::EntityPass() = default;

EntityPass::~EntityPass() = default;

void EntityPass::SetDelegate(std::unique_ptr<EntityPassDelegate> delegate) {
  if (!delegate) {
    return;
  }
  delegate_ = std::move(delegate);
}

void EntityPass::AddEntity(Entity entity) {
  if (entity.GetBlendMode() > Entity::kLastPipelineBlendMode) {
    reads_from_pass_texture_ += 1;
  }

  elements_.emplace_back(std::move(entity));
}

void EntityPass::SetElements(std::vector<Element> elements) {
  elements_ = std::move(elements);
}

size_t EntityPass::GetSubpassesDepth() const {
  size_t max_subpass_depth = 0u;
  for (const auto& element : elements_) {
    if (auto subpass = std::get_if<std::unique_ptr<EntityPass>>(&element)) {
      max_subpass_depth =
          std::max(max_subpass_depth, subpass->get()->GetSubpassesDepth());
    }
  }
  return max_subpass_depth + 1u;
}

const std::shared_ptr<LazyGlyphAtlas>& EntityPass::GetLazyGlyphAtlas() const {
  return lazy_glyph_atlas_;
}

std::optional<Rect> EntityPass::GetElementsCoverage(
    std::optional<Rect> coverage_crop) const {
  std::optional<Rect> result;
  for (const auto& element : elements_) {
    std::optional<Rect> coverage;

    if (auto entity = std::get_if<Entity>(&element)) {
      coverage = entity->GetCoverage();

      if (coverage.has_value() && coverage_crop.has_value()) {
        coverage = coverage->Intersection(coverage_crop.value());
      }
    } else if (auto subpass =
                   std::get_if<std::unique_ptr<EntityPass>>(&element)) {
      coverage = GetSubpassCoverage(*subpass->get(), coverage_crop);
    } else {
      FML_UNREACHABLE();
    }

    if (!result.has_value() && coverage.has_value()) {
      result = coverage;
      continue;
    }
    if (!coverage.has_value()) {
      continue;
    }
    result = result->Union(coverage.value());
  }
  return result;
}

std::optional<Rect> EntityPass::GetSubpassCoverage(
    const EntityPass& subpass,
    std::optional<Rect> coverage_clip) const {
  auto entities_coverage = subpass.GetElementsCoverage(coverage_clip);
  // The entities don't cover anything. There is nothing to do.
  if (!entities_coverage.has_value()) {
    return std::nullopt;
  }

  // The delegates don't have an opinion on what the entity coverage has to be.
  // Just use that as-is.
  auto delegate_coverage = subpass.delegate_->GetCoverageRect();
  if (!delegate_coverage.has_value()) {
    return entities_coverage;
  }
  // The delegate coverage hint is in given in local space, so apply the subpass
  // transformation.
  delegate_coverage = delegate_coverage->TransformBounds(subpass.xformation_);

  // If the delegate tells us the coverage is smaller than it needs to be, then
  // great. OTOH, if the delegate is being wasteful, limit coverage to what is
  // actually needed.
  return entities_coverage->Intersection(delegate_coverage.value());
}

EntityPass* EntityPass::GetSuperpass() const {
  return superpass_;
}

EntityPass* EntityPass::AddSubpass(std::unique_ptr<EntityPass> pass) {
  if (!pass) {
    return nullptr;
  }
  FML_DCHECK(pass->superpass_ == nullptr);
  pass->superpass_ = this;

  if (pass->blend_mode_ > Entity::kLastPipelineBlendMode ||
      pass->backdrop_filter_proc_.has_value()) {
    reads_from_pass_texture_ += 1;
  }

  auto subpass_pointer = pass.get();
  elements_.emplace_back(std::move(pass));
  return subpass_pointer;
}

static RenderTarget CreateRenderTarget(ContentContext& renderer,
                                       ISize size,
                                       bool readable) {
  auto context = renderer.GetContext();

  /// All of the load/store actions are managed by `InlinePassContext` when
  /// `RenderPasses` are created, so we just set them to `kDontCare` here.
  /// What's important is the `StorageMode` of the textures, which cannot be
  /// changed for the lifetime of the textures.

  if (context->SupportsOffscreenMSAA()) {
    return RenderTarget::CreateOffscreenMSAA(
        *context,                          // context
        size,                              // size
        "EntityPass",                      // label
        StorageMode::kDeviceTransient,     // color_storage_mode
        StorageMode::kDevicePrivate,       // color_resolve_storage_mode
        LoadAction::kDontCare,             // color_load_action
        StoreAction::kMultisampleResolve,  // color_store_action
        readable ? StorageMode::kDevicePrivate
                 : StorageMode::kDeviceTransient,  // stencil_storage_mode
        LoadAction::kDontCare,                     // stencil_load_action
        StoreAction::kDontCare                     // stencil_store_action
    );
  }

  return RenderTarget::CreateOffscreen(
      *context,                     // context
      size,                         // size
      "EntityPass",                 // label
      StorageMode::kDevicePrivate,  // color_storage_mode
      LoadAction::kDontCare,        // color_load_action
      StoreAction::kDontCare,       // color_store_action
      readable ? StorageMode::kDevicePrivate
               : StorageMode::kDeviceTransient,  // stencil_storage_mode
      LoadAction::kDontCare,                     // stencil_load_action
      StoreAction::kDontCare                     // stencil_store_action
  );
}

bool EntityPass::Render(ContentContext& renderer,
                        RenderTarget render_target) const {
  if (reads_from_pass_texture_ > 0) {
    auto offscreen_target =
        CreateRenderTarget(renderer, render_target.GetRenderTargetSize(), true);
    if (!OnRender(renderer, offscreen_target.GetRenderTargetSize(),
                  offscreen_target, Point(), Point(), 0)) {
      return false;
    }

    auto command_buffer = renderer.GetContext()->CreateCommandBuffer();
    command_buffer->SetLabel("EntityPass Root Command Buffer");
    auto render_pass = command_buffer->CreateRenderPass(render_target);
    render_pass->SetLabel("EntityPass Root Render Pass");

    {
      auto size_rect = Rect::MakeSize(offscreen_target.GetRenderTargetSize());
      auto contents = TextureContents::MakeRect(size_rect);
      contents->SetTexture(offscreen_target.GetRenderTargetTexture());
      contents->SetSourceRect(size_rect);

      Entity entity;
      entity.SetContents(contents);
      entity.SetBlendMode(BlendMode::kSource);

      entity.Render(renderer, *render_pass);
    }

    if (!render_pass->EncodeCommands()) {
      return false;
    }
    if (!command_buffer->SubmitCommands()) {
      return false;
    }

    return true;
  }

  return OnRender(renderer, render_target.GetRenderTargetSize(), render_target,
                  Point(), Point(), 0);
}

EntityPass::EntityResult EntityPass::GetEntityForElement(
    const EntityPass::Element& element,
    ContentContext& renderer,
    InlinePassContext& pass_context,
    ISize root_pass_size,
    Point position,
    uint32_t pass_depth,
    size_t stencil_depth_floor) const {
  Entity element_entity;

  //--------------------------------------------------------------------------
  /// Setup entity element.
  ///

  if (const auto& entity = std::get_if<Entity>(&element)) {
    element_entity = *entity;
    if (!position.IsZero()) {
      // If the pass image is going to be rendered with a non-zero position,
      // apply the negative translation to entity copies before rendering them
      // so that they'll end up rendering to the correct on-screen position.
      element_entity.SetTransformation(
          Matrix::MakeTranslation(Vector3(-position)) *
          element_entity.GetTransformation());
    }
  }

  //--------------------------------------------------------------------------
  /// Setup subpass element.
  ///

  else if (const auto& subpass_ptr =
               std::get_if<std::unique_ptr<EntityPass>>(&element)) {
    auto subpass = subpass_ptr->get();

    if (subpass->delegate_->CanElide()) {
      return EntityPass::EntityResult::Skip();
    }

    if (subpass->delegate_->CanCollapseIntoParentPass() &&
        !subpass->backdrop_filter_proc_.has_value()) {
      // Directly render into the parent target and move on.
      if (!subpass->OnRender(renderer, root_pass_size,
                             pass_context.GetRenderTarget(), position, position,
                             stencil_depth_floor)) {
        return EntityPass::EntityResult::Failure();
      }
      return EntityPass::EntityResult::Skip();
    }

    std::shared_ptr<Contents> backdrop_filter_contents = nullptr;
    if (subpass->backdrop_filter_proc_.has_value()) {
      auto texture = pass_context.GetTexture();
      // Render the backdrop texture before any of the pass elements.
      const auto& proc = subpass->backdrop_filter_proc_.value();
      backdrop_filter_contents =
          proc(FilterInput::Make(std::move(texture)), subpass->xformation_);

      // The subpass will need to read from the current pass texture when
      // rendering the backdrop, so if there's an active pass, end it prior to
      // rendering the subpass.
      pass_context.EndPass();
    }

    auto subpass_coverage =
        GetSubpassCoverage(*subpass, Rect::MakeSize(root_pass_size));
    if (subpass->cover_whole_screen_) {
      subpass_coverage = Rect(
          position, Size(pass_context.GetRenderTarget().GetRenderTargetSize()));
    }
    if (backdrop_filter_contents) {
      auto backdrop_coverage = backdrop_filter_contents->GetCoverage(Entity{});
      if (backdrop_coverage.has_value()) {
        backdrop_coverage->origin += position;

        if (subpass_coverage.has_value()) {
          subpass_coverage = subpass_coverage->Union(backdrop_coverage.value());
        } else {
          subpass_coverage = backdrop_coverage;
        }
      }
    }

    if (subpass_coverage.has_value()) {
      subpass_coverage =
          subpass_coverage->Intersection(Rect::MakeSize(root_pass_size));
    }

    if (!subpass_coverage.has_value()) {
      return EntityPass::EntityResult::Skip();
    }

    if (subpass_coverage->size.IsEmpty()) {
      // It is not an error to have an empty subpass. But subpasses that can't
      // create their intermediates must trip errors.
      return EntityPass::EntityResult::Skip();
    }

    RenderTarget subpass_target;
    if (subpass->reads_from_pass_texture_ > 0) {
      subpass_target = CreateRenderTarget(
          renderer, ISize::Ceil(subpass_coverage->size), true);
    } else {
      subpass_target = CreateRenderTarget(
          renderer, ISize::Ceil(subpass_coverage->size), false);
    }

    auto subpass_texture = subpass_target.GetRenderTargetTexture();

    if (!subpass_texture) {
      return EntityPass::EntityResult::Failure();
    }

    auto offscreen_texture_contents =
        subpass->delegate_->CreateContentsForSubpassTarget(
            subpass_texture, subpass->xformation_);

    if (!offscreen_texture_contents) {
      // This is an error because the subpass delegate said the pass couldn't
      // be collapsed into its parent. Yet, when asked how it want's to
      // postprocess the offscreen texture, it couldn't give us an answer.
      //
      // Theoretically, we could collapse the pass now. But that would be
      // wasteful as we already have the offscreen texture and we don't want
      // to discard it without ever using it. Just make the delegate do the
      // right thing.
      return EntityPass::EntityResult::Failure();
    }

    // Stencil textures aren't shared between EntityPasses (as much of the
    // time they are transient).
    if (!subpass->OnRender(renderer, root_pass_size, subpass_target,
                           subpass_coverage->origin, position, ++pass_depth,
                           subpass->stencil_depth_, backdrop_filter_contents)) {
      return EntityPass::EntityResult::Failure();
    }

    element_entity.SetContents(std::move(offscreen_texture_contents));
    element_entity.SetStencilDepth(subpass->stencil_depth_);
    element_entity.SetBlendMode(subpass->blend_mode_);
    element_entity.SetTransformation(
        Matrix::MakeTranslation(Vector3(subpass_coverage->origin - position)));
  } else {
    FML_UNREACHABLE();
  }

  return EntityPass::EntityResult::Success(element_entity);
}

struct StencilLayer {
  std::optional<Rect> coverage;
  size_t stencil_depth;
};

bool EntityPass::OnRender(
    ContentContext& renderer,
    ISize root_pass_size,
    RenderTarget render_target,
    Point position,
    Point parent_position,
    uint32_t pass_depth,
    size_t stencil_depth_floor,
    std::shared_ptr<Contents> backdrop_filter_contents) const {
  TRACE_EVENT0("impeller", "EntityPass::OnRender");

  auto context = renderer.GetContext();
  InlinePassContext pass_context(context, render_target,
                                 reads_from_pass_texture_);
  if (!pass_context.IsValid()) {
    return false;
  }

  std::vector<StencilLayer> stencil_stack = {StencilLayer{
      .coverage = Rect::MakeSize(render_target.GetRenderTargetSize()),
      .stencil_depth = stencil_depth_floor}};

  auto render_element = [&stencil_depth_floor, &pass_context, &pass_depth,
                         &renderer, &stencil_stack](Entity& element_entity) {
    auto result = pass_context.GetRenderPass(pass_depth);

    if (!result.pass) {
      return false;
    }

    // If the pass context returns a texture, we need to draw it to the current
    // pass. We do this because it's faster and takes significantly less memory
    // than storing/loading large MSAA textures.
    if (result.backdrop_texture) {
      auto size_rect = Rect::MakeSize(result.pass->GetRenderTargetSize());
      auto msaa_backdrop_contents = TextureContents::MakeRect(size_rect);
      msaa_backdrop_contents->SetStencilEnabled(false);
      msaa_backdrop_contents->SetLabel("MSAA backdrop");
      msaa_backdrop_contents->SetSourceRect(size_rect);
      msaa_backdrop_contents->SetTexture(result.backdrop_texture);

      Entity msaa_backdrop_entity;
      msaa_backdrop_entity.SetContents(std::move(msaa_backdrop_contents));
      msaa_backdrop_entity.SetBlendMode(BlendMode::kSource);
      if (!msaa_backdrop_entity.Render(renderer, *result.pass)) {
        return false;
      }
    }

    if (!element_entity.ShouldRender(stencil_stack.back().coverage)) {
      return true;  // Nothing to render.
    }

    auto stencil_coverage =
        element_entity.GetStencilCoverage(stencil_stack.back().coverage);

    switch (stencil_coverage.type) {
      case Contents::StencilCoverage::Type::kNone:
        break;
      case Contents::StencilCoverage::Type::kAppend: {
        auto op = stencil_stack.back().coverage;
        stencil_stack.push_back(StencilLayer{
            .coverage = stencil_coverage.coverage,
            .stencil_depth = element_entity.GetStencilDepth() + 1});

        if (!op.has_value()) {
          // Running this append op won't impact the stencil because the whole
          // screen is already being clipped, so skip it.
          return true;
        }
      } break;
      case Contents::StencilCoverage::Type::kRestore: {
        if (stencil_stack.back().stencil_depth <=
            element_entity.GetStencilDepth()) {
          // Drop stencil restores that will do nothing.
          return true;
        }

        FML_DCHECK(stencil_stack.size() > 1);

        stencil_stack.pop_back();

        if (!stencil_stack.back().coverage.has_value()) {
          // Running this restore op won't make anything renderable, so skip it.
          return true;
        }
      } break;
    }

    element_entity.SetStencilDepth(element_entity.GetStencilDepth() -
                                   stencil_depth_floor);
    if (!element_entity.Render(renderer, *result.pass)) {
      return false;
    }
    return true;
  };

  if (backdrop_filter_proc_.has_value()) {
    if (!backdrop_filter_contents) {
      return false;
    }

    Entity backdrop_entity;
    backdrop_entity.SetContents(std::move(backdrop_filter_contents));
    backdrop_entity.SetTransformation(
        Matrix::MakeTranslation(Vector3(parent_position - position)));
    backdrop_entity.SetStencilDepth(stencil_depth_floor);

    render_element(backdrop_entity);
  }

  for (const auto& element : elements_) {
    EntityResult result =
        GetEntityForElement(element, renderer, pass_context, root_pass_size,
                            position, pass_depth, stencil_depth_floor);

    switch (result.status) {
      case EntityResult::kSuccess:
        break;
      case EntityResult::kFailure:
        return false;
      case EntityResult::kSkip:
        continue;
    };

    //--------------------------------------------------------------------------
    /// Setup advanced blends.
    ///

    if (result.entity.GetBlendMode() > Entity::kLastPipelineBlendMode) {
      // End the active pass and flush the buffer before rendering "advanced"
      // blends. Advanced blends work by binding the current render target
      // texture as an input ("destination"), blending with a second texture
      // input ("source"), writing the result to an intermediate texture, and
      // finally copying the data from the intermediate texture back to the
      // render target texture. And so all of the commands that have written
      // to the render target texture so far need to execute before it's bound
      // for blending (otherwise the blend pass will end up executing before
      // all the previous commands in the active pass).
      if (!pass_context.EndPass()) {
        return false;
      }

      // Amend an advanced blend filter to the contents, attaching the pass
      // texture.
      auto texture = pass_context.GetTexture();
      if (!texture) {
        return false;
      }

      FilterInput::Vector inputs = {
          FilterInput::Make(result.entity.GetContents()),
          FilterInput::Make(texture,
                            result.entity.GetTransformation().Invert())};
      auto contents =
          ColorFilterContents::MakeBlend(result.entity.GetBlendMode(), inputs);
      contents->SetCoverageCrop(result.entity.GetCoverage());
      result.entity.SetContents(std::move(contents));
      result.entity.SetBlendMode(BlendMode::kSource);
    }

    //--------------------------------------------------------------------------
    /// Render the Element.
    ///

    if (!render_element(result.entity)) {
      return false;
    }
  }

  return true;
}

void EntityPass::IterateAllEntities(std::function<bool(Entity&)> iterator) {
  if (!iterator) {
    return;
  }

  for (auto& element : elements_) {
    if (auto entity = std::get_if<Entity>(&element)) {
      if (!iterator(*entity)) {
        return;
      }
      continue;
    }
    if (auto subpass = std::get_if<std::unique_ptr<EntityPass>>(&element)) {
      subpass->get()->IterateAllEntities(iterator);
      continue;
    }
    FML_UNREACHABLE();
  }
}

std::unique_ptr<EntityPass> EntityPass::Clone() const {
  std::vector<Element> new_elements;
  new_elements.reserve(elements_.size());

  for (const auto& element : elements_) {
    if (auto entity = std::get_if<Entity>(&element)) {
      new_elements.push_back(*entity);
      continue;
    }
    if (auto subpass = std::get_if<std::unique_ptr<EntityPass>>(&element)) {
      new_elements.push_back(subpass->get()->Clone());
      continue;
    }
    FML_UNREACHABLE();
  }

  auto pass = std::make_unique<EntityPass>();
  pass->SetElements(std::move(new_elements));
  return pass;
}

void EntityPass::SetTransformation(Matrix xformation) {
  xformation_ = std::move(xformation);
}

void EntityPass::SetStencilDepth(size_t stencil_depth) {
  stencil_depth_ = stencil_depth;
}

void EntityPass::SetBlendMode(BlendMode blend_mode) {
  blend_mode_ = blend_mode;
  cover_whole_screen_ = Entity::BlendModeShouldCoverWholeScreen(blend_mode);
}

void EntityPass::SetBackdropFilter(std::optional<BackdropFilterProc> proc) {
  if (superpass_) {
    VALIDATION_LOG << "Backdrop filters cannot be set on EntityPasses that "
                      "have already been appended to another pass.";
  }

  backdrop_filter_proc_ = proc;
}

}  // namespace impeller
