// stygian_ap_vk.c - Vulkan 1.0+ access-point backend

#include "../include/stygian.h"
#include "../include/stygian_memory.h"
#include "../src/stygian_internal.h" // stygian_cpystr
#include "../window/stygian_window.h"
#include "stygian_ap.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <vulkan/vulkan.h>

#define STYGIAN_VK_IMAGE_SAMPLERS 16
#define STYGIAN_VK_MAX_SWAPCHAIN_IMAGES 3
#define STYGIAN_VK_FRAMES_IN_FLIGHT 3

// ============================================================================
// Vulkan Access Point Structure
// ============================================================================

struct StygianAP {
  // Vulkan core
  VkInstance instance;
  VkPhysicalDevice physical_device;
  VkDevice device;
  VkQueue graphics_queue;
  uint32_t graphics_family;

  // Swapchain
  VkSurfaceKHR surface;
  VkSwapchainKHR swapchain;
  VkImage swapchain_images[STYGIAN_VK_MAX_SWAPCHAIN_IMAGES];
  VkImageView swapchain_views[STYGIAN_VK_MAX_SWAPCHAIN_IMAGES];
  VkFramebuffer framebuffers[STYGIAN_VK_MAX_SWAPCHAIN_IMAGES];
  uint32_t swapchain_image_count;
  VkFormat swapchain_format;
  VkExtent2D swapchain_extent;

  // Render pass & pipeline
  VkRenderPass render_pass;
  VkPipelineLayout pipeline_layout;
  VkPipeline graphics_pipeline;

  // Resources
  VkBuffer clip_ssbo;
  VkDeviceMemory clip_ssbo_memory;
  VkBuffer vertex_buffer;
  VkDeviceMemory vertex_memory;

  // Descriptors
  VkDescriptorSetLayout descriptor_layout;
  VkDescriptorPool descriptor_pool;
  VkDescriptorSet descriptor_set;

  // Command buffers
  VkCommandPool command_pool;
  VkCommandBuffer command_buffers[STYGIAN_VK_FRAMES_IN_FLIGHT];

  // Synchronization
  VkSemaphore image_available[STYGIAN_VK_FRAMES_IN_FLIGHT];
  VkSemaphore render_finished[STYGIAN_VK_FRAMES_IN_FLIGHT];
  VkFence in_flight[STYGIAN_VK_FRAMES_IN_FLIGHT];
  VkFence image_in_flight[STYGIAN_VK_MAX_SWAPCHAIN_IMAGES];
  uint32_t current_frame;
  uint32_t current_image;
  bool frame_active;
  bool swapchain_needs_recreate;
  int resize_pending_w;
  int resize_pending_h;
  uint32_t resize_stable_count;
  uint32_t resize_debounce_frames;
  uint32_t resize_suboptimal_count;
  uint32_t resize_suboptimal_threshold;
  bool resize_telemetry_enabled;
  uint32_t resize_telemetry_period;
  uint32_t resize_telemetry_frames;
  uint32_t resize_telemetry_recreate_count;
  double resize_telemetry_acquire_ms;
  double resize_telemetry_submit_ms;
  double resize_telemetry_present_ms;
  double resize_telemetry_recreate_ms;

  // Shader modules
  VkShaderModule vert_module;
  VkShaderModule frag_module;

  // Font texture (placeholder for now)
  VkImage font_image;
  VkDeviceMemory font_memory;
  VkImageView font_view;
  VkSampler font_sampler;

  // Config
  char shader_dir[256];
  uint32_t max_elements;
  uint32_t element_count;
  StygianAllocator *allocator;
  uint32_t last_upload_bytes;
  uint32_t last_upload_ranges;
  float last_gpu_ms;
  StygianWindow *window;
  bool initialized;
  StygianAPAdapterClass adapter_class;
  uint32_t graphics_timestamp_valid_bits;
  float gpu_timestamp_period_ns;
  bool gpu_timer_supported;
  VkQueryPool gpu_timer_query_pool;
  float atlas_width;
  float atlas_height;
  float px_range;
  bool output_color_transform_enabled;
  float output_color_matrix[9];
  bool output_src_srgb_transfer;
  float output_src_gamma;
  bool output_dst_srgb_transfer;
  float output_dst_gamma;

  // SoA SSBOs (bindings 4/5/6 — mirrors GL backend)
  VkBuffer soa_hot_buf;
  VkDeviceMemory soa_hot_mem;
  VkBuffer soa_appearance_buf;
  VkDeviceMemory soa_appearance_mem;
  VkBuffer soa_effects_buf;
  VkDeviceMemory soa_effects_mem;

  // Per-chunk GPU version tracking (for dirty range upload)
  uint32_t *gpu_hot_versions;
  uint32_t *gpu_appearance_versions;
  uint32_t *gpu_effects_versions;
  uint32_t soa_chunk_count;

  // Main surface (embedded for the primary window)
  struct StygianAPSurface *main_surface;
};

// ============================================================================
// Per-Window Surface (for multi-window support)
// ============================================================================

struct StygianAPSurface {
  StygianAP *ap;         // Parent AP (shared resources)
  StygianWindow *window; // Associated window

  // Vulkan surface resources
  VkSurfaceKHR surface;
  VkSwapchainKHR swapchain;
  VkImage swapchain_images[STYGIAN_VK_MAX_SWAPCHAIN_IMAGES];
  VkImageView swapchain_views[STYGIAN_VK_MAX_SWAPCHAIN_IMAGES];
  VkFramebuffer framebuffers[STYGIAN_VK_MAX_SWAPCHAIN_IMAGES];
  uint32_t image_count;
  VkFormat format;
  VkExtent2D extent;

  // Frame state
  uint32_t current_image;
  VkSemaphore image_available;
  VkSemaphore render_finished;
  VkFence in_flight;
  VkCommandBuffer command_buffer;
  bool frame_active;
  bool swapchain_needs_recreate;
  int resize_pending_w;
  int resize_pending_h;
  uint32_t resize_stable_count;
  uint32_t resize_debounce_frames;
  uint32_t resize_suboptimal_count;
};

typedef struct StygianVKPushConstants {
  float screen_atlas[4];   // x=screen w, y=screen h, z=atlas w, w=atlas h
  float px_range_flags[4]; // x=px range, y=enabled, z=src sRGB, w=dst sRGB
  float output_row0[4];
  float output_row1[4];
  float output_row2[4];
  float gamma[4]; // x=src gamma, y=dst gamma
} StygianVKPushConstants;

// Forward declaration (used by create() error path).
void stygian_ap_destroy(StygianAP *ap);

// Allocator helpers: use AP allocator when set, else CRT (bootstrap/fallback)
static void *ap_alloc(StygianAP *ap, size_t size, size_t alignment) {
  if (ap->allocator && ap->allocator->alloc)
    return ap->allocator->alloc(ap->allocator, size, alignment);
  (void)alignment;
  return malloc(size);
}
static void ap_free(StygianAP *ap, void *ptr) {
  if (!ptr)
    return;
  if (ap->allocator && ap->allocator->free)
    ap->allocator->free(ap->allocator, ptr);
  else
    free(ptr);
}

// Config-based allocator helpers for bootstrap (before AP struct exists)
static void *cfg_alloc(StygianAllocator *allocator, size_t size,
                       size_t alignment) {
  if (allocator && allocator->alloc)
    return allocator->alloc(allocator, size, alignment);
  (void)alignment;
  return malloc(size);
}
static void cfg_free(StygianAllocator *allocator, void *ptr) {
  if (!ptr)
    return;
  if (allocator && allocator->free)
    allocator->free(allocator, ptr);
  else
    free(ptr);
}

// copy_cstr removed — use stygian_cpystr from stygian_internal.h

static double stygian_vk_now_ms(void) {
  struct timespec ts;
  timespec_get(&ts, TIME_UTC);
  return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}

static void fill_push_constants(const StygianAP *ap, float screen_w,
                                float screen_h,
                                StygianVKPushConstants *out_pc) {
  if (!ap || !out_pc)
    return;
  memset(out_pc, 0, sizeof(*out_pc));
  out_pc->screen_atlas[0] = screen_w;
  out_pc->screen_atlas[1] = screen_h;
  out_pc->screen_atlas[2] = ap->atlas_width;
  out_pc->screen_atlas[3] = ap->atlas_height;
  out_pc->px_range_flags[0] = ap->px_range;
  out_pc->px_range_flags[1] = ap->output_color_transform_enabled ? 1.0f : 0.0f;
  out_pc->px_range_flags[2] = ap->output_src_srgb_transfer ? 1.0f : 0.0f;
  out_pc->px_range_flags[3] = ap->output_dst_srgb_transfer ? 1.0f : 0.0f;

  out_pc->output_row0[0] = ap->output_color_matrix[0];
  out_pc->output_row0[1] = ap->output_color_matrix[1];
  out_pc->output_row0[2] = ap->output_color_matrix[2];
  out_pc->output_row1[0] = ap->output_color_matrix[3];
  out_pc->output_row1[1] = ap->output_color_matrix[4];
  out_pc->output_row1[2] = ap->output_color_matrix[5];
  out_pc->output_row2[0] = ap->output_color_matrix[6];
  out_pc->output_row2[1] = ap->output_color_matrix[7];
  out_pc->output_row2[2] = ap->output_color_matrix[8];
  out_pc->gamma[0] = ap->output_src_gamma;
  out_pc->gamma[1] = ap->output_dst_gamma;
}

static void update_image_sampler_array(StygianAP *ap) {
  if (!ap || ap->font_sampler == VK_NULL_HANDLE ||
      ap->font_view == VK_NULL_HANDLE)
    return;

  VkDescriptorImageInfo image_infos[STYGIAN_VK_IMAGE_SAMPLERS];
  for (uint32_t i = 0; i < STYGIAN_VK_IMAGE_SAMPLERS; ++i) {
    image_infos[i].sampler = ap->font_sampler;
    image_infos[i].imageView = ap->font_view;
    image_infos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  }

  VkWriteDescriptorSet image_write = {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = ap->descriptor_set,
      .dstBinding = 2,
      .dstArrayElement = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = STYGIAN_VK_IMAGE_SAMPLERS,
      .pImageInfo = image_infos,
  };
  vkUpdateDescriptorSets(ap->device, 1, &image_write, 0, NULL);
}

// ============================================================================
// Helper Functions
// ============================================================================

static VkShaderModule load_shader_module(StygianAP *ap, const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    printf("[Stygian AP VK] Failed to open shader: %s\n", path);
    return VK_NULL_HANDLE;
  }

  fseek(f, 0, SEEK_END);
  size_t size = ftell(f);
  fseek(f, 0, SEEK_SET);

  uint32_t *code = (uint32_t *)ap_alloc(ap, size, _Alignof(uint32_t));
  fread(code, 1, size, f);
  fclose(f);

  VkShaderModuleCreateInfo create_info = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = size,
      .pCode = code,
  };

  VkShaderModule module;
  VkResult result =
      vkCreateShaderModule(ap->device, &create_info, NULL, &module);
  ap_free(ap, code);

  if (result != VK_SUCCESS) {
    printf("[Stygian AP VK] Failed to create shader module: %d\n", result);
    return VK_NULL_HANDLE;
  }

  return module;
}

static uint32_t find_memory_type(VkPhysicalDevice physical_device,
                                 uint32_t type_filter,
                                 VkMemoryPropertyFlags properties) {
  VkPhysicalDeviceMemoryProperties mem_props;
  vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_props);

  for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
    if ((type_filter & (1 << i)) &&
        (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
      return i;
    }
  }

  return UINT32_MAX;
}

// ============================================================================
// Vulkan Initialization
// ============================================================================

static bool create_instance(StygianAP *ap) {
  VkApplicationInfo app_info = {
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pApplicationName = "Stygian UI",
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName = "Stygian",
      .engineVersion = VK_MAKE_VERSION(1, 0, 0),
      .apiVersion = VK_API_VERSION_1_0,
  };

  const char *extensions[8] = {0};
  uint32_t ext_count = stygian_window_vk_get_instance_extensions(extensions, 8);

  VkInstanceCreateInfo create_info = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pApplicationInfo = &app_info,
      .enabledExtensionCount = ext_count,
      .ppEnabledExtensionNames = extensions,
  };

  VkResult result = vkCreateInstance(&create_info, NULL, &ap->instance);
  if (result != VK_SUCCESS) {
    printf("[Stygian AP VK] Failed to create instance: %d\n", result);
    return false;
  }

  printf("[Stygian AP VK] Instance created\n");
  return true;
}

