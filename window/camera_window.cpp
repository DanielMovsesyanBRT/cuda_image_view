/*
 * camera_window.cpp
 *
 *  Created on: Nov 21, 2019
 *      Author: daniel
 */

#include "camera_window.hpp"
#include "window_manager.hpp"
// #include "cuda_debayering.h"

#include <iostream>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>

#include <utils.hpp>
#include <cuda_utils.hpp>


namespace brt
{
namespace jupiter
{
namespace window
{

/*
 * \\fn Constructor CameraWindow::CameraWindow
 *
 * created on: Nov 21, 2019
 * author: daniel
 *
 */
CameraWindow::CameraWindow(const char* title, int x, int y,
    size_t width, size_t height, Window* parent)
: Window(title, x, y, width,  height, parent)
, _video_width(width)
, _video_height(height)
, _actual_width(width)
, _actual_height(height)
, _rows(0)
, _cols(0)
, _glc(0)
, _texture(0)
, _click(-1)
, _global_number(0)
, _vi(nullptr)
, _font(nullptr)
, _gc(nullptr)
{

}

/*
 * \\fn Destructor CameraWindow::~CameraWindow
 *
 * created on: Nov 21, 2019
 * author: daniel
 *
 */
CameraWindow::~CameraWindow()
{
}


/*
 * \\fn CameraWindow* CameraWindow::create
 *
 * created on: Nov 21, 2019
 * author: daniel
 *
 */
CameraWindow* CameraWindow::create(const char* title, Window* parent, size_t width, size_t height,
                                        int x /*= RANDOM_POS*/, int y /*= RANDOM_POS*/)
{
  CameraWindow* wnd = new CameraWindow(title, x, y, width, height, parent);
  return wnd;
}

/*
 * \\fn bool CameraWindow::x_event
 *
 * created on: Nov 21, 2019
 * author: daniel
 *
 */
bool CameraWindow::x_event(Context ctx,const XEvent &event)
{
  switch (event.type)
  {
  case Expose:
    {
      XWindowAttributes attribs;
      XGetWindowAttributes(wm::get()->x11_display(ctx), handle(), &attribs);
      glViewport(0, 0, attribs.width, attribs.height);

      glXSwapBuffers(wm::get()->x11_display(ctx), handle());
    }
    return true;

  case ButtonPress:
    {
      int col = event.xbutton.x / _actual_width;
      int row = event.xbutton.y / _actual_height;

      size_t id = (size_t)-1;

      _mutex.lock();
      for (size_t index = 0; index < _gl_map.size(); index++)
      {
        if ((_gl_map[index]._col == static_cast<size_t>(col)) && (_gl_map[index]._row == static_cast<size_t>(row)))
        {
          id = index;
          break;
        }
      }
      _mutex.unlock();

      if (id < 4)
      {
        int_fast32_t expected = -1;
        _click.compare_exchange_strong(expected, id);
      }
      std::cout << "Button Press x=" << col << "; y=" << row << "; id=" << id << std::endl;
    }
    return true;

  default:break;
  }
  return Window::x_event(ctx,event);
}


/*
 * \\fn bool CameraWindow::l_event
 *
 * created on: Nov 21, 2019
 * author: daniel
 *
 */
bool CameraWindow::l_event(Context ctx,const LEvent *event)
{
  switch(event->_type)
  {
  case eShowImageStructure:
    show_video(ctx,(LShowImageEvent*)event);
    return true;

  default:
    break;
  }

  return Window::l_event(ctx,event);
}

/*
 * \\fn size_t CameraWindow::add_subwnd
 *
 * created on: Nov 21, 2019
 * author: daniel
 *
 */
size_t CameraWindow::add_subwnd(image::ImageProducer* ip)
{
  _mutex.lock();
  size_t num_items = _gl_map.size();

  double min_square = std::sqrt((double)num_items + 1.0);
  _cols = (size_t )min_square;
  _rows = (size_t )min_square;

  if (min_square > std::floor(min_square))
    _cols++;

  float fcol_size = 2.0 / _cols;
  float frow_size = 2.0 / _rows;

  size_t row = 0, col = 0, index = 0;
  for (col = 0; col < _cols; col++)
  {
    for (row = 0; row < _rows; row++)
    {
      index = col * _rows + row;
      if (index >= num_items)
        break;

      _gl_map[index]._left = -1.0 + col * fcol_size;
      _gl_map[index]._top  = -1.0 + row * frow_size;
      _gl_map[index]._right = -1.0 + (col + 1) * fcol_size;
      _gl_map[index]._bottom = -1.0 + (row + 1) * frow_size;
      _gl_map[index]._col = col;
      _gl_map[index]._row = row;
    }

    if (index >= num_items)
      break;
  }

  GLWindow  wnd;
  wnd._left = -1.0 + col * fcol_size;
  wnd._top  = -1.0 + row * frow_size;
  wnd._right = -1.0 + (col + 1) * fcol_size;
  wnd._bottom = -1.0 + (row + 1) * frow_size;
  wnd._col = col;
  wnd._row = row;
  wnd._text.reset(new std::vector<std::string>());

  //wnd._ip.reset(new image::ImageProcessor());

  _gl_map.push_back(wnd);
  _mutex.unlock();

  if (ip != nullptr)
    ip->register_consumer(this,Metadata().set("id",num_items));

  return num_items;
}

/*
 * \\fn void CameraWindow::consume
 *
 * created on: Nov 21, 2019
 * author: daniel
 *
 */
void CameraWindow::consume(image::ImageBox box)
{
  if (!is_window())
    return;

  if (box.empty())
    return;

  int id = box[0]->get("id",-1);
  if ((id == -1) || box.empty())
    return;

  std::unique_lock<std::mutex> l(_mutex);
  if (id >= static_cast<int>(_gl_map.size()))
    return;

  if (_gl_map[id]._image)
    return;

  _gl_map[id]._image = box[0]->get_bits();
  wm::get()->post_message(_context, LShowImageEvent(id, this).serialize());
}

/*
 * \\fn void CameraWindow::x_create
 *
 * created on: Nov 21, 2019
 * author: daniel
 *
 */
void CameraWindow::x_create(Context context)
{
  WinSize screen_size = wm::get()->resolution(context, wm::get()->default_screen(context));

  int full_width = _actual_width * _cols;
  int full_height = _actual_height * _rows;

#define WIDTH_GAP             (150)
#define HEIGHT_GAP            (50)

  if (full_width > (screen_size._width - WIDTH_GAP))
  {
    float ratio = (float)full_width / (screen_size._width - WIDTH_GAP);
    _actual_width = (size_t)((float)_actual_width / ratio);
    _actual_height = (size_t)((float)_actual_height / ratio);

    full_width = _actual_width * _cols;
    full_height = _actual_height * _rows;
  }

  if (full_height > (screen_size._height - HEIGHT_GAP))
  {
    float ratio = (float)full_height / (screen_size._height - HEIGHT_GAP);
    _actual_width = (size_t)((float)_actual_width / ratio);
    _actual_height = (size_t)((float)_actual_height / ratio);

    full_width = _actual_width * _cols;
    full_height = _actual_height * _rows;
  }

  _width = full_width;
  _height = full_height;

  Window::x_create(context);
}

/*
 * \\fn void CameraWindow::pre_create_window
 *
 * created on: Nov 21, 2019
 * author: daniel
 *
 */
void CameraWindow::pre_create_window(Context context)
{
  GLint att[] =
  {
    GLX_RGBA,
    GLX_DOUBLEBUFFER,
    GLX_DEPTH_SIZE, 24,
    GLX_STENCIL_SIZE, 8,
    GLX_RED_SIZE, 8,
    GLX_GREEN_SIZE, 8,
    GLX_BLUE_SIZE, 8,
    GLX_SAMPLE_BUFFERS, 0,
    GLX_SAMPLES, 0,
    None
  };

  Display* dsp = x11_display(context);
  int screenId = screen(context);

  _vi = glXChooseVisual(dsp, screenId, att);
  if (_vi == nullptr)
    return;

  _swa.colormap = XCreateColormap(x11_display(context), RootWindow(x11_display(context), screen(context)), _vi->visual, AllocNone);
}

/*
 * \\fn void CameraWindow::on_create_window
 *
 * created on: Nov 21, 2019
 * author: daniel
 *
 */
void CameraWindow::on_create_window(Context context)
{
  _glc = glXCreateContext(x11_display(context), _vi, NULL, GL_TRUE);
  glXMakeCurrent(x11_display(context), handle(), _glc);

  // Set GL Sample stuff
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glGenTextures(1, &_texture);

  const char * fontname = "-*-courier 10 *-medium-r-*-*-*-*-*-*-*-*-*-*";
  _font = XLoadQueryFont (x11_display(context), fontname);

  /* If the font could not be loaded, revert to the "fixed" font. */
  if (_font == nullptr)
  {
     std::cerr << "unable to load font " << fontname << ": using fixed\n" << std::endl;
     _font = XLoadQueryFont (x11_display(context), "fixed");
  }

  _gc = XCreateGC (x11_display(context), handle(), 0, 0);

  Colormap cmap = DefaultColormap(x11_display(context), screen(context));
  // I guess XParseColor will work here
  _text_color.red = 32000; _text_color.green = 65000; _text_color.blue = 32000;
  _text_color.flags = DoRed | DoGreen | DoBlue;
  XAllocColor(x11_display(context), cmap, &_text_color);

}


/*
 * \\fn CameraWindow::gl_rect
 *
 * created on: Nov 26, 2019
 * author: daniel
 *
 */
Rect CameraWindow::gl_rect(const GLWindow& wnd)
{
  Rect result;

  float min = -1.0f;
  result.left = (int)(_width * (wnd._left - min) / 2.0f);
  result.right = (int)(_width * (wnd._right - min) / 2.0f);
  result.top = (int)(_height * (wnd._top - min) / 2.0f);
  result.bottom = (int)(_height * (wnd._bottom - min) / 2.0f);

  return result;
}


/*
 * \\fn void CameraWindow::show_video
 *
 * created on: Nov 21, 2019
 * author: daniel
 *
 */
void CameraWindow::show_video(Context context,LShowImageEvent* evt)
{
  _mutex.lock();
  GLWindow  wnd = _gl_map[evt->_id];
  _mutex.unlock();

  if (!wnd._image)
    return;

  if (_click.load() == static_cast<int>(evt->_id))
  {
    std::string file_name = Utils::string_format("image_%04d_%04d.raw", evt->_id, _global_number++);
    uint32_t w = wnd._image->width(), h = wnd._image->height(), bytes = 2 /* RAW12*/;
    std::ofstream raw_file (file_name, std::ios::out | std::ios::binary);
    raw_file.write(reinterpret_cast<const char*>(&w), sizeof(w));
    raw_file.write(reinterpret_cast<const char*>(&h), sizeof(h));
    raw_file.write(reinterpret_cast<const char*>(&bytes), sizeof(bytes));

    raw_file.write(reinterpret_cast<const char*>(wnd._image->bytes()),w * h * bytes);

    _click.store(-1);
  }
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
  glBindTexture(GL_TEXTURE_2D, _texture);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,  wnd._image->width(), wnd._image->height(), 0, GL_RGB, GL_UNSIGNED_SHORT, wnd._image->bytes());

  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, _texture);
  //glEnable(GL_TEXTURE_2D);
  glBegin(GL_QUADS);
    glTexCoord2f(1.0, 0.0); glVertex3f(wnd._left, wnd._top, 0.0);
    glTexCoord2f(1.0, 1.0); glVertex3f(wnd._left, wnd._bottom, 0.0);
    glTexCoord2f(0.0, 1.0); glVertex3f(wnd._right, wnd._bottom, 0.0);
    glTexCoord2f(0.0, 0.0); glVertex3f(wnd._right, wnd._top, 0.0);
  glEnd();

