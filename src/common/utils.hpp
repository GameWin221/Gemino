#ifndef GEMINO_UTILS_HPP
#define GEMINO_UTILS_HPP

#include <common/types.hpp>
#include <common/debug.hpp>

#include <render_api/vertex.hpp>
#include <glm/gtc/quaternion.hpp>

#include <vector>
#include <string>
#include <cstring>
#include <fstream>
#include <unordered_map>

namespace Utils {
    u32 nearest_pot_floor(u32 x);

    u32 calculate_mipmap_levels_x(u32 width);
    u32 calculate_mipmap_levels_xy(u32 width, u32 height);
    u32 calculate_mipmap_levels_xyz(u32 width, u32 height, u32 depth);

    std::vector<u8> read_file_bytes(const std::string& path);
    std::vector<std::string> read_file_lines(const std::string& path);

    std::string get_file_name(const std::string &path, bool with_extension = true);
    std::string get_directory(const std::string &path);

    inline u32 as_u32(const f32 x) {
        return *(u32*)&x;
    }
    inline f32 as_f32(const u32 x) {
        return *(f32*)&x;
    }

    // https://stackoverflow.com/questions/1659440/32-bit-to-16-bit-floating-point-conversion
    inline u16 f32_to_f16(f32 x) { // IEEE-754 16-bit floating-point format (without infinity): 1-5-10, exp-15, +-131008.0, +-6.1035156E-5, +-5.9604645E-8, 3.311 digits
        u32 b = as_u32(x)+0x00001000; // round-to-nearest-even: add last bit after truncated mantissa
        u32 e = (b&0x7F800000)>>23; // exponent
        u32 m = b&0x007FFFFF; // mantissa; in line below: 0x007FF000 = 0x00800000-0x00001000 = decimal indicator flag - initial rounding
        return (b&0x80000000)>>16 | (e>112)*((((e-112)<<10)&0x7C00)|m>>13) | ((e<113)&(e>101))*((((0x007FF000+m)>>(125-e))+1)>>1) | (e>143)*0x7FFF; // sign : normalized : denormalized : saturate
    }

    // https://stackoverflow.com/questions/1659440/32-bit-to-16-bit-floating-point-conversion
    inline f32 f16_to_f32(u16 x) { // IEEE-754 16-bit floating-point format (without infinity): 1-5-10, exp-15, +-131008.0, +-6.1035156E-5, +-5.9604645E-8, 3.311 digits
        u32 e = (x&0x7C00)>>10; // exponent
        u32 m = (x&0x03FF)<<13; // mantissa
        u32 v = as_u32((f32)m)>>23; // evil log2 bit hack to count leading zeros in denormalized format
        return as_f32((x&0x8000)<<16 | (e!=0)*((e+112)<<23|m) | ((e==0)&(m!=0))*((v-37)<<23|((m<<(150-v))&0x007FE000))); // sign : normalized : denormalized
    }

    constexpr usize align(usize alignment, usize size) {
        return ((size - 1) / alignment + 1) * alignment;
    }
    constexpr usize div_ceil(usize a, usize b) {
        return (a - 1) / b + 1;
    }
}

#endif