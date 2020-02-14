/*
 * ValueData.hpp
 *
 *  Created on: Jul 30, 2019
 *      Author: daniel
 */

#ifndef SCRIPT_VALUEDATA_HPP_
#define SCRIPT_VALUEDATA_HPP_

#include <stdint.h>
#include <stdlib.h>

#include <string>
#include <vector>
#include <atomic>
#include <unordered_map>

namespace brt
{
namespace jupiter
{

/*
 * \\class RealUtil
 *
 * created on: Aug 6, 2019
 *
 */
class RealUtil
{
public:
  static  void                    from_double(double,uint8_t*,size_t);
  static  double                  to_double(const uint8_t*,size_t);
};

/*
 * \\class ValueData
 *
 * created on: Jul 30, 2019
 *
 */
class ValueData
{
public:
  typedef std::vector<uint8_t>    byte_buffer;

  template<class T> class Iter
  {
  public:
    Iter(T *data, size_t index = 0) : _data(data), _index(index) {}
    virtual ~Iter() {}

    T& operator*()                { return _data->at(_index); }

    Iter& operator++()            { _index++; return *this; }
    bool operator==(const Iter& rval) { return _index == rval._index; }

  private:
    T*                            _data;
    size_t                        _index;
  };

  typedef Iter<ValueData>         iterator;
  typedef Iter<const ValueData>   const_iterator;

  enum value_type
  {
    NIL,
    BOOL,
    INT,
    FLOAT,
    ULONGLONG,
    PTR,
    STRING,
    BYTEARRAY
  };

  ValueData()
  : _buffer(nullptr)  , _size(0)  , _type(NIL)
  , _array()  , _ref_cntr(1)  , _little_endian(true)
  {  }

  ValueData(const ValueData& data);
  virtual ~ValueData()
  {
    if (_buffer != nullptr)
      delete[] _buffer;
  }

          ValueData&              operator=(const ValueData& data);
          value_type              type() const { return _type; }
          size_t                  size() const { return _size; }
          size_t                  length() const { return _array.size() + 1; }


          ValueData&              set_bool(bool value,size_t size = sizeof(bool));
          ValueData&              set_int(int value,size_t size = sizeof(int));
          ValueData&              set_float(double value,size_t size = sizeof(double));
          ValueData&              set_ull(uint64_t value,size_t size = sizeof(uint64_t));
          ValueData&              set_ptr(void* value,size_t size = sizeof(void*));
          ValueData&              set_string(const char* value,size_t size = (size_t)-1);
          ValueData&              set_byte_array(const uint8_t* value,size_t size,bool little_endian = true);
          ValueData&              set_byte_array(const byte_buffer&,bool little_endian = true);


          template<typename T>    struct default_arg { static size_t get() { return sizeof(T); } };
          template<typename T>    ValueData& set(T value,size_t size = default_arg<T>::get()) { return *this; }

          bool                    get_bool() const;
          int                     get_int() const;
          double                  get_float() const;
          uint64_t                get_ull() const;
          void*                   get_ptr() const;
          std::string             get_string() const;
          byte_buffer             get_byte_array() const;

          template<typename T>    T get() const { return T(); }

          ValueData&              at(size_t index)
          {
            if (index == 0)
              return *this;

            index -= 1;
            if (_array.size() <= index)
              _array.resize(index + 1);

            return _array.at(index);
          }

          iterator                begin() { return iterator(this); }
          iterator                end()   { return iterator(this,length()); }

          const_iterator          begin() const { return const_iterator(this); }
          const_iterator          end() const { return   const_iterator(this,length()); }

          void                    extract(ValueData& where, int start, int length) const;

          void                    add_ref() { ++_ref_cntr; }
          void                    release() { if (--_ref_cntr == 0) delete this; }

private:
  uint8_t*                        _buffer;
  size_t                          _size;
  value_type                      _type;
  std::vector<ValueData>          _array;

  std::atomic_uint_fast32_t       _ref_cntr;
  bool                            _little_endian;
};

typedef std::unordered_map<std::string,ValueData>     value_database;

template<> struct ValueData::default_arg<const char*> { static size_t get() { return (size_t)-1; } };
template<> struct ValueData::default_arg<char*> { static size_t get() { return (size_t)-1; } };

template<> ValueData& ValueData::set<bool>(bool value,size_t size);
template<> ValueData& ValueData::set<int>(int value,size_t size);
template<> ValueData& ValueData::set<double>(double value,size_t size);
template<> ValueData& ValueData::set<float>(float value,size_t size);
template<> ValueData& ValueData::set<uint64_t>(uint64_t value,size_t size);
template<> ValueData& ValueData::set<void*>(void* value,size_t size);
template<> ValueData& ValueData::set<const char*>(const char* value,size_t size);

template<> bool ValueData::get<bool>() const;
template<> int ValueData::get<int>() const;
template<> double ValueData::get<double>() const;
template<> float ValueData::get<float>() const;
template<> uint64_t ValueData::get<uint64_t>() const;
template<> void* ValueData::get<void*>() const;
template<> std::string ValueData::get<std::string>() const;
template<> ValueData::byte_buffer ValueData::get<ValueData::byte_buffer>() const;

} /* namespace jupiter */
} /* namespace brt */

#endif /* SCRIPT_VALUEDATA_HPP_ */
