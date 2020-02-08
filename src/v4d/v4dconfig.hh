#pragma once

// Needed for ImGui
namespace VkFunctions {}
#define XVK_EXPOSE_NATIVE_VULKAN_FUNCTIONS_NAMESPACE VkFunctions
#ifdef _V4D_CORE
	#define XVK_EXPORT
#else
	#define XVK_IMPORT
#endif
