#include "../Core/MIDIMap.hpp"
#include <cassert>
#include <iostream>
int main(){
 gs::MIDIMap maps;unsigned address=99;float value=0;
 maps.learning.store(3);assert(!maps.receive(0,74,0,address,value));assert(maps.mapping(3));
 assert(!maps.receive(1,74,127,address,value));assert(maps.receive(0,74,127,address,value));assert(address==3&&std::abs(value-16000)<.01);
 maps.learning.store(5);assert(!maps.receive(0,74,20,address,value));assert(!maps.mapping(3)&&maps.mapping(5));
 assert(maps.receive(0,74,127,address,value)&&address==5&&value==1);
 maps.learning.store(7);assert(!maps.receive(0,64,127,address,value));assert(maps.learning.load()==7);
 assert(!gs::MIDIMap::valid(0xffffffff));assert(gs::MIDIMap::valid(maps.mapping(5)));
 maps.clear(5);assert(!maps.mapping(5));
 for(unsigned a=0;a<19;a++){assert(std::isfinite(gs::MIDIMap::scale(a,0)));assert(std::isfinite(gs::MIDIMap::scale(a,127)));}
 gs::MIDIMap full;
 for(unsigned i=0;i<16;i++){full.learning.store(int(i));full.receive(0,40+i,0,address,value);}
 full.learning.store(18);assert(!full.receive(0,80,0,address,value));assert(full.learning.load()==-2);
 for(unsigned i=0;i<16;i++)assert(full.mapping(i));
 std::cout<<"PASS MIDI Learn: exact channels, first CC no jump, reassignment, reserved controllers, scaling and clear\n";
}
