#include "riffmaster_full_mapping.h"
#include "riffmaster_slots.h"
#include "riffmaster_sensitivity.h"
#include <array>
#include <cassert>
using namespace xe::hid::riffmaster;
int main(){
 assert(ApplyTiltGain(0,150)==0 && ApplyTiltGain(-1,150)==0);
 assert(ApplyTiltGain(10000,150)==15000 && ApplyTiltGain(23000,150)==32767);
 assert(ApplyTiltGain(32767,150)==32767 && ApplyTiltGain(16000,100)==16000);
 assert(ValidTiltGain(0)==150 && ValidTiltGain(251)==150);
 for(unsigned gain: {50u,100u,135u,150u,250u}){
  int previousTilt=0;
  for(int value=0;value<=32767;++value){
   const auto mapped=ApplyTiltGain(static_cast<int16_t>(value),gain);
   assert(mapped>=previousTilt && mapped>=0);previousTilt=mapped;
   if(gain==100)assert(mapped==value);
  }
 }
 std::array<uint8_t,32> p{};FullState state;
 assert(DecodeFullReport(0x20,p.data(),p.size(),&state));
 assert(state.whammy==-32768 && state.tilt==0 && state.buttons==0);
 p[3]=255;p[2]=255;
 assert(DecodeFullReport(0x20,p.data(),p.size(),&state));
 assert(state.whammy==32767 && state.tilt==32767);
 p[3]=128;p[2]=128;
 assert(DecodeFullReport(0x20,p.data(),p.size(),&state));
 assert(state.whammy==128 && state.tilt==16448);
 p[5]=0x1F;p[1]=1;
 assert(DecodeFullReport(0x20,p.data(),p.size(),&state));assert(state.buttons==0xF101);
 p[5]=0;p[6]=3;p[0]=12;p[1]=2;
 assert(DecodeFullReport(0x20,p.data(),p.size(),&state));assert(state.buttons==0x3072);
 SlotLayout slots;
 slots=PlanSlots(0,true,slots);assert(slots.guitar==0);
 slots=PlanSlots(1,true,slots);assert(slots.guitar==0 && slots.native[1]==0);
 slots=PlanSlots(3,true,slots);assert(slots.guitar==0 && slots.native[1]==0 && slots.native[2]==1);
 slots=PlanSlots(0,true,slots);assert(slots.guitar==0);
 slots=PlanSlots(1,false,slots);assert(slots.guitar==-1 && slots.native[0]==0);
 slots=PlanSlots(1,true,slots);assert(slots.native[0]==0 && slots.guitar==1);
 slots=PlanSlots(15,true,slots);assert(slots.guitar==1);
 for(int i=0;i<4;++i)assert(i!=slots.guitar || slots.native[i]==-1);
 SlotLayout full=PlanSlots(15,false,{});full=PlanSlots(15,true,full);assert(full.guitar==-1);
 // Exhaust every connect/disconnect transition from each initial layout.
 for(uint32_t oldMask=0;oldMask<16;++oldMask)for(int oldGuitar=0;oldGuitar<2;++oldGuitar){
  const auto beforeSlots=PlanSlots(oldMask,oldGuitar!=0,{});
  for(uint32_t newMask=0;newMask<16;++newMask)for(int newGuitar=0;newGuitar<2;++newGuitar){
   const auto after=PlanSlots(newMask,newGuitar!=0,beforeSlots);uint32_t seen=0;
   if(newGuitar && beforeSlots.guitar>=0)assert(after.guitar==beforeSlots.guitar);
   if(!newGuitar)assert(after.guitar==-1);
   for(int guest=0;guest<4;++guest){
    int host=after.native[guest];
    if(host>=0){assert(host<4 && guest!=after.guitar && (newMask&(1u<<host)) && !(seen&(1u<<host)));seen|=1u<<host;}
    int previousHost=beforeSlots.native[guest];
    if(previousHost>=0 && (newMask&(1u<<previousHost)))assert(host==previousHost);
   }
  }
 }
 const FullState before=state;
 assert(!DecodeFullReport(0x07,p.data(),p.size(),&state));
 assert(!DecodeFullReport(0x20,p.data(),6,&state));
 assert(!DecodeFullReport(0x20,nullptr,32,&state));
 assert(state.buttons==before.buttons && state.whammy==before.whammy);
}
