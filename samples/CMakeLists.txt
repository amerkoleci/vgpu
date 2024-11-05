function(add_sample SAMPLE_NAME)
    file(GLOB SOURCE_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/${SAMPLE_NAME}/*.h"
	    "${CMAKE_CURRENT_SOURCE_DIR}/${SAMPLE_NAME}/*.cpp"
    )

    if (ANDROID)
        add_library(${SAMPLE_NAME} SHARED ${SOURCE_FILES})
        target_link_libraries(${SAMPLE_NAME} log android native_app_glue)
    else ()
    	add_executable(${SAMPLE_NAME} ${SOURCE_FILES})
        target_compile_definitions(${SAMPLE_NAME} PRIVATE GLFW_INCLUDE_NONE)
        target_link_libraries(${SAMPLE_NAME} glfw)
    endif ()

    if (EMSCRIPTEN)
        target_link_options(${SAMPLE_NAME} PUBLIC
            "-Os"
            "-sUSE_GLFW=3"
            "-sFORCE_FILESYSTEM=1"
        )

        set_target_properties(${SAMPLE_NAME} PROPERTIES SUFFIX ".html")
    endif ()

    target_link_libraries(${SAMPLE_NAME} vgpu)

    set_target_properties(${SAMPLE_NAME} PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        FOLDER "Samples"
    )

    if (VGPU_INSTALL)
        install(
            TARGETS ${SAMPLE_NAME}
            RUNTIME DESTINATION bin
        )
    endif ()
endfunction()

add_sample(HelloWorld)
