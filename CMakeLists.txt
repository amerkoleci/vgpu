# CMake version
cmake_minimum_required (VERSION 3.14)

if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
	message(FATAL_ERROR "In-source builds are not supported. Please choose a different binary directory.")
	return()
endif()

# Configure CMake global variables
set(CMAKE_INSTALL_MESSAGE LAZY)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# Use solution folders to organize projects
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Project declaration
project (vgfx C CXX)

# Build Type
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	# By default, we use Release
	message(STATUS "Setting build type to 'Release' as none was specified.")
	set(CMAKE_BUILD_TYPE "Release" CACHE
		STRING "Choose the type of build." FORCE
	)

	# Set the possible values of build type for cmake-gui
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo")
endif()

# Platform Flags
if(APPLE)
	set(CMAKE_MACOSX_RPATH ON)
	set(CMAKE_OSX_DEPLOYMENT_TARGET 10.9)
	set(LOBJC "objc")
endif()

# Options
include(CMakeDependentOption)

option(VGFX_BUILD_SHARED "Build vgfx as a shared library (DLL or .so)" ON)
option(VGFX_INSTALL "Generate install rules for vgfx" ON)
cmake_dependent_option(VGFX_VULKAN_DRIVER "Build the Vulkan backend" ON "NOT EMSCRIPTEN" OFF)
cmake_dependent_option(VGFX_D3D11_DRIVER "Build the D3D11 backend" ON "WIN32" OFF)
cmake_dependent_option(VGFX_D3D12_DRIVER "Build the D3D12 backend" ON "WIN32" OFF)
cmake_dependent_option(VGFX_WEBGPU_DRIVER "Build the WebGPU backend" ON "EMSCRIPTEN" OFF)
option(VGFX_SAMPLES "Build samples" ON)

if (VGFX_BUILD_SHARED)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

# Build version
set (VGFX_VERSION_MAJOR 0)
set (VGFX_VERSION_MINOR 1)
set (VGFX_VERSION_PATCH 0)
set (VGPU_VERSION ${VGFX_VERSION_MAJOR}.${VGFX_VERSION_MINOR}.${VGFX_VERSION_PATCH})

