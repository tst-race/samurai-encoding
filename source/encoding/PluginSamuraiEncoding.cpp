#include "PluginSamuraiEncoding.h"

#include "samurai_encoder.hpp"
#include <RaceLog.h>

#include <nlohmann/json.hpp>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <fstream>
#include <chrono>

const char *PluginSamuraiEncoding::name = "samurai";

namespace {
constexpr int32_t kMaxBytesLow = 130960;
constexpr int32_t kMaxBytesMed = 261936;
constexpr int32_t kMaxBytesHigh = 523888;
constexpr double kEncodingTimeSeconds = 1.0;
constexpr char kMimeType[] = "image/png";

// Helper to get environment variable (returns empty string if not set)
std::string getEnv(const char *name) {
    const char *value = std::getenv(name);
    if (value && value[0] != '\0') {
        return std::string(value);
    }
    return "";
}

// Helper to parse JSON parameters from EncodingParameters::json
// Priority: JSON (explicit framework params) > Environment variables > Hardcoded defaults
SamuraiEncoder::Parameters parseParameters(const std::string &jsonStr,
                                           const std::string &password,
                                           const std::string &level,
                                           const std::string &carrier_width,
                                           const std::string &carrier_height,
                                           const std::string &carrier_type,
                                           const std::string &save_encoded_output) {
    SamuraiEncoder::Parameters params;
    
    // Start with hardcoded defaults
    params.level = SamuraiEncoder::Level::Low;
    params.carrier_width = 512;
    params.carrier_height = 512;
    params.carrier_type = SamuraiEncoder::CarrierType::Noise;
    params.save_encoded_output = false;

    // Parse from JSON (from EncodingParameters.json field) - highest priority
    bool hasJsonPassword = false;
    bool hasJsonLevel = false;
    bool hasJsonCarrierWidth = false;
    bool hasJsonCarrierHeight = false;
    bool hasJsonCarrierType = false;
    bool hasJsonSaveEncodedOutput = false;
    
    if (!jsonStr.empty() && jsonStr != "{}") {
        try {
            auto j = nlohmann::json::parse(jsonStr);
            if (j.contains("password") && j["password"].is_string()) {
                params.password = j["password"].get<std::string>();
                hasJsonPassword = true;
            }
            if (j.contains("level") && j["level"].is_string()) {
                auto levelOpt = SamuraiEncoder::parse_level(j["level"].get<std::string>());
                if (levelOpt) {
                    params.level = *levelOpt;
                    hasJsonLevel = true;
                }
            }
            if (j.contains("carrier_width") && j["carrier_width"].is_number_integer()) {
                params.carrier_width = j["carrier_width"].get<unsigned int>();
                hasJsonCarrierWidth = true;
            }
            if (j.contains("carrier_height") && j["carrier_height"].is_number_integer()) {
                params.carrier_height = j["carrier_height"].get<unsigned int>();
                hasJsonCarrierHeight = true;
            }
            if (j.contains("carrier_type") && j["carrier_type"].is_string()) {
                auto typeOpt = SamuraiEncoder::parse_carrier_type(j["carrier_type"].get<std::string>());
                if (typeOpt) {
                    params.carrier_type = *typeOpt;
                    hasJsonCarrierType = true;
                }
            }
            if (j.contains("save-encoded-output") && j["save-encoded-output"].is_boolean()) {
                params.save_encoded_output = j["save-encoded-output"].get<bool>();
                hasJsonSaveEncodedOutput = true;
            }
        } catch (const nlohmann::json::exception &) {
            // Invalid JSON, fall through to environment variables
        }
    }

    // Apply environment variables only for parameters not set by JSON
    if (!hasJsonPassword && !password.empty()) {
        params.password = password;
    }
    if (!hasJsonLevel && !level.empty()) {
        auto levelOpt = SamuraiEncoder::parse_level(level);
        if (levelOpt) params.level = *levelOpt;
    }
    if (!hasJsonCarrierWidth && !carrier_width.empty()) {
        params.carrier_width = static_cast<unsigned int>(std::stoul(carrier_width));
    }
    if (!hasJsonCarrierHeight && !carrier_height.empty()) {
        params.carrier_height = static_cast<unsigned int>(std::stoul(carrier_height));
    }
    if (!hasJsonCarrierType && !carrier_type.empty()) {
        auto typeOpt = SamuraiEncoder::parse_carrier_type(carrier_type);
        if (typeOpt) params.carrier_type = *typeOpt;
    }
    if (!hasJsonSaveEncodedOutput && !save_encoded_output.empty()) {
        params.save_encoded_output = (save_encoded_output == "true");
    }

    // Ensure password is set (required)
    if (params.password.empty() && !password.empty()) {
        params.password = password;
    }

    return params;
}

// Compute max bytes based on parameters
int32_t computeMaxBytes(const SamuraiEncoder::Parameters &params) {
    auto cap = SamuraiEncoder::compute_capacity(params);
    return static_cast<int32_t>(cap.max_payload_bytes);
}

// Get all encoding parameters from environment
SamuraiEncoder::Parameters getEncodingParams(const std::string &jsonStr = "") {
    return parseParameters(
        jsonStr,
        getEnv("SAMURAI_PASSWORD"),
        getEnv("SAMURAI_LEVEL"),
        getEnv("SAMURAI_CARRIER_WIDTH"),
        getEnv("SAMURAI_CARRIER_HEIGHT"),
        getEnv("SAMURAI_CARRIER_TYPE"),
        getEnv("SAMURAI_SAVE_ENCODED_OUTPUT")
    );
}
} // namespace