static bool pick_physical_device(StygianAP *ap) {
  uint32_t device_count = 0;
  vkEnumeratePhysicalDevices(ap->instance, &device_count, NULL);

  if (device_count == 0) {
    printf("[Stygian AP VK] No Vulkan devices found\n");
    return false;
  }

  VkPhysicalDevice devices[8];
  if (device_count > 8)
    device_count = 8;
  vkEnumeratePhysicalDevices(ap->instance, &device_count, devices);

  // Pick first discrete GPU, or fallback to first device
  ap->physical_device = devices[0];
  ap->adapter_class = STYGIAN_AP_ADAPTER_UNKNOWN;
  for (uint32_t i = 0; i < device_count; i++) {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(devices[i], &props);

    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
      ap->physical_device = devices[i];
      ap->adapter_class = STYGIAN_AP_ADAPTER_DGPU;
      printf("[Stygian AP VK] Selected GPU: %s\n", props.deviceName);
      break;
    }
  }

  if (ap->adapter_class == STYGIAN_AP_ADAPTER_UNKNOWN) {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(ap->physical_device, &props);
    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
      ap->adapter_class = STYGIAN_AP_ADAPTER_IGPU;
    } else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
      ap->adapter_class = STYGIAN_AP_ADAPTER_DGPU;
    }
  }

  return true;
}

static bool find_queue_families(StygianAP *ap) {
  uint32_t queue_family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(ap->physical_device,
                                           &queue_family_count, NULL);

  VkQueueFamilyProperties queue_families[16];
  if (queue_family_count > 16)
    queue_family_count = 16;
  vkGetPhysicalDeviceQueueFamilyProperties(ap->physical_device,
                                           &queue_family_count, queue_families);

  // Find graphics queue family
  for (uint32_t i = 0; i < queue_family_count; i++) {
    if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      ap->graphics_family = i;
      ap->graphics_timestamp_valid_bits = queue_families[i].timestampValidBits;
      return true;
    }
  }

  printf("[Stygian AP VK] No graphics queue family found\n");
  return false;
}

static bool create_logical_device(StygianAP *ap) {
  float queue_priority = 1.0f;
  VkDeviceQueueCreateInfo queue_create_info = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueFamilyIndex = ap->graphics_family,
      .queueCount = 1,
      .pQueuePriorities = &queue_priority,
  };

  const char *device_extensions[] = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  };

  VkPhysicalDeviceFeatures device_features = {0};

  VkDeviceCreateInfo create_info = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .queueCreateInfoCount = 1,
      .pQueueCreateInfos = &queue_create_info,
      .enabledExtensionCount = 1,
      .ppEnabledExtensionNames = device_extensions,
      .pEnabledFeatures = &device_features,
  };

  VkResult result =
      vkCreateDevice(ap->physical_device, &create_info, NULL, &ap->device);
  if (result != VK_SUCCESS) {
    printf("[Stygian AP VK] Failed to create logical device: %d\n", result);
    return false;
  }

  vkGetDeviceQueue(ap->device, ap->graphics_family, 0, &ap->graphics_queue);
  printf("[Stygian AP VK] Logical device created\n");
  return true;
}

static void init_gpu_timer_support(StygianAP *ap) {
  VkPhysicalDeviceProperties props;
  VkQueryPoolCreateInfo query_pool_info;

  if (!ap)
    return;

  ap->last_gpu_ms = 0.0f;
  ap->gpu_timer_supported = false;
  ap->gpu_timestamp_period_ns = 0.0f;
  ap->gpu_timer_query_pool = VK_NULL_HANDLE;

  vkGetPhysicalDeviceProperties(ap->physical_device, &props);
  ap->gpu_timestamp_period_ns = props.limits.timestampPeriod;

  if (!props.limits.timestampComputeAndGraphics ||
      ap->graphics_timestamp_valid_bits == 0 ||
      ap->gpu_timestamp_period_ns <= 0.0f) {
    printf("[Stygian AP VK] GPU timer unavailable "
           "(timestampComputeAndGraphics=%u, validBits=%u)\n",
           props.limits.timestampComputeAndGraphics ? 1u : 0u,
           ap->graphics_timestamp_valid_bits);
    return;
  }

  memset(&query_pool_info, 0, sizeof(query_pool_info));
  query_pool_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
  query_pool_info.queryType = VK_QUERY_TYPE_TIMESTAMP;
  query_pool_info.queryCount = STYGIAN_VK_FRAMES_IN_FLIGHT * 2u;

  if (vkCreateQueryPool(ap->device, &query_pool_info, NULL,
                        &ap->gpu_timer_query_pool) != VK_SUCCESS) {
    printf("[Stygian AP VK] Failed to create GPU timer query pool\n");
    ap->gpu_timer_query_pool = VK_NULL_HANDLE;
    return;
  }

  ap->gpu_timer_supported = true;
}

static void update_gpu_timer_sample_for_frame(StygianAP *ap,
                                              uint32_t frame_slot) {
  uint64_t timestamps[2];
  uint64_t delta;
  uint32_t query_base;
  VkResult result;

  if (!ap || !ap->gpu_timer_supported || !ap->gpu_timer_query_pool)
    return;

  query_base = frame_slot * 2u;
  timestamps[0] = 0u;
  timestamps[1] = 0u;
  // Non-blocking read: if timestamps are not ready yet, keep previous sample.
  result = vkGetQueryPoolResults(
      ap->device, ap->gpu_timer_query_pool, query_base, 2u, sizeof(timestamps),
      timestamps, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
  if (result != VK_SUCCESS)
    return;
  if (timestamps[1] <= timestamps[0])
    return;

  delta = timestamps[1] - timestamps[0];
  ap->last_gpu_ms = (float)(((double)delta * (double)ap->gpu_timestamp_period_ns) /
                            1000000.0);
}

static bool create_surface(StygianAP *ap) {
  if (!stygian_window_vk_create_surface(ap->window, ap->instance,
                                        (void **)&ap->surface)) {
    printf("[Stygian AP VK] Failed to create surface\n");
    return false;
  }

  // Verify surface support for our queue family
  VkBool32 supported = VK_FALSE;
  vkGetPhysicalDeviceSurfaceSupportKHR(ap->physical_device, ap->graphics_family,
                                       ap->surface, &supported);
  if (!supported) {
    printf("[Stygian AP VK] Surface not supported by queue family\n");
    return false;
  }

  printf("[Stygian AP VK] Surface created\n");
  return true;
}

static bool create_swapchain(StygianAP *ap, int width, int height,
                             VkSwapchainKHR old_swapchain) {
  // Query surface capabilities
  VkSurfaceCapabilitiesKHR capabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ap->physical_device, ap->surface,
                                            &capabilities);

  // Query surface formats
  uint32_t format_count;
  vkGetPhysicalDeviceSurfaceFormatsKHR(ap->physical_device, ap->surface,
                                       &format_count, NULL);
  VkSurfaceFormatKHR formats[16];
  if (format_count > 16)
    format_count = 16;
  vkGetPhysicalDeviceSurfaceFormatsKHR(ap->physical_device, ap->surface,
                                       &format_count, formats);

  // Pick format (prefer BGRA8 UNORM for linear colors like OpenGL)
  VkSurfaceFormatKHR surface_format = formats[0];
  for (uint32_t i = 0; i < format_count; i++) {
    if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM &&
        formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      surface_format = formats[i];
      break;
    }
  }

  // Query present modes
  uint32_t present_mode_count;
  vkGetPhysicalDeviceSurfacePresentModesKHR(ap->physical_device, ap->surface,
                                            &present_mode_count, NULL);
  VkPresentModeKHR present_modes[8];
  if (present_mode_count > 8)
    present_mode_count = 8;
  vkGetPhysicalDeviceSurfacePresentModesKHR(ap->physical_device, ap->surface,
                                            &present_mode_count, present_modes);

  // Pick present mode (prefer FIFO for vsync)
  VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;

  // Determine extent
  VkExtent2D extent;
  if (capabilities.currentExtent.width != UINT32_MAX) {
    extent = capabilities.currentExtent;
  } else {
    extent.width = width;
    extent.height = height;

    // Clamp to min/max
    if (extent.width < capabilities.minImageExtent.width)
      extent.width = capabilities.minImageExtent.width;
    if (extent.width > capabilities.maxImageExtent.width)
      extent.width = capabilities.maxImageExtent.width;
    if (extent.height < capabilities.minImageExtent.height)
      extent.height = capabilities.minImageExtent.height;
    if (extent.height > capabilities.maxImageExtent.height)
      extent.height = capabilities.maxImageExtent.height;
  }

  // Determine image count (triple buffering)
  uint32_t image_count = 3;
  if (capabilities.maxImageCount > 0 &&
      image_count > capabilities.maxImageCount)
    image_count = capabilities.maxImageCount;
  if (image_count < capabilities.minImageCount)
    image_count = capabilities.minImageCount;

  VkSwapchainCreateInfoKHR create_info = {
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .surface = ap->surface,
      .minImageCount = image_count,
      .imageFormat = surface_format.format,
      .imageColorSpace = surface_format.colorSpace,
      .imageExtent = extent,
      .imageArrayLayers = 1,
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .preTransform = capabilities.currentTransform,
      .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      .presentMode = present_mode,
      .clipped = VK_TRUE,
      .oldSwapchain = old_swapchain,
  };

  VkSwapchainKHR new_swapchain = VK_NULL_HANDLE;
  VkResult result =
      vkCreateSwapchainKHR(ap->device, &create_info, NULL, &new_swapchain);
  if (result != VK_SUCCESS) {
    printf("[Stygian AP VK] Failed to create swapchain: %d\n", result);
    return false;
  }

  // Get swapchain images
  vkGetSwapchainImagesKHR(ap->device, new_swapchain, &ap->swapchain_image_count,
                          NULL);
  if (ap->swapchain_image_count > STYGIAN_VK_MAX_SWAPCHAIN_IMAGES)
    ap->swapchain_image_count = STYGIAN_VK_MAX_SWAPCHAIN_IMAGES;
  vkGetSwapchainImagesKHR(ap->device, new_swapchain, &ap->swapchain_image_count,
                          ap->swapchain_images);

  ap->swapchain_format = surface_format.format;
  ap->swapchain_extent = extent;
  ap->swapchain = new_swapchain;

  printf("[Stygian AP VK] Swapchain created: %dx%d, %d images\n", extent.width,
         extent.height, ap->swapchain_image_count);
  return true;
}

static bool create_image_views(StygianAP *ap) {
  for (uint32_t i = 0; i < ap->swapchain_image_count; i++) {
    VkImageViewCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = ap->swapchain_images[i],
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = ap->swapchain_format,
        .components =
            {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    VkResult result = vkCreateImageView(ap->device, &create_info, NULL,
                                        &ap->swapchain_views[i]);
    if (result != VK_SUCCESS) {
      printf("[Stygian AP VK] Failed to create image view %d: %d\n", i, result);
      return false;
    }
  }

  printf("[Stygian AP VK] Image views created\n");
  return true;
}

static bool create_render_pass(StygianAP *ap) {
  VkAttachmentDescription color_attachment = {
      .format = ap->swapchain_format,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
  };

  VkAttachmentReference color_attachment_ref = {
      .attachment = 0,
      .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
  };

  VkSubpassDescription subpass = {
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .colorAttachmentCount = 1,
      .pColorAttachments = &color_attachment_ref,
  };

  VkSubpassDependency dependency = {
      .srcSubpass = VK_SUBPASS_EXTERNAL,
      .dstSubpass = 0,
      .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      .srcAccessMask = 0,
      .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
  };

  VkRenderPassCreateInfo render_pass_info = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .attachmentCount = 1,
      .pAttachments = &color_attachment,
      .subpassCount = 1,
      .pSubpasses = &subpass,
      .dependencyCount = 1,
      .pDependencies = &dependency,
  };

  VkResult result =
      vkCreateRenderPass(ap->device, &render_pass_info, NULL, &ap->render_pass);
  if (result != VK_SUCCESS) {
    printf("[Stygian AP VK] Failed to create render pass: %d\n", result);
    return false;
  }

  printf("[Stygian AP VK] Render pass created\n");
  return true;
}

static bool create_framebuffers(StygianAP *ap) {
  for (uint32_t i = 0; i < ap->swapchain_image_count; i++) {
    VkImageView attachments[] = {ap->swapchain_views[i]};

    VkFramebufferCreateInfo framebuffer_info = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = ap->render_pass,
        .attachmentCount = 1,
        .pAttachments = attachments,
        .width = ap->swapchain_extent.width,
        .height = ap->swapchain_extent.height,
        .layers = 1,
    };

    VkResult result = vkCreateFramebuffer(ap->device, &framebuffer_info, NULL,
                                          &ap->framebuffers[i]);
    if (result != VK_SUCCESS) {
      printf("[Stygian AP VK] Failed to create framebuffer %d: %d\n", i,
             result);
      return false;
    }
  }

  printf("[Stygian AP VK] Framebuffers created\n");
  return true;
}

