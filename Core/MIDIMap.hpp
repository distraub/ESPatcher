#pragma once
#include <array>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace gs {
// Independent ESPatcher implementation. No upstream MIDI Learn source is copied.
struct MIDIMap {
    static constexpr unsigned count=16;
    std::array<std::atomic<uint32_t>,count> entries{};
    std::atomic<int> learning{-1};
    std::atomic<uint32_t> revision{0};
    static bool allowed(unsigned cc){return cc<120&&cc!=0&&cc!=6&&cc!=32&&cc!=38&&cc!=64&&(cc<96||cc>101);}
    static uint32_t pack(unsigned address,unsigned channel,unsigned cc){return ((address+1)<<11)|(channel<<7)|cc;}
    static bool valid(uint32_t entry){return !entry||((entry>>11)>=1&&(entry>>11)<=19&&allowed(entry&127)&&(entry>>16)==0);}
    uint32_t mapping(unsigned address)const{for(auto &cell:entries){auto e=cell.load();if(e>>11==address+1)return e;}return 0;}
    void clear(unsigned address){for(auto &cell:entries){auto e=cell.load();if(e>>11==address+1){cell.store(0);revision.fetch_add(1);}}}
    // Render side only: bind without moving the parameter, then route later CCs.
    bool receive(unsigned channel,unsigned cc,unsigned value,unsigned &address,float &result){
        if(channel>15||value>127||!allowed(cc))return false;
        int target=learning.exchange(-1);
        if(target>=0&&target<19){
            int slot=-1;
            for(unsigned i=0;i<count;i++){auto e=entries[i].load();if(e&&(e>>11==unsigned(target)+1||(e&2047)==((channel<<7)|cc))){slot=int(i);break;}}
            if(slot<0)for(unsigned i=0;i<count;i++)if(!entries[i].load()){slot=int(i);break;}
            if(slot<0){learning.store(-2);return false;}
            for(unsigned i=0;i<count;i++){auto e=entries[i].load();if(e&&(e>>11==unsigned(target)+1||(e&2047)==((channel<<7)|cc)))entries[i].store(0);}
            entries[slot].store(pack(unsigned(target),channel,cc));revision.fetch_add(1);return false;
        }
        for(auto &cell:entries){auto e=cell.load();if(e&&(e&2047)==((channel<<7)|cc)){
            address=(e>>11)-1;result=scale(address,value);return true;
        }}return false;
    }
    static float scale(unsigned address,unsigned cc){
        static const float lo[]={0,.005f,.01f,80,0,0,.05f,0,0,0,.25f,.05f,1,0,40,0,0,0,0},hi[]={2,2,3,16000,.5f,1,20,1,1,3,20,1,4,1,300,8,1,8,4};
        if(address>=19)return 0;
        float x=std::min(cc,127u)/127.f;
        bool log=address==1||address==2||address==3||address==6||address==10;
        float value=log?lo[address]*std::pow(hi[address]/lo[address],x):lo[address]+x*(hi[address]-lo[address]);
        bool discrete=address==0||address==8||address==9||address==12||address==13||address==15||address>=16;
        return discrete?std::round(value):value;
    }
};
}
