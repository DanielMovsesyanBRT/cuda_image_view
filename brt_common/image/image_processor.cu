

#include "image_processor.hpp"

namespace brt
{
namespace jupiter
{
namespace image
{

// Debayer an CRBC bayered image using bilinear interpolation
// and output debayered image in RGBRGBRGB (or BGRBGRBGR) in its
// original resolution.
__global__ void runCudaDebayer(const uint16_t* bayeredImg,
                               uint16_t* debayeredImg,
                               size_t width,
                               size_t height,
                               bool outputBGR,
                               uint32_t* histogram,
                               uint32_t hist_size,
                               int hist_size_bits,
                               bool overexpose)
{
    // The bayered image must have the following format (when expanded to 2D):
    //
    // C R C R C R
    // B C B C B C
    // C R C R C R
    // B C B C B C
    // C R C R C R
    // B C B C B C
    //
    // where upper left corner (i.e. the first element in the array is C,
    // and the second element in the array is R).
    //
    // Other format might work, but requires rewriting this kernel
    // Otherwise the color will be messed up.
    //
    // Also, each pixel in the original bayered image but be 12 bits
    // which stored in a 16 bit uint16_t structure.
    //
    // We will treat C channel as G channel in this kernel, because during image capture,
    // we have already set the proper gain for R and B.
    //
    // (x, y) is the coordinate how we will inspect the bayered pattern
    // Note that x and y are only even numbers, meaning that
    // in every kernel, we will perform bilinear interpolation for four pixels.
    // Therefore, for image size of 1920 * 1208, this kernel is called 960 * 604 times

    int x = 2 * ((blockIdx.x * blockDim.x) + threadIdx.x);
    int y = 2 * ((blockIdx.y * blockDim.y) + threadIdx.y);


    uint32_t b, g, r;
    uint32_t brightness;

    /* Upper left: C */
    if (x == 0 && y == 0)
    {
        g = bayeredImg[y * width + x];
        r = bayeredImg[y * width + (x+1)];
        b = bayeredImg[(y+1) * width + x];
    }
    else if (x == 0)
    {
        g = bayeredImg[y * width + x];
        r = bayeredImg[y * width + (x+1)];
        b = (bayeredImg[(y-1) * width + x] + bayeredImg[(y+1) * width + x]) / 2;
    }
    else if (y == 0)
    {
        g = bayeredImg[y * width + x];
        r = (bayeredImg[y * width + (x-1)] + bayeredImg[y * width + (x+1)]) / 2;
        b = bayeredImg[(y+1) * width + x];
    }
    else
    {
        g = bayeredImg[y * width + x];
        r = (bayeredImg[y * width + (x-1)] + bayeredImg[y * width + (x+1)]) / 2;
        b = (bayeredImg[(y-1) * width + x] + bayeredImg[(y+1) * width + x]) / 2;
    }

    if (outputBGR)
    {
        debayeredImg[3 * (y * width + x)] = b;
        debayeredImg[3 * (y * width + x) + 1] = g;
        debayeredImg[3 * (y * width + x) + 2] = r;
    } else
    {
        debayeredImg[3 * (y * width + x)] = r;
        debayeredImg[3 * (y * width + x) + 1] = g;
        debayeredImg[3 * (y * width + x) + 2] = b;
    }

    if (histogram != nullptr)
    {
      brightness = ((uint32_t)(r+r+r+b+g+g+g+g) >> 3) * hist_size >> hist_size_bits;
      atomicAdd(&histogram[brightness & ((1 << hist_size_bits) - 1)], 1);

      //
      if ((brightness == ((1 << hist_size_bits) - 1)) && overexpose)
      {
        debayeredImg[3 * (y * width + x)] = 0xFFFF;
        debayeredImg[3 * (y * width + x) + 1] = 0;
        debayeredImg[3 * (y * width + x) + 2] = 0;
      }
    }

    /* Upper right: R */
    if (x == width - 2 && y == 0)
    {
        r = bayeredImg[y * width + (x+1)];
        g = (bayeredImg[y * width + x] + bayeredImg[(y+1) * width + (x+1)]) / 2;
        b = bayeredImg[(y+1) * width + x];
    }
    else if (y == 0)
    {
        r = bayeredImg[y * width + (x+1)];
        g = (bayeredImg[y * width + x] + bayeredImg[y * width + (x+2)] +
                        bayeredImg[(y+1) * width + (x+1)]) / 3;

        b = (bayeredImg[(y+1) * width + x] + bayeredImg[(y+1) * width + (x+2)]) / 2;
    }
    else if (x == width - 2)
    {
        r = bayeredImg[y * width + (x+1)];
        g = (bayeredImg[y * width + x] + bayeredImg[(y-1) * width + (x+1)] +
                                      bayeredImg[(y+1) * width + (x+1)]) / 3;

        b = (bayeredImg[(y-1) * width + x] + bayeredImg[(y+1) * width + x]) / 2;
    }
    else
    {
        r = bayeredImg[y * width + (x+1)];
        g = (bayeredImg[y * width + x] + bayeredImg[y * width + (x+2)] +
                       bayeredImg[(y-1) * width + (x+1)] + bayeredImg[(y+1) * width + (x+1)]) / 4;

        b = (bayeredImg[(y-1) * width + x] + bayeredImg[(y-1) * width + (x+2)] +
                       bayeredImg[(y+1) * width + x] + bayeredImg[(y+1) * width + (x+2)]) / 4;
    }

    if (outputBGR)
    {
        debayeredImg[3 * (y * width + (x+1))] = b;
        debayeredImg[3 * (y * width + (x+1)) + 1] = g;
        debayeredImg[3 * (y * width + (x+1)) + 2] = r;
    }
    else
    {
        debayeredImg[3 * (y * width + (x+1))] = r;
        debayeredImg[3 * (y * width + (x+1)) + 1] = g;
        debayeredImg[3 * (y * width + (x+1)) + 2] = b;
    }

    if (histogram != nullptr)
    {
      brightness = ((uint32_t)(r+r+r+b+g+g+g+g) >> 3) * hist_size >> hist_size_bits;
      atomicAdd(&histogram[brightness & ((1 << hist_size_bits) - 1)], 1);

      //
      if ((brightness == ((1 << hist_size_bits) - 1)) && overexpose)
      {
        debayeredImg[3 * (y * width + (x+1))] = 0xFFFF;
        debayeredImg[3 * (y * width + (x+1)) + 1] = 0;
        debayeredImg[3 * (y * width + (x+1)) + 2] = 0;
      }
    }

    /* Lower left: B */
    if (x == 0 && y == height - 2)
    {
        b = bayeredImg[(y+1) * width + x];
        r = bayeredImg[y * width + (x+1)];
        g = (bayeredImg[y * width + x] + bayeredImg[(y+1) * width + (x+1)]) / 2;
    }
    else if (x == 0)
    {
        b = bayeredImg[(y+1) * width + x];
        r = (bayeredImg[y * width + (x+1)] + bayeredImg[(y+2) * width + (x+1)]) / 2;
        g = (bayeredImg[y * width + x] + bayeredImg[(y+1) * width + (x+1)] +
                            bayeredImg[(y+2) * width + x]) / 3;
    }
    else if (y == height - 2)
    {
        b = bayeredImg[(y+1) * width + x];
        r = (bayeredImg[y * width + (x-1)] + bayeredImg[y * width + (x+1)]) / 2;
        g = (bayeredImg[y * width + x] + bayeredImg[(y+1) * width + (x+1)] +
                            bayeredImg[(y+1) * width + (x-1)]) / 3;
    }
    else
    {
        b = bayeredImg[(y+1) * width + x];
        r = (bayeredImg[y * width + (x-1)] + bayeredImg[y * width + (x+1)] +
                            bayeredImg[(y+2) * width + (x-1)] + bayeredImg[(y+2) * width + (x+1)]) / 4;

        g = (bayeredImg[y * width + x] + bayeredImg[(y+1) * width + (x+1)] +
                            bayeredImg[(y+2) * width + x] + bayeredImg[(y+1) * width + (x-1)]) / 4;
    }

    if (outputBGR)
    {
        debayeredImg[3 * ((y+1) * width + x)] = b;
        debayeredImg[3 * ((y+1) * width + x) + 1] = g;
        debayeredImg[3 * ((y+1) * width + x) + 2] = r;
    }
    else
    {
        debayeredImg[3 * ((y+1) * width + x)] = r;
        debayeredImg[3 * ((y+1) * width + x) + 1] = g;
        debayeredImg[3 * ((y+1) * width + x) + 2] = b;
    }

    if (histogram != nullptr)
    {
      brightness = ((uint32_t)(r+r+r+b+g+g+g+g) >> 3) * hist_size >> hist_size_bits;
      atomicAdd(&histogram[brightness & ((1 << hist_size_bits) - 1)], 1);

      //
      if ((brightness == ((1 << hist_size_bits) - 1)) && overexpose)
      {
        debayeredImg[3 * ((y+1) * width + x)] = 0xFFFF;
        debayeredImg[3 * ((y+1) * width + x) + 1] = 0;
        debayeredImg[3 * ((y+1) * width + x) + 2] = 0;
      }
    }

    /* Lower right: C */
    if (x == width - 2 && y == height - 2)
    {
        g = bayeredImg[(y+1) * width + (x+1)];
        r = bayeredImg[y * width + (x+1)];
        b = bayeredImg[(y+1) * width + x];
    }
    else if (x == width - 2)
    {
        g = bayeredImg[(y+1) * width + (x+1)];
        r = (bayeredImg[y * width + (x+1)] + bayeredImg[(y+2) * width + (x+1)]) / 2;
        b = bayeredImg[(y+1) * width + x];
    }
    else if (y == height - 2)
    {
        g = bayeredImg[(y+1) * width + (x+1)];
        r = bayeredImg[y * width + (x+1)];
        b = (bayeredImg[(y+1) * width + x] + bayeredImg[(y+1) * width + (x+2)]) / 2;
    }
    else
    {
        g = bayeredImg[(y+1) * width + (x+1)];
        r = (bayeredImg[y * width + (x+1)] + bayeredImg[(y+2) * width + (x+1)]) / 2;
        b = (bayeredImg[(y+1) * width + x] + bayeredImg[(y+1) * width + (x+2)]) / 2;
    }

    if (outputBGR)
    {
        debayeredImg[3 * ((y+1) * width + (x+1))] = b;
        debayeredImg[3 * ((y+1) * width + (x+1)) + 1] = g;
        debayeredImg[3 * ((y+1) * width + (x+1)) + 2] = r;
    }
    else
    {
        debayeredImg[3 * ((y+1) * width + (x+1))] = r;
        debayeredImg[3 * ((y+1) * width + (x+1)) + 1] = g;
        debayeredImg[3 * ((y+1) * width + (x+1)) + 2] = b;
    }

    if (histogram != nullptr)
    {
      brightness = ((uint32_t)(r+r+r+b+g+g+g+g) >> 3) * hist_size >> hist_size_bits;
      atomicAdd(&histogram[brightness & ((1 << hist_size_bits) - 1)], 1);

      //
      if ((brightness == ((1 << hist_size_bits) - 1)) && overexpose)
      {
        debayeredImg[3 * ((y+1) * width + (x+1))] = 0xFFFF;
        debayeredImg[3 * ((y+1) * width + (x+1)) + 1] = 0;
        debayeredImg[3 * ((y+1) * width + (x+1)) + 2] = 0;
      }
   }
}


/*
 * \\fn __global__ void reduceHistogram
 *
 * created on: Nov 26, 2019
 * author: daniel
 *
 */
__global__ void reduceHistogram(uint32_t* big_hist, uint32_t big_size,
                                uint32_t* small_hist, uint32_t small_size)
{
  int tid = threadIdx.x + blockIdx.x * blockDim.x;
  int sm_index = tid * small_size / big_size;
  atomicAdd(&small_hist[sm_index], big_hist[tid]);
}

/*
 * \\fn void cudaMax
 *
 * created on: Nov 22, 2019
 * author: daniel
 *
 */
__global__ void cudaMax(uint32_t* max)
{
    const int tid = threadIdx.x + blockIdx.x * blockDim.x;

    auto step_size = 1;
    int number_of_threads = gridDim.x * blockDim.x;

    while (number_of_threads > 0)
    {
        if (tid < number_of_threads)
        {
            const auto fst = tid * step_size * 2;
            const auto snd = fst + step_size;

            max[fst] = (max[fst] < max[snd]) ? max[snd] : max[fst];
        }

        step_size <<= 1;
        number_of_threads >>= 1;
    }
}

/*
 * \\fn bool ImageProcessor::runDebayer
 *
 * created on: Nov 25, 2019
 * author: daniel
 *
 */
bool ImageProcessor::runDebayer(bool outputBGR)
{
  int hist_size_bits = ((sizeof(unsigned int) * 8) - 1 -
          __builtin_clz(_histogram.size()));

  dim3 threads(_thx,_thy);
  dim3 blocks(_blkx, _blky);

  runCudaDebayer<<<blocks,threads>>>(_img_buffer.ptr(),
                                     _img_debayer_buffer.ptr(),
                                     _width,
                                     _height,
                                     outputBGR,
                                     _histogram.ptr(),
                                     _histogram.size(),
                                     hist_size_bits,
                                     _overexposure_flag.load());

  int thx = 64;
  while (_histogram.size() < thx)
    thx >>= 1;

  if (_histogram.copy(_histogram_max))
    cudaMax<<<_histogram.size() / thx, thx>>>(_histogram_max.ptr());

  reduceHistogram<<<_histogram.size() / thx, thx>>>(_histogram.ptr(),
                                                    _histogram.size(),
                                                    _small_histogram.ptr(),
                                                    _small_histogram.size());
  return true;
}

}
}
}
