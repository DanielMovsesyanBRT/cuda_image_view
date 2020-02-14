/*
 * debayer.hpp
 *
 *  Created on: Feb 11, 2020
 *      Author: daniel
 */

#ifndef DEBAYER_HPP_
#define DEBAYER_HPP_

#include "image.hpp"
#include <cassert>

#define DEFAULT_NUMBER_OF_THREADS           (64)

namespace brt {
namespace jupiter {


class Debayer_Impl;
/*
 * \\class CudaImage
 *
 * created on: Feb 11, 2020
 *
 */
class Debayer
{
public:
  Debayer();
  virtual ~Debayer();

          bool                    init(size_t width,size_t height,image::PixelType,size_t small_hits_size);
  	  	  image::RawRGBPtr        debayer(image::RawRGBPtr);
  	  	  bool                    get_histogram(image::HistPtr& histogram);
private:
  Debayer_Impl*                   _impl;
};

} // jupiter
} // brt





#endif /* DEBAYER_HPP_ */
