// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/renderer/backend/vulkan/capabilities_vk.h"

#include <algorithm>

#include "impeller/base/validation.h"
#include "impeller/core/formats.h"
#include "impeller/renderer/backend/vulkan/vk.h"

namespace impeller {

static constexpr const char* kInstanceLayer = "ImpellerInstance";

CapabilitiesVK::CapabilitiesVK(bool enable_validations) {
  auto extensions = vk::enumerateInstanceExtensionProperties();
  auto layers = vk::enumerateInstanceLayerProperties();

  if (extensions.result != vk::Result::eSuccess ||
      layers.result != vk::Result::eSuccess) {
    return;
  }

  for (const auto& ext : extensions.value) {
    exts_[kInstanceLayer].insert(ext.extensionName);
  }

  for (const auto& layer : layers.value) {
    const std::string layer_name = layer.layerName;
    auto layer_exts = vk::enumerateInstanceExtensionProperties(layer_name);
    if (layer_exts.result != vk::Result::eSuccess) {
      return;
    }
    for (const auto& layer_ext : layer_exts.value) {
      exts_[layer_name].insert(layer_ext.extensionName);
    }
  }

  validations_enabled_ =
      enable_validations && HasLayer("VK_LAYER_KHRONOS_validation");
  if (enable_validations && !validations_enabled_) {
    FML_LOG(ERROR)
        << "Requested Impeller context creation with validations but the "
           "validation layers could not be found. Expect no Vulkan validation "
           "checks!";
  }
  if (validations_enabled_) {
    FML_LOG(INFO) << "Vulkan validations are enabled.";
  }
  is_valid_ = true;
}

CapabilitiesVK::~CapabilitiesVK() = default;

bool CapabilitiesVK::IsValid() const {
  return is_valid_;
}

bool CapabilitiesVK::AreValidationsEnabled() const {
  return validations_enabled_;
}

std::optional<std::vector<std::string>> CapabilitiesVK::GetEnabledLayers()
    const {
  std::vector<std::string> required;

  if (validations_enabled_) {
    // The presence of this layer is already checked in the ctor.
    required.push_back("VK_LAYER_KHRONOS_validation");
  }

  return required;
}

std::optional<std::vector<std::string>>
CapabilitiesVK::GetEnabledInstanceExtensions() const {
  std::vector<std::string> required;

  if (!HasExtension("VK_KHR_surface")) {
    // Swapchain support is required and this is a dependency of
    // VK_KHR_swapchain.
    VALIDATION_LOG << "Could not find the surface extension.";
    return std::nullopt;
  }
  required.push_back("VK_KHR_surface");

  auto has_wsi = false;
  if (HasExtension("VK_MVK_macos_surface")) {
    required.push_back("VK_MVK_macos_surface");
    has_wsi = true;
  }

  if (HasExtension("VK_EXT_metal_surface")) {
    required.push_back("VK_EXT_metal_surface");
    has_wsi = true;
  }

  if (HasExtension("VK_KHR_portability_enumeration")) {
    required.push_back("VK_KHR_portability_enumeration");
    has_wsi = true;
  }

  if (HasExtension("VK_KHR_win32_surface")) {
    required.push_back("VK_KHR_win32_surface");
    has_wsi = true;
  }

  if (HasExtension("VK_KHR_android_surface")) {
    required.push_back("VK_KHR_android_surface");
    has_wsi = true;
  }

  if (HasExtension("VK_KHR_xcb_surface")) {
    required.push_back("VK_KHR_xcb_surface");
    has_wsi = true;
  }

  if (HasExtension("VK_KHR_xlib_surface")) {
    required.push_back("VK_KHR_xlib_surface");
    has_wsi = true;
  }

  if (HasExtension("VK_KHR_wayland_surface")) {
    required.push_back("VK_KHR_wayland_surface");
    has_wsi = true;
  }

  if (!has_wsi) {
    // Don't really care which WSI extension there is as long there is at least
    // one.
    VALIDATION_LOG << "Could not find a WSI extension.";
    return std::nullopt;
  }

  if (validations_enabled_) {
    if (!HasExtension("VK_EXT_debug_utils")) {
      VALIDATION_LOG << "Requested validations but could not find the "
                        "VK_EXT_debug_utils extension.";
      return std::nullopt;
    }
    required.push_back("VK_EXT_debug_utils");

    if (HasExtension("VK_EXT_validation_features")) {
      // It's valid to not have `VK_EXT_validation_features` available.  That's
      // the case when using AGI as a frame debugger.
      required.push_back("VK_EXT_validation_features");
    }
  }

  return required;
}

static const char* GetExtensionName(RequiredCommonDeviceExtensionVK ext) {
  switch (ext) {
    case RequiredCommonDeviceExtensionVK::kKHRSwapchain:
      return VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    case RequiredCommonDeviceExtensionVK::kLast:
      return "Unknown";
  }
  FML_UNREACHABLE();
}

static const char* GetExtensionName(RequiredAndroidDeviceExtensionVK ext) {
  switch (ext) {
    case RequiredAndroidDeviceExtensionVK::
        kANDROIDExternalMemoryAndroidHardwareBuffer:
      return "VK_ANDROID_external_memory_android_hardware_buffer";
    case RequiredAndroidDeviceExtensionVK::kKHRSamplerYcbcrConversion:
      return VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME;
    case RequiredAndroidDeviceExtensionVK::kKHRExternalMemory:
      return VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME;
    case RequiredAndroidDeviceExtensionVK::kEXTQueueFamilyForeign:
      return VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME;
    case RequiredAndroidDeviceExtensionVK::kKHRDedicatedAllocation:
      return VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME;
    case RequiredAndroidDeviceExtensionVK::kLast:
      return "Unknown";
  }
  FML_UNREACHABLE();
}

static const char* GetExtensionName(OptionalDeviceExtensionVK ext) {
  switch (ext) {
    case OptionalDeviceExtensionVK::kEXTPipelineCreationFeedback:
      return VK_EXT_PIPELINE_CREATION_FEEDBACK_EXTENSION_NAME;
    case OptionalDeviceExtensionVK::kVKKHRPortabilitySubset:
      return "VK_KHR_portability_subset";
    case OptionalDeviceExtensionVK::kLast:
      return "Unknown";
  }
  FML_UNREACHABLE();
}

template <class T>
static bool IterateExtensions(const std::function<bool(T)>& it) {
  if (!it) {
    return false;
  }
  for (size_t i = 0; i < static_cast<uint32_t>(T::kLast); i++) {
    if (!it(static_cast<T>(i))) {
      return false;
    }
  }
  return true;
}

static std::optional<std::set<std::string>> GetSupportedDeviceExtensions(
    const vk::PhysicalDevice& physical_device) {
  auto device_extensions = physical_device.enumerateDeviceExtensionProperties();
  if (device_extensions.result != vk::Result::eSuccess) {
    return std::nullopt;
  }

  std::set<std::string> exts;
  for (const auto& device_extension : device_extensions.value) {
    exts.insert(device_extension.extensionName);
  };

  return exts;
}

std::optional<std::vector<std::string>>
CapabilitiesVK::GetEnabledDeviceExtensions(
    const vk::PhysicalDevice& physical_device) const {
  auto exts = GetSupportedDeviceExtensions(physical_device);

  if (!exts.has_value()) {
    return std::nullopt;
  }

  std::vector<std::string> enabled;

  auto for_each_common_extension = [&](RequiredCommonDeviceExtensionVK ext) {
    auto name = GetExtensionName(ext);
    if (exts->find(name) == exts->end()) {
      VALIDATION_LOG << "Device does not support required extension: " << name;
      return false;
    }
    enabled.push_back(name);
    return true;
  };

  auto for_each_android_extension = [&](RequiredAndroidDeviceExtensionVK ext) {
#ifdef FML_OS_ANDROID
    auto name = GetExtensionName(ext);
    if (exts->find(name) == exts->end()) {
      VALIDATION_LOG << "Device does not support required Android extension: "
                     << name;
      return false;
    }
    enabled.push_back(name);
#endif  //  FML_OS_ANDROID
    return true;
  };

  auto for_each_optional_extension = [&](OptionalDeviceExtensionVK ext) {
    auto name = GetExtensionName(ext);
    if (exts->find(name) != exts->end()) {
      enabled.push_back(name);
    }
    return true;
  };

  const auto iterate_extensions =
      IterateExtensions<RequiredCommonDeviceExtensionVK>(
          for_each_common_extension) &&
      IterateExtensions<RequiredAndroidDeviceExtensionVK>(
          for_each_android_extension) &&
      IterateExtensions<OptionalDeviceExtensionVK>(for_each_optional_extension);

  if (!iterate_extensions) {
    VALIDATION_LOG << "Device not suitable since required extensions are not "
                      "supported.";
    return std::nullopt;
  }

  return enabled;
}

static bool HasSuitableColorFormat(const vk::PhysicalDevice& device,
                                   vk::Format format) {
  const auto props = device.getFormatProperties(format);
  // This needs to be more comprehensive.
  return !!(props.optimalTilingFeatures &
            vk::FormatFeatureFlagBits::eColorAttachment);
}

static bool HasSuitableDepthStencilFormat(const vk::PhysicalDevice& device,
                                          vk::Format format) {
  const auto props = device.getFormatProperties(format);
  return !!(props.optimalTilingFeatures &
            vk::FormatFeatureFlagBits::eDepthStencilAttachment);
}

static bool PhysicalDeviceSupportsRequiredFormats(
    const vk::PhysicalDevice& device) {
  const auto has_color_format =
      HasSuitableColorFormat(device, vk::Format::eB8G8R8A8Unorm);
  const auto has_stencil_format =
      HasSuitableDepthStencilFormat(device, vk::Format::eD32SfloatS8Uint) ||
      HasSuitableDepthStencilFormat(device, vk::Format::eD24UnormS8Uint);
  return has_color_format && has_stencil_format;
}

static bool HasRequiredProperties(const vk::PhysicalDevice& physical_device) {
  auto properties = physical_device.getProperties();
  if (!(properties.limits.framebufferColorSampleCounts &
        (vk::SampleCountFlagBits::e1 | vk::SampleCountFlagBits::e4))) {
    return false;
  }
  return true;
}

static bool HasRequiredQueues(const vk::PhysicalDevice& physical_device) {
  auto queue_flags = vk::QueueFlags{};
  for (const auto& queue : physical_device.getQueueFamilyProperties()) {
    if (queue.queueCount == 0) {
      continue;
    }
    queue_flags |= queue.queueFlags;
  }
  return static_cast<VkQueueFlags>(queue_flags &
                                   (vk::QueueFlagBits::eGraphics |
                                    vk::QueueFlagBits::eCompute |
                                    vk::QueueFlagBits::eTransfer));
}

template <class ExtensionEnum>
static bool IsExtensionInList(const std::vector<std::string>& list,
                              ExtensionEnum ext) {
  const std::string name = GetExtensionName(ext);
  return std::find(list.begin(), list.end(), name) != list.end();
}

std::optional<CapabilitiesVK::PhysicalDeviceFeatures>
CapabilitiesVK::GetEnabledDeviceFeatures(
    const vk::PhysicalDevice& device) const {
  if (!PhysicalDeviceSupportsRequiredFormats(device)) {
    VALIDATION_LOG << "Device doesn't support the required formats.";
    return std::nullopt;
  }

  if (!HasRequiredProperties(device)) {
    VALIDATION_LOG << "Device doesn't support the required properties.";
    return std::nullopt;
  }

  if (!HasRequiredQueues(device)) {
    VALIDATION_LOG << "Device doesn't support the required queues.";
    return std::nullopt;
  }

  const auto enabled_extensions = GetEnabledDeviceExtensions(device);
  if (!enabled_extensions.has_value()) {
    VALIDATION_LOG << "Device doesn't support the required queues.";
    return std::nullopt;
  }

  PhysicalDeviceFeatures supported_chain;
  device.getFeatures2(&supported_chain.get());

  PhysicalDeviceFeatures required_chain;

  // Base features.
  {
    auto& required = required_chain.get().features;
    const auto& supported = supported_chain.get().features;

    // We require this for enabling wireframes in the playground. But its not
    // necessarily a big deal if we don't have this feature.
    required.fillModeNonSolid = supported.fillModeNonSolid;
  }
  // VK_KHR_sampler_ycbcr_conversion features.
  if (IsExtensionInList(
          enabled_extensions.value(),
          RequiredAndroidDeviceExtensionVK::kKHRSamplerYcbcrConversion)) {
    auto& required =
        required_chain
            .get<vk::PhysicalDeviceSamplerYcbcrConversionFeaturesKHR>();
    const auto& supported =
        supported_chain
            .get<vk::PhysicalDeviceSamplerYcbcrConversionFeaturesKHR>();

    required.samplerYcbcrConversion = supported.samplerYcbcrConversion;
  }

  return required_chain;
}

bool CapabilitiesVK::HasLayer(const std::string& layer) const {
  for (const auto& [found_layer, exts] : exts_) {
    if (found_layer == layer) {
      return true;
    }
  }
  return false;
}

bool CapabilitiesVK::HasExtension(const std::string& ext) const {
  for (const auto& [layer, exts] : exts_) {
    if (exts.find(ext) != exts.end()) {
      return true;
    }
  }
  return false;
}

void CapabilitiesVK::SetOffscreenFormat(PixelFormat pixel_format) const {
  default_color_format_ = pixel_format;
}

bool CapabilitiesVK::SetPhysicalDevice(const vk::PhysicalDevice& device) {
  if (HasSuitableDepthStencilFormat(device, vk::Format::eD32SfloatS8Uint)) {
    default_depth_stencil_format_ = PixelFormat::kD32FloatS8UInt;
  } else if (HasSuitableDepthStencilFormat(device,
                                           vk::Format::eD24UnormS8Uint)) {
    default_depth_stencil_format_ = PixelFormat::kD24UnormS8Uint;
  } else {
    default_depth_stencil_format_ = PixelFormat::kUnknown;
  }

  if (HasSuitableDepthStencilFormat(device, vk::Format::eS8Uint)) {
    default_stencil_format_ = PixelFormat::kS8UInt;
  } else if (default_depth_stencil_format_ != PixelFormat::kUnknown) {
    default_stencil_format_ = default_depth_stencil_format_;
  }

  device_properties_ = device.getProperties();

  auto physical_properties_2 =
      device.getProperties2<vk::PhysicalDeviceProperties2,
                            vk::PhysicalDeviceSubgroupProperties>();

  // Currently shaders only want access to arithmetic subgroup features.
  // If that changes this needs to get updated, and so does Metal (which right
  // now assumes it from compile time flags based on the MSL target version).

  supports_compute_subgroups_ =
      !!(physical_properties_2.get<vk::PhysicalDeviceSubgroupProperties>()
             .supportedOperations &
         vk::SubgroupFeatureFlagBits::eArithmetic);

  // Determine the optional device extensions this physical device supports.
  {
    required_common_device_extensions_.clear();
    required_android_device_extensions_.clear();
    optional_device_extensions_.clear();
    auto exts = GetSupportedDeviceExtensions(device);
    if (!exts.has_value()) {
      return false;
    }
    IterateExtensions<RequiredCommonDeviceExtensionVK>([&](auto ext) -> bool {
      auto ext_name = GetExtensionName(ext);
      if (exts->find(ext_name) != exts->end()) {
        required_common_device_extensions_.insert(ext);
      }
      return true;
    });
    IterateExtensions<RequiredAndroidDeviceExtensionVK>([&](auto ext) -> bool {
      auto ext_name = GetExtensionName(ext);
      if (exts->find(ext_name) != exts->end()) {
        required_android_device_extensions_.insert(ext);
      }
      return true;
    });
    IterateExtensions<OptionalDeviceExtensionVK>([&](auto ext) -> bool {
      auto ext_name = GetExtensionName(ext);
      if (exts->find(ext_name) != exts->end()) {
        optional_device_extensions_.insert(ext);
      }
      return true;
    });
  }

  return true;
}

// |Capabilities|
bool CapabilitiesVK::SupportsOffscreenMSAA() const {
  return true;
}

// |Capabilities|
bool CapabilitiesVK::SupportsImplicitResolvingMSAA() const {
  return false;
}

// |Capabilities|
bool CapabilitiesVK::SupportsSSBO() const {
  return true;
}

// |Capabilities|
bool CapabilitiesVK::SupportsBufferToTextureBlits() const {
  return true;
}

// |Capabilities|
bool CapabilitiesVK::SupportsTextureToTextureBlits() const {
  return true;
}

// |Capabilities|
bool CapabilitiesVK::SupportsFramebufferFetch() const {
  return true;
}

// |Capabilities|
bool CapabilitiesVK::SupportsCompute() const {
  // Vulkan 1.1 requires support for compute.
  return true;
}

// |Capabilities|
bool CapabilitiesVK::SupportsComputeSubgroups() const {
  // Set by |SetPhysicalDevice|.
  return supports_compute_subgroups_;
}

// |Capabilities|
bool CapabilitiesVK::SupportsReadFromResolve() const {
  return false;
}

bool CapabilitiesVK::SupportsDecalSamplerAddressMode() const {
  return true;
}

// |Capabilities|
PixelFormat CapabilitiesVK::GetDefaultColorFormat() const {
  return default_color_format_;
}

// |Capabilities|
PixelFormat CapabilitiesVK::GetDefaultStencilFormat() const {
  return default_stencil_format_;
}

// |Capabilities|
PixelFormat CapabilitiesVK::GetDefaultDepthStencilFormat() const {
  return default_depth_stencil_format_;
}

const vk::PhysicalDeviceProperties&
CapabilitiesVK::GetPhysicalDeviceProperties() const {
  return device_properties_;
}

PixelFormat CapabilitiesVK::GetDefaultGlyphAtlasFormat() const {
  return PixelFormat::kR8UNormInt;
}

bool CapabilitiesVK::HasExtension(RequiredCommonDeviceExtensionVK ext) const {
  return required_common_device_extensions_.find(ext) !=
         required_common_device_extensions_.end();
}

bool CapabilitiesVK::HasExtension(RequiredAndroidDeviceExtensionVK ext) const {
  return required_android_device_extensions_.find(ext) !=
         required_android_device_extensions_.end();
}

bool CapabilitiesVK::HasExtension(OptionalDeviceExtensionVK ext) const {
  return optional_device_extensions_.find(ext) !=
         optional_device_extensions_.end();
}

}  // namespace impeller
