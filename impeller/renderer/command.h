// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "flutter/fml/logging.h"
#include "flutter/fml/macros.h"
#include "impeller/geometry/rect.h"
#include "impeller/renderer/buffer_view.h"
#include "impeller/renderer/formats.h"
#include "impeller/renderer/pipeline.h"
#include "impeller/renderer/sampler.h"
#include "impeller/renderer/shader_types.h"
#include "impeller/renderer/texture.h"
#include "impeller/renderer/vertex_buffer.h"
#include "impeller/renderer/vertex_buffer_builder.h"
#include "impeller/tessellator/tessellator.h"

namespace impeller {

template <class T>
struct Resource {
  using ResourceType = T;
  const ShaderMetadata* isa;
  ResourceType resource;

  Resource() : isa(nullptr) {}

  Resource(const ShaderMetadata* p_isa, ResourceType p_resource)
      : isa(p_isa), resource(p_resource) {}
};

using BufferResource = Resource<BufferView>;
using TextureResource = Resource<std::shared_ptr<const Texture>>;
using SamplerResource = Resource<std::shared_ptr<const Sampler>>;

struct Bindings {
  std::map<size_t, BufferResource> buffers;
  std::map<size_t, TextureResource> textures;
  std::map<size_t, SamplerResource> samplers;
};

//------------------------------------------------------------------------------
/// @brief      An object used to specify work to the GPU along with references
///             to resources the GPU will used when doing said work.
///
///             To construct a valid command, follow these steps:
///             * Specify a valid pipeline.
///             * Specify vertex information via a call `BindVertices`
///             * Specify any stage bindings.
///             * (Optional) Specify a debug label.
///
///             Command are very lightweight objects and can be created
///             frequently and on demand. The resources referenced in commands
///             views into buffers managed by other allocators and resource
///             managers.
///
struct Command {
  //----------------------------------------------------------------------------
  /// The pipeline to use for this command.
  ///
  std::shared_ptr<Pipeline> pipeline;
  //----------------------------------------------------------------------------
  /// The buffer, texture, and sampler bindings used by the vertex pipeline
  /// stage.
  ///
  Bindings vertex_bindings;
  //----------------------------------------------------------------------------
  /// The buffer, texture, and sampler bindings used by the fragment pipeline
  /// stage.
  ///
  Bindings fragment_bindings;
  //----------------------------------------------------------------------------
  /// The index buffer binding used by the vertex shader stage. Instead of
  /// setting this directly, it usually easier to specify the vertex and index
  /// buffer bindings directly via a single call to `BindVertices`.
  ///
  /// @see         `BindVertices`
  ///
  BufferView index_buffer;
  //----------------------------------------------------------------------------
  /// The number of indices to use from the index buffer. Set the vertex and
  /// index buffers as well as the index count using a call to `BindVertices`.
  ///
  /// @see         `BindVertices`
  ///
  size_t index_count = 0u;
  //----------------------------------------------------------------------------
  /// The type of indices in the index buffer. The indices must be tightly
  /// packed in the index buffer.
  ///
  IndexType index_type = IndexType::kUnknown;
  //----------------------------------------------------------------------------
  /// The debugging label to use for the command.
  ///
  std::string label;
  //----------------------------------------------------------------------------
  /// The type of primitives in the vertex buffer. Set the vertex and index
  /// buffers using a call to `BindVertices`.
  ///
  /// @see         `BindVertices`
  ///
  PrimitiveType primitive_type = PrimitiveType::kTriangle;
  //----------------------------------------------------------------------------
  /// The orientation of vertices of the front-facing polygons. This usually
  /// matters when culling is enabled.
  ///
  /// @see         `cull_mode`
  ///
  WindingOrder winding = WindingOrder::kClockwise;
  //----------------------------------------------------------------------------
  /// How to control culling of polygons. The orientation of front-facing
  /// polygons is controlled via the `winding` parameter.
  ///
  /// @see         `winding`
  ///
  CullMode cull_mode = CullMode::kNone;
  //----------------------------------------------------------------------------
  /// The reference value to use in stenciling operations. Stencil configuration
  /// is part of pipeline setup and can be read from the pipelines descriptor.
  ///
  /// @see         `Pipeline`
  /// @see         `PipelineDescriptor`
  ///
  uint32_t stencil_reference = 0u;
  //----------------------------------------------------------------------------
  /// The offset used when indexing into the vertex buffer.
  ///
  uint64_t base_vertex = 0u;
  //----------------------------------------------------------------------------
  /// The viewport coordinates that the rasterizer linearly maps normalized
  /// device coordinates to.
  /// If unset, the viewport is the size of the render target with a zero
  /// origin, znear=0, and zfar=1.
  ///
  std::optional<Viewport> viewport;
  //----------------------------------------------------------------------------
  /// The scissor rect to use for clipping writes to the render target. The
  /// scissor rect must lie entirely within the render target.
  /// If unset, no scissor is applied.
  ///
  std::optional<IRect> scissor;
  //----------------------------------------------------------------------------
  /// The number of instances of the given set of vertices to render. Not all
  /// backends support rendering more than one instance at a time.
  ///
  /// @warning      Setting this to more than one will limit the availability of
  ///               backends to use with this command.
  ///
  size_t instance_count = 1u;

  //----------------------------------------------------------------------------
  /// @brief      Specify the vertex and index buffer to use for this command.
  ///
  /// @param[in]  buffer  The vertex and index buffer definition.
  ///
  /// @return     returns if the binding was updated.
  ///
  bool BindVertices(const VertexBuffer& buffer);

  bool BindResource(ShaderStage stage,
                    const ShaderUniformSlot& slot,
                    const ShaderMetadata& metadata,
                    BufferView view);

  bool BindResource(ShaderStage stage,
                    const SampledImageSlot& slot,
                    const ShaderMetadata& metadata,
                    std::shared_ptr<const Texture> texture);

  bool BindResource(ShaderStage stage,
                    const SampledImageSlot& slot,
                    const ShaderMetadata& metadata,
                    std::shared_ptr<const Sampler> sampler);

  bool BindResource(ShaderStage stage,
                    const SampledImageSlot& slot,
                    const ShaderMetadata& metadata,
                    std::shared_ptr<const Texture> texture,
                    std::shared_ptr<const Sampler> sampler);

  BufferView GetVertexBuffer() const;

  constexpr operator bool() const { return pipeline && pipeline->IsValid(); }
};

template <class VertexShader_, class FragmentShader_>
struct CommandT {
  using VertexShader = VertexShader_;
  using FragmentShader = FragmentShader_;
  using VertexBufferBuilder =
      VertexBufferBuilder<typename VertexShader_::PerVertexData>;
  using Pipeline = PipelineT<VertexShader_, FragmentShader_>;

  CommandT(PipelineT<VertexShader, FragmentShader>& pipeline) {
    command_.label = VertexShader::kLabel;

    // This could be moved to the accessor to delay the wait.
    command_.pipeline = pipeline.WaitAndGet();
  }

  static VertexBufferBuilder CreateVertexBuilder() {
    VertexBufferBuilder builder;
    builder.SetLabel(std::string{VertexShader::kLabel});
    return builder;
  }

  Command& Get() { return command_; }

  operator Command&() { return Get(); }

  bool BindVertices(VertexBufferBuilder builder, HostBuffer& buffer) {
    return command_.BindVertices(builder.CreateVertexBuffer(buffer));
  }

  bool BindVerticesDynamic(const VertexBuffer& buffer) {
    return command_.BindVertices(buffer);
  }

 private:
  Command command_;
};

}  // namespace impeller
