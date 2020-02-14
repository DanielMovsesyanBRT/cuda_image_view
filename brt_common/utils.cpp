//
// Created by Daniel Movsesyan on 2019-04-19.
//

#include <cstdio>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <iostream>

#include "utils.hpp"

#undef _lengthof
#define _lengthof(x)        (sizeof(x)/sizeof(x[0]))

#define _PATH_PROCNET_X11                   "/tmp/.X11-unix"
#define _PATH_PROCNET_TCP                   "/proc/net/tcp"
#define _PATH_PROCNET_TCP6                  "/proc/net/tcp6"
#define X11_PORT_MIN                        (6000)
#define X11_PORT_MAX                        (6100)


namespace brt {

namespace jupiter {


/**
 *
 * @param src
 * @param dst
 * @param len
 * @return
 */
const char* Utils::stristr(const char* src,const char* dst,size_t len /*= (size_t)-1*/)
{
  const char* _dst = dst;
  while (*src && *_dst && ( (len != (size_t)-1) ? len-- : true))
  {
    if (tolower(*_dst) == tolower(*src))
    {
      if (*(++_dst) == '\0')
        return src - (_dst - dst - 1);
    }
    else
      _dst = dst; // reset Destination pointer

    src++;
  }

  return nullptr;
}


/**
 *
 * @param src
 * @param dst
 * @param len
 * @return
 */
size_t Utils::stristr(const std::string& src,const char* dst,size_t len /*= (size_t)-1*/)
{
  const char *result = stristr(src.c_str(),dst,len);
  if (result == nullptr)
    return std::string::npos;

  return 0;
}

/*
 * \\fn double Utils::frame_rate
 *
 * created on: Sep 17, 2019
 * author: daniel
 *
 */
double Utils::frame_rate(const char* fr_string)
{
  char *end;
  double frame_rate = strtod(fr_string, &end), multiplier = 1.0;

  if (*end != '\0')
  {
    if (stristr(end, "fps") != nullptr)
      multiplier = 1.0;
    else if (stristr(end, "fpms") != nullptr)
      multiplier = 1e3;
    else if (stristr(end, "fpm") != nullptr)
      multiplier = 1.0 / 60.0;
    else if (stristr(end, "fpu") != nullptr)
      multiplier = 1e6;
  }

  return frame_rate = 1.0 / (frame_rate * multiplier);
}

/*
 * \\fn std::vector<std::string> FLTKManager::enumerate_displays
 *
 * created on: Nov 19, 2019
 * author: daniel
 *
 */
std::set<X11Display> Utils::enumerate_displays(DisplayType dt)
{
  std::set<X11Display> result;

  // Check local displays
  if ((dt & eLocalDisplay) != 0)
  {
    DIR* d = opendir(_PATH_PROCNET_X11);

    if (d != nullptr)
    {
      struct dirent *dr;
      while ((dr = readdir(d)) != nullptr)
      {
        if (dr->d_name[0] != 'X')
          continue;

        result.insert(X11Display(eLocalDisplay,Utils::string_format(":%s", dr->d_name + 1)));
      }
      closedir(d);
    }
  }

  // Check remotes
  if ((dt & eRemoteDisplay) != 0)
  {
    FILE *fd = fopen(_PATH_PROCNET_TCP, "r");

    if (fd != nullptr)
    {
      size_t pagesz = getpagesize();
      char *buf = (char *)malloc(pagesz);
      setvbuf(fd, buf, _IOFBF, pagesz);

      char buffer[8192];
      int d, timeout, uid;
      unsigned int local_port, rem_port, state, timer_run;
      char rem_addr[128], local_addr[128];
      unsigned long rxq, txq, time_len, retr, inode;

      do
      {
        if (fgets(buffer, sizeof(buffer), fd))
        {
          sscanf(buffer,
                          "%d: %64[0-9A-Fa-f]:%X %64[0-9A-Fa-f]:%X %X %lX:%lX %X:%lX %lX %d %d %lu %*s\n",
                          &d, local_addr, &local_port, rem_addr, &rem_port, &state,
                          &txq, &rxq, &timer_run, &time_len, &retr, &uid, &timeout, &inode);

          if ((local_port >= X11_PORT_MIN) && (local_port < X11_PORT_MAX))
            result.insert(X11Display(eRemoteDisplay,Utils::string_format("localhost:%d.0", local_port - X11_PORT_MIN)));

        }
      }
      while (!feof(fd));

      fclose(fd);
      ::free(buf);
    }
  }

  return result;
}

/*
 * \\fn std::string Utils::aquire_display
 *
 * created on: Dec 5, 2019
 * author: daniel
 *
 */
X11Display Utils::aquire_display(const char* extra_string)
{
  X11Display result;
  char* display_name = getenv("DISPLAY");

  std::set<X11Display> displays = enumerate_displays();
  if (displays.size() == 0)
  {
    if ((display_name == nullptr) || (strlen(display_name) == 0))
      result = X11Display(eLocalDisplay,display_name);
  }

  else if (displays.size() == 1)
  {
    result = *displays.begin();
  }
  else
  {
    std::string line;
    size_t id;

    do
    {
      std::cout << "Please select default display for " << extra_string << std::endl;

      size_t index = 1;
      for (auto display : displays)
      {
        std::cout << index++ << ") " << display._name;
        if ((display_name != nullptr) && (display._name.compare(display_name) == 0))
          std::cout << "[default]";
        std::cout << std::endl;
      }

      std::cin >> id;
      if ((id < 1) || (id > displays.size()))
      {
        if ((display_name == nullptr) || (strlen(display_name) == 0))
        {
          result = X11Display(eLocalDisplay,display_name);
          break;
        }

        std::cout << "Invalid entry" << std::endl;
      }
    }
    while((id < 1) || (id > displays.size()));

    if ((id >= 1) && (id <= displays.size()))
      result = [displays,&id]()->X11Display
      {
        auto iter = displays.begin();
        while (--id != 0)
          iter++;

        return *iter;
      }();
  }
  return result;
}

} // jupiter
} // brt
