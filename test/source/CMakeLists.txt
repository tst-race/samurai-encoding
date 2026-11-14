find_package(GTest REQUIRED CONFIG)
find_library(LIB_RACE_SDK_COMMON raceSdkCommon REQUIRED)

add_executable(encodingSamuraiTests
    TestPluginSamuraiEncoding.cpp
    ../../source/encoding/PluginSamuraiEncoding.cpp
    ../../source/encoding/samurai_encoder.cpp
    ../../source/encoding/image.cpp
    ../../source/encoding/aes.cpp
    ../../source/encoding/sha256.cpp
    ../../source/encoding/crc32.cpp
    ../../source/encoding/stb/stb_image.c
    ../../source/encoding/stb/stb_image_write.c
)
target_include_directories(encodingSamuraiTests PRIVATE ../../source/encoding)
target_compile_definitions(encodingSamuraiTests PRIVATE
    BUILD_VERSION="${BUILD_VERSION}"
    STB_IMAGE_IMPLEMENTATION
    STB_IMAGE_WRITE_IMPLEMENTATION
)
target_link_libraries(encodingSamuraiTests PRIVATE
    GTest::gmock
    GTest::gtest_main
    ${LIB_RACE_SDK_COMMON}
)

if(NOT EXISTS "${RACEBOAT_SOURCE_DIR}/test/common/MockEncodingSdk.h")
    message(FATAL_ERROR "Set RACEBOAT_SOURCE_DIR to the Raceboat checkout for MockEncodingSdk.h")
endif()
target_include_directories(encodingSamuraiTests PRIVATE
    "${RACEBOAT_SOURCE_DIR}/test/common"
    "${RACEBOAT_SOURCE_DIR}/include"
)

add_test(NAME encodingSamuraiTests COMMAND encodingSamuraiTests)