/*
 * WindowManager.hpp
 *
 *  Created on: Jun 28, 2019
 *      Author: daniel
 */

#ifndef WINDOW_WINDOW_MANAGER_HPP_
#define WINDOW_WINDOW_MANAGER_HPP_

#include <X11/X.h>
#include <X11/Xlib.h>
#include <climits>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <atomic>

#include "win_structures.hpp"
#include <utils.hpp>

namespace brt
{
namespace jupiter
{
namespace window
{
class Window;

/*
 * \\enum DisplayType
 *
 * created on: Nov 19, 2019
 *
 */
enum DisplayType
{
  eLocalDisplays = 1,
  eRemoteDisplay = 2,
  eAllDisplays = eLocalDisplays | eRemoteDisplay
};

/*
 * \\class WindowManager
 *
 * created on: Jun 28, 2019
 *
 */
class WindowManager
{
friend Window;

  WindowManager();
  virtual ~WindowManager();

public:
  static  WindowManager*          get() { return &_object; }
          void                    init();
          void                    release();
          Context                 get_context(X11Display display_name,bool create = false);

          X11Display              display(Context ctx) const;
          Display*                x11_display(Context ctx) const;
          int                     default_screen(Context ctx) const;
          int                     black_color(Context) const;
          int                     white_color(Context) const;

          WinSize                 resolution(Context,size_t screen_id) const;

          void                    post_message(Context, const bytestream& msg);

private:
  struct _Context
  {
    X11Display                      _display_name;
    Display*                        _display;
    std::vector<Screen*>            _screens;

    std::thread                     _thread;

    int                             _pipe[2];
    std::atomic_bool                _terminate;

    int                             _blackColor;
    int                             _whiteColor;
  };

          void                    x_loop(_Context*);
   static int                     x_error_handler(Display*, XErrorEvent*);

          void                    x_event(_Context*);
          void                    l_event(_Context*);


private:
  static WindowManager            _object;
  X11Display                      _default_display;

  typedef std::unordered_set<Window*> window_set;
  window_set                      _wind_set;

  std::unordered_set<_Context*>   _display_db;


  std::condition_variable         _cv;
  mutable std::mutex              _mutex;
};

} /* namespace window */
} /* namespace jupiter */
} /* namespace brt */

using wm = brt::jupiter::window::WindowManager;

#endif /* WINDOW_WINDOW_MANAGER_HPP_ */
