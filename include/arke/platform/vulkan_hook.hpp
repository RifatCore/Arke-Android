#pragma once

#include <cstdint>

namespace arke::platform {

// Vulkan hook callbacks
void onVkQueuePresentPre(void* queue, const void* presentInfo);
void onVkCreateShaderModulePre(void* device, const void* createInfo, void* module);

} // namespace arke::platform
