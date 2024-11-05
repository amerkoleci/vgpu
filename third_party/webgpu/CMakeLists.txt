# see: https://github.com/eliemichel/WebGPU-distribution/tree/wgpu
if (EMSCRIPTEN)
	add_library(webgpu INTERFACE)

	target_include_directories(webgpu INTERFACE
		"${CMAKE_CURRENT_SOURCE_DIR}/include-emscripten"
	)

	# This is used to advertise the flavor of WebGPU that this zip provides
	target_compile_definitions(webgpu INTERFACE WEBGPU_BACKEND_EMSCRIPTEN)

	target_link_options(webgpu INTERFACE
		-sUSE_WEBGPU # Handle WebGPU symbols
	)
else()
    add_library(webgpu INTERFACE)

    # This is used to advertise the flavor of WebGPU that this zip provides
	target_compile_definitions(webgpu INTERFACE WEBGPU_BACKEND_WGPU)

    target_include_directories(webgpu INTERFACE
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
	)

    target_sources(webgpu INTERFACE 
        ${CMAKE_CURRENT_SOURCE_DIR}/include/webgpu/webgpu.h
        ${CMAKE_CURRENT_SOURCE_DIR}/include/webgpu/wgpu.h
    )

endif()
