/*
 * cuda_data.cu
 *
 *  Created on: Feb 14, 2020
 *      Author: daniel
 */


#include "cuda_data.hpp"
#include <iostream>

#define CUDA_CHECK(x)   cudaError_t ___err = (x); \
                        if (___err != cudaSuccess) \
                          std::cerr << " Function " #x " error:" << ___err << std::endl;



namespace brt {
namespace jupiter {

/*
 * \\fn CudaData::CudaData
 *
 * created on: Feb 14, 2020
 * author: daniel
 *
 */
CudaData::CudaData(size_t size)
: _size(size)
, _data(nullptr)
, _ref_cnt(1)
, _valid(false)
{
  if (cudaMalloc(&_data,size) == cudaSuccess)
    _valid = true;
}

/*
 * \\fn CudaData::~CudaData
 *
 * created on: Feb 14, 2020
 * author: daniel
 *
 */
CudaData::~CudaData()
{
  if (_valid)
    cudaFree(_data);
}

/*
 * \\fn void CudaData::memset
 *
 * created on: Feb 14, 2020
 * author: daniel
 *
 */
void CudaData::memset(int value)
{
  if (!_valid)
    return;

  CUDA_CHECK(cudaMemset(_data, value, _size));
}

/*
 * \\fn void CudaData::from_host
 *
 * created on: Feb 14, 2020
 * author: daniel
 *
 */
void CudaData::from_host(const void* data,size_t size)
{
  if (!_valid)
    return;

  CUDA_CHECK(cudaMemcpy(_data, data, size , cudaMemcpyHostToDevice));
}

/*
 * \\fn void CudaData::to_host
 *
 * created on: Feb 14, 2020
 * author: daniel
 *
 */
void CudaData::to_host(void* data,size_t size)
{
  if (!_valid)
    return;

  size_t size_to_copy = std::min(size ,_size);
  CUDA_CHECK(cudaMemcpy(data, _data, size_to_copy , cudaMemcpyDeviceToHost));
}

/*
 * \\fn void CudaData::copy
 *
 * created on: Feb 14, 2020
 * author: daniel
 *
 */
void CudaData::copy(CudaData* cd)
{
  if (!_valid)
    return;

  size_t size_to_copy = std::min(cd->_size ,_size);
  CUDA_CHECK(cudaMemcpy(cd->_data, _data, size_to_copy, cudaMemcpyDeviceToDevice));
}

}
}
