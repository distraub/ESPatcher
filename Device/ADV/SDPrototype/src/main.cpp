// Experimental persistent runtime. No OTA/flash writes exist in this target.
#include <Arduino.h>
#include <M5Cardputer.h>
#include <SPI.h>
#include <SD.h>
#include <esp_heap_caps.h>
#include <mbedtls/sha256.h>
#include <atomic>
#include <memory>
#include <algorithm>
#include <vector>
#include "../../../../Core/InstrumentPackage.hpp"
#include "../../../../Core/WasmInstrument.hpp"
#include "../../../../Core/Navigation.hpp"
#include "../../../../Core/Scope.hpp"
#include "../../../../Firmware/lfo.h"
#include "../../src/Themes.h"
static constexpr unsigned rate=32000,frames=256;
static std::atomic<bool> pauseRequested{false},pauseAcknowledged{false},panicRequested{false};
static std::atomic<gs::WasmInstrument*> running{nullptr};
static std::unique_ptr<gs::WasmInstrument> owner;
static std::unique_ptr<gs::InstrumentPackage> activePackage;
static unsigned engineIndex=0,sceneIndex=0,knob=0;
static std::array<float,19> values{};
struct Event {uint8_t kind,index;float value;};
static Event queue[64];static std::atomic<unsigned> written{0},readIndex{0};
static std::atomic<uint32_t> maxUs{0},late{0},fault{0},voices{0},phase{0};
static int16_t pcm[3][frames];static std::atomic<bool> busy[3]{};
static gs::Scope scope;static M5Canvas canvas(&M5Cardputer.Display);
struct LibraryItem {std::string path,name;unsigned engines;};
static std::array<LibraryItem,8> library;static unsigned itemCount=0,selected=0,selectedEngine=0;
static enum View {Library,Engines,Instrument} view=Library;
static bool sdReady=false,audioReady=false,fatal=false;
static TaskHandle_t audioHandle=nullptr;
static std::string status="Insert SD: /ESPatcher/Instruments";
static int octave=4;static std::array<int,13> held;
static std::array<bool,13> blocked{};
static unsigned previousNumbers=0;static bool previousL=false,previousEnter=false,previousEscape=false,previousUp=false,previousDown=false,previousLeft=false,previousRight=false,previousZ=false,previousX=false,previousSpace=false,previousR=false,previousMinus=false;
static const char noteKeys[]="awsedftgyhujk";
static void enqueue(Event event){auto w=written.load();if(w-readIndex.load(std::memory_order_acquire)>=64){panicRequested.store(true);return;}queue[w%64]=event;written.store(w+1,std::memory_order_release);}
static bool key(char c){return M5Cardputer.Keyboard.isKeyPressed(c);}
static void silence(){panicRequested.store(true);held.fill(-1);for(unsigned i=0;i<13;i++)blocked[i]=key(noteKeys[i]);}
static void release(void*,const void* data,uint8_t){for(unsigned i=0;i<3;i++)if(data==pcm[i])busy[i].store(false,std::memory_order_release);}
static void audioTask(void*){
    unsigned buffer=0;float samples[frames]{};std::array<float,19> parameters{};
    for(;;){
        if(pauseRequested.load(std::memory_order_acquire)){
            if(auto vm=running.load())vm->panic();
            pauseAcknowledged.store(true,std::memory_order_release);
            while(pauseRequested.load(std::memory_order_acquire))vTaskDelay(1);
            pauseAcknowledged.store(false,std::memory_order_release);
        }
        if(busy[buffer].load()||M5Cardputer.Speaker.isPlaying(0)>=2){vTaskDelay(1);continue;}
        auto started=micros();auto vm=running.load(std::memory_order_acquire);
        auto r=readIndex.load(),w=written.load(std::memory_order_acquire);
        if(panicRequested.exchange(false)){if(vm)vm->panic();r=w;}
        while(r!=w){auto event=queue[r++%64];
            if(event.kind==0&&vm)vm->noteOn(event.index,unsigned(event.value));
            if(event.kind==1&&event.index<19)parameters[event.index]=event.value;
        }readIndex.store(r,std::memory_order_release);
        if(vm){vm->parameters(parameters);vm->render(samples,nullptr,frames);voices.store(std::max(0,vm->voicesActive()));phase.store(vm->lfoStatus());fault.store(vm->faulted());}
        else {std::fill_n(samples,frames,0);voices.store(0);phase.store(0);}
        for(unsigned i=0;i<frames;i++)pcm[buffer][i]=int16_t(std::clamp(samples[i],-.9f,.9f)*32767);
        scope.push(samples,frames);auto elapsed=micros()-started;if(elapsed>maxUs.load())maxUs.store(elapsed);if(vm&&elapsed>8000)late.fetch_add(1);
        busy[buffer].store(true,std::memory_order_release);
        if(!M5Cardputer.Speaker.playRaw(pcm[buffer],frames,rate,false,1,0,false))busy[buffer].store(false);
        buffer=(buffer+1)%3;
    }
}
static bool mountSD(){SPI.begin(40,39,14,12);sdReady=SD.begin(12,SPI,12000000);return sdReady;}
static void readMetadata(File& file,gs::InstrumentPackage& package){
    gs::readInstrumentPackage([&](uint32_t offset,void *target,uint32_t count){return file.seek(offset)&&file.read((uint8_t*)target,count)==count;},uint32_t(file.size()),package);
}
static void scanLibrary(){
    itemCount=selected=0;unsigned bad=0;
    if(!sdReady&&!mountSD()){status="SD unavailable. Insert card, press R.";return;}
    File directory=SD.open("/ESPatcher/Instruments");
    if(!directory||!directory.isDirectory()){status="Missing /ESPatcher/Instruments";return;}
    for(File file=directory.openNextFile();file;file=directory.openNextFile()){
        String name=file.name();if(file.isDirectory()||!name.endsWith(".espkg"))continue;
        try{
            auto package=std::make_unique<gs::InstrumentPackage>();readMetadata(file,*package);
            if(itemCount<library.size()){
                std::string path=file.name();if(path.empty()||path[0]!='/')path="/ESPatcher/Instruments/"+path;
                library[itemCount++]={path,package->name,package->engineCount};
            }
        }catch(const std::exception&){bad++;}
    }
    std::sort(library.begin(),library.begin()+itemCount,[](auto &a,auto &b){return a.name<b.name;});
    status=std::to_string(itemCount)+" package(s), "+std::to_string(bad)+" rejected. R rescans.";
}
static void verifyModule(File& file,const gs::PackageEngine& engine){
    mbedtls_sha256_context sha;mbedtls_sha256_init(&sha);mbedtls_sha256_starts_ret(&sha,0);
    uint8_t block[512],digest[32];uint32_t remaining=engine.moduleSize;
    if(!file.seek(engine.moduleOffset)){mbedtls_sha256_free(&sha);throw std::runtime_error("Cannot seek module");}
    while(remaining){unsigned count=std::min(remaining,uint32_t(sizeof(block)));if(file.read(block,count)!=count){mbedtls_sha256_free(&sha);throw std::runtime_error("SD read failed");}mbedtls_sha256_update_ret(&sha,block,count);remaining-=count;}
    mbedtls_sha256_finish_ret(&sha,digest);mbedtls_sha256_free(&sha);
    if(std::memcmp(digest,engine.digest.data(),32))throw std::runtime_error("Module checksum mismatch");
}
static bool pauseAudio(){
    // Do not mistake a previous pause acknowledgement for the new one.
    auto resumeStart=millis();
    while(pauseAcknowledged.load(std::memory_order_acquire)){
        if(millis()-resumeStart>5000){status="Previous audio resume timed out";return false;}delay(1);
    }
    pauseRequested.store(true,std::memory_order_release);auto start=millis();
    while(!pauseAcknowledged.load(std::memory_order_acquire)){
        if(millis()-start>5000){pauseRequested.store(false);status="Audio pause timeout - no switch";return false;}delay(1);
    }return true;
}
static void loadEngine(unsigned index){
    if(!audioReady){status="Audio unavailable";return;}
    try{
        File file=SD.open(library[selected].path.c_str());if(!file)throw std::runtime_error("Package missing from SD");
        auto candidate=std::make_unique<gs::InstrumentPackage>();readMetadata(file,*candidate);
        if(index>=candidate->engineCount)throw std::runtime_error("Engine index out of range");
        auto& engine=candidate->engines[index];
        if(engine.memory>131072)throw std::runtime_error("ADV experiment needs <=128 KiB modules");
        verifyModule(file,engine); // Failure here preserves the loaded engine.
        silence();if(!pauseAudio())return;
        // One VM at a time: failed allocation returns to the resident Library.
        running.store(nullptr,std::memory_order_release);owner.reset();activePackage.reset();
        readIndex.store(written.load());panicRequested.store(false);
        try{
            const unsigned required=engine.memory+16384+engine.moduleSize+49152;
            if(ESP.getFreeHeap()<required||heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)<engine.memory+256)throw std::runtime_error("Insufficient heap for VM - Library safe");
            std::vector<uint8_t> bytes(engine.moduleSize);
            if(!file.seek(engine.moduleOffset)||file.read(bytes.data(),bytes.size())!=bytes.size())throw std::runtime_error("SD removed while loading");
            auto next=std::make_unique<gs::WasmInstrument>(std::move(bytes),engine.id,rate,19,engine.memory,16384);
            values=engine.defaults;next->parameters(values);
            for(unsigned p=0;p<19;p++)enqueue({1,uint8_t(p),values[p]});
            activePackage=std::move(candidate);owner=std::move(next);engineIndex=index;sceneIndex=knob=0;
            maxUs.store(0);late.store(0);fault.store(0);running.store(owner.get(),std::memory_order_release);view=Instrument;
            status="Loaded from SD / L: Library";
        }catch(const std::exception& error){view=Library;status=error.what();}
        pauseRequested.store(false,std::memory_order_release);
    }catch(const std::exception& error){status=error.what();}
}
static void draw(){
    auto &lcd=canvas;const auto &theme=gs_themes[GS_DEFAULT_THEME];lcd.fillScreen(theme.background);lcd.setTextWrap(false);lcd.setTextSize(1);
    lcd.setTextColor(theme.accent,theme.background);lcd.setCursor(4,3);lcd.print("ESPatcher SD prototype");
    if(view!=Instrument||!activePackage){
        lcd.setTextColor(theme.text,theme.background);lcd.setCursor(4,18);lcd.print(view==Library?"INSTRUMENT LIBRARY":"SELECT ENGINE");
        unsigned count=view==Library?itemCount:library[selected].engines,chosen=view==Library?selected:selectedEngine;
        std::unique_ptr<gs::InstrumentPackage> package;
        if(view==Engines){try{File file=SD.open(library[selected].path.c_str());if(!file)throw std::runtime_error("Package missing");package=std::make_unique<gs::InstrumentPackage>();readMetadata(file,*package);}catch(...){view=Library;status="Package unavailable";}}
        unsigned first=chosen>2?chosen-2:0;
        for(unsigned i=first;i<count&&i<first+5;i++){
            if(i==chosen)lcd.fillRect(2,34+(i-first)*15,236,14,theme.surface);
            lcd.setTextColor(i==chosen?theme.accent:theme.text,theme.background);lcd.setCursor(5,35+(i-first)*15);
            lcd.printf("%c %.32s",i==chosen?'>':' ',view==Library?library[i].name.c_str():package?package->engines[i].name.c_str():"Unavailable");
        }
        lcd.setTextColor(theme.chrome,theme.background);lcd.setCursor(4,112);lcd.printf("%.38s",status.c_str());lcd.setCursor(4,124);lcd.print("Arrows select / Enter load / R scan");
    }else{
        auto &engine=activePackage->engines[engineIndex];auto &scene=engine.scenes[sceneIndex];
        lcd.setTextColor(theme.text,theme.background);lcd.setCursor(4,16);lcd.printf("%.18s / %.17s",scene.title.c_str(),engine.name.c_str());
        lcd.setTextColor(theme.chrome,theme.background);lcd.setCursor(4,27);lcd.printf("%.38s",scene.subtitle.c_str());
        if(scene.layout==0){auto samples=scope.snapshot();float amplitude=.02f;for(auto value:samples)amplitude=std::max(amplitude,std::abs(value));for(unsigned i=1;i<128;i++)lcd.drawLine(4+(i-1)*230/127,52-int(samples[i-1]/amplitude*15),4+i*230/127,52-int(samples[i]/amplitude*15),theme.accent);}
        if(scene.layout==1){float depth=values[7];unsigned shape=unsigned(values[18]);for(unsigned i=1;i<128;i++)lcd.drawLine(4+(i-1)*230/127,52-int(gs_lfo_wave((i-1)/128.f,shape,0)*depth*15),4+i*230/127,52-int(gs_lfo_wave(i/128.f,shape,0)*depth*15),theme.accent);lcd.drawFastVLine(4+(phase.load()>>16)*230/65536,36,32,theme.alternate);}
        if(scene.layout==3){for(unsigned i=0;i<8;i++){unsigned p=scene.bindings[i];if(p==255)continue;float fraction=p==3?std::log(values[p]/80)/std::log(200.f):std::min(1.f,std::max(0.f,values[p]/(p==14?300.f:p==17||p==15?8.f:p==0?2.f:1.f)));lcd.fillRect(5+i*29,67-int(fraction*30),22,int(fraction*30),theme.accent);}}
        for(unsigned i=0;i<8;i++){
            auto p=scene.bindings[i];if(p==17&&values[16]<.5f)p=6;
            int x=3+(i%4)*60,y=(scene.layout==2?44:75)+(i/4)*25;
            lcd.setTextColor(i==knob?theme.accent:theme.chrome,theme.background);lcd.setCursor(x,y);lcd.printf("%u %.8s",i+1,scene.labels[i].c_str());
            lcd.setTextColor(theme.text,theme.background);lcd.setCursor(x,y+11);
            if(p==255)lcd.print("--");else if(p==1||p==2)lcd.printf("%.0fms",values[p]*1000);else if(p==3)lcd.printf("%.1fk",values[p]/1000);else if(p==15||p==17){const char* division[]={"1/1","1/2","1/4","1/8","1/8T","1/16","1/16T","1/32","1/32T"};lcd.print(division[int(values[p])]);}else lcd.printf("%.2g",values[p]);
        }
        lcd.setTextColor(fault.load()?TFT_RED:theme.chrome,theme.background);lcd.setCursor(4,125);lcd.printf("%lu/4 %luus late:%lu L Library",(unsigned long)voices.load(),(unsigned long)maxUs.load(),(unsigned long)late.load());
        if(fault.load()){lcd.fillRect(0,112,240,11,TFT_RED);lcd.setCursor(4,113);lcd.setTextColor(TFT_WHITE,TFT_RED);lcd.print("Engine halted - L returns Library");}
    }canvas.pushSprite(0,0);
}
void setup(){
    Serial.begin(115200);held.fill(-1);
    auto config=M5.config();config.internal_mic=false;config.internal_spk=true;config.fallback_board=m5::board_t::board_M5CardputerADV;
    M5Cardputer.begin(config,true);M5Cardputer.Display.setRotation(1);
    if(M5.getBoard()!=m5::board_t::board_M5CardputerADV){fatal=true;M5Cardputer.Display.println("ADV required");return;}
    M5Cardputer.Mic.end();M5Cardputer.Speaker.end();
    auto speaker=M5Cardputer.Speaker.config();speaker.sample_rate=rate;speaker.dma_buf_len=128;speaker.dma_buf_count=4;speaker.task_pinned_core=1;speaker.task_priority=5;
    M5Cardputer.Speaker.config(speaker);M5Cardputer.Speaker.setBufferReleaseCallback(nullptr,release);
    canvas.setColorDepth(8);if(!canvas.createSprite(240,135)){fatal=true;M5Cardputer.Display.println("Display allocation failed");return;}
    if(!M5Cardputer.Speaker.begin()){fatal=true;status="Audio startup failed";draw();return;}
    M5Cardputer.Speaker.setVolume(160);
    audioReady=xTaskCreatePinnedToCore(audioTask,"sd-audio",24576,nullptr,4,&audioHandle,1)==pdPASS;
    if(!audioReady){status="Audio task allocation failed";draw();return;}
    mountSD();scanLibrary();draw();
    Serial.printf("ESPatcher SD prototype: heap %u largest %u. No flash writes.\n",ESP.getFreeHeap(),heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}
void loop(){
    if(fatal){delay(100);return;}M5Cardputer.update();auto &keys=M5Cardputer.Keyboard.keysState();
    bool l=key('l')||M5Cardputer.BtnA.isPressed(),enter=keys.enter,escape=keys.esc||keys.backspace;
    bool up=keys.up||key(';'),down=keys.down||key('.'),left=keys.left||key(','),right=keys.right||key('/'),z=key('z'),x=key('x'),space=keys.space,r=key('r'),minus=key('-');
    if(l&&!previousL){silence();view=Library;}
    if(escape&&!previousEscape){silence();view=view==Library&&activePackage?Instrument:Library;}
    if(view!=Instrument){
        unsigned count=view==Library?itemCount:library[selected].engines;unsigned &choice=view==Library?selected:selectedEngine;
        if(count&&up&&!previousUp)choice=(choice+count-1)%count;
        if(count&&down&&!previousDown)choice=(choice+1)%count;
        if(r&&!previousR){silence();view=Library;sdReady=false;SD.end();scanLibrary();}
        if(enter&&!previousEnter&&count){if(view==Library&&library[selected].engines>1){view=Engines;selectedEngine=0;}else loadEngine(view==Library?0:selectedEngine);}
    }else if(activePackage){
        const char pageKeys[]={'c','v','b','n'};auto &engine=activePackage->engines[engineIndex];
        for(unsigned i=0;i<engine.sceneCount;i++)if(key(pageKeys[i]))sceneIndex=i;
        unsigned numbers=0;for(unsigned i=0;i<8;i++)if(key(char('1'+i)))numbers|=1u<<i;
        for(unsigned i=0;i<8;i++)if(numbers&(1u<<i)){knob=i;break;}
        auto &scene=engine.scenes[sceneIndex];unsigned address=scene.bindings[knob];if(address==17&&values[16]<.5f)address=6;
        if(address!=255&&((up&&!previousUp)||(right&&!previousRight)||(down&&!previousDown)||(left&&!previousLeft))){
            values[address]=gs::editParameter(address,values[address],(up||right?1:-1)*(keys.shift?.002f:.02f),rate*.45f);enqueue({1,uint8_t(address),values[address]});
        }
        if((z&&!previousZ)||(x&&!previousX)){silence();octave=std::clamp(octave+(x?1:-1),1,7);}
        if(space&&!previousSpace)silence();
        if(minus&&!previousMinus){values[8]=values[8]>=.5f?0:1;enqueue({1,8,values[8]});}
        for(unsigned i=0;i<13;i++){
            bool pressed=key(noteKeys[i]);
            if(!pressed){blocked[i]=false;if(held[i]>=0){int note=held[i];held[i]=-1;if(std::find(held.begin(),held.end(),note)==held.end())enqueue({0,uint8_t(note),0});}}
            else if(!blocked[i]&&held[i]<0){unsigned pad=(3-i/4)*4+i%4,note=12*(octave+1)+scene.pads[pad];if(note<=127){bool fresh=std::find(held.begin(),held.end(),int(note))==held.end();held[i]=int(note);if(fresh)enqueue({0,uint8_t(note),100});}}
        }
        previousNumbers=numbers;
    }
    previousL=l;previousEnter=enter;previousEscape=escape;previousUp=up;previousDown=down;previousLeft=left;previousRight=right;previousZ=z;previousX=x;previousSpace=space;previousR=r;previousMinus=minus;
    static uint32_t last=0,report=0;if(millis()-last>=100){last=millis();draw();}
    if(Serial&&millis()-report>2000){report=millis();Serial.printf("heap=%u largest=%u dsp_max_us=%lu late=%lu fault=%lu ui_stack=%u audio_stack=%u\n",ESP.getFreeHeap(),heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),(unsigned long)maxUs.load(),(unsigned long)late.load(),(unsigned long)fault.load(),uxTaskGetStackHighWaterMark(nullptr),audioHandle?uxTaskGetStackHighWaterMark(audioHandle):0);}
    delay(2);
}