static void cleanup_main_swapchain_attachments(StygianAP *ap) {
  if (!ap || !ap->device)
    return;

  for (uint32_t i = 0; i < ap->swapchain_image_count; i++) {
    if (ap->framebuffers[i]) {
      vkDestroyFramebuffer(ap->device, ap->framebuffers[i], NULL);
      ap->framebuffers[i] = VK_NULL_HANDLE;
    }
    if (ap->swapchain_views[i]) {
      vkDestroyImageView(ap->device, ap->swapchain_views[i], NULL);
      ap->swapchain_views[i] = VK_NULL_HANDLE;
    }
  }

  ap->swapchain_image_count = 0;
}

static void cleanup_main_swapchain_resources(StygianAP *ap) {
  if (!ap || !ap->device)
    return;
  cleanup_main_swapchain_attachments(ap);
  if (ap->swapchain) {
    vkDestroySwapchainKHR(ap->device, ap->swapchain, NULL);
    ap->swapchain = VK_NULL_HANDLE;
  }
}

static bool recreate_main_swapchain(StygianAP *ap, int width, int height) {
  if (!ap || !ap->device)
    return false;
  double t0 = stygian_vk_now_ms();

  if (width <= 0 || height <= 0) {
    stygian_window_get_framebuffer_size(ap->window, &width, &height);
    if (width <= 0 || height <= 0) {
      return false; // Minimized window, skip recreate for now.
    }
  }

  // Wait for in-flight frames instead of idling the whole queue.
  vkWaitForFences(ap->device, STYGIAN_VK_FRAMES_IN_FLIGHT, ap->in_flight,
                  VK_TRUE, UINT64_MAX);
  VkSwapchainKHR old_swapchain = ap->swapchain;
  cleanup_main_swapchain_attachments(ap);

  if (!create_swapchain(ap, width, height, old_swapchain)) {
    return false;
  }
  if (!create_image_views(ap)) {
    cleanup_main_swapchain_resources(ap);
    return false;
  }
  if (!create_framebuffers(ap)) {
    cleanup_main_swapchain_resources(ap);
    return false;
  }

  for (uint32_t i = 0; i < STYGIAN_VK_MAX_SWAPCHAIN_IMAGES; ++i) {
    ap->image_in_flight[i] = VK_NULL_HANDLE;
  }

  if (old_swapchain) {
    vkDestroySwapchainKHR(ap->device, old_swapchain, NULL);
  }

  if (ap->resize_telemetry_enabled) {
    ap->resize_telemetry_recreate_ms += stygian_vk_now_ms() - t0;
    ap->resize_telemetry_recreate_count++;
  }

  return true;
}

static bool create_command_pool(StygianAP *ap) {
  VkCommandPoolCreateInfo pool_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = ap->graphics_family,
  };

  VkResult result =
      vkCreateCommandPool(ap->device, &pool_info, NULL, &ap->command_pool);
  if (result != VK_SUCCESS) {
    printf("[Stygian AP VK] Failed to create command pool: %d\n", result);
    return false;
  }

  // Allocate command buffers (2 for double buffering)
  VkCommandBufferAllocateInfo alloc_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = ap->command_pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = STYGIAN_VK_FRAMES_IN_FLIGHT,
  };

  result =
      vkAllocateCommandBuffers(ap->device, &alloc_info, ap->command_buffers);
  if (result != VK_SUCCESS) {
    printf("[Stygian AP VK] Failed to allocate command buffers: %d\n", result);
    return false;
  }

  printf("[Stygian AP VK] Command pool and buffers created\n");
  return true;
}

static bool create_sync_objects(StygianAP *ap) {
  VkSemaphoreCreateInfo semaphore_info = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
  };

  VkFenceCreateInfo fence_info = {
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .flags = VK_FENCE_CREATE_SIGNALED_BIT, // Start signaled
  };

  for (int i = 0; i < STYGIAN_VK_FRAMES_IN_FLIGHT; i++) {
    if (vkCreateSemaphore(ap->device, &semaphore_info, NULL,
                          &ap->image_available[i]) != VK_SUCCESS ||
        vkCreateSemaphore(ap->device, &semaphore_info, NULL,
                          &ap->render_finished[i]) != VK_SUCCESS ||
        vkCreateFence(ap->device, &fence_info, NULL, &ap->in_flight[i]) !=
            VK_SUCCESS) {
      printf("[Stygian AP VK] Failed to create sync objects\n");
      return false;
    }
  }

  ap->current_frame = 0;
  printf("[Stygian AP VK] Sync objects created\n");
  return true;
}

static bool create_buffers(StygianAP *ap) {
  VkMemoryRequirements mem_requirements;
  VkBufferCreateInfo buffer_info = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
  };
  VkMemoryAllocateInfo alloc_info = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
  };

  // Create clip SSBO (vec4 clip rects)
  VkDeviceSize clip_size = STYGIAN_MAX_CLIPS * sizeof(float) * 4;
  buffer_info.size = clip_size;
  buffer_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

  if (vkCreateBuffer(ap->device, &buffer_info, NULL, &ap->clip_ssbo) !=
      VK_SUCCESS) {
    printf("[Stygian AP VK] Failed to create clip SSBO\n");
    return false;
  }

  vkGetBufferMemoryRequirements(ap->device, ap->clip_ssbo, &mem_requirements);
  alloc_info.allocationSize = mem_requirements.size;
  alloc_info.memoryTypeIndex =
      find_memory_type(ap->physical_device, mem_requirements.memoryTypeBits,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  if (vkAllocateMemory(ap->device, &alloc_info, NULL, &ap->clip_ssbo_memory) !=
      VK_SUCCESS) {
    printf("[Stygian AP VK] Failed to allocate clip SSBO memory\n");
    return false;
  }
  vkBindBufferMemory(ap->device, ap->clip_ssbo, ap->clip_ssbo_memory, 0);

  // Create SoA SSBOs (hot=binding 4, appearance=binding 5, effects=binding 6)
  struct {
    VkBuffer *buf;
    VkDeviceMemory *mem;
    VkDeviceSize size;
    const char *name;
  } soa_bufs[3] = {
      {&ap->soa_hot_buf, &ap->soa_hot_mem,
       ap->max_elements * sizeof(StygianSoAHot), "hot"},
      {&ap->soa_appearance_buf, &ap->soa_appearance_mem,
       ap->max_elements * sizeof(StygianSoAAppearance), "appearance"},
      {&ap->soa_effects_buf, &ap->soa_effects_mem,
       ap->max_elements * sizeof(StygianSoAEffects), "effects"},
  };

  for (int si = 0; si < 3; si++) {
    buffer_info.size = soa_bufs[si].size;
    buffer_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    if (vkCreateBuffer(ap->device, &buffer_info, NULL, soa_bufs[si].buf) !=
        VK_SUCCESS) {
      printf("[Stygian AP VK] Failed to create SoA %s SSBO\n",
             soa_bufs[si].name);
      return false;
    }

    vkGetBufferMemoryRequirements(ap->device, *soa_bufs[si].buf,
                                  &mem_requirements);
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex =
        find_memory_type(ap->physical_device, mem_requirements.memoryTypeBits,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(ap->device, &alloc_info, NULL, soa_bufs[si].mem) !=
        VK_SUCCESS) {
      printf("[Stygian AP VK] Failed to allocate SoA %s memory\n",
             soa_bufs[si].name);
      return false;
    }

    vkBindBufferMemory(ap->device, *soa_bufs[si].buf, *soa_bufs[si].mem, 0);
  }

  printf("[Stygian AP VK] SoA SSBOs created (hot: %zu, appearance: %zu, "
         "effects: %zu bytes)\n",
         (size_t)soa_bufs[0].size, (size_t)soa_bufs[1].size,
         (size_t)soa_bufs[2].size);

  // Create vertex buffer (quad: 6 vertices)
  float quad_vertices[] = {
      -1.0f, -1.0f, 1.0f, -1.0f, 1.0f,  1.0f,
      -1.0f, -1.0f, 1.0f, 1.0f,  -1.0f, 1.0f,
  };

  VkDeviceSize vb_size = sizeof(quad_vertices);

  buffer_info.size = vb_size;
  buffer_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

  if (vkCreateBuffer(ap->device, &buffer_info, NULL, &ap->vertex_buffer) !=
      VK_SUCCESS) {
    printf("[Stygian AP VK] Failed to create vertex buffer\n");
    return false;
  }

  vkGetBufferMemoryRequirements(ap->device, ap->vertex_buffer,
                                &mem_requirements);

  alloc_info.allocationSize = mem_requirements.size;
  alloc_info.memoryTypeIndex =
      find_memory_type(ap->physical_device, mem_requirements.memoryTypeBits,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  if (vkAllocateMemory(ap->device, &alloc_info, NULL, &ap->vertex_memory) !=
      VK_SUCCESS) {
    printf("[Stygian AP VK] Failed to allocate vertex buffer memory\n");
    return false;
  }

  vkBindBufferMemory(ap->device, ap->vertex_buffer, ap->vertex_memory, 0);

  // Upload vertex data
  void *data;
  vkMapMemory(ap->device, ap->vertex_memory, 0, vb_size, 0, &data);
  memcpy(data, quad_vertices, vb_size);
  vkUnmapMemory(ap->device, ap->vertex_memory);

  printf("[Stygian AP VK] Buffers created (SoA hot/app/fx: %zu/%zu/%zu, "
         "Clip: %zu, VB: %zu bytes)\n",
         (size_t)soa_bufs[0].size, (size_t)soa_bufs[1].size,
         (size_t)soa_bufs[2].size, (size_t)clip_size, (size_t)vb_size);
  return true;
}

static bool create_descriptor_sets(StygianAP *ap) {
  // Descriptor set layout:
  // 1 = font sampler, 2 = image sampler array, 3 = clip SSBO,
  // 4 = SoA hot, 5 = SoA appearance, 6 = SoA effects
  VkDescriptorSetLayoutBinding bindings[6] = {
      {
          .binding = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      },
      {
          .binding = 2,
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .descriptorCount = STYGIAN_VK_IMAGE_SAMPLERS,
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      },
      {
          .binding = 3,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      },
      {
          .binding = 4,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .descriptorCount = 1,
          .stageFlags =
              VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
      },
      {
          .binding = 5,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .descriptorCount = 1,
          .stageFlags =
              VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
      },
      {
          .binding = 6,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .descriptorCount = 1,
          .stageFlags =
              VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
      },
  };

  VkDescriptorSetLayoutCreateInfo layout_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 6,
      .pBindings = bindings,
  };

  if (vkCreateDescriptorSetLayout(ap->device, &layout_info, NULL,
                                  &ap->descriptor_layout) != VK_SUCCESS) {
    printf("[Stygian AP VK] Failed to create descriptor set layout\n");
    return false;
  }

  // Descriptor pool (4 storage buffers: clip + hot + appearance + effects)
  VkDescriptorPoolSize pool_sizes[2] = {
      {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 4},
      {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
       .descriptorCount = 1 + STYGIAN_VK_IMAGE_SAMPLERS},
  };

  VkDescriptorPoolCreateInfo pool_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .poolSizeCount = 2,
      .pPoolSizes = pool_sizes,
      .maxSets = 1,
  };

  if (vkCreateDescriptorPool(ap->device, &pool_info, NULL,
                             &ap->descriptor_pool) != VK_SUCCESS) {
    printf("[Stygian AP VK] Failed to create descriptor pool\n");
    return false;
  }

  // Allocate descriptor set
  VkDescriptorSetAllocateInfo alloc_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = ap->descriptor_pool,
      .descriptorSetCount = 1,
      .pSetLayouts = &ap->descriptor_layout,
  };

  if (vkAllocateDescriptorSets(ap->device, &alloc_info, &ap->descriptor_set) !=
      VK_SUCCESS) {
    printf("[Stygian AP VK] Failed to allocate descriptor set\n");
    return false;
  }

  // Update descriptor set: clip + SoA hot/appearance/effects
  VkDescriptorBufferInfo clip_buffer_info = {
      .buffer = ap->clip_ssbo,
      .offset = 0,
      .range = VK_WHOLE_SIZE,
  };
  VkDescriptorBufferInfo soa_hot_info = {
      .buffer = ap->soa_hot_buf,
      .offset = 0,
      .range = VK_WHOLE_SIZE,
  };
  VkDescriptorBufferInfo soa_appearance_info = {
      .buffer = ap->soa_appearance_buf,
      .offset = 0,
      .range = VK_WHOLE_SIZE,
  };
  VkDescriptorBufferInfo soa_effects_info = {
      .buffer = ap->soa_effects_buf,
      .offset = 0,
      .range = VK_WHOLE_SIZE,
  };

  VkWriteDescriptorSet descriptor_writes[4] = {
      {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = ap->descriptor_set,
          .dstBinding = 3,
          .dstArrayElement = 0,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .descriptorCount = 1,
          .pBufferInfo = &clip_buffer_info,
      },
      {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = ap->descriptor_set,
          .dstBinding = 4,
          .dstArrayElement = 0,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .descriptorCount = 1,
          .pBufferInfo = &soa_hot_info,
      },
      {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = ap->descriptor_set,
          .dstBinding = 5,
          .dstArrayElement = 0,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .descriptorCount = 1,
          .pBufferInfo = &soa_appearance_info,
      },
      {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = ap->descriptor_set,
          .dstBinding = 6,
          .dstArrayElement = 0,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .descriptorCount = 1,
          .pBufferInfo = &soa_effects_info,
      },
  };

  vkUpdateDescriptorSets(ap->device, 4, descriptor_writes, 0, NULL);

  printf("[Stygian AP VK] Descriptor sets created (6 bindings, SoA-only)\n");
  return true;
}

