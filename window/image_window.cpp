/*
 * image_window.cpp
 *
 *  Created on: Jan 22, 2020
 *      Author: daniel
 */

#include "image_window.hpp"
#include "window_manager.hpp"

namespace brt
{
namespace jupiter
{
namespace window
{

/*
 * \\fn Constructor ImageWindow::ImageWindow
 *
 * created on: Jan 22, 2020
 * author: daniel
 *
 */
ImageWindow::ImageWindow(const char* title, int x, int y,
    image::RawRGBPtr bits, Window* parent)
: Window(title, x, y, bits->width(), bits->height(), parent)
, _bits(bits)
, _actual_width(bits->width())
, _actual_height(bits->height())
, _vi(nullptr)
, _gc(nullptr)
, _font(nullptr)
, _glc(0)
, _texture(0)
{
}


/*
 * \\fn Destructor ImageWindow::~ImageWindow
 *
 * created on: Jan 22, 2020
 * author: daniel
 *
 */
ImageWindow::~ImageWindow()
{
}


/*
 * \\fn ImageWindow* ImageWindow::create
 *
 * created on: Jan 22, 2020
 * author: daniel
 *
 */
ImageWindow* ImageWindow::create(const char* title,
                                        Window* parent, image::RawRGBPtr bits,
                                        int x /*= RANDOM_POS*/, int y /*= RANDOM_POS*/)
{
  ImageWindow* wnd = new ImageWindow(title, x, y, bits, parent);
  return wnd;
}

/*
 * \\fn bool ImageWindow::x_event
 *
 * created on: Jan 22, 2020
 * author: daniel
 *
 */
bool ImageWindow::x_event(Context ctx,const XEvent &event)
{
  switch (event.type)
  {
  case Expose:
    {
      if (display(ctx)._dt == eLocalDisplay)
      {
        XWindowAttributes attribs;
        XGetWindowAttributes(wm::get()->x11_display(ctx), handle(), &attribs);
        glViewport(0, 0, attribs.width, attribs.height);
        glXSwapBuffers(wm::get()->x11_display(ctx), handle());
      }

      show_img(ctx);
    }
    return true;

//  case ButtonPress:
//    {
//      int col = event.xbutton.x / _actual_width;
//      int row = event.xbutton.y / _actual_height;
//
//      size_t id = (size_t)-1;
//
//      _mutex.lock();
//      for (size_t index = 0; index < _gl_map.size(); index++)
//      {
//        if ((_gl_map[index]._col == static_cast<size_t>(col)) && (_gl_map[index]._row == static_cast<size_t>(row)))
//        {
//          id = index;
//          break;
//        }
//      }
//      _mutex.unlock();
//
//      if (id < 4)
//      {
//        int_fast32_t expected = -1;
//        _click.compare_exchange_strong(expected, id);
//      }
//      std::cout << "Button Press x=" << col << "; y=" << row << "; id=" << id << std::endl;
//    }
//    return true;

  default:break;
  }
  return Window::x_event(ctx,event);
}

/*
 * \\fn void ImageWindow::show_img
 *
 * created on: Jan 22, 2020
 * author: daniel
 *
 */
void ImageWindow::show_img(Context ctx)
{
  if (!_bits)
    return;

  if (display(ctx)._dt == eLocalDisplay)
  {
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
    glBindTexture(GL_TEXTURE_2D, _texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,  _bits->width(), _bits->height(), 0, GL_RGBA, GL_UNSIGNED_SHORT, _bits->bytes());

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, _texture);
    //glEnable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
      glTexCoord2f(0.0, 1.0); glVertex3f(-1.0, -1.0, 0.0);
      glTexCoord2f(0.0, 0.0); glVertex3f(-1.0, 1.0, 0.0);
      glTexCoord2f(1.0, 0.0); glVertex3f(1.0, 1.0, 0.0);
      glTexCoord2f(1.0, 1.0); glVertex3f(1.0, -1.0, 0.0);
    glEnd();

    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    image::HistPtr hist = _bits->get_histogram();
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
        GLfloat gap = 20 / _bits->width();
        for (size_t hist_index = 0; hist_index < hist->_histogram.size(); hist_index++)
        {
          GLfloat value = (max_value == 0) ? -1.0 : -1.0 + 2.0 * hist->_histogram[hist_index] / max_value;
          GLfloat xx = -1.0 + gap + 2.0 * (1.0 - gap) * hist_index / hist->_histogram.size();

          glVertex3f(xx, value, 1.0);
        }

      glEnd();
      glDisable(GL_BLEND);
    }

    glXSwapBuffers(x11_display(ctx), handle());

    std::vector<std::string> text;
    // Replace text
    text.clear();
    for (size_t index = 0; index < hist->_small_hist.size(); index++)
      text.push_back(Utils::string_format("Num pixels: %d", hist->_small_hist[index]));

    //_gl_map[evt->_id]._image.reset();

    if (_font != nullptr)
    {
      XSetFont (x11_display(ctx), _gc, _font->fid);
      XSetForeground(x11_display(ctx), _gc, _text_color.pixel);

      // Centre the text in the middle of the box.
      int direction, ascent, descent;
      XCharStruct overall;

      XTextExtents (_font, text.at(0).c_str(), text.at(0).size(),
                    & direction, & ascent, & descent, & overall);

      for (size_t index = 0; index < text.size(); index++)
      {
        int x = 20;
        int y = 20 + (overall.ascent + 5) * index;

        //XClearWindow (display(context), handle());
        XDrawString (x11_display(ctx), handle(), _gc, x, y, text.at(index).c_str(), text.at(index).size());
      }
    }
    XFlush(x11_display(ctx));


  }
  else
  {
    Display* dsp = x11_display(ctx);
    if (_vi != nullptr)
    {
      XImage *ximage = XCreateImage(dsp, _vi->visual, 24, ZPixmap, 0,reinterpret_cast<char*>(_bits->bytes()), _bits->width(), _bits->height(), 8, 0);
      XPutImage(dsp, handle(), _gc, ximage, 0, 0, 0, 0, _actual_width, _actual_height);
    }
  }
}
/*
 * \\fn bool ImageWindow::l_event
 *
 * created on: Jan 22, 2020
 * author: daniel
 *
 */
//bool ImageWindow::l_event(Context,const LEvent *)
//{
//
//}

/*
 * \\fn void CameraWindow::x_create
 *
 * created on: Nov 21, 2019
 * author: daniel
 *
 */
void ImageWindow::x_create(Context context)
{
  WinSize screen_size = wm::get()->resolution(context, wm::get()->default_screen(context));

  int full_width = _actual_width;
  int full_height = _actual_height;

#define WIDTH_GAP             (150)
#define HEIGHT_GAP            (50)

  if (full_width > (screen_size._width - WIDTH_GAP))
  {
    float ratio = (float)full_width / (screen_size._width - WIDTH_GAP);
    _actual_width = (size_t)((float)_actual_width / ratio);
    _actual_height = (size_t)((float)_actual_height / ratio);

    full_width = _actual_width;
    full_height = _actual_height;
  }

  if (full_height > (screen_size._height - HEIGHT_GAP))
  {
    float ratio = (float)full_height / (screen_size._height - HEIGHT_GAP);
    _actual_width = (size_t)((float)_actual_width / ratio);
    _actual_height = (size_t)((float)_actual_height / ratio);

    full_width = _actual_width;
    full_height = _actual_height;
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
void ImageWindow::pre_create_window(Context context)
{
  Display* dsp = x11_display(context);
  int screenId = screen(context);

  if (display(context)._dt == eLocalDisplay)
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

    _vi = glXChooseVisual(dsp, screenId, att);
    if (_vi == nullptr)
      return;
  }
  else
  {
    if (_bits->depth() > 8)
      _bits = _bits->clone(8);

    XVisualInfo visual_template;
    int nxvisuals = 0;
    visual_template.screen = screenId;
    _vi = XGetVisualInfo (dsp, VisualScreenMask, &visual_template, &nxvisuals);
  }
  _swa.colormap = XCreateColormap(x11_display(context), RootWindow(x11_display(context), screen(context)), _vi->visual, AllocNone);
}

/*
 * \\fn void CameraWindow::on_create_window
 *
 * created on: Nov 21, 2019
 * author: daniel
 *
 */
void ImageWindow::on_create_window(Context context)
{
  if (display(context)._dt == eLocalDisplay)
  {
    _glc = glXCreateContext(x11_display(context), _vi, NULL, GL_TRUE);
    glXMakeCurrent(x11_display(context), handle(), _glc);

    // Set GL Sample stuff
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glGenTextures(1, &_texture);
  }

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


//  Display* dsp = x11_display(context);
//  _gc = XCreateGC (dsp, handle(), 0, 0);
}


} /* namespace wnidow */
} /* namespace jupiter */
} /* namespace brt */
