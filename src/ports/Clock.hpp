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


// Commands:
//   help                        - show this help
//   show [pat]                  - show pattern (0..3), or all if omitted
//   bpm <value>                 - set global BPM
//   div <pat> <division>        - set pattern division (1=1/4,2=1/8,4=1/16,...)
//   len <pat> <length>          - set pattern length (0..64)
//   idx <pat> <step> <0..8>     - set step's note index (0=REST)
//   vel <pat> <step> <1..127>   - set velocity
//   gate <pat> <step> <1..200>  - set gate percent
//   oct <pat> <step> <-8..+8>   - set octave transpose
//   prob <pat> <step> <0..100>  - set probability
//   on <pat> <step>             - enable step
//   off <pat> <step>            - disable step