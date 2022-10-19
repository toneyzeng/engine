// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <memory>

#include "flutter/fml/macros.h"
#include "impeller/base/backend_cast.h"
#include "impeller/renderer/backend/vulkan/context_vk.h"
#include "impeller/renderer/device_buffer.h"

namespace impeller {

class DeviceBufferAllocationVK {
 public:
  DeviceBufferAllocationVK(const VmaAllocator& allocator,
                           VkBuffer buffer,
                           VmaAllocation allocation,
                           VmaAllocationInfo allocation_info);

  ~DeviceBufferAllocationVK();

  vk::Buffer GetBufferHandle() const;

  void* GetMapping() const;

 private:
  const VmaAllocator& allocator_;
  vk::Buffer buffer_;
  VmaAllocation allocation_;
  VmaAllocationInfo allocation_info_;

  FML_DISALLOW_COPY_AND_ASSIGN(DeviceBufferAllocationVK);
};

class DeviceBufferVK final : public DeviceBuffer,
                             public BackendCast<DeviceBufferVK, DeviceBuffer> {
 public:
  DeviceBufferVK(DeviceBufferDescriptor desc,
                 ContextVK& context,
                 std::unique_ptr<DeviceBufferAllocationVK> device_allocation);

  // |DeviceBuffer|
  ~DeviceBufferVK() override;

  vk::Buffer GetVKBufferHandle() const;

 private:
  friend class AllocatorVK;

  ContextVK& context_;
  std::unique_ptr<DeviceBufferAllocationVK> device_allocation_;

  // |DeviceBuffer|
  uint8_t* OnGetContents() const override;

  // |DeviceBuffer|
  bool OnCopyHostBuffer(const uint8_t* source,
                        Range source_range,
                        size_t offset) override;

  // |DeviceBuffer|
  bool SetLabel(const std::string& label) override;

  // |DeviceBuffer|
  bool SetLabel(const std::string& label, Range range) override;

  FML_DISALLOW_COPY_AND_ASSIGN(DeviceBufferVK);
};

}  // namespace impeller
