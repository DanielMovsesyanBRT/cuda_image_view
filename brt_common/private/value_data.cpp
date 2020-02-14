/*
 * ValueData.cpp
 *
 *  Created on: Jul 30, 2019
 *      Author: daniel
 */

#include <string.h>

#include <typeinfo>
#include <locale>
#include "value_data.hpp"
#include "utils.hpp"

namespace brt
{
namespace jupiter
{

#undef _lengthof
#define _lengthof(x)      (sizeof(x)/sizeof(x[0]))

const  struct FloatRatio
{
  int                             _size;
  int                             _exponent;
  int                             _fraction;

}  _float_formats[] =
{
    {0,   0,   0},
    {1,   2,   5},
    {2,   4,  11},
    {3,   6,  17},
    {4,   8,  23}, // Standar Float
    {5,   9,  30},
    {6,   9,  38},
    {7,  10,  45}
};

/*
 * \\fn void RealUtil::from_double
 *
 * created on: Aug 6, 2019
 * author: daniel
 *
 */
void RealUtil::from_double(double value,uint8_t* buffer,size_t size)
{
  memset(buffer,0, size);
  switch (size)
  {
  case (sizeof(double)):
    memcpy(buffer, &value, size);
    break;

  case (sizeof(float)):
    {
      float f_value = static_cast<float>(value);
      memcpy(buffer, &f_value, size);
    }
    break;

  default:
    if (size < _lengthof(_float_formats))
    {
      uint64_t bitmap;

      memcpy(&bitmap,&value, sizeof(double));
      int sign = (bitmap >> 62);
      bitmap >>= (52 - _float_formats[size]._fraction);
      bitmap &= ((1ull << ((size * 8) - 2)) - 1);
      bitmap |= (sign << ((size * 8)- 2));

      memcpy(buffer, &bitmap, size);
    }
    else
      memcpy(buffer, &value, sizeof(double));
    break;
  }
}

/*
 * \\fn double RealUtil::to_double
 *
 * created on: Aug 6, 2019
 * author: daniel
 *
 */
double RealUtil::to_double(const uint8_t* buffer,size_t size)
{
  double value = 0.0;

  switch (size)
  {
  case (sizeof(double)):
    memcpy(&value, buffer, size);
    break;

  case (sizeof(float)):
    {
      float f_value;
      memcpy(&f_value, buffer, size);
      value = f_value;
    }
    break;

  default:
    if (size < _lengthof(_float_formats))
    {
      uint64_t bitmap = 0ull;

      memcpy(&bitmap, buffer, size);
      uint64_t sign = bitmap >> ((size * 8) - 2);
      bitmap &= ((1ull << ((size * 8) - 2)) - 1);
      bitmap <<= (52 - _float_formats[size]._fraction);
      bitmap |= (sign << 62);

      memcpy(&value, &bitmap, sizeof(double));
    }
    else
      memcpy(&value, buffer, sizeof(double));
    break;
  }

  return value;
}


template<> ValueData& ValueData::set<bool>(bool value,size_t size) { return set_bool(value,size); }
template<> ValueData& ValueData::set<int>(int value,size_t size) { return set_int(value,size); }
template<> ValueData& ValueData::set<double>(double value,size_t size) { return set_float(value,size); }
template<> ValueData& ValueData::set<float>(float value,size_t size) { return set_float(value,size); }
template<> ValueData& ValueData::set<uint64_t>(uint64_t value,size_t size) { return set_ull(value,size); }
template<> ValueData& ValueData::set<void*>(void* value,size_t size) { return set_ptr(value,size); }
template<> ValueData& ValueData::set<const char*>(const char* value,size_t size) { return set_string(value,size); }
template<> ValueData& ValueData::set<char*>(char* value,size_t size) { return set_string(value,size); }
template<> ValueData& ValueData::set<std::string>(std::string value,size_t size) { return set_string(value.c_str(),size); }

template<> bool ValueData::get<bool>() const { return get_bool(); }
template<> int ValueData::get<int>() const { return get_int(); }
template<> double ValueData::get<double>() const { return get_float(); }
template<> float ValueData::get<float>() const { return get_float(); }
template<> uint64_t ValueData::get<uint64_t>() const { return get_ull(); }
template<> void* ValueData::get<void*>() const { return get_ptr(); }
template<> std::string ValueData::get<std::string>() const { return get_string(); }
template<> const char* ValueData::get<const char*>() const { return get_string().c_str(); }
template<> ValueData::byte_buffer ValueData::get<ValueData::byte_buffer>() const { return get_byte_array(); }


/*
 * \\fn Constructor ValueData::ValueData
 *
 * created on: Jul 30, 2019
 * author: daniel
 *
 */
ValueData::ValueData(const ValueData& data)
: _buffer(nullptr)
, _size(data._size)
, _type(data._type)
, _array()
, _ref_cntr(1)
, _little_endian(data._little_endian)
{
  if (data._buffer != nullptr)
  {
    _buffer = new uint8_t[_size];
    memcpy(_buffer,data._buffer,_size);
  }

  _array.assign(data._array.begin(),data._array.end());
}


/*
 * \\fn ValueData& ValueData::operator=
 *
 * created on: Jul 30, 2019
 * author: daniel
 *
 */
ValueData& ValueData::operator=(const ValueData& data)
{
  if (_buffer != nullptr)
    delete[] _buffer;
  _buffer = nullptr;

  _size = data._size;
  _type = data._type;
  _little_endian = data._little_endian;

  if (data._buffer != nullptr)
  {
    _buffer = new uint8_t[_size];
    memcpy(_buffer,data._buffer,_size);
  }

  return *this;
}

/*
 * \\fn ValueData& ValueData::set_bool
 *
 * created on: Jul 30, 2019
 * author: daniel
 *
 */
ValueData& ValueData::set_bool(bool value,size_t size /*= sizeof(bool)*/)
{
  if (_buffer != nullptr)
    delete[] _buffer;

  _type = BOOL;
  _size = size;
  _buffer = new uint8_t[_size];
  memcpy(_buffer,&value,std::min(sizeof(value),_size));
  return *this;
}

/*
 * \\fn ValueData& ValueData::set_int
 *
 * created on: Jul 30, 2019
 * author: daniel
 *
 */
ValueData& ValueData::set_int(int value,size_t size /*= sizeof(int)*/)
{
  if (_buffer != nullptr)
    delete[] _buffer;

  _type = INT;
  _size = size;
  _buffer = new uint8_t[_size];
  memcpy(_buffer,&value,std::min(sizeof(value),_size));
  return *this;
}

/*
 * \\fn ValueData& ValueData::set_float
 *
 * created on: Jul 30, 2019
 * author: daniel
 *
 */
ValueData& ValueData::set_float(double value,size_t size /*= sizeof(double)*/)
{
  if (_buffer != nullptr)
    delete[] _buffer;

  _type = FLOAT;
  _size = size;
  _buffer = new uint8_t[_size];
  RealUtil::from_double(value, _buffer, size);
  //memcpy(_buffer,&value,std::min(sizeof(value),_size));
  return *this;
}

/*
 * \\fn ValueData& ValueData::set_ull
 *
 * created on: Jul 30, 2019
 * author: daniel
 *
 */
ValueData& ValueData::set_ull(uint64_t value,size_t size /*= sizeof(uint64_t)*/)
{
  if (_buffer != nullptr)
    delete[] _buffer;

  _type = ULONGLONG;
  _size = size;
  _buffer = new uint8_t[_size];
  memcpy(_buffer,&value,std::min(sizeof(value),_size));
  return *this;
}

/*
 * \\fn ValueData& ValueData::set_ptr
 *
 * created on: Jul 30, 2019
 * author: daniel
 *
 */
ValueData& ValueData::set_ptr(void* value,size_t size /*= sizeof(uint64_t)*/)
{
  if (_buffer != nullptr)
    delete[] _buffer;

  _type = PTR;
  _size = size;
  _buffer = new uint8_t[_size];
  memcpy(_buffer,&value,std::min(sizeof(value),_size));
  return *this;
}

/*
 * \\fn ValueData& ValueData::set_string
 *
 * created on: Jul 30, 2019
 * author: daniel
 *
 */
ValueData& ValueData::set_string(const char* value,size_t size /*= (size_t)-1*/)
{
  if (_buffer != nullptr)
    delete[] _buffer;

  _buffer = nullptr;
  _type = NIL;

  if (size == (size_t)-1)
    _size = strlen(value);
  else
    _size = size;

  if (_size > 0)
  {
    _buffer = new uint8_t[_size];
    memcpy(_buffer,value,_size);
    _type = STRING;
  }
  return *this;
}

/*
 * \\fn ValueData& ValueData::set_byte_array
 *
 * created on: Jul 30, 2019
 * author: daniel
 *
 */
ValueData& ValueData::set_byte_array(const uint8_t* value,size_t size,bool little_endian /*= true*/)
{
  if (_buffer != nullptr)
    delete[] _buffer;

  _buffer = nullptr;
  _type = NIL;
  _little_endian = little_endian;

  _size = size;
  if (_size > 0)
  {
    _buffer = new uint8_t[_size];
    memcpy(_buffer,value,_size);
    _type = BYTEARRAY;
  }
  return *this;
}

/*
 * \\fn ValueData& ValueData::set_byte_array
 *
 * created on: Jul 30, 2019
 * author: daniel
 *
 */
ValueData& ValueData::set_byte_array(const ValueData::byte_buffer& value,bool little_endian /*= true*/)
{
  if (_buffer != nullptr)
    delete[] _buffer;

  _buffer = nullptr;
  _type = NIL;
  _little_endian = little_endian;

  _size = value.size();
  if (_size > 0)
  {
    _buffer = new uint8_t[_size];
    memcpy(_buffer,value.data(),_size);
    _type = BYTEARRAY;
  }
  return *this;
}

/*
 * \\fn bool ValueData::get_bool
 *
 * created on: Jul 30, 2019
 * author: daniel
 *
 */
bool ValueData::get_bool() const
{
  bool result = false;
  switch (_type)
  {
  case BOOL:
    memcpy(&result,_buffer,std::min(sizeof(result),_size));
    break;

  case INT:
    result = (get_int() != 0);
    break;

  case FLOAT:
    result = (get_float() != 0.0);
    break;

  case ULONGLONG:
    result = (get_ull() != 0ull);
    break;

  case PTR:
    result = (get_ptr() != nullptr);
    break;

  case STRING:
    {
      std::string str = get_string();
      auto& f = std::use_facet<std::ctype<char>>(std::locale());
      f.toupper(&str[0], &str[0] + str.size());
      result = (str == "TRUE");
    }
    break;

  case BYTEARRAY:
    result = !get_byte_array().empty();
    break;

  default:
    break;
  }
  return result;
}

/*
 * \\fn int ValueData::get_int()
 *
 * created on: Jul 30, 2019
 * author: daniel
 *
 */
int ValueData::get_int() const
{
  int result = 0;
  switch (_type)
  {
  case BOOL:
    result = get_bool() ? 1 : 0;
    break;

  case INT:
    memcpy(&result,_buffer,std::min(sizeof(result),_size));
    break;

  case FLOAT:
    result = static_cast<int>(get_float());
    break;

  case ULONGLONG:
    result = static_cast<int>(get_ull());
    break;

  case PTR:
    result = reinterpret_cast<std::uintptr_t>(get_ptr());
    break;

  case STRING:
    result = static_cast<int>(strtol(get_string().c_str(),nullptr,0));
    break;

  case BYTEARRAY:
    if ((_buffer != nullptr) && (_size > 0))
    {
      if (_little_endian)
        memcpy(&result,_buffer,std::min(sizeof(int),_size));
      else
      {
        for (size_t index = 0; index < _size; index++)
        {
          result <<= 8;
          result |= _buffer[index];
        }
      }
    }
    break;

  default:
    break;
  }
  return result;
}

/*
 * \\fn double ValueData::get_float
 *
 * created on: Jul 30, 2019
 * author: daniel
 *
 */
double ValueData::get_float() const
{
  double result = 0.0;
  switch (_type)
  {
  case BOOL:
    result = get_bool() ? 1.0 : 0.0;
    break;

  case INT:
    result = get_int();
    break;

  case FLOAT:
    if (_little_endian)
      result = RealUtil::to_double(_buffer,size());
    else
    {
      uint8_t copy_buffer[sizeof(double)] = {0};
      size_t sz = std::min(sizeof(double),_size);
      for (size_t index = 0; index < sz; index++)
        copy_buffer[sz - index - 1] = _buffer[index];

      result = RealUtil::to_double(copy_buffer,size());
    }
    break;

  case ULONGLONG:
    result = static_cast<double>(get_ull());
    break;

  case PTR:
    result = reinterpret_cast<std::uintptr_t>(get_ptr());
    break;

  case STRING:
    result = strtod(get_string().c_str(),nullptr);
    break;

  case BYTEARRAY:
    if ((_buffer != nullptr) && (_size > 0))
    {
      uint8_t copy_buffer[sizeof(double)] = {0};
      if (_little_endian)
        memcpy(copy_buffer,_buffer,std::min(sizeof(double),_size));
      else
      {
        size_t sz = std::min(sizeof(double),_size);
        for (size_t index = 0; index < sz; index++)
          copy_buffer[sz - index - 1] = _buffer[index];
      }
      result = *reinterpret_cast<double*>(copy_buffer);
    }
    break;

  default:
    break;
  }
  return result;
}

/*
 * \\fn uint64_t uint64_t ValueData::get_ull
 *
 * created on: Jul 30, 2019
 * author: daniel
 *
 */
uint64_t ValueData::get_ull() const
{
  uint64_t result = 0ull;
  switch (_type)
  {
  case BOOL:
    result = get_bool() ? 1ull : 0ull;
    break;

  case INT:
    result = get_int();
    break;

  case FLOAT:
    result = static_cast<uint64_t>(get_float());
    break;

  case ULONGLONG:
    memcpy(&result,_buffer,std::min(sizeof(result),_size));
    break;

  case PTR:
    result = reinterpret_cast<std::uintptr_t>(get_ptr());
    break;

  case STRING:
    result = static_cast<uint64_t>(strtoul(get_string().c_str(),nullptr,0));
    break;

  case BYTEARRAY:
    if ((_buffer != nullptr) && (_size > 0))
    {
      uint8_t copy_buffer[sizeof(uint64_t)] = {0};
      if (_little_endian)
        memcpy(copy_buffer,_buffer,std::min(sizeof(double),_size));
      else
      {
        size_t sz = std::min(sizeof(double),_size);
        for (size_t index = 0; index < sz; index++)
          copy_buffer[sz - index - 1] = _buffer[index];
      }
      result = *reinterpret_cast<uint64_t*>(copy_buffer);
    }
    break;

  default:
    break;
  }
  return result;
}

/*
 * \\fn void* ValueData::get_ptr
 *
 * created on: Jul 30, 2019
 * author: daniel
 *
 */
void* ValueData::get_ptr() const
{
  void* result = nullptr;
  switch (_type)
  {
  case INT:
    result = reinterpret_cast<void*>(static_cast<std::uintptr_t>(get_int()));
    break;

  case FLOAT:
    result = reinterpret_cast<void*>(static_cast<std::uintptr_t>(get_float()));
    break;

  case ULONGLONG:
    result = reinterpret_cast<void*>(static_cast<std::uintptr_t>(get_ull()));
    break;

  case PTR:
    memcpy(&result,_buffer,std::min(sizeof(result),_size));
    break;

  case STRING:
    result = reinterpret_cast<void*>(static_cast<std::uintptr_t>(strtoul(get_string().c_str(),nullptr,0)));
    break;

  case BYTEARRAY:
    if ((_buffer != nullptr) && (_size > 0))
    {
      uint8_t copy_buffer[sizeof(void*)] = {0};
      if (_little_endian)
        memcpy(copy_buffer,_buffer,std::min(sizeof(double),_size));
      else
      {
        size_t sz = std::min(sizeof(double),_size);
        for (size_t index = 0; index < sz; index++)
          copy_buffer[sz - index - 1] = _buffer[index];
      }
      result = *reinterpret_cast<void**>(copy_buffer);
    }
    break;

  default:
    break;
  }
  return result;
}

/*
 * \\fn std::string ValueData::get_string
 *
 * created on: Jul 30, 2019
 * author: daniel
 *
 */
std::string ValueData::get_string() const
{
  std::string result;
  switch (_type)
  {
  case BOOL:
    result = get_bool() ? "TRUE" : "FALSE";
    break;

  case INT:
    result = Utils::string_format("%d", get_int());
    break;

  case FLOAT:
    result = Utils::string_format("%g", get_float());
    break;

  case ULONGLONG:
    result = Utils::string_format("%llu", get_ull());
    break;

  case PTR:
    result = Utils::string_format("%p", get_ptr());
    break;

  case STRING:
  case BYTEARRAY:
    if ((_buffer != nullptr) && (_size > 0))
      result = std::string((const char*)_buffer,_size);
    break;

  default:
    break;
  }
  return result;
}

/*
 * \\fn ValueData::byte_buffer ValueData::get_buffer
 *
 * created on: Jul 30, 2019
 * author: daniel
 *
 */
ValueData::byte_buffer ValueData::get_byte_array() const
{
  byte_buffer result;
  if ((_buffer != nullptr) && (_size > 0))
    result = byte_buffer(_buffer,_buffer + _size);

  return result;
}

/*
 * \\fn void ValueData::extract
 *
 * created on: Jul 31, 2019
 * author: daniel
 *
 */
void ValueData::extract(ValueData& where, int start, int length) const
{
  where._type = _type;
  if (start >= static_cast<int>(size()) || (length == 0))
  {
    where._size = 0;
    if (where._buffer != nullptr)
      delete[] where._buffer;

    where._buffer = nullptr;
  }
  else
  {
    if ((start + length) > static_cast<int>(size()))
      length = size() - start;

    where._size = length;
    if (where._buffer != nullptr)
      delete[] where._buffer;

    where._buffer = new uint8_t[where._size];
    memcpy(where._buffer,&_buffer[start],where._size);
  }
}

} /* namespace jupiter */
} /* namespace brt */
