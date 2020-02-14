/*
 * WinStructures.hpp
 *
 *  Created on: Jun 28, 2019
 *      Author: daniel
 */

#ifndef WINDOW_WIN_STRUCTURES_HPP_
#define WINDOW_WIN_STRUCTURES_HPP_

#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <climits>
#include <vector>

#define RANDOM_POS                          (INT_MAX)


namespace brt
{
namespace jupiter
{
namespace window
{

class bytestream : public std::vector<uint8_t>
{
public:
  const uint8_t*                  buf() const
  {
    return data();
  }

  template <typename T>
  bytestream& operator<<(const T& data)
  {
    size_t cur_size = size();
    resize(cur_size + sizeof(T));
    memcpy(&at(cur_size),&data,sizeof(T));
    return *this;
  }
};

/*
 * \\enum name
 *
 * created on: Jul 1, 2019
 *
 */
enum LEvents
{
  eEventStop = 0,
  eCreateWindowEvent = 1,
  eShowImageStructure = 2,
  eCloseWindowEvent = 3,
  eUpdateWindowEvent = 4
};

/*
 * \\struct LEvent
 *
 * created on: Jul 1, 2019
 *
 */
struct LEvent
{
  uint32_t  _type;

  virtual ~LEvent() {}
  virtual bytestream              serialize()
  {
    bytestream result;
    result << _type;
    return result;
  }
};

/*
 * \\struct LStopEvent
 *
 * created on: Jul 1, 2019
 *
 */
struct LStopEvent : public LEvent
{
  LStopEvent() {_type = eEventStop; }
};


class Window;

/*
 * \\struct LWndEvent : public LEvent
 *
 * created on: Nov 19, 2019
 *
 */
struct LWindowEvent : public LEvent
{
  LWindowEvent(int pipe_fd, int type)
  : _wnd(nullptr)
  {
    _type = type;
    ::read(pipe_fd, &_wnd,sizeof(_wnd));
  }

  LWindowEvent(Window *wnd, int type)
  : _wnd(wnd)
  {
    _type = type;
  }

  virtual bytestream              serialize()
  {
    bytestream result = LEvent::serialize();
    result << _wnd;
    return result;
  }

  Window*                         _wnd;
};

/*
 * \\struct LCreateWindowEvent
 *
 * created on: Jul 1, 2019
 *
 */
struct LCreateWindowEvent : public LWindowEvent
{
  LCreateWindowEvent(int pipe_fd)
  : LWindowEvent(pipe_fd, eCreateWindowEvent)
  {  }

  LCreateWindowEvent(Window *wnd)
  : LWindowEvent(wnd, eCreateWindowEvent)
  {  }
};


/*
 * \\struct LShowImageEvent
 *
 * created on: Jul 1, 2019
 *
 */
struct LShowImageEvent : public LWindowEvent
{
  LShowImageEvent(int pipe_fd)
  : LWindowEvent(pipe_fd, eShowImageStructure)
  {
    ::read(pipe_fd, &_id,sizeof(_id));
  }

  LShowImageEvent(size_t id, Window* wnd)
  : LWindowEvent(wnd, eShowImageStructure)
  , _id(id)
  {
  }

  virtual bytestream              serialize()
  {
    bytestream result = LWindowEvent::serialize();
    result << _id;
    return result;
  }

  size_t                          _id;
};

/*
 * \\struct LCloseWindowEvent
 *
 * created on: Nov 19, 2019
 *
 */
struct LCloseWindowEvent : public LWindowEvent
{
  LCloseWindowEvent(int pipe_fd)
  : LWindowEvent(pipe_fd, eCloseWindowEvent)
  {  }

  LCloseWindowEvent(Window *wnd)
  : LWindowEvent(wnd, eCloseWindowEvent)
  {  }
};

/*
 * \\struct LUpdateWindowEvent
 *
 * created on: Nov 19, 2019
 *
 */
struct LUpdateWindowEvent : public LWindowEvent
{
  LUpdateWindowEvent(int pipe_fd)
  : LWindowEvent(pipe_fd, eUpdateWindowEvent)
  {  }

  LUpdateWindowEvent(Window *wnd)
  : LWindowEvent(wnd, eUpdateWindowEvent)
  {  }
};


/*
 * \\struct WinPoint
 *
 * created on: Nov 20, 2019
 *
 */
struct WinPoint
{
  WinPoint() : _x(0), _y(0) {}
  WinPoint(int x, int y) : _x(x), _y(y) {}
  WinPoint(const WinPoint& point) : _x(point._x), _y(point._y) {}

  int                             _x,_y;
};

/*
 * \\struct WinSize
 *
 * created on: Jun 28, 2019
 *
 */
struct WinSize
{
  WinSize() : _width(0), _height(0) {}
  WinSize(int width, int height) : _width(width), _height(height) {}
  WinSize(const WinSize& size) : _width(size._width), _height(size._height) {}

  int                             _width;
  int                             _height;
};


/*
 * \\struct WinRect
 *
 * created on: Nov 20, 2019
 *
 */
struct WinRect
{
  WinRect() : _origin(), _size() {}
  WinRect(int x,int y,int width,int height) : _origin(x, y), _size(width,height) {}
  WinRect(const WinRect& rect) : _origin(rect._origin), _size(rect._size) {}

  WinPoint                        _origin;
  WinSize                         _size;

          bool                    pt_in_rect(const WinPoint &point)
          {
            return ((point._x >= _origin._x) &&
                    (point._y >= _origin._y) &&
                    (point._x <= (_origin._x + _size._width)) &&
                    (point._y <= (_origin._y + _size._height)));
          }
};

typedef void* Context;


} /* namespace window */
} /* namespace jupiter */
} /* namespace brt */


#endif /* WINDOW_WIN_STRUCTURES_HPP_ */