PluginSamuraiEncoding::PluginSamuraiEncoding(IEncodingSdk *sdk) : sdk(sdk) {
    if (sdk == nullptr) {
        throw std::invalid_argument("PluginSamuraiEncoding: sdk parameter is null");
    }
    sdk->updateState(COMPONENT_STATE_STARTED);
}

ComponentStatus PluginSamuraiEncoding::onUserInputReceived(RaceHandle, bool,
                                                        const std::string &) {
    return COMPONENT_OK;
}

EncodingProperties PluginSamuraiEncoding::getEncodingProperties() {
    return {kEncodingTimeSeconds, kMimeType};
}

SpecificEncodingProperties PluginSamuraiEncoding::getEncodingPropertiesForParameters(
    const EncodingParameters &params) {
    auto samuraiParams = getEncodingParams(params.json);
    return {computeMaxBytes(samuraiParams)};
}

ComponentStatus PluginSamuraiEncoding::encodeBytes(RaceHandle handle,
                                                 const EncodingParameters &params,
                                                 const std::vector<uint8_t> &bytes) {
    auto samuraiParams = getEncodingParams(params.json);

    // Validate password
    if (samuraiParams.password.empty()) {
        sdk->onBytesEncoded(handle, {}, ENCODE_FAILED);
        return COMPONENT_ERROR;
    }

    // Check capacity
    int32_t maxBytes = computeMaxBytes(samuraiParams);
    if (bytes.size() > static_cast<size_t>(maxBytes)) {
        sdk->onBytesEncoded(handle, {}, ENCODE_FAILED);
        return COMPONENT_ERROR;
    }

    // Encode using Samurai steganography
    auto result = SamuraiEncoder::encode(bytes, samuraiParams);
    if (!result.success) {
        sdk->onBytesEncoded(handle, {}, ENCODE_FAILED);
        return COMPONENT_ERROR;
    }

    // Save encoded output if requested
    if (samuraiParams.save_encoded_output) {
        // Use a timestamp-based filename to avoid collisions
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        std::string filename = "/tmp/samurai_encoded_" + std::to_string(timestamp) + ".png";
        
        std::ofstream out(filename, std::ios::binary);
        if (out) {
            out.write(reinterpret_cast<const char*>(result.png_bytes.data()), result.png_bytes.size());
            out.close();
            RaceLog::logDebug("PluginSamuraiEncoding",
                              "Saved encoded output to " + filename + " (" + std::to_string(result.png_bytes.size()) + " bytes)",
                              "");
        } else {
            RaceLog::logWarning("PluginSamuraiEncoding",
                                "Failed to save encoded output to " + filename,
                                "");
        }
    }

    // Return the PNG bytes as encoded output
    sdk->onBytesEncoded(handle, result.png_bytes, ENCODE_OK);
    return COMPONENT_OK;
}

ComponentStatus PluginSamuraiEncoding::decodeBytes(RaceHandle handle,
                                                 const EncodingParameters &params,
                                                 const std::vector<uint8_t> &bytes) {
    auto samuraiParams = getEncodingParams(params.json);

    // Validate password
    if (samuraiParams.password.empty()) {
        sdk->onBytesDecoded(handle, {}, ENCODE_FAILED);
        return COMPONENT_ERROR;
    }

    // Decode using Samurai steganography
    auto result = SamuraiEncoder::decode(bytes, samuraiParams);
    if (!result.success) {
        sdk->onBytesDecoded(handle, {}, ENCODE_FAILED);
        return COMPONENT_ERROR;
    }

    // Check decoded payload size
    int32_t maxBytes = computeMaxBytes(samuraiParams);
    if (result.payload.size() > static_cast<size_t>(maxBytes)) {
        sdk->onBytesDecoded(handle, {}, ENCODE_FAILED);
        return COMPONENT_ERROR;
    }

    sdk->onBytesDecoded(handle, result.payload, ENCODE_OK);
    return COMPONENT_OK;
}

IEncodingComponent *createEncoding(const std::string &encoding, IEncodingSdk *sdk,
                                   const std::string &, const PluginConfig &) {
    if (encoding != PluginSamuraiEncoding::name || sdk == nullptr) {
        return nullptr;
    }
    return new PluginSamuraiEncoding(sdk);
}

void destroyEncoding(IEncodingComponent *component) {
    delete component;
}

const RaceVersionInfo raceVersion = RACE_VERSION;