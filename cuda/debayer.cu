/**
 *
 *
 *
 */
#include "debayer.hpp"
#include "image_processor.hpp"

namespace brt {
namespace jupiter {


__constant__ size_t color_map[image::eNumTypes][image::NumColors] =
{
  //            Blue = 0, Green = 1, Red = 2,  Alpha = 3, Bayer = 4
  /*eNone = 0*/  {  0,       0,          0,        0,          0     },
  /*eBayer = 1*/ {  0,       0,          0,        0,          0     },
  /*eRGB = 2*/   {  0,       1,          2,        0,          0     },
  /*eBGR = 3*/   {  2,       1,          0,        0,          0     },
  /*eRGBA= 4*/   {  0,       1,          2,        3,          0     },
  /*eBGRA= 5*/   {  2,       1,          0,        3,          0     },
};

size_t type_size[image::eNumTypes] =
{
  /*eNone = 0*/  1,
  /*eBayer = 1*/ 1,
  /*eRGB = 2*/   3,
  /*eBGR = 3*/   3,
  /*eRGBA= 4*/   4,
  /*eBGRA= 5*/   4,
};

__constant__ double             _Xn = (0.950456);
__constant__ double             _Zn = (1.088754);


class MemPtr
{
public:
  __device__ MemPtr(cudaPitchedPtr mem,int element_size,int xoffset = 2,int yoffset = 2)
  : _mem(mem), _el_size(element_size), _xo(xoffset), _yo(yoffset)
  {   }

protected:
  __device__ uint8_t* ptr(int x,int y)
  {
    return ((uint8_t*)_mem.ptr) + ((x + _xo) * _el_size + (y + _yo) * _mem.pitch);
  }

private:
  cudaPitchedPtr                  _mem;
  int                             _el_size;
  int                             _xo,_yo;
};


/*
 * \\class Bayer
 *
 * created on: Feb 12, 2020, 3:08:28 PM
 * author daniel
 *
 */
class Bayer : public MemPtr
{
public:
  __device__ Bayer(cudaPitchedPtr mem,int xoffset = 2,int yoffset = 2)
  : MemPtr(mem,sizeof(uint16_t), xoffset, yoffset) { }

  __device__ uint16_t& operator()(int x,int y)
  {
    return *(uint16_t*)ptr(x,y);
  }
};


/*
 * \\class Pix
 *
 * created on: Feb 12, 2020, 3:08:36 PM
 * author daniel
 *
 */
class Pix : public MemPtr
{
public:
  __device__ Pix(cudaPitchedPtr mem,image::PixelType type,int typesize,int xoffset = 2,int yoffset = 2)
  : MemPtr(mem,typesize * sizeof(uint16_t), xoffset, yoffset)
  , _type(type)
  { }

  __device__ uint16_t& operator()(int x,int y,image::Color cl)
  {
    return *(uint16_t*)(ptr(x,y) + color_map[_type][cl] * sizeof(uint16_t));
  }

private:
  image::PixelType                _type;
};


/*
 * \\class LAB
 *
 * created on: Feb 12, 2020, 3:10:01 PM
 * author daniel
 *
 */
class LAB
{
public:
  __device__ LAB() : _L(0.0),_a(0.0),_b(0.0) {}

  __device__ void   fromRGB(uint16_t r,uint16_t g,uint16_t b)
  {
    double X,Y,Z;

    // Matrix multiplication
    X = (0.412453 * static_cast<double>(r)  +
         0.357580 * static_cast<double>(g)  +
         0.180423 * static_cast<double>(b)) / _Xn;

    Y = (0.212671 * static_cast<double>(r) +
         0.715160 * static_cast<double>(g) +
         0.072169 * static_cast<double>(b));

    Z = (0.019334 * static_cast<double>(r) +
         0.119193 * static_cast<double>(g) +
         0.950227 * static_cast<double>(b)) / _Zn;

    auto adjust = [](double value)->double
    {
      return (value > 0.00856) ? pow(value,0.33333333333) : (7.787 * value + 0.1379310);
    };

    _L = (Y > 0.00856) ? (116.0 * pow(Y,0.33333333333) - 16.0) : 903.3 * Y;
    _a = 500.0 * (adjust(X) - adjust(Y));
    _b = 200.0 * (adjust(Y) - adjust(Z));
  }

