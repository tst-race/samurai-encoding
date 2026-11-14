# Third-Party Notices for samurai-encoding

This plugin incorporates code from the following third-party sources. All licenses are preserved in their respective source files.

## 1. 7thSamurai/steganography
- **Source URL**: https://github.com/7thSamurai/steganography
- **License**: MIT License
- **Version/Commit**: Used as seed reference; adapted source files included in this plugin
- **Local Files**:
  - `source/encoding/samurai_encoder.cpp` (adapted from seed repo's encoder)
  - `source/encoding/samurai_encoder.hpp` (adapted from seed repo's encoder)
  - `source/encoding/image.cpp` (adapted from seed repo's image handling)
  - `source/encoding/image.hpp` (adapted from seed repo's image handling)
  - `source/encoding/aes.cpp` (adapted from seed repo's AES implementation)
  - `source/encoding/aes.hpp` (adapted from seed repo's AES implementation)
  - `source/encoding/sha256.cpp` (adapted from seed repo's SHA-256 implementation)
  - `source/encoding/sha256.hpp` (adapted from seed repo's SHA-256 implementation)
  - `source/encoding/crc32.cpp` (adapted from seed repo's CRC32 implementation)
  - `source/encoding/crc32.hpp` (adapted from seed repo's CRC32 implementation)
  - `source/encoding/random.hpp` (adapted from seed repo's random number generation)
- **Modifications**:
  - Refactored into a single-header Parameters struct and static encode/decode methods
  - Added carrier type selection (noise/solid) and level parameter (low/med/high)
  - Added capacity computation API
  - Removed file I/O; all operations are in-memory
  - Added C++17 compatibility fixes
  - Integrated with raceboat IEncodingComponent API

**MIT License Text**:
```
MIT License

Copyright (c) 2023 7thSamurai

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## 2. stb_image / stb_image_write
- **Source URL**: https://github.com/nothings/stb
- **License**: Public Domain (or MIT-0)
- **Version**: stb_image.h 2.28, stb_image_write.h 1.16
- **Local Files**:
  - `source/encoding/stb/stb_image.c`
  - `source/encoding/stb/stb_image.h`
  - `source/encoding/stb/stb_image_write.c`
  - `source/encoding/stb/stb_image_write.h`
- **Modifications**: None; used as-is from upstream

**Public Domain Notice**:
```
This software is dual-licensed to the public domain and under the following
license: you are granted a perpetual, irrevocable license to copy, modify,
publish, and distribute this file as you see fit.
```

## 3. nlohmann/json
- **Source URL**: https://github.com/nlohmann/json
- **License**: MIT License
- **Version**: 3.11.2 (provided by raceboat builder image)
- **Usage**: Linked as system dependency; not vendored in this plugin
- **Local Files**: None (header-only, consumed from `/opt/race/include` in builder)

## 4. Race SDK (raceboat framework)
- **Source URL**: https://github.com/tst-race/raceboat
- **License**: Apache License 2.0
- **Version**: main branch (builder image tag `main`)
- **Usage**: Framework headers and libraries for IEncodingComponent API
- **Local Files**: None (consumed from builder image)

## 5. GoogleTest / GoogleMock
- **Source URL**: https://github.com/google/googletest
- **License**: BSD-3-Clause
- **Version**: 1.12.0 (provided by raceboat builder image)
- **Usage**: Unit test framework; linked as system dependency in test build

---

## Attribution Summary

| Component | License | Vendored | Modified |
|-----------|---------|----------|----------|
| 7thSamurai/steganography (core encoder) | MIT | Yes | Yes |
| stb_image / stb_image_write | Public Domain | Yes | No |
| nlohmann/json | MIT | No (system) | No |
| Race SDK | Apache 2.0 | No (system) | No |
| GoogleTest | BSD-3-Clause | No (system) | No |

All vendored source files retain their original copyright and license headers. The adapted Samurai encoder files include a header referencing the original MIT license and the upstream repository URL.