//
// Created by Daniel Movsesyan on 2019-04-19.
//

#ifndef MOTEC_BU_UTILS_HPP
#define MOTEC_BU_UTILS_HPP


#include <string>
#include <memory>
#include <atomic>
#include <vector>
#include <set>
#include <algorithm>
#include <iostream>

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define DEFAULT_FIFO_SIZE                   (32)
#define BYTES_PER_PIXELS(x)                 (((x - 1) >> 3) + 1)

namespace brt {

namespace jupiter {




/**
 *
 */
template<typename T,int S = DEFAULT_FIFO_SIZE>
class FIFO
{
public:
  FIFO()
  : _write(0)
  , _read(0)
  {
    for (size_t index = 0;index < S;index++)
      _elem[index] = T(0);
  }

  virtual ~FIFO() {}

  /**
   *
   * @param elem
   */
  bool                            push(T elem)
  {
    if (available() == 0)
      return false;

    uint32_t wr = _write.load();
    _elem[wr] = elem;

    if (++wr >= S)
      wr = 0;

    _write.store(wr);
    return true;
  }

  /**
   *
   * @param elem
   * @return
   */
  bool                            pop(T& elem)
  {
    uint32_t wr = _write.load();
    uint32_t rd = _read.load();

    if (rd == wr)
      return false;

    elem = _elem[rd];
    if (++rd >= S)
      rd = 0;

    _read.store(rd);
    return true;
  }

  /**
   *
   * @return
   */
  uint32_t                        available() const
  {
    uint32_t wr = _write.load();
    uint32_t rd = _read.load();

    if (rd > wr)
      return rd - wr - 1;

    return S - wr + rd - 1;
  }

  /**
   *
   * @return
   */
  uint32_t                        num_elements() const
  {
    uint32_t wr = _write.load();
    uint32_t rd = _read.load();

    if (rd > wr)
      return S - rd + wr;

    return wr - rd;
  }

private:
  std::atomic_uint_fast32_t       _write;
  std::atomic_uint_fast32_t       _read;
  T                               _elem[S];
};



/*
 * \\class Pointer
 *
 * created on: Nov 25, 2019
 *
 */
template <class T>
class Pointer
{
private:

  /*
   * \\struct Data
   *
   * created on: Nov 25, 2019
   *
   */
  struct Data
  {
    Data(size_t size)
    : _size(size)
    , _data(nullptr)
    , _ref_cnt(1)
    {
      if (_size > 0)
        _data = malloc(_size);
    }

    ~Data()
    {
      if (_data != nullptr)
        ::free(_data);
    }

    void addref()
    { _ref_cnt++; }


    void release()
    {
      if (--_ref_cnt == 0)
        delete this;
    }

    size_t                          _size;
    void*                           _data;
    std::atomic_int_fast32_t        _ref_cnt;
  };

public:
  Pointer() : _data(nullptr) {}

  /*
   * \\fn Constructor Pointer
   *
   * created on: Nov 25, 2019
   * author: daniel
   *
   */
  Pointer(size_t size) : _data(new Data(size * sizeof(T)))
  {
    if (_data->_data == nullptr)
    {
      delete _data;
      _data = nullptr;
    }
  }

  /*
   * \\fn Constructor Pointer
   *
   * created on: Nov 25, 2019
   * author: daniel
   *
   */
  Pointer(const Pointer& rval) : _data(rval._data)
  {
    if (_data != nullptr)
      _data->addref();
  }

  /*
   * \\fn Destructor ~Pointer
   *
   * created on: Nov 25, 2019
   * author: daniel
   *
   */
  ~Pointer()
  {
    if (_data != nullptr)
      _data->release();
  }

  /*
   * \\fn Pointer& operator=
   *
   * created on: Nov 25, 2019
   * author: daniel
   *
   */
  Pointer& operator=(const Pointer& rval)
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
    if ((_data == nullptr) || (_data->_data == nullptr))
      return 0;

    return _data->_size / sizeof(T);
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
    return ((_data != nullptr) && (_data->_data != nullptr));
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
      _data = new Data(size * sizeof(T));

    else if (_data->_size != (size * sizeof(T)))
    {
      _data->release();
      _data = new Data(size * sizeof(T));
    }

    if (_data->_data == nullptr)
    {
      delete _data;
      _data = nullptr;
      return false;
    }

    memcpy(_data->_data, data, size * sizeof(T));
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
    if ((_data == nullptr) || (_data->_data == nullptr))
      return 0;

    size_t size_to_copy = std::min(size * sizeof(T),_data->_size);
    memcpy(data, _data->_data, size_to_copy);
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
    if ((_data == nullptr) || (_data->_data == nullptr))
      return nullptr;

    return (T*)(_data->_data);
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
    if ((_data == nullptr) || (_data->_data == nullptr))
      return nullptr;

    return (T*)(_data->_data);
  }


  /*
   * \\fn void fill
   *
   * created on: Nov 25, 2019
   * author: daniel
   *
   */
  void fill(uint8_t value)
  {
    if ((_data == nullptr) || (_data->_data == nullptr))
      return;

    memset(_data->_data, value, _data->_size);
  }


  /*
   * \\fn void copy
   *
   * created on: Nov 25, 2019
   * author: daniel
   *
   */
  bool copy(Pointer<T>& dst)
  {
    if (!dst || (dst.size() != size()))
    {
      dst = Pointer<T>(size());
      if (!dst)
        return false;
    }

    memcpy(dst._data->_data, _data->_data, _data->_size);
    return true;
  }

private:
  Data*                           _data;
};

/*
 * \\enum DisplayType
 *
 * created on: Nov 19, 2019
 *
 */
enum DisplayType
{
  eNone = 0,
  eLocalDisplay = 1,
  eRemoteDisplay = 2,
  eAllDisplays = eLocalDisplay | eRemoteDisplay
};

/*
 * \\struct X11Display
 *
 * created on: Jan 22, 2020
 *
 */
struct X11Display
{
  X11Display() : _dt(eNone), _name() {}
  X11Display(DisplayType dt,const char* name) : _dt(dt), _name(name) {}
  X11Display(DisplayType dt,std::string name) : _dt(dt), _name(name) {}

  explicit operator bool() const { return ((_dt != eNone) && !_name.empty()) ;}
  bool operator==(const X11Display& dsp) const { return ((_dt == dsp._dt) && (_name == dsp._name)); }
  bool operator<(const X11Display& dsp) const { return (_dt < dsp._dt) ? true : (_dt > dsp._dt) ? false : (_name < dsp._name); }

  DisplayType                     _dt;
  std::string                     _name;
};

/**
 *
 */
class Utils
{
public:
  Utils() {}
  virtual ~Utils() {}

  template<typename ... Args>
  static std::string              string_format( const std::string& format, Args ... arguments)
  {
    size_t size = snprintf( nullptr, 0, format.c_str(), arguments ... ) + 1; // Extra space for '\0'
    std::unique_ptr<char[]> buf( new char[ size ] );
    snprintf( buf.get(), size, format.c_str(), arguments ... );
    return std::string( buf.get(), buf.get() + size - 1 ); // We don't want the '\0' inside
  }

  static const char*              stristr(const char* src,const char* dst,size_t len = (size_t)-1);
  static size_t                   stristr(const std::string& src,const char* dst,size_t len = (size_t)-1);
  static double                   frame_rate(const char* fr_string);

  static  std::set<X11Display>    enumerate_displays(DisplayType = eAllDisplays);
  static  X11Display              aquire_display(const char* extra_string);

};

} // jupiter
} // brt


#endif //MOTEC_BU_UTILS_HPP
