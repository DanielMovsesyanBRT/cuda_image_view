/*
 * debayer_bilin.cu
 *
 *  Created on: Feb 19, 2020
 *      Author: daniel
 */


#include "debayer_bilin.hpp"
#include "cuda_2d_mem.hpp"
#include "cuda_mem.hpp"

#include <cuda_profiler_api.h>

#define BITS_PER_PIXEL                      (1 << 16)
#define SMALL_HIST_SIZE                     (9)

namespace brt
{
namespace jupiter
{

// Debayer an CRBC bayered image using bilinear interpolation
// and output debayered image in RGBRGBRGB (or BGRBGRBGR) in its
// original resolution.
__global__ void runCudaDebayer(const uint16_t* bayeredImg,
                               uint16_t* debayeredImg,
                               size_t width,
                               size_t height,
                               uint32_t* histogram,
                               uint32_t hist_size,
                               int hist_size_bits)
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
    r = bayeredImg[y * width + (x + 1)];
    b = bayeredImg[(y + 1) * width + x];
  }
  else if (x == 0)
  {
    g = bayeredImg[y * width + x];
    r = bayeredImg[y * width + (x + 1)];
    b = (bayeredImg[(y - 1) * width + x] + bayeredImg[(y + 1) * width + x]) / 2;
  }
  else if (y == 0)
  {
    g = bayeredImg[y * width + x];
    r = (bayeredImg[y * width + (x - 1)] + bayeredImg[y * width + (x + 1)]) / 2;
    b = bayeredImg[(y + 1) * width + x];
  }
  else
  {
    g = bayeredImg[y * width + x];
    r = (bayeredImg[y * width + (x - 1)] + bayeredImg[y * width + (x + 1)]) / 2;
    b = (bayeredImg[(y - 1) * width + x] + bayeredImg[(y + 1) * width + x]) / 2;
  }

  debayeredImg[4 * (y * width + x)] = r;
  debayeredImg[4 * (y * width + x) + 1] = g;
  debayeredImg[4 * (y * width + x) + 2] = b;
  debayeredImg[4 * (y * width + x) + 3] = (uint16_t) -1;

  if (histogram != nullptr)
  {
    brightness = ((uint32_t) (r + r + r + b + g + g + g + g) >> 3) * hist_size
        >> hist_size_bits;
    atomicAdd(&histogram[brightness & ((1 << hist_size_bits) - 1)], 1);

    //
//      if ((brightness == ((1 << hist_size_bits) - 1)) && overexpose)
//      {
//        debayeredImg[3 * (y * width + x)] = 0xFFFF;
//        debayeredImg[3 * (y * width + x) + 1] = 0;
//        debayeredImg[3 * (y * width + x) + 2] = 0;
//      }
  }

  /* Upper right: R */
  if (x == width - 2 && y == 0)
  {
    r = bayeredImg[y * width + (x + 1)];
    g = (bayeredImg[y * width + x] + bayeredImg[(y + 1) * width + (x + 1)]) / 2;
    b = bayeredImg[(y + 1) * width + x];
  }
  else if (y == 0)
  {
    r = bayeredImg[y * width + (x + 1)];
    g = (bayeredImg[y * width + x] + bayeredImg[y * width + (x + 2)]
        + bayeredImg[(y + 1) * width + (x + 1)]) / 3;

    b =
        (bayeredImg[(y + 1) * width + x] + bayeredImg[(y + 1) * width + (x + 2)])
            / 2;
  }
  else if (x == width - 2)
  {
    r = bayeredImg[y * width + (x + 1)];
    g = (bayeredImg[y * width + x] + bayeredImg[(y - 1) * width + (x + 1)]
        + bayeredImg[(y + 1) * width + (x + 1)]) / 3;

    b = (bayeredImg[(y - 1) * width + x] + bayeredImg[(y + 1) * width + x]) / 2;
  }
  else
  {
    r = bayeredImg[y * width + (x + 1)];
    g = (bayeredImg[y * width + x] + bayeredImg[y * width + (x + 2)]
        + bayeredImg[(y - 1) * width + (x + 1)]
        + bayeredImg[(y + 1) * width + (x + 1)]) / 4;

    b = (bayeredImg[(y - 1) * width + x] + bayeredImg[(y - 1) * width + (x + 2)]
        + bayeredImg[(y + 1) * width + x]
        + bayeredImg[(y + 1) * width + (x + 2)]) / 4;
  }