  glDisable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, 0);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

  image::HistPtr hist = wnd._image->get_histogram();
  if (hist)
  {
    uint32_t max_value = (hist->_max_value >> 5) << 6;

    glShadeModel(GL_SMOOTH);
    glLineWidth(4.0);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBegin(GL_LINE_STRIP);
      glColor4ub(255, 0, 0, 32);
      //glColor3f(1.0, 0.0, 0.0);
      GLfloat gap = (wnd._right -  wnd._left) * 10 / wnd._image->width();
      for (size_t hist_index = 0; hist_index < hist->_histogram.size(); hist_index++)
      {
        GLfloat value = (max_value == 0) ? wnd._top : wnd._top + (wnd._bottom -  wnd._top) * hist->_histogram[hist_index] / max_value;
        GLfloat xx = wnd._left + gap + (wnd._right -  wnd._left - 2.0 * gap) * hist_index / hist->_histogram.size();

        glVertex3f(xx, value, 1.0);
      }

    glEnd();
    glDisable(GL_BLEND);
  }

  glXSwapBuffers(x11_display(context), handle());

  // Replace text
  wnd._text->clear();
  for (size_t index = 0; index < hist->_small_hist.size(); index++)
    wnd._text->push_back(Utils::string_format("Num pixels: %d", hist->_small_hist[index]));

  _mutex.lock();
  _gl_map[evt->_id]._image.reset();

  if (_font != nullptr)
  {
    for (auto glwnd : _gl_map)
    {
      if (glwnd._text->empty())
        continue;

      Rect cur_rect = gl_rect(glwnd);

      XSetFont (x11_display(context), _gc, _font->fid);
      XSetForeground(x11_display(context), _gc, _text_color.pixel);

      // Centre the text in the middle of the box.
      int direction, ascent, descent;
      XCharStruct overall;

      XTextExtents (_font, glwnd._text->at(0).c_str(), glwnd._text->at(0).size(),
                    & direction, & ascent, & descent, & overall);

      for (size_t index = 0; index < glwnd._text->size(); index++)
      {
        int x = cur_rect.left + 20;
        int y = cur_rect.top + 20 + (overall.ascent + 5) * index;

        //XClearWindow (display(context), handle());
        XDrawString (x11_display(context), handle(), _gc, x, y, glwnd._text->at(index).c_str(), glwnd._text->at(index).size());
      }
    }
  }
  XFlush(x11_display(context));
  _mutex.unlock();

}

} /* namespace window */
} /* namespace jupiter */
} /* namespace brt */
