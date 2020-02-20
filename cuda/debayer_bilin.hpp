/*
 * debayer_bilin.hpp
 *
 *  Created on: Feb 19, 2020
 *      Author: daniel
 */

#ifndef DEBAYER_BILIN_HPP_
#define DEBAYER_BILIN_HPP_


#include <stdint.h>
#include <vector>
#include <atomic>

#include "image.hpp"

#define DEFAULT_NUMBER_OF_THREADS           (64)

namespace brt
{
namespace jupiter
{


class Debayer_Bilinear_Impl;

/*
 * \\class Debayer_Bilinear
 *
 * created on: Nov 25, 2019
 *
 */
class Debayer_Bilinear : public image::ImageConsumer
                       , public image::ImageProducer
{
public:
  Debayer_Bilinear();
  virtual ~Debayer_Bilinear();

          image::RawRGBPtr        ahd(image::RawRGBPtr img);
  virtual void                    consume(image::ImageBox box);

private:
  Debayer_Bilinear_Impl*          _impl;
  size_t                          _width;
  size_t                          _height;
};

} /* namespace jupiter */
} /* namespace brt */



#endif /* DEBAYER_BILIN_HPP_ */