static bool load_shaders_and_create_pipeline(StygianAP *ap) {
  // Load SPIR-V shaders
  char vert_path[512], frag_path[512];
  snprintf(vert_path, sizeof(vert_path), "%s/build/stygian.vert.spv",
           ap->shader_dir);
  snprintf(frag_path, sizeof(frag_path), "%s/build/stygian.frag.spv",
           ap->shader_dir);

  ap->vert_module = load_shader_module(ap, vert_path);
  ap->frag_module = load_shader_module(ap, frag_path);

  if (!ap->vert_module || !ap->frag_module) {
    printf("[Stygian AP VK] Failed to load shaders\n");
    return false;
  }

  VkPipelineShaderStageCreateInfo shader_stages[] = {
      {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_VERTEX_BIT,
          .module = ap->vert_module,
          .pName = "main",
      },
      {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
          .module = ap->frag_module,
          .pName = "main",
      },
  };

  // Vertex input: single vec2 attribute
  VkVertexInputBindingDescription binding_desc = {
      .binding = 0,
      .stride = 2 * sizeof(float),
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
  };

  VkVertexInputAttributeDescription attribute_desc = {
      .binding = 0,
      .location = 0,
      .format = VK_FORMAT_R32G32_SFLOAT,
      .offset = 0,
  };

  VkPipelineVertexInputStateCreateInfo vertex_input_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = 1,
      .pVertexBindingDescriptions = &binding_desc,
      .vertexAttributeDescriptionCount = 1,
      .pVertexAttributeDescriptions = &attribute_desc,
  };

  VkPipelineInputAssemblyStateCreateInfo input_assembly = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .primitiveRestartEnable = VK_FALSE,
  };

  VkViewport viewport = {
      .x = 0.0f,
      .y = 0.0f,
      .width = (float)ap->swapchain_extent.width,
      .height = (float)ap->swapchain_extent.height,
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  };

  VkRect2D scissor = {
      .offset = {0, 0},
      .extent = ap->swapchain_extent,
  };

  VkPipelineViewportStateCreateInfo viewport_state = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .pViewports = &viewport,
      .scissorCount = 1,
      .pScissors = &scissor,
  };

  VkDynamicState dynamic_states[] = {
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR,
  };
  VkPipelineDynamicStateCreateInfo dynamic_state = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = 2,
      .pDynamicStates = dynamic_states,
  };

  VkPipelineRasterizationStateCreateInfo rasterizer = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .depthClampEnable = VK_FALSE,
      .rasterizerDiscardEnable = VK_FALSE,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .lineWidth = 1.0f,
      .cullMode = VK_CULL_MODE_NONE,
      .frontFace = VK_FRONT_FACE_CLOCKWISE,
      .depthBiasEnable = VK_FALSE,
  };

  VkPipelineMultisampleStateCreateInfo multisampling = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .sampleShadingEnable = VK_FALSE,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
  };

  VkPipelineColorBlendAttachmentState color_blend_attachment = {
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
      .blendEnable = VK_TRUE,
      .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
      .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      .colorBlendOp = VK_BLEND_OP_ADD,
      .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
      .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
      .alphaBlendOp = VK_BLEND_OP_ADD,
  };

  VkPipelineColorBlendStateCreateInfo color_blending = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .logicOpEnable = VK_FALSE,
      .attachmentCount = 1,
      .pAttachments = &color_blend_attachment,
  };

  // Push constants for screen/atlas + output color transform.
  VkPushConstantRange push_constant = {
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
      .offset = 0,
      .size = sizeof(StygianVKPushConstants),
  };

  VkPipelineLayoutCreateInfo pipeline_layout_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 1,
      .pSetLayouts = &ap->descriptor_layout,
      .pushConstantRangeCount = 1,
      .pPushConstantRanges = &push_constant,
  };

  if (vkCreatePipelineLayout(ap->device, &pipeline_layout_info, NULL,
                             &ap->pipeline_layout) != VK_SUCCESS) {
    printf("[Stygian AP VK] Failed to create pipeline layout\n");
    return false;
  }

  VkGraphicsPipelineCreateInfo pipeline_info = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .stageCount = 2,
      .pStages = shader_stages,
      .pVertexInputState = &vertex_input_info,
      .pInputAssemblyState = &input_assembly,
      .pViewportState = &viewport_state,
      .pRasterizationState = &rasterizer,
      .pMultisampleState = &multisampling,
      .pColorBlendState = &color_blending,
      .pDynamicState = &dynamic_state,
      .layout = ap->pipeline_layout,
      .renderPass = ap->render_pass,
      .subpass = 0,
  };

  if (vkCreateGraphicsPipelines(ap->device, VK_NULL_HANDLE, 1, &pipeline_info,
                                NULL, &ap->graphics_pipeline) != VK_SUCCESS) {
    printf("[Stygian AP VK] Failed to create graphics pipeline\n");
    return false;
  }

  printf("[Stygian AP VK] Shaders loaded and pipeline created\n");
  return true;
}

// ============================================================================
// Lifecycle
// ============================================================================

StygianAP *stygian_ap_create(const StygianAPConfig *config) {
  if (!config || !config->window) {
    printf("[Stygian AP VK] Error: window required\n");
    return NULL;
  }

  StygianAP *ap = (StygianAP *)cfg_alloc(config->allocator, sizeof(StygianAP),
                                         _Alignof(StygianAP));
  if (!ap)
    return NULL;
  memset(ap, 0, sizeof(StygianAP));
  ap->allocator = config->allocator;
  ap->window = config->window;
  ap->max_elements = config->max_elements > 0 ? config->max_elements : 16384;
  ap->atlas_width = 1.0f;
  ap->atlas_height = 1.0f;
  ap->px_range = 4.0f;
  ap->output_color_transform_enabled = false;
  ap->output_src_srgb_transfer = true;
  ap->output_dst_srgb_transfer = true;
  ap->output_src_gamma = 2.4f;
  ap->output_dst_gamma = 2.4f;
  ap->resize_debounce_frames = 2;
  ap->resize_suboptimal_threshold = 3;
  ap->resize_telemetry_enabled = false;
  ap->resize_telemetry_period = 120;
  memset(ap->output_color_matrix, 0, sizeof(ap->output_color_matrix));
  ap->output_color_matrix[0] = 1.0f;
  ap->output_color_matrix[4] = 1.0f;
  ap->output_color_matrix[8] = 1.0f;

  // Copy shader directory
  stygian_cpystr(ap->shader_dir, sizeof(ap->shader_dir),
                 (config->shader_dir && config->shader_dir[0])
                     ? config->shader_dir
                     : "shaders");

  {
    const char *debounce_env = getenv("STYGIAN_VK_RESIZE_DEBOUNCE");
    if (debounce_env && debounce_env[0]) {
      long v = strtol(debounce_env, NULL, 10);
      if (v >= 0 && v <= 30) {
        ap->resize_debounce_frames = (uint32_t)v;
      }
    }
  }
  {
    const char *suboptimal_env = getenv("STYGIAN_VK_SUBOPTIMAL_THRESHOLD");
    if (suboptimal_env && suboptimal_env[0]) {
      long v = strtol(suboptimal_env, NULL, 10);
      if (v >= 1 && v <= 120) {
        ap->resize_suboptimal_threshold = (uint32_t)v;
      }
    }
  }
  {
    const char *telemetry_env = getenv("STYGIAN_VK_RESIZE_TELEMETRY");
    ap->resize_telemetry_enabled = (telemetry_env && telemetry_env[0] &&
                                    strtol(telemetry_env, NULL, 10) != 0);
  }
  {
    const char *period_env = getenv("STYGIAN_VK_RESIZE_TELEMETRY_PERIOD");
    if (period_env && period_env[0]) {
      long v = strtol(period_env, NULL, 10);
      if (v >= 1 && v <= 10000) {
        ap->resize_telemetry_period = (uint32_t)v;
      }
    }
  }

  printf("[Stygian AP VK] Initializing Vulkan backend...\n");

  // Create Vulkan instance
  if (!create_instance(ap)) {
    cfg_free(ap->allocator, ap);
    return NULL;
  }

  // Pick physical device
  if (!pick_physical_device(ap)) {
    vkDestroyInstance(ap->instance, NULL);
    cfg_free(ap->allocator, ap);
    return NULL;
  }

  // Find queue families
  if (!find_queue_families(ap)) {
    vkDestroyInstance(ap->instance, NULL);
    cfg_free(ap->allocator, ap);
    return NULL;
  }

  // Create logical device
  if (!create_logical_device(ap)) {
    vkDestroyInstance(ap->instance, NULL);
    cfg_free(ap->allocator, ap);
    return NULL;
  }

  init_gpu_timer_support(ap);

  // Create surface
  if (!create_surface(ap)) {
    vkDestroyDevice(ap->device, NULL);
    vkDestroyInstance(ap->instance, NULL);
    cfg_free(ap->allocator, ap);
    return NULL;
  }

  // Get window size for swapchain
  int width, height;
  stygian_window_get_size(ap->window, &width, &height);

  // Create swapchain
  if (!create_swapchain(ap, width, height, VK_NULL_HANDLE)) {
    vkDestroySurfaceKHR(ap->instance, ap->surface, NULL);
    vkDestroyDevice(ap->device, NULL);
    vkDestroyInstance(ap->instance, NULL);
    cfg_free(ap->allocator, ap);
    return NULL;
  }

  // Create image views
  if (!create_image_views(ap)) {
    vkDestroySwapchainKHR(ap->device, ap->swapchain, NULL);
    vkDestroySurfaceKHR(ap->instance, ap->surface, NULL);
    vkDestroyDevice(ap->device, NULL);
    vkDestroyInstance(ap->instance, NULL);
    cfg_free(ap->allocator, ap);
    return NULL;
  }

  // Create render pass
  if (!create_render_pass(ap)) {
    for (uint32_t i = 0; i < ap->swapchain_image_count; i++) {
      vkDestroyImageView(ap->device, ap->swapchain_views[i], NULL);
    }
    vkDestroySwapchainKHR(ap->device, ap->swapchain, NULL);
    vkDestroySurfaceKHR(ap->instance, ap->surface, NULL);
    vkDestroyDevice(ap->device, NULL);
    vkDestroyInstance(ap->instance, NULL);
    cfg_free(ap->allocator, ap);
    return NULL;
  }

  // Create framebuffers
  if (!create_framebuffers(ap)) {
    vkDestroyRenderPass(ap->device, ap->render_pass, NULL);
    for (uint32_t i = 0; i < ap->swapchain_image_count; i++) {
      vkDestroyImageView(ap->device, ap->swapchain_views[i], NULL);
    }
    vkDestroySwapchainKHR(ap->device, ap->swapchain, NULL);
    vkDestroySurfaceKHR(ap->instance, ap->surface, NULL);
    vkDestroyDevice(ap->device, NULL);
    vkDestroyInstance(ap->instance, NULL);
    cfg_free(ap->allocator, ap);
    return NULL;
  }

  // Create command pool and buffers
  if (!create_command_pool(ap)) {
    for (uint32_t i = 0; i < ap->swapchain_image_count; i++) {
      vkDestroyFramebuffer(ap->device, ap->framebuffers[i], NULL);
    }
    vkDestroyRenderPass(ap->device, ap->render_pass, NULL);
    for (uint32_t i = 0; i < ap->swapchain_image_count; i++) {
      vkDestroyImageView(ap->device, ap->swapchain_views[i], NULL);
    }
    vkDestroySwapchainKHR(ap->device, ap->swapchain, NULL);
    vkDestroySurfaceKHR(ap->instance, ap->surface, NULL);
    vkDestroyDevice(ap->device, NULL);
    vkDestroyInstance(ap->instance, NULL);
    cfg_free(ap->allocator, ap);
    return NULL;
  }

  // Create synchronization objects
  if (!create_sync_objects(ap)) {
    vkDestroyCommandPool(ap->device, ap->command_pool, NULL);
    for (uint32_t i = 0; i < ap->swapchain_image_count; i++) {
      vkDestroyFramebuffer(ap->device, ap->framebuffers[i], NULL);
    }
    vkDestroyRenderPass(ap->device, ap->render_pass, NULL);
    for (uint32_t i = 0; i < ap->swapchain_image_count; i++) {
      vkDestroyImageView(ap->device, ap->swapchain_views[i], NULL);
    }
    vkDestroySwapchainKHR(ap->device, ap->swapchain, NULL);
    vkDestroySurfaceKHR(ap->instance, ap->surface, NULL);
    vkDestroyDevice(ap->device, NULL);
    vkDestroyInstance(ap->instance, NULL);
    cfg_free(ap->allocator, ap);
    return NULL;
  }

  // Create buffers
  if (!create_buffers(ap)) {
    for (int i = 0; i < STYGIAN_VK_FRAMES_IN_FLIGHT; i++) {
      vkDestroySemaphore(ap->device, ap->render_finished[i], NULL);
      vkDestroySemaphore(ap->device, ap->image_available[i], NULL);
      vkDestroyFence(ap->device, ap->in_flight[i], NULL);
    }
    vkDestroyCommandPool(ap->device, ap->command_pool, NULL);
    for (uint32_t i = 0; i < ap->swapchain_image_count; i++) {
      vkDestroyFramebuffer(ap->device, ap->framebuffers[i], NULL);
    }
    vkDestroyRenderPass(ap->device, ap->render_pass, NULL);
    for (uint32_t i = 0; i < ap->swapchain_image_count; i++) {
      vkDestroyImageView(ap->device, ap->swapchain_views[i], NULL);
    }
    vkDestroySwapchainKHR(ap->device, ap->swapchain, NULL);
    vkDestroySurfaceKHR(ap->instance, ap->surface, NULL);
    vkDestroyDevice(ap->device, NULL);
    vkDestroyInstance(ap->instance, NULL);
    cfg_free(ap->allocator, ap);
    return NULL;
  }

  // Create descriptor sets
  if (!create_descriptor_sets(ap)) {
    vkDestroyBuffer(ap->device, ap->vertex_buffer, NULL);
    vkFreeMemory(ap->device, ap->vertex_memory, NULL);
    if (ap->clip_ssbo)
      vkDestroyBuffer(ap->device, ap->clip_ssbo, NULL);
    if (ap->clip_ssbo_memory)
      vkFreeMemory(ap->device, ap->clip_ssbo_memory, NULL);
    if (ap->soa_hot_buf)
      vkDestroyBuffer(ap->device, ap->soa_hot_buf, NULL);
    if (ap->soa_hot_mem)
      vkFreeMemory(ap->device, ap->soa_hot_mem, NULL);
    if (ap->soa_appearance_buf)
      vkDestroyBuffer(ap->device, ap->soa_appearance_buf, NULL);
    if (ap->soa_appearance_mem)
      vkFreeMemory(ap->device, ap->soa_appearance_mem, NULL);
    if (ap->soa_effects_buf)
      vkDestroyBuffer(ap->device, ap->soa_effects_buf, NULL);
    if (ap->soa_effects_mem)
      vkFreeMemory(ap->device, ap->soa_effects_mem, NULL);
    // ... (cleanup sync, command pool, framebuffers, etc.)
    cfg_free(ap->allocator, ap);
    return NULL;
  }

  // Load shaders and create pipeline
  if (!load_shaders_and_create_pipeline(ap)) {
    vkDestroyDescriptorPool(ap->device, ap->descriptor_pool, NULL);
    vkDestroyDescriptorSetLayout(ap->device, ap->descriptor_layout, NULL);
    vkDestroyBuffer(ap->device, ap->vertex_buffer, NULL);
    vkFreeMemory(ap->device, ap->vertex_memory, NULL);
    // ... (cleanup sync, command pool, framebuffers, etc.)
    cfg_free(ap->allocator, ap);
    return NULL;
  }

  // Allocate GPU-side version tracking for SoA chunk upload
  // Default chunk_size 256 → chunk_count = ceil(max_elements / 256)
  {
    uint32_t cs = 256u;
    uint32_t cc = (ap->max_elements + cs - 1u) / cs;
    ap->soa_chunk_count = cc;
    ap->gpu_hot_versions =
        (uint32_t *)ap_alloc(ap, cc * sizeof(uint32_t), _Alignof(uint32_t));
    ap->gpu_appearance_versions =
        (uint32_t *)ap_alloc(ap, cc * sizeof(uint32_t), _Alignof(uint32_t));
    ap->gpu_effects_versions =
        (uint32_t *)ap_alloc(ap, cc * sizeof(uint32_t), _Alignof(uint32_t));
    if (ap->gpu_hot_versions)
      memset(ap->gpu_hot_versions, 0, cc * sizeof(uint32_t));
    if (ap->gpu_appearance_versions)
      memset(ap->gpu_appearance_versions, 0, cc * sizeof(uint32_t));
    if (ap->gpu_effects_versions)
      memset(ap->gpu_effects_versions, 0, cc * sizeof(uint32_t));
  }

  printf("[Stygian AP VK] Vulkan backend initialized successfully\n");
  ap->initialized = true;
  return ap;
}

