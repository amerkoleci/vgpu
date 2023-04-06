# CMake version
cmake_minimum_required (VERSION 3.16)

if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
	message(FATAL_ERROR "In-source builds are not supported. Please choose a different binary directory.")
	return()
endif()

# Build version
set (VGPU_VERSION_MAJOR 1)
set (VGPU_VERSION_MINOR 0)
set (VGPU_VERSION_PATCH 0)
set (VGPU_VERSION ${VGPU_VERSION_MAJOR}.${VGPU_VERSION_MINOR}.${VGPU_VERSION_PATCH})

# Project declaration
project (vgpu
    VERSION ${VGPU_VERSION}
    DESCRIPTION "Cross platform low-level graphics API"
    HOMEPAGE_URL "https://github.com/amerkoleci/vgpu"
    LANGUAGES CXX C
)

# Configure CMake global variables
set (CMAKE_CXX_STANDARD 11)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_EXTENSIONS OFF)
set (CMAKE_C_STANDARD 99)

# Configure CMake global variables
set(CMAKE_INSTALL_MESSAGE LAZY)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Use solution folders to organize projects
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

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
if (IOS OR EMSCRIPTEN)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Disable shared library on (IOS, WEB)" FORCE)
else()
    option(BUILD_SHARED_LIBS "Build shared library" ON)
endif ()

option(VGPU_INSTALL "Generate install rules for vgpu" ON)
cmake_dependent_option(VGPU_VULKAN_DRIVER "Build the Vulkan backend" ON "NOT EMSCRIPTEN;NOT WINDOWS_STORE" OFF)
cmake_dependent_option(VGPU_D3D11_DRIVER "Build the D3D11 backend" ON "WIN32" OFF)
cmake_dependent_option(VGPU_D3D12_DRIVER "Build the D3D12 backend" ON "WIN32" OFF)
cmake_dependent_option(VGPU_WEBGPU_DRIVER "Build the WebGPU backend" ON "EMSCRIPTEN" OFF)
cmake_dependent_option(VGPU_SAMPLES "Build samples" ON "NOT WINDOWS_STORE" OFF)

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

# Third party
add_subdirectory(third_party)

# Setup library
set(SOURCE_FILES
    include/vgpu.h
    src/vgpu_driver.h
    src/vgpu.c
)

add_library(${PROJECT_NAME} ${SOURCE_FILES})
if (BUILD_SHARED_LIBS)
    target_compile_definitions(${PROJECT_NAME}
        PUBLIC VGPU_SHARED_LIBRARY=1
        PRIVATE VGPU_IMPLEMENTATION=1
    )
endif ()

# MinGW builds should statically link libgcc
if(MINGW)
    target_link_libraries(${PROJECT_NAME} PRIVATE -static-libgcc)
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wpedantic -Wextra)
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    # Use fast floating point model
    target_compile_options(${PROJECT_NAME} PRIVATE /fp:fast)

    # Set warning level 3
    target_compile_options(${PROJECT_NAME} PRIVATE /W3)

    # Warnings as Errors Enabled
    target_compile_options(${PROJECT_NAME} PRIVATE /WX)

    # Multiprocessor compiler
    target_compile_options(${PROJECT_NAME} PRIVATE /MP)

    target_compile_options(${PROJECT_NAME} PRIVATE /sdl /permissive- /JMC- /Zc:__cplusplus)

    if(ENABLE_CODE_ANALYSIS)
      target_compile_options(${PROJECT_NAME} PRIVATE /analyze)
    endif()

    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.24)
        target_compile_options(${PROJECT_NAME} PRIVATE /ZH:SHA_256)
    endif()

    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.26)
        target_compile_options(${PROJECT_NAME} PRIVATE /Zc:preprocessor /wd5104 /wd5105)
    endif()
endif ()

if (WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif ()

if (VGPU_D3D11_DRIVER)
    target_sources(${PROJECT_NAME} PRIVATE
        src/vgpu_driver_d3d11.cpp
    )

    target_compile_definitions(${PROJECT_NAME} PRIVATE VGPU_D3D11_DRIVER=1)

    if(WINDOWS_STORE)
        target_link_libraries(${PROJECT_NAME} PRIVATE d3d11 dxgi)
    endif ()
endif ()

if (VGPU_D3D12_DRIVER)
    target_sources(${PROJECT_NAME} PRIVATE
        src/vgpu_driver_d3d12.cpp
    )

    target_compile_definitions(${PROJECT_NAME} PRIVATE VGPU_D3D12_DRIVER=1)
    target_link_libraries(${PROJECT_NAME} PRIVATE DirectX-Headers D3D12MemoryAllocator)

    if(WINDOWS_STORE)
        target_link_libraries(${PROJECT_NAME} PRIVATE d3d12 dxgi)
    endif ()
endif ()

if (VGPU_VULKAN_DRIVER)
    if(ANDROID)
        target_compile_definitions(${PROJECT_NAME} PRIVATE VK_USE_PLATFORM_ANDROID_KHR)
    elseif(WIN32)
        target_compile_definitions(${PROJECT_NAME} PRIVATE VK_USE_PLATFORM_WIN32_KHR)
    elseif(APPLE)
        target_compile_definitions(${PROJECT_NAME} PRIVATE INTERFACE VK_USE_PLATFORM_METAL_EXT)
    elseif(UNIX)
        find_package(X11 REQUIRED)

        target_compile_definitions(${PROJECT_NAME} PRIVATE INTERFACE VK_USE_PLATFORM_XLIB_KHR)
        target_include_directories(${PROJECT_NAME} PRIVATE ${X11_INCLUDE_DIR})
        target_link_libraries(${PROJECT_NAME} PRIVATE ${X11_LIBRARIES})
    endif()

    target_sources(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/volk/volk.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/volk/volk.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/vma/include/vk_mem_alloc.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vgpu_driver_vulkan.cpp
    )

    target_include_directories(${PROJECT_NAME} PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/lib/volk>
	    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/lib/vma/include>
    )

    target_compile_definitions(${PROJECT_NAME} PRIVATE VGPU_VULKAN_DRIVER=1)
    target_link_libraries(${PROJECT_NAME} PRIVATE vulkan spirv-reflect)
endif ()

if (VGPU_WEBGPU_DRIVER)
    target_sources(${PROJECT_NAME} PRIVATE
        src/vgpu_driver_webgpu.cpp
    )

    target_compile_definitions(${PROJECT_NAME} PRIVATE VGPU_WEBGPU_DRIVER=1)
endif ()

# Include directories
target_include_directories(${PROJECT_NAME} PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/lib>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include>
)

# Libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    stb
)

# Soname
set_target_properties(vgpu PROPERTIES
    OUTPUT_NAME "vgpu"
	VERSION ${VGPU_VERSION}
	SOVERSION ${VGPU_VERSION_MAJOR}
)

# Samples
if (VGPU_SAMPLES)
    add_subdirectory(samples)
endif ()

# Install
if (VGPU_INSTALL)
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

if (BUILD_SHARED_LIBS)
    message(STATUS "  Library         SHARED")
else ()
    message(STATUS "  Library         STATIC")
endif ()

message(STATUS "  Backends:")
if (VGPU_VULKAN_DRIVER)
    message(STATUS "      - Vulkan")
endif()
if (VGPU_D3D11_DRIVER)
    message(STATUS "      - Direct3D11")
endif ()
if (VGPU_D3D12_DRIVER)
    message(STATUS "      - Direct3D12")
endif ()

if (VGPU_SAMPLES)
    message(STATUS "  Samples         ON")
else ()
    message(STATUS "  Samples         OFF")
endif ()
