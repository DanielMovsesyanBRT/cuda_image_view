/*
 * image_window.hpp
 *
 *  Created on: Jan 22, 2020
 *      Author: daniel
 */

#ifndef SOURCE_WINDOW_IMAGE_WINDOW_HPP_
#define SOURCE_WINDOW_IMAGE_WINDOW_HPP_

#include "window.hpp"
#include "image.hpp"

#include <GL/gl.h>
#include <GL/glx.h>

namespace brt
{
namespace jupiter
{
namespace window
{

/*
 * \\class ImageWindow
 *
 * created on: Jan 22, 2020
 *
 */
class ImageWindow : public Window
{
private:
  ImageWindow(const char* title, int x, int y,
      image::RawRGBPtr bits, Window* parent);
  virtual ~ImageWindow();

public:
  static  ImageWindow*            create(const char* title,
                                          Window* parent, image::RawRGBPtr bits,
                                          int x = RANDOM_POS, int y = RANDOM_POS);

  virtual bool                    x_event(Context,const XEvent &);

protected:
  virtual void                    x_create(Context);
  virtual void                    pre_create_window(Context);
  virtual void                    on_create_window(Context);

private:
          void                    show_img(Context);

private:
  image::RawRGBPtr                _bits;
  size_t                          _actual_width;
  size_t                          _actual_height;

  XVisualInfo*                    _vi;
  GC                              _gc;

  XFontStruct*                    _font;
  XColor                          _text_color;
  GLXContext                      _glc;
  GLuint                          _texture;
};

}
} /* namespace jupiter */
} /* namespace brt */

#endif /* SOURCE_WINDOW_IMAGE_WINDOW_HPP_ */
