#include "PluginSamuraiEncoding.h"

#include <MockEncodingSdk.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>
#include <vector>

using ::testing::_;
using ::testing::DoAll;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;

namespace {
ChannelResponse sdkResponse() { return {CM_OK, NULL_RACE_HANDLE}; }

// Test parameters with valid password
const EncodingParameters paramsWithPassword{
    "test-link", "image/png", true,
    R"({"password": "test_password_123", "level": "low", "carrier_width": 512, "carrier_height": 512, "carrier_type": "noise"})"};

// Test parameters with empty password (should fail)
const EncodingParameters paramsNoPassword{
    "test-link", "image/png", true,
    R"({"password": "", "level": "low", "carrier_width": 512, "carrier_height": 512, "carrier_type": "noise"})"};

class PluginSamuraiEncodingTest : public ::testing::Test {
protected:
    void SetUp() override {
        ON_CALL(sdk, updateState(_)).WillByDefault(Return(sdkResponse()));
        ON_CALL(sdk, onBytesEncoded(_, _, _)).WillByDefault(Return(sdkResponse()));
        ON_CALL(sdk, onBytesDecoded(_, _, _)).WillByDefault(Return(sdkResponse()));
    }

    NiceMock<MockEncodingSdk> sdk;
};
} // namespace

TEST_F(PluginSamuraiEncodingTest, startsAndReportsConfiguredProperties) {
    EXPECT_CALL(sdk, updateState(COMPONENT_STATE_STARTED)).Times(1);
    PluginSamuraiEncoding encoding(&sdk);

    const auto properties = encoding.getEncodingProperties();
    EXPECT_EQ("image/png", properties.type);
    EXPECT_GT(properties.encodingTime, 0.0);
    // Max bytes for default low-level 512x512 carrier
    EXPECT_EQ(130960, encoding.getEncodingPropertiesForParameters(paramsWithPassword).maxBytes);
}

TEST_F(PluginSamuraiEncodingTest, roundTripsEmptyPayload) {
    PluginSamuraiEncoding encoding(&sdk);
    const std::vector<uint8_t> empty;
    std::vector<uint8_t> emptyEncoded{1};

    // Encode empty payload - should produce valid PNG
    EXPECT_CALL(sdk, onBytesEncoded(11, _, ENCODE_OK))
        .WillOnce(DoAll(SaveArg<1>(&emptyEncoded), Return(sdkResponse())));
    EXPECT_EQ(COMPONENT_OK, encoding.encodeBytes(11, paramsWithPassword, empty));

    // Verify encoded output is not empty (valid PNG)
    EXPECT_FALSE(emptyEncoded.empty());
    // Verify PNG signature (89 50 4E 47 0D 0A 1A 0A)
    EXPECT_GE(emptyEncoded.size(), 8);
    EXPECT_EQ(0x89, emptyEncoded[0]);
    EXPECT_EQ(0x50, emptyEncoded[1]);
    EXPECT_EQ(0x4E, emptyEncoded[2]);
    EXPECT_EQ(0x47, emptyEncoded[3]);

    // Decode the empty payload carrier - should return exactly 0 bytes
    std::vector<uint8_t> emptyDecoded{1};
    EXPECT_CALL(sdk, onBytesDecoded(12, _, ENCODE_OK))
        .WillOnce(DoAll(SaveArg<1>(&emptyDecoded), Return(sdkResponse())));
    EXPECT_EQ(COMPONENT_OK, encoding.decodeBytes(12, paramsWithPassword, emptyEncoded));
    EXPECT_TRUE(emptyDecoded.empty());
}

TEST_F(PluginSamuraiEncodingTest, roundTripsAllByteValues) {
    PluginSamuraiEncoding encoding(&sdk);

    std::vector<uint8_t> input;
    for (unsigned int value = 0; value < 256; ++value) {
        input.push_back(static_cast<uint8_t>(value));
    }

    std::vector<uint8_t> encoded;
    EXPECT_CALL(sdk, onBytesEncoded(13, _, ENCODE_OK))
        .WillOnce(DoAll(SaveArg<1>(&encoded), Return(sdkResponse())));
    EXPECT_EQ(COMPONENT_OK, encoding.encodeBytes(13, paramsWithPassword, input));

    // Verify encoded output is valid PNG
    EXPECT_FALSE(encoded.empty());
    EXPECT_EQ(0x89, encoded[0]);
    EXPECT_EQ(0x50, encoded[1]);
    EXPECT_EQ(0x4E, encoded[2]);
    EXPECT_EQ(0x47, encoded[3]);

    std::vector<uint8_t> decoded;
    EXPECT_CALL(sdk, onBytesDecoded(14, _, ENCODE_OK))
        .WillOnce(DoAll(SaveArg<1>(&decoded), Return(sdkResponse())));
    EXPECT_EQ(COMPONENT_OK, encoding.decodeBytes(14, paramsWithPassword, encoded));
    EXPECT_EQ(input, decoded);
}

TEST_F(PluginSamuraiEncodingTest, roundTripsRandomPayload) {
    PluginSamuraiEncoding encoding(&sdk);

    // 1 KB random payload
    std::vector<uint8_t> input(1024, 0xA5);
    for (size_t i = 0; i < input.size(); ++i) {
        input[i] = static_cast<uint8_t>(i * 17 + 42);
    }

    std::vector<uint8_t> encoded;
    EXPECT_CALL(sdk, onBytesEncoded(15, _, ENCODE_OK))
        .WillOnce(DoAll(SaveArg<1>(&encoded), Return(sdkResponse())));
    EXPECT_EQ(COMPONENT_OK, encoding.encodeBytes(15, paramsWithPassword, input));

    std::vector<uint8_t> decoded;
    EXPECT_CALL(sdk, onBytesDecoded(16, _, ENCODE_OK))
        .WillOnce(DoAll(SaveArg<1>(&decoded), Return(sdkResponse())));
    EXPECT_EQ(COMPONENT_OK, encoding.decodeBytes(16, paramsWithPassword, encoded));
    EXPECT_EQ(input, decoded);
}

