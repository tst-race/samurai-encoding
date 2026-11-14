#include "samurai_encoder.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>

std::optional<SamuraiEncoder::Level> SamuraiEncoder::parse_level(const std::string &s) {
    if (s == "low") return Level::Low;
    if (s == "med") return Level::Med;
    if (s == "high") return Level::High;
    return std::nullopt;
}

std::optional<SamuraiEncoder::CarrierType> SamuraiEncoder::parse_carrier_type(const std::string &s) {
    if (s == "noise") return CarrierType::Noise;
    if (s == "solid") return CarrierType::Solid;
    return std::nullopt;
}

SamuraiEncoder::CapacityInfo SamuraiEncoder::compute_capacity(const Parameters &params) {
    CapacityInfo info;
    info.carrier_pixels = static_cast<std::size_t>(params.carrier_width) * params.carrier_height;
    info.carrier_channels = info.carrier_pixels * 4;

    // Header overhead encoded at Low level (8 channels per byte)
    const std::size_t header_channels = HEADER_OVERHEAD_BYTES * 8; // 96 * 8 = 768

    if (info.carrier_channels <= header_channels) {
        info.max_payload_bytes = 0;
        return info;
    }

    std::size_t cpb = channels_per_byte(to_image_level(params.level));
    std::size_t available_channels = info.carrier_channels - header_channels;
    std::size_t padded_max = available_channels / cpb;

    // Conservative: max PKCS#7 padding is 16 bytes
    if (padded_max <= 16) {
        info.max_payload_bytes = 0;
    } else {
        info.max_payload_bytes = padded_max - 16;
    }

    return info;
}

void SamuraiEncoder::derive_key(const std::string &password, const std::uint8_t salt[16], std::uint8_t key[32]) {
    // Password -> SHA256 -> 32 bytes
    std::array<std::uint8_t, 32> password_hash;
    SHA256 sha;
    sha.update(password.data(), password.size());
    sha.finish();
    sha.get_hash(password_hash.data());

    // PBKDF2-HMAC-SHA256
    pbkdf2_hmac_sha256(password_hash.data(), password_hash.size(), salt, 16, key, 32, KEY_ROUNDS);
}

std::size_t SamuraiEncoder::pkcs7_pad_size(std::size_t data_size) {
    std::size_t padded = data_size + 1; // At least one byte of padding
    if (padded % 16 != 0)
        padded = (data_size / 16 + 1) * 16;
    return padded;
}

void SamuraiEncoder::pkcs7_pad(std::uint8_t *data, std::size_t data_size, std::size_t padded_size) {
    std::uint8_t pad_value = static_cast<std::uint8_t>(padded_size - data_size);
    std::fill_n(data + data_size, pad_value, pad_value);
}

std::size_t SamuraiEncoder::pkcs7_unpad_size(const std::uint8_t *data, std::size_t padded_size) {
    if (padded_size == 0) return 0;
    std::uint8_t pad_value = data[padded_size - 1];
    if (pad_value == 0 || pad_value > 16) return SIZE_MAX; // Invalid padding

    // Verify all padding bytes have the same value
    for (std::size_t i = padded_size - pad_value; i < padded_size; ++i) {
        if (data[i] != pad_value) return SIZE_MAX;
    }
    return padded_size - pad_value;
}

std::size_t SamuraiEncoder::channels_per_byte(Image::EncodingLevel level) {
    switch (level) {
        case Image::EncodingLevel::Low:  return 8;
        case Image::EncodingLevel::Med:  return 4;
        case Image::EncodingLevel::High: return 2;
    }
    return 8;
}

Image::EncodingLevel SamuraiEncoder::to_image_level(Level level) {
    switch (level) {
        case Level::Low:  return Image::EncodingLevel::Low;
        case Level::Med:  return Image::EncodingLevel::Med;
        case Level::High: return Image::EncodingLevel::High;
    }
    return Image::EncodingLevel::Low;
}