void stygian_ap_destroy(StygianAP *ap) {
  if (!ap)
    return;

  // Wait for device to finish all operations
  if (ap->device) {
    vkDeviceWaitIdle(ap->device);
  }

  // Destroy sync objects
  for (int i = 0; i < STYGIAN_VK_FRAMES_IN_FLIGHT; i++) {
    if (ap->render_finished[i])
      vkDestroySemaphore(ap->device, ap->render_finished[i], NULL);
    if (ap->image_available[i])
      vkDestroySemaphore(ap->device, ap->image_available[i], NULL);
    if (ap->in_flight[i])
      vkDestroyFence(ap->device, ap->in_flight[i], NULL);
  }

  // Destroy command pool (frees command buffers automatically)
  if (ap->command_pool)
    vkDestroyCommandPool(ap->device, ap->command_pool, NULL);
  if (ap->gpu_timer_query_pool)
    vkDestroyQueryPool(ap->device, ap->gpu_timer_query_pool, NULL);

  if (ap->graphics_pipeline)
    vkDestroyPipeline(ap->device, ap->graphics_pipeline, NULL);
  if (ap->pipeline_layout)
    vkDestroyPipelineLayout(ap->device, ap->pipeline_layout, NULL);
  if (ap->vert_module)
    vkDestroyShaderModule(ap->device, ap->vert_module, NULL);
  if (ap->frag_module)
    vkDestroyShaderModule(ap->device, ap->frag_module, NULL);
  if (ap->descriptor_pool)
    vkDestroyDescriptorPool(ap->device, ap->descriptor_pool, NULL);
  if (ap->descriptor_layout)
    vkDestroyDescriptorSetLayout(ap->device, ap->descriptor_layout, NULL);
  if (ap->vertex_buffer)
    vkDestroyBuffer(ap->device, ap->vertex_buffer, NULL);
  if (ap->vertex_memory)
    vkFreeMemory(ap->device, ap->vertex_memory, NULL);
  if (ap->clip_ssbo)
    vkDestroyBuffer(ap->device, ap->clip_ssbo, NULL);
  if (ap->clip_ssbo_memory)
    vkFreeMemory(ap->device, ap->clip_ssbo_memory, NULL);
  // SoA buffer cleanup
  if (ap->soa_hot_buf)
    vkDestroyBuffer(ap->device, ap->soa_hot_buf, NULL);
  if (ap->soa_hot_mem)
    vkFreeMemory(ap->device, ap->soa_hot_mem, NULL);
  if (ap->soa_appearance_buf)
    vkDestroyBuffer(ap->device, ap->soa_appearance_buf, NULL);
  if (ap->soa_appearance_mem)
    vkFreeMemory(ap->device, ap->soa_appearance_mem, NULL);
  if (ap->soa_effects_buf)
    vkDestroyBuffer(ap->device, ap->soa_effects_buf, NULL);
  if (ap->soa_effects_mem)
    vkFreeMemory(ap->device, ap->soa_effects_mem, NULL);
  if (ap->font_sampler)
    vkDestroySampler(ap->device, ap->font_sampler, NULL);
  if (ap->font_view)
    vkDestroyImageView(ap->device, ap->font_view, NULL);
  if (ap->font_image)
    vkDestroyImage(ap->device, ap->font_image, NULL);
  if (ap->font_memory)
    vkFreeMemory(ap->device, ap->font_memory, NULL);

  // Destroy framebuffers
  for (uint32_t i = 0; i < ap->swapchain_image_count; i++) {
    if (ap->framebuffers[i])
      vkDestroyFramebuffer(ap->device, ap->framebuffers[i], NULL);
  }

  // Destroy render pass
  if (ap->render_pass)
    vkDestroyRenderPass(ap->device, ap->render_pass, NULL);

  // Destroy image views
  for (uint32_t i = 0; i < ap->swapchain_image_count; i++) {
    if (ap->swapchain_views[i])
      vkDestroyImageView(ap->device, ap->swapchain_views[i], NULL);
  }

  // Destroy swapchain
  if (ap->swapchain)
    vkDestroySwapchainKHR(ap->device, ap->swapchain, NULL);

  // Destroy surface
  if (ap->surface)
    vkDestroySurfaceKHR(ap->instance, ap->surface, NULL);

  // Destroy device
  if (ap->device)
    vkDestroyDevice(ap->device, NULL);

  // Destroy instance
  if (ap->instance)
    vkDestroyInstance(ap->instance, NULL);

  // Free AP struct via its own allocator
  StygianAllocator *allocator = ap->allocator;
  cfg_free(allocator, ap);
}

StygianAPAdapterClass stygian_ap_get_adapter_class(const StygianAP *ap) {
  if (!ap)
    return STYGIAN_AP_ADAPTER_UNKNOWN;
  return ap->adapter_class;
}

uint32_t stygian_ap_get_last_upload_bytes(const StygianAP *ap) {
  if (!ap)
    return 0u;
  return ap->last_upload_bytes;
}

uint32_t stygian_ap_get_last_upload_ranges(const StygianAP *ap) {
  if (!ap)
    return 0u;
  return ap->last_upload_ranges;
}

float stygian_ap_get_last_gpu_ms(const StygianAP *ap) {
  if (!ap)
    return 0.0f;
  return ap->last_gpu_ms;
}

void stygian_ap_gpu_timer_begin(StygianAP *ap) { (void)ap; }

void stygian_ap_gpu_timer_end(StygianAP *ap) { (void)ap; }

