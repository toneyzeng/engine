// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/renderer/backend/gles/context_gles.h"

#include "impeller/base/config.h"
#include "impeller/base/validation.h"
#include "impeller/base/work_queue_common.h"

namespace impeller {

std::shared_ptr<ContextGLES> ContextGLES::Create(
    std::unique_ptr<ProcTableGLES> gl,
    const std::vector<std::shared_ptr<fml::Mapping>>& shader_libraries) {
  return std::shared_ptr<ContextGLES>(
      new ContextGLES(std::move(gl), shader_libraries));
}

ContextGLES::ContextGLES(std::unique_ptr<ProcTableGLES> gl,
                         const std::vector<std::shared_ptr<fml::Mapping>>&
                             shader_libraries_mappings) {
  reactor_ = std::make_shared<ReactorGLES>(std::move(gl));
  if (!reactor_->IsValid()) {
    VALIDATION_LOG << "Could not create valid reactor.";
    return;
  }

  // Create the shader library.
  {
    auto library = std::shared_ptr<ShaderLibraryGLES>(
        new ShaderLibraryGLES(shader_libraries_mappings));
    if (!library->IsValid()) {
      VALIDATION_LOG << "Could not create valid shader library.";
      return;
    }
    shader_library_ = std::move(library);
  }

  // Create the pipeline library.
  {
    pipeline_library_ =
        std::shared_ptr<PipelineLibraryGLES>(new PipelineLibraryGLES(reactor_));
  }

  // Create allocators.
  {
    resource_allocator_ =
        std::shared_ptr<AllocatorGLES>(new AllocatorGLES(reactor_));
    if (!resource_allocator_->IsValid()) {
      VALIDATION_LOG << "Could not create a resource allocator.";
      return;
    }
  }

  // Create the sampler library.
  {
    sampler_library_ =
        std::shared_ptr<SamplerLibraryGLES>(new SamplerLibraryGLES());
  }

  // Create the work queue.
  {
    work_queue_ = WorkQueueCommon::Create();
    if (!work_queue_) {
      VALIDATION_LOG << "Could not create work queue.";
      return;
    }
  }

  is_valid_ = true;
}

ContextGLES::~ContextGLES() = default;

const ReactorGLES::Ref& ContextGLES::GetReactor() const {
  return reactor_;
}

std::optional<ReactorGLES::WorkerID> ContextGLES::AddReactorWorker(
    const std::shared_ptr<ReactorGLES::Worker>& worker) {
  if (!IsValid()) {
    return std::nullopt;
  }
  return reactor_->AddWorker(worker);
}

bool ContextGLES::RemoveReactorWorker(ReactorGLES::WorkerID id) {
  if (!IsValid()) {
    return false;
  }
  return reactor_->RemoveWorker(id);
}

bool ContextGLES::IsValid() const {
  return is_valid_;
}

// |Context|
std::shared_ptr<Allocator> ContextGLES::GetResourceAllocator() const {
  return resource_allocator_;
}

// |Context|
std::shared_ptr<ShaderLibrary> ContextGLES::GetShaderLibrary() const {
  return shader_library_;
}

// |Context|
std::shared_ptr<SamplerLibrary> ContextGLES::GetSamplerLibrary() const {
  return sampler_library_;
}

// |Context|
std::shared_ptr<PipelineLibrary> ContextGLES::GetPipelineLibrary() const {
  return pipeline_library_;
}

// |Context|
std::shared_ptr<CommandBuffer> ContextGLES::CreateCommandBuffer() const {
  return std::shared_ptr<CommandBufferGLES>(
      new CommandBufferGLES(weak_from_this(), reactor_));
}

// |Context|
std::shared_ptr<WorkQueue> ContextGLES::GetWorkQueue() const {
  return work_queue_;
}

// |Context|
bool ContextGLES::HasThreadingRestrictions() const {
  return true;
}

// |Context|
bool ContextGLES::SupportsOffscreenMSAA() const {
  return false;
}

}  // namespace impeller
