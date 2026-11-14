#pragma once

// Adapted from 7thSamurai/steganography (MIT License)
// Original source: https://github.com/7thSamurai/steganography
// Copyright (c) 2023 7thSamurai
// See samurai_encoder.hpp for full license text.

#include <cstdint>
#include <cstddef>

class CRC32
{
public:
    CRC32();

    void update(const void *data, std::size_t size);
    std::uint32_t get_hash() const;

private:
    std::uint32_t hash;
};
