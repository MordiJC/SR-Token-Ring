#ifndef SERIALIZABLE_H
#define SERIALIZABLE_H

#include <vector>
#include <cstdint>

class Serializable {
public:
  typedef uint8_t data_type;
  typedef std::vector<Serializable::data_type> container_type;
  typedef size_t size_type;

  virtual ~Serializable() = default;

  virtual size_type fromBinary(const Serializable::container_type & buffer) = 0;

  virtual Serializable::container_type toBinary() const = 0;
};

#endif // SERIALIZABLE_H