SamuraiEncoder::EncodeResult SamuraiEncoder::encode(const std::vector<std::uint8_t> &payload, const Parameters &params) {
    EncodeResult result;
    result.success = false;

    // Validate password
    if (params.password.empty()) {
        result.error_message = "Password is required";
        return result;
    }

    // Validate carrier dimensions
    if (params.carrier_width < 16 || params.carrier_width > 4096 ||
        params.carrier_height < 16 || params.carrier_height > 4096) {
        result.error_message = "Carrier dimensions must be between 16 and 4096";
        return result;
    }

    // Check capacity
    CapacityInfo cap = compute_capacity(params);
    if (payload.size() > cap.max_payload_bytes) {
        result.error_message = "Payload too large: " + std::to_string(payload.size()) +
                               " bytes, max is " + std::to_string(cap.max_payload_bytes);
        return result;
    }

    // Generate carrier image
    Image carrier = Image::create_carrier(params.carrier_width, params.carrier_height,
                                          params.carrier_type == CarrierType::Noise);

    // Calculate padded size
    std::size_t padded_size = pkcs7_pad_size(payload.size());

    // Prepare padded payload
    std::vector<std::uint8_t> padded_data(padded_size);
    if (!payload.empty()) {
        std::copy(payload.begin(), payload.end(), padded_data.begin());
    }
    pkcs7_pad(padded_data.data(), payload.size(), padded_size);

    // Compute CRC32 of unpadded payload
    CRC32 crc;
    crc.update(payload.data(), payload.size());
    std::uint32_t crc_hash = crc.get_hash();

    // Generate random salt and IV
    Random random;
    std::uint8_t salt[16], iv[16];
    if (!random.get(salt, 16) || !random.get(iv, 16)) {
        result.error_message = "Failed to generate random salt/IV";
        return result;
    }

    // Generate random offset for payload (after header)
    std::uint32_t offset;
    if (!random.get(&offset, sizeof(offset))) {
        result.error_message = "Failed to generate random offset";
        return result;
    }

    // Header overhead in channels (at Low level)
    const std::size_t header_channels = HEADER_OVERHEAD_BYTES * 8; // 768
    const std::size_t payload_channels = padded_size * channels_per_byte(to_image_level(params.level));
    const std::size_t total_channels = carrier.data_size();
    const std::size_t max_payload_channels = total_channels - header_channels;

    if (payload_channels > max_payload_channels) {
        result.error_message = "Payload too large for carrier after offset calculation";
        return result;
    }

    // Ensure offset is aligned to channel boundary for the payload level
    std::size_t cpb = channels_per_byte(to_image_level(params.level));
    std::size_t range = max_payload_channels - payload_channels + 1;
    if (range == 0) {
        result.error_message = "Payload too large for carrier (no space for offset)";
        return result;
    }
    if (range == 1) {
        offset = header_channels;
    } else {
        offset = header_channels + (offset % range);
    }
    // Align offset to cpb boundary
    offset = (offset / cpb) * cpb;

    // Build header
    Header header;
    header.sig[0] = 'H'; header.sig[1] = 'I'; header.sig[2] = 'D'; header.sig[3] = 'E';
    header.version = VERSION;
    header.level = static_cast<std::uint8_t>(params.level);
    header.flags = 0;
    header.offset = offset;
    header.size = static_cast<std::uint32_t>(padded_size);
    header.hash = crc_hash;

    const char *default_name = "payload.bin";
    std::size_t name_len = std::min<std::size_t>(strlen(default_name), sizeof(header.name));
    std::copy_n(default_name, name_len, header.name);
    std::fill_n(header.name + name_len, sizeof(header.name) - name_len, 0);
    std::fill_n(header.reserved, sizeof(header.reserved), 0);

    // Derive key
    std::uint8_t key[32];
    derive_key(params.password, salt, key);

    // Encrypt header and payload
    AES aes(key, iv);

    std::vector<std::uint8_t> encrypted_header(sizeof(Header));
    aes.cbc_encrypt(&header, sizeof(Header), encrypted_header.data());

    std::vector<std::uint8_t> encrypted_payload(padded_size);
    aes.cbc_encrypt(padded_data.data(), padded_size, encrypted_payload.data());

    // Embed into carrier (all at Low level except payload at selected level)
    carrier.encode(salt, 16, Image::EncodingLevel::Low);
    carrier.encode(iv, 16, Image::EncodingLevel::Low, Image::encoded_size(16, Image::EncodingLevel::Low));
    carrier.encode(encrypted_header.data(), sizeof(Header), Image::EncodingLevel::Low,
                   Image::encoded_size(32, Image::EncodingLevel::Low));
    carrier.encode(encrypted_payload.data(), padded_size, to_image_level(params.level), offset);

    // Save to memory
    result.png_bytes = carrier.save_to_memory();
    if (result.png_bytes.empty()) {
        result.error_message = "Failed to encode PNG to memory";
        return result;
    }

    result.success = true;
    return result;
}