execute_process(COMMAND git rev-list --count HEAD
    OUTPUT_VARIABLE VGPU_REVISION_ID
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

if ("${VGPU_REVISION_ID}" STREQUAL "")
    # We don't have any source control so just hard-code all of this information to empty
    set (VGPU_REVISION_ID 0)
    set (VGPU_SHORT_CHANGE_SET 0)
    set (VGPU_CHANGE_SET 0)
    set (VGPU_CHANGE_SET_DATE "")
else ()
    execute_process(COMMAND git rev-parse --short HEAD
        OUTPUT_VARIABLE VGPU_SHORT_CHANGE_SET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    execute_process(COMMAND git rev-parse HEAD
        OUTPUT_VARIABLE VGPU_CHANGE_SET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    execute_process(COMMAND git log -1 --abbrev=12 --date=format:%Y-%m-%d --pretty=format:%cd
        OUTPUT_VARIABLE VGPU_CHANGE_SET_DATE
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif ()

# Build flags
if(NOT MSVC)
    #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -Wno-strict-aliasing -fno-exceptions -fno-rtti")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-strict-aliasing -fno-exceptions -fno-rtti")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g3 -D_DEBUG=1 -Wno-unused -O0")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g0 -DNDEBUG=1 -flto -O3")
endif()

if(EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s DISABLE_EXCEPTION_CATCHING=1")

    set(CMAKE_EXE_LINKER_FLAGS "-s ASSERTIONS=0 -s EVAL_CTORS=1 -s WASM=1 -s USE_WEBGPU=1 -s NO_EXIT_RUNTIME=1 -s STRICT=1 --shell-file ${CMAKE_CURRENT_LIST_DIR}/assets/shell.html")

	# Linker flags to optimize for smallest output code size
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s ENVIRONMENT=web -s MINIMAL_RUNTIME=2 -s TEXTDECODER=2 -s ABORTING_MALLOC=0 -s ALLOW_MEMORY_GROWTH=0 -s SUPPORT_ERRNO=0 -s MALLOC=emmalloc -s NO_FILESYSTEM=1 --output_eol=linux")
endif ()

if (IOS OR EMSCRIPTEN)
    set(VGFX_BUILD_SHARED OFF CACHE BOOL "Always Disable shared library on (IOS, WEB)" FORCE)
endif ()

# Third party
add_subdirectory(third_party)

# Setup library
set(SOURCE_FILES
    include/vgfx.h
    src/vgfx_driver.h
    src/vgfx.c
    src/vgfx_driver_vulkan.cpp
    src/vgfx_driver_d3d11.cpp
    src/vgfx_driver_d3d12.cpp
    src/vgfx_driver_webgpu.cpp
)

if (VGFX_BUILD_SHARED)
    add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES})
    target_compile_definitions(${PROJECT_NAME}
        PUBLIC VGFX_SHARED_LIBRARY=1
        PRIVATE VGFX_IMPLEMENTATION=1
    )
else()
    add_library(${PROJECT_NAME} STATIC ${SOURCE_FILES})
endif ()

# MinGW builds should statically link libgcc
if(MINGW)
    target_link_libraries(${PROJECT_NAME} PRIVATE -static-libgcc)
endif()

if(MSVC)
     # Use fast floating point model
    target_compile_options(${PROJECT_NAME} PRIVATE /fp:fast)

    # Set warning level 4
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)

    # Warnings as Errors Enabled
    target_compile_options(${PROJECT_NAME} PRIVATE /WX)

    # Multiprocessor compiler
    target_compile_options(${PROJECT_NAME} PRIVATE /MP)
endif ()

if (VGFX_VULKAN_DRIVER)
    target_compile_definitions(${PROJECT_NAME} PRIVATE VGFX_VULKAN_DRIVER=1)
    target_link_libraries(${PROJECT_NAME} PRIVATE volk vma spirv-reflect)
endif ()

if (VGFX_D3D12_DRIVER)
    target_compile_definitions(${PROJECT_NAME} PRIVATE VGFX_D3D12_DRIVER=1)
endif ()

if (VGFX_D3D11_DRIVER)
    target_compile_definitions(${PROJECT_NAME} PRIVATE VGFX_D3D11_DRIVER=1)
endif ()

if (VGFX_WEBGPU_DRIVER)
    target_compile_definitions(${PROJECT_NAME} PRIVATE VGFX_WEBGPU_DRIVER=1)
endif ()

# Include directories
target_include_directories(${PROJECT_NAME} PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include>
)

# Soname
set_target_properties(vgfx PROPERTIES
    OUTPUT_NAME "vgfx"
	VERSION ${VGPU_VERSION}
	SOVERSION ${VGFX_VERSION_MAJOR}
)

# Samples
if (VGFX_SAMPLES)
    add_subdirectory(samples)
endif ()

# Install
if (VGFX_INSTALL)
    # README.md and license
    install (FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/README.md
        ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE
        DESTINATION ${CMAKE_INSTALL_PREFIX}
    )

    install (DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include DESTINATION ${CMAKE_INSTALL_PREFIX}/include FILES_MATCHING PATTERN *.h)

    install(TARGETS ${PROJECT_NAME}
        EXPORT ${PROJECT_NAME}
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
    )
endif ()

# Print current build configuration
message(STATUS "VGPU version ${VGPU_VERSION}")
message(STATUS "GIT Revision ID: ${VGPU_REVISION_ID}")
message(STATUS "GIT Short ChangeSet: ${VGPU_SHORT_CHANGE_SET}")
message(STATUS "GIT ChangeSet: ${VGPU_CHANGE_SET}")
message(STATUS "GIT ChangeSet Date: ${VGPU_CHANGE_SET_DATE}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

if (VGFX_BUILD_SHARED)
    message(STATUS "  Library         SHARED")
else ()
    message(STATUS "  Library         STATIC")
endif ()