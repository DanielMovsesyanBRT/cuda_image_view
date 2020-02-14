/*
 * Image.hpp
 *
 *  Created on: Jul 2, 2019
 *      Author: daniel
 */

#ifndef IMAGE_IMAGE_HPP_
#define IMAGE_IMAGE_HPP_

#include <memory>
#include <unordered_set>
#include <mutex>
#include <vector>

#include "metadata.hpp"
#include "utils.hpp"

namespace brt
{
namespace jupiter
{
namespace image
{

class ImagePtr;
class ImageProducer;


/*
 * \\struct struct Histogram
 *
 * created on: Nov 26, 2019
 *
 */
struct Histogram
{
  std::vector<uint32_t>           _histogram;
  uint32_t                        _max_value;
  std::vector<uint32_t>           _small_hist;
};

typedef std::shared_ptr<Histogram> HistPtr;

class RawRGB;
typedef std::shared_ptr<RawRGB> RawRGBPtr;

/*
 * \\enum PixelType
 *
 * created on: Jan 23, 2020
 *
 */
enum PixelType
{
  eNone =  0,
  eBayer = 1,
  eRGB =   2,
  eBGR =   3,
  eRGBA =  4,
  eBGRA =  5,

  eNumTypes
};


/*
 * \\fn size_t type_size
 *
 * created on: Jan 31, 2020
 * author: daniel
 *
 */
inline size_t type_size(PixelType type)
{
  switch (type)
  {
  case eBayer:
    return 1;

  case eRGB:
  case eBGR:
    return 3;

  case eRGBA:
  case eBGRA:
    return 4;

  default:
    break;
  }
  return 0;
}


#if 0


/*
 * \\fn Pixel
 *
 * created on: Jan 23, 2020
 * author: daniel
 *
 */
class Pixel
{
public:
  enum Color
  {
    Red,Green,Blue,Alpha
  };

private:
  uint32_t                 offset(Color color) const
  {
    switch (_type)
    {
    case eRGB:
      switch (color)
      {
      case Red: return 2;
      case Green: return 1;
      case Blue: return 0;
      case Alpha: return 0;
      }
      break;

    case eBGR:
      switch (color)
      {
      case Red: return 0;
      case Green: return 1;
      case Blue: return 2;
      case Alpha: return 0;
      }
      break;

    case eRGBA:
      switch (color)
      {
      case Red: return 2;
      case Green: return 1;
      case Blue: return 0;
      case Alpha: return 3;
      }
      break;

    case eBGRA:
      switch (color)
      {
      case Red: return 0;
      case Green: return 1;
      case Blue: return 2;
      case Alpha: return 3;
      }
      break;

    default:
      break;
    }
    return 0;
  }

public:
  Pixel() : _ptr(nullptr), _type(eNone), _depth(0) {}
  Pixel(uint8_t* ptr, PixelType type, size_t depth)
      : _ptr(ptr), _type(type), _depth(depth) {}

  Pixel(const Pixel& pixel)
  : _ptr(pixel._ptr), _type(pixel._type), _depth(pixel._depth)
  {}

  uint32_t                        raw(int offset = 0) const
  {
    if (_ptr == nullptr)
      return (uint32_t)-1;

    switch (BYTES_PER_PIXELS(_depth))
    {
    case 1:
      return *(_ptr + offset);

    case 2:
      return *(reinterpret_cast<uint16_t*>(_ptr) + offset);

    case 3:
      return *reinterpret_cast<uint32_t*>(_ptr + offset * 3) & 0xFFFFFF;

    case 4:
      return *(reinterpret_cast<uint32_t*>(_ptr) + offset);

    default:
      break;
    }

    return (uint32_t)-1;
  }

  Pixel&                          set_raw(uint32_t value,int offset = 0)
  {
    if (_ptr == nullptr)
      return *this;

    if (value >= static_cast<uint32_t>(1 << _depth))
      value = static_cast<uint32_t>(1 << _depth) - 1;

    switch (BYTES_PER_PIXELS(_depth))
    {
    case 1:
      *(_ptr + offset) = static_cast<uint8_t>(value);
      break;

    case 2:
      *(reinterpret_cast<uint16_t*>(_ptr) + offset) = static_cast<uint16_t>(value);
      break;

    case 3:
      *reinterpret_cast<uint32_t*>(_ptr + offset * 3) = value & 0xFFFFFF;
      break;

    case 4:
      *(reinterpret_cast<uint32_t*>(_ptr) + offset) = value;
      break;

    default:
      break;
    }

    return *this;
  }

  uint32_t                        red() const { return raw(offset(Red)); }
  uint32_t                        green() const { return raw(offset(Green)); }
  uint32_t                        blue() const { return raw(offset(Blue)); }
  uint32_t                        alpha() const { return raw(offset(Alpha)); }
  uint32_t                        color(Color c) const {return raw(offset(c)); }

  Pixel&                          set_red(uint32_t value) { return set_raw(value,offset(Red)); }
  Pixel&                          set_green(uint32_t value) { return set_raw(value,offset(Green)); }
  Pixel&                          set_blue(uint32_t value) {  return set_raw(value,offset(Blue)); }
  Pixel&                          set_alpha(uint32_t value) {  return set_raw(value,offset(Alpha)); }
  Pixel&                          set_color(uint32_t value,Color c) {  return set_raw(value,offset(c)); }

  Pixel&                          assign(const Pixel& other)
  {
    if (_type == other._type && _depth == other._depth)
    {
      memcpy(_ptr,other._ptr,type_size(_type) * BYTES_PER_PIXELS(_depth));
    }
    else
    {
      set_red(other.red());
      set_green(other.green());
      set_blue(other.blue());
      set_alpha(other.alpha());
    }

    return *this;
  }

private:
  uint8_t*                        _ptr;
  PixelType                       _type;
  size_t                          _depth;
};

#endif

/*
 * \\class RawRGB
 *
 * created on: Jul 2, 2019
 *
 */
class RawRGB
{
public:
  RawRGB(size_t w, size_t h, size_t depth, PixelType type = eBayer);
  RawRGB(const uint8_t*, size_t w, size_t h, size_t depth, PixelType type = eBayer);
  RawRGB(const char *);
  virtual ~RawRGB();

          size_t                  width() const { return _width; }
          size_t                  height() const { return _height; }
          size_t                  depth() const { return _depth; }
          PixelType               type() const { return _type; }
          size_t                  size() const { return _width * _height * BYTES_PER_PIXELS(_depth) * _type;}

          uint8_t*                bytes() { return _buffer; }
          const uint8_t*          bytes() const { return _buffer; }
          bool                    empty() const { return (_buffer == nullptr);}

          //Pixel                   pixel(int x, int y);

          void                    set_histogram(HistPtr hist) { _hist = hist; }
          HistPtr                 get_histogram() const { return _hist; }

          RawRGBPtr               clone(size_t depth);

private:
  size_t                          _width;
  size_t                          _height;
  size_t                          _depth;
  PixelType                       _type;
  uint8_t*                        _buffer;
  HistPtr                         _hist;
};

/*
 * \\class Image
 *
 * created on: Jul 2, 2019
 *
 */
class Image : public Metadata
{
friend ImagePtr;
  Image(RawRGBPtr);

public:
  virtual ~Image();

          RawRGBPtr               get_bits();

private:
  RawRGBPtr                       _other_source;
};

/*
 * \\class ImagePtr
 *
 * created on: Jul 2, 2019
 *
 */
class ImagePtr : public std::shared_ptr<Image>
{
public:
  ImagePtr()  : std::shared_ptr<Image>() { }
  ImagePtr(RawRGBPtr raw_rgb) : std::shared_ptr<Image>(new Image(raw_rgb))  { }
};


/*
 * \\class ImgBox
 *
 * created on: Jul 3, 2019
 *
 */
class ImageBox : public std::vector<ImagePtr>
{
public:
  ImageBox() : std::vector<ImagePtr>() {}
  ImageBox(RawRGBPtr raw_rgb)
  {
    push_back(ImagePtr(raw_rgb));
  }


  void                            append(const ImageBox& array)
  {
    for (ImagePtr img : array)
      push_back(img);
  }
};

class ImageConsumer;
/*
 * \\class ImageProducer
 *
 * created on: Jul 2, 2019
 *
 */
class ImageProducer
{
public:
  ImageProducer();
  virtual ~ImageProducer();

          void                    register_consumer(ImageConsumer*,const Metadata& meta = Metadata());
          void                    unregister_consumer(ImageConsumer*);
          void                    consume(ImageBox);

private:
  typedef std::unordered_set<ImageConsumer*> consumer_set;
  consumer_set                    _consumers;
  mutable std::mutex              _mutex;
  Metadata                        _meta;
};


/*
 * \\class ImageConsumer
 *
 * created on: Jul 2, 2019
 *
 */
class ImageConsumer
{
public:
  ImageConsumer() {}
  virtual ~ImageConsumer() {}

  virtual void                    consume(ImageBox) = 0;
};

} /* namespace image */
} /* namespace jupiter */
} /* namespace brt */

#endif /* IMAGE_IMAGE_HPP_ */
