// BSD-3-Clause. Keep an instrument's guest slot stable as native pads join.
#pragma once
#include <array>
#include <cstdint>
namespace xe::hid::riffmaster {
struct SlotLayout {
 std::array<int,4> native{{-1,-1,-1,-1}}; // guest slot -> host XInput index
 int guitar=-1;
};
inline SlotLayout PlanSlots(uint32_t native_mask,bool guitar_present,const SlotLayout& previous){
 SlotLayout result;
 uint32_t assigned=0;
 if(guitar_present && previous.guitar>=0 && previous.guitar<4)result.guitar=previous.guitar;
 // Keep existing host controllers where the guest already knows them.
 for(int guest=0;guest<4;++guest){
  int host=previous.native[guest];
  if(guest!=result.guitar && host>=0 && host<4 && (native_mask&(1u<<host)) && !(assigned&(1u<<host))){result.native[guest]=host;assigned|=1u<<host;}
 }
 for(int host=0;host<4;++host){
  if(!(native_mask&(1u<<host)) || (assigned&(1u<<host)))continue;
  for(int guest=0;guest<4;++guest){
   if(guest!=result.guitar && result.native[guest]<0){result.native[guest]=host;assigned|=1u<<host;break;}
  }
 }
 // A newly attached guitar uses the first genuinely unused guest slot.
 if(guitar_present && result.guitar<0){for(int guest=0;guest<4;++guest){if(result.native[guest]<0){result.guitar=guest;break;}}}
 return result;
}
}
