# Define target name
set (TARGET_NAME vgpu)

# Setup library
set(SOURCE_FILES
    vgpu.h
    vgpu_driver.h
    vgpu.cpp
)

add_library(${TARGET_NAME} ${SOURCE_FILES})
if (BUILD_SHARED_LIBS)
    target_compile_definitions(${TARGET_NAME}
        PUBLIC VGPU_SHARED_LIBRARY=1
        PRIVATE VGPU_IMPLEMENTATION=1
    )
endif ()

# Include directories
target_include_directories(${TARGET_NAME} PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include>
)

set_target_properties(${TARGET_NAME} PROPERTIES
    CXX_STANDARD 11
    COMPILE_WARNING_AS_ERROR ON
)

if (MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE /W4)
else()
    target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra -pedantic)
endif()

if (VGPU_D3D11_DRIVER OR VGPU_D3D12_DRIVER)
    target_sources(${TARGET_NAME} PRIVATE
        vgpu_driver_d3d.h
    )

    if(WINDOWS_STORE)
        target_link_libraries(${TARGET_NAME} PRIVATE dxgi)
    endif ()
endif()

if (VGPU_D3D11_DRIVER)
    target_sources(${TARGET_NAME} PRIVATE
        vgpu_driver_d3d11.cpp
    )

    target_compile_definitions(${TARGET_NAME} PRIVATE VGPU_D3D11_DRIVER=1)

    if(WINDOWS_STORE)
        target_link_libraries(${TARGET_NAME} PRIVATE d3d11)
    endif ()
endif ()

if (VGPU_D3D12_DRIVER)
    target_sources(${TARGET_NAME} PRIVATE
        vgpu_driver_d3d12.cpp
    )

    target_compile_definitions(${TARGET_NAME} PRIVATE VGPU_D3D12_DRIVER=1)
    target_link_libraries(${TARGET_NAME} PRIVATE DirectX-Headers D3D12MemoryAllocator)

    if(WINDOWS_STORE)
        target_link_libraries(${TARGET_NAME} PRIVATE d3d12)
    endif ()
endif ()

if (VGPU_VULKAN_DRIVER)
    target_sources(${TARGET_NAME} PRIVATE
        vgpu_driver_vulkan.cpp
    )

    target_compile_definitions(${TARGET_NAME} PRIVATE VGPU_VULKAN_DRIVER=1)
    target_link_libraries(${TARGET_NAME} PRIVATE volk vma spirv-reflect)
endif ()

if (VGPU_WEBGPU_DRIVER)
    target_sources(${TARGET_NAME} PRIVATE
        vgpu_driver_webgpu.cpp
    )

    target_compile_definitions(${TARGET_NAME} PRIVATE VGPU_WEBGPU_DRIVER=1)
endif ()

# Soname
set_target_properties(${TARGET_NAME} PROPERTIES
    OUTPUT_NAME "${TARGET_NAME}"
	VERSION ${LIB_VERSION}
	SOVERSION ${LIB_VERSION_MAJOR}
)

# Install
if (VLIBS_INSTALL)
    install (FILES "vgpu.h" DESTINATION DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${TARGET_NAME})

    install(TARGETS ${TARGET_NAME}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif ()
