#include <iostream>
#include <string>
#include <cstddef>
#include <memory>
#include <chrono>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define TIME_ELAPSED_START(x) auto x##0 = std::chrono::system_clock::now();
#define TIME_ELAPSED_END(x, mess)                                      \
   auto x##1 = std::chrono::system_clock::now();                       \
   std::chrono::duration<double, std::milli> x##elapsed = x##1 - x##0; \
   std::cout << mess << x##elapsed.count() << " ms" << std::endl;

constexpr auto view_size = 1024;

extern "C" unsigned char *MandelbrotCPU(std::size_t, int, float, float, float);
#ifdef __CUDACC__
extern "C" unsigned char *MandelbrotGPU(std::size_t, int, float, float, float);
#endif

struct RGBA
{
   unsigned char r, g, b, a;
};

int draw(unsigned char *ptr, const std::string &filename)
{
   constexpr std::size_t width{view_size}, height{view_size};
   std::unique_ptr<RGBA[][width]> rgba(new (std::nothrow) RGBA[height][width]);
   if (!rgba)
      return -1;

   for (std::size_t row{}; row < height; ++row)
      for (std::size_t col{}; col < width; ++col)
      {
         memcpy(&rgba[row][col], &ptr[(col + row * view_size) * 4], 4);
      }

   stbi_write_png(filename.c_str(), static_cast<int>(width), static_cast<int>(height),
                  static_cast<int>(sizeof(RGBA)), rgba.get(), 0);
   return 0;
}

int main()
{

   auto size = view_size * view_size * 4 * sizeof(unsigned char);
   const auto scale = 2.0f;
   const auto center_x = 0.0f;
   const auto center_y = 0.0f;
   // const auto scale = 0.00002f;
   //  const auto center_x = 0.743643135f;
   //  const auto center_y = 0.131825963f;

   // run on CPU
   TIME_ELAPSED_START(CPU);
   auto *ptr_cpu = MandelbrotCPU(size, view_size, scale, center_x, center_y);
   TIME_ELAPSED_END(CPU, "CPU total result..  ");
   draw(ptr_cpu, "picture_CPU.png");
   delete (ptr_cpu);

#ifdef __CUDACC__
   //  run on CPU
   TIME_ELAPSED_START(GPU);
   auto *ptr_gpu = MandelbrotGPU(size, view_size, scale, center_x, center_y);
   TIME_ELAPSED_END(GPU, "GPU total result..  ");
   draw(ptr_gpu, "picture_GPU.png");
   delete (ptr_gpu);
#endif
}
