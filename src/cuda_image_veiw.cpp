//============================================================================
// Name        : cuda_image_veiw.cpp
// Author      : 
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <iostream>
#include <sstream>

#include <glob.h>

#include "utils.hpp"
#include "metadata.hpp"
#include "image_window.hpp"
#include "window_manager.hpp"

#include "debayer.hpp"
#include "debayer_bilin.hpp"


using namespace brt::jupiter;

/*
 * \\fn std::vector<std::string> glob
 *
 * created on: Feb 12, 2020, 4:12:48 PM
 * author daniel
 *
 */
std::vector<std::string> glob(const std::string& pattern)
{
  // glob struct resides on the stack
  glob_t glob_result;
  memset(&glob_result, 0, sizeof(glob_result));

  // do the glob operation
  int return_value = glob(pattern.c_str(), GLOB_TILDE, NULL, &glob_result);
  if (return_value != 0)
  {
    globfree(&glob_result);
    std::stringstream ss;
    ss << "glob() failed with return_value " << return_value << std::endl;
    throw std::runtime_error(ss.str());
  }

  // collect all the filenames into a std::list<std::string>
  std::vector<std::string> filenames;
  for (size_t i = 0; i < glob_result.gl_pathc; ++i)
  {
    filenames.push_back(std::string(glob_result.gl_pathv[i]));
  }

  // cleanup
  globfree(&glob_result);

  // done
  return filenames;
}



// static std::string fn;
/*
 * \\fn void show_window
 *
 * created on: Jan 22, 2020
 * author: daniel
 *
 */
void show_window(const std::string& filename,
                  std::string output_dir,
                  const std::string& ext = "png",
                  bool show = true,
                  const std::string& prefix = "")
{
  image::RawRGBPtr image;
  image::RawRGBPtr raw_image(new image::RawRGB(filename.c_str()));
  if (raw_image->type() == image::eBayer)
  {
    Debayer db;
    db.init(raw_image->width(),raw_image->height(),9);
    image = db.ahd(raw_image);

//    Debayer_Bilinear db;
//    image = db.ahd(raw_image);

//    image::HistPtr hist;
//    db.get_histogram(hist);
//    image->set_histogram(hist);
  }
  else
    image = raw_image;

  if (!image)
    return;


  window::ImageWindow* wnd = window::ImageWindow::create(filename.c_str(), nullptr, image);
  if (wnd != nullptr)
  {
    wnd->show();

    do
    {
       std::cout << '\n' << "Press a key to continue...";
    } while (std::cin.get() != '\n');

    wnd->close();
  }

}

/*
 * \\fn int main
 *
 * created on: Feb 11, 2020
 * author: daniel
 *
 */
int main(int argc,char** argv)
{
  wm::get()->init();

  Metadata meta_args;
  meta_args.parse(argc,argv);

  size_t num_images = meta_args.size("<default>");

  std::cin.get();

  for (size_t index = 0; index < num_images; index++)
  {
    std::string path = meta_args.get_at<std::string>("<default>", index);
    std::vector<std::string> files = glob(path);

    for (auto filename : files)
    {
      show_window(filename,
              meta_args.get<std::string>("out_dir",""),
              meta_args.get<std::string>("ext","png"),
              meta_args.get<bool>("show",false),
              meta_args.get<std::string>("prefix",""));
      //break;
    }
  }

  wm::get()->release();
  return 0;
}