TEST_F(PluginSamuraiEncodingTest, acceptsDeclaredMaximumAndRejectsLargerInput) {
    PluginSamuraiEncoding encoding(&sdk);

    // Max payload for Low level 512x512 = 130960 bytes
    const std::vector<uint8_t> maximum(130960, 0xA5);
    std::vector<uint8_t> encoded;
    EXPECT_CALL(sdk, onBytesEncoded(21, _, ENCODE_OK))
        .WillOnce(DoAll(SaveArg<1>(&encoded), Return(sdkResponse())));
    EXPECT_EQ(COMPONENT_OK, encoding.encodeBytes(21, paramsWithPassword, maximum));

    // Verify encoded output is valid PNG
    EXPECT_FALSE(encoded.empty());
    EXPECT_EQ(0x89, encoded[0]);

    // Max + 1 byte should be rejected
    const std::vector<uint8_t> tooLarge(130961, 0);
    EXPECT_CALL(sdk, onBytesEncoded(22, _, ENCODE_FAILED)).Times(1);
    EXPECT_EQ(COMPONENT_ERROR, encoding.encodeBytes(22, paramsWithPassword, tooLarge));
}

TEST_F(PluginSamuraiEncodingTest, rejectsEmptyPassword) {
    PluginSamuraiEncoding encoding(&sdk);

    const std::vector<uint8_t> input{0x01, 0x02, 0x03};

    // Should fail with empty password
    EXPECT_CALL(sdk, onBytesEncoded(31, _, ENCODE_FAILED)).Times(1);
    EXPECT_EQ(COMPONENT_ERROR, encoding.encodeBytes(31, paramsNoPassword, input));
}

TEST_F(PluginSamuraiEncodingTest, rejectsWrongPasswordOnDecode) {
    PluginSamuraiEncoding encoding(&sdk);

    const std::vector<uint8_t> input{0x01, 0x02, 0x03};

    // Encode with correct password
    std::vector<uint8_t> encoded;
    EXPECT_CALL(sdk, onBytesEncoded(41, _, ENCODE_OK))
        .WillOnce(DoAll(SaveArg<1>(&encoded), Return(sdkResponse())));
    EXPECT_EQ(COMPONENT_OK, encoding.encodeBytes(41, paramsWithPassword, input));

    // Decode with wrong password - should fail
    const EncodingParameters paramsWrongPassword{
        "test-link", "image/png", true,
        R"({"password": "wrong_password", "level": "low", "carrier_width": 512, "carrier_height": 512, "carrier_type": "noise"})"};

    EXPECT_CALL(sdk, onBytesDecoded(42, _, ENCODE_FAILED)).Times(1);
    EXPECT_EQ(COMPONENT_ERROR, encoding.decodeBytes(42, paramsWrongPassword, encoded));
}

TEST_F(PluginSamuraiEncodingTest, malformedInputCallsFailureCallback) {
    PluginSamuraiEncoding encoding(&sdk);

    // Invalid PNG data
    const std::vector<uint8_t> malformed{'!', '!', '!', '!'};
    EXPECT_CALL(sdk, onBytesDecoded(51, _, ENCODE_FAILED)).Times(1);
    EXPECT_EQ(COMPONENT_ERROR, encoding.decodeBytes(51, paramsWithPassword, malformed));
}

TEST_F(PluginSamuraiEncodingTest, getEncodingPropertiesForParametersRespectsLevel) {
    PluginSamuraiEncoding encoding(&sdk);

    // Low level
    const EncodingParameters paramsLow{
        "test-link", "image/png", true,
        R"({"password": "test", "level": "low", "carrier_width": 512, "carrier_height": 512, "carrier_type": "noise"})"};
    EXPECT_EQ(130960, encoding.getEncodingPropertiesForParameters(paramsLow).maxBytes);

    // Med level
    const EncodingParameters paramsMed{
        "test-link", "image/png", true,
        R"({"password": "test", "level": "med", "carrier_width": 512, "carrier_height": 512, "carrier_type": "noise"})"};
    EXPECT_EQ(261936, encoding.getEncodingPropertiesForParameters(paramsMed).maxBytes);

    // High level
    const EncodingParameters paramsHigh{
        "test-link", "image/png", true,
        R"({"password": "test", "level": "high", "carrier_width": 512, "carrier_height": 512, "carrier_type": "noise"})"};
    EXPECT_EQ(523888, encoding.getEncodingPropertiesForParameters(paramsHigh).maxBytes);
}

TEST_F(PluginSamuraiEncodingTest, getEncodingPropertiesForParametersRespectsCarrierSize) {
    PluginSamuraiEncoding encoding(&sdk);

    // 256x256 carrier at Low level
    const EncodingParameters paramsSmall{
        "test-link", "image/png", true,
        R"({"password": "test", "level": "low", "carrier_width": 256, "carrier_height": 256, "carrier_type": "noise"})"};
    EXPECT_EQ(32656, encoding.getEncodingPropertiesForParameters(paramsSmall).maxBytes);

    // 1024x1024 carrier at Low level
    const EncodingParameters paramsLarge{
        "test-link", "image/png", true,
        R"({"password": "test", "level": "low", "carrier_width": 1024, "carrier_height": 1024, "carrier_type": "noise"})"};
    EXPECT_EQ(524176, encoding.getEncodingPropertiesForParameters(paramsLarge).maxBytes);
}