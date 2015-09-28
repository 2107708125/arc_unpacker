#include "fmt/alice_soft/vsp_image_decoder.h"
#include "err.h"
#include "fmt/alice_soft/pms_image_decoder.h"
#include "util/range.h"

using namespace au;
using namespace au::fmt::alice_soft;

bool VspImageDecoder::is_recognized_internal(File &file) const
{
    return file.has_extension("vsp");
}

static bstr decompress_vsp(io::IO &input_io, size_t width, size_t height)
{
    if (width % 8 != 0)
        throw std::logic_error("Invalid width");

    bstr output(width * height);

    std::unique_ptr<u8[][4]> buf1(new u8[height][4]);
    std::unique_ptr<u8[][4]> buf2(new u8[height][4]);

    auto bp = buf1.get();
    auto bc = buf2.get();
    auto mask = 0;

    for (auto x : util::range(width / 8))
    {
        for (auto plane : util::range(4))
        {
            size_t y = 0;
            while (y < height)
            {
                auto c = input_io.read_u8();
                switch (c)
                {
                    case 0x00:
                    {
                        auto n = input_io.read_u8() + 1;
                        while (n-- && y < height)
                        {
                            bc[y][plane] = bp[y][plane];
                            y++;
                        }
                        break;
                    }

                    case 0x01:
                    {
                        auto n = input_io.read_u8() + 1;
                        auto b0 = input_io.read_u8();
                        while (n-- && y < height)
                            bc[y++][plane] = b0;
                        break;
                    }

                    case 0x02:
                    {
                        auto n = input_io.read_u8() + 1;
                        auto b0 = input_io.read_u8();
                        auto b1 = input_io.read_u8();
                        while (n-- && y < height)
                        {
                            bc[y++][plane] = b0;
                            if (y >= height) break;
                            bc[y++][plane] = b1;
                        }
                        break;
                    }

                    case 0x03:
                    {
                        auto n = input_io.read_u8() + 1;
                        while (n-- && y < height)
                        {
                            bc[y][plane] = bc[y][0] ^ mask;
                            y++;
                        }
                        mask = 0;
                        break;
                    }

                    case 0x04:
                    {
                        auto n = input_io.read_u8() + 1;
                        while (n-- && y < height)
                        {
                            bc[y][plane] = bc[y][1] ^ mask;
                            y++;
                        }
                        mask = 0;
                        break;
                    }

                    case 0x05:
                    {
                        auto n = input_io.read_u8() + 1;
                        while (n-- && y < height)
                        {
                            bc[y][plane] = bc[y][2] ^ mask;
                            y++;
                        }
                        mask = 0;
                        break;
                    }

                    case 0x06:
                        mask = 0xFF;
                        break;

                    case 0x07:
                        bc[y++][plane] = input_io.read_u8();
                        break;

                    default:
                        bc[y++][plane] = c;
                        break;
                }
            }
        }

        for (auto y : util::range(height))
        {
            int component[4];
            for (auto i : util::range(4))
                component[i] = (bc[y][i] << i) | (bc[y][i] >> (8 - i));

            auto output_ptr = &output[(y * (width / 8) + x) * 8];
            for (auto j : util::range(8))
            {
                for (auto i : util::range(4))
                    *output_ptr |= (component[i] >> (7 - j)) & (1 << i);
                output_ptr++;
            }
        }

        auto tmp = bp;
        bp = bc;
        bc = tmp;
    }

    return output;
}

pix::Grid VspImageDecoder::decode_internal(File &file) const
{
    auto x = file.io.read_u16_le();
    auto y = file.io.read_u16_le();
    auto width = file.io.read_u16_le() - x;
    auto height = file.io.read_u16_le() - y;
    auto use_pms = file.io.read_u8() > 0;

    std::unique_ptr<pix::Grid> pixels;

    if (use_pms)
    {
        width = ((width + 7) / 8) * 8;

        file.io.seek(0x20);
        pix::Palette palette(256, file.io, pix::Format::RGB888);

        file.io.seek(0x320);
        auto pixel_data
            = PmsImageDecoder::decompress_8bit(file.io, width, height);
        pixels.reset(new pix::Grid(width, height, pixel_data, palette));
    }
    else
    {
        width *= 8;

        file.io.seek(0x0A);
        pix::Palette palette(16);
        for (auto &c : palette)
        {
            c.b = (file.io.read_u8() & 0x0F) * 0x11;
            c.r = (file.io.read_u8() & 0x0F) * 0x11;
            c.g = (file.io.read_u8() & 0x0F) * 0x11;
        }

        file.io.seek(0x3A);
        auto pixel_data = decompress_vsp(file.io, width, height);
        pixels.reset(new pix::Grid(width, height, pixel_data, palette));
    }

    return *pixels;
}

static auto dummy = fmt::Registry::add<VspImageDecoder>("alice/vsp");