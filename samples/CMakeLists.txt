function(add_sample sample_name)
    file(GLOB SOURCE_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/${sample_name}/*.h"
	    "${CMAKE_CURRENT_SOURCE_DIR}/${sample_name}/*.cpp"
    )

    if (ANDROID)
        add_library(${sample_name} SHARED ${SOURCE_FILES})
        target_link_libraries(${sample_name} log android native_app_glue)
    else ()
    	add_executable(${sample_name} ${SOURCE_FILES})
        target_compile_definitions(${sample_name} PRIVATE GLFW_INCLUDE_NONE)
        target_link_libraries(${sample_name} glfw)
    endif ()
    target_link_libraries(${sample_name} vgpu)

    #if (WIN32)
    #    set_target_properties(${sample_name} PROPERTIES WIN32_EXECUTABLE TRUE)
    #endif ()

    if (PLATFORM_UWP)
	    set_target_properties(${sample_name} PROPERTIES VS_WINDOWS_TARGET_PLATFORM_MIN_VERSION "${UWP_MIN_VERSION}")
    endif ()

    set_property(TARGET ${sample_name} PROPERTY FOLDER "Samples")
    set_property(TARGET ${sample_name} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

    if (VGPU_INSTALL)
        install(
            TARGETS ${sample_name}
            RUNTIME DESTINATION bin
        )
    endif ()
endfunction()

add_sample(HelloWorld)
