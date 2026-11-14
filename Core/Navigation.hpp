#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <cmath>
namespace gs {
enum class Page {Home,Global,OSC,LFO,Arp};
struct NavInput { int pageKey=-1;unsigned numbers=0;bool fn=false,left=false,right=false,up=false,down=false,tab=false,reverse=false,enter=false,escape=false,fine=false; };
struct Navigation {
    Page page=Page::Home;
    std::array<int,5> selection{};
    bool editing=false;
    int section=0,menu=0,browser=0,theme=24,savedTheme=24,themeBefore=24,instrument=0;
    int editAddress=-1,loadInstrument=-1;float delta=0;bool saveTheme=false;
    unsigned blockedNumbers=0;uint32_t repeatAt=0,heldSince=0;
    NavInput previous{};
    static int binding(Page page,int slot){
        static const int home[8]={3,5,0,1,2,4,17,7},osc[8]={0,1,2,3,5,4,-1,-1},lfo[8]={18,17,7,16,-1,-1,-1,-1},arp[8]={9,15,11,12,13,8,-1,-1};
        if(slot<0||slot>7)return -1;
        return page==Page::Arp?arp[slot]:page==Page::OSC?osc[slot]:page==Page::LFO?lfo[slot]:home[slot];
    }
    int selected()const{return selection[int(page)];}
    int address()const{return binding(page,selected());}
    void cancelTheme(){if(section==2)theme=themeBefore;}
    void go(Page next){cancelTheme();page=next;section=0;editing=false;}
    void scan(NavInput input,uint32_t ms){
        editAddress=loadInstrument=-1;delta=0;saveTheme=false;
        blockedNumbers&=input.numbers;
        if(input.pageKey>=0&&input.pageKey<5&&input.pageKey!=previous.pageKey)go(Page(input.pageKey));
        unsigned edges=input.numbers&~previous.numbers;
        if(input.fn){blockedNumbers|=input.numbers;for(int i=0;i<5;i++)if(edges&(1u<<i))go(Page(i));}
        unsigned held=input.fn?0:input.numbers&~blockedNumbers;
        for(int i=0;i<8;i++)if(held&(1u<<i)){selection[int(page)]=i;break;}
        if(input.escape&&!previous.escape){
            if(page==Page::Global&&section){cancelTheme();section=0;}
            else if(editing)editing=false;
            else go(Page::Home);
        }
        if(input.tab&&!previous.tab&&page==Page::Global){
            int move=input.reverse?-1:1;
            if(section==0)menu=(menu+move+3)%3;
            else if(section==1)browser=(browser+move+2)%2;
            else if(section==2)theme=(theme+move+38)%38;
        }
        else if(input.tab&&!previous.tab){editing=true;int &slot=selection[int(page)];int d=input.reverse?-1:1;do{slot=(slot+d+8)%8;}while(binding(page,slot)<0);}
        if(input.enter&&!previous.enter&&page==Page::Global&&!held){
            if(section==0){section=menu+1;if(section==1)browser=instrument;else themeBefore=theme;}
            else if(section==1){loadInstrument=browser;instrument=browser;go(Page::Home);}
            else if(section==2){savedTheme=theme;saveTheme=true;section=0;}
            else section=0;
        }
        bool arrow=input.left||input.right||input.up||input.down;
        bool arrowEdge=(input.left&&!previous.left)||(input.right&&!previous.right)||(input.up&&!previous.up)||(input.down&&!previous.down);
        if(arrowEdge){heldSince=ms;repeatAt=ms+400;}
        bool repeat=arrow&&!arrowEdge&&int32_t(ms-repeatAt)>=0;
        if(arrowEdge||repeat){
            int direction=(input.right||input.up)?1:-1;
            if(page==Page::Global&&!held&&!editing){
                // Down/right advances browser entries; up/left goes back.
                int move=(input.right||input.down)?1:-1;
                if(section==0)menu=(menu+move+3)%3;
                else if(section==1)browser=(browser+move+2)%2;
                else if(section==2)theme=(theme+move+38)%38;
                else{editAddress=14;delta=direction*(input.fine?.1f:1.f)/260.f;}
            }else if(held||(editing&&(input.up||input.down))){
                editAddress=address();delta=direction*(input.fine?.002f:((ms-heldSince)>2000?.05f:.02f));
            }else if(editing){
                int &slot=selection[int(page)];do{slot=(slot+direction+8)%8;}while(binding(page,slot)<0);
            }
            repeatAt=ms+(repeat?80:400);
        }
        previous=input;
    }
};
inline float editParameter(unsigned address,float value,float delta,float maxCutoff=16000){
    static const float lo[]={0,.005f,.01f,80,0,0,.05f,0,0,0,.25f,.05f,1,0,40,0,0,0,0},hi[]={2,2,3,16000,.5f,1,20,1,1,3,20,1,4,1,300,8,1,8,4};
    if(address>=19)return value;
    if(address==0||address==8||address==9||address==12||address==13||address==15||address>=16)return std::clamp(value+(delta>0?1.f:-1.f),lo[address],hi[address]);
    float high=address==3?maxCutoff:hi[address];
    bool logarithmic=address==1||address==2||address==3||address==6||address==10;
    float fraction=logarithmic?(std::log(value)-std::log(lo[address]))/(std::log(high)-std::log(lo[address])):(value-lo[address])/(high-lo[address]);
    fraction=std::clamp(fraction+delta,0.f,1.f);
    return logarithmic?std::exp(std::log(lo[address])+fraction*(std::log(high)-std::log(lo[address]))):lo[address]+fraction*(high-lo[address]);
}
}
