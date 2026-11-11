#include "arke/platform/vulkan_hook.hpp"
#include "arke/logger.hpp"

namespace arke::platform {

void onVkQueuePresentPre(void* queue, const void* presentInfo) {
    // Scaffolding for Vulkan swapchain presentation
}

void onVkCreateShaderModulePre(void* device, const void* createInfo, void* module) {
    // Scaffolding for Vulkan SPIR-V shader module replacement
}

} // namespace arke::platform
