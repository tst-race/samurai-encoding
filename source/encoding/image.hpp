#pragma once

// Adapted from 7thSamurai/steganography (MIT License)
// Original source: https://github.com/7thSamurai/steganography
// Copyright (c) 2023 7thSamurai
// See samurai_encoder.hpp for full license text.

#include <cstdint>
#include <string>
#include <memory>
#include <vector>

class Image
{
public:
    enum class EncodingLevel {
        Low  = 0,
        Med  = 1,
        High = 2,
    };

    Image();

    // File-based API (original)
    bool load(const std::string &path);
    bool save(const std::string &path);

    // In-memory API (added for PoC)
    bool load_from_memory(const std::uint8_t *data, std::size_t size);
    std::vector<std::uint8_t> save_to_memory() const;

    // Get raw pixel data (RGBA, width * height * 4 bytes)
    const std::uint8_t* data() const { return image.get(); }
    std::size_t data_size() const { return static_cast<std::size_t>(width) * height * 4; }
    unsigned int w() const { return width; }
    unsigned int h() const { return height; }

    // Create a new synthetic carrier image
    static Image create_carrier(unsigned int width, unsigned int height, bool noise = true);

    // LSB encoding/decoding
    void encode(const std::uint8_t *data, std::size_t size, EncodingLevel level, std::size_t offset = 0);
    std::unique_ptr<std::uint8_t[]> decode(std::size_t size, EncodingLevel level, std::size_t offset = 0) const;

    static std::size_t encoded_size(std::size_t size, EncodingLevel level);

private:
    std::unique_ptr<std::uint8_t[]> image;
    unsigned int width, height;
};