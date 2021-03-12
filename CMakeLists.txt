# CMake version
cmake_minimum_required(VERSION 3.10)
project(vgpu)

set(VGPU_VERSION_MAJOR 0)
set(VGPU_VERSION_MINOR 1)
set(VGPU_VERSION_PATCH 0)
set(VGPU_VERSION ${VGPU_VERSION_MAJOR}.${VGPU_VERSION_MINOR}.${VGPU_VERSION_PATCH})

# Enable folders in IDE (VisualStudio)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Output paths
set(VGPU_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(VGPU_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR})
mark_as_advanced(VGPU_ROOT_DIR)

set (CMAKE_POSITION_INDEPENDENT_CODE ON)
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${VGPU_BUILD_DIR}/bin")
set (CMAKE_PDB_OUTPUT_DIRECTORY "${VGPU_BUILD_DIR}/bin")
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY "${VGPU_BUILD_DIR}/lib")
set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${VGPU_BUILD_DIR}/lib")

# Options
if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
	option(VGPU_BACKEND_D3D11 "Include Direct3D12 backend" ON)
	option(VGPU_BACKEND_D3D12 "Include Direct3D12 backend" ON)
	option(VGPU_BACKEND_OPENGL "Include OpenGL backend" ON)
	option(VGPU_BACKEND_VULKAN "Include Vulkan backend" ON)
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "WindowsStore")
	option(VGPU_BACKEND_D3D11 "Include Direct3D12 backend" ON)
	option(VGPU_BACKEND_D3D12 "Include Direct3D12 backend" ON)
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Durango")
	# XboxOne
	option(VGPU_BACKEND_D3D11 "Include Direct3D12 backend" ON)
	option(VGPU_BACKEND_D3D12 "Include Direct3D12 backend" ON)
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
	set(VGPU_BACKEND_VULKAN ON CACHE BOOL "Include Vulkan renderer")
else()
	set(VGPU_BACKEND_OPENGL ON CACHE BOOL "Include OpenGL renderer")
	set(VGPU_BACKEND_OPENGL ON CACHE BOOL "Include OpenGL renderer")
endif ()

# Platform Flags
if(APPLE)
	set(CMAKE_MACOSX_RPATH ON)
	set(CMAKE_OSX_DEPLOYMENT_TARGET 10.9)
	set(LOBJC "objc")