  __device__ double               L() const { return _L; }
  __device__ double               a() const { return _a; }
  __device__ double               b() const { return _b; }

private:
  double                          _L;
  double                          _a;
  double                          _b;
};


/*
 * \\class Lab
 *
 * created on: Feb 12, 2020, 3:17:40 PM
 * author daniel
 *
 */
class Lab : public MemPtr
{
public:
  __device__ Lab(cudaPitchedPtr mem,int xoffset = 2,int yoffset = 2)
  : MemPtr(mem,sizeof(LAB), xoffset, yoffset)
  { }

  __device__ LAB& operator()(int x,int y)
  {
    return *(LAB*) ptr(x,y);
  }
};


/*
 * \\fn void green_interpolate
 *
 * created on: Feb 11, 2020, 4:25:08 PM
 * author daniel
 *
 */
__global__ void green_interpolate(image::PixelType type, int typesize,
                                  cudaPitchedPtr raw_image,
                                  cudaPitchedPtr horiz_image,
                                  cudaPitchedPtr vertical_image)
{
  int origx = ((blockIdx.x * blockDim.x) + threadIdx.x) << 1;
  int origy = ((blockIdx.y * blockDim.y) + threadIdx.y) << 1;

  Bayer raw(raw_image);
  Pix hr(horiz_image,type,typesize);
  Pix vr(vertical_image,type,typesize);

  auto limit = [](int x,int a,int b)->int
  {
    int result = max(x,min(a,b));
    return min(result,max(a,b));
  };

  // C R
  // B C
  // (0,0) -> Clear
  int x = origx, y = origy;
  vr(x,y,image::Green) = hr(x,y,image::Green) = raw(x,y);
  vr(x,y,image::Alpha) = hr(x,y,image::Alpha) = (uint16_t)-1;

  ////////////////////////////////////////////////
  // (1,0) -> Red
  x = origx + 1;
  y = origy;

  int value = (((raw(x-1,y) + raw(x,y) + raw(x+1,y)) * 2) - raw(x - 2,y) - raw(x + 2,y)) >> 2;
  hr(x,y,image::Green) = limit(value,raw(x - 1,y),raw(x + 1,y));

  value = (((raw(x,y-1) + raw(x,y) + raw(x,y+1)) * 2) - raw(x,y-2) - raw(x,y+2)) >> 2;
  vr(x,y,image::Green) = limit(value,raw(x,y-1),raw(x,y+1));

  vr(x,y,image::Alpha) = hr(x,y,image::Alpha) = (uint16_t)-1;

  ////////////////////////////////////////////////
  // (0,1) -> Blue
  x = origx;
  y = origy + 1;

  value = (((raw(x-1,y) + raw(x,y) + raw(x+1,y)) * 2) - raw(x - 2,y) - raw(x + 2,y)) >> 2;
  hr(x,y,image::Green) = limit(value,raw(x - 1,y),raw(x + 1,y));

  value = (((raw(x,y-1) + raw(x,y) + raw(x,y+1)) * 2) - raw(x,y-2) - raw(x,y+2)) >> 2;
  vr(x,y,image::Green) = limit(value,raw(x,y-1),raw(x,y+1));

  vr(x,y,image::Alpha) = hr(x,y,image::Alpha) = (uint16_t)-1;

  ////////////////////////////////////////////////
  // (1,1) -> Clear
  x = origx + 1;
  y = origy + 1;

  vr(x,y,image::Green) = hr(x,y,image::Green) = raw(x,y);
  vr(x,y,image::Alpha) = hr(x,y,image::Alpha) = (uint16_t)-1;
}


/*
 * \\fn void blue_red_interpolate
 *
 * created on: Feb 12, 2020, 11:35:37 AM
 * author daniel
 *
 */
__global__ void blue_red_interpolate(image::PixelType type, int typesize,
                                      cudaPitchedPtr raw_image,
                                      cudaPitchedPtr horiz_image,
                                      cudaPitchedPtr vertical_image,
                                      cudaPitchedPtr hlab,
                                      cudaPitchedPtr vlab)
{
  int origx = ((blockIdx.x * blockDim.x) + threadIdx.x) << 1;
  int origy = ((blockIdx.y * blockDim.y) + threadIdx.y) << 1;

  Bayer raw(raw_image);
  Pix hr(horiz_image,type,typesize);
  Pix vr(vertical_image,type,typesize);

  Lab hl(hlab);
  Lab vl(vlab);


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
  int value = hr(x,y,image::Green) + ((raw(x-1,y) - hr(x-1,y,image::Green) + raw(x+1,y) - hr(x+1,y,image::Green)) >> 1);
  hr(x,y,image::Red) = limit(value, 0, ((1 << 16) - 1));

  value = hr(x,y,image::Green) + ((raw(x,y-1) - hr(x,y-1,image::Green) + raw(x,y+1) - hr(x,y+1,image::Green)) >> 1);
  hr(x,y,image::Blue) = limit(value, 0, ((1 << 16) - 1));
  hl(x,y).fromRGB(hr(x,y,image::Red),hr(x,y,image::Green),hr(x,y,image::Blue));

  // Vertical
  value = vr(x,y,image::Green) + ((raw(x-1,y) - vr(x-1,y,image::Green) + raw(x+1,y) - vr(x+1,y,image::Green)) >> 1);
  vr(x,y,image::Red) = limit(value, 0, ((1 << 16) - 1));

  value = vr(x,y,image::Green) + ((raw(x,y-1) - vr(x,y-1,image::Green) + raw(x,y+1) - vr(x,y+1,image::Green)) >> 1);
  vr(x,y,image::Blue) = limit(value, 0, ((1 << 16) - 1));
  vl(x,y).fromRGB(vr(x,y,image::Red),vr(x,y,image::Green),vr(x,y,image::Blue));

  ////////////////////////////////////////////////
  // (1,0) -> Red
  x = origx + 1;
  y = origy;

  hr(x,y,image::Red) = vr(x,y,image::Red) = raw(x,y);

  value = hr(x,y,image::Green) + ((raw(x-1,y-1) - hr(x-1,y-1,image::Green) +
                                   raw(x-1,y+1) - hr(x-1,y+1,image::Green) +
                                   raw(x+1,y-1) - hr(x+1,y-1,image::Green) +
                                   raw(x+1,y+1) - hr(x+1,y+1,image::Green)) >> 2);
  hr(x,y,image::Blue) = limit(value, 0, ((1 << 16) - 1));
  hl(x,y).fromRGB(hr(x,y,image::Red),hr(x,y,image::Green),hr(x,y,image::Blue));

  value = vr(x,y,image::Green) + ((raw(x-1,y-1) - vr(x-1,y-1,image::Green) +
                                   raw(x-1,y+1) - vr(x-1,y+1,image::Green) +
                                   raw(x+1,y-1) - vr(x+1,y-1,image::Green) +
                                   raw(x+1,y+1) - vr(x+1,y+1,image::Green)) >> 2);
  vr(x,y,image::Blue) = limit(value, 0, ((1 << 16) - 1));
  vl(x,y).fromRGB(vr(x,y,image::Red),vr(x,y,image::Green),vr(x,y,image::Blue));

  ////////////////////////////////////////////////
  // (0,1) -> Blue
  x = origx;
  y = origy + 1;

  hr(x,y,image::Blue) = vr(x,y,image::Blue) = raw(x,y);

  value = hr(x,y,image::Green) + ((raw(x-1,y-1) - hr(x-1,y-1,image::Green) +
                                   raw(x-1,y+1) - hr(x-1,y+1,image::Green) +
                                   raw(x+1,y-1) - hr(x+1,y-1,image::Green) +
                                   raw(x+1,y+1) - hr(x+1,y+1,image::Green)) >> 2);
  hr(x,y,image::Red) = limit(value, 0, ((1 << 16) - 1));
  hl(x,y).fromRGB(hr(x,y,image::Red),hr(x,y,image::Green),hr(x,y,image::Blue));

  value = vr(x,y,image::Green) + ((raw(x-1,y-1) - vr(x-1,y-1,image::Green) +
                                   raw(x-1,y+1) - vr(x-1,y+1,image::Green) +
                                   raw(x+1,y-1) - vr(x+1,y-1,image::Green) +
                                   raw(x+1,y+1) - vr(x+1,y+1,image::Green)) >> 2);
  vr(x,y,image::Red) = limit(value, 0, ((1 << 16) - 1));
  vl(x,y).fromRGB(vr(x,y,image::Red),vr(x,y,image::Green),vr(x,y,image::Blue));

  ////////////////////////////////////////////////
  // (1,1) -> ClearBlue
  x = origx + 1;
  y = origy + 1;

  // Horizontal
  value = hr(x,y,image::Green) + ((raw(x-1,y) - hr(x-1,y,image::Green) + raw(x+1,y) - hr(x+1,y,image::Green)) >> 1);
  hr(x,y,image::Blue) = limit(value, 0, ((1 << 16) - 1));

  value = hr(x,y,image::Green) + ((raw(x,y-1) - hr(x,y-1,image::Green) + raw(x,y+1) - hr(x,y+1,image::Green)) >> 1);
  hr(x,y,image::Red) = limit(value, 0, ((1 << 16) - 1));
  hl(x,y).fromRGB(hr(x,y,image::Red),hr(x,y,image::Green),hr(x,y,image::Blue));

  // Vertical
  value = vr(x,y,image::Green) + ((raw(x-1,y) - vr(x-1,y,image::Green) + raw(x+1,y) - vr(x+1,y,image::Green)) >> 1);
  vr(x,y,image::Blue) = limit(value, 0, ((1 << 16) - 1));

  value = vr(x,y,image::Green) + ((raw(x,y-1) - vr(x,y-1,image::Green) + raw(x,y+1) - vr(x,y+1,image::Green)) >> 1);
  vr(x,y,image::Red) = limit(value, 0, ((1 << 16) - 1));
  vl(x,y).fromRGB(vr(x,y,image::Red),vr(x,y,image::Green),vr(x,y,image::Blue));
}

/*
 * \\fn void misguidance_color_artifacts
 *
 * created on: Feb 12, 2020, 3:21:22 PM
 * author daniel
 *
 */
__global__ void misguidance_color_artifacts(image::PixelType type, int typesize,
                                            cudaPitchedPtr result_image,
                                            cudaPitchedPtr horiz_image,
                                            cudaPitchedPtr vertical_image,
                                            cudaPitchedPtr hlab,
                                            cudaPitchedPtr vlab,
                                            uint32_t* histogram,uint32_t histogram_size,
                                            uint32_t* small_histogram,uint32_t small_histogram_size)
{
  int hist_size_bits = ((sizeof(unsigned int) * 8) - 1 -  __clz(histogram_size));
  int small_hist_size_bits = ((sizeof(unsigned int) * 8) - 1 -  __clz(small_histogram_size));

  int x = ((blockIdx.x * blockDim.x) + threadIdx.x);
  int y = ((blockIdx.y * blockDim.y) + threadIdx.y);

  Pix rst(result_image,type,typesize,0,0);
  Pix hr(horiz_image,type,typesize);
  Pix vr(vertical_image,type,typesize);

  Lab hl(hlab);
  Lab vl(vlab);

  auto sqr=[](double a)->double { return a*a; };

  double lv[2],lh[2],cv[2],ch[2];
  int hh = 0,hv = 0;

  lh[0] = fabs(hl(x,y).L() - hl(x-1,y).L());
  lh[1] = fabs(hl(x,y).L() - hl(x+1,y).L());

  lv[0] = fabs(vl(x,y).L() - vl(x,y-1).L());
  lv[1] = fabs(vl(x,y).L() - vl(x,y+1).L());

  ch[0] = sqr(hl(x,y).a() - hl(x-1,y).a()) +
          sqr(hl(x,y).b() - hl(x-1,y).b());

  ch[1] = sqr(hl(x,y).a() - hl(x+1,y).a()) +
          sqr(hl(x,y).b() - hl(x+1,y).b());

  cv[0] = sqr(vl(x,y).a() - vl(x,y-1).a()) +
          sqr(vl(x,y).b() - vl(x,y-1).b());

  cv[1] = sqr(vl(x,y).a() - vl(x,y+1).a()) +
          sqr(vl(x,y).b() - vl(x,y+1).b());

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
  if (hh > hv)
  {
    r = rst(x,y,image::Red) = hr(x,y,image::Red);
    g = rst(x,y,image::Green) = hr(x,y,image::Green);
    b = rst(x,y,image::Blue) = hr(x,y,image::Blue);
    rst(x,y,image::Alpha) = hr(x,y,image::Alpha);
  }
  else if (hv > hh)
  {
    r = rst(x,y,image::Red) = vr(x,y,image::Red);
    g = rst(x,y,image::Green) = vr(x,y,image::Green);
    b = rst(x,y,image::Blue) = vr(x,y,image::Blue);
    rst(x,y,image::Alpha) = vr(x,y,image::Alpha);
  }
  else //if (hv == hh)
  {
    r = rst(x,y,image::Red) = (hr(x,y,image::Red) + vr(x,y,image::Red)) >> 1;
    g = rst(x,y,image::Green) = (hr(x,y,image::Green) + vr(x,y,image::Green)) >> 1;
    b = rst(x,y,image::Blue) = (hr(x,y,image::Blue) + vr(x,y,image::Blue)) >> 1;
    rst(x,y,image::Alpha) = (uint16_t)-1;
  }

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
 * \\class CudaImage
 *
 * created on: Feb 11, 2020
 *
 */
class Debayer_Impl
{
friend Debayer;
public:
  Debayer_Impl();
  virtual ~Debayer_Impl();

          bool                    init(size_t width,size_t height,image::PixelType,size_t small_hits_size);
          image::RawRGBPtr        ahd(image::RawRGBPtr);
private:
          void                    free();

private:
  cudaPitchedPtr                  _raw_image;
  cudaPitchedPtr                  _horiz_image;
  cudaPitchedPtr                  _vertical_image;
  cudaPitchedPtr                  _result_image;
  cudaPitchedPtr                  _hlab;
  cudaPitchedPtr                  _vlab;

  void*                           _histogram;
  void*                           _histogram_max;
  size_t                          _histogram_size;

  void*                           _small_histogram;
  size_t                          _small_histogram_size;

  size_t                          _width;
  size_t                          _height;
  image::PixelType                _type;

  int                             _thx,_thy;
  int                             _blkx,_blky;
};

/*
 * \\fn Constructor Debayer_Impl::Debayer_Impl
 *
 * created on: Feb 11, 2020, 3:05:08 PM
 * author daniel
 *
 */
Debayer_Impl::Debayer_Impl()
: _raw_image{nullptr,0,0,0}
, _horiz_image{nullptr,0,0,0}
, _vertical_image{nullptr,0,0,0}
, _result_image{nullptr,0,0,0}
, _hlab{nullptr,0,0,0}
, _vlab{nullptr,0,0,0}
, _histogram(nullptr)
, _histogram_max(nullptr)
, _histogram_size(0)
, _small_histogram(nullptr)
, _small_histogram_size(0)
, _width(0)
, _height(0)
, _type(image::eNone)
, _thx(0)
, _thy(0)
, _blkx(0)
, _blky(0)
{
}

/*
 * \\fn Destructor Debayer_Impl::~Debayer_Impl
 *
 * created on: Feb 11, 2020, 3:05:58 PM
 * author daniel
 *
 */
Debayer_Impl::~Debayer_Impl()
{
  free();
}

/*
 * \\fn bool Debayer_Impl::init
 *
 * created on: Feb 11, 2020, 3:41:45 PM
 * author daniel
 *
 */
bool Debayer_Impl::init(size_t width,size_t height,image::PixelType type,size_t small_hits_size)
{
  if ((_width == width) && (_height == height) && (type_size[type] == type_size[_type]))
    return true;

  free();

  _width = width;
  _height = height;
  _type = type;
  _small_histogram_size = small_hits_size;

  // Raw image with extra 2 pixels at each end....
  cudaError_t err = cudaMalloc3D(&_raw_image, make_cudaExtent((_width + 4) * sizeof(uint16_t) * type_size[image::eBayer],_height + 4, 1));
  if (err != cudaSuccess)
    return false;

  err = cudaMemset3D(_raw_image, 0, make_cudaExtent((_width + 4) * sizeof(uint16_t) * type_size[image::eBayer],_height + 4, 1));
  if (err != cudaSuccess)
    return false;
  //////////////////////////

  err = cudaMalloc3D(&_horiz_image, make_cudaExtent((_width + 4) * type_size[_type] * sizeof(uint16_t),_height + 4, 1));
  if (err != cudaSuccess)
    return false;

  err = cudaMemset3D(_horiz_image, 0, make_cudaExtent((_width + 4) * type_size[_type] * sizeof(uint16_t),_height + 4, 1));
  if (err != cudaSuccess)
    return false;
  //////////////////////////

  err = cudaMalloc3D(&_vertical_image, make_cudaExtent((_width + 4) * type_size[_type] * sizeof(uint16_t),_height + 4, 1));
  if (err != cudaSuccess)
    return false;

  err = cudaMemset3D(_vertical_image, 0, make_cudaExtent((_width + 4) * type_size[_type] * sizeof(uint16_t),_height + 4, 1));
  if (err != cudaSuccess)
    return false;
  //////////////////////////

  err = cudaMalloc3D(&_result_image, make_cudaExtent(_width * type_size[_type] * sizeof(uint16_t),_height, 1));
  if (err != cudaSuccess)
    return false;

  err = cudaMemset3D(_result_image, 0, make_cudaExtent(_width * type_size[_type] * sizeof(uint16_t),_height, 1));
  if (err != cudaSuccess)
    return false;
  //////////////////////////

  err = cudaMalloc3D(&_hlab, make_cudaExtent((_width + 4) * sizeof(double) * 3,_height + 4, 1));
  if (err != cudaSuccess)
    return false;

  err = cudaMemset3D(_hlab, 0, make_cudaExtent((_width + 4) * sizeof(double) * 3,_height + 4, 1));
  if (err != cudaSuccess)
    return false;
  //////////////////////////

  err = cudaMalloc3D(&_vlab, make_cudaExtent((_width + 4) * sizeof(double) * 3,_height + 4, 1));
  if (err != cudaSuccess)
    return false;

  err = cudaMemset3D(_vlab, 0, make_cudaExtent((_width + 4) * sizeof(double) * 3,_height + 4, 1));
  if (err != cudaSuccess)
    return false;
  //////////////////////////

  _histogram_size = (1 << 16);
  err = cudaMalloc(&_histogram,_histogram_size * sizeof(uint32_t));
  if (err != cudaSuccess)
    return false;

  err = cudaMalloc(&_histogram_max,_histogram_size * sizeof(uint32_t));
  if (err != cudaSuccess)
    return false;


  err = cudaMalloc(&_small_histogram,_small_histogram_size * sizeof(uint32_t));
  if (err != cudaSuccess)
    return false;


  _thx = std::min(DEFAULT_NUMBER_OF_THREADS, (1 << __builtin_ctz(_width)));
  if (_thx == 0)
    _thx = 1;

  _thy = std::min(DEFAULT_NUMBER_OF_THREADS, (1 << __builtin_ctz(_height)));
  if (_thy == 0)
    _thy = 1;

  _blkx = _width / _thx;
  if ((_width % _thx) != 0)
    _blkx++;

  _blky = _height / _thy;
  if ((_height % _thy) != 0)
    _blky++;

  return true;
}

/*
 * \\fn void Debayer_Impl::free
 *
 * created on: Feb 11, 2020, 3:54:09 PM
 * author daniel
 *
 */
void Debayer_Impl::free()
{
  if (_raw_image.pitch != 0)
    cudaFree(_raw_image.ptr);

  if (_horiz_image.pitch != 0)
    cudaFree(_horiz_image.ptr);

  if (_vertical_image.pitch != 0)
    cudaFree(_vertical_image.ptr);

  if (_result_image.pitch != 0)
    cudaFree(_result_image.ptr);

  if (_hlab.pitch != 0)
    cudaFree(_hlab.ptr);

  if (_vlab.pitch != 0)
    cudaFree(_vlab.ptr);

  cudaFree(_histogram);
  cudaFree(_histogram_max);
  cudaFree(_small_histogram);
}


/*
 * \\fn image::RawRGBPtr Debayer_Impl::ahd
 *
 * created on: Feb 11, 2020, 3:14:04 PM
 * author daniel
 *
 */
image::RawRGBPtr Debayer_Impl::ahd(image::RawRGBPtr img)
{
  if (!img)
    return image::RawRGBPtr();

  cudaMemcpy3DParms mcp = {0};
  mcp.srcPtr.ptr  = img->bytes();
  mcp.srcPtr.pitch = img->width() * sizeof(uint16_t);
  mcp.srcPtr.xsize = img->width() * sizeof(uint16_t);
  mcp.srcPtr.ysize = img->height();

  mcp.dstPtr.ptr = _raw_image.ptr;
  mcp.dstPtr.pitch = _raw_image.pitch;
  mcp.dstPtr.xsize = _raw_image.xsize;
  mcp.dstPtr.ysize = _raw_image.ysize;

  mcp.dstPos.x     = 2 * sizeof(uint16_t);
  mcp.dstPos.y     = 2;
  mcp.dstPos.z     = 0;

  mcp.extent.width  = img->width()* sizeof(uint16_t);
  mcp.extent.height = img->height();
  mcp.extent.depth  = 1;

  mcp.kind = cudaMemcpyHostToDevice;
  assert(cudaMemcpy3D(&mcp) == cudaSuccess);

  //
  dim3 threads(_thx >> 1,_thy >> 1);
  dim3 blocks(_blkx, _blky);

  green_interpolate<<<blocks,threads>>>(_type, type_size[_type],
                                        _raw_image,
                                        _horiz_image,
                                        _vertical_image);
  cudaDeviceSynchronize();

  blue_red_interpolate<<<blocks,threads>>>(_type, type_size[_type],
                                        _raw_image,
                                        _horiz_image,
                                        _vertical_image,
                                        _hlab,
                                        _vlab);
  cudaDeviceSynchronize();

  dim3 threads2(_thx,_thy);

  cudaMemset(_histogram,0,_histogram_size * sizeof(uint32_t));
  cudaMemset(_small_histogram,0,_small_histogram_size * sizeof(uint32_t));

  misguidance_color_artifacts<<<blocks,threads2>>>(_type, type_size[_type],
                                        _result_image,
                                        _horiz_image,
                                        _vertical_image,
                                        _hlab,
                                        _vlab,
                                        (uint32_t*)_histogram, _histogram_size,
                                        (uint32_t*)_small_histogram,_small_histogram_size);
  cudaDeviceSynchronize();


  int thx = 64;
  while (_histogram_size < thx)
    thx >>= 1;

//  assert(cudaMemcpy(_histogram_max, _histogram, _histogram_size * sizeof(uint32_t),cudaMemcpyDeviceToDevice) == cudaSuccess);
  cudaMax<<<_histogram_size / thx, thx>>>((uint32_t*)_histogram, (uint32_t*)_histogram_max);

  image::RawRGBPtr result(new image::RawRGB(img->width(), img->height(), img->depth(), _type));
  memset(&mcp,0,sizeof(mcp));

  mcp.dstPtr.ptr  = result->bytes();
  mcp.dstPtr.pitch = result->width() * type_size[_type] * sizeof(uint16_t);
  mcp.dstPtr.xsize = result->width() * type_size[_type] * sizeof(uint16_t);
  mcp.dstPtr.ysize = result->height();

  mcp.srcPtr.ptr = _result_image.ptr;
  mcp.srcPtr.pitch = _result_image.pitch;
  mcp.srcPtr.xsize = _result_image.xsize;
  mcp.srcPtr.ysize = _result_image.ysize;

  mcp.extent.width  = result->width() * type_size[_type] * sizeof(uint16_t);
  mcp.extent.height = result->height();
  mcp.extent.depth  = 1;

  mcp.kind = cudaMemcpyDeviceToHost;

  assert(cudaMemcpy3D(&mcp) == cudaSuccess);
  return result;
}


/*
 * \\fn Constructor Debayer::Debayer
 *
 * created on: Feb 12, 2020, 4:25:18 PM
 * author daniel
 *
 */
Debayer::Debayer()
: _impl(new Debayer_Impl())
{

}

/*
 * \\fn Destructor Debayer::~Debayer
 *
 * created on: Feb 12, 2020, 4:26:01 PM
 * author daniel
 *
 */
Debayer::~Debayer()
{
  delete _impl;
}

/*
 * \\fn bool Debayer::init
 *
 * created on: Feb 12, 2020, 4:26:33 PM
 * author daniel
 *
 */
bool Debayer::init(size_t width,size_t height,image::PixelType type,size_t small_hits_size)
{
  return _impl->init(width,height,type,small_hits_size);
}

/*
 * \\fn image::RawRGBPtr Debayer::debayer
 *
 * created on: Feb 12, 2020, 4:27:24 PM
 * author daniel
 *
 */
image::RawRGBPtr Debayer::debayer(image::RawRGBPtr img)
{
  if (!img)
    return image::RawRGBPtr();

  return _impl->ahd(img);
}

/*
 * \\fn Debayer::get_histogram
 *
 * created on: Feb 13, 2020
 * author: daniel
 *
 */
bool Debayer::get_histogram(image::HistPtr& histogram)
{
  if (!histogram)
    histogram.reset(new image::Histogram);

  if (histogram->_histogram.size() != _impl->_histogram_size)
    histogram->_histogram.resize(_impl->_histogram_size);

  assert(cudaMemcpy(histogram->_histogram.data(), _impl->_histogram, _impl->_histogram_size * sizeof(uint32_t),cudaMemcpyDeviceToHost) == cudaSuccess);

  if (histogram->_small_hist.size() != _impl->_small_histogram_size)
    histogram->_small_hist.resize(_impl->_small_histogram_size);

  assert(cudaMemcpy(histogram->_small_hist.data(), _impl->_small_histogram, _impl->_small_histogram_size * sizeof(uint32_t),cudaMemcpyDeviceToHost) == cudaSuccess);
  assert(cudaMemcpy(&histogram->_max_value, _impl->_histogram_max, sizeof(uint32_t),cudaMemcpyDeviceToHost) == cudaSuccess);

  return true;
}

}
// jupiter
} // brt
