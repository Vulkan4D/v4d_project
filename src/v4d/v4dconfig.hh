#pragma once

#define ZAP_USE_REINTERPRET_CAST_INSTEAD_OF_MEMCPY
// #define EVENT_DEFINITIONS_VARIADIC

// Vulkan configuration
#define VULKAN_VALIDATION_ABORT_ON_ERROR
#define V4D_VULKAN_USE_VMA

// #define XVK_USE_QT_VULKAN_LOADER // uncomment if using Qt
#define XVK_INCLUDE_GLFW // comment if using Qt or another window context manager

// GLM Configuration
#define XVK_INCLUDE_GLM
#define GLM_FORCE_INTRINSICS
#define GLM_FORCE_SIMD_AVX2
#define GLM_FORCE_CXX17
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL

// Needed for ImGui and VMA
namespace VkFunctions {}
#define XVK_EXPOSE_NATIVE_VULKAN_FUNCTIONS_NAMESPACE VkFunctions
#ifdef _V4D_CORE
	#define XVK_EXPORT
#else
	#define XVK_IMPORT
#endif

#define V4D_RENDERER_RAYTRACING_USE_DEVICE_LOCAL_VERTEX_INDEX_BUFFERS

// #define V4D_INCLUDE_TINYOBJLOADER
#define V4D_INCLUDE_TINYGLTFLOADER
