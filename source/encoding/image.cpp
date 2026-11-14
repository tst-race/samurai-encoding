#include "image.hpp"
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image.h"
#include "stb/stb_image_write.h"
#include "random.hpp"

#include <algorithm>
#include <cstdlib>

Image::Image() : width(0), height(0) {
}

bool Image::load(const std::string &path) {
    int x, y, n = 4;

    auto *buffer = stbi_load(path.c_str(), &x, &y, &n, n);
    if (!buffer)
        return false;

    image = std::make_unique<std::uint8_t[]>(x * y * 4);
    std::copy_n(buffer, x * y * 4, image.get());
    stbi_image_free(buffer);

    width  = x;
    height = y;

    return true;
}

bool Image::save(const std::string &path) {
    int result = stbi_write_png(path.c_str(), width, height, 4, image.get(), width * 4);
    return result != 0;
}

bool Image::load_from_memory(const std::uint8_t *data, std::size_t size) {
    int x, y, n = 4;

    auto *buffer = stbi_load_from_memory(data, static_cast<int>(size), &x, &y, &n, n);
    if (!buffer)
        return false;

    image = std::make_unique<std::uint8_t[]>(x * y * 4);
    std::copy_n(buffer, x * y * 4, image.get());
    stbi_image_free(buffer);

    width  = x;
    height = y;

    return true;
}

std::vector<std::uint8_t> Image::save_to_memory() const {
    int out_len = 0;
    auto *buffer = stbi_write_png_to_mem(
        image.get(), width * 4, width, height, 4, &out_len);
    if (!buffer || out_len <= 0) {
        return {};
    }

    std::vector<std::uint8_t> result(buffer, buffer + out_len);
    stbi_image_free(buffer);
    return result;
}

Image Image::create_carrier(unsigned int width, unsigned int height, bool noise) {
    Image img;
    img.width = width;
    img.height = height;
    img.image = std::make_unique<std::uint8_t[]>(width * height * 4);

    if (noise) {
        Random random;
        if (!random.get(img.image.get(), width * height * 4)) {
            // Fallback: deterministic pattern
            for (unsigned int i = 0; i < width * height * 4; ++i) {
                img.image[i] = static_cast<std::uint8_t>(i * 17 + 128);
            }
        }
    } else {
        // Solid gray (0x80) with full opacity (0xFF)
        std::fill_n(img.image.get(), width * height * 4, 0x80);
        // Set alpha channel to 0xFF
        for (unsigned int y = 0; y < height; ++y) {
            for (unsigned int x = 0; x < width; ++x) {
                img.image[(y * width + x) * 4 + 3] = 0xFF;
            }
        }
    }

    return img;
}

void Image::encode(const std::uint8_t *data, std::size_t size, EncodingLevel level, std::size_t offset) {
    auto image_ptr = this->image.get() + offset;

    if (level == EncodingLevel::Low) {
        for (std::size_t i = 0; i < size; ++i, image_ptr += 8) {
            image_ptr[0] = (image_ptr[0] & ~0b1) | ((data[i] >> 0) & 0b1);
            image_ptr[1] = (image_ptr[1] & ~0b1) | ((data[i] >> 1) & 0b1);
            image_ptr[2] = (image_ptr[2] & ~0b1) | ((data[i] >> 2) & 0b1);
            image_ptr[3] = (image_ptr[3] & ~0b1) | ((data[i] >> 3) & 0b1);
            image_ptr[4] = (image_ptr[4] & ~0b1) | ((data[i] >> 4) & 0b1);
            image_ptr[5] = (image_ptr[5] & ~0b1) | ((data[i] >> 5) & 0b1);
            image_ptr[6] = (image_ptr[6] & ~0b1) | ((data[i] >> 6) & 0b1);
            image_ptr[7] = (image_ptr[7] & ~0b1) | ((data[i] >> 7) & 0b1);
        }
    }
    else if (level == EncodingLevel::Med) {
        for (std::size_t i = 0; i < size; ++i, image_ptr += 4) {
            image_ptr[0] = (image_ptr[0] & ~0b11) | ((data[i] >> 0) & 0b11);
            image_ptr[1] = (image_ptr[1] & ~0b11) | ((data[i] >> 2) & 0b11);
            image_ptr[2] = (image_ptr[2] & ~0b11) | ((data[i] >> 4) & 0b11);
            image_ptr[3] = (image_ptr[3] & ~0b11) | ((data[i] >> 6) & 0b11);
        }
    }
    // High
    else {
        for (std::size_t i = 0; i < size / 2; ++i, image_ptr += 4, data += 2) {
            image_ptr[0] = (image_ptr[0] & ~0xf) | (data[0] & 0xf);
            image_ptr[1] = (image_ptr[1] & ~0xf) | (data[0] >> 4);
            image_ptr[2] = (image_ptr[2] & ~0xf) | (data[1] & 0xf);
            image_ptr[3] = (image_ptr[3] & ~0xf) | (data[1] >> 4);
        }

        if (size % 2) {
            image_ptr[0] = (image_ptr[0] & ~0xf) | (*data & 0xf);
            image_ptr[1] = (image_ptr[1] & ~0xf) | (*data >> 4);
        }
    }
}

std::unique_ptr<std::uint8_t[]> Image::decode(std::size_t size, EncodingLevel level, std::size_t offset) const {
    auto data  = std::make_unique<std::uint8_t[]>(size);
    auto image_ptr = this->image.get() + offset;

    if (level == EncodingLevel::Low) {
        for (std::size_t i = 0; i < size; ++i, image_ptr += 8) {
            data[i] = ((image_ptr[0] & 0b1) << 0) | ((image_ptr[1] & 0b1) << 1) |
                      ((image_ptr[2] & 0b1) << 2) | ((image_ptr[3] & 0b1) << 3) |
                      ((image_ptr[4] & 0b1) << 4) | ((image_ptr[5] & 0b1) << 5) |
                      ((image_ptr[6] & 0b1) << 6) | ((image_ptr[7] & 0b1) << 7);
        }
    }
    else if (level == EncodingLevel::Med) {
        for (std::size_t i = 0; i < size; ++i, image_ptr += 4) {
            data[i] = ((image_ptr[0] & 0b11) << 0) | ((image_ptr[1] & 0b11) << 2) |
                      ((image_ptr[2] & 0b11) << 4) | ((image_ptr[3] & 0b11) << 6);
        }
    }
    // High
    else {
        auto buffer = data.get();

        for (std::size_t i = 0; i < size / 2; ++i, image_ptr += 4, buffer += 2) {
            buffer[0] = (image_ptr[0] & 0xf) | (image_ptr[1] << 4);
            buffer[1] = (image_ptr[2] & 0xf) | (image_ptr[3] << 4);
        }

        if (size % 2)
            *buffer = (image_ptr[0] & 0xf) | (image_ptr[1] << 4);
    }

    return data;
}

std::size_t Image::encoded_size(std::size_t size, EncodingLevel level) {
    if (level == EncodingLevel::Low)
        return size * 8;
    else if (level == EncodingLevel::Med)
        return size * 4;
    // High
    return size * 2;
}