#pragma once

#include <cstdint>
#include <iosfwd>

namespace L4 {

// SerializerHelper provides help functions to write to IStreamWriter.
class SerializerHelper {
 public:
  SerializerHelper(std::ostream& stream) : m_stream{stream} {}

  SerializerHelper(const SerializerHelper&) = delete;
  SerializerHelper& operator=(const SerializerHelper&) = delete;

  template <typename T>
  void Serialize(const T& obj) {
    m_stream.write(reinterpret_cast<const char*>(&obj), sizeof(obj));
  }

  void Serialize(const void* data, std::uint32_t dataSize) {
    m_stream.write(static_cast<const char*>(data), dataSize);
  }

 private:
  std::ostream& m_stream;
};

// DeserializerHelper provides help functions to read from IStreamReader.
class DeserializerHelper {
 public:
  DeserializerHelper(std::istream& stream) : m_stream{stream} {}

  DeserializerHelper(const DeserializerHelper&) = delete;
  DeserializerHelper& operator=(const DeserializerHelper&) = delete;

  template <typename T>
  void Deserialize(T& obj) {
    m_stream.read(reinterpret_cast<char*>(&obj), sizeof(obj));
  }

  void Deserialize(void* data, std::uint32_t dataSize) {
    m_stream.read(static_cast<char*>(data), dataSize);
  }

 private:
  std::istream& m_stream;
};

}  // namespace L4
