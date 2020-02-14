/*
 * pixel.cpp
 *
 *  Created on: Feb 5, 2020
 *      Author: daniel
 */

#include "pixel.hpp"
#include "utils.hpp"

namespace brt
{
namespace jupiter
{
namespace image
{

//const size_t Pixel::color_map[eNumTypes][Pixel::NumColors] =
//{
//  //            Blue = 0, Green = 1, Red = 2,  Alpha = 3, Bayer = 4
//  /*eNone = 0*/  {  0,       0,          0,        0,          0     },
//  /*eBayer = 1*/ {  0,       0,          0,        0,          0     },
//  /*eRGB = 2*/   {  0,       1,          2,        0,          0     },
//  /*eBGR = 3*/   {  2,       1,          0,        0,          0     },
//  /*eRGBA= 4*/   {  0,       1,          2,        3,          0     },
//  /*eBGRA= 5*/   {  3,       2,          1,        0,          0     },
//};
//

/*
 * \\fn Constructor Pixel::Pixel
 *
 * created on: Feb 5, 2020
 * author: daniel
 *
 */
Pixel::Pixel(RawRGBPtr image, size_t offset /*= 0*/)
: _image(image)
, _offset(offset)
, _total_size(0)
, _width(0)
, _height(0)
, _bytes_per_pixel(0)
{
  if (_image)
  {
    _width = _image->width();
    _height = _image->height();
    _bytes_per_pixel = BYTES_PER_PIXELS(_image->depth()) * type_size(_image->type());
    _total_size = _width * _height * _bytes_per_pixel;
  }
}

/*
 * \\fn Destructor Pixel::~Pixel
 *
 * created on: Feb 5, 2020
 * author: daniel
 *
 */
Pixel::~Pixel()
{

}

/*
 * \\fn Pixel Pixel::operator()
 *
 * created on: Feb 5, 2020
 * author: daniel
 *
 */
Pixel Pixel::operator()(int x,int y)
{
  if (!_image)
    return Pixel();

  int new_offset = (x + _width * y) * _bytes_per_pixel + _offset;

  return Pixel(_image,static_cast<size_t>(new_offset));
}

/*
 * \\fn Pixel Pixel::operator()
 *
 * created on: Feb 5, 2020
 * author: daniel
 *
 */
Pixel Pixel::operator()(const Point& pt)
{
  if (!_image)
    return Pixel();

  int new_offset = (pt._x + _width * pt._y) * _bytes_per_pixel + _offset;

  return Pixel(_image,static_cast<size_t>(new_offset));
}

/*
 * \\fn int Pixel::get
 *
 * created on: Feb 5, 2020
 * author: daniel
 *
 */
int Pixel::get(Color color) const
{
  if (!_image)
    return 0;

  int full_offset = _offset + color_map[_image->type()][color] * BYTES_PER_PIXELS(_image->depth());
  if ((full_offset < 0) || (full_offset > static_cast<int>(_total_size - BYTES_PER_PIXELS(_image->depth()))))
    return 0;

  switch (BYTES_PER_PIXELS(_image->depth()))
  {
  case 1:
    return _image->bytes()[full_offset];

  case 2:
    return *(reinterpret_cast<uint16_t*>(_image->bytes() + full_offset));

  case 3:
    return *(reinterpret_cast<uint32_t*>(_image->bytes() + full_offset)) & 0xFFFFFF;

  case 4:
    return *(reinterpret_cast<uint32_t*>(_image->bytes() + full_offset));

  default:
    break;
  }

  return 0;
}

/*
 * \\fn void Pixel::set
 *
 * created on: Feb 5, 2020
 * author: daniel
 *
 */
void Pixel::set(Color color,int value)
{
  if (!_image)
    return;

  int full_offset = _offset + color_map[_image->type()][color] * BYTES_PER_PIXELS(_image->depth());
  if ((full_offset < 0) || (full_offset > static_cast<int>(_total_size - BYTES_PER_PIXELS(_image->depth()))))
    return;

  memcpy(_image->bytes() + full_offset, &value, std::min(BYTES_PER_PIXELS(_image->depth()),sizeof(int)));
}

/*
 * \\fn Pixel& Pixel::operator=
 *
 * created on: Feb 5, 2020
 * author: daniel
 *
 */
Pixel& Pixel::operator=(const Pixel& px)
{
  if (!_image || !px._image || (_image->type() != px._image->type()))
    return *this;

  if ((_offset < 0) && (_offset > (_total_size - BYTES_PER_PIXELS(_image->depth()))))
    return *this;

  if ((px._offset < 0) && (px._offset > (px._total_size - BYTES_PER_PIXELS(px._image->depth()))))
    return *this;

  if (_image->depth() == px._image->depth())
    memcpy(_image->bytes() + _offset, px._image->bytes() + px._offset,BYTES_PER_PIXELS(_image->depth())) ;
  else
  {
    set(Red,px.get(Red));
    set(Green,px.get(Green));
    set(Blue,px.get(Blue));
    set(Alpha,px.get(Alpha));
  }

  return *this;
}

/*
 * \\fn Pixel& Pixel::operator=
 *
 * created on: Feb 5, 2020
 * author: daniel
 *
 */
Pixel& Pixel::operator=(const Point& pt)
{
  if (_image)
  {
    int new_offset = (pt._x + static_cast<int>(_image->width()) * pt._y) *
        BYTES_PER_PIXELS(_image->depth()) * type_size(_image->type());

    _offset = static_cast<size_t>(new_offset);
  }
  return *this;
}

/*
 * \\fn Pixel& Pixel::operator+=
 *
 * created on: Feb 5, 2020
 * author: daniel
 *
 */
Pixel& Pixel::operator+=(const Point& pt)
{
  if (_image)
  {
    int new_offset = static_cast<int>(_offset) + (pt._x + static_cast<int>(_image->width()) * pt._y) *
        BYTES_PER_PIXELS(_image->depth()) * type_size(_image->type());

    _offset = static_cast<size_t>(new_offset);
  }
  return *this;
}

/*
 * \\fn Pixel Pixel::operator+
 *
 * created on: Feb 5, 2020
 * author: daniel
 *
 */
Pixel Pixel::operator+(const Point& pt)
{
  if (!_image)
    return Pixel();

  int new_offset = static_cast<int>(_offset) + (pt._x + static_cast<int>(_image->width()) * pt._y) *
      BYTES_PER_PIXELS(_image->depth()) * type_size(_image->type());

  return Pixel(_image,static_cast<size_t>(new_offset));
}

/*
 * \\fn Pixel& Pixel::operator-=
 *
 * created on: Feb 5, 2020
 * author: daniel
 *
 */
Pixel& Pixel::operator-=(const Point& pt)
{
  if (_image)
  {
    int new_offset = static_cast<int>(_offset) - (pt._x + static_cast<int>(_image->width()) * pt._y) *
        BYTES_PER_PIXELS(_image->depth()) * type_size(_image->type());

    _offset = static_cast<size_t>(new_offset);
  }
  return *this;
}

/*
 * \\fn Pixel Pixel::operator-
 *
 * created on: Feb 5, 2020
 * author: daniel
 *
 */
Pixel Pixel::operator-(const Point& pt)
{
  if (!_image)
    return Pixel();

  int new_offset = static_cast<int>(_offset) - (pt._x + static_cast<int>(_image->width()) * pt._y) *
      BYTES_PER_PIXELS(_image->depth()) * type_size(_image->type());

  return Pixel(_image,static_cast<size_t>(new_offset));
}

/*
 * \\fn Point Pixel::point
 *
 * created on: Feb 5, 2020
 * author: daniel
 *
 */
Point Pixel::point() const
{
  if (!_image)
    return Point(0,0);

  int raw_size = _width * _bytes_per_pixel;

  int y = _offset / raw_size;
  int x = _offset - (raw_size * y);

  return Point(x,y);
}


} /* namespace image */
} /* namespace jupiter */
} /* namespace brt */
