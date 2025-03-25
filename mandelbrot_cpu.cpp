#include <new>
#include <cstring>
#include <complex>
#include <cstddef>
#include <iostream>
#include <omp.h>
#include <cstdint>

inline float fminf(float a, float b)
{
    return a < b ? a : b;
}

inline float fmaxf(float a, float b)
{
    return a > b ? a : b;
}

inline float clamp(float f, float a, float b)
{
    return fmaxf(a, fminf(f, b));
}

// https://scrapbox.io/ePi5131/%E6%8B%A1%E5%BC%B5%E7%B7%A8%E9%9B%86%E3%81%AEHSV%E9%96%A2%E6%95%B0%E3%81%AEHSV%3ERGB%E5%A4%89%E6%8F%9B%E3%81%AE%E5%86%85%E5%AE%B9
inline void hsv2rgb_cpu(int h, int s, int v, uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (h < 0)
        h += (1 - h / 360) * 360;
    if (360 < h)
        h %= 360;
    auto h1 = (h * 4096 + 50) / 120;
    auto s1 = (s * 4096 + 50) / 100;
    auto v1 = (v * 4096 + 50) / 100;
    auto h2 = h1 % 4096;
    auto a1{0}, a2{0};
    if (h2 < 2048)
    {
        a1 = (4096 - (2048 - h2) * s1 / 2048) * v1 / 4096;
        a2 = v1;
    }
    else
    {
        a2 = (4096 - (h2 - 2048) * s1 / 2048) * v1 / 4096;
        a1 = v1;
    }

    auto b1 = clamp((a2 * 255 + 2048) / 4096, 0, 255);
    auto b2 = clamp((a1 * 255 + 2048) / 4096, 0, 255);
    auto b3 = clamp(((4096 - s1) * v1 / 4096 * 255 + 2048) / 4096, 0, 255);

    switch (h1 / 4096)
    {
    case 1:
        *g = b1;
        *b = b2;
        *r = b3;
        break;
    case 2:
        *b = b1;
        *r = b2;
        *g = b3;
        break;
    default:
        *r = b1;
        *g = b2;
        *b = b3;
        break;
    }
}

inline int recurrence_formula(std::complex<float> c, std::complex<float> z, int max_iter)
{
    // mandelbrot recurrence formula
    for (int i = 0; i < max_iter; i++)
    {
        z = z * z + c;
        if (std::norm(z) > 2.0f)
            return i % 360;
    }
    return 0;
}

inline int mandelbrot(int x, int y, int view_size, float scale, float center_x, float center_y)
{
    auto center = static_cast<float>(view_size) / 2.0f;
    auto jx = (static_cast<float>(x) - center) / center * scale - center_x;
    auto jy = (center - static_cast<float>(y)) / center * scale + center_y;
    // 実部jx、虚部jyの複素数
    std::complex<float> c(jx, jy);
    std::complex<float> z(0, 0);
    return recurrence_formula(c, z, 1000);
}

extern "C" unsigned char *MandelbrotCPU(std::size_t size, int view_size, float scale, float center_x, float center_y)
{
    auto *ptr = new unsigned char[size];
    uint8_t r{0}, g{0}, b{0};

#pragma omp parallel for private(r, g, b)
    for (auto y = 0; y < view_size; y++)
    {
        for (auto x = 0; x < view_size; x++)
        {
            auto offset = x + y * view_size;
            auto value = mandelbrot(x, y, view_size, scale, center_x, center_y);
            hsv2rgb_cpu(value, 100, 100, &r, &g, &b);
            uint8_t rgba[4] = {r, g, b, 255};
            memcpy(ptr + offset * 4, rgba, 4);
        }
    }
    return ptr;
}
