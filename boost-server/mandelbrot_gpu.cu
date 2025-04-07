#include <iostream>
#include <cstddef>
#include <thrust/complex.h>
#include <cuda.h>
#include <cstdint>

constexpr auto threads_perblock = 1024;

static void HandleError(cudaError_t err, const char *file, int line)
{
    if (err != cudaSuccess)
    {
        std::cout << cudaGetErrorString(err) << "  " << file << "  " << line << std::endl;
        exit(EXIT_FAILURE);
    }
}
#define HANDLE_ERROR(err) (HandleError(err, __FILE__, __LINE__))

inline __device__ float fminf(float a, float b)
{
    return a < b ? a : b;
}

inline __device__ float fmaxf(float a, float b)
{
    return a > b ? a : b;
}

inline __device__ float clamp(float f, float a, float b)
{
    return fmaxf(a, fminf(f, b));
}

inline __device__ void hsv2rgb_gpu(int h, int s, int v, uint8_t *r, uint8_t *g, uint8_t *b)
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

inline __device__ int recurrence_formula(thrust::complex<float> c, thrust::complex<float> z, int max_iter)
{
    // mandelbrot recurrence formula
    for (int i = 0; i < max_iter; i++)
    {
        z = z * z + c;
        if (thrust::norm(z) > 2.0f)
            return i % 360;
    }
    return 0;
}

inline __device__ int mandelbrot(int x, int y, int view_size, float scale, float center_x, float center_y)
{
    auto center = static_cast<float>(view_size) / 2.0f;
    auto jx = (static_cast<float>(x) - center) / center * scale - center_x;
    auto jy = (center - static_cast<float>(y)) / center * scale + center_y;
    thrust::complex<float> c(jx, jy);
    thrust::complex<float> z(0, 0);
    return recurrence_formula(c, z, 1000);
}

__global__ void kernel(unsigned char *ptr, int view_size, float scale, float center_x, float center_y)
{
    uint8_t r{0}, g{0}, b{0};
    auto x = blockIdx.x;
    auto y = threadIdx.x;
    auto offset = x + y * gridDim.x;
    auto value = mandelbrot(x, y, view_size, scale, center_x, center_y);
    hsv2rgb_gpu(value, 100, 100, &r, &g, &b);
    uint8_t rgba[4] = {r, g, b, 255};
    memcpy(ptr + offset * 4, rgba, 4);
}

extern "C" unsigned char *MandelbrotGPU(std::size_t size, int view_size, float scale, float center_x, float center_y)
{
    unsigned char *ptr_gpu;

    HANDLE_ERROR(cudaMalloc((void **)&ptr_gpu, size));

    auto *ptr = new unsigned char[size];
    HANDLE_ERROR(cudaMemcpy(ptr_gpu, ptr, size, cudaMemcpyHostToDevice));

    auto blocks_per_grid = ((view_size * view_size) + threads_perblock - 1) / threads_perblock;
    std::cout << "CUDA kernel [" << blocks_per_grid << "] blocks [" << threads_perblock << "] threads" << std::endl;

    kernel<<<1024, 1024>>>(ptr_gpu, view_size, scale, center_x, center_y);

    HANDLE_ERROR(cudaMemcpy(ptr, ptr_gpu, size, cudaMemcpyDeviceToHost));
    HANDLE_ERROR(cudaFree(ptr_gpu));
    return ptr;
}