  debayeredImg[4 * (y * width + (x + 1))] = r;
  debayeredImg[4 * (y * width + (x + 1)) + 1] = g;
  debayeredImg[4 * (y * width + (x + 1)) + 2] = b;
  debayeredImg[4 * (y * width + (x + 1)) + 3] = (uint16_t) -1;

  if (histogram != nullptr)
  {
    brightness = ((uint32_t) (r + r + r + b + g + g + g + g) >> 3) * hist_size
        >> hist_size_bits;
    atomicAdd(&histogram[brightness & ((1 << hist_size_bits) - 1)], 1);

    //
//      if ((brightness == ((1 << hist_size_bits) - 1)) && overexpose)
//      {
//        debayeredImg[3 * (y * width + (x+1))] = 0xFFFF;
//        debayeredImg[3 * (y * width + (x+1)) + 1] = 0;
//        debayeredImg[3 * (y * width + (x+1)) + 2] = 0;
//      }
  }

  /* Lower left: B */
  if (x == 0 && y == height - 2)
  {
    b = bayeredImg[(y + 1) * width + x];
    r = bayeredImg[y * width + (x + 1)];
    g = (bayeredImg[y * width + x] + bayeredImg[(y + 1) * width + (x + 1)]) / 2;
  }
  else if (x == 0)
  {
    b = bayeredImg[(y + 1) * width + x];
    r =
        (bayeredImg[y * width + (x + 1)] + bayeredImg[(y + 2) * width + (x + 1)])
            / 2;
    g = (bayeredImg[y * width + x] + bayeredImg[(y + 1) * width + (x + 1)]
        + bayeredImg[(y + 2) * width + x]) / 3;
  }
  else if (y == height - 2)
  {
    b = bayeredImg[(y + 1) * width + x];
    r = (bayeredImg[y * width + (x - 1)] + bayeredImg[y * width + (x + 1)]) / 2;
    g = (bayeredImg[y * width + x] + bayeredImg[(y + 1) * width + (x + 1)]
        + bayeredImg[(y + 1) * width + (x - 1)]) / 3;
  }
  else
  {
    b = bayeredImg[(y + 1) * width + x];
    r = (bayeredImg[y * width + (x - 1)] + bayeredImg[y * width + (x + 1)]
        + bayeredImg[(y + 2) * width + (x - 1)]
        + bayeredImg[(y + 2) * width + (x + 1)]) / 4;

    g = (bayeredImg[y * width + x] + bayeredImg[(y + 1) * width + (x + 1)]
        + bayeredImg[(y + 2) * width + x]
        + bayeredImg[(y + 1) * width + (x - 1)]) / 4;
  }

  debayeredImg[4 * ((y + 1) * width + x)] = r;
  debayeredImg[4 * ((y + 1) * width + x) + 1] = g;
  debayeredImg[4 * ((y + 1) * width + x) + 2] = b;
  debayeredImg[4 * ((y + 1) * width + x) + 3] = (uint16_t) -1;

  if (histogram != nullptr)
  {
    brightness = ((uint32_t) (r + r + r + b + g + g + g + g) >> 3) * hist_size
        >> hist_size_bits;
    atomicAdd(&histogram[brightness & ((1 << hist_size_bits) - 1)], 1);

//    //
//    if ((brightness == ((1 << hist_size_bits) - 1)) && overexpose)
//    {
//      debayeredImg[3 * ((y + 1) * width + x)] = 0xFFFF;
//      debayeredImg[3 * ((y + 1) * width + x) + 1] = 0;
//      debayeredImg[3 * ((y + 1) * width + x) + 2] = 0;
//    }
  }

