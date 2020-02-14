/*
 * image_list_window.cpp
 *
 *  Created on: Jan 24, 2020
 *      Author: daniel
 */

#include "image_list_window.hpp"

#include <fstream>

#include <image_processor.hpp>

namespace brt
{
namespace jupiter
{
namespace window
{

/*
 * \\fn Constructor ImageListWindow::ImageListWindow
 *
 * created on: Jan 24, 2020
 * author: daniel
 *
 */
ImageListWindow::ImageListWindow(const std::vector<std::string>& files, int x, int y)
: Window("", x, y, 0, 0, nullptr)
, _files(files)
, _index(0)
{

}

/*
 * \\fn Destructor ImageListWindow::~ImageListWindow
 *
 * created on: Jan 24, 2020
 * author: daniel
 *
 */
ImageListWindow::~ImageListWindow()
{

}

/*
 * \\fn ImageListWindow* ImageListWindow::create
 *
 * created on: Jan 24, 2020
 * author: daniel
 *
 */
ImageListWindow* ImageListWindow::create(const std::vector<std::string>& files,
                                          int x /*= RANDOM_POS*/,
                                          int y /*= RANDOM_POS*/)
{
  if (files.empty())
    return nullptr;

  ImageListWindow* wnd = new ImageListWindow(files, x, y);
  if (!wnd->next_image())
  {
    delete wnd;
    return nullptr;
  }

  return wnd;
}

/*
 * \\fn bool ImageListWindow::next_image
 *
 * created on: Jan 24, 2020
 * author: daniel
 *
 */
bool ImageListWindow::next_image()
{
  image::RawRGBPtr image;
  while(!image && (_index < _files.size()))
  {
    try
    {
      std::ifstream raw_file (_files[_index], std::ios::binary);
      if (!raw_file.is_open())
        throw 1;

      raw_file.seekg(0, std::ios::end);
      size_t file_size = raw_file.tellg();
      raw_file.seekg(0, std::ios::beg);

      if (file_size < sizeof(uint32_t) * 3)
        throw 1;

      bool rgb = false;

      uint32_t width,height,numbytes;
      raw_file.read(reinterpret_cast<char*>(&width), sizeof(uint32_t));
      raw_file.read(reinterpret_cast<char*>(&height), sizeof(uint32_t));
      raw_file.read(reinterpret_cast<char*>(&numbytes), sizeof(uint32_t));

      if (file_size >= (width * height * numbytes * image::eRGBA))
        rgb = true;
      else if (file_size < (width * height * numbytes))
        throw 1;

      image.reset(new image::RawRGB(width,height,16, rgb ? image::eRGBA : image::eBayer));
      raw_file.read(reinterpret_cast<char*>(image->bytes()),image->size());

      if (image->type() == image::eBayer)
      {
        image::Debayer db;
        image = db.debayer(image);
      }
    }
    catch(...)
    {   }

    _index++;
  }

  if (!image)
    return false;

  _bits = image;
  return true;
}

void                    x_create(Context);

void                    pre_create_window(Context);

void                    on_create_window(Context);


} /* namespace window */
} /* namespace jupiter */
} /* namespace brt */
