#include "dec/liar_soft/wcg_image_decoder.h"
#include "algo/format.h"
#include "algo/range.h"
#include "dec/liar_soft/cg_decompress.h"
#include "err.h"
#include "io/memory_stream.h"

using namespace au;
using namespace au::dec::liar_soft;

static const bstr magic = "WG"_b;

bool WcgImageDecoder::is_recognized_impl(io::File &input_file) const
{
    if (input_file.stream.read(magic.size()) != magic)
        return false;

    const int version = input_file.stream.read_le<u16>();
    if (((version & 0xF) != 1) || ((version & 0x1C0) != 64))
        return false;

    return true;
}

res::Image WcgImageDecoder::decode_impl(
    const Logger &logger, io::File &input_file) const
{
    input_file.stream.seek(magic.size());

    input_file.stream.skip(2);
    const auto depth = input_file.stream.read_le<u16>();
    if (depth != 32)
        throw err::UnsupportedBitDepthError(depth);
    input_file.stream.skip(2);

    const auto width = input_file.stream.read_le<u32>();
    const auto height = input_file.stream.read_le<u32>();
    const auto canvas_size = width * height;

    bstr output(canvas_size * 4);
    cg_decompress(output, 2, 4, input_file.stream, 2);
    cg_decompress(output, 0, 4, input_file.stream, 2);

    for (const auto i : algo::range(0, output.size(), 4))
        output[i + 3] ^= 0xFF;

    return res::Image(width, height, output, res::PixelFormat::BGRA8888);
}

static auto _ = dec::register_decoder<WcgImageDecoder>("liar-soft/wcg");
