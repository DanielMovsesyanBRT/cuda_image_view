/*
 * cuda_mem.hpp
 *
 *  Created on: Feb 14, 2020
 *      Author: daniel
 */

#ifndef BRT_COMMON_CUDA_MEM_HPP_
#define BRT_COMMON_CUDA_MEM_HPP_

#include <stddef.h>
#include <stdint.h>

#include "private/cuda/cuda_data.hpp"

namespace brt {
namespace jupiter {


/*
 * \\class CudaPtr
 *
 * created on: Nov 25, 2019
 *
 */
template <class T>
class CudaPtr
{
public:
  CudaPtr() : _data(nullptr) {}

  /*
   * \\fn Constructor CudaPtr
   *
   * created on: Nov 25, 2019
   * author: daniel
   *
   */
  CudaPtr(size_t size) : _data(new CudaData(size * sizeof(T)))
  {
    if (!_data->valid())
    {
      delete _data;
      _data = nullptr;
    }
  }

  /*
   * \\fn Constructor CudaPtr
   *
   * created on: Nov 25, 2019
   * author: daniel
   *
   */
  CudaPtr(const CudaPtr& rval) : _data(rval._data)
  {
    if (_data != nullptr)
      _data->addref();
  }

  /*
   * \\fn Destructor ~CudaPtr
   *
   * created on: Nov 25, 2019
   * author: daniel
   *
   */
  ~CudaPtr()
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
  CudaPtr& operator=(const CudaPtr& rval)
  {
    if (_data != nullptr)
      _data->release();

    _data = rval._data;

    if (_data != nullptr)
      _data->addref();

    return *this;
  }

  /*
   * \\fn size_t size
   *
   * created on: Nov 25, 2019
   * author: daniel
   *
   */
  size_t size() const
  {
    if (_data == nullptr)
      return 0;

    return _data->size() / sizeof(T);
  }

  /*
   * \\fn operator bool
   *
   * created on: Nov 25, 2019
   * author: daniel
   *
   */
  operator bool()
  {
    return ((_data != nullptr) && (_data->valid()));
  }

  /*
   * \\fn bool put
   *
   * created on: Nov 25, 2019
   * author: daniel
   *
   */
  bool put(const T* data, size_t size)
  {
    if (_data == nullptr)
      _data = new CudaData(size * sizeof(T));

    else if (_data->size() != (size * sizeof(T)))
    {
      _data->release();
      _data = new CudaData(size * sizeof(T));
    }

    if (!_data->valid())
    {
      delete _data;
      _data = nullptr;
      return false;
    }
    _data->from_host(data, size * sizeof(T));
    return true;
  }

  /*
   * \\fn bool get
   *
   * created on: Nov 25, 2019
   * author: daniel
   *
   */
  size_t get(T* data, size_t size)
  {
    if ((_data == nullptr) || (!_data->valid()))
      return 0;

    size_t size_to_copy = std::min(size * sizeof(T),_data->size());
    _data->to_host(data, size_to_copy);

    return size_to_copy / sizeof(T);
  }


  /*
   * \\fn const T* ptr
   *
   * created on: Nov 25, 2019
   * author: daniel
   *
   */
  const T* ptr() const
  {
    if (_data == nullptr)
      return nullptr;

    return (T*)(_data->ptr());
  }

  /*
   * \\fn T* ptr
   *
   * created on: Nov 25, 2019
   * author: daniel
   *
   */
  T* ptr()
  {
    if (_data == nullptr)
      return nullptr;

    return (T*)(_data->ptr());
  }


  /*
   * \\fn void fill
   *
   * created on: Nov 25, 2019
   * author: daniel
   *
   */
  void fill(int value)
  {
    if ((_data == nullptr) || (!_data->valid()))
      return;

    _data->memset((int)value);
  }


  /*
   * \\fn void copy
   *
   * created on: Nov 25, 2019
   * author: daniel
   *
   */
  bool copy(CudaPtr<T>& dst)
  {
    if (!dst || (dst.size() != size()))
    {
      dst = CudaPtr<T>(size());
      if (!dst)
        return false;
    }
    dst._data->copy(&dst);
    return true;
  }

protected:
  CudaData*                       _data;
};


/*
 * \\class Cuda2DMem
 *
 * created on: Feb 18, 2020
 *
 */
template <class T>
class Cuda2DMem : public CudaPtr<T>
{
public:
  Cuda2DMem():_width(0),_height(0) {}

  Cuda2DMem(uint32_t w,uint32_t h)
  : CudaPtr<T>(w * h)
  , _width(w)
  , _height(h)
  {   }

  Cuda2DMem(const Cuda2DMem& mem)
  : CudaPtr<T>((const CudaPtr<T>&)mem)
  , _width(mem._width)
  , _height(mem._height)
  {}

  Cuda2DMem& operator=(const Cuda2DMem& mem)
  {
    *((CudaPtr<T>*)this) = (const CudaPtr<T>&)mem;
    _width = mem._width;
    _height = mem._height;

    return *this;
  }

  __device__ const T operator()(int x,int y) const
  {
    if ((x < 0) || (y < 0) || (x >= _width) || (y >= _height))
      return (T)0;

    int offset = (x  + y * _width);
    return  *( ((T*)CudaPtr<T>::_data->ptr()) + offset);
  }

  __device__ T& operator()(int x,int y)
  {
    int offset = (x  + y * _width);
    return  *( ((T*)CudaPtr<T>::_data->ptr()) + offset);
  }

private:
  uint32_t                        _width;
  uint32_t                        _height;
};



} // jupiter
} // brt


#endif /* BRT_COMMON_CUDA_MEM_HPP_ */
