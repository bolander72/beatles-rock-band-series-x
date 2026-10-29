// BSD-3-Clause. Decode a Riffmaster 0x20 GIP payload, without its USB header.
#pragma once
#include <cstddef>
#include <cstdint>
namespace xe::hid::riffmaster {
struct FullState {
 uint16_t buttons=0;
 int16_t whammy=-32768;
 int16_t tilt=0;
};
inline bool DecodeFullReport(uint32_t report_id,const uint8_t* p,size_t size,FullState* out){
 if(!out || !p || report_id!=0x20 || size<7 || size>128)return false;
 FullState result;
 const uint16_t word=uint16_t(p[0]) | uint16_t(uint16_t(p[1])<<8);
 if(word&0x0004)result.buttons|=0x0010;
 if(word&0x0008)result.buttons|=0x0020;
 if(word&0x0100)result.buttons|=0x0001;
 if(word&0x0200)result.buttons|=0x0002;
 if(word&0x0400)result.buttons|=0x0004;
 if(word&0x0800)result.buttons|=0x0008;
 const uint8_t frets=p[5]|p[6];
 if(frets&1)result.buttons|=0x1000;
 if(frets&2)result.buttons|=0x2000;
 if(frets&4)result.buttons|=0x8000;
 if(frets&8)result.buttons|=0x4000;
 if(frets&16)result.buttons|=0x0100;
 if(p[6]&0x1F)result.buttons|=0x0040;
 result.whammy=static_cast<int16_t>(int32_t(p[3])*257-32768);
 result.tilt=static_cast<int16_t>((uint32_t(p[2])*257)/2);
 *out=result;return true;
}
}