  /* Lower right: C */
  if (x == width - 2 && y == height - 2)
  {
    g = bayeredImg[(y + 1) * width + (x + 1)];
    r = bayeredImg[y * width + (x + 1)];
    b = bayeredImg[(y + 1) * width + x];
  }
  else if (x == width - 2)
  {
    g = bayeredImg[(y + 1) * width + (x + 1)];
    r = (bayeredImg[y * width + (x + 1)] + bayeredImg[(y + 2) * width + (x + 1)]) / 2;
    b = bayeredImg[(y + 1) * width + x];
  }
  else if (y == height - 2)
  {
    g = bayeredImg[(y + 1) * width + (x + 1)];
    r = bayeredImg[y * width + (x + 1)];
    b = (bayeredImg[(y + 1) * width + x] + bayeredImg[(y + 1) * width + (x + 2)]) / 2;
  }
  else
  {
    g = bayeredImg[(y + 1) * width + (x + 1)];
    r = (bayeredImg[y * width + (x + 1)] + bayeredImg[(y + 2) * width + (x + 1)]) / 2;
    b = (bayeredImg[(y + 1) * width + x] + bayeredImg[(y + 1) * width + (x + 2)]) / 2;
  }

  debayeredImg[4 * ((y + 1) * width + (x + 1))] = r;
  debayeredImg[4 * ((y + 1) * width + (x + 1)) + 1] = g;
  debayeredImg[4 * ((y + 1) * width + (x + 1)) + 2] = b;
  debayeredImg[4 * ((y + 1) * width + (x + 1)) + 3] = (uint16_t) -1;

