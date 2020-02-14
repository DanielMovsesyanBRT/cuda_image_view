/*
 * camera_window.hpp
 *
 *  Created on: Nov 21, 2019
 *      Author: daniel
 */

#ifndef SOURCE_WINDOW_CAMERA_WINDOW_HPP_
#define SOURCE_WINDOW_CAMERA_WINDOW_HPP_

#include "window.hpp"

#include <GL/gl.h>
#include <GL/glx.h>

#include <string>
#include <mutex>
#include <atomic>

#include "image.hpp"
#include "image_processor.hpp"

namespace brt
{
namespace jupiter
{
namespace window
{

/*
 * \\class CameraWindow
 *
 * created on: Nov 21, 2019
 *
 */
class CameraWindow : public Window
                   , public image::ImageConsumer
{
  CameraWindow(const char* title, int x, int y,
            size_t width, size_t height, Window* parent);
  virtual ~CameraWindow();
public:

  static  CameraWindow*           create(const char* title,
                                          Window* parent, size_t width, size_t height,
                                          int x = RANDOM_POS, int y = RANDOM_POS);

  virtual bool                    x_event(Context,const XEvent &);
  virtual bool                    l_event(Context,const LEvent *);

          size_t                  add_subwnd(image::ImageProducer* ip);

  virtual void                    consume(image::ImageBox);

protected:
  virtual void                    x_create(Context);
  virtual void                    pre_create_window(Context);
  virtual void                    on_create_window(Context);

          void                    show_video(Context ,LShowImageEvent*);

private:
  struct GLWindow
  {
    GLfloat                         _left;
    GLfloat                         _top;
    GLfloat                         _right;
    GLfloat                         _bottom;
    GLuint                          _tex;
    size_t                          _col;
    size_t                          _row;
    image::RawRGBPtr                _image;

    std::shared_ptr<std::vector<std::string>>
                                    _text;
  };

          Rect                    gl_rect(const GLWindow& wnd);

private:
  size_t                          _video_width, _video_height;
  size_t                          _actual_width, _actual_height;
  size_t                          _rows, _cols;
  GLXContext                      _glc;
  GLuint                          _texture;

  std::vector<GLWindow>           _gl_map;
  std::atomic_int_fast32_t        _click;
  uint32_t                        _global_number;

  XVisualInfo*                    _vi;
  XFontStruct*                    _font;
  GC                              _gc;
  XColor                          _text_color;
};

} /* namespace window */
} /* namespace jupiter */
} /* namespace brt */

#endif /* SOURCE_WINDOW_CAMERA_WINDOW_HPP_ */
