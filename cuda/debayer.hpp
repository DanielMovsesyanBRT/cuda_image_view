/*
 * debayer.hpp
 *
 *  Created on: Feb 14, 2020
 *      Author: daniel
 */

#ifndef SOURCE_IMAGE_DEBAYER_HPP_
#define SOURCE_IMAGE_DEBAYER_HPP_

#include <stdint.h>
#include <stddef.h>

#include "image.hpp"

#define DEFAULT_NUMBER_OF_THREADS           (64)

namespace brt
{
namespace jupiter
{

class Debayer_impl;
/*
 * \\class Debayer
 *
 * created on: Feb 14, 2020
 *
 */
class Debayer : public image::ImageConsumer
              , public image::ImageProducer
{
public:
  Debayer();
  virtual ~Debayer();

          bool                    init(size_t width,size_t height,size_t small_hits_size);
          image::RawRGBPtr        ahd(image::RawRGBPtr img);

  virtual void                    consume(image::ImageBox box);

private:
  Debayer_impl*                   _impl;
  size_t                          _width;
  size_t                          _height;
};

} /* namespace jupiter */
} /* namespace brt */

#endif /* SOURCE_IMAGE_DEBAYER_HPP_ */