void stygian_ap_begin_frame(StygianAP *ap, int width, int height) {
  if (!ap)
    return;

  ap->frame_active = false;

  if (ap->window) {
    int fb_w = 0, fb_h = 0;
    stygian_window_get_framebuffer_size(ap->window, &fb_w, &fb_h);
    if (fb_w > 0 && fb_h > 0) {
      width = fb_w;
      height = fb_h;
    }
  }

  if (width <= 0 || height <= 0) {
    return; // Minimized window
  }

  if (ap->swapchain_needs_recreate) {
    if (!recreate_main_swapchain(ap, width, height)) {
      return;
    }
    ap->swapchain_needs_recreate = false;
  }

  // Coalesce resize churn: only recreate after size is stable for N frames.
  if (!ap->swapchain_needs_recreate &&
      ((uint32_t)width != ap->swapchain_extent.width ||
       (uint32_t)height != ap->swapchain_extent.height)) {
    if (ap->resize_pending_w != width || ap->resize_pending_h != height) {
      ap->resize_pending_w = width;
      ap->resize_pending_h = height;
      ap->resize_stable_count = 0;
      return;
    }

    ap->resize_stable_count++;
    if (ap->resize_stable_count < ap->resize_debounce_frames) {
      return;
    }

    ap->swapchain_needs_recreate = true;
    if (!recreate_main_swapchain(ap, width, height)) {
      return;
    }
    ap->swapchain_needs_recreate = false;
    ap->resize_stable_count = 0;
  } else {
    ap->resize_pending_w = width;
    ap->resize_pending_h = height;
    ap->resize_stable_count = 0;
  }

  // Wait for previous frame to finish
  vkWaitForFences(ap->device, 1, &ap->in_flight[ap->current_frame], VK_TRUE,
                  UINT64_MAX);
  update_gpu_timer_sample_for_frame(ap, ap->current_frame);

  // Acquire next swapchain image
  double acquire_t0 = stygian_vk_now_ms();
  VkResult result =
      vkAcquireNextImageKHR(ap->device, ap->swapchain, UINT64_MAX,
                            ap->image_available[ap->current_frame],
                            VK_NULL_HANDLE, &ap->current_image);
  if (ap->resize_telemetry_enabled) {
    ap->resize_telemetry_acquire_ms += (stygian_vk_now_ms() - acquire_t0);
  }

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    ap->swapchain_needs_recreate = true;
    return;
  }

  if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    printf("[Stygian AP VK] Failed to acquire swapchain image: %d\n", result);
    return;
  }
  if (result == VK_SUBOPTIMAL_KHR) {
    ap->resize_suboptimal_count++;
    if (ap->resize_suboptimal_count >= ap->resize_suboptimal_threshold) {
      ap->swapchain_needs_recreate = true;
    }
  } else {
    ap->resize_suboptimal_count = 0;
  }

  // If this image is still in use by a previous frame, wait only for that one.
  if (ap->current_image < STYGIAN_VK_MAX_SWAPCHAIN_IMAGES &&
      ap->image_in_flight[ap->current_image] != VK_NULL_HANDLE) {
    vkWaitForFences(ap->device, 1, &ap->image_in_flight[ap->current_image],
                    VK_TRUE, UINT64_MAX);
  }
  if (ap->current_image < STYGIAN_VK_MAX_SWAPCHAIN_IMAGES) {
    ap->image_in_flight[ap->current_image] = ap->in_flight[ap->current_frame];
  }

  // Reset fence for this frame
  vkResetFences(ap->device, 1, &ap->in_flight[ap->current_frame]);

  // Reset and begin command buffer
  VkCommandBuffer cmd = ap->command_buffers[ap->current_frame];
  vkResetCommandBuffer(cmd, 0);

  VkCommandBufferBeginInfo begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };
  vkBeginCommandBuffer(cmd, &begin_info);

  if (ap->gpu_timer_supported && ap->gpu_timer_query_pool) {
    uint32_t query_base = ap->current_frame * 2u;
    vkCmdResetQueryPool(cmd, ap->gpu_timer_query_pool, query_base, 2u);
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        ap->gpu_timer_query_pool, query_base);
  }

  // Begin render pass
  VkClearValue clear_color = {{{0.1f, 0.1f, 0.1f, 1.0f}}};
  VkRenderPassBeginInfo render_pass_info = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass = ap->render_pass,
      .framebuffer = ap->framebuffers[ap->current_image],
      .renderArea = {.offset = {0, 0}, .extent = ap->swapchain_extent},
      .clearValueCount = 1,
      .pClearValues = &clear_color,
  };

  vkCmdBeginRenderPass(cmd, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
  ap->frame_active = true;
}

void stygian_ap_submit(StygianAP *ap, const StygianSoAHot *soa_hot,
                       uint32_t count) {
  // Vulkan path uses explicit descriptor slots; texture IDs are already compact.
  (void)soa_hot;
  if (!ap || count == 0)
    return;
  if (count > ap->max_elements)
    count = ap->max_elements;
  ap->element_count = count;
}

void stygian_ap_submit_soa(StygianAP *ap, const StygianSoAHot *hot,
                           const StygianSoAAppearance *appearance,
                           const StygianSoAEffects *effects,
                           uint32_t element_count,
                           const StygianBufferChunk *chunks,
                           uint32_t chunk_count, uint32_t chunk_size) {
  if (!ap || !hot || !appearance || !effects || !chunks || element_count == 0)
    return;

  // Clamp to AP mirror metadata; commit side owns authoritative chunk count.
  if (chunk_count > ap->soa_chunk_count) {
    chunk_count = ap->soa_chunk_count;
  }

  ap->last_upload_bytes = 0u;
  ap->last_upload_ranges = 0u;

  // Map once per frame and copy only dirty ranges to keep upload cost bounded.
  void *hot_mapped = NULL;
  void *app_mapped = NULL;
  void *eff_mapped = NULL;

  vkMapMemory(ap->device, ap->soa_hot_mem, 0, VK_WHOLE_SIZE, 0, &hot_mapped);
  vkMapMemory(ap->device, ap->soa_appearance_mem, 0, VK_WHOLE_SIZE, 0,
              &app_mapped);
  vkMapMemory(ap->device, ap->soa_effects_mem, 0, VK_WHOLE_SIZE, 0,
              &eff_mapped);

  for (uint32_t ci = 0; ci < chunk_count; ci++) {
    const StygianBufferChunk *c = &chunks[ci];
    uint32_t base = ci * chunk_size;

    // --- Hot buffer ---
    if (ap->gpu_hot_versions && c->hot_version != ap->gpu_hot_versions[ci]) {
      uint32_t dmin = c->hot_dirty_min;
      uint32_t dmax = c->hot_dirty_max;
      if (dmin <= dmax) {
        uint32_t abs_min = base + dmin;
        uint32_t abs_max = base + dmax;
        if (abs_max >= element_count)
          abs_max = element_count - 1;
        if (abs_min < element_count && hot_mapped) {
          uint32_t range_count = abs_max - abs_min + 1;
          size_t offset = (size_t)abs_min * sizeof(StygianSoAHot);
          size_t bytes = (size_t)range_count * sizeof(StygianSoAHot);
          memcpy((char *)hot_mapped + offset, &hot[abs_min], bytes);
          ap->last_upload_bytes += (uint32_t)bytes;
          ap->last_upload_ranges++;
        }
      }
      ap->gpu_hot_versions[ci] = c->hot_version;
    }

    // --- Appearance buffer ---
    if (ap->gpu_appearance_versions &&
        c->appearance_version != ap->gpu_appearance_versions[ci]) {
      uint32_t dmin = c->appearance_dirty_min;
      uint32_t dmax = c->appearance_dirty_max;
      if (dmin <= dmax) {
        uint32_t abs_min = base + dmin;
        uint32_t abs_max = base + dmax;
        if (abs_max >= element_count)
          abs_max = element_count - 1;
        if (abs_min < element_count && app_mapped) {
          uint32_t range_count = abs_max - abs_min + 1;
          size_t offset = (size_t)abs_min * sizeof(StygianSoAAppearance);
          size_t bytes = (size_t)range_count * sizeof(StygianSoAAppearance);
          memcpy((char *)app_mapped + offset, &appearance[abs_min], bytes);
          ap->last_upload_bytes += (uint32_t)bytes;
          ap->last_upload_ranges++;
        }
      }
      ap->gpu_appearance_versions[ci] = c->appearance_version;
    }

    // --- Effects buffer ---
    if (ap->gpu_effects_versions &&
        c->effects_version != ap->gpu_effects_versions[ci]) {
      uint32_t dmin = c->effects_dirty_min;
      uint32_t dmax = c->effects_dirty_max;
      if (dmin <= dmax) {
        uint32_t abs_min = base + dmin;
        uint32_t abs_max = base + dmax;
        if (abs_max >= element_count)
          abs_max = element_count - 1;
        if (abs_min < element_count && eff_mapped) {
          uint32_t range_count = abs_max - abs_min + 1;
          size_t offset = (size_t)abs_min * sizeof(StygianSoAEffects);
          size_t bytes = (size_t)range_count * sizeof(StygianSoAEffects);
          memcpy((char *)eff_mapped + offset, &effects[abs_min], bytes);
          ap->last_upload_bytes += (uint32_t)bytes;
          ap->last_upload_ranges++;
        }
      }
      ap->gpu_effects_versions[ci] = c->effects_version;
    }
  }

  // Unmap all three buffers
  if (hot_mapped)
    vkUnmapMemory(ap->device, ap->soa_hot_mem);
  if (app_mapped)
    vkUnmapMemory(ap->device, ap->soa_appearance_mem);
  if (eff_mapped)
    vkUnmapMemory(ap->device, ap->soa_effects_mem);
}

void stygian_ap_draw(StygianAP *ap) {
  if (!ap || !ap->frame_active || ap->element_count == 0)
    return;
  stygian_ap_draw_range(ap, 0u, ap->element_count);
}

void stygian_ap_draw_range(StygianAP *ap, uint32_t first_instance,
                           uint32_t instance_count) {
  if (!ap || !ap->frame_active || instance_count == 0)
    return;

  VkCommandBuffer cmd = ap->command_buffers[ap->current_frame];

  // Bind pipeline
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    ap->graphics_pipeline);

  // Bind descriptor set (SSBO)
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          ap->pipeline_layout, 0, 1, &ap->descriptor_set, 0,
                          NULL);

  // Push constants (shared vertex/fragment constants)
  {
    StygianVKPushConstants pc;
    fill_push_constants(ap, (float)ap->swapchain_extent.width,
                        (float)ap->swapchain_extent.height, &pc);
    vkCmdPushConstants(cmd, ap->pipeline_layout,
                       VK_SHADER_STAGE_VERTEX_BIT |
                           VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(pc), &pc);
  }

  // Bind vertex buffer
  VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers(cmd, 0, 1, &ap->vertex_buffer, &offset);

  // Dynamic viewport/scissor to match current swapchain extent after resize.
  VkViewport viewport = {
      .x = 0.0f,
      .y = 0.0f,
      .width = (float)ap->swapchain_extent.width,
      .height = (float)ap->swapchain_extent.height,
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  };
  VkRect2D scissor = {
      .offset = {0, 0},
      .extent = ap->swapchain_extent,
  };
  vkCmdSetViewport(cmd, 0, 1, &viewport);
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  // Draw instanced (6 vertices per quad, count instances)
  vkCmdDraw(cmd, 6, instance_count, 0, first_instance);
}

void stygian_ap_end_frame(StygianAP *ap) {
  if (!ap || !ap->frame_active)
    return;

  // End render pass
  VkCommandBuffer cmd = ap->command_buffers[ap->current_frame];
  vkCmdEndRenderPass(cmd);

  if (ap->gpu_timer_supported && ap->gpu_timer_query_pool) {
    uint32_t query_base = ap->current_frame * 2u;
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                        ap->gpu_timer_query_pool, query_base + 1u);
  }

  // End command buffer
  VkResult result = vkEndCommandBuffer(cmd);
  if (result != VK_SUCCESS) {
    printf("[Stygian AP VK] Failed to end command buffer: %d\n", result);
    return;
  }

  // Submit command buffer
  VkSemaphore wait_semaphores[] = {ap->image_available[ap->current_frame]};
  VkPipelineStageFlags wait_stages[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  VkSemaphore signal_semaphores[] = {ap->render_finished[ap->current_frame]};

  VkSubmitInfo submit_info = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = wait_semaphores,
      .pWaitDstStageMask = wait_stages,
      .commandBufferCount = 1,
      .pCommandBuffers = &cmd,
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = signal_semaphores,
  };

  double submit_t0 = stygian_vk_now_ms();
  result = vkQueueSubmit(ap->graphics_queue, 1, &submit_info,
                         ap->in_flight[ap->current_frame]);
  if (ap->resize_telemetry_enabled) {
    ap->resize_telemetry_submit_ms += (stygian_vk_now_ms() - submit_t0);
  }
  if (result != VK_SUCCESS) {
    printf("[Stygian AP VK] Failed to submit command buffer: %d\n", result);
  }
}

void stygian_ap_swap(StygianAP *ap) {
  if (!ap || !ap->frame_active)
    return;

  // Present
  VkSemaphore signal_semaphores[] = {ap->render_finished[ap->current_frame]};
  VkSwapchainKHR swapchains[] = {ap->swapchain};

  VkPresentInfoKHR present_info = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = signal_semaphores,
      .swapchainCount = 1,
      .pSwapchains = swapchains,
      .pImageIndices = &ap->current_image,
  };

  double present_t0 = stygian_vk_now_ms();
  VkResult result = vkQueuePresentKHR(ap->graphics_queue, &present_info);
  if (ap->resize_telemetry_enabled) {
    ap->resize_telemetry_present_ms += (stygian_vk_now_ms() - present_t0);
  }
  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    ap->swapchain_needs_recreate = true;
  } else if (result == VK_SUBOPTIMAL_KHR) {
    ap->resize_suboptimal_count++;
    if (ap->resize_suboptimal_count >= ap->resize_suboptimal_threshold) {
      ap->swapchain_needs_recreate = true;
    }
  } else if (result != VK_SUCCESS) {
    printf("[Stygian AP VK] Failed to present: %d\n", result);
  } else {
    ap->resize_suboptimal_count = 0;
  }

  if (ap->resize_telemetry_enabled) {
    ap->resize_telemetry_frames++;
    if (ap->resize_telemetry_frames >= ap->resize_telemetry_period) {
      double frames = (double)ap->resize_telemetry_frames;
      printf("[Stygian AP VK] resize telemetry: acquire=%.3fms submit=%.3fms "
             "present=%.3fms recreate_total=%.3fms recreates=%u/%u frames\n",
             ap->resize_telemetry_acquire_ms / frames,
             ap->resize_telemetry_submit_ms / frames,
             ap->resize_telemetry_present_ms / frames,
             ap->resize_telemetry_recreate_ms,
             ap->resize_telemetry_recreate_count, ap->resize_telemetry_frames);
      ap->resize_telemetry_frames = 0;
      ap->resize_telemetry_recreate_count = 0;
      ap->resize_telemetry_acquire_ms = 0.0;
      ap->resize_telemetry_submit_ms = 0.0;
      ap->resize_telemetry_present_ms = 0.0;
      ap->resize_telemetry_recreate_ms = 0.0;
    }
  }

  // Advance to next frame
  ap->current_frame = (ap->current_frame + 1) % STYGIAN_VK_FRAMES_IN_FLIGHT;
  ap->frame_active = false;
}

