//
// Created by Daniel Movsesyan on 2019-04-06.
//

#ifndef EXPRESSIONS_VALUE_HPP
#define EXPRESSIONS_VALUE_HPP

#include <string>
#include <vector>
#include <cassert>

namespace brt {

namespace jupiter {


class ValueData;

/**
 *
 */
class Value
{
public:
  typedef std::vector<uint8_t>    byte_buffer;
  typedef void *                  pointer;

  Value();
  Value(const Value& val);
  Value(ValueData& vd);
  virtual ~Value();

          operator bool() const;
          operator int() const;
          operator double() const;
          operator std::string() const;
          operator byte_buffer() const;
          operator pointer() const;


          /*
           * \\fn set
           *
           * created on: Jul 30, 2019
           * author: daniel
           *
           */
          template<typename T>
          Value&                 set(T value,size_t size = (size_t)-1);// { return *this; }

          /*
           *
           */
          Value&                  set_byte_array(const uint8_t* value,size_t size,bool little_endian = true);

          size_t                  size() const;

          // Unary Operators
          Value&                  operator++();
          Value                   operator++(int);
          Value&                  operator--();
          Value                   operator--(int);
          Value                   operator-();
          Value                   operator!();
          Value                   operator~();

          // Arithmetic
          Value                   operator+(const Value& val) const;
          Value                   operator-(const Value& val) const;
          Value                   operator/(const Value& val) const;
          Value                   operator%(const Value& val) const;
          Value                   operator*(const Value& val) const;
          Value                   operator|(const Value& val) const;
          Value                   operator&(const Value& val) const;
          Value                   operator^(const Value& val) const;
          Value                   operator>>(const Value& val) const;
          Value                   operator<<(const Value& val) const;

          Value&                  operator+=(const Value& val);
          Value&                  operator-=(const Value& val);
          Value&                  operator/=(const Value& val);
          Value&                  operator%=(const Value& val);
          Value&                  operator*=(const Value& val);
          Value&                  operator|=(const Value& val);
          Value&                  operator&=(const Value& val);
          Value&                  operator^=(const Value& val);
          Value&                  operator>>=(const Value& val);
          Value&                  operator<<=(const Value& val);
          Value&                  operator=(const Value& val);

          // Logic
          Value                   operator==(const Value& val) const;
          Value                   operator!=(const Value& val) const;
          Value                   operator>(const Value& val) const;
          Value                   operator<(const Value& val) const;
          Value                   operator>=(const Value& val) const;
          Value                   operator<=(const Value& val) const;
          Value                   operator&&(const Value& val) const;
          Value                   operator||(const Value& val) const;

          Value                   at(size_t index);
          Value                   sub_array(int start, int length);

private:
          template<typename T>
          Value&                  _set(T value,size_t size = (size_t)-1);
private:
  ValueData*                      _data;
};

} // jupiter
} // brt

#endif //EXPRESSIONS_VALUE_HPP
