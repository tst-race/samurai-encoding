#pragma once

// SamuraiEncoder - Adapted from 7thSamurai/steganography (MIT License)
// Original source: https://github.com/7thSamurai/steganography
// Copyright (c) 2023 7thSamurai
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <cstdint>
#include <vector>
#include <string>
#include <array>
#include <memory>
#include <optional>

#include "image.hpp"
#include "aes.hpp"
#include "sha256.hpp"
#include "crc32.hpp"
#include "random.hpp"

class SamuraiEncoder
{
public:
    static constexpr std::size_t KEY_ROUNDS = 20000;
    static constexpr std::size_t HEADER_SIZE = 64;
    static constexpr std::size_t SALT_SIZE = 16;
    static constexpr std::size_t IV_SIZE = 16;
    static constexpr std::size_t HEADER_OVERHEAD_BYTES = HEADER_SIZE + SALT_SIZE + IV_SIZE; // 96 bytes

    enum class Level {
        Low  = 0,
        Med  = 1,
        High = 2,
    };

    enum class CarrierType {
        Noise,
        Solid,
    };

    struct Parameters {
        std::string password;           // Required
        Level level = Level::Low;       // Optional, default Low
        unsigned int carrier_width = 512;   // Optional, default 512
        unsigned int carrier_height = 512;  // Optional, default 512
        CarrierType carrier_type = CarrierType::Noise; // Optional, default Noise
        bool save_encoded_output = false;   // Optional, default false
    };

    struct EncodeResult {
        std::vector<std::uint8_t> png_bytes;
        bool success;
        std::string error_message;
    };

    struct DecodeResult {
        std::vector<std::uint8_t> payload;
        bool success;
        std::string error_message;
    };

    struct CapacityInfo {
        std::size_t max_payload_bytes;
        std::size_t carrier_pixels;
        std::size_t carrier_channels;
    };

    // Convert string to Level enum
    static std::optional<Level> parse_level(const std::string &s);
    static std::optional<CarrierType> parse_carrier_type(const std::string &s);

    // Compute max payload bytes for given parameters
    static CapacityInfo compute_capacity(const Parameters &params);

    // Encode payload to PNG (in-memory)
    static EncodeResult encode(const std::vector<std::uint8_t> &payload, const Parameters &params);

    // Decode PNG to payload (in-memory)
    static DecodeResult decode(const std::vector<std::uint8_t> &png_bytes, const Parameters &params);

private:
    // Header structure (64 bytes)
    struct Header {
        std::uint8_t  sig[4];       // "HIDE"
        std::uint16_t version;      // 1
        std::uint8_t  level;        // 0=Low, 1=Med, 2=High
        std::uint8_t  flags;        // 0
        std::uint32_t offset;       // Offset to payload data
        std::uint32_t size;         // Padded payload size
        std::uint32_t hash;         // CRC32 of unpadded payload
        std::uint8_t  name[32];     // "payload.bin" + zeros
        std::uint8_t  reserved[12]; // Zeros
    };

    static constexpr std::uint16_t VERSION = 1;

    // Derive AES key from password + salt
    static void derive_key(const std::string &password, const std::uint8_t salt[16], std::uint8_t key[32]);

    // PKCS#7 padding
    static std::size_t pkcs7_pad_size(std::size_t data_size);
    static void pkcs7_pad(std::uint8_t *data, std::size_t data_size, std::size_t padded_size);
    static std::size_t pkcs7_unpad_size(const std::uint8_t *data, std::size_t padded_size);

    // Image level to encoded_size channels per byte
    static std::size_t channels_per_byte(Image::EncodingLevel level);
    static Image::EncodingLevel to_image_level(Level level);
};