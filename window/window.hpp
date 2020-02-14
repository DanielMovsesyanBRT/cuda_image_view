/*
 * window.hpp
 *
 *  Created on: Nov 19, 2019
 *      Author: daniel
 */

#ifndef WINDOW_WINDOW_HPP_
#define WINDOW_WINDOW_HPP_

#include <X11/Xlib.h>
#include "win_structures.hpp"
#include "utils.hpp"

#include <string>
#include <mutex>
#include <atomic>

namespace brt
{
namespace jupiter
{
namespace window
{

/*
 * \\struct Point
 *
 * created on: Nov 26, 2019
 *
 */
struct Point
{
  int   x,y;
};


/*
 * \\struct Rect
 *
 * created on: Nov 26, 2019
 *
 */
struct Rect
{
  int  left,right,top,bottom;
};

/*
 * \\class Window
 *
 * created on: Nov 19, 2019
 *
 */
class Window
{
protected:
  Window(const char* title, int x, int y,
              size_t width, size_t height, Window* parent);

  virtual ~Window();

public:
  static  Window*                 create(const char* title,
                                          Window* parent, size_t width, size_t height,
                                          int x = RANDOM_POS, int y = RANDOM_POS);

          void                    show(X11Display display_name = X11Display());

  virtual bool                    x_event(Context,const XEvent &);
  virtual bool                    l_event(Context,const LEvent *);

          bool                    is_window() const { return handle() != 0; }
          ::Window                handle() const { return _handle; }

          X11Display              display(Context = nullptr) const;
          Display*                x11_display(Context = nullptr) const;
          int                     screen(Context = nullptr) const;

          void                    close();
          void                    update();

          WinRect                 get_window_rect() const;

protected:
  virtual void                    x_create(Context);
  virtual void                    pre_create_window(Context) {}
  virtual void                    on_create_window(Context) {}

  virtual void                    x_close(Context);
  virtual void                    on_close(Context) {}

  virtual void                    on_draw(Context,GC) {}


protected:
  std::atomic<::Window>           _handle;
  Window*                         _parent;
  Context                         _context;
  mutable std::mutex              _mutex;

  // Attributes
  std::string                     _title;
  int                             _x, _y;
  size_t                          _width, _height;

  Visual*                         _visual;
  int                             _depth;
  unsigned int                    _class;
  unsigned long                   _value_mask;
  XSetWindowAttributes            _swa;
  int                             _border_width;
};

} /* namespace window */
} /* namespace jupiter */
} /* namespace brt */

#endif /* WINDOW_WINDOW_HPP_ */
