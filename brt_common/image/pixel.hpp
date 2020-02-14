/*
 * pixel.hpp
 *
 *  Created on: Feb 5, 2020
 *      Author: daniel
 */

#ifndef BRT_COMMON_IMAGE_PIXEL_HPP_
#define BRT_COMMON_IMAGE_PIXEL_HPP_


#include "image.hpp"

namespace brt
{
namespace jupiter
{
namespace image
{

/*
 * \\struct name
 *
 * created on: Feb 5, 2020
 *
 */
struct Point
{
  Point(int x,int y) : _x(x), _y(y) {}
  int                             _x,_y;
};

/*
 * \\class Pixel
 *
 * created on: Feb 5, 2020
 *
 */
class Pixel
{
public:
  Pixel() : _image(), _offset(0) , _total_size(0), _width(0), _height(0), _bytes_per_pixel(0) {}
  Pixel(RawRGBPtr image, size_t offset = 0);
  virtual ~Pixel();

  enum Color { Blue = 0, Green = 1, Red = 2, Alpha = 3, Bayer = 4, NumColors};

          Pixel                   operator()(int x,int y);
          Pixel                   operator()(const Point& pt);

          int                     get(Color color = Bayer) const;
          void                    set(Color color,int);
          operator int()          { return get(); }

          Pixel&                  operator=(const Pixel& px);
          Pixel&                  operator=(const Point& pt);
          Pixel&                  operator+=(const Point& pt);
          Pixel                   operator+(const Point& pt);
          Pixel&                  operator-=(const Point& pt);
          Pixel                   operator-(const Point& pt);

          RawRGBPtr               image() const { return _image; }
          Point                   point() const;

public:
  const  size_t                  color_map[eNumTypes][Pixel::NumColors] =
  {
    //            Blue = 0, Green = 1, Red = 2,  Alpha = 3, Bayer = 4
    /*eNone = 0*/  {  0,       0,          0,        0,          0     },
    /*eBayer = 1*/ {  0,       0,          0,        0,          0     },
    /*eRGB = 2*/   {  0,       1,          2,        0,          0     },
    /*eBGR = 3*/   {  2,       1,          0,        0,          0     },
    /*eRGBA= 4*/   {  0,       1,          2,        3,          0     },
    /*eBGRA= 5*/   {  3,       2,          1,        0,          0     },
  };


private:
  RawRGBPtr                       _image;
  size_t                          _offset;

  uint32_t                        _total_size;
  uint32_t                        _width;
  uint32_t                        _height;
  uint32_t                        _bytes_per_pixel;
};

} /* namespace image */
} /* namespace jupiter */
} /* namespace brt */

#endif /* BRT_COMMON_IMAGE_PIXEL_HPP_ */
