set(PLUGIN_NAME PluginSamuraiEncoding)
set(PLUGIN_BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../kit")

if(ANDROID)
    if("${TARGET_ARCHITECTURE}" STREQUAL "ANDROID_arm64-v8a")
        set(KIT_DIRS android-arm64-v8a-client)
    else()
        set(KIT_DIRS android-x86_64-client)
    endif()
else()
    if("${TARGET_ARCHITECTURE}" STREQUAL "LINUX_arm64-v8a")
        set(NODE_TYPE linux-arm64-v8a)
    else()
        set(NODE_TYPE linux-x86_64)
    endif()
    set(KIT_DIRS ${NODE_TYPE}-client ${NODE_TYPE}-server)
endif()

add_library(${PLUGIN_NAME} SHARED
    encoding/PluginSamuraiEncoding.cpp
    encoding/samurai_encoder.cpp
    encoding/image.cpp
    encoding/aes.cpp
    encoding/sha256.cpp
    encoding/crc32.cpp
    encoding/stb/stb_image.c
    encoding/stb/stb_image_write.c
)
target_include_directories(${PLUGIN_NAME} PRIVATE encoding)
target_compile_definitions(${PLUGIN_NAME} PRIVATE
    BUILD_VERSION="${BUILD_VERSION}"
    STB_IMAGE_IMPLEMENTATION
    STB_IMAGE_WRITE_IMPLEMENTATION
)
set_target_properties(${PLUGIN_NAME} PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    NO_SONAME ON
)

install(TARGETS ${PLUGIN_NAME} LIBRARY DESTINATION lib/race/comms/${PLUGIN_NAME})
install(FILES manifest.json DESTINATION lib/race/comms/${PLUGIN_NAME})

foreach(KIT_DIR ${KIT_DIRS})
    set(KIT_PATH "${PLUGIN_BASE_DIR}/artifacts/${KIT_DIR}/${PLUGIN_NAME}")
    add_custom_command(TARGET ${PLUGIN_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${KIT_PATH}"
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${PLUGIN_NAME}>" "${KIT_PATH}/"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/manifest.json" "${KIT_PATH}/manifest.json"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${KIT_PATH}/config"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/../config/actions-fetch-only.json" "${KIT_PATH}/config/actions-fetch-only.json"
        VERBATIM
    )
endforeach()