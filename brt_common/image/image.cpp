/*
 * Image.cpp
 *
 *  Created on: Jul 2, 2019
 *      Author: daniel
 */

#include <string.h>
#include <iostream>
#include <fstream>

#include "image.hpp"
#include <utils.hpp>

namespace brt
{
namespace jupiter
{
namespace image
{

/*
 * \\fn Constructor RawRGB::RawRGB
 *
 * created on: Nov 25, 2019
 * author: daniel
 *
 */
RawRGB::RawRGB(size_t w, size_t h, size_t depth, PixelType type /*= eBayer*/)
{
  _width = w;
  _height = h;
  _depth = depth;
  _type = type;

  uint32_t srcPitch = _width * BYTES_PER_PIXELS(_depth) * type_size(_type);
  uint32_t srcImageSize = srcPitch * _height;

  if (srcImageSize > 0)
    _buffer = (uint8_t*)::malloc(srcImageSize);
  else
    _buffer = nullptr;
}

/*
 * \\fn Constructor RawRGB::RawRGB
 *
 * created on: Aug 8, 2019
 * author: daniel
 *
 */
RawRGB::RawRGB(const uint8_t* buffer, size_t w, size_t h, size_t depth, PixelType type /*= eBayer*/)
: _buffer(nullptr)
{
  _width = w;
  _height = h;
  _depth = depth;
  _type = type;

  uint32_t srcPitch = _width * BYTES_PER_PIXELS(_depth) * type_size(_type);
  uint32_t srcImageSize = srcPitch * _height;

  if (srcImageSize > 0)
  {
    _buffer = (uint8_t*)::malloc(srcImageSize);
    memcpy(_buffer, buffer, srcImageSize);
  }
}

/*
 * \\fn Constructor RawRGB::RawRGB
 *
 * created on: Aug 9, 2019
 * author: daniel
 *
 */
RawRGB::RawRGB(const char *raw_image_file)
: _width(0)
, _height(0)
, _depth(0)
, _type(eBayer)
, _buffer(nullptr)
{
  std::ifstream image_file(raw_image_file, std::ios::in | std::ios::binary);
  if (image_file.is_open())
  {
    try
    {
      uint32_t w,h, depth;
      if (image_file.read(reinterpret_cast<char*>(&w), sizeof(w)).rdstate() != std::ios_base::goodbit)
        throw;

      if (image_file.read(reinterpret_cast<char*>(&h), sizeof(h)).rdstate() != std::ios_base::goodbit)
        throw;

      if (image_file.read(reinterpret_cast<char*>(&depth), sizeof(depth)).rdstate() != std::ios_base::goodbit)
        throw;

      /// Backward compatibility
      if (depth == 2)
        depth = 16;

      _buffer = new uint8_t[w * h * BYTES_PER_PIXELS(depth)];

      if (image_file.read(reinterpret_cast<char*>(_buffer), w * h * BYTES_PER_PIXELS(depth)).rdstate() &
          ((std::ios_base::badbit | std::ios_base::failbit) != 0))
        throw;

      _depth = depth;
      _width = w;
      _height = h;
    }
    catch(...)
    {
      if (_buffer != nullptr)
      {
        delete _buffer;
        _buffer = nullptr;
      }
    }
  }
}

/*
 * \\fn Destructor RawRGB::~RawRGB
 *
 * created on: Jul 2, 2019
 * author: daniel
 *
 */
RawRGB::~RawRGB()
{
  if (_buffer != nullptr)
    free(_buffer);
}
//
///*
// * \\fn Pixel RawRGB::pixel
// *
// * created on: Jan 23, 2020
// * author: daniel
// *
// */
//Pixel RawRGB::pixel(int x, int y)
//{
//  if ((_buffer == nullptr) ||
//      (x >= static_cast<int>(_width)) ||
//      (y >= static_cast<int>(_height)) ||
//      (x < 0) || (y < 0))
//    return Pixel();
//
//  size_t offset = (x + y * _width) * BYTES_PER_PIXELS(_depth) * type_size(_type);
//  return Pixel(_buffer + offset,_type, _depth);
//}

/*
 * \\fn RawRGBPtr RawRGB::clone
 *
 * created on: Jan 23, 2020
 * author: daniel
 *
 */
RawRGBPtr RawRGB::clone(size_t depth)
{
  if (depth == _depth)
    return RawRGBPtr(new RawRGB(_buffer, _width, _height, _depth, _type));

  RawRGB* result = new RawRGB(_width, _height, depth, _type);

  uint8_t*  src = _buffer;
  uint8_t*  dst = result->_buffer;

  for (size_t h = 0; h < _height; h++)
  {
    for (size_t w = 0; w < _width; w++)
    {
      for (size_t byte = 0; byte < type_size(_type); byte++)
      {
        uint32_t pixel = 0;
        switch (BYTES_PER_PIXELS(_depth))
        {
        case 1:
          pixel = *src;
          break;

        case 2:
          pixel = *reinterpret_cast<uint16_t*>(src);
          break;

        case 3:
          pixel = *reinterpret_cast<uint32_t*>(src) & 0xFFFFFF;
          break;

        case 4:
          pixel = *reinterpret_cast<uint32_t*>(src);
          break;

        default:
          break;
        }

        if (_depth > depth)
          pixel >>= (_depth - depth);
        else
          pixel <<= (depth - _depth);

        switch (BYTES_PER_PIXELS(depth))
        {
        case 1:
          *dst = static_cast<uint8_t>(pixel & 0xFF);
          break;

        case 2:
          *reinterpret_cast<uint16_t*>(dst) = static_cast<uint16_t>(pixel & 0xFFFF);
          break;

        case 3:
          *reinterpret_cast<uint32_t*>(dst) = pixel & 0xFFFFFF;
          break;

        case 4:
          *reinterpret_cast<uint32_t*>(dst) = pixel;
          break;

        default:
          break;
        }

        src += BYTES_PER_PIXELS(_depth);
        dst += BYTES_PER_PIXELS(depth);
      }
    }
  }

  return RawRGBPtr(result);
}

/*
 * \\fn Constructor Image::Image
 *
 * created on: Jul 2, 2019
 * author: daniel
 *
 */
Image::Image(RawRGBPtr other_source)
: _other_source(other_source)
{

}

/*
 * \\fn Destructor Image::~Image
 *
 * created on: Jul 2, 2019
 * author: daniel
 *
 */
Image::~Image()
{
}

/*
 * \\fn RawRGBPtr Image::get_bits
 *
 * created on: Jul 2, 2019
 * author: daniel
 *
 */
RawRGBPtr Image::get_bits()
{
  return _other_source;
}

/*
 * \\fn Constructor ImageProducer::ImageProducer
 *
 * created on: Jul 2, 2019
 * author: daniel
 *
 */
ImageProducer::ImageProducer()
{

}

/*
 * \\fn Destructor ImageProducer::~ImageProducer
 *
 * created on: Jul 2, 2019
 * author: daniel
 *
 */
ImageProducer::~ImageProducer()
{

}

/*
 * \\fn void ImageProducer::register_consumer
 *
 * created on: Jul 2, 2019
 * author: daniel
 *
 */
void ImageProducer::register_consumer(ImageConsumer* consumer,const Metadata& meta /*= Metadata()*/)
{
  std::unique_lock<std::mutex> l(_mutex);
  _consumers.insert(consumer);
  _meta += meta;
}

/*
 * \\fn void ImageProducer::unregister_consumer
 *
 * created on: Jul 2, 2019
 * author: daniel
 *
 */
void ImageProducer::unregister_consumer(ImageConsumer* consumer)
{
  std::unique_lock<std::mutex> l(_mutex);
  _consumers.erase(consumer);
}

/*
 * \\fn void ImageProducer::consume
 *
 * created on: Jul 2, 2019
 * author: daniel
 *
 */
void ImageProducer::consume(ImageBox box)
{
  std::unique_lock<std::mutex> l(_mutex);

  for (ImagePtr img : box)
  {
    if (img)
      *(img.get()) += _meta;
  }

  for (ImageConsumer* consumer : _consumers)
  {
    consumer->consume(box);
  }
}


} /* namespace image */
} /* namespace jupiter */
} /* namespace brt */
