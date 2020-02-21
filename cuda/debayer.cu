/*
 * debayer.cu
 *
 *  Created on: Feb 14, 2020
 *      Author: daniel
 */

#include "debayer.hpp"
#include "cuda_2d_mem.hpp"
#include "cuda_mem.hpp"

#include <cuda_profiler_api.h>

namespace brt
{
namespace jupiter
{

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


struct LAB
{
  __device__ LAB() : _L(0), _a(0), _b(0) {}
  int32_t                         _L;
  int32_t                         _a;
  int32_t                         _b;

  __device__ void                 from(RGBA& rgba)
  {
    _L = (rgba._r+rgba._r+rgba._r + rgba._b + rgba._g+rgba._g+rgba._g+rgba._g) >> 3;
    _a = 5 * (rgba._r+rgba._r + rgba._b - rgba._g-rgba._g-rgba._g);
    _b = 4 * (rgba._r+rgba._r + rgba._g+rgba._g+rgba._g - rgba._b-rgba._b-rgba._b-rgba._b);
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

  Cuda2DMem<uint16_t>             _raw;
  Cuda2DMem<RGBA>                 _horiz;
  Cuda2DMem<RGBA>                 _vert;
  Cuda2DMem<RGBA>                 _result;

  Cuda2DMem<LAB>                  _hlab;
  Cuda2DMem<LAB>                  _vlab;

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
__global__ void green_interpolate(size_t width, size_t height,
                                          uint16_t* raw,
                                          RGBA* hr, RGBA* vr)
{
  int origx = ((blockIdx.x * blockDim.x) + threadIdx.x) << 1;
  int origy = ((blockIdx.y * blockDim.y) + threadIdx.y) << 1;

  auto limit = [](int x,int a,int b)->int
  {
    if (a>b)
      return (x<b)?b:(x>a)?a:x;

    return (x<a)?a:(x>b)?b:x;
  };

  // C R
  // B C
  // (0,0) -> Clear
  int x = origx, y = origy;
  int io = x + y * width; // input offset

  vr[io]._g = hr[io]._g = raw[io];
  vr[io]._a = hr[io]._a = (uint16_t)-1;

  ////////////////////////////////////////////////
  // (1,0) -> Red
  x = origx + 1;
  y = origy;
  io = x + y * width; // input offset
  {
    int px[] = { (x > 0) ? raw[io - 1] : 0,                // x-1,y
                 (x < (width - 1)) ? raw[io + 1] : 0,      // x+1,y
                 (x > 1) ? raw[io - 2] : 0,                // x-2,y
                 (x < (width - 2)) ? raw[io + 2] : 0 };    // x+2,y

    int value =  ((( px[0] + raw[io] + px[1]) * 2) - px[2] - px[3]) >> 2;
    hr[io]._g = limit(value, px[0], px[1]);

    int py[] = { (y > 0) ? raw[io - width] : 0,                    // x,y-1
                 (y < (height - 1)) ? raw[io + width] : 0,         // x,y+1
                 (y > 1) ? raw[io - (2 * width)] : 0,              // x,y-2
                 (y < (height - 2))?raw[io + (2 * width)] : 0 };   // x,y+2

    value =  ((( py[0] + raw[io] + py[1]) * 2) - py[2] - py[3]) >> 2;
    vr[io]._g = limit(value, py[0], py[1]);
  }

  ////////////////////////////////////////////////
  // (0,1) -> Blue
  x = origx;
  y = origy + 1;
  io = x + y * width; // input offset
  {
    int px[] = { (x > 0) ? raw[io - 1] : 0,                // x-1,y
                 (x < (width - 1)) ? raw[io + 1] : 0,      // x+1,y
                 (x > 1) ? raw[io - 2] : 0,                // x-2,y
                 (x < (width - 2)) ? raw[io + 2] : 0 };    // x+2,y

    int value =  ((( px[0] + raw[io] + px[1]) * 2) - px[2] - px[3]) >> 2;
    hr[io]._g = limit(value, px[0], px[1]);

    int py[] = { (y > 0) ? raw[io - width] : 0,                    // x,y-1
                 (y < (height - 1)) ? raw[io + width] : 0,         // x,y+1
                 (y > 1) ? raw[io - (2 * width)] : 0,              // x,y-2
                 (y < (height - 2))?raw[io + (2 * width)] : 0 };   // x,y+2

    value =  ((( py[0] + raw[io] + py[1]) * 2) - py[2] - py[3]) >> 2;
    vr[io]._g = limit(value, py[0], py[1]);
  }
  ////////////////////////////////////////////////
  // (1,1) -> Clear
  x = origx + 1;
  y = origy + 1;
  io = x + y * width; // input offset

  vr[io]._g = hr[io]._g = raw[io];
  vr[io]._a = hr[io]._a = (uint16_t)-1;
}

/*
 * \\fn void blue_red_interpolate
 *
 * created on: Feb 12, 2020, 11:35:37 AM
 * author daniel
 *
 */
__global__ void blue_red_interpolate( size_t width, size_t height,
                                      uint16_t* raw,
                                      RGBA* ip0,RGBA* ip1,
                                      LAB* lb0,LAB* lb1)
//                                      RGBA* hr, RGBA* vr,
//                                      LAB* hl,LAB* vl)
{
  int origx = ((blockIdx.x * blockDim.x) + threadIdx.x) << 1;
  int origy = ((blockIdx.y * blockDim.y) + threadIdx.y) << 1;
  int z = blockIdx.z;

  RGBA* ip[2] = {ip0, ip1};
  LAB*  lb[2] = {lb0, lb1};

  auto limit = [](int x,int a,int b)->int
  {
    return (x<a)?a:(x>b)?b:x;
  };

  int sub[4][4] = { {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0} };

  int io = origx + origy * width;


  if (origx > 0)
  {
    if (origy > 0)
      sub[0][0] = raw[io-width-1] - ip[z][io-width-1]._g;

    sub[1][0] = raw[io-1] - ip[z][io-1]._g;
    sub[2][0] = raw[io+width-1] - ip[z][io+width-1]._g;

    if (origy < (height - 2))
      sub[3][0] = raw[io+2*width-1] - ip[z][io+2*width-1]._g;
  }

  if (origx < (width - 2))
  {
    if (origy > 0)
      sub[0][3] = raw[io-width+2] - ip[z][io-width+2]._g;

    sub[1][3] = raw[io+2] - ip[z][io+2]._g;
    sub[2][3] = raw[io+width+2] - ip[z][io+width+2]._g;

    if (origy < (height - 2))
      sub[3][3] = raw[io+2*width+2] - ip[z][io+2*width+2]._g;
  }

  if (origy > 0)
  {
    sub[0][1] = raw[io-width] - ip[z][io-width]._g;
    sub[0][2] = raw[io-width+1] - ip[z][io-width+1]._g;
  }

  if (origy < (height - 2))
  {
    sub[3][1] = raw[io+2*width] - ip[z][io+2*width]._g;
    sub[3][2] = raw[io+2*width+1] - ip[z][io+2*width+1]._g;
  }

  sub[1][1] = raw[io] - ip[z][io]._g;
  sub[1][2] = raw[io+1] - ip[z][io+1]._g;
  sub[2][1] = raw[io+width] - ip[z][io+width]._g;
  sub[2][2] = raw[io+width+1] - ip[z][io+width+1]._g;

  // C R
  // B C
  ////////////////////////////////////////////////
  // (0,0) -> ClearRead
  {
    ip[z][io]._r = limit(ip[z][io]._g + ((sub[1][0] + sub[1][2]) >> 1),0,(1<<16)-1);
    ip[z][io]._b = limit(ip[z][io]._g + ((sub[0][1] + sub[2][1]) >> 1),0,(1<<16)-1);

    lb[z][io].from(ip[z][io]);
  }

  ////////////////////////////////////////////////
  // (1,0) -> Red
  {
    ip[z][io+1]._r = raw[io+1];
    ip[z][io+1]._b = limit(ip[z][io+1]._g + ((sub[0][1] + sub[0][3] + sub[2][1] + sub[2][3]) >> 2),0,(1<<16)-1);

    lb[z][io+1].from(ip[z][io+1]);
  }

  ////////////////////////////////////////////////
  // (0,1) -> Blue
  {
    ip[z][io+width]._b = raw[io+width];

    ip[z][io+width]._r = limit(ip[z][io+width]._g + ((sub[1][0] + sub[1][2] + sub[3][0] + sub[3][2]) >> 2),0,(1<<16)-1);
    lb[z][io+width].from(ip[z][io+width]);
  }

  ////////////////////////////////////////////////
  // (1,1) -> ClearBlue
  {
    ip[z][io+width+1]._b = limit(ip[z][io+width+1]._g + ((sub[2][1] + sub[2][3]) >> 1),0,(1<<16)-1);
    ip[z][io+width+1]._r = limit(ip[z][io+width+1]._g + ((sub[1][2] + sub[3][2]) >> 1),0,(1<<16)-1);

    lb[z][io+width+1].from(ip[z][io+width+1]);
  }


#if 0
  int h_sub[4][4] = { {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0} };
  int v_sub[4][4] = { {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0} };

  int io = origx + origy * width;


  if (origx > 0)
  {
    if (origy > 0)
    {
      h_sub[0][0] = raw[io-width-1] - hr[io-width-1]._g;
      v_sub[0][0] = raw[io-width-1] - vr[io-width-1]._g;
    }

    h_sub[1][0] = raw[io-1] - hr[io-1]._g;
    v_sub[1][0] = raw[io-1] - vr[io-1]._g;

    h_sub[2][0] = raw[io+width-1] - hr[io+width-1]._g;
    v_sub[2][0] = raw[io+width-1] - vr[io+width-1]._g;

    if (origy < (height - 2))
    {
      h_sub[3][0] = raw[io+2*width-1] - hr[io+2*width-1]._g;
      v_sub[3][0] = raw[io+2*width-1] - vr[io+2*width-1]._g;
    }
  }

  if (origx < (width - 2))
  {
    if (origy > 0)
    {
      h_sub[0][3] = raw[io-width+2] - hr[io-width+2]._g;
      v_sub[0][3] = raw[io-width+2] - vr[io-width+2]._g;
    }

    h_sub[1][3] = raw[io+2] - hr[io+2]._g;
    v_sub[1][3] = raw[io+2] - vr[io+2]._g;

    h_sub[2][3] = raw[io+width+2] - hr[io+width+2]._g;
    v_sub[2][3] = raw[io+width+2] - vr[io+width+2]._g;

    if (origy < (height - 2))
    {
      h_sub[3][3] = raw[io+2*width+2] - hr[io+2*width+2]._g;
      v_sub[3][3] = raw[io+2*width+2] - vr[io+2*width+2]._g;
    }
  }

  if (origy > 0)
  {
    h_sub[0][1] = raw[io-width] - hr[io-width]._g;
    v_sub[0][1] = raw[io-width] - vr[io-width]._g;

    h_sub[0][2] = raw[io-width+1] - hr[io-width+1]._g;
    v_sub[0][2] = raw[io-width+1] - vr[io-width+1]._g;
  }

  if (origy < (height - 2))
  {
    h_sub[3][1] = raw[io+2*width] - hr[io+2*width]._g;
    v_sub[3][1] = raw[io+2*width] - vr[io+2*width]._g;

    h_sub[3][2] = raw[io+2*width+1] - hr[io+2*width+1]._g;
    v_sub[3][2] = raw[io+2*width+1] - vr[io+2*width+1]._g;
  }

  h_sub[1][1] = raw[io] - hr[io]._g;
  v_sub[1][1] = raw[io] - vr[io]._g;

  h_sub[1][2] = raw[io+1] - hr[io+1]._g;
  v_sub[1][2] = raw[io+1] - vr[io+1]._g;

  h_sub[2][1] = raw[io+width] - hr[io+width]._g;
  v_sub[2][1] = raw[io+width] - vr[io+width]._g;

  h_sub[2][2] = raw[io+width+1] - hr[io+width+1]._g;
  v_sub[2][2] = raw[io+width+1] - vr[io+width+1]._g;


  // C R
  // B C
  ////////////////////////////////////////////////
  // (0,0) -> ClearRead
  {
    hr[io]._r = limit(hr[io]._g + ((h_sub[1][0] + h_sub[1][2]) >> 1),0,(1<<16)-1);
    hr[io]._b = limit(hr[io]._g + ((h_sub[0][1] + h_sub[2][1]) >> 1),0,(1<<16)-1);

    vr[io]._r = limit(vr[io]._g + ((v_sub[1][0] + v_sub[1][2]) >> 1),0,(1<<16)-1);
    vr[io]._b = limit(vr[io]._g + ((v_sub[0][1] + v_sub[2][1]) >> 1),0,(1<<16)-1);

    hl[io].from(hr[io]);
    vl[io].from(vr[io]);
  }

  ////////////////////////////////////////////////
  // (1,0) -> Red
  {
    hr[io+1]._r = vr[io+1]._r = raw[io+1];

    hr[io+1]._b = limit(hr[io+1]._g + ((h_sub[0][1] + h_sub[0][3] + h_sub[2][1] + h_sub[2][3]) >> 2),0,(1<<16)-1);
    vr[io+1]._b = limit(vr[io+1]._g + ((v_sub[0][1] + v_sub[0][3] + v_sub[2][1] + v_sub[2][3]) >> 2),0,(1<<16)-1);

    hl[io+1].from(hr[io+1]);
    vl[io+1].from(vr[io+1]);
  }

  ////////////////////////////////////////////////
  // (0,1) -> Blue
  {
    hr[io+width]._b = vr[io+width]._b = raw[io+width];

    hr[io+width]._r = limit(hr[io+width]._g + ((h_sub[1][0] + h_sub[1][2] + h_sub[3][0] + h_sub[3][2]) >> 2),0,(1<<16)-1);
    vr[io+width]._r = limit(vr[io+width]._g + ((v_sub[1][0] + v_sub[1][2] + v_sub[3][0] + v_sub[3][2]) >> 2),0,(1<<16)-1);

    hl[io+width].from(hr[io+width]);
    vl[io+width].from(vr[io+width]);
  }

  ////////////////////////////////////////////////
  // (1,1) -> ClearBlue
  {
    hr[io+width+1]._b = limit(hr[io+width+1]._g + ((h_sub[2][1] + h_sub[2][3]) >> 1),0,(1<<16)-1);
    hr[io+width+1]._r = limit(hr[io+width+1]._g + ((h_sub[1][2] + h_sub[3][2]) >> 1),0,(1<<16)-1);

    vr[io+width+1]._b = limit(vr[io+width+1]._g + ((v_sub[2][1] + v_sub[2][3]) >> 1),0,(1<<16)-1);
    vr[io+width+1]._r = limit(vr[io+width+1]._g + ((v_sub[1][2] + v_sub[3][2]) >> 1),0,(1<<16)-1);

    hl[io+width+1].from(hr[io+width+1]);
    vl[io+width+1].from(vr[io+width+1]);
  }
#endif
}


/*
 * \\fn void misguidance_color_artifacts
 *
 * created on: Feb 12, 2020, 3:21:22 PM
 * author daniel
 *
 */
__global__ void misguidance_color_artifacts(size_t width, size_t height, RGBA* rst,
                                            RGBA* hr,RGBA* vr,
                                            LAB* hl,LAB* vl,
                                            uint32_t* histogram,uint32_t histogram_size,
                                            uint32_t* small_histogram, uint32_t small_histogram_size)
{
  int x = ((blockIdx.x * blockDim.x) + threadIdx.x);
  int y = ((blockIdx.y * blockDim.y) + threadIdx.y);
  int io = x + y * width;

  auto sqr=[](int a)->int { return a*a; };

  int lv[2],lh[2],cv[2],ch[2];
  int hh = 0,hv = 0;


  LAB ph[] = { (x > 0) ? hl[io-1] : LAB(),                            // x-1,y
               (x < (width - 1)) ? hl[io+1] : LAB()};                 // x+1,y

  LAB pv[] = { (y > 0) ? vl[io-width] : LAB(),                        // x,y-1
               (y < (height - 1)) ? vl[io+width] : LAB()};            // x,y+1


  lh[0] = __sad(hl[io]._L,ph[0]._L,0);
  lh[1] = __sad(hl[io]._L,ph[1]._L,0);

  lv[0] = __sad(vl[io]._L,pv[0]._L,0);
  lv[1] = __sad(vl[io]._L,pv[1]._L,0);

  ch[0] = sqr(hl[io]._a - ph[0]._a) +
          sqr(hl[io]._b - ph[0]._b);

  ch[1] = sqr(hl[io]._a - ph[1]._a) +
          sqr(hl[io]._b - ph[1]._b);

  cv[0] = sqr(vl[io]._a - pv[0]._a) +
          sqr(vl[io]._b - pv[0]._b);

  cv[1] = sqr(vl[io]._a - pv[1]._a) +
          sqr(vl[io]._b - pv[1]._b);


  int h = (lh[0]>lh[1]) ? lh[0]: lh[1];
  int v = (lv[0]>lv[1]) ? lv[0]: lv[1];
  int eps_l = v < h ? v : h;

  h = (ch[0]>ch[1])?ch[0]:ch[1];
  v = (cv[0]>cv[1])?cv[0]:cv[1];
  int eps_c = v < h ? v : h;

  hh = ((lh[0]<=eps_l) * (ch[0]<= eps_c)) +
       ((lh[1]<=eps_l) * (ch[1]<= eps_c));

  hv = ((lv[0]<=eps_l) * (cv[0]<= eps_c)) +
       ((lv[1]<=eps_l) * (cv[1]<= eps_c));

  uint32_t r = 0,g = 0,b = 0;
  RGBA *lf, *rt;

  lf = (hh > hv)? &hr[io] : &vr[io];
  rt = (hv > hh)? &vr[io] : &hr[io];

  r = rst[io]._r = (lf->_r + rt->_r) >> 1;
  g = rst[io]._g = (lf->_g + rt->_g) >> 1;
  b = rst[io]._b = (lf->_b + rt->_b) >> 1;
  rst[io]._a = (uint16_t)-1;

  uint32_t brightness = ((r+r+r+b+g+g+g+g) >> 3) * small_histogram_size >> 16;
  atomicAdd(&small_histogram[brightness % small_histogram_size], 1);

  brightness = ((r+r+r+b+g+g+g+g) >> 3) * histogram_size >> 16;
  atomicAdd(&histogram[brightness & ((1 << 16) - 1)], 1);
}


/*
 * \\fn void cudaMax
 *
 * created on: Nov 22, 2019
 * author: daniel
 *
 */
__global__ void cudaMax(uint32_t* org,uint32_t* max)
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
  _raw = Cuda2DMem<uint16_t>(width,height);  _raw.fill(0);

  _horiz = Cuda2DMem<RGBA>(width,height); _horiz.fill(0);
  _vert = Cuda2DMem<RGBA>(width,height); _vert.fill(0);
  _result = Cuda2DMem<RGBA>(width,height); _result.fill(0);

  _hlab = Cuda2DMem<LAB>(width,height); _hlab.fill(0);
  _vlab = Cuda2DMem<LAB>(width,height); _vlab.fill(0);

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

  _histogram_max.fill(0);
  _small_histogram.fill(0);
  _raw.put((uint16_t*)img->bytes(),img->width() * img->height());

  cudaProfilerStart();

  dim3 threads(_thx >> 1,_thy >> 1);
  dim3 blocks(_blkx, _blky);
  dim3 blocks2(_blkx, _blky, 2);

  green_interpolate<<<blocks,threads>>>(img->width(), img->height(),_raw.ptr(), _horiz.ptr(), _vert.ptr());
  blue_red_interpolate<<<blocks2,threads>>>(img->width(), img->height(),_raw.ptr(), _horiz.ptr(), _vert.ptr(), _hlab.ptr(), _vlab.ptr());

  dim3 threads2(_thx,_thy);
  misguidance_color_artifacts<<<blocks,threads2>>>(img->width(), img->height(),
                      _result.ptr(), _horiz.ptr(),
                      _vert.ptr(), _hlab.ptr(), _vlab.ptr(),_histogram.ptr(), _histogram.size(),_small_histogram.ptr(), _small_histogram.size());

  int thx = 64;
  while (_histogram.size() < thx)
    thx >>= 1;

  cudaMax<<<_histogram.size() / thx, thx>>>(_histogram.ptr(), _histogram_max.ptr());

  cudaProfilerStop();

  image::RawRGBPtr result(new image::RawRGB(img->width(), img->height(), img->depth(), image::eRGBA));
  _result.get((RGBA*)result->bytes(), result->width() * result->height());

  image::HistPtr  full_hist(new image::Histogram);
  full_hist->_histogram.resize(_histogram.size());
  full_hist->_small_hist.resize(_small_histogram.size());

  _histogram.get((uint32_t*)full_hist->_histogram.data(), _histogram.size());
  _small_histogram.get(full_hist->_small_hist.data(), _small_histogram.size());
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
