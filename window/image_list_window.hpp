/*
 * image_list_window.hpp
 *
 *  Created on: Jan 24, 2020
 *      Author: daniel
 */

#ifndef SOURCE_WINDOW_IMAGE_LIST_WINDOW_HPP_
#define SOURCE_WINDOW_IMAGE_LIST_WINDOW_HPP_

#include "window.hpp"
#include "image.hpp"

#include <vector>
#include <string>

namespace brt
{
namespace jupiter
{
namespace window
{

/*
 * \\class ImageListWindow
 *
 * created on: Jan 24, 2020
 *
 */
class ImageListWindow: public Window
{
private:
  ImageListWindow(const std::vector<std::string>&, int x, int y);
  virtual ~ImageListWindow();
public:
  static  ImageListWindow*        create(const std::vector<std::string>&,
                                            int x = RANDOM_POS,
                                            int y = RANDOM_POS);
protected:
  virtual void                    x_create(Context);
  virtual void                    pre_create_window(Context);
  virtual void                    on_create_window(Context);

private:
          bool                    next_image();

private:
  std::vector<std::string>        _files;
  image::RawRGBPtr                _bits;
  size_t                          _index;
};

} /* namespace window */
} /* namespace jupiter */
} /* namespace brt */

#endif /* SOURCE_WINDOW_IMAGE_LIST_WINDOW_HPP_ */
