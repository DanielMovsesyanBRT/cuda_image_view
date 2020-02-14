/*
 * cuda_data.hpp
 *
 *  Created on: Feb 14, 2020
 *      Author: daniel
 */

#ifndef BRT_COMMON_PRIVATE_CUDA_DATA_HPP_
#define BRT_COMMON_PRIVATE_CUDA_DATA_HPP_

#include <atomic>
#include <stddef.h>

namespace brt {
namespace jupiter {


/*
 * \\class CudaData
 *
 * created on: Feb 14, 2020
 *
 */
class CudaData
{
public:
  CudaData(size_t size);
  virtual ~CudaData();

          void addref()  { _ref_cnt++; }

          void release()
          {
            if (--_ref_cnt == 0)
              delete this;
          }

          void                    memset(int value);
          size_t                  size() const { return (!_valid)? 0 : _size; }
          bool                    valid() const { return _valid; }
          void*                   ptr() { return _data; }

          void                    from_host(const void* data,size_t size);
          void                    to_host(void* data,size_t size);
          void                    copy(CudaData* cd);

private:
  size_t                          _size;
  void*                           _data;
  std::atomic_int_fast32_t        _ref_cnt;
  bool                            _valid;
};

} // jupiter
} // brt


#endif /* BRT_COMMON_PRIVATE_CUDA_DATA_HPP_ */
