/*
 * image_processor.hpp
 *
 *  Created on: Nov 25, 2019
 *      Author: daniel
 */

#ifndef SOURCE_IMAGE_IMAGE_PROCESSOR_HPP_
#define SOURCE_IMAGE_IMAGE_PROCESSOR_HPP_

#include <stdint.h>
#include <vector>
#include <atomic>
#include <cmath>
#include <type_traits>

#include "utils.hpp"
#include "image.hpp"
#include "pixel.hpp"

#ifdef _CUDA_VERSION
#include <cuda_utils.hpp>
#define Ptr                                 CudaPtr
#endif


#define DEFAULT_NUMBER_OF_THREADS           (64)

namespace brt
{
namespace jupiter
{
namespace image
{

enum Color { Blue = 0, Green = 1, Red = 2, Alpha = 3, Bayer = 4, NumColors};

//
//template<typename T>
//class RGB
//{
//public:
//  RGB(T* source) : _array(source) {}
//
//          T&                      red() { return (_array != nullptr)?_array[2]:_local[2]; }
//          T&                      green() { return (_array != nullptr)?_array[1]:_local[1]; }
//          T&                      blue() { return (_array != nullptr)?_array[0]:_local[0]; }
//
//  const   T&                      red() const { return (_array != nullptr)?_array[2]:_local[2]; }
//  const   T&                      green() const { return (_array != nullptr)?_array[1]:_local[1]; }
//  const   T&                      blue() const { return (_array != nullptr)?_array[0]:_local[0]; }
//
//private:
//  T*                              _array;
//  T                               _local[3];
//};
//

/*
 * \\struct LAB
 *
 * created on: Feb 3, 2020
 *
 */
class LAB
{
public:
  LAB() : _array{0.0,0.0,0.0} {}

          double&                 L() { return _array[0]; }
          double&                 a() { return _array[1]; }
          double&                 b() { return _array[2]; }

          void                    from_pixel(const Pixel &);

          template<typename T>
          void                    from(T* ptr,int type);

private:
  double                          _array[3];
  const double                    _Xn = (0.950456);
  const double                    _Zn = (1.088754);
};


/*
 * \\class Debayer
 *
 * created on: Feb 3, 2020
 *
 */
class Debayer
{
public:
  Debayer();
  virtual ~Debayer();

          RawRGBPtr               debayer(RawRGBPtr raw, PixelType);
#ifdef _CUDA_VERSION
          bool                    get_histogram(HistPtr& histogram);
          void                    set_overexp_flag(bool flag) { _overexposure_flag = flag; }

private:
          bool                    runDebayer(bool outputBGR);

private:
  Ptr<uint16_t>                   _img_buffer;
  Ptr<uint16_t>                   _img_debayer_buffer;
  Ptr<uint32_t>                   _histogram;
  Ptr<uint32_t>                   _histogram_max;
  Ptr<uint32_t>                   _small_histogram;

  uint16_t                        _width;
  uint16_t                        _height;
  int                             _thx,_thy;
  int                             _blkx,_blky;

  std::atomic_bool                _overexposure_flag;
#else

private:
          RawRGBPtr               biliner_interpolation(RawRGBPtr raw);
          // Adaptive Homogeneity-Directed
          RawRGBPtr               ahd(RawRGBPtr raw);

          template<typename T>
          RawRGBPtr               ahd_rgba(RawRGBPtr raw,PixelType);

private:

  enum ColorPos
  {
    eClearRed = 0, eRed = 1, eBlue = 2, eClearBlue = 3
  };

  ColorPos  position(int x, int y)
  {
    return static_cast<ColorPos>((x & 1) + (y & 1) * 2);
  };

  struct number
  {
    number() : _val(0), _div(0) {}
    number(uint32_t value): _val(value), _div(1) {}

    number& operator+=(uint32_t value) { _val += value; _div++; return *this; }
    number& operator=(uint32_t value) { _val = value; _div = 1; return *this; }
    operator uint32_t () const { return _val / _div; }

    uint32_t    _val;
    uint32_t    _div;
  };

  void                            green_plane(RawRGBPtr src,RawRGBPtr dst);
  void                            red_blue_plane(RawRGBPtr src,RawRGBPtr dst);

  template<typename T>
  inline int                      px_value(T px)
  {
    if (std::is_unsigned<T>::value && ( (px >> (8 * sizeof(T) - 1) == 1)))
      return 0;

    return static_cast<int>(px);
  }

  template<typename T>
  T                               local_abs(T a, T b)
  {
    if ((a > 0xFFFF) || (a < 0) || (b > 0xFFFF) || (b < 0))
      return 0;

    if (std::is_integral<T>::value)
      return std::abs(static_cast<int>(a) - static_cast<int>(b));

    return static_cast<T>(std::abs(static_cast<double>(a) - static_cast<double>(b)));
  }


  /*
   *
   * Arguments of gradient matrix
   *
   *
   *    |    | P8 |    |
   * ---|----|----|----|---
   *    | P3 | P4 | P5 |
   * ---|----|----|----|---
   *    |    | P7 |    |
   * ---|----|----|----|---
   *    | P0 | P1 | P2 |
   * ---|----|----|----|---
   *    |    | P6 |    |
   * ---|----|----|----|---
   *
   *   Grad = |P6-P7| + |P7-P8| + |P1-P4| + |P0-P3|/2 + |P2-P5|/2
   *
   */
  template<typename T,size_t A>
  double                          gradient(const T (&&P)[A][2])
  {
    double sum = .0;
    for (size_t index = 0; index < A; index++)
    {
      sum += local_abs(P[index][0],P[index][1]);
    }

    return sum;
  }

#endif
};

/*
 * \\class ImageProcessor
 *
 * created on: Nov 25, 2019
 *
 */
class ImageProcessor : public ImageConsumer
                     , public ImageProducer
{
public:
  ImageProcessor();
  virtual ~ImageProcessor();

  virtual void                    consume(ImageBox);

private:
  Debayer                         _dbr;
};

} /* namespace image */
} /* namespace jupiter */
} /* namespace brt */

#endif /* SOURCE_IMAGE_IMAGE_PROCESSOR_HPP_ */
