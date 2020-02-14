//
// Created by Daniel Movsesyan on 2019-06-19.
//

#include <string>
#include <vector>
#include <string.h>
#include <sstream>
#include "metadata.hpp"
#include "meta_impl.hpp"
#include "utils.hpp"


brt::jupiter::Metadata _context;

namespace brt {

namespace jupiter {



/*
 * \\fn void void Metadata::init
 *
 * created on: Sep 9, 2019
 * author: daniel
 *
 */
void Metadata::init()
{
  _impl = new MetaImpl;
}

/*
 * \\fn Destructor Metadata::~Metadata
 *
 * created on: Sep 6, 2019
 * author: dmovsesyan
 *
 */
Metadata::~Metadata()
{
  if (_impl != nullptr)
    delete _impl;
}

/**
 *
 *  Setter Implementation
 *
 */
template<> Metadata& Metadata::set<bool>(const char* key,bool value)
    { _impl->set<bool>(key,value); return *this; }

template<> Metadata& Metadata::set<int>(const char* key,int value)
    { _impl->set<int>(key,value); return *this; }

template<> Metadata& Metadata::set<unsigned long>(const char* key,unsigned long value)
    { _impl->set<unsigned long>(key,value); return *this; }

template<> Metadata& Metadata::set<double>(const char* key,double value)
    { _impl->set<double>(key,value); return *this; }

template<> Metadata& Metadata::set<const char *>(const char* key,const char* value)
    { _impl->set<const char*>(key,value); return *this; }

template<> Metadata& Metadata::set<void*>(const char* key,void* value)
    { _impl->set<void*>(key,value); return *this; }

template<> Metadata& Metadata::set<std::string>(const char* key,std::string value)
    { _impl->set<std::string>(key,value); return *this; }

template<> Metadata& Metadata::set<Metadata::byte_buffer>(const char* key,Metadata::byte_buffer value)
    { _impl->set<Metadata::byte_buffer>(key,value); return *this; }

/**
 *
 *   GETTER IMPLEMENTATION
 *
 */
template<> bool Metadata::get<bool>(const char* key,bool default_value) const
    { return _impl->get<bool>(key,default_value); }

template<> int  Metadata::get<int>(const char* key,int default_value) const
    { return _impl->get<int>(key,default_value); }

template<> unsigned long Metadata::get<unsigned long>(const char* key,unsigned long default_value) const
    { return _impl->get<unsigned long>(key,default_value); }

template<> double Metadata::get<double>(const char* key,double default_value) const
    { return _impl->get<double>(key,default_value); }

template<> const char* Metadata::get<const char*>(const char* key,const char *default_value) const
    { return _impl->get<std::string>(key,default_value).c_str(); }

template<> void* Metadata::get<void*>(const char* key,void* default_value) const
    { return _impl->get<void*>(key,default_value); }

template<> std::string Metadata::get<std::string>(const char* key,std::string default_value) const
    { return _impl->get<std::string>(key,default_value); }

template<> Metadata::byte_buffer Metadata::get<Metadata::byte_buffer>(const char* key,Metadata::byte_buffer default_value) const
    { return _impl->get<Metadata::byte_buffer>(key,default_value); }


/*
 * \\fn bool Metadata::get_at<bool>
 *
 * created on: Sep 9, 2019
 * author: daniel
 *
 */
template<> bool Metadata::get_at<bool>(const char* key,size_t index, const bool& default_value)
{
  if (!_impl->exist(key))
    return default_value;

  ValueData& val = _impl->value(key).at(index);
  return val.get<bool>();
}

/*
 * \\fn int Metadata::get_at<int>
 *
 * created on: Sep 9, 2019
 * author: daniel
 *
 */
template<> int Metadata::get_at<int>(const char* key,size_t index, const int& default_value)
{
  if (!_impl->exist(key))
    return default_value;

  ValueData& val = _impl->value(key).at(index);
  return val.get<int>();
}

/*
 * \\fn unsigned long Metadata::get_at<unsigned long>
 *
 * created on: Sep 9, 2019
 * author: daniel
 *
 */
template<> unsigned long Metadata::get_at<unsigned long>(const char* key,size_t index, const unsigned long& default_value)
{
  if (!_impl->exist(key))
    return default_value;

  ValueData& val = _impl->value(key).at(index);
  return val.get<unsigned long>();
}

/*
 * \\fn double Metadata::get_at<double>
 *
 * created on: Sep 9, 2019
 * author: daniel
 *
 */
template<> double Metadata::get_at<double>(const char* key,size_t index, const double& default_value)
{
  if (!_impl->exist(key))
    return default_value;

  ValueData& val = _impl->value(key).at(index);
  return val.get<double>();
}

/*
 * \\fn std::string Metadata::get_at<std::string>
 *
 * created on: Sep 9, 2019
 * author: daniel
 *
 */
template<> std::string Metadata::get_at<std::string>(const char* key,size_t index, const std::string& default_value)
{
  if (!_impl->exist(key))
    return default_value;

  ValueData& val = _impl->value(key).at(index);
  return val.get<std::string>();
}

/*
 * \\fn Metadata::byte_buffer Metadata::get_at<Metadata::byte_buffer>
 *
 * created on: Sep 9, 2019
 * author: daniel
 *
 */
template<> Metadata::byte_buffer Metadata::get_at<Metadata::byte_buffer>(const char* key,size_t index, const Metadata::byte_buffer& default_value)
{
  if (!_impl->exist(key))
    return default_value;

  ValueData& val = _impl->value(key).at(index);
  return val.get<Metadata::byte_buffer>();
}

/*
 * \\fn size_t Metadata::size
 *
 * created on: Dec 9, 2019
 * author: daniel
 *
 */
size_t Metadata::size(const char* key) const
{
  if (!_impl->exist(key))
    return 0;

  return _impl->value(key).length();
}

/*
 * \\fn ValueData& Metadata::value_at
 *
 * created on: Dec 10, 2019
 * author: daniel
 *
 */
Value Metadata::value_at(const char* key,size_t index)
{
  return Value(_impl->value(key).at(index));
}


/*
 * \\fn void Metadata::parse
 *
 * created on: Nov 22, 2019
 * author: daniel
 *
 */
void Metadata::parse(int argc,char** argv,const char* default_arg_name /*= "<default>"*/)
{
  _impl->parse(argc,argv,default_arg_name);
}

/*
 * \\fn bool Metadata::exist
 *
 * created on: May 17, 2019
 * author: daniel
 *
 */
bool Metadata::exist(const char *key) const
{
  return _impl->exist(key);
}

/*
 * \\fn void Metadata::erase
 *
 * created on: May 17, 2019
 * author: daniel
 *
 */
void Metadata::erase(const char *key)
{
  _impl->erase(key);
}

/*
 * \\fn Metadata& Metadata::copy_key
 *
 * created on: Nov 26, 2019
 * author: daniel
 *
 */
Metadata& Metadata::copy_key(const char* to, const char* from, const Metadata& meta)
{
  _impl->copy_key(to, from, meta._impl);
  return *this;
}

/*
 * \\fn Metadata& Metadata::copy_metadata
 *
 * created on: May 17, 2019
 * author: daniel
 *
 */
Metadata& Metadata::copy_metadata(const Metadata *from)
{
  _impl->copy_metadata(from->_impl);
  return *this;
}

/*
 * \\fn std::vector<std::string> Metadata::matching_keys
 *
 * created on: Aug 20, 2019
 * author: daniel
 *
 */
std::vector<std::string> Metadata::matching_keys(const char *regex) const
{
  return _impl->matching_keys(regex);
}

/*
 * \\fn Metadata& Metadata::add
 *
 * created on: May 17, 2019
 * author: daniel
 *
 */
Metadata& Metadata::add(const char *semicolon_separated_list)
{
  _impl->add(semicolon_separated_list);
  return *this;
}

/*
 * \\fn Metadata& Metadata::operator+=
 *
 * created on: May 17, 2019
 * author: daniel
 *
 */
Metadata& Metadata::operator=(const Metadata& params)
{
  return copy_metadata(&params);
}
/*
 * \\fn Metadata& Metadata::operator+=
 *
 * created on: May 17, 2019
 * author: daniel
 *
 */
Metadata& Metadata::operator+=(const Metadata& params)
{
  _impl->add(params._impl);
  return *this;
}

/*
 * \\fn Metadata& Metadata::operator+=
 *
 * created on: May 17, 2019
 * author: daniel
 *
 */
Metadata& Metadata::operator+=(const char *semicolon_separated_list)
{
  _impl->add(semicolon_separated_list);
  return *this;
}


/*
 * \\fn Metadata& Metadata::operator+=
 *
 * created on: May 17, 2019
 * author: daniel
 *
 */
bool Metadata::operator()(const char *key) const 
{
  return _impl->get<bool>(key,false); 
}

/*
 * \\fn std::string std::string Metadata::to_string
 *
 * created on: May 17, 2019
 * author: daniel
 *
 */
std::string Metadata::to_string() const
{
  return _impl->to_string();
}

/*
 * \\fn std::string Metadata::to_json
 *
 * created on: Aug 12, 2019
 * author: daniel
 *
 */
std::string Metadata::to_json(bool nicely_formatted /*= true*/) const
{
  return _impl->to_json(nicely_formatted);
}

#if COMPILE_WITH_ROS

/*
 * \\fn void Metadata::import_ros_params
 *
 * created on: Dec 12, 2019
 * author: daniel
 *
 */
void Metadata::import_ros_params(const ros::NodeHandle &node_handle, const char* name_space)
{
  std::vector<std::string> keys;
  if (!node_handle.getParamNames(keys))
    return;

  for (std::string key : keys)
  {
    if (key.find(name_space) != 0)
      continue;

    key = key.substr(strlen(name_space));
    if (key.empty())
      continue;

    XmlRpc::XmlRpcValue  rpc;
    if (node_handle.getParam(key,rpc))
      add_ros_param(key,rpc);
  }
}

/*
 * \\fn void Metadata::add_ros_param
 *
 * created on: Dec 13, 2019
 * author: daniel
 *
 */
void Metadata::add_ros_param(std::string key,XmlRpc::XmlRpcValue& rpc)
{
  switch(rpc.getType())
  {
  case XmlRpc::XmlRpcValue::TypeBoolean:
    _impl->add(key.c_str(), (bool)rpc);
    break;

  case XmlRpc::XmlRpcValue::TypeInt:
    _impl->add(key.c_str(), (int)rpc);
    break;

  case XmlRpc::XmlRpcValue::TypeDouble:
    _impl->add(key.c_str(), (double)rpc);
    break;

  case XmlRpc::XmlRpcValue::TypeString:
    _impl->add(key.c_str(), std::string(rpc).c_str());
    break;

  case XmlRpc::XmlRpcValue::TypeStruct:
    for (auto srpc : rpc)
    {
      std::string name = key + srpc.first;
      add_ros_param(name,srpc.second);
    }
    break;

  case XmlRpc::XmlRpcValue::TypeArray:
    for (size_t index = 0; index < rpc.size(); index++)
      add_ros_param(key,rpc[index]);

    break;

  default:
    break;
  }
}
#endif

} // jupiter
} // brt
