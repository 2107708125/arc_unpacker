#include "fmt/qlie/mt.h"
#include "util/range.h"

using namespace au;
using namespace au::fmt::qlie;

static const int n = 64;
static const int m = 39;
static const u32 matrix_a = 0x9908B0DFul;
static const u32 upper_mask = 0x80000000ul;
static const u32 lower_mask = 0x7FFFFFFFul;

struct CustomMersenneTwister::Priv final
{
    u32 state[n];
    int mti;
};

static void init_state(u32 state[], const u32 seed, int &mti)
{
    state[0] = seed & 0xFFFFFFFFul;
    for (mti = 1; mti < n; mti++)
    {
        u32 tmp = state[mti - 1] ^ (state[mti - 1] >> 30);
        state[mti] = (1712438297ul * tmp + mti) & 0xFFFFFFFFul;
    }
}

CustomMersenneTwister::CustomMersenneTwister(u32 seed) : p(new Priv)
{
    init_state(p->state, seed, p->mti);
}

CustomMersenneTwister::~CustomMersenneTwister()
{
}

void CustomMersenneTwister::xor_state(const bstr &data)
{
    const u32 *data_ptr = data.get<const u32>();
    const u32 *data_end = data.end<const u32>();

    size_t i = 0;
    while (i < n && data_ptr < data_end)
        p->state[i++] ^= *data_ptr++;
}

u32 CustomMersenneTwister::get_next_integer()
{
    u32 y;
    static u32 mag01[2] = {0x0ul, matrix_a};

    if (p->mti >= n)
    {
        int kk;

        if (p->mti == n + 1)
            init_state(p->state, 5489ul, p->mti);

        for (kk = 0; kk < n - m; kk++)
        {
            y = (p->state[kk] & upper_mask)
                | ((p->state[kk + 1] & lower_mask) >> 1);

            p->state[kk]
                = p->state[kk + m] ^ y ^ mag01[p->state[kk + 1] & 0x1ul];
        }

        for (; kk < n - 1; kk++)
        {
            y = (p->state[kk] & upper_mask)
                | ((p->state[kk + 1] & lower_mask) >> 1);

            p->state[kk]
                = p->state[kk + (m - n)] ^ y ^ mag01[p->state[kk + 1] & 0x1ul];
        }

        y = (p->state[n - 1] & upper_mask) | ((p->state[0] & lower_mask) >> 1);
        p->state[n - 1] = p->state[m - 1] ^ y ^ mag01[p->state[n - 1] & 0x1ul];
        p->mti = 0;
    }

    y = p->state[p->mti++];

    y ^= (y >> 11);
    y ^= (y << 7) & 0x9C4F88E3ul;
    y ^= (y << 15) & 0xE7F70000ul;
    y ^= (y >> 18);

    return y;
}