StygianAPTexture stygian_ap_texture_create(StygianAP *ap, int w, int h,
                                           const void *rgba) {
  if (!ap || !rgba)
    return 0;

  // Create image
  VkImageCreateInfo image_info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = VK_FORMAT_R8G8B8A8_UNORM,
      .extent = {.width = w, .height = h, .depth = 1},
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };

  VkImage image;
  if (vkCreateImage(ap->device, &image_info, NULL, &image) != VK_SUCCESS) {
    printf("[Stygian AP VK] Failed to create texture image\n");
    return 0;
  }

  // Allocate memory
  VkMemoryRequirements mem_reqs;
  vkGetImageMemoryRequirements(ap->device, image, &mem_reqs);

  VkMemoryAllocateInfo alloc_info = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize = mem_reqs.size,
      .memoryTypeIndex =
          find_memory_type(ap->physical_device, mem_reqs.memoryTypeBits,
                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
  };

  VkDeviceMemory memory;
  if (vkAllocateMemory(ap->device, &alloc_info, NULL, &memory) != VK_SUCCESS) {
    vkDestroyImage(ap->device, image, NULL);
    printf("[Stygian AP VK] Failed to allocate texture memory\n");
    return 0;
  }

  vkBindImageMemory(ap->device, image, memory, 0);

  // Create staging buffer
  VkDeviceSize image_size = w * h * 4;
  VkBuffer staging_buffer;
  VkDeviceMemory staging_memory;

  VkBufferCreateInfo buffer_info = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = image_size,
      .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
  };

  vkCreateBuffer(ap->device, &buffer_info, NULL, &staging_buffer);

  VkMemoryRequirements buf_mem_reqs;
  vkGetBufferMemoryRequirements(ap->device, staging_buffer, &buf_mem_reqs);

  VkMemoryAllocateInfo buf_alloc = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize = buf_mem_reqs.size,
      .memoryTypeIndex =
          find_memory_type(ap->physical_device, buf_mem_reqs.memoryTypeBits,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
  };

  vkAllocateMemory(ap->device, &buf_alloc, NULL, &staging_memory);
  vkBindBufferMemory(ap->device, staging_buffer, staging_memory, 0);

  // Upload to staging buffer
  void *data;
  vkMapMemory(ap->device, staging_memory, 0, image_size, 0, &data);
  memcpy(data, rgba, image_size);
  vkUnmapMemory(ap->device, staging_memory);

  // Create one-time command buffer for copy
  VkCommandBufferAllocateInfo cmd_alloc = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = ap->command_pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
  };

  VkCommandBuffer cmd;
  vkAllocateCommandBuffers(ap->device, &cmd_alloc, &cmd);

  VkCommandBufferBeginInfo begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };

  vkBeginCommandBuffer(cmd, &begin_info);

  // Transition image to TRANSFER_DST_OPTIMAL
  VkImageMemoryBarrier barrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image,
      .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
      .srcAccessMask = 0,
      .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
  };

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1,
                       &barrier);

  // Copy buffer to image
  VkBufferImageCopy region = {
      .bufferOffset = 0,
      .bufferRowLength = 0,
      .bufferImageHeight = 0,
      .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
      .imageOffset = {0, 0, 0},
      .imageExtent = {w, h, 1},
  };

  vkCmdCopyBufferToImage(cmd, staging_buffer, image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

  // Transition to SHADER_READ_ONLY_OPTIMAL
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0,
                       NULL, 1, &barrier);

  vkEndCommandBuffer(cmd);

  // Submit and wait
  VkSubmitInfo submit_info = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &cmd,
  };

  vkQueueSubmit(ap->graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
  vkQueueWaitIdle(ap->graphics_queue);

  vkFreeCommandBuffers(ap->device, ap->command_pool, 1, &cmd);
  vkDestroyBuffer(ap->device, staging_buffer, NULL);
  vkFreeMemory(ap->device, staging_memory, NULL);

  // Create image view
  VkImageViewCreateInfo view_info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = VK_FORMAT_R8G8B8A8_UNORM,
      .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
  };

  VkImageView view;
  if (vkCreateImageView(ap->device, &view_info, NULL, &view) != VK_SUCCESS) {
    vkDestroyImage(ap->device, image, NULL);
    vkFreeMemory(ap->device, memory, NULL);
    printf("[Stygian AP VK] Failed to create image view\n");
    return 0;
  }

  // Create sampler
  VkSamplerCreateInfo sampler_info = {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .magFilter = VK_FILTER_LINEAR,
      .minFilter = VK_FILTER_LINEAR,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .anisotropyEnable = VK_FALSE,
      .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
      .unnormalizedCoordinates = VK_FALSE,
      .compareEnable = VK_FALSE,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
  };

  VkSampler sampler;
  if (vkCreateSampler(ap->device, &sampler_info, NULL, &sampler) !=
      VK_SUCCESS) {
    vkDestroyImageView(ap->device, view, NULL);
    vkDestroyImage(ap->device, image, NULL);
    vkFreeMemory(ap->device, memory, NULL);
    printf("[Stygian AP VK] Failed to create sampler\n");
    return 0;
  }

  // Store font texture
  ap->font_image = image;
  ap->font_memory = memory;
  ap->font_view = view;
  ap->font_sampler = sampler;

  // Update descriptor set with font texture (binding 1)
  VkDescriptorImageInfo desc_image_info = {
      .sampler = sampler,
      .imageView = view,
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };

  VkWriteDescriptorSet descriptor_write = {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = ap->descriptor_set,
      .dstBinding = 1,
      .dstArrayElement = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = 1,
      .pImageInfo = &desc_image_info,
  };

  vkUpdateDescriptorSets(ap->device, 1, &descriptor_write, 0, NULL);
  update_image_sampler_array(ap);

  printf("[Stygian AP VK] Texture created: %dx%d\n", w, h);
  return (StygianAPTexture)1;
}

bool stygian_ap_texture_update(StygianAP *ap, StygianAPTexture tex, int x,
                               int y, int w, int h, const void *rgba) {
  (void)ap;
  (void)tex;
  (void)x;
  (void)y;
  (void)w;
  (void)h;
  (void)rgba;
  return false;
}

void stygian_ap_texture_destroy(StygianAP *ap, StygianAPTexture tex) {
  if (!ap || !tex)
    return;

  // TODO: Destroy VkImage and free memory
}

void stygian_ap_texture_bind(StygianAP *ap, StygianAPTexture tex,
                             uint32_t slot) {
  if (!ap)
    return;
  (void)tex;
  (void)slot;

  // TODO: Update descriptor set with texture
}

void stygian_ap_set_font_texture(StygianAP *ap, StygianAPTexture tex,
                                 int atlas_w, int atlas_h, float px_range) {
  if (!ap || !tex)
    return;
  ap->atlas_width = atlas_w > 0 ? (float)atlas_w : 1.0f;
  ap->atlas_height = atlas_h > 0 ? (float)atlas_h : 1.0f;
  ap->px_range = px_range > 0.0f ? px_range : 4.0f;

  // Update descriptor set with font texture (binding 1)
  VkDescriptorImageInfo image_info = {
      .sampler = ap->font_sampler,
      .imageView = ap->font_view,
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };

  VkWriteDescriptorSet descriptor_write = {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = ap->descriptor_set,
      .dstBinding = 1,
      .dstArrayElement = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = 1,
      .pImageInfo = &image_info,
  };

  vkUpdateDescriptorSets(ap->device, 1, &descriptor_write, 0, NULL);
  update_image_sampler_array(ap);
  printf("[Stygian AP VK] Font texture bound: %dx%d, px_range=%.1f\n", atlas_w,
         atlas_h, px_range);
}

void stygian_ap_set_output_color_transform(
    StygianAP *ap, bool enabled, const float *rgb3x3, bool src_srgb_transfer,
    float src_gamma, bool dst_srgb_transfer, float dst_gamma) {
  static const float identity[9] = {
      1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
  };
  if (!ap)
    return;
  ap->output_color_transform_enabled = enabled;
  memcpy(ap->output_color_matrix, rgb3x3 ? rgb3x3 : identity,
         sizeof(ap->output_color_matrix));
  ap->output_src_srgb_transfer = src_srgb_transfer;
  ap->output_dst_srgb_transfer = dst_srgb_transfer;
  ap->output_src_gamma = src_gamma > 0.0f ? src_gamma : 2.2f;
  ap->output_dst_gamma = dst_gamma > 0.0f ? dst_gamma : 2.2f;
}

void stygian_ap_set_clips(StygianAP *ap, const float *clips, uint32_t count) {
  if (!ap || ap->clip_ssbo_memory == VK_NULL_HANDLE)
    return;
  if (!clips || count == 0)
    return;
  if (count > STYGIAN_MAX_CLIPS)
    count = STYGIAN_MAX_CLIPS;

  void *data = NULL;
  if (vkMapMemory(ap->device, ap->clip_ssbo_memory, 0, VK_WHOLE_SIZE, 0,
                  &data) != VK_SUCCESS) {
    return;
  }
  memcpy(data, clips, count * sizeof(float) * 4);
  vkUnmapMemory(ap->device, ap->clip_ssbo_memory);
}

bool stygian_ap_reload_shaders(StygianAP *ap) {
  if (!ap)
    return false;

  printf("[Stygian AP VK] Shader reload not yet implemented\n");
  return false;
}

bool stygian_ap_shaders_need_reload(StygianAP *ap) {
  return false; // TODO: Implement file watching
}

// ============================================================================
// Multi-Surface API
// ============================================================================

static bool create_surface_swapchain(StygianAPSurface *surf, int width,
                                     int height) {
  StygianAP *ap = surf->ap;

  if (!surf->surface) {
    if (!stygian_window_vk_create_surface(surf->window, ap->instance,
                                          (void **)&surf->surface)) {
      printf("[Stygian AP VK] Failed to create surface for window\n");
      return false;
    }
  }

  // Verify surface support
  VkBool32 supported = VK_FALSE;
  vkGetPhysicalDeviceSurfaceSupportKHR(ap->physical_device, ap->graphics_family,
                                       surf->surface, &supported);
  if (!supported) {
    printf("[Stygian AP VK] Surface not supported by queue family\n");
    return false;
  }

  // Query surface capabilities
  VkSurfaceCapabilitiesKHR capabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ap->physical_device, surf->surface,
                                            &capabilities);

  // Query formats
  uint32_t format_count;
  vkGetPhysicalDeviceSurfaceFormatsKHR(ap->physical_device, surf->surface,
                                       &format_count, NULL);
  VkSurfaceFormatKHR formats[16];
  if (format_count > 16)
    format_count = 16;
  vkGetPhysicalDeviceSurfaceFormatsKHR(ap->physical_device, surf->surface,
                                       &format_count, formats);

  VkSurfaceFormatKHR surface_format = formats[0];
  for (uint32_t i = 0; i < format_count; i++) {
    if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM) {
      surface_format = formats[i];
      break;
    }
  }

  // Determine extent
  VkExtent2D extent;
  if (capabilities.currentExtent.width != UINT32_MAX) {
    extent = capabilities.currentExtent;
  } else {
    extent.width = width;
    extent.height = height;
  }

  // Create swapchain
  uint32_t image_count = 2; // Double buffering for secondary windows
  if (image_count < capabilities.minImageCount)
    image_count = capabilities.minImageCount;
  if (capabilities.maxImageCount > 0 &&
      image_count > capabilities.maxImageCount)
    image_count = capabilities.maxImageCount;

  VkSwapchainCreateInfoKHR swapchain_info = {
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .surface = surf->surface,
      .minImageCount = image_count,
      .imageFormat = surface_format.format,
      .imageColorSpace = surface_format.colorSpace,
      .imageExtent = extent,
      .imageArrayLayers = 1,
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .preTransform = capabilities.currentTransform,
      .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      .presentMode = VK_PRESENT_MODE_FIFO_KHR,
      .clipped = VK_TRUE,
  };

  if (vkCreateSwapchainKHR(ap->device, &swapchain_info, NULL,
                           &surf->swapchain) != VK_SUCCESS) {
    printf("[Stygian AP VK] Failed to create swapchain for surface\n");
    return false;
  }

  // Get swapchain images
  vkGetSwapchainImagesKHR(ap->device, surf->swapchain, &surf->image_count,
                          NULL);
  if (surf->image_count > STYGIAN_VK_MAX_SWAPCHAIN_IMAGES)
    surf->image_count = STYGIAN_VK_MAX_SWAPCHAIN_IMAGES;
  vkGetSwapchainImagesKHR(ap->device, surf->swapchain, &surf->image_count,
                          surf->swapchain_images);

  surf->format = surface_format.format;
  surf->extent = extent;

  // Create image views
  for (uint32_t i = 0; i < surf->image_count; i++) {
    VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = surf->swapchain_images[i],
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = surf->format,
        .components = {VK_COMPONENT_SWIZZLE_IDENTITY,
                       VK_COMPONENT_SWIZZLE_IDENTITY,
                       VK_COMPONENT_SWIZZLE_IDENTITY,
                       VK_COMPONENT_SWIZZLE_IDENTITY},
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
    };

    if (vkCreateImageView(ap->device, &view_info, NULL,
                          &surf->swapchain_views[i]) != VK_SUCCESS) {
      return false;
    }
  }

  // Create framebuffers
  for (uint32_t i = 0; i < surf->image_count; i++) {
    VkImageView attachments[] = {surf->swapchain_views[i]};

    VkFramebufferCreateInfo fb_info = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = ap->render_pass,
        .attachmentCount = 1,
        .pAttachments = attachments,
        .width = surf->extent.width,
        .height = surf->extent.height,
        .layers = 1,
    };

    if (vkCreateFramebuffer(ap->device, &fb_info, NULL,
                            &surf->framebuffers[i]) != VK_SUCCESS) {
      return false;
    }
  }

  // Create sync objects and command buffer (ONLY IF MISSING)
  if (!surf->image_available) {
    VkSemaphoreCreateInfo sem_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fence_info = {.sType =
                                        VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                                    .flags = VK_FENCE_CREATE_SIGNALED_BIT};

    vkCreateSemaphore(ap->device, &sem_info, NULL, &surf->image_available);
    vkCreateSemaphore(ap->device, &sem_info, NULL, &surf->render_finished);
    vkCreateFence(ap->device, &fence_info, NULL, &surf->in_flight);
  }

  if (!surf->command_buffer) {
    VkCommandBufferAllocateInfo cmd_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = ap->command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    vkAllocateCommandBuffers(ap->device, &cmd_info, &surf->command_buffer);
  }

  printf("[Stygian AP VK] Surface created: %dx%d\n", extent.width,
         extent.height);
  surf->resize_suboptimal_count = 0;
  return true;
}

