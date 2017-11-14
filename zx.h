
/*  ZX Spectrum Emulator.

    Copyright (C) 2017 Ivan Kosarev.
    ivan@kosarev.info

    Published under the MIT license.
*/

#include "z80/z80.h"

namespace zx {

using z80::least_u8;
using z80::fast_u8;
using z80::fast_u16;

template<typename T>
T non_constexpr() {
    return T();
}

template<typename T>
constexpr T div_exact(T a, T b) {
    return a % b == 0 ? a / b : non_constexpr<T>();
}

class spectrum48 : public z80::processor<spectrum48> {
public:
    typedef processor<spectrum48> base;
    typedef uint_fast32_t ticks_type;

    spectrum48()
            : ticks(0) {
        uint_fast32_t rnd = 0xde347a01;
        for(auto &cell : image) {
            cell = static_cast<least_u8>(rnd);
            rnd = (rnd * 0x74392cef) ^ (rnd >> 16);
        }
    }

    void tick(unsigned t) { ticks += t; }

    ticks_type get_ticks() const { return ticks; }

    least_u8 &on_access(fast_u16 addr) {
        assert(addr < image_size);
        return image[addr];
    }

protected:
    // Four bits per frame pixel in brightness:grb format.
    static const unsigned bits_per_frame_pixel = 4;

    static const unsigned brightness_bit = 3;
    static const unsigned green_bit = 2;
    static const unsigned red_bit = 1;
    static const unsigned blue_bit = 0;

    static const unsigned brightness_mask = 1u << brightness_bit;
    static const unsigned green_mask = 1u << green_bit;
    static const unsigned red_mask = 1u << red_bit;
    static const unsigned blue_mask = 1u << blue_bit;

    // Eight frame pixels per chunk. The leftmost pixel occupies the most
    // significant bits.
    static const unsigned frame_pixels_per_chunk = 8;

    // The type of frame chunks.
    static const unsigned frame_chunk_width = 32;
    static_assert(
        bits_per_frame_pixel * frame_pixels_per_chunk <= frame_chunk_width,
        "The frame chunk width is too small!");
    typedef uint_least32_t frame_chunk;

    // The dimensions of the viewable area.
    // TODO: Support the NTSC geometry.
    static const unsigned screen_width = 256;
    static const unsigned screen_height = 192;
    static const unsigned border_width = 48;
    static const unsigned top_border_height = 48;
    static const unsigned bottom_border_height = 40;

    static const unsigned frame_width =
        border_width + screen_width + border_width;
    static const unsigned frame_height =
        top_border_height + screen_height + bottom_border_height;

    // We want screen, border and frame widths be multiples of chunk widths to
    // simplify the processing code and to benefit from aligned memory accesses.
    static const unsigned chunks_per_border_width =
        div_exact(border_width, frame_pixels_per_chunk);
    static const unsigned chunks_per_screen_line =
        div_exact(screen_width, frame_pixels_per_chunk);
    static const unsigned chunks_per_frame_line =
        div_exact(frame_width, frame_pixels_per_chunk);

    frame_chunk frame_chunks[frame_height][chunks_per_frame_line];

    void render_frame() {
        static_assert(bits_per_frame_pixel == 4,
                      "Unsupported frame pixel format!");
        static_assert(frame_pixels_per_chunk == 8,
                      "Unsupported frame chunk format!");

        const unsigned black = 0;
        const unsigned white = red_mask | green_mask | blue_mask;
        const frame_chunk white_chunk = 0x11111111 * white;

        // Render the top border area.
        unsigned i = 0;
        for(; i != top_border_height; ++i) {
            frame_chunk *line = frame_chunks[i];
            for(unsigned j = 0; j != chunks_per_frame_line; ++j)
                line[j] = white_chunk;
        }

        // Render the screen area.
        fast_u16 line_addr = 0x4000;
        for(; i != top_border_height + screen_height; ++i) {
            frame_chunk *line = frame_chunks[i];

            // Left border.
            unsigned j = 0;
            for(; j != chunks_per_border_width; ++j)
                line[j] = white_chunk;

            // Screen.
            fast_u16 addr = line_addr;
            for(; j != chunks_per_border_width + chunks_per_screen_line; ++j) {
                fast_u8 b = on_access(addr);
                uint_fast32_t c = 0;
                c |= (b & 0x80) ? black : white; c <<= 4;
                c |= (b & 0x40) ? black : white; c <<= 4;
                c |= (b & 0x20) ? black : white; c <<= 4;
                c |= (b & 0x10) ? black : white; c <<= 4;
                c |= (b & 0x08) ? black : white; c <<= 4;
                c |= (b & 0x04) ? black : white; c <<= 4;
                c |= (b & 0x02) ? black : white; c <<= 4;
                c |= (b & 0x01) ? black : white;
                line[j] = static_cast<frame_chunk>(c);
                ++addr;
            }

            // Right border.
            for(; j != chunks_per_frame_line; ++j)
                line[j] = white_chunk;

            // Move to next line.
            if(((line_addr += 0x0100) & 0x700) == 0) {
                line_addr += 0x20;
                line_addr -= (line_addr & 0xff) < 0x20 ? 0x100 : 0x800;
            }
        }

        // Render the bottom border area.
        for(; i != frame_height; ++i) {
            frame_chunk *line = frame_chunks[i];
            for(unsigned j = 0; j != chunks_per_frame_line; ++j)
                line[j] = white_chunk;
        }
    }

    void execute_frame() {
        const ticks_type ticks_per_frame = 69888;
        while(ticks < ticks_per_frame)
            step();
        ticks -= ticks_per_frame;
    }

    ticks_type ticks;

private:
    static const z80::size_type image_size = 0x10000;  // 64K bytes.
    least_u8 image[image_size];
};

}  // namespace zx