elseif(WIN32)
	# "vgpu.dll", not "vgpu.dll"
	set(CMAKE_SHARED_LIBRARY_PREFIX "")

	option(ALIMER_USE_STATIC_CRT "Link against the static runtime libraries." ON)

	# Select static/dynamic runtime library
	if (ALIMER_USE_STATIC_CRT)
		string(REGEX REPLACE "/MDd" "/MTd" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
		string(REGEX REPLACE "/MDd" "/MTd" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
		string(REGEX REPLACE "/MD" "/MT" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
		string(REGEX REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
	else()
		string(REGEX REPLACE "/MTd" "/MDd" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
		string(REGEX REPLACE "/MTd" "/MDd" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
		string(REGEX REPLACE "/MT" "/MD" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
		string(REGEX REPLACE "/MT" "/MD" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
	endif()
endif()

if (ANDROID OR IOS)
	set(BUILD_EXAMPLES OFF CACHE INTERNAL "Build examples" FORCE)
else ()
	option (BUILD_EXAMPLES "Build the vgpu samples" ON)
endif ()

option(VGPU_INSTALL "Generate installation target" ON)
option(BUILD_SHARED_LIBS "Build shared library" ON)

# third_party

# GLFW
if (BUILD_EXAMPLES AND NOT (EMSCRIPTEN OR ANDROID))
	set (GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
	set (GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
	set (GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
	set (GLFW_INSTALL OFF CACHE BOOL "" FORCE)
	set(BUILD_SHARED_LIBS OFF CACHE BOOL "")
	#set (GLFW_USE_WAYLAND ${VGPU_USE_WAYLAND} CACHE BOOL "" FORCE)
	add_subdirectory(third_party/glfw)
	set_property(TARGET "glfw" PROPERTY FOLDER "third_party")
	unset (LIB_SUFFIX CACHE)
endif ()

# vgpu
set(SOURCE_FILES
	# Public Headers
	include/vgpu/vgpu.h
	# Internal Headers
	src/vgpu_driver.h
	src/vgpu_d3d_common.h
	# Source Files
	src/vgpu.cpp
	src/vgpu_driver_vulkan.cpp
	src/vgpu_driver_d3d11.cpp
	src/vgpu_driver_d3d12.cpp
)

add_library (vgpu ${SOURCE_FILES})
target_compile_definitions(vgpu PRIVATE VGPU_IMPLEMENTATION)

if (BUILD_SHARED_LIBS)
	target_compile_definitions(vgpu PRIVATE VGPU_SHARED_LIBRARY)
endif ()

if (VGPU_BACKEND_D3D11)
	target_compile_definitions(vgpu PRIVATE VGPU_DRIVER_D3D11)
endif ()

if (VGPU_BACKEND_D3D12)
	target_sources(vgpu PRIVATE
		src/d3d12/D3D12MemAlloc.h
		src/d3d12/D3D12MemAlloc.cpp
	)

	target_compile_definitions(vgpu PRIVATE VGPU_DRIVER_D3D12)

	#target_include_directories(vgpu PUBLIC
	#	"$<BUILD_INTERFACE:${VGPU_ROOT_DIR}/src/d3d12>"
	#)

endif ()

if (VGPU_BACKEND_VULKAN)
	target_sources(vgpu PRIVATE
		src/vk/vkbind.h
		src/vk/vk_mem_alloc.h
		
	)

	target_compile_definitions(vgpu PRIVATE VGPU_DRIVER_VULKAN)
endif ()

# Build flags
if (NOT MSVC)
	set_property(TARGET vgpu PROPERTY COMPILE_FLAGS "-std=gnu99 -Wall -Wno-strict-aliasing -pedantic")
endif()

# MinGW builds should statically link libgcc
if (MINGW)
	target_link_libraries(vgpu PRIVATE -static-libgcc)
endif()

set_target_properties(vgpu PROPERTIES
	OUTPUT_NAME "vgpu"
	VERSION ${VGPU_VERSION}
	SOVERSION ${VGPU_VERSION_MAJOR}
)

target_include_directories(vgpu PUBLIC
	"$<BUILD_INTERFACE:${VGPU_ROOT_DIR}/include>"
	"$<INSTALL_INTERFACE:${CMAKE_INSTALL_FULL_INCLUDEDIR}>"
)

if (MSVC)
	# Set warning level 3 and treat all other warnings as errors
	target_compile_options(vgpu PRIVATE
		/W3
		/WX
	)
endif ()

if (VGPU_INSTALL)
	install(TARGETS vgpu
		EXPORT vgpu
		RUNTIME DESTINATION "bin"
		ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
		LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	)
endif()

if (WIN32)
	# Shader compiler
	set(SHADERC_SOURCE_FILES
		tools/vgpu_shader.h
		tools/vgpu_shader.cpp
	)

	add_library (vgpu_shader ${SHADERC_SOURCE_FILES})
	target_compile_definitions(vgpu_shader PRIVATE VGPU_SHADER_IMPLEMENTATION)
	
	target_include_directories(vgpu_shader PUBLIC
		"$<BUILD_INTERFACE:${VGPU_ROOT_DIR}/tools>"
		"$<INSTALL_INTERFACE:${CMAKE_INSTALL_FULL_INCLUDEDIR}>"
	)

	if (BUILD_SHARED_LIBS)
		target_compile_definitions(vgpu_shader PRIVATE VGPU_SHARED_LIBRARY)
	endif ()
endif ()

# samples
if (BUILD_EXAMPLES)
	add_subdirectory(samples)
endif ()

if (VGPU_INSTALL)
	install(DIRECTORY include/vgpu DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} FILES_MATCHING PATTERN vgpu.h)
endif ()

# Print current build configuration
message(STATUS "Build Configuration:")
if (BUILD_SHARED_LIBS)
	message(STATUS "  Library         SHARED")
else ()
	message(STATUS "  Library         STATIC")
endif ()
