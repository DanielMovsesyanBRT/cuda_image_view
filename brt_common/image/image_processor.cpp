/*
 * image_processor.cpp
 *
 *  Created on: Nov 25, 2019
 *      Author: daniel
 */

#include "image_processor.hpp"

#include <cmath>
#include <algorithm>

#define BITS_PER_PIXEL                      (1 << 16)
#define SMALL_HIST_SIZE                     (9)

namespace brt
{
namespace jupiter
{
namespace image
{

const size_t color_map[eNumTypes][NumColors] =
{
  //            Blue = 0, Green = 1, Red = 2,  Alpha = 3, Bayer = 4
  /*eNone = 0*/  {  0,       0,          0,        0,          0     },
  /*eBayer = 1*/ {  0,       0,          0,        0,          0     },
  /*eRGB = 2*/   {  0,       1,          2,        0,          0     },
  /*eBGR = 3*/   {  2,       1,          0,        0,          0     },
  /*eRGBA= 4*/   {  0,       1,          2,        3,          0     },
  /*eBGRA= 5*/   {  2,       1,          0,        3,          0     },
};

const size_t _type_size[eNumTypes] =
{
  /*eNone = 0*/  1,
  /*eBayer = 1*/ 1,
  /*eRGB = 2*/   3,
  /*eBGR = 3*/   3,
  /*eRGBA= 4*/   4,
  /*eBGRA= 5*/   4,
};

/*
 * \\fn LAB::from_pixel
 *
 * created on: Feb 5, 2020
 * author: daniel
 *
 */
void LAB::from_pixel(const Pixel &pixel)
{
  double X,Y,Z;

  // Matrix multiplication
  X = (0.412453 * static_cast<double>(pixel.get(Pixel::Red))    +
       0.357580 * static_cast<double>(pixel.get(Pixel::Green))  +
       0.180423 * static_cast<double>(pixel.get(Pixel::Blue))) / _Xn;

  Y = (0.212671 * static_cast<double>(pixel.get(Pixel::Red)) +
       0.715160 * static_cast<double>(pixel.get(Pixel::Green)) +
       0.072169 * static_cast<double>(pixel.get(Pixel::Blue)));

  Z = (0.019334 * static_cast<double>(pixel.get(Pixel::Red)) +
       0.119193 * static_cast<double>(pixel.get(Pixel::Green)) +
       0.950227 * static_cast<double>(pixel.get(Pixel::Blue))) / _Zn;

  auto adjust = [](double value)->double
  {
    if (value > 0.00856)
      return std::pow(value,0.33333333333);

    return 7.787 * value + 0.1379310;
  };

  _array[0] = (Y > 0.00856) ? (116.0 * std::pow(Y,0.33333333333) - 16.0) : 903.3 * Y;
  _array[1] = 500.0 * (adjust(X) - adjust(Y));
  _array[2] = 200.0 * (adjust(Y) - adjust(Z));
}

/*
 * \\fn template<typename T> void LAB::from
 *
 * created on: Feb 6, 2020
 * author: daniel
 *
 */
template<typename T> void LAB::from(T* ptr,int type)
{
  double X,Y,Z;

  // Matrix multiplication
  X = (0.412453 * static_cast<double>(ptr[color_map[type][Red]])  +
       0.357580 * static_cast<double>(ptr[color_map[type][Green]])  +
       0.180423 * static_cast<double>(ptr[color_map[type][Blue]])) / _Xn;

  Y = (0.212671 * static_cast<double>(ptr[color_map[type][Red]]) +
       0.715160 * static_cast<double>(ptr[color_map[type][Green]]) +
       0.072169 * static_cast<double>(ptr[color_map[type][Blue]]));

  Z = (0.019334 * static_cast<double>(ptr[color_map[type][Red]]) +
       0.119193 * static_cast<double>(ptr[color_map[type][Green]]) +
       0.950227 * static_cast<double>(ptr[color_map[type][Blue]])) / _Zn;

  auto adjust = [](double value)->double
  {
    if (value > 0.00856)
      return std::pow(value,0.33333333333);

    return 7.787 * value + 0.1379310;
  };

  _array[0] = (Y > 0.00856) ? (116.0 * std::pow(Y,0.33333333333) - 16.0) : 903.3 * Y;
  _array[1] = 500.0 * (adjust(X) - adjust(Y));
  _array[2] = 200.0 * (adjust(Y) - adjust(Z));
}

/*
 * \\fn Debayer::Debayer
 *
 * created on: Nov 25, 2019
 * author: daniel
 *
 */
Debayer::Debayer()
#ifdef _CUDA_VERSION
: _img_buffer()
, _img_debayer_buffer()
, _histogram()
, _histogram_max()
, _width(0)
, _height(0)
, _thx(0)
, _thy(0)
, _blkx(0)
, _blky(0)
, _overexposure_flag(false)
#endif
{
}

/*
 * \\fn Debayer::~Debayer
 *
 * created on: Nov 25, 2019
 * author: daniel
 *
 */
Debayer::~Debayer()
{
}

#ifdef _CUDA_VERSION
/*
 * \\fn RawRGBPtr Debayer::debayer
 *
 * created on: Nov 25, 2019
 * author: daniel
 *
 */
RawRGBPtr Debayer::debayer(RawRGBPtr raw,bool outputBGR)
{
  if (!raw)
    return RawRGBPtr();

  size_t img_size = raw->width() * raw->height();
  if (!_img_buffer.put((uint16_t*)raw->bytes(), img_size))
  {
    // assert
    return RawRGBPtr();
  }

  size_t debayer_img_size = img_size * 3; /* RGB*/

  if (!_img_debayer_buffer || (_img_debayer_buffer.size() != debayer_img_size))
    _img_debayer_buffer = Ptr<uint16_t>(debayer_img_size);

  if (!_img_debayer_buffer)
  {
    // assert
    return RawRGBPtr();
  }

  if (!_histogram || (_histogram.size() != BITS_PER_PIXEL))
    _histogram = Ptr<uint32_t>(BITS_PER_PIXEL);

  if (!_histogram_max || (_histogram_max.size() != BITS_PER_PIXEL))
    _histogram_max = Ptr<uint32_t>(BITS_PER_PIXEL);

  if (!_small_histogram || (_small_histogram.size() != SMALL_HIST_SIZE))
    _small_histogram = Ptr<uint32_t>(SMALL_HIST_SIZE);

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

  runDebayer(outputBGR);

  RawRGBPtr result(new RawRGB(raw->width(), raw->height(), 3 * sizeof(uint16_t)));
  _img_debayer_buffer.get((uint16_t*)result->bytes(), debayer_img_size);

  image::HistPtr  full_hist;
  get_histogram(full_hist);
  result->set_histogram(full_hist);

  return result;
}

/*
 * \\fn bool Debayer::get_histogram
 *
 * created on: Nov 25, 2019
 * author: daniel
 *
 */
bool Debayer::get_histogram(HistPtr& histogram)
{
  if (!_histogram || !_histogram_max)
    return false;

  if (!histogram)
    histogram.reset(new Histogram);

  if (histogram->_histogram.size() != _histogram.size())
    histogram->_histogram.resize(_histogram.size());

  if (histogram->_small_hist.size() != _small_histogram.size())
    histogram->_small_hist.resize(_small_histogram.size());


  _histogram.get(histogram->_histogram.data(), histogram->_histogram.size());
  _small_histogram.get(histogram->_small_hist.data(), histogram->_small_hist.size());
  _histogram_max.get(&histogram->_max_value, 1);

  return true;
}

#else
/*
 * \\fn RawRGBPtr Debayer::debayer
 *
 * created on: Nov 25, 2019
 * author: daniel
 *
 */
RawRGBPtr Debayer::debayer(RawRGBPtr raw,PixelType type)
{
  return ahd_rgba<uint16_t>(raw,type);
}


/*
 * \\fn RawRGBPtr Debayer::biliner_interpolation
 *
 * created on: Jan 31, 2020
 * author: daniel
 *
 */
RawRGBPtr Debayer::biliner_interpolation(RawRGBPtr raw)
{
  if (!raw)
    return RawRGBPtr();

  if (raw->type() != eBayer)
    return raw;

  RawRGBPtr result(new RawRGB(raw->width(), raw->height(), raw->depth(), eRGBA));
  Pixel out(result);
  Pixel in(raw);

  for (size_t y = 0; y < raw->height(); y++)
  {
    for (size_t x = 0; x < raw->width(); x++)
    {
      number r,g,b;

      switch(position(x,y))
      {
      case eClearRed:
        g = in(x, y).get();
        if (y < (raw->height() - 1))
          b += in(x, y + 1).get();

        if (y > 0)
          b += in(x, y - 1).get();

        if (x < (raw->width() - 1))
          r += in(x + 1, y).get();

        if (x > 0)
          r += in(x - 1, y).get();
        break;

      case eRed:
        r = in(x, y).get();
        if (y < (raw->height() - 1))
        {
          g += in(x, y + 1).get();
          if (x < (raw->width() - 1))
            b += in(x + 1, y + 1).get();

          if (x > 0)
            b += in(x - 1, y + 1).get();
        }

        if (y > 0)
        {
          g += in(x, y - 1).get();
          if (x < (raw->width() - 1))
            b += in(x + 1, y - 1).get();

          if (x > 0)
            b += in(x - 1, y - 1).get();
        }

        if (x < (raw->width() - 1))
          g += in(x + 1, y).get();

        if (x > 0)
          g += in(x - 1, y).get();
        break;

      case eClearBlue:
        g = in(x, y).get();
        if (y < (raw->height() - 1))
          r += in(x, y + 1).get();

        if (y > 0)
          r += in(x, y - 1).get();

        if (x < (raw->width() - 1))
          b += in(x + 1, y).get();

        if (x > 0)
          b += in(x - 1, y).get();
        break;

      case eBlue:
        b = in(x, y).get();
        if (y < (raw->height() - 1))
        {
          g += in(x, y + 1).get();
          if (x < (raw->width() - 1))
            r += in(x + 1, y + 1).get();

          if (x > 0)
            r += in(x - 1, y + 1).get();
        }

        if (y > 0)
        {
          g += in(x, y - 1).get();
          if (x < (raw->width() - 1))
            r += in(x + 1, y - 1).get();

          if (x > 0)
            r += in(x - 1, y - 1).get();
        }

        if (x < (raw->width() - 1))
          g += in(x + 1, y).get();

        if (x > 0)
          g += in(x - 1, y).get();
        break;
      }

      out.set(Pixel::Red, r);
      out.set(Pixel::Green, g);
      out.set(Pixel::Blue, b);
      out.set(Pixel::Alpha,(uint32_t)-1);
    }
  }

  return result;
}


/*
 * \\fn RawRGBPtr Debayer::ahd
 *
 * created on: Feb 3, 2020
 * author: daniel
 *
 */
RawRGBPtr Debayer::ahd(RawRGBPtr raw)
{
  size_t width = raw->width(), height = raw->height();
  // Interpolate Horizontal and Vertical
  Pixel hr(RawRGBPtr(new RawRGB(width,height,raw->depth(),eRGBA)));
  Pixel vr(RawRGBPtr(new RawRGB(width,height,raw->depth(),eRGBA)));
  Pixel in(raw);

  LAB* vlab = new LAB[width * height];
  LAB* hlab = new LAB[width * height];

  auto limit = [](int x,int a,int b)->int
  {
    if (a > b)
      return std::max(b, std::min(x,a));
    else
      return std::max(a, std::min(x,b));
  };

  // First Green Colors
  for (int y = 0;y < static_cast<int>(height); y++)
  {
    for (int x = 0;x < static_cast<int>(width); x++)
    {
      ColorPos pos = position(x,y);
      if ((pos == eRed) || (pos == eBlue))
      {
        int value = ((in(x-1,y) + in(x,y) + in(x+1,y)) * 2 - in(x-2,y) - in(x+2,y)) >> 2;
        hr(x,y).set(Pixel::Green,limit(value,in(x-1,y),in(x+1,y)));

        value = ((in(x,y-1) + in(x,y) + in(x,y+1)) * 2 - in(x,y-2) - in(x,y+2)) >> 2;
        vr(x,y).set(Pixel::Green,limit(value,in(x,y-1),in(x,y+1)));
      }
      else
      {
        int value = in(x,y);
        hr(x,y).set(Pixel::Green, value);
        vr(x,y).set(Pixel::Green, value);
      }

      hr(x,y).set(Pixel::Alpha, -1);
      vr(x,y).set(Pixel::Alpha, -1);
    }
  }

  // Now Blue and Red
  for (int y = 0;y < static_cast<int>(height); y++)
  {
    for (int x = 0;x < static_cast<int>(width); x++)
    {
      ColorPos pos = position(x,y);
      int value;

      switch (pos)
      {
      case eRed:
        hr(x,y).set(Pixel::Red,in(x,y));
        vr(x,y).set(Pixel::Red,in(x,y));

        // horizontal
        value = hr(x,y).get(Pixel::Green) +
                  (((in(x-1,y-1) - hr(x-1,y-1).get(Pixel::Green)) +
                    (in(x-1,y+1) - hr(x-1,y+1).get(Pixel::Green)) +
                    (in(x+1,y-1) - hr(x+1,y-1).get(Pixel::Green)) +
                    (in(x+1,y+1) - hr(x+1,y+1).get(Pixel::Green))) >> 2);

        hr(x,y).set(Pixel::Blue, limit(value,0,((1 << 16) - 1)));

        // vertical
        value = vr(x,y).get(Pixel::Green) +
                  (((in(x-1,y-1) - vr(x-1,y-1).get(Pixel::Green)) +
                    (in(x-1,y+1) - vr(x-1,y+1).get(Pixel::Green)) +
                    (in(x+1,y-1) - vr(x+1,y-1).get(Pixel::Green)) +
                    (in(x+1,y+1) - vr(x+1,y+1).get(Pixel::Green))) >> 2);

        vr(x,y).set(Pixel::Blue, limit(value,0,((1 << 16) - 1)));
        break;

      case eBlue:
        hr(x,y).set(Pixel::Blue,in(x,y));
        vr(x,y).set(Pixel::Blue,in(x,y));

        // horizontal
        value = hr(x,y).get(Pixel::Green) +
                  (((in(x-1,y-1) - hr(x-1,y-1).get(Pixel::Green)) +
                    (in(x-1,y+1) - hr(x-1,y+1).get(Pixel::Green)) +
                    (in(x+1,y-1) - hr(x+1,y-1).get(Pixel::Green)) +
                    (in(x+1,y+1) - hr(x+1,y+1).get(Pixel::Green))) >> 2);

        hr(x,y).set(Pixel::Red, limit(value,0,((1 << 16) - 1)));

        // vertical
        value = vr(x,y).get(Pixel::Green) +
                  (((in(x-1,y-1) - vr(x-1,y-1).get(Pixel::Green)) +
                    (in(x-1,y+1) - vr(x-1,y+1).get(Pixel::Green)) +
                    (in(x+1,y-1) - vr(x+1,y-1).get(Pixel::Green)) +
                    (in(x+1,y+1) - vr(x+1,y+1).get(Pixel::Green))) >> 2);

        vr(x,y).set(Pixel::Red, limit(value,0,((1 << 16) - 1)));
        break;

      case eClearRed:
        // horizontal
        value = hr(x,y).get(Pixel::Green) + ((in(x-1,y) - hr(x-1,y).get(Pixel::Green) + in(x+1,y) - hr(x+1,y).get(Pixel::Green)) >> 1);
        hr(x,y).set(Pixel::Red, limit(value,0,((1 << 16) - 1)));

        value = hr(x,y).get(Pixel::Green) + ((in(x,y-1) - hr(x,y-1).get(Pixel::Green) + in(x,y+1) - hr(x,y+1).get(Pixel::Green)) >> 1);
        hr(x,y).set(Pixel::Blue, limit(value,0,((1 << 16) - 1)));

        // vertical
        value = vr(x,y).get(Pixel::Green) + ((in(x-1,y) - vr(x-1,y).get(Pixel::Green) + in(x+1,y) - vr(x+1,y).get(Pixel::Green)) >> 1);
        vr(x,y).set(Pixel::Red, limit(value,0,((1 << 16) - 1)));

        value = vr(x,y).get(Pixel::Green) + ((in(x,y-1) - vr(x,y-1).get(Pixel::Green) + in(x,y+1) - vr(x,y+1).get(Pixel::Green)) >> 1);
        vr(x,y).set(Pixel::Blue, limit(value,0,((1 << 16) - 1)));
        break;

      case eClearBlue:
        // horizontal
        value = hr(x,y).get(Pixel::Green) + ((in(x-1,y) - hr(x-1,y).get(Pixel::Green) + in(x+1,y) - hr(x+1,y).get(Pixel::Green)) >> 1);
        hr(x,y).set(Pixel::Blue, limit(value,0,((1 << 16) - 1)));

        value = hr(x,y).get(Pixel::Green) + ((in(x,y-1) - hr(x,y-1).get(Pixel::Green) + in(x,y+1) - hr(x,y+1).get(Pixel::Green)) >> 1);
        hr(x,y).set(Pixel::Red, limit(value,0,((1 << 16) - 1)));

        // vertical
        value = vr(x,y).get(Pixel::Green) + ((in(x-1,y) - vr(x-1,y).get(Pixel::Green) + in(x+1,y) - vr(x+1,y).get(Pixel::Green)) >> 1);
        vr(x,y).set(Pixel::Blue, limit(value,0,((1 << 16) - 1)));

        value = vr(x,y).get(Pixel::Green) + ((in(x,y-1) - vr(x,y-1).get(Pixel::Green) + in(x,y+1) - vr(x,y+1).get(Pixel::Green)) >> 1);
        vr(x,y).set(Pixel::Red, limit(value,0,((1 << 16) - 1)));
        break;
      }

      vlab[x + y * width].from_pixel(vr(x,y));
      hlab[x + y * width].from_pixel(hr(x,y));
    }
  }

  auto get_lab = [width,height](int x,int y, LAB* lab)->LAB
  {
    int offset = y * width + x;
    if (offset < 0)
      return LAB();

    return lab[offset];
  };

  auto sqr = [](double v)->double { return v*v; };

  Pixel out(RawRGBPtr(new RawRGB(width,height,raw->depth(),eRGBA)));

  for (int y = 0;y < static_cast<int>(raw->height()); y++)
  {
    for (int x = 0;x < static_cast<int>(raw->width()); x++)
    {
      double lv[2],lh[2],cv[2],ch[2];
      int hh = 0,hv = 0;

      lh[0] = local_abs(get_lab(x,y,hlab).L(),get_lab(x - 1,y,hlab).L());
      lh[1] = local_abs(get_lab(x,y,hlab).L(),get_lab(x + 1,y,hlab).L());

      lv[0] = local_abs(get_lab(x,y,hlab).L(),get_lab(x,y - 1,hlab).L());
      lv[1] = local_abs(get_lab(x,y,hlab).L(),get_lab(x,y + 1,hlab).L());

      ch[0] = sqr(get_lab(x,y,hlab).a() - get_lab(x - 1,y,hlab).a()) + sqr(get_lab(x,y,hlab).b() - get_lab(x - 1,y,hlab).b());
      ch[1] = sqr(get_lab(x,y,hlab).a() - get_lab(x + 1,y,hlab).a()) + sqr(get_lab(x,y,hlab).b() - get_lab(x + 1,y,hlab).b());

      cv[0] = sqr(get_lab(x,y,vlab).a() - get_lab(x,y - 1,vlab).a()) + sqr(get_lab(x,y,vlab).b() - get_lab(x,y - 1,vlab).b());
      cv[1] = sqr(get_lab(x,y,vlab).a() - get_lab(x,y + 1,vlab).a()) + sqr(get_lab(x,y,vlab).b() - get_lab(x,y + 1,vlab).b());

      double eps_l = std::min(std::max(lh[0],lh[1]),std::max(lv[0],lv[1]));
      double eps_c = std::min(std::max(ch[0],ch[1]),std::max(cv[0],cv[1]));

      for (size_t index = 0; index < 2; index++)
      {
        if ((lh[index] <= eps_l) && (ch[index] <= eps_c))
          hh++;

        if ((lv[index] <= eps_l) && (cv[index] <= eps_c))
          hv++;
      }


      if (hh > hv)
        out(x,y) = hr(x,y);
      else if (hv > hh)
        out(x,y) = vr(x,y);
      else //if (hv == hh)
      {
        out.set(Pixel::Red,(hr.get(Pixel::Red) + vr.get(Pixel::Red)) >> 1);
        out.set(Pixel::Green,(hr.get(Pixel::Green) + vr.get(Pixel::Green)) >> 1);
        out.set(Pixel::Blue,(hr.get(Pixel::Blue) + vr.get(Pixel::Blue)) >> 1);
        out.set(Pixel::Alpha,-1);
      }
    }
  }

  delete[] vlab;
  delete[] hlab;

#if MEDIAN
#define SWAP(a,b)                           { int temp =(a );( a)=(b );( b)= temp ; }
#define SORT(a,b)                           { if ((a)>(b)) SWAP((a),(b )); }
#define ITERATIONS                          (2)

  // FAST median filter
  auto median9 = [](int * a)->int
  {
//     SORT (a[1] ,a [2]); SORT (a[4] ,a [5]); SORT (a[7] ,a [8]);
//     SORT (a[0] ,a [1]); SORT (a[3] ,a [4]); SORT (a[6] ,a [7]);
//     SORT (a[1] ,a [2]); SORT (a[4] ,a [5]); SORT (a[7] ,a [8]);
//     SORT (a[0] ,a [3]); SORT (a[5] ,a [8]); SORT (a[4] ,a [7]);
//     SORT (a[3] ,a [6]); SORT (a[1] ,a [4]); SORT (a[2] ,a [5]);
//     SORT (a[4] ,a [7]); SORT (a[4] ,a [2]); SORT (a[6] ,a [4]);
//     SORT (a[4] ,a [2]);
    std::sort(a, a + 9);
    return a [4];
  };

  auto median_diff = [median9](int x, int y, Pixel::Color c1, Pixel::Color c2,RawRGBPtr image)->int
  {
    int diffs [9];
    int i = 0;
    for ( int dy = -1; dy <= 1; dy ++)
    {
      for ( int dx = -1; dx <= 1; dx ++)
      {
        uint32_t val1 = image->pixel(x + dx,y + dy).color(c1);
        uint32_t val2 = image->pixel(x + dx,y + dy).color(c2);
        diffs [i ++] = static_cast<int>(val1) - static_cast<int>(val2) ;
      }
    }
    return median9 ( diffs );
  };


  for (int i = 0; i < ITERATIONS; i++)
  {
    RawRGBPtr result2 = result->clone(result->depth());
    // Median
    for (int y = 0;y < static_cast<int>(raw->height()); y++)
    {
      for (int x = 0;x < static_cast<int>(raw->width()); x++)
      {
        uint32_t r = result->pixel(x,y).green() + median_diff(x,y,Pixel::Red,Pixel::Green, result);
        uint32_t b = result->pixel(x,y).green() + median_diff(x,y,Pixel::Blue,Pixel::Green, result);
        uint32_t g = (median_diff(x,y,Pixel::Green,Pixel::Red, result) + r + median_diff(x,y,Pixel::Green,Pixel::Blue, result) + b) / 2;

        result2->pixel(x,y).set_red(r);
        result2->pixel(x,y).set_green(g);
        result2->pixel(x,y).set_blue(b);
      }
    }

    if (cback != nullptr)
      cback(result2,Utils::string_format("median_%d",i).c_str());
    result = result2;
  }
#endif

  return out.image();
}

/*
 * \\fn RawRGBPtr Debayer::ahd_rgba
 *
 * created on: Feb 6, 2020
 * author: daniel
 *
 */
template<typename T>
RawRGBPtr Debayer::ahd_rgba(RawRGBPtr raw,PixelType type)
{
  if (!raw || (BYTES_PER_PIXELS(raw->depth()) != sizeof(T)))
    return RawRGBPtr();

  int width = static_cast<int>(raw->width()),
      height = static_cast<int>(raw->height());

  // Interpolate Horizontal and Vertical
  RawRGBPtr hr(new RawRGB(width,height,raw->depth(),type));
  RawRGBPtr vr(new RawRGB(width,height,raw->depth(),type));

  T*  hrp = reinterpret_cast<T*>(hr->bytes());
  T*  vrp = reinterpret_cast<T*>(vr->bytes());
  T*  rawp = reinterpret_cast<T*>(raw->bytes());

  LAB* vlab = new LAB[width * height];
  LAB* hlab = new LAB[width * height];

  auto limit = [](int x,int a,int b)->int
  {
    if (a > b)
      return std::max(b, std::min(x,a));
    else
      return std::max(a, std::min(x,b));
  };

  const int go = color_map[type][Green]; //green offset
  const int ro = color_map[type][Red]; //red offset
  const int bo = color_map[type][Blue]; //blue offset
  const int ao = color_map[type][Alpha]; //alpha offset

#define _t(x) (x) * _type_size[type]
  // First Green Colors
  for (int y = 0;y < height; y++)
  {
    for (int x = 0;x < width; x++)
    {
      int io = x + y * width; // input offset
      int oo = _t(io); // output offset

      ColorPos pos = position(x,y);
      if ((pos == eRed) || (pos == eBlue))
      {
        int px[] = { (x > 0) ? rawp[io - 1] : 0,                // x-1,y
                     (x < (width - 1)) ? rawp[io + 1] : 0,      // x+1,y
                     (x > 1) ? rawp[io - 2] : 0,                // x-2,y
                     (x < (width - 2)) ? rawp[io + 2] : 0 };    // x+2,y

        int value =  ((( px[0] + rawp[io] + px[1]) * 2) - px[2] - px[3]) >> 2;
        hrp[oo + go] = static_cast<T>(limit(value,px[0],px[1]));

        int py[] = { (y > 0) ? rawp[io - width] : 0,                    // x,y-1
                     (y < (height - 1)) ? rawp[io + width] : 0,         // x,y+1
                     (y > 1) ? rawp[io - (2 * width)] : 0,              // x,y-2
                     (y < (height - 2))?rawp[io + (2 * width)] : 0 };   // x,y+2

        value =  ((( py[0] + rawp[io] + py[1]) * 2) - py[2] - py[3]) >> 2;
        vrp[oo + go] = static_cast<T>(limit(value,py[0],py[1]));
      }
      else
      {
        hrp[oo + go] = vrp[oo + go] = rawp[io];
      }

      hrp[oo + ao] = vrp[oo + ao] = static_cast<T>(-1);
    }
  }

  auto sum = [](int arr[4])->int { return arr[0] + arr[1] + arr[2] + arr[3]; };

  // Now Blue and Red
  for (int y = 0;y < static_cast<int>(height); y++)
  {
    for (int x = 0;x < static_cast<int>(width); x++)
    {
      int io = x + y * width; // input offset
      int oo = _t(io); // output offset

      ColorPos pos = position(x,y);
      int value;

      switch (pos)
      {
      case eRed:
      case eBlue:
        {
          hrp[oo + ((pos == eRed) ? ro : bo)] = vrp[oo + ((pos == eRed) ? ro : bo)] = rawp[io];

          int pp[] = { (x > 0 && y > 0) ? rawp[io - width - 1] : 0,                           // x-1,y-1
                       (x > 0 && y < (height - 1)) ? rawp[io + width - 1] : 0,                // x-1,y+1
                       (x < (width - 1) && y > 0) ? rawp[io - width + 1] : 0,                 // x+1,y-1
                       (x < (width - 1) && y < (height - 1)) ? rawp[io + width + 1] : 0};     // x+1,y+1

          int ph[] = { (x > 0 && y > 0) ? hrp[oo - _t(width + 1) + go] : 0,                       // x-1,y-1
                       (x > 0 && y < (height - 1)) ? hrp[oo + _t(width - 1) + go] : 0,            // x-1,y+1
                       (x < (width - 1) && y > 0) ? hrp[oo - _t(width - 1) + go] : 0,             // x+1,y-1
                       (x < (width - 1) && y < (height - 1)) ? hrp[oo + _t(width + 1) + go] : 0}; // x+1,y+1

          int pv[] = { (x > 0 && y > 0) ? vrp[oo - _t(width + 1) + go] : 0,                           // x-1,y-1
                       (x > 0 && y < (height - 1)) ? vrp[oo + _t(width - 1) + go] : 0,                // x-1,y+1
                       (x < (width - 1) && y > 0) ? vrp[oo - _t(width - 1) + go] : 0,                 // x+1,y-1
                       (x < (width - 1) && y < (height - 1)) ? vrp[oo + _t(width + 1) + go] : 0};     // x+1,y+1

          // horizontal
          value = hrp[oo + go] + ((sum(pp) - sum(ph)) >> 2);
          hrp[oo + ((pos == eRed) ? bo : ro)] = static_cast<T>(limit(value,0,((1 << 16) - 1)));

          value = vrp[oo + go] + ((sum(pp) - sum(pv)) >> 2);
          vrp[oo + ((pos == eRed) ? bo : ro)] = static_cast<T>(limit(value,0,((1 << 16) - 1)));
        }
        break;

      case eClearBlue:
      case eClearRed:
        {
          int pp[] = { (x > 0) ? rawp[io - 1] : 0,                       // x-1,y
                       (y > 0) ? rawp[io - width] : 0,                   // x,y-1
                       (x < (width - 1)) ? rawp[io + 1] : 0,             // x+1,y
                       (y < (height - 1)) ? rawp[io + width] : 0};       // x,y+1

          int ph[] = { (x > 0) ? hrp[oo - _t(1) + go] : 0,                    // x-1,y
                       (y > 0) ? hrp[oo - _t(width) + go] : 0,                // x,y-1
                       (x < (width - 1)) ? hrp[oo + _t(1) + go] : 0,          // x+1,y
                       (y < (height - 1)) ? hrp[oo + _t(width) + go] : 0};    // x,y+1

          int pv[] = { (x > 0) ? vrp[oo - _t(1) + go] : 0,                    // x-1,y
                       (y > 0) ? vrp[oo - _t(width) + go] : 0,                // x,y-1
                       (x < (width - 1)) ? vrp[oo + _t(1) + go] : 0,          // x+1,y
                       (y < (height - 1)) ? vrp[oo + _t(width) + go] : 0};    // x,y+1

          value = hrp[oo + go] + ((pp[0] - ph[0] + pp[2] - ph[2]) >> 1);
          hrp[oo + ((pos == eClearRed) ? ro : bo)] = static_cast<T>(limit(value,0,((1 << 16) - 1)));

          value = hrp[oo + go] + ((pp[1] - ph[1] + pp[3] - ph[3]) >> 1);
          hrp[oo + ((pos == eClearRed) ? bo : ro)] = static_cast<T>(limit(value,0,((1 << 16) - 1)));

          value = vrp[oo + go] + ((pp[0] - pv[0] + pp[2] - pv[2]) >> 1);
          vrp[oo + ((pos == eClearRed) ? ro : bo)] = static_cast<T>(limit(value,0,((1 << 16) - 1)));

          value = vrp[oo + go] + ((pp[1] - pv[1] + pp[3] - pv[3]) >> 1);
          vrp[oo + ((pos == eClearRed) ? bo : ro)] = static_cast<T>(limit(value,0,((1 << 16) - 1)));
        }
        break;

      }
      vlab[io].from<T>(vrp + oo, type);
      hlab[io].from<T>(hrp + oo, type);
    }
  }

  auto sqr = [](double v)->double { return v*v; };

  RawRGBPtr result(new RawRGB(width,height,raw->depth(),type));
  T*  rsp = reinterpret_cast<T*>(result->bytes());

  for (int y = 0;y < height; y++)
  {
    for (int x = 0;x < width; x++)
    {
      double lv[2],lh[2],cv[2],ch[2];
      int hh = 0,hv = 0;

      int io = x + y * width;
      int oo = _t(io);

      lh[0] = local_abs(hlab[io].L(),(x > 0) ? hlab[io - 1].L() : 0);
      lh[1] = local_abs(hlab[io].L(),(x < (width - 1)) ? hlab[io + 1].L() : 0);

      lv[0] = local_abs(vlab[io].L(),(y > 0) ? vlab[io - width].L() : 0);
      lv[1] = local_abs(vlab[io].L(),(y < (height - 1)) ? vlab[io + width].L() : 0);

      ch[0] = sqr(hlab[io].a() - ((x > 0)?hlab[io - 1].a():0)) +
              sqr(hlab[io].b() - ((x > 0)?hlab[io - 1].b():0));

      ch[1] = sqr(hlab[io].a() - ((x < (width - 1))?hlab[io + 1].a():0)) +
              sqr(hlab[io].b() - ((x < (width - 1))?hlab[io + 1].b():0));

      cv[0] = sqr(vlab[io].a() - ((y > 0)?vlab[io - width].a():0)) +
              sqr(vlab[io].b() - ((y > 0)?vlab[io - width].b():0));

      cv[1] = sqr(vlab[io].a() - ((y < (height - 1))?vlab[io + width].a():0)) +
              sqr(vlab[io].b() - ((y < (height - 1))?vlab[io + width].b():0));

      double eps_l = std::min(std::max(lh[0],lh[1]),std::max(lv[0],lv[1]));
      double eps_c = std::min(std::max(ch[0],ch[1]),std::max(cv[0],cv[1]));

      for (size_t index = 0; index < 2; index++)
      {
        if ((lh[index] <= eps_l) && (ch[index] <= eps_c))
          hh++;

        if ((lv[index] <= eps_l) && (cv[index] <= eps_c))
          hv++;
      }

      if (hh > hv)
        memcpy(rsp + oo, hrp + oo,sizeof(T) * 4);
      else if (hv > hh)
        memcpy(rsp + oo, vrp + oo,sizeof(T) * 4);
      else //if (hv == hh)
      {
        rsp[oo + ro] = (hrp[oo + ro] + vrp[oo + ro]) >> 1;
        rsp[oo + go] = (hrp[oo + go] + vrp[oo + go]) >> 1;
        rsp[oo + bo] = (hrp[oo + bo] + vrp[oo + bo]) >> 1;
        rsp[oo + ao] = static_cast<T>(-1);
      }
    }
  }

  delete[] vlab;
  delete[] hlab;

  return result;
}



#endif


/*
 * \\fn void ImageProcessor::consume
 *
 * created on: Jan 21, 2020
 * author: daniel
 *
 */
void ImageProcessor::consume(ImageBox box)
{
  for (ImagePtr img : box)
  {
    image::RawRGBPtr result = _dbr.debayer(img->get_bits(),eBGRA);
    if (result)
      ImageProducer::consume(image::ImageBox(result));
  }
}

} /* namespace image */
} /* namespace jupiter */
} /* namespace brt */
