#ifdef BITALIGN
    #pragma OPENCL EXTENSION cl_amd_media_ops : enable
    #define RRotate(x, n) amd_bitalign(x, x, (uint) (32 - n))
#else
    #define RRotate(x, n) rotate(x, (uint) n)
#endif
#define RShift(x, n) ((x) >> n)

__constant uint hi[8] =
{
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0xfc08884d, 0xec9fcd13
};

__constant uint k[64] =
{
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

__kernel void search(const uint h0, const uint h1, const uint h2, const uint h3,
                     const uint h4, const uint h5, const uint h6, const uint h7,
                     const uint w0, const uint w1, const uint w2, __global ulong *out)
{
    uint w[128];
    w[0] = w0;
    w[1] = w1;
    w[2] = w2;
    w[3] = get_global_id(0);
    w[4] = 0x80000000U;
    w[5] = w[6] = w[7] = w[8] = w[9] = w[10] = w[11] = w[12] = w[13] = w[14] = 0x00000000U;
    w[15] = 0x00000280U;

    /* SHA pass 1 */
    for (int i = 16; i < 64; i++)
    {
        uint s0 = RRotate(w[i - 15], 7) ^ RRotate(w[i - 15], 18) ^ RShift(w[i - 15], 3);
        uint s1 = RRotate(w[i - 2], 17) ^ RRotate(w[i - 2], 19) ^ RShift(w[i - 2], 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint a = h0, b = h1, c = h2, d = h3, e = h4, f = h5, g = h6, h = h7;
    for (int i = 0; i < 64; i++)
    {
        uint S0 = RRotate(a, 2) ^ RRotate(a, 13) ^ RRotate(a, 22);
        uint maj = (a & b) ^ (a & c) ^ (b & c);
        uint t2 = S0 + maj;
        uint S1 = RRotate(e, 6) ^ RRotate(e, 11) ^ RRotate(e, 25);
        uint ch = (e & f) ^ ((~e) & g);
        uint t1 = h + S1 + ch + k[i] + w[i];

        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    /* SHA pass 2 */
    w[64] = h0 + a;
    w[65] = h1 + b;
    w[66] = h2 + c;
    w[67] = h3 + d;
    w[68] = h4 + e;
    w[69] = h5 + f;
    w[70] = h6 + g;
    w[71] = h7 + h;
    w[72] = 0x80000000U;
    w[73] = w[74] = w[75] = w[76] = w[77] = w[78] = 0x00000000U;
    w[79] = 0x00000100U;
    for (int i = 80; i < 128; i++)
    {
        uint s0 = RRotate(w[i - 15], 7) ^ RRotate(w[i - 15], 18) ^ RShift(w[i - 15], 3);
        uint s1 = RRotate(w[i - 2], 17) ^ RRotate(w[i - 2], 19) ^ RShift(w[i - 2], 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    a = hi[0], b = hi[1], c = hi[2], d = hi[3], e = hi[4], f = hi[5], g = hi[6], h = hi[7];
    for (int i = 0; i < 64; i++)
    {
        uint S0 = RRotate(a, 2) ^ RRotate(a, 13) ^ RRotate(a, 22);
        uint maj = (a & b) ^ (a & c) ^ (b & c);
        uint t2 = S0 + maj;
        uint S1 = RRotate(e, 6) ^ RRotate(e, 11) ^ RRotate(e, 25);
        uint ch = (e & f) ^ ((~e) & g);
        uint t1 = h + S1 + ch + k[i] + w[i];

        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    if (hi[0] + a == 0)
    {
        out[0] = 1;
        out[1] = get_global_id(0);
    }
}
