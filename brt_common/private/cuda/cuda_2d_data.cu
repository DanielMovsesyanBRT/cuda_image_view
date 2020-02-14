/*
 * cuda_3d_data.cu
 *
 *  Created on: Feb 14, 2020
 *      Author: daniel
 */

#include <iostream>
#include "cuda_2d_data.hpp"

namespace brt {
namespace jupiter {


/*
 * \\fn constructor Cuda2DData::Cuda2DData
 *
 * created on: Feb 14, 2020
 * author: daniel
 *
 */
Cuda2DData::Cuda2DData(size_t xsize,size_t ysize)
: _valid(false)
, _ref_cnt(1)
{
  if (cudaMalloc3D(&_pitchedDevPtr,make_cudaExtent(xsize,ysize, 1)) == cudaSuccess)
    _valid = true;
}

/*
 * \\fn Deestructor Cuda2DData::~Cuda2DData
 *
 * created on: Feb 14, 2020
 * author: daniel
 *
 */
Cuda2DData::~Cuda2DData()
{
  if (_valid)
    cudaFree(_pitchedDevPtr.ptr);
}


/*
 * \\fn void Cuda2DData::memset
 *
 * created on: Feb 14, 2020
 * author: daniel
 *
 */
void Cuda2DData::memset(int value)
{
  if (!_valid)
    return;

  cudaMemset3D(_pitchedDevPtr, value, make_cudaExtent(_pitchedDevPtr.xsize,_pitchedDevPtr.ysize, 1));
}

/*
 * \\fn void Cuda2DData::from_host
 *
 * created on: Feb 14, 2020
 * author: daniel
 *
 */
void Cuda2DData::from_host(void* data,int width,int height,int xoffset, int yoffset)
{
  cudaMemcpy3DParms mcp = {0};

  mcp.srcPtr.ptr  = data;
  mcp.srcPtr.pitch = width;
  mcp.srcPtr.xsize = width;
  mcp.srcPtr.ysize = height;

  mcp.dstPtr.ptr = _pitchedDevPtr.ptr;
  mcp.dstPtr.pitch = _pitchedDevPtr.pitch;
  mcp.dstPtr.xsize = _pitchedDevPtr.xsize;
  mcp.dstPtr.ysize = _pitchedDevPtr.ysize;

  mcp.dstPos.x     = xoffset;
  mcp.dstPos.y     = yoffset;
  mcp.dstPos.z     = 0;

  mcp.extent.width  = width;
  mcp.extent.height = height;
  mcp.extent.depth  = 1;

  mcp.kind = cudaMemcpyHostToDevice;

  cudaError_t err = cudaMemcpy3D(&mcp);
  if (err != cudaSuccess)
    std::cerr << "Unable to copy memory err: " << err << std::endl;
}

/*
 * \\fn void Cuda2DData::to_host
 *
 * created on: Feb 14, 2020
 * author: daniel
 *
 */
void Cuda2DData::to_host(void* data,int width,int height,int xoffset, int yoffset)
{
  cudaMemcpy3DParms mcp = {0};

  mcp.dstPtr.ptr  = data;
  mcp.dstPtr.pitch = width;
  mcp.dstPtr.xsize = width;
  mcp.dstPtr.ysize = height;

  mcp.srcPtr.ptr = _pitchedDevPtr.ptr;
  mcp.srcPtr.pitch = _pitchedDevPtr.pitch;
  mcp.srcPtr.xsize = _pitchedDevPtr.xsize;
  mcp.srcPtr.ysize = _pitchedDevPtr.ysize;

  mcp.extent.width  = width;
  mcp.extent.height = height;
  mcp.extent.depth  = 1;

  mcp.kind = cudaMemcpyDeviceToHost;

  cudaError_t err = cudaMemcpy3D(&mcp);
  if (err != cudaSuccess)
    std::cerr << "Unable to copy memory err: " << err << std::endl;
}

/*
 * \\fn size_t Cuda2DData::width
 *
 * created on: Feb 14, 2020
 * author: daniel
 *
 */
size_t Cuda2DData::width() const
{
  if (!_valid)
    return 0;

  return _pitchedDevPtr.xsize;
}

/*
 * \\fn size_t Cuda2DData::height
 *
 * created on: Feb 14, 2020
 * author: daniel
 *
 */
size_t Cuda2DData::height() const
{
  if (!_valid)
    return 0;

  return _pitchedDevPtr.ysize;
}

///*
// * \\fn void* Cuda2DData::at
// *
// * created on: Feb 14, 2020
// * author: daniel
// *
// */
//__device__ void* Cuda2DData::at(int x,int y)
//{
//  if (!_valid)
//    return nullptr;
//
//  int offset = x + y * _pitchedDevPtr.pitch;
//  return ((uint8_t*)_pitchedDevPtr.ptr) + offset;
//}

}
}

