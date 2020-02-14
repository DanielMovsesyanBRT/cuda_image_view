/*
 * WindowManager.cpp
 *
 *  Created on: Jun 28, 2019
 *      Author: daniel
 */

#include <iostream>
#include <algorithm>
//#include <chrono>
#include <cmath>

#include <stdio.h>
#include <dirent.h>
#include <string.h>

#include "window.hpp"
#include "window_manager.hpp"

#define _PATH_PROCNET_X11                   "/tmp/.X11-unix"
#define _PATH_PROCNET_TCP                   "/proc/net/tcp"
#define _PATH_PROCNET_TCP6                  "/proc/net/tcp6"
#define X11_PORT_MIN                        (6000)
#define X11_PORT_MAX                        (6100)

//using namespace std::chrono_literals;

namespace brt
{
namespace jupiter
{
namespace window
{

WindowManager WindowManager::_object;

/*
 * \\fn Constructor WindowManager::WindowManager
 *
 * created on: Jun 28, 2019
 * author: daniel
 *
 */
WindowManager::WindowManager()
: _wind_set()
, _cv()
, _mutex()
{
  srand(time(nullptr));
}

/*
 * \\fn Destructor WindowManager::~WindowManager
 *
 * created on: Jun 28, 2019
 * author: daniel
 *
 */
WindowManager::~WindowManager()
{
}


/*
 * \\fn void WindowManager::init
 *
 * created on: Jun 28, 2019
 * author: daniel
 *
 */
void WindowManager::init()
{
  XInitThreads();
  _default_display = Utils::aquire_display("videos");
}

/*
 * \\fn Context WindowManager::get_context
 *
 * created on: Nov 20, 2019
 * author: daniel
 *
 */
Context WindowManager::get_context(X11Display diplay_name,bool create /*= false*/)
{
  if (!diplay_name)
    diplay_name = _default_display;

  std::unordered_set<_Context*>::iterator iter =
      std::find_if(_display_db.begin(), _display_db.end(),[diplay_name](_Context* db)
  {
    return db->_display_name == diplay_name;
  });

  if (iter == _display_db.end())
  {
    if (!create)
      return nullptr;

    _Context* db = new _Context;
    db->_display_name = (diplay_name) ? diplay_name : X11Display();
    pipe(db->_pipe);
    db->_terminate.store(false);

    db->_thread  = std::thread([](WindowManager* m, _Context* d)
    {
      pthread_setname_np(pthread_self(), "wnd_mngr");

      m->x_loop(d);
    },this, db);

    _mutex.lock();
    iter  = _display_db.insert(db).first;
    _mutex.unlock();
  }

  return (*iter);
}

/*
 * \\fn void WindowManager::release
 *
 * created on: Jun 28, 2019
 * author: daniel
 *
 */
void WindowManager::release()
{
  while (_wind_set.size())
  {
    (*_wind_set.begin())->close();
    _wind_set.erase(_wind_set.begin());
  }

  while (_display_db.size())
  {
    LStopEvent se;

    post_message(*_display_db.begin(), se.serialize());
    (*_display_db.begin())->_thread.join();
    delete (*_display_db.begin());
    _display_db.erase(_display_db.begin());
  }
}

/*
 * \\fn X11Display WindowManager::display_name
 *
 * created on: Jan 23, 2020
 * author: daniel
 *
 */
X11Display WindowManager::display(Context ctx) const
{
  if (ctx == nullptr)
    return X11Display();

  return reinterpret_cast<_Context*>(ctx)->_display_name;
}

/*
 * \\fn Display* WindowManager::display
 *
 * created on: Nov 19, 2019
 * author: daniel
 *
 */
Display* WindowManager::x11_display(Context ctx) const
{
  if (ctx == nullptr)
    return nullptr;

  return reinterpret_cast<_Context*>(ctx)->_display;
}

/*
 * \\fn Screen WindowManager::default_screen
 *
 * created on: Jun 28, 2019
 * author: daniel
 *
 */
int WindowManager::default_screen(Context ctx) const
{
  if (ctx == nullptr)
    return -1;

  return DefaultScreen(x11_display(ctx));
}

/*
 * \\fn int WindowManager::black_color
 *
 * created on: Nov 19, 2019
 * author: daniel
 *
 */
int WindowManager::black_color(Context ctx) const
{
  if (ctx == nullptr)
    return -1;

  return reinterpret_cast<_Context*>(ctx)->_blackColor;
}

/*
 * \\fn WindowManager::white_color
 *
 * created on: Nov 19, 2019
 * author: daniel
 *
 */
int WindowManager::white_color(Context ctx) const
{
  if (ctx == nullptr)
    return -1;

  return reinterpret_cast<_Context*>(ctx)->_whiteColor;
}

/*
 * \\fn WinSize WindowManager::resolution
 *
 * created on: Jun 28, 2019
 * author: daniel
 *
 */
WinSize WindowManager::resolution(Context context,size_t screen_id) const
{
  _Context *ctx = reinterpret_cast<_Context*>(context);
  if (ctx == nullptr)
    return WinSize(-1,-1);

  if (screen_id >= ctx->_screens.size())
    return WinSize(-1,-1);

  return WinSize(ctx->_screens[screen_id]->width, ctx->_screens[screen_id]->height);
}

/*
 * \\fn void WindowManager::post_message
 *
 * created on: Jul 1, 2019
 * author: daniel
 *
 */
void WindowManager::post_message(Context ctx, const bytestream& msg)
{
  if (ctx != nullptr)
  {
    std::lock_guard<std::mutex> l(_mutex);
    ::write(reinterpret_cast<_Context*>(ctx)->_pipe[1],msg.buf(),msg.size());
  }
}

/*
 * \\fn void WindowManager::x_loop
 *
 * created on: Jun 28, 2019
 * author: daniel
 *
 */
void WindowManager::x_loop(_Context* ctx)
{
  XSetErrorHandler(x_error_handler);
  Display* dsp = XOpenDisplay((ctx->_display_name) ? ctx->_display_name._name.c_str() : nullptr);

  if (dsp == nullptr)
  {
    std::cerr << "Error: cannot open display " << std::endl;
    return;
  }

  _mutex.lock();
  ctx->_display = dsp;
  ctx->_blackColor = BlackPixel(dsp, DefaultScreen(dsp));
  ctx->_whiteColor = WhitePixel(dsp, DefaultScreen(dsp));

  int number_of_screens = ScreenCount(dsp);
  for (int index = 0; index < number_of_screens; index++)
  {
    Screen* sc = ScreenOfDisplay(dsp, index);
    if (sc != nullptr)
      ctx->_screens.push_back(sc);
  }
  _mutex.unlock();

  int     x11_fd = ConnectionNumber(dsp);
  fd_set  in_fds;
  while(!ctx->_terminate)
  {
    FD_ZERO(&in_fds);
    FD_SET(ctx->_pipe[0],&in_fds);
    FD_SET(x11_fd,&in_fds);

    if (::select(std::max(ctx->_pipe[0],x11_fd) + 1, &in_fds, nullptr, nullptr, nullptr) > 0)
    {
      if (FD_ISSET(ctx->_pipe[0], &in_fds))
        l_event(ctx);

      if (FD_ISSET(x11_fd, &in_fds))
        x_event(ctx);
    }
  }

  XCloseDisplay(dsp);
}


/*
 * \\fn void WindowManager::x_event
 *
 * created on: Nov 19, 2019
 * author: daniel
 *
 */
void WindowManager::x_event(_Context* ctx)
{
  XEvent e;
  while(!ctx->_terminate && XPending(ctx->_display))
  {
    XNextEvent(ctx->_display, &e);
    if (e.type == DestroyNotify) // MapNotify)
      ctx->_terminate = true;

    _mutex.lock();
    window_set::iterator iter = std::find_if(_wind_set.begin(), _wind_set.end(), [e](Window* wnd)->bool
    {
      return (wnd->handle() ==  e.xany.window);
    });

    if (iter != _wind_set.end())
    {
      (*iter)->x_event(ctx, e);
      if (e.type == ClientMessage)
        _wind_set.erase(iter);
    }

    _mutex.unlock();

  }
}

/*
 * \\fn void WindowManager::l_event
 *
 * created on: Jul 1, 2019
 * author: daniel
 *
 */
void WindowManager::l_event(_Context* ctx)
{
  uint32_t  type;
  if (::read(ctx->_pipe[0], &type, sizeof(type)) != sizeof(type))
    return;

  switch (type)
  {
  case eEventStop:
    ctx->_terminate.store(true);
    break;

  case eCreateWindowEvent:
    {
      LCreateWindowEvent ce(ctx->_pipe[0]);
      if (ce._wnd != nullptr)
        ce._wnd->l_event(ctx, &ce);
    }
    break;

  case eShowImageStructure:
    {
      LShowImageEvent se(ctx->_pipe[0]);
      if (se._wnd != nullptr)
        se._wnd->l_event(ctx, &se);
    }
    break;

  case eCloseWindowEvent:
    {
      LCloseWindowEvent se(ctx->_pipe[0]);

      std::lock_guard<std::mutex> l(_mutex);
      if ((se._wnd != nullptr) && (_wind_set.find(se._wnd) != _wind_set.end()))
      {
        _wind_set.erase(se._wnd);
        se._wnd->l_event(ctx, &se);
      }
    }
    break;

  case eUpdateWindowEvent:
    {
      LUpdateWindowEvent ue(ctx->_pipe[0]);
      if (ue._wnd != nullptr)
        ue._wnd->l_event(ctx, &ue);
    }
    break;

  default:
    break;
  }
}

/*
 * \\fn int WindowManager::x_error_handler
 *
 * created on: Jun 29, 2019
 * author: daniel
 *
 */
int WindowManager::x_error_handler(Display* display, XErrorEvent* err)
{
  char text[1024];
  XGetErrorText(display,err->error_code,text,sizeof(text));
  std::cerr << "Error: " << text << std::endl;
  std::cerr << "Request code: " << err->request_code << std::endl;
  std::cerr << "Minor code: " << err->minor_code << std::endl;
  return 0;
}

} /* namespace window */
} /* namespace jupiter */
} /* namespace brt */
