#include "palette.h"

#include <array.h>
#include <memory.h>
#include <string_stream.h>
#include <temp_allocator.h>

#define __STDC_WANT_LIB_EXT1__ 1
#include <cstring>
#include <limits>
#include <string>

#include <engine/file.h>
#include <engine/log.h>

bool grunka::load_palette(const char *file_path, foundation::Array<math::Color4f> &palette) {
    using namespace foundation;

    if (!engine::file::exist(file_path)) {
        log_error("Palette file missing: %s", file_path);
        return false;
    }

    TempAllocator1024 ta;
    Array<char> buffer(ta);
    if (!engine::file::read(buffer, file_path)) {
        log_error("Could not read palette file: %s", file_path);
        return false;
    }

    char *p = array::begin(buffer);
    const char *pe = array::end(buffer);
    auto incr_p = [&p, pe, file_path](size_t increment, bool require_remaining = true) -> bool {
        p += increment;
        if (require_remaining ? p > pe : p >= pe) {
            log_error("Could not parse: %s, invalid file format", file_path);
            return false;
        }
        return true;
    };

    // JASC-PAL
    {
        const int header_size = 8;
        char header_buf[header_size];

        if (pe - p < header_size) {
            log_error("Could not parse: %s, invalid file format", file_path);
            return false;
        }

        memcpy(header_buf, p, header_size);

        if (strncmp(header_buf, "JASC-PAL", header_size) != 0) {
            log_error("Could not parse: %s, invalid header, expected JASC-PAL", file_path);
            return false;
        }

        if (!incr_p(header_size)) {
            return false;
        }

        if (*p == '\r') {
            if (!incr_p(1)) {
                return false;
            }
        }

        if (*p == '\n') {
            if (!incr_p(1)) {
                return false;
            }
        }
    }

    // 0100
    {
        const int header_size = 4;
        char header_buf[header_size];

        if (pe - p < header_size) {
            log_error("Could not parse: %s, invalid file format", file_path);
            return false;
        }

        memcpy(header_buf, p, header_size);

        if (strncmp(header_buf, "0100", header_size) != 0) {
            log_error("Could not parse: %s, invalid header, expected 0100", file_path);
            return false;
        }

        if (!incr_p(header_size)) {
            return false;
        }

        if (*p == '\r') {
            if (!incr_p(1)) {
                return false;
            }
        }

        if (*p == '\n') {
            if (!incr_p(1)) {
                return false;
            }
        }
    }

    int num_colors = 0;

    {
        string_stream::Buffer ss(ta);
        while (*p != '\n') {
            if (*p != '\r') {
                array::push_back(ss, *p);
            }
            if (!incr_p(1)) {
                return false;
            }
        }

        if (!incr_p(1)) {
            return false;
        }

        try {
            num_colors = std::stoi(string_stream::c_str(ss));
        } catch (...) {
            log_error("Could not parse: %s, invalid number of colors", file_path);
            return false;
        }
    }

    for (int i = 0; i < num_colors; ++i) {
        if (p >= pe) {
            log_error("Could not parse: %s, invalid file format.", file_path);
            return false;
        }

        string_stream::Buffer ss(ta);
        while (true) {
            if (*p != '\r') {
                array::push_back(ss, *p);
            }

            if (p + 1 == pe) {
                break;
            } else {
                if (!incr_p(1, false)) {
                    return false;
                }
            }

            if (*p == '\n') {
                if (p + 1 < pe) {
                    if (!incr_p(1, false)) {
                        return false;
                    }
                }
                break;
            }
        }

        unsigned int r = 0;
        unsigned int g = 0;
        unsigned int b = 0;

        if (sscanf(string_stream::c_str(ss), "%u %u %u", &r, &g, &b) != 3) {
            log_error("Could not parse: %s, invalid colors: %s", file_path, string_stream::c_str(ss));
            return false;
        }

        if (r > std::numeric_limits<uint8_t>::max() || r < std::numeric_limits<uint8_t>::min()) {
            log_error("Color component out of bounds %u", r);
            return false;
        }

        if (g > std::numeric_limits<uint8_t>::max() || g < std::numeric_limits<uint8_t>::min()) {
            log_error("Color component out of bounds %u", r);
            return false;
        }

        if (b > std::numeric_limits<uint8_t>::max() || b < std::numeric_limits<uint8_t>::min()) {
            log_error("Color component out of bounds %u", r);
            return false;
        }

        math::Color4f col;
        col.r = static_cast<uint8_t>(r) / 255.0f;
        col.g = static_cast<uint8_t>(g) / 255.0f;
        col.b = static_cast<uint8_t>(b) / 255.0f;
        col.a = 1.0f;

        array::push_back(palette, col);
    }

    return true;
}
