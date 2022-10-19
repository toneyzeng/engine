// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/renderer/backend/vulkan/context_vk.h"

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "flutter/fml/build_config.h"
#include "flutter/fml/string_conversion.h"
#include "flutter/fml/trace_event.h"
#include "impeller/base/validation.h"
#include "impeller/base/work_queue_common.h"
#include "impeller/renderer/backend/vulkan/allocator_vk.h"
#include "impeller/renderer/backend/vulkan/capabilities_vk.h"
#include "impeller/renderer/backend/vulkan/command_buffer_vk.h"
#include "impeller/renderer/backend/vulkan/surface_producer_vk.h"
#include "impeller/renderer/backend/vulkan/swapchain_details_vk.h"
#include "impeller/renderer/backend/vulkan/vk.h"
#include "vulkan/vulkan.hpp"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace impeller {

static std::set<std::string> kRequiredDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
#if FML_OS_MACOSX
    "VK_KHR_portability_subset",  // For Molten VK. No define present in header.
#endif
};

std::vector<std::string> kRequiredWSIInstanceExtensions = {
#if FML_OS_WIN
    "VK_KHR_win32_surface",
#elif FML_OS_ANDROID
    "VK_KHR_android_surface",
#elif FML_OS_LINUX
    "VK_KHR_xcb_surface",
    "VK_KHR_xlib_surface",
    "VK_KHR_wayland_surface",
#elif FML_OS_MACOSX
    "VK_EXT_metal_surface",
#endif
};

#if FML_OS_MACOSX
static const char* MVK_MACOS_SURFACE_EXT = "VK_MVK_macos_surface";
#endif

static bool HasRequiredQueues(const vk::PhysicalDevice& device) {
  auto present_flags = vk::QueueFlags{};
  for (const auto& queue : device.getQueueFamilyProperties()) {
    if (queue.queueCount == 0) {
      continue;
    }
    present_flags |= queue.queueFlags;
  }
  return static_cast<VkQueueFlags>(present_flags &
                                   (vk::QueueFlagBits::eGraphics |
                                    vk::QueueFlagBits::eCompute |
                                    vk::QueueFlagBits::eTransfer));
}

static std::vector<std::string> HasRequiredExtensions(
    const vk::PhysicalDevice& device) {
  std::set<std::string> exts;
  std::vector<std::string> missing;
  for (const auto& ext : device.enumerateDeviceExtensionProperties().value) {
    exts.insert(ext.extensionName);
  }
  for (const auto& req_ext : kRequiredDeviceExtensions) {
    if (exts.count(req_ext) != 1u) {
      missing.push_back(req_ext);
    }
  }
  return missing;
}

static vk::PhysicalDeviceFeatures GetRequiredPhysicalDeviceFeatures() {
  vk::PhysicalDeviceFeatures features;
  features.setRobustBufferAccess(true);
  return features;
};

static bool HasRequiredProperties(const vk::PhysicalDevice& device) {
  auto properties = device.getProperties();
  if (!(properties.limits.framebufferColorSampleCounts &
        (vk::SampleCountFlagBits::e1 | vk::SampleCountFlagBits::e4))) {
    return false;
  }
  return true;
}

static bool IsPhysicalDeviceCompatible(const vk::PhysicalDevice& device) {
  if (!HasRequiredQueues(device)) {
    FML_LOG(ERROR) << "Device doesn't have required queues.";
    return false;
  }
  auto missing_exts = HasRequiredExtensions(device);
  if (!missing_exts.empty()) {
    FML_LOG(ERROR) << "Device doesn't have required extensions: "
                   << fml::Join(missing_exts, ", ");
    return false;
  }
  if (!HasRequiredProperties(device)) {
    FML_LOG(ERROR) << "Device doesn't have required properties.";
    return false;
  }
  return true;
}

static std::optional<vk::PhysicalDevice> PickPhysicalDevice(
    const vk::Instance& instance) {
  for (const auto& device : instance.enumeratePhysicalDevices().value) {
    if (IsPhysicalDeviceCompatible(device)) {
      return device;
    }
  }
  return std::nullopt;
}

static std::vector<vk::DeviceQueueCreateInfo> GetQueueCreateInfos(
    std::initializer_list<QueueVK> queues) {
  std::map<size_t /* family */, size_t /* index */> family_index_map;
  for (const auto& queue : queues) {
    family_index_map[queue.family] = 0;
  }
  for (const auto& queue : queues) {
    auto value = family_index_map[queue.family];
    family_index_map[queue.family] = std::max(value, queue.index);
  }

  static float kQueuePriority = 1.0f;
  std::vector<vk::DeviceQueueCreateInfo> infos;
  for (const auto& item : family_index_map) {
    vk::DeviceQueueCreateInfo info;
    info.setQueueFamilyIndex(item.first);
    info.setQueueCount(item.second + 1);
    info.setQueuePriorities(kQueuePriority);
    infos.push_back(info);
  }
  return infos;
}

static std::optional<QueueVK> PickQueue(const vk::PhysicalDevice& device,
                                        vk::QueueFlagBits flags) {
  // This can be modified to ensure that dedicated queues are returned for each
  // queue type depending on support.
  const auto families = device.getQueueFamilyProperties();
  for (size_t i = 0u; i < families.size(); i++) {
    if (!(families[i].queueFlags & flags)) {
      continue;
    }
    return QueueVK{.family = i, .index = 0};
  }
  return std::nullopt;
}

static std::optional<QueueVK> PickPresentQueue(const vk::PhysicalDevice& device,
                                               vk::SurfaceKHR surface) {
  const auto families = device.getQueueFamilyProperties();
  for (size_t i = 0u; i < families.size(); i++) {
    auto res = device.getSurfaceSupportKHR(i, surface);
    if (res.result != vk::Result::eSuccess) {
      continue;
    }
    vk::Bool32 present_supported = res.value;
    if (present_supported) {
      return QueueVK{.family = i, .index = 0};
    }
  }
  VALIDATION_LOG << "No present queue found.";
  return std::nullopt;
}

std::shared_ptr<ContextVK> ContextVK::Create(
    PFN_vkGetInstanceProcAddr proc_address_callback,
    const std::vector<std::shared_ptr<fml::Mapping>>& shader_libraries_data,
    const std::shared_ptr<const fml::Mapping>& pipeline_cache_data,
    std::shared_ptr<fml::ConcurrentTaskRunner> worker_task_runner,
    const std::string& label) {
  auto context = std::shared_ptr<ContextVK>(new ContextVK(
      proc_address_callback,          //
      shader_libraries_data,          //
      pipeline_cache_data,            //
      std::move(worker_task_runner),  //
      label                           //
      ));
  if (!context->IsValid()) {
    return nullptr;
  }
  return context;
}