// Helper to clean up swapchain resources (for resize or destroy)
static void cleanup_surface_swapchain(StygianAP *ap,
                                      StygianAPSurface *surface) {
  if (surface->in_flight) {
    vkWaitForFences(ap->device, 1, &surface->in_flight, VK_TRUE, UINT64_MAX);
  }

  // Destroy framebuffers and image views
  for (uint32_t i = 0; i < surface->image_count; i++) {
    if (surface->framebuffers[i]) {
      vkDestroyFramebuffer(ap->device, surface->framebuffers[i], NULL);
      surface->framebuffers[i] = VK_NULL_HANDLE;
    }
    if (surface->swapchain_views[i]) {
      vkDestroyImageView(ap->device, surface->swapchain_views[i], NULL);
      surface->swapchain_views[i] = VK_NULL_HANDLE;
    }
  }

  if (surface->swapchain) {
    vkDestroySwapchainKHR(ap->device, surface->swapchain, NULL);
    surface->swapchain = VK_NULL_HANDLE;
  }
}

StygianAPSurface *stygian_ap_surface_create(StygianAP *ap,
                                            StygianWindow *window) {
  if (!ap || !window)
    return NULL;

  StygianAPSurface *surf = (StygianAPSurface *)ap_alloc(
      ap, sizeof(StygianAPSurface), _Alignof(StygianAPSurface));
  if (!surf)
    return NULL;
  memset(surf, 0, sizeof(StygianAPSurface));

  surf->ap = ap;
  surf->window = window;
  surf->resize_debounce_frames = ap->resize_debounce_frames;

  int width, height;
  stygian_window_get_framebuffer_size(window, &width, &height);

  if (!create_surface_swapchain(surf, width, height)) {
    ap_free(ap, surf);
    return NULL;
  }

  return surf;
}

void stygian_ap_surface_destroy(StygianAP *ap, StygianAPSurface *surface) {
  if (!ap || !surface)
    return;

  vkDeviceWaitIdle(ap->device);

  // Destroy sync objects
  if (surface->image_available)
    vkDestroySemaphore(ap->device, surface->image_available, NULL);
  if (surface->render_finished)
    vkDestroySemaphore(ap->device, surface->render_finished, NULL);
  if (surface->in_flight)
    vkDestroyFence(ap->device, surface->in_flight, NULL);

  // Cleanup swapchain resources
  cleanup_surface_swapchain(ap, surface);

  if (surface->surface)
    vkDestroySurfaceKHR(ap->instance, surface->surface, NULL);

  ap_free(ap, surface);
  printf("[Stygian AP VK] Surface destroyed\n");
}

void stygian_ap_surface_begin(StygianAP *ap, StygianAPSurface *surface,
                              int width, int height) {
  if (!ap || !surface)
    return;
  surface->frame_active = false;

  if (width <= 0 || height <= 0) {
    return; // Window minimized or invalid size
  }

  // Coalesce resize churn for secondary surfaces too (same policy as main).
  if (surface->swapchain_needs_recreate ||
      surface->extent.width != (uint32_t)width ||
      surface->extent.height != (uint32_t)height) {
    if (surface->resize_pending_w != width ||
        surface->resize_pending_h != height) {
      surface->resize_pending_w = width;
      surface->resize_pending_h = height;
      surface->resize_stable_count = 0;
      return;
    }
    surface->resize_stable_count++;
    if (surface->resize_stable_count < surface->resize_debounce_frames) {
      return;
    }

    cleanup_surface_swapchain(ap, surface);
    if (!create_surface_swapchain(surface, width, height)) {
      printf("[Stygian AP VK] Failed to recreate swapchain during resize\n");
      return;
    }
    surface->swapchain_needs_recreate = false;
    surface->resize_stable_count = 0;
  } else {
    surface->resize_pending_w = width;
    surface->resize_pending_h = height;
    surface->resize_stable_count = 0;
  }

  // Wait for previous frame
  VkResult fence_res =
      vkWaitForFences(ap->device, 1, &surface->in_flight, VK_TRUE, UINT64_MAX);
  if (fence_res != VK_SUCCESS) {
    printf("[Stygian AP VK] Wait for fences failed: %d\n", fence_res);
    return;
  }
  vkResetFences(ap->device, 1, &surface->in_flight);

  // Acquire next image
  VkResult result = vkAcquireNextImageKHR(
      ap->device, surface->swapchain, UINT64_MAX, surface->image_available,
      VK_NULL_HANDLE, &surface->current_image);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    surface->swapchain_needs_recreate = true;
    return;
  }

  if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    printf("[Stygian AP VK] Failed to acquire image: %d\n", result);
    return;
  }
  if (result == VK_SUBOPTIMAL_KHR) {
    surface->resize_suboptimal_count++;
    if (surface->resize_suboptimal_count >= ap->resize_suboptimal_threshold) {
      surface->swapchain_needs_recreate = true;
    }
  } else {
    surface->resize_suboptimal_count = 0;
  }

  // Begin command buffer
  vkResetCommandBuffer(surface->command_buffer, 0);

  VkCommandBufferBeginInfo begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
  };
  vkBeginCommandBuffer(surface->command_buffer, &begin_info);

  // Begin render pass
  VkClearValue clear_color = {{{0.08f, 0.08f, 0.08f, 1.0f}}};
  VkRenderPassBeginInfo rp_info = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass = ap->render_pass,
      .framebuffer = surface->framebuffers[surface->current_image],
      .renderArea = {{0, 0}, surface->extent},
      .clearValueCount = 1,
      .pClearValues = &clear_color,
  };
  vkCmdBeginRenderPass(surface->command_buffer, &rp_info,
                       VK_SUBPASS_CONTENTS_INLINE);

  // Set viewport and scissor (PHYSICAL)
  VkViewport viewport = {
      .x = 0,
      .y = 0,
      .width = (float)surface->extent.width,
      .height = (float)surface->extent.height,
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  };

  // Periodic floating-surface extent log for resize diagnostics.
  static int surf_debug = 0;
  if (surf_debug++ % 600 == 0) {
    if (surface != ap->main_surface) { // Only log floating windows
      printf("[Stygian VK] Surface Extent: %dx%d\n", surface->extent.width,
             surface->extent.height);
    }
  }
  VkRect2D scissor = {{0, 0}, surface->extent};
  vkCmdSetViewport(surface->command_buffer, 0, 1, &viewport);
  vkCmdSetScissor(surface->command_buffer, 0, 1, &scissor);

  // Bind pipeline and descriptors
  vkCmdBindPipeline(surface->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    ap->graphics_pipeline);
  vkCmdBindDescriptorSets(surface->command_buffer,
                          VK_PIPELINE_BIND_POINT_GRAPHICS, ap->pipeline_layout,
                          0, 1, &ap->descriptor_set, 0, NULL);
  surface->frame_active = true;
}

void stygian_ap_surface_submit(StygianAP *ap, StygianAPSurface *surface,
                               const StygianSoAHot *soa_hot, uint32_t count) {
  if (!ap || !surface || !surface->frame_active || count == 0)
    return;

  // SoA data is uploaded via stygian_ap_submit_soa(); no AoS memcpy needed.

  // Push viewport size constant (LOGICAL SIZE for correct projection!)
  // If physical != logical (DPI scaling), we must use logical size here
  // because the UI elements were laid out in logical coordinates.
  int log_w, log_h;
  if (surface->window) {
    stygian_window_get_size(surface->window, &log_w, &log_h);
  } else {
    // Fallback if no window attached (?)
    log_w = surface->extent.width;
    log_h = surface->extent.height;
  }

  {
    StygianVKPushConstants pc;
    fill_push_constants(ap, (float)log_w, (float)log_h, &pc);
    vkCmdPushConstants(surface->command_buffer, ap->pipeline_layout,
                       VK_SHADER_STAGE_VERTEX_BIT |
                           VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(pc), &pc);
  }

  // Bind vertex buffer and draw
  VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers(surface->command_buffer, 0, 1, &ap->vertex_buffer,
                         &offset);

  VkViewport viewport = {
      .x = 0.0f,
      .y = 0.0f,
      .width = (float)surface->extent.width,
      .height = (float)surface->extent.height,
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  };
  VkRect2D scissor = {
      .offset = {0, 0},
      .extent = surface->extent,
  };
  vkCmdSetViewport(surface->command_buffer, 0, 1, &viewport);
  vkCmdSetScissor(surface->command_buffer, 0, 1, &scissor);

  vkCmdDraw(surface->command_buffer, 6, count, 0, 0);
}

void stygian_ap_surface_end(StygianAP *ap, StygianAPSurface *surface) {
  if (!ap || !surface || !surface->frame_active)
    return;

  vkCmdEndRenderPass(surface->command_buffer);
  vkEndCommandBuffer(surface->command_buffer);
}

void stygian_ap_surface_swap(StygianAP *ap, StygianAPSurface *surface) {
  if (!ap || !surface || !surface->frame_active)
    return;

  // Submit command buffer
  VkPipelineStageFlags wait_stages[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

  VkSubmitInfo submit_info = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &surface->image_available,
      .pWaitDstStageMask = wait_stages,
      .commandBufferCount = 1,
      .pCommandBuffers = &surface->command_buffer,
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = &surface->render_finished,
  };

  VkResult res =
      vkQueueSubmit(ap->graphics_queue, 1, &submit_info, surface->in_flight);
  if (res != VK_SUCCESS) {
    printf("[Stygian AP VK] Failed to submit draw command buffer: %d\n", res);
  }

  // Present
  VkPresentInfoKHR present_info = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &surface->render_finished,
      .swapchainCount = 1,
      .pSwapchains = &surface->swapchain,
      .pImageIndices = &surface->current_image,
  };

  res = vkQueuePresentKHR(ap->graphics_queue, &present_info);
  if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
    surface->swapchain_needs_recreate = true;
  } else if (res != VK_SUCCESS) {
    printf("[Stygian AP VK] Failed to present: %d\n", res);
  }

  // Wait for present to finish (simple sync for multi-window stability)
  // vkQueueWaitIdle(ap->graphics_queue);
  surface->frame_active = false;
}

StygianAPSurface *stygian_ap_get_main_surface(StygianAP *ap) {
  return ap ? ap->main_surface : NULL;
}
