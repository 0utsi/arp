#pragma once
#include <cstdint>

// ports::IClock – "port" czasu. Na PC użyjemy std::chrono,
// na mikrokontrolerze – hardware timer. Reszta programu o tym nie wie.
namespace ports {
struct IClock {
  virtual ~IClock() = default;
  virtual uint64_t now_ms() const = 0; // czas w milisekundach od startu
};
} // namespace ports
