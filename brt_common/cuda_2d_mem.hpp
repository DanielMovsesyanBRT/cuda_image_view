/*
 * cuda_3d_mem.hpp
 *
 *  Created on: Feb 14, 2020
 *      Author: daniel
 */

#ifndef BRT_COMMON_PRIVATE_CUDA_2D_MEM_HPP_
#define BRT_COMMON_PRIVATE_CUDA_2D_MEM_HPP_


#include <stddef.h>
#include <stdint.h>


#include "private/cuda/cuda_2d_data.hpp"

namespace brt {
namespace jupiter {

/*
 * \\class Cuda2DPtr
 *
 * created on: Feb 14, 2020
 *
 */
template <class T>
class Cuda2DPtr
{
public:
  Cuda2DPtr() : _data(nullptr) , _xoffset(0), _yoffset(0) {}

  /*
   * \\fn Cuda2DPtr
   *
   * created on: Feb 14, 2020
   * author: daniel
   *
   */
  Cuda2DPtr(size_t width,size_t height,int xoffset = 0,int yoffset = 0)
  : _data(new Cuda2DData((width + (xoffset * 2)) * sizeof(T), height + yoffset * 2))
  , _xoffset(xoffset), _yoffset(yoffset)
  {
    if (!_data->valid())
    {
      delete _data;
      _data = nullptr;
    }
  }

  /*
   * \\fn Constructor Cuda2DPtr
   *
   * created on: Feb 14, 2020
   * author: daniel
   *
   */
  Cuda2DPtr(const Cuda2DPtr& rval) : _data(rval._data)
  {
    if (_data != nullptr)
      _data->addref();

    _xoffset = rval._xoffset;
    _yoffset = rval._yoffset;
  }


  /*
   * \\fn ~Cuda2DPtr
   *
   * created on: Feb 14, 2020
   * author: daniel
   *
   */
  ~Cuda2DPtr()
  {
    if (_data != nullptr)
      _data->release();
  }

  /*
   * \\fn CudaPtr& operator=
   *
   * created on: Nov 25, 2019
   * author: daniel
   *
   */
  Cuda2DPtr& operator=(const Cuda2DPtr& rval)
  {
    if (_data != nullptr)
      _data->release();

    _data = rval._data;
    _xoffset = rval._xoffset;
    _yoffset = rval._yoffset;

    if (_data != nullptr)
      _data->addref();

    return *this;
  }

  /*
   * \\fn void fill(int value)
   *
   * created on: Feb 14, 2020
   * author: daniel
   *
   */
  void fill(int value)
  {
    if (_data != nullptr)
      _data->memset(value);
  }


  /*
   * \\fn size_t width
   *
   * created on: Feb 14, 2020
   * author: daniel
   *
   */
  size_t width() const
  {
    if (_data == nullptr)
      return 0;

    return _data->width() / sizeof(T) - (_xoffset * 2);
  }

  /*
   * \\fn size_t height
   *
   * created on: Feb 14, 2020
   * author: daniel
   *
   */
  size_t height() const
  {
    if (_data == nullptr)
      return 0;

    return _data->height()- (_yoffset * 2);
  }

  /*
   * \\fn T&  at(int x,int y)
   *
   * created on: Feb 14, 2020
   * author: daniel
   *
   */
  __device__ T*  at(int x,int y)
  {
    if (_data == nullptr)
      return nullptr;

    return reinterpret_cast<T*>(_data->at((_xoffset + x) * sizeof(T),_yoffset + y));
  }

  /*
   * \\fn T& operator()(int x,int y)
   *
   * created on: Feb 14, 2020
   * author: daniel
   *
   */
  __device__ T& operator()(int x,int y)
  {
    return *at(x,y);
  }

  /*
   * \\fn void put
   *
   * created on: Feb 14, 2020
   * author: daniel
   *
   */
  void put(T* data)
  {
    if (_data == nullptr)
      return;

    _data->from_host(data, width(), height(), _xoffset, _yoffset);
  }

  /*
   * \\fn void put
   *
   * created on: Feb 14, 2020
   * author: daniel
   *
   */
  void get(T* data)
  {
    if (_data == nullptr)
      return;

    _data->to_host(data, width(), height(), _xoffset, _yoffset);
  }

private:
  Cuda2DData*                       _data;
  int                               _xoffset;
  int                               _yoffset;
};

} // jupiter
} // brt


#endif /* BRT_COMMON_PRIVATE_CUDA_2D_MEM_HPP_ */