ContextVK::ContextVK(
    PFN_vkGetInstanceProcAddr proc_address_callback,
    const std::vector<std::shared_ptr<fml::Mapping>>& shader_libraries_data,
    const std::shared_ptr<const fml::Mapping>& pipeline_cache_data,
    std::shared_ptr<fml::ConcurrentTaskRunner> worker_task_runner,
    const std::string& label)
    : worker_task_runner_(std::move(worker_task_runner)) {
  TRACE_EVENT0("impeller", "ContextVK::Create");

  if (!worker_task_runner_) {
    VALIDATION_LOG << "Invalid worker task runner.";
    return;
  }

  auto& dispatcher = VULKAN_HPP_DEFAULT_DISPATCHER;
  dispatcher.init(proc_address_callback);

  auto capabilities = std::make_unique<CapabilitiesVK>();

  vk::InstanceCreateFlags instance_flags = {};
  std::vector<const char*> enabled_layers;
  std::vector<const char*> enabled_extensions;

// This define may need to change into a runtime check if using SwiftShader on
// Mac.
#if FML_OS_MACOSX
  //----------------------------------------------------------------------------
  /// Ensure we need any Vulkan implementations that are not fully compliant
  /// with the requested Vulkan Spec. This is necessary for MoltenVK on Mac
  /// (backed by Metal).
  ///
  if (!capabilities->HasExtension(
          VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
    VALIDATION_LOG << "On Mac: Required extension "
                   << VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
                   << " absent.";
    return;
  }
  // Molten VK on Mac is not fully compliant. We opt into being OK not getting
  // back a fully compliant version of a Vulkan implementation.
  enabled_extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
  instance_flags |= vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;

  if (!capabilities->HasExtension(
          VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME)) {
    VALIDATION_LOG << "On Mac: Required extension "
                   << VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
                   << " absent.";
    return;
  }
  // This is dependency of VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME which
  // is a requirement for opting into Molten VK on Mac.
  enabled_extensions.push_back(
      VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

  // Required for glfw macOS surfaces.
  if (!capabilities->HasExtension(MVK_MACOS_SURFACE_EXT)) {
    VALIDATION_LOG << "On Mac: Required extension " << MVK_MACOS_SURFACE_EXT
                   << " absent.";
    return;
  }
  enabled_extensions.push_back(MVK_MACOS_SURFACE_EXT);
#endif  // FML_OS_MACOSX

  //----------------------------------------------------------------------------
  /// Even though this is a WSI responsibility, require the surface extension
  /// for swapchains.
  if (!capabilities->HasExtension(VK_KHR_SURFACE_EXTENSION_NAME)) {
    VALIDATION_LOG << "Required extension " VK_KHR_SURFACE_EXTENSION_NAME
                   << " absent.";
    return;
  }
  enabled_extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);

  //----------------------------------------------------------------------------
  /// Enable WSI Instance Extensions. Having any one of these is sufficient.
  ///
  bool has_wsi_extensions = false;
  for (const auto& wsi_ext : kRequiredWSIInstanceExtensions) {
    if (capabilities->HasExtension(wsi_ext)) {
      enabled_extensions.push_back(wsi_ext.c_str());
      has_wsi_extensions = true;
    }
  }
  if (!has_wsi_extensions) {
    VALIDATION_LOG
        << "Instance doesn't have any of the required WSI extensions: "
        << fml::Join(kRequiredWSIInstanceExtensions, ", ");
    return;
  }

  //----------------------------------------------------------------------------
  /// Enable any and all validation as well as debug toggles.
  ///
  auto has_debug_utils = false;
  constexpr const char* kKhronosValidationLayerName =
      "VK_LAYER_KHRONOS_validation";
  if (capabilities->HasLayer(kKhronosValidationLayerName)) {
    enabled_layers.push_back(kKhronosValidationLayerName);
    if (capabilities->HasLayerExtension(kKhronosValidationLayerName,
                                        VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
      enabled_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
      has_debug_utils = true;
    } else {
      FML_LOG(ERROR) << "Vulkan debug utils are absent.";
    }
  } else {
    FML_LOG(ERROR) << "Vulkan validation layers are absent.";
  }

  vk::ApplicationInfo application_info;
  application_info.setApplicationVersion(VK_API_VERSION_1_0);
  application_info.setApiVersion(VK_API_VERSION_1_1);
  application_info.setEngineVersion(VK_API_VERSION_1_0);
  application_info.setPEngineName("Impeller");
  application_info.setPApplicationName("Impeller");

  vk::InstanceCreateInfo instance_info;
  instance_info.setPEnabledLayerNames(enabled_layers);
  instance_info.setPEnabledExtensionNames(enabled_extensions);
  instance_info.setPApplicationInfo(&application_info);
  instance_info.setFlags(instance_flags);

  auto instance = vk::createInstanceUnique(instance_info);
  if (instance.result != vk::Result::eSuccess) {
    FML_LOG(ERROR) << "Could not create instance: "
                   << vk::to_string(instance.result);
    return;
  }

  dispatcher.init(instance.value.get());

  vk::UniqueDebugUtilsMessengerEXT debug_messenger;

  if (has_debug_utils) {
    vk::DebugUtilsMessengerCreateInfoEXT debug_messenger_info;

    debug_messenger_info.messageSeverity =
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
    debug_messenger_info.messageType =
        vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
        vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
        vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation;
    debug_messenger_info.pUserData = nullptr;
    debug_messenger_info.pfnUserCallback =
        [](VkDebugUtilsMessageSeverityFlagBitsEXT severity,
           VkDebugUtilsMessageTypeFlagsEXT type,
           const VkDebugUtilsMessengerCallbackDataEXT* data,
           void* user_data) -> VkBool32 {
      if (type == VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
        // do not terminate on performance warnings.
        FML_LOG(ERROR)
            << vk::to_string(vk::DebugUtilsMessageSeverityFlagBitsEXT{severity})
            << ": " << data->pMessage;
      } else {
        FML_DCHECK(false)
            << vk::to_string(vk::DebugUtilsMessageSeverityFlagBitsEXT{severity})
            << ": " << data->pMessage;
      }
      return true;
    };

    auto debug_messenger_result =
        instance.value->createDebugUtilsMessengerEXTUnique(
            debug_messenger_info);

    if (debug_messenger_result.result != vk::Result::eSuccess) {
      FML_LOG(ERROR) << "Could not create debug messenger: "
                     << vk::to_string(debug_messenger_result.result);
      return;
    }

    debug_messenger = std::move(debug_messenger_result.value);
  }

  auto physical_device = PickPhysicalDevice(instance.value.get());
  if (!physical_device.has_value()) {
    VALIDATION_LOG << "No valid Vulkan device found.";
    return;
  }

  auto graphics_queue =
      PickQueue(physical_device.value(), vk::QueueFlagBits::eGraphics);
  auto transfer_queue =
      PickQueue(physical_device.value(), vk::QueueFlagBits::eTransfer);
  auto compute_queue =
      PickQueue(physical_device.value(), vk::QueueFlagBits::eCompute);

  physical_device_ = physical_device.value();

  if (!graphics_queue.has_value() || !transfer_queue.has_value() ||
      !compute_queue.has_value()) {
    VALIDATION_LOG << "Could not pick device queues.";
    return;
  }

  std::vector<const char*> required_extensions;
  for (const auto& ext : kRequiredDeviceExtensions) {
    required_extensions.push_back(ext.data());
  }

  const auto queue_create_infos = GetQueueCreateInfos(
      {graphics_queue.value(), compute_queue.value(), transfer_queue.value()});

  const auto required_features = GetRequiredPhysicalDeviceFeatures();

  vk::DeviceCreateInfo device_info;
  device_info.setQueueCreateInfos(queue_create_infos);
  device_info.setPEnabledExtensionNames(required_extensions);
  device_info.setPEnabledFeatures(&required_features);
  // Device layers are deprecated and ignored.

  auto device = physical_device->createDeviceUnique(device_info);
  if (device.result != vk::Result::eSuccess) {
    VALIDATION_LOG << "Could not create logical device.";
    return;
  }

  auto allocator = std::shared_ptr<AllocatorVK>(new AllocatorVK(
      *this,                             //
      application_info.apiVersion,       //
      physical_device.value(),           //
      device.value.get(),                //
      instance.value.get(),              //
      dispatcher.vkGetInstanceProcAddr,  //
      dispatcher.vkGetDeviceProcAddr     //
      ));

  if (!allocator->IsValid()) {
    VALIDATION_LOG << "Could not create memory allocator.";
    return;
  }

  auto pipeline_library = std::shared_ptr<PipelineLibraryVK>(
      new PipelineLibraryVK(device.value.get(),   //
                            pipeline_cache_data,  //
                            worker_task_runner_   //
                            ));

  if (!pipeline_library->IsValid()) {
    VALIDATION_LOG << "Could not create pipeline library.";
    return;
  }

  auto sampler_library = std::shared_ptr<SamplerLibraryVK>(
      new SamplerLibraryVK(device.value.get()));

  auto shader_library = std::shared_ptr<ShaderLibraryVK>(
      new ShaderLibraryVK(device.value.get(), shader_libraries_data));

  if (!shader_library->IsValid()) {
    VALIDATION_LOG << "Could not create shader library.";
    return;
  }

  auto work_queue = WorkQueueCommon::Create();

  if (!work_queue) {
    VALIDATION_LOG << "Could not create workqueue.";
    return;
  }

  instance_ = std::move(instance.value);
  debug_messenger_ = std::move(debug_messenger);
  device_ = std::move(device.value);
  allocator_ = std::move(allocator);
  shader_library_ = std::move(shader_library);
  sampler_library_ = std::move(sampler_library);
  pipeline_library_ = std::move(pipeline_library);
  work_queue_ = std::move(work_queue);
  graphics_queue_ =
      device_->getQueue(graphics_queue->family, graphics_queue->index);
  compute_queue_ =
      device_->getQueue(compute_queue->family, compute_queue->index);
  transfer_queue_ =
      device_->getQueue(transfer_queue->family, transfer_queue->index);
  graphics_command_pool_ =
      CommandPoolVK::Create(*device_, graphics_queue->index);
  descriptor_pool_ = std::make_shared<DescriptorPoolVK>(*device_);
  is_valid_ = true;
}

ContextVK::~ContextVK() = default;

bool ContextVK::IsValid() const {
  return is_valid_;
}

std::shared_ptr<Allocator> ContextVK::GetResourceAllocator() const {
  return allocator_;
}

std::shared_ptr<ShaderLibrary> ContextVK::GetShaderLibrary() const {
  return shader_library_;
}

std::shared_ptr<SamplerLibrary> ContextVK::GetSamplerLibrary() const {
  return sampler_library_;
}

std::shared_ptr<PipelineLibrary> ContextVK::GetPipelineLibrary() const {
  return pipeline_library_;
}

// |Context|
std::shared_ptr<WorkQueue> ContextVK::GetWorkQueue() const {
  return work_queue_;
}

std::shared_ptr<CommandBuffer> ContextVK::CreateCommandBuffer() const {
  return CommandBufferVK::Create(weak_from_this(), *device_,
                                 graphics_command_pool_->Get(),
                                 surface_producer_.get());
}

vk::Instance ContextVK::GetInstance() const {
  return *instance_;
}

std::unique_ptr<Surface> ContextVK::AcquireSurface(size_t current_frame) {
  return surface_producer_->AcquireSurface(current_frame);
}

void ContextVK::SetupSwapchain(vk::UniqueSurfaceKHR surface) {
  surface_ = std::move(surface);
  auto present_queue_out = PickPresentQueue(physical_device_, *surface_);
  if (!present_queue_out.has_value()) {
    return;
  }
  present_queue_ =
      device_->getQueue(present_queue_out->family, present_queue_out->index);

  auto swapchain_details =
      SwapchainDetailsVK::Create(physical_device_, *surface_);
  if (!swapchain_details) {
    return;
  }
  swapchain_ = SwapchainVK::Create(*device_, *surface_, *swapchain_details);
  auto weak_this = weak_from_this();
  surface_producer_ = SurfaceProducerVK::Create(
      weak_this, {
                     .device = *device_,
                     .graphics_queue = graphics_queue_,
                     .present_queue = present_queue_,
                     .swapchain = swapchain_.get(),
                 });
}

bool ContextVK::SupportsOffscreenMSAA() const {
  return true;
}

std::shared_ptr<DescriptorPoolVK> ContextVK::GetDescriptorPool() const {
  return descriptor_pool_;
}

}  // namespace impeller