  if (histogram != nullptr)
  {
    brightness = ((uint32_t) (r + r + r + b + g + g + g + g) >> 3) * hist_size
        >> hist_size_bits;
    atomicAdd(&histogram[brightness & ((1 << hist_size_bits) - 1)], 1);

//    //
//    if ((brightness == ((1 << hist_size_bits) - 1)) && overexpose)
//    {
//      debayeredImg[3 * ((y + 1) * width + (x + 1))] = 0xFFFF;
//      debayeredImg[3 * ((y + 1) * width + (x + 1)) + 1] = 0;
//      debayeredImg[3 * ((y + 1) * width + (x + 1)) + 2] = 0;
//    }
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
 * \\fn void cudaMax_bl
 *
 * created on: Nov 22, 2019
 * author: daniel
 *
 */
__global__ void cudaMax_bl(uint32_t* org,uint32_t* max)
{
  const int tid = threadIdx.x + blockIdx.x * blockDim.x;
  max[tid] = org[tid];

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
 * \\class Debayer_Bilinear_Impl
 *
 * created on: Feb 19, 2020
 *
 */
class Debayer_Bilinear_Impl
{
friend Debayer_Bilinear;
public:
  Debayer_Bilinear_Impl()
  : _width(0), _height(0)
  , _thx(0),_thy(0)
  , _blkx(0),_blky(0)
  { }

  virtual ~Debayer_Bilinear_Impl() {}

          image::RawRGBPtr        ahd(image::RawRGBPtr img);

private:
  CudaPtr<uint16_t>               _img_buffer;
  CudaPtr<uint16_t>               _img_debayer_buffer;
  CudaPtr<uint32_t>               _histogram;
  CudaPtr<uint32_t>               _histogram_max;
  CudaPtr<uint32_t>               _small_histogram;

  uint16_t                        _width;
  uint16_t                        _height;
  int                             _thx,_thy;
  int                             _blkx,_blky;
};




/*
 * \\fn constructor Debayer_Bilinear::Debayer_Bilinear
 *
 * created on: Feb 14, 2020
 * author: daniel
 *
 */
Debayer_Bilinear::Debayer_Bilinear()
: _width(0)
, _height(0)
{
  _impl = new Debayer_Bilinear_Impl();
}

/*
 * \\fn Debayer_Bilinear::~Debayer_Bilinear
 *
 * created on: Feb 14, 2020
 * author: daniel
 *
 */
Debayer_Bilinear::~Debayer_Bilinear()
{
  delete _impl;
}

/*
 * \\fn image::RawRGBPtr Debayer_Bilinear::ahd
 *
 * created on: Feb 19, 2020
 * author: daniel
 *
 */
image::RawRGBPtr Debayer_Bilinear::ahd(image::RawRGBPtr raw)
{
  return _impl->ahd(raw);
}

/*
 * \\fn Debayer_Bilinear::consume
 *
 * created on: Feb 19, 2020
 * author: daniel
 *
 */
void Debayer_Bilinear::consume(image::ImageBox box)
{

}

/*
 * \\fn image::RawRGBPtr Debayer_Bilinear_Impl::ahd
 *
 * created on: Feb 19, 2020
 * author: daniel
 *
 */
image::RawRGBPtr Debayer_Bilinear_Impl::ahd(image::RawRGBPtr raw)
{
  if (!raw)
    return image::RawRGBPtr();

  size_t img_size = raw->width() * raw->height();
  if (!_img_buffer.put((uint16_t*)raw->bytes(), img_size))
    return image::RawRGBPtr();

  size_t debayer_img_size = img_size * 4; /* RGBA*/

  if (!_img_debayer_buffer || (_img_debayer_buffer.size() != debayer_img_size))
    _img_debayer_buffer = CudaPtr<uint16_t>(debayer_img_size);

  if (!_img_debayer_buffer)
    return image::RawRGBPtr();

  if (!_histogram || (_histogram.size() != BITS_PER_PIXEL))
    _histogram = CudaPtr<uint32_t>(BITS_PER_PIXEL);

  if (!_histogram_max || (_histogram_max.size() != BITS_PER_PIXEL))
    _histogram_max = CudaPtr<uint32_t>(BITS_PER_PIXEL);

  if (!_small_histogram || (_small_histogram.size() != SMALL_HIST_SIZE))
    _small_histogram = CudaPtr<uint32_t>(SMALL_HIST_SIZE);

  // Check, whether image dimensions have changed
  if ((_width != raw->width()) || (_height != raw->height()))
  {
    _width = raw->width();
    _height = raw->height();

    _thx = std::min(DEFAULT_NUMBER_OF_THREADS, (1 << __builtin_ctz(_width >> 1)));
    if (_thx == 0)
      _thx = 1;

    _thy = std::min(DEFAULT_NUMBER_OF_THREADS, (1 << __builtin_ctz(_height >> 1)));
    if (_thy == 0)
      _thy = 1;

    _blkx = (_width >> 1) / _thx;
    if (((_width >> 1) % _thx) != 0)
      _blkx++;

    _blky = (_height >> 1) / _thy;
    if (((_height >> 1) % _thy) != 0)
      _blky++;
  }

  _histogram.fill(0);
  _histogram_max.fill(0);
  _small_histogram.fill(0);

  int hist_size_bits = ((sizeof(unsigned long) * 8) - 1 - __builtin_clzl(_histogram.size()));

  cudaProfilerStart();

  dim3 threads(_thx,_thy);
  dim3 blocks(_blkx, _blky);

  runCudaDebayer<<<blocks,threads>>>(_img_buffer.ptr(),
                                     _img_debayer_buffer.ptr(),
                                     _width, _height,
                                     _histogram.ptr(),
                                     _histogram.size(),
                                     hist_size_bits);

  int thx = 64;
  while (_histogram.size() < thx)
    thx >>= 1;

  cudaMax_bl<<<_histogram.size() / thx, thx>>>(_histogram.ptr(), _histogram_max.ptr());

  reduceHistogram<<<_histogram.size() / thx, thx>>>(_histogram.ptr(),
                                                    _histogram.size(),
                                                    _small_histogram.ptr(),
                                                    _small_histogram.size());

  cudaProfilerStop();

  image::RawRGBPtr result(new image::RawRGB(raw->width(), raw->height(), raw->depth(), image::eRGBA));
  _img_debayer_buffer.get((uint16_t*)result->bytes(), debayer_img_size);

  image::HistPtr  full_hist(new image::Histogram);
  full_hist->_histogram.resize(_histogram.size());
  full_hist->_small_hist.resize(_small_histogram.size());

  _histogram.get((uint32_t*)full_hist->_histogram.data(), _histogram.size());
  _small_histogram.get(full_hist->_small_hist.data(), _small_histogram.size());
  _histogram_max.get(&full_hist->_max_value, 1);

  result->set_histogram(full_hist);

  return result;
}


} // jupiter
} // brt