SamuraiEncoder::DecodeResult SamuraiEncoder::decode(const std::vector<std::uint8_t> &png_bytes, const Parameters &params) {
    DecodeResult result;
    result.success = false;

    // Validate password
    if (params.password.empty()) {
        result.error_message = "Password is required";
        return result;
    }

    // Load PNG from memory
    Image image;
    if (!image.load_from_memory(png_bytes.data(), png_bytes.size())) {
        result.error_message = "Failed to load PNG from memory (invalid/corrupt PNG)";
        return result;
    }

    // Verify 4 channels
    if (image.data_size() != image.w() * image.h() * 4) {
        result.error_message = "Unexpected image channel count (expected 4/RGBA)";
        return result;
    }

    // Extract salt (16 bytes at Low level, offset 0)
    auto salt_data = image.decode(16, Image::EncodingLevel::Low);
    std::uint8_t salt[16];
    std::copy_n(salt_data.get(), 16, salt);

    // Extract IV (16 bytes at Low level, offset 16*8=128 channels)
    auto iv_data = image.decode(16, Image::EncodingLevel::Low,
                                 Image::encoded_size(16, Image::EncodingLevel::Low));
    std::uint8_t iv[16];
    std::copy_n(iv_data.get(), 16, iv);

    // Derive key
    std::uint8_t key[32];
    derive_key(params.password, salt, key);

    // Extract encrypted header (64 bytes at Low level, offset 32*8=256 channels)
    auto encrypted_header_data = image.decode(sizeof(Header), Image::EncodingLevel::Low,
                                               Image::encoded_size(32, Image::EncodingLevel::Low));

    // Decrypt header
    AES aes(key, iv);
    Header header;
    aes.cbc_decrypt(encrypted_header_data.get(), sizeof(Header), &header);

    // Verify header
    if (header.sig[0] != 'H' || header.sig[1] != 'I' || header.sig[2] != 'D' || header.sig[3] != 'E') {
        result.error_message = "Invalid signature (wrong password or corrupt data)";
        return result;
    }
    if (header.version != VERSION) {
        result.error_message = "Unsupported version: " + std::to_string(header.version);
        return result;
    }
    for (std::uint8_t r : header.reserved) {
        if (r != 0) {
            result.error_message = "Invalid reserved bytes in header";
            return result;
        }
    }

    // Get payload level and parameters
    Image::EncodingLevel payload_level = static_cast<Image::EncodingLevel>(header.level);
    std::uint32_t offset = header.offset;
    std::uint32_t padded_size = header.size;

    // Extract encrypted payload
    auto encrypted_payload_data = image.decode(padded_size, payload_level, offset);

    // Decrypt payload
    std::vector<std::uint8_t> padded_data(padded_size);
    aes.cbc_decrypt(encrypted_payload_data.get(), padded_size, padded_data.data());

    // Unpad
    std::size_t payload_size = pkcs7_unpad_size(padded_data.data(), padded_size);
    if (payload_size == SIZE_MAX) {
        result.error_message = "Invalid PKCS#7 padding";
        return result;
    }

    // Verify CRC32
    CRC32 crc;
    crc.update(padded_data.data(), payload_size);
    if (crc.get_hash() != header.hash) {
        result.error_message = "CRC32 checksum mismatch (wrong password or corrupt data)";
        return result;
    }

    // Return payload
    result.payload.assign(padded_data.begin(), padded_data.begin() + payload_size);
    result.success = true;
    return result;
}