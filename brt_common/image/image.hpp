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
