// BSD-3-Clause. Positive guitar tilt gain; decoding stays unmodified.
#pragma once
#include <cstdint>
namespace xe::hid::riffmaster {
constexpr unsigned kDefaultTiltGainPercent=150;
inline unsigned ValidTiltGain(unsigned percent){
 return percent>=50 && percent<=250?percent:kDefaultTiltGainPercent;
}
inline int16_t ApplyTiltGain(int16_t tilt,unsigned percent){
 if(tilt<=0)return 0;
 const uint32_t scaled=uint32_t(tilt)*ValidTiltGain(percent)/100;
 return static_cast<int16_t>(scaled>32767?32767:scaled);
}
}
