/*
 * debayer.cu
 *
 *  Created on: Feb 14, 2020
 *      Author: daniel
 */

#include "debayer.hpp"
#include "cuda_2d_mem.hpp"
#include "cuda_mem.hpp"

namespace brt
{
namespace jupiter
{

__constant__ double             _Xn = (0.950456);
__constant__ double             _Zn = (1.088754);

/*
 * \\class name
 *
 * created on: Feb 14, 2020
 *
 */
struct RGBA
{
  uint16_t                        _r;
  uint16_t                        _g;
  uint16_t                        _b;
  uint16_t                        _a;
};


/*
 * \\struct LAB
 *
 * created on: Feb 14, 2020
 *
 */
struct LAB
{
  double                          _L;
  double                          _a;
  double                          _b;

  __device__ void                 from(RGBA& rgba)
  {
    double X,Y,Z;

    // Matrix multiplication
    X = (0.412453 * static_cast<double>(rgba._r)  +
         0.357580 * static_cast<double>(rgba._g)  +
         0.180423 * static_cast<double>(rgba._b)) / _Xn;

    Y = (0.212671 * static_cast<double>(rgba._r) +
         0.715160 * static_cast<double>(rgba._g) +
         0.072169 * static_cast<double>(rgba._b));

    Z = (0.019334 * static_cast<double>(rgba._r) +
         0.119193 * static_cast<double>(rgba._g) +
         0.950227 * static_cast<double>(rgba._b)) / _Zn;

    auto adjust = [](double value)->double
    {
      return (value > 0.00856) ? pow(value,0.33333333333) : (7.787 * value + 0.1379310);
    };

    _L = (Y > 0.00856) ? (116.0 * pow(Y,0.33333333333) - 16.0) : 903.3 * Y;
    _a = 500.0 * (adjust(X) - adjust(Y));
    _b = 200.0 * (adjust(Y) - adjust(Z));
  }
};

/*
 * \\class Debayer_impl
 *
 * created on: Feb 14, 2020
 *
 */
class Debayer_impl
{
friend Debayer;
public:
  Debayer_impl()
  : _thx(0),_thy(0)
  , _blkx(0),_blky(0)
  { }

  virtual ~Debayer_impl() {}

          void                    init(size_t width,size_t height,size_t small_hits_size);
          image::RawRGBPtr        ahd(image::RawRGBPtr img);
private:

  Cuda2DPtr<uint16_t>             _raw;
  Cuda2DPtr<RGBA>                 _horiz;
  Cuda2DPtr<RGBA>                 _vert;
  Cuda2DPtr<RGBA>                 _result;

  Cuda2DPtr<LAB>                  _hlab;
  Cuda2DPtr<LAB>                  _vlab;

  CudaPtr<uint32_t>               _histogram;
  CudaPtr<uint32_t>               _histogram_max;
  CudaPtr<uint32_t>               _small_histogram;

  int                             _thx,_thy;
  int                             _blkx,_blky;
};

/*
 * \\fn void green_interpolate
 *
 * created on: Feb 11, 2020, 4:25:08 PM
 * author daniel
 *
 */
__global__ void green_interpolate(Cuda2DRef<uint16_t> raw,
                                  Cuda2DRef<RGBA> hr,
                                  Cuda2DRef<RGBA> vr)
{
  int origx = ((blockIdx.x * blockDim.x) + threadIdx.x) << 1;
  int origy = ((blockIdx.y * blockDim.y) + threadIdx.y) << 1;


  auto limit = [](int x,int a,int b)->int
  {
    int result = max(x,min(a,b));
    return min(result,max(a,b));
  };

  // C R
  // B C
  // (0,0) -> Clear
  int x = origx, y = origy;
  vr(x,y)._g = hr(x,y)._g = raw(x,y);
  vr(x,y)._a = hr(x,y)._a = (uint16_t)-1;

  ////////////////////////////////////////////////
  // (1,0) -> Red
  x = origx + 1;
  y = origy;

  int value = (((raw(x-1,y) + raw(x,y) + raw(x+1,y)) * 2) - raw(x - 2,y) - raw(x + 2,y)) >> 2;
  hr(x,y)._g = limit(value,raw(x - 1,y),raw(x + 1,y));

  value = (((raw(x,y-1) + raw(x,y) + raw(x,y+1)) * 2) - raw(x,y-2) - raw(x,y+2)) >> 2;
  vr(x,y)._g = limit(value,raw(x,y-1),raw(x,y+1));

  vr(x,y)._a = hr(x,y)._a = (uint16_t)-1;

  ////////////////////////////////////////////////
  // (0,1) -> Blue
  x = origx;
  y = origy + 1;

  value = (((raw(x-1,y) + raw(x,y) + raw(x+1,y)) * 2) - raw(x - 2,y) - raw(x + 2,y)) >> 2;
  hr(x,y)._g = limit(value,raw(x - 1,y),raw(x + 1,y));

  value = (((raw(x,y-1) + raw(x,y) + raw(x,y+1)) * 2) - raw(x,y-2) - raw(x,y+2)) >> 2;
  vr(x,y)._g = limit(value,raw(x,y-1),raw(x,y+1));

  vr(x,y)._a = hr(x,y)._a = (uint16_t)-1;

  ////////////////////////////////////////////////
  // (1,1) -> Clear
  x = origx + 1;
  y = origy + 1;

  vr(x,y)._g = hr(x,y)._g = raw(x,y);
  vr(x,y)._a = hr(x,y)._a = (uint16_t)-1;
}

/*
 * \\fn void blue_red_interpolate
 *
 * created on: Feb 12, 2020, 11:35:37 AM
 * author daniel
 *
 */
__global__ void blue_red_interpolate( Cuda2DRef<uint16_t> raw,
                                      Cuda2DRef<RGBA> hr,
                                      Cuda2DRef<RGBA> vr,
                                      Cuda2DRef<LAB> hl,
                                      Cuda2DRef<LAB> vl)
{
  int origx = ((blockIdx.x * blockDim.x) + threadIdx.x) << 1;
  int origy = ((blockIdx.y * blockDim.y) + threadIdx.y) << 1;

  auto limit = [](int x,int a,int b)->int
  {
    int result = max(x,min(a,b));
    return min(result,max(a,b));
  };

  // C R
  // B C

  ////////////////////////////////////////////////
  // (0,0) -> ClearRead
  int x = origx, y = origy;

  // Horizontal
  int value = hr(x,y)._g + ((raw(x-1,y) - hr(x-1,y)._g + raw(x+1,y) - hr(x+1,y)._g) >> 1);
  hr(x,y)._r = limit(value, 0, ((1 << 16) - 1));

  value = hr(x,y)._g  + ((raw(x,y-1) - hr(x,y-1)._g + raw(x,y+1) - hr(x,y+1)._g) >> 1);
  hr(x,y)._b = limit(value, 0, ((1 << 16) - 1));
  hl(x,y).from(hr(x,y));

  // Vertical
  value = vr(x,y)._g + ((raw(x-1,y) - vr(x-1,y)._g + raw(x+1,y) - vr(x+1,y)._g) >> 1);
  vr(x,y)._r = limit(value, 0, ((1 << 16) - 1));

  value = vr(x,y)._g + ((raw(x,y-1) - vr(x,y-1)._g + raw(x,y+1) - vr(x,y+1)._g) >> 1);
  vr(x,y)._b = limit(value, 0, ((1 << 16) - 1));
  vl(x,y).from(vr(x,y));

  ////////////////////////////////////////////////
  // (1,0) -> Red
  x = origx + 1;
  y = origy;

  hr(x,y)._r = vr(x,y)._r = raw(x,y);

  value = hr(x,y)._g + ((raw(x-1,y-1) - hr(x-1,y-1)._g +
                         raw(x-1,y+1) - hr(x-1,y+1)._g +
                         raw(x+1,y-1) - hr(x+1,y-1)._g +
                         raw(x+1,y+1) - hr(x+1,y+1)._g) >> 2);
  hr(x,y)._b = limit(value, 0, ((1 << 16) - 1));
  hl(x,y).from(hr(x,y));

  value = vr(x,y)._g + ((raw(x-1,y-1) - vr(x-1,y-1)._g +
                         raw(x-1,y+1) - vr(x-1,y+1)._g +
                         raw(x+1,y-1) - vr(x+1,y-1)._g +
                         raw(x+1,y+1) - vr(x+1,y+1)._g) >> 2);
  vr(x,y)._b = limit(value, 0, ((1 << 16) - 1));
  vl(x,y).from(vr(x,y));

  ////////////////////////////////////////////////
  // (0,1) -> Blue
  x = origx;
  y = origy + 1;

  hr(x,y)._b = vr(x,y)._b = raw(x,y);

  value = hr(x,y)._g + ((raw(x-1,y-1) - hr(x-1,y-1)._g +
                         raw(x-1,y+1) - hr(x-1,y+1)._g +
                         raw(x+1,y-1) - hr(x+1,y-1)._g +
                         raw(x+1,y+1) - hr(x+1,y+1)._g) >> 2);
  hr(x,y)._r = limit(value, 0, ((1 << 16) - 1));
  hl(x,y).from(hr(x,y));

  value = vr(x,y)._g + ((raw(x-1,y-1) - vr(x-1,y-1)._g +
                         raw(x-1,y+1) - vr(x-1,y+1)._g +
                         raw(x+1,y-1) - vr(x+1,y-1)._g +
                         raw(x+1,y+1) - vr(x+1,y+1)._g) >> 2);
  vr(x,y)._r = limit(value, 0, ((1 << 16) - 1));
  vl(x,y).from(vr(x,y));

  ////////////////////////////////////////////////
  // (1,1) -> ClearBlue
  x = origx + 1;
  y = origy + 1;

  // Horizontal
  value = hr(x,y)._g + ((raw(x-1,y) - hr(x-1,y)._g + raw(x+1,y) - hr(x+1,y)._g) >> 1);
  hr(x,y)._b = limit(value, 0, ((1 << 16) - 1));

  value = hr(x,y)._g + ((raw(x,y-1) - hr(x,y-1)._g + raw(x,y+1) - hr(x,y+1)._g) >> 1);
  hr(x,y)._r = limit(value, 0, ((1 << 16) - 1));
  hl(x,y).from(hr(x,y));

  // Vertical
  value = vr(x,y)._g + ((raw(x-1,y) - vr(x-1,y)._g + raw(x+1,y) - vr(x+1,y)._g) >> 1);
  vr(x,y)._b = limit(value, 0, ((1 << 16) - 1));

  value = vr(x,y)._g + ((raw(x,y-1) - vr(x,y-1)._g + raw(x,y+1) - vr(x,y+1)._g) >> 1);
  vr(x,y)._r = limit(value, 0, ((1 << 16) - 1));
  vl(x,y).from(vr(x,y));
}


/*
 * \\fn void misguidance_color_artifacts
 *
 * created on: Feb 12, 2020, 3:21:22 PM
 * author daniel
 *
 */
__global__ void misguidance_color_artifacts(Cuda2DRef<RGBA> rst,
                                            Cuda2DRef<RGBA> hr,
                                            Cuda2DRef<RGBA> vr,
                                            Cuda2DRef<LAB> hl,
                                            Cuda2DRef<LAB> vl,
                                            uint32_t* histogram, size_t histogram_size,
                                            uint32_t* small_histogram, size_t small_histogram_size)
{
  int hist_size_bits = ((sizeof(unsigned int) * 8) - 1 -  __clz(histogram_size));
  int small_hist_size_bits = ((sizeof(unsigned int) * 8) - 1 -  __clz(small_histogram_size));

  int x = ((blockIdx.x * blockDim.x) + threadIdx.x);
  int y = ((blockIdx.y * blockDim.y) + threadIdx.y);

  auto sqr=[](double a)->double { return a*a; };

  double lv[2],lh[2],cv[2],ch[2];
  int hh = 0,hv = 0;

  lh[0] = fabs(hl(x,y)._L - hl(x-1,y)._L);
  lh[1] = fabs(hl(x,y)._L - hl(x+1,y)._L);

  lv[0] = fabs(vl(x,y)._L - vl(x,y-1)._L);
  lv[1] = fabs(vl(x,y)._L - vl(x,y+1)._L);

  ch[0] = sqr(hl(x,y)._a - hl(x-1,y)._a) +
          sqr(hl(x,y)._b - hl(x-1,y)._b);

  ch[1] = sqr(hl(x,y)._a - hl(x+1,y)._a) +
          sqr(hl(x,y)._b - hl(x+1,y)._b);

  cv[0] = sqr(vl(x,y)._a - vl(x,y-1)._a) +
          sqr(vl(x,y)._b - vl(x,y-1)._b);

  cv[1] = sqr(vl(x,y)._a - vl(x,y+1)._a) +
          sqr(vl(x,y)._b - vl(x,y+1)._b);

  double eps_l = min(max(lh[0],lh[1]),max(lv[0],lv[1]));
  double eps_c = min(max(ch[0],ch[1]),max(cv[0],cv[1]));

  for (size_t index = 0; index < 2; index++)
  {
    if ((lh[index] <= eps_l) && (ch[index] <= eps_c))
      hh++;

    if ((lv[index] <= eps_l) && (cv[index] <= eps_c))
      hv++;
  }

  uint32_t r = 0,g = 0,b = 0;
  RGBA *lf, *rt;

  lf = (hh > hv)? &hr(x,y) : &vr(x,y);
  rt = (hv > hh)? &vr(x,y) : &hr(x,y);

  r = rst(x,y)._r = (lf->_r + rt->_r) >> 1;
  g = rst(x,y)._g = (lf->_g + rt->_g) >> 1;
  b = rst(x,y)._b = (lf->_b + rt->_b) >> 1;
  rst(x,y)._a = (uint16_t)-1;

  uint32_t brightness = ((r+r+r+b+g+g+g+g) >> 3) * small_histogram_size >> small_hist_size_bits;
  atomicAdd(&small_histogram[brightness & ((1 << small_hist_size_bits) - 1)], 1);

  brightness = ((r+r+r+b+g+g+g+g) >> 3) * histogram_size >> hist_size_bits;
  atomicAdd(&histogram[brightness & ((1 << hist_size_bits) - 1)], 1);
}


/*
 * \\fn void cudaMax
 *
 * created on: Nov 22, 2019
 * author: daniel
 *
 */
__global__ void cudaMax(uint32_t *org, uint32_t *max)
{
  const int tid = threadIdx.x + blockIdx.x * blockDim.x;
  max[tid] = org[tid];

  auto step_size = 1;
  int number_of_threads = gridDim.x * blockDim.x;

  __syncthreads();

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
 * \\fn constructor Debayer::Debayer
 *
 * created on: Feb 14, 2020
 * author: daniel
 *
 */
Debayer::Debayer()
: _width(0)
, _height(0)
{
  _impl = new Debayer_impl();
}

/*
 * \\fn Debayer::~Debayer
 *
 * created on: Feb 14, 2020
 * author: daniel
 *
 */
Debayer::~Debayer()
{
  delete _impl;
}


/*
 * \\fn Debayer_impl::init
 *
 * created on: Feb 14, 2020
 * author: daniel
 *
 */
void Debayer_impl::init(size_t width,size_t height,size_t small_hits_size)
{
  _raw = Cuda2DPtr<uint16_t>(width,height,2,2);  _raw.fill(0);

  _horiz = Cuda2DPtr<RGBA>(width,height,2,2); _horiz.fill(0);
  _vert = Cuda2DPtr<RGBA>(width,height,2,2); _vert.fill(0);
  _result = Cuda2DPtr<RGBA>(width,height); _result.fill(0);

  _hlab = Cuda2DPtr<LAB>(width,height,2,2); _hlab.fill(0);
  _vlab = Cuda2DPtr<LAB>(width,height,2,2); _vlab.fill(0);

  _histogram = CudaPtr<uint32_t>(1 << 16); _histogram.fill(0);
  _histogram_max = CudaPtr<uint32_t>(1 << 16); _histogram_max.fill(0);
  _small_histogram = CudaPtr<uint32_t>(small_hits_size); _small_histogram.fill(0);

  _thx = std::min(DEFAULT_NUMBER_OF_THREADS, (1 << __builtin_ctz(width)));
  if (_thx == 0)
    _thx = 1;

  _thy = std::min(DEFAULT_NUMBER_OF_THREADS, (1 << __builtin_ctz(height)));
  if (_thy == 0)
    _thy = 1;

  _blkx = width / _thx;
  if ((width % _thx) != 0)
    _blkx++;

  _blky = height / _thy;
  if ((height % _thy) != 0)
    _blky++;
}
/*
 * \\fn Debayer::init
 *
 * created on: Feb 14, 2020
 * author: daniel
 *
 */
bool Debayer::init(size_t width,size_t height,size_t small_hits_size)
{
  if ((_width == width) && (_height == height))
    return true;

  _impl->init(width, height, small_hits_size);
  return true;
}

/*
 * \\fn image::RawRGBPtr Debayer::ahd
 *
 * created on: Feb 14, 2020
 * author: daniel
 *
 */
image::RawRGBPtr Debayer_impl::ahd(image::RawRGBPtr img)
{
  if (!img)
    return image::RawRGBPtr();

  _raw.put((uint16_t*)img->bytes());

  dim3 threads(_thx >> 1,_thy >> 1);
  dim3 blocks(_blkx, _blky);

  green_interpolate<<<blocks,threads>>>(_raw.ref(), _horiz.ref(), _vert.ref());
  blue_red_interpolate<<<blocks,threads>>>(_raw.ref(), _horiz.ref(), _vert.ref(), _hlab.ref(), _vlab.ref());


  _histogram_max.fill(0);
  _small_histogram.fill(0);

  dim3 threads2(_thx,_thy);
  misguidance_color_artifacts<<<blocks,threads2>>>(_result.ref(), _horiz.ref(),
                      _vert.ref(), _hlab.ref(), _vlab.ref(),
                      _histogram.ptr(), _histogram.size(),
                      _small_histogram.ptr(), _small_histogram.size());

  int thx = 64;
  while (_histogram.size() < thx)
    thx >>= 1;

  cudaMax<<<_histogram.size() / thx, thx>>>(_histogram.ptr(), _histogram_max.ptr());

  image::RawRGBPtr result(new image::RawRGB(img->width(), img->height(), img->depth(), image::eRGBA));
  _result.get((RGBA*)result->bytes());

  image::HistPtr  full_hist(new image::Histogram);
  full_hist->_histogram.resize(_histogram.size());
  full_hist->_small_hist.resize(_small_histogram.size());

  _histogram.get(full_hist->_histogram.data(), full_hist->_histogram.size());
  _small_histogram.get(full_hist->_small_hist.data(), full_hist->_small_hist.size());
  _histogram_max.get(&full_hist->_max_value, 1);

  result->set_histogram(full_hist);


  return result;
}

/*
 * \\fn image::RawRGBPtr Debayer::ahd
 *
 * created on: Feb 14, 2020
 * author: daniel
 *
 */
image::RawRGBPtr Debayer::ahd(image::RawRGBPtr img)
{
  return _impl->ahd(img);
}

/*
 * \\fn void Debayer::consume
 *
 * created on: Feb 14, 2020
 * author: daniel
 *
 */
void Debayer::consume(image::ImageBox box)
{
  for (image::ImagePtr img : box)
  {
    image::RawRGBPtr result = _impl->ahd(img->get_bits());
    if (result)
      ImageProducer::consume(image::ImageBox(result));
  }
}


} /* namespace jupiter */
} /* namespace brt */
