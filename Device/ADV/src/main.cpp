#include <Arduino.h>
#include <Preferences.h>
#include <M5Cardputer.h>
#include <atomic>
#include <cmath>
#include "ControlState.hpp"
#include "../../../Core/Scope.hpp"
#include "../../../Firmware/lfo.h"

extern "C" {
uint32_t analog_lfo_status(void);uint32_t analog_arp_status(void);uint32_t analog_voice_limit(void);uint32_t analog_active_voices(void);void analog_init(uint32_t);void analog_note(uint32_t,uint32_t);void analog_parameter(uint32_t,float);void analog_panic(void);float *analog_render(uint32_t);
uint32_t bell_lfo_status(void);uint32_t bell_arp_status(void);uint32_t bell_voice_limit(void);uint32_t bell_active_voices(void);void bell_init(uint32_t);void bell_note(uint32_t,uint32_t);void bell_parameter(uint32_t,float);void bell_panic(void);float *bell_render(uint32_t);
}
struct Instrument {
    void (*init)(uint32_t);void (*note)(uint32_t,uint32_t);void (*parameter)(uint32_t,float);void (*panic)();float *(*render)(uint32_t);uint32_t (*activeVoices)();uint32_t (*arpStatus)();uint32_t (*lfoStatus)();
};
static const Instrument instruments[]={
    {analog_init,analog_note,analog_parameter,analog_panic,analog_render,analog_active_voices,analog_arp_status,analog_lfo_status},
    {bell_init,bell_note,bell_parameter,bell_panic,bell_render,bell_active_voices,bell_arp_status,bell_lfo_status}
};
static_assert(ARDUINO_RUNNING_CORE==0,"UI must run on core 0");
static_assert(std::atomic<uint32_t>::is_always_lock_free,"Control indices must be lock free");
static constexpr unsigned queueSize=64,bufferCount=3;
static adv::Event events[queueSize];
static std::atomic<uint32_t> written{0},readIndex{0},overflows{0},forcePanic{0},resync{0};
static std::atomic<uint32_t> renderMaxUs{0},lateBlocks{0},starvations{0},audioError{0},voiceCount{0},arpStatus{0},lfoStatus{0};
static int16_t pcm[bufferCount][adv::blockFrames];
static std::atomic<uint32_t> bufferBusy[bufferCount]{};
static adv::Controls controls;
static Preferences preferences;
static bool themeSavePending=false;
static gs::Scope scope;
static M5Canvas screen(&M5Cardputer.Display);
static bool failed=false,dirty=true;
static uint32_t lastFrame=0,lastReport=0;

// UI thread is the only producer; DSP task is the only consumer.
static void enqueue(adv::Event e){
    auto w=written.load(std::memory_order_relaxed);
    if(w-readIndex.load(std::memory_order_acquire)>=queueSize){overflows.fetch_add(1);forcePanic.store(1);return;}
    events[w%queueSize]=e;written.store(w+1,std::memory_order_release);
}
// Called by M5Unified's playback task when a buffer is no longer referenced.
static void releaseBuffer(void*,const void* data,uint8_t){
    for(unsigned i=0;i<bufferCount;i++)if(data==pcm[i]){bufferBusy[i].store(0,std::memory_order_release);break;}
}
static void audioTask(void*){
    int active=0;unsigned fadeRemaining=0,buffer=0;
    float fadeFrom=0,lastSample=0;
    float scopeBlock[adv::blockFrames]{};
    std::array<float,19> params{{1,.015f,.25f,5000,.2f,0,1,0,0,0,4,.5f,1,0,120,3,1,1,1}};
    for(auto& synth:instruments){synth.init(adv::sampleRate);for(unsigned p=0;p<19;p++)synth.parameter(p,params[p]);}
    for(;;){
        // Backpressure: never overwrite memory still owned by the playback task.
        if(bufferBusy[buffer].load(std::memory_order_acquire)||M5Cardputer.Speaker.isPlaying(0)>=2){vTaskDelay(1);continue;}
        auto start=micros();
        if(forcePanic.exchange(0)){
            for(auto& synth:instruments)synth.panic();readIndex.store(written.load(std::memory_order_acquire),std::memory_order_release);resync.store(1);
        }
        auto r=readIndex.load(std::memory_order_relaxed),w=written.load(std::memory_order_acquire);
        unsigned consumed=0;
        while(r!=w&&consumed++<queueSize){
            auto e=events[r++%queueSize];
            switch(e.kind){
                case adv::Kind::Note:instruments[active].note(e.index,unsigned(e.value));break;
                case adv::Kind::Parameter:if(e.index<19){params[e.index]=e.value;instruments[active].parameter(e.index,e.value);}break;
                case adv::Kind::Panic:instruments[active].panic();break;
                case adv::Kind::Instrument:
                    if(e.index<2){instruments[active].panic();active=e.index;instruments[active].init(adv::sampleRate);for(unsigned p=0;p<19;p++)instruments[active].parameter(p,params[p]);fadeRemaining=128;fadeFrom=lastSample;}
                    break;
            }
        }
        readIndex.store(r,std::memory_order_release);
        float *samples=instruments[active].render(adv::blockFrames);
        voiceCount.store(instruments[active].activeVoices());arpStatus.store(instruments[active].arpStatus());lfoStatus.store(instruments[active].lfoStatus());
        for(unsigned i=0;i<adv::blockFrames;i++){
            float sample=samples&&std::isfinite(samples[i])?std::clamp(samples[i],-.9f,.9f):0;
            if(fadeRemaining){float old=fadeRemaining/128.f;sample=sample*(1-old)+fadeFrom*old;--fadeRemaining;}
            pcm[buffer][i]=int16_t(sample*32767.f);lastSample=sample;scopeBlock[i]=sample;
        }
        scope.push(scopeBlock,adv::blockFrames);
        uint32_t elapsed=micros()-start;
        if(elapsed>renderMaxUs.load())renderMaxUs.store(elapsed);
        if(elapsed>adv::blockFrames*1000000u/adv::sampleRate)lateBlocks.fetch_add(1);
        if(!M5Cardputer.Speaker.isPlaying(0))starvations.fetch_add(1);
        bufferBusy[buffer].store(1,std::memory_order_release);
        if(!M5Cardputer.Speaker.playRaw(pcm[buffer],adv::blockFrames,adv::sampleRate,false,1,0,false)){
            bufferBusy[buffer].store(0,std::memory_order_release);audioError.store(1);
            for(auto& synth:instruments)synth.panic();vTaskDelay(pdMS_TO_TICKS(5));
        }
        buffer=(buffer+1)%bufferCount;
    }
}
static bool key(char c){return M5Cardputer.Keyboard.isKeyPressed(c)||M5Cardputer.Keyboard.isKeyPressed(char(c>='a'&&c<='z'?c-'a'+'A':c));}
static adv::Input input(){
    adv::Input state;
    for(unsigned i=0;i<13;i++)state.notes[i]=key(adv::noteKeys[i]);
    auto& keys=M5Cardputer.Keyboard.keysState();
    state.octaveDown=key('z');state.octaveUp=key('x');
    auto &nav=state.navigation;
    const char pageKeys[]={'c','v','b','n','m'};
    for(int page=0;page<5;page++)if(key(pageKeys[page]))nav.pageKey=page;
    bool functions[]={keys.f1,keys.f2,keys.f3,keys.f4,keys.f5,keys.f6,keys.f7,keys.f8};
    for(unsigned slot=0;slot<8;slot++)if(key(char('1'+slot))||functions[slot])nav.numbers|=1u<<slot;
    nav.fn=keys.fn;nav.fine=keys.shift;nav.reverse=keys.shift;
    nav.tab=keys.tab;nav.enter=keys.enter;nav.escape=keys.esc||keys.backspace||key('`');
    nav.left=keys.left||key(',')||key('<');nav.right=keys.right||key('/')||key('?');
    nav.up=keys.up||key(';')||key(':');nav.down=keys.down||key('.')||key('>');
    state.play=key('-')||key('_');
    state.panic=keys.space||M5Cardputer.BtnA.isPressed();state.diagnostics=keys.fn&&keys.f8;
    return state;
}
static void draw(){
    const auto& theme=gs_themes[controls.theme];
    auto& lcd=screen;
    lcd.startWrite();lcd.fillScreen(theme.background);lcd.setTextWrap(false);lcd.setTextSize(1);lcd.setTextColor(theme.accent,theme.background);
    lcd.setCursor(5,4);static const char* pages[]={"HOME","GLOBAL","OSC","LFO","ARP"};
    lcd.printf("%s / %s",pages[int(controls.nav.page)],controls.instrument?"FM BELL":"ANALOG");
    lcd.setTextColor(theme.alternate,theme.background);lcd.setCursor(178,4);lcd.printf("%lu/4 O%d",(unsigned long)voiceCount.load(),controls.octave);
    if(controls.nav.page==gs::Page::Global){
        auto &nav=controls.nav;
        lcd.setTextColor(theme.accent,theme.background);lcd.setCursor(5,24);
        lcd.print(nav.section==0?"GLOBAL":nav.section==1?"INSTRUMENTS":nav.section==2?"THEMES / PREVIEW":"GLOBAL / TEMPO");
        if(nav.section==3){
            lcd.setCursor(5,25);lcd.print("GLOBAL / TEMPO");lcd.setTextSize(3);lcd.setCursor(10,55);lcd.printf("%.1f",controls.parameters[14]);
            lcd.setTextSize(1);lcd.print(" BPM");lcd.setCursor(5,105);lcd.print("Arrows 1 BPM / Shift 0.1");lcd.setCursor(5,123);lcd.print("OK back / Esc back");
        }else{
        int count=nav.section==2?GS_THEME_COUNT:nav.section==0?3:2;
        int selected=nav.section==0?nav.menu:nav.section==1?nav.browser:nav.theme;
        int first=std::max(0,std::min(count-4,selected-1));
        for(int i=first;i<std::min(count,first+4);i++){
            int y=44+(i-first)*17;bool active=i==selected;
            if(active)lcd.fillRect(3,y-2,234,14,theme.surface);
            lcd.setTextColor(active?theme.accent:theme.text,active?theme.surface:theme.background);lcd.setCursor(6,y);
            const char *label=nav.section==0?(i==0?"INSTRUMENT":i==1?"THEME":"TEMPO"):nav.section==1?(i==0?"AMSYNTH ANALOG":"AMSYNTH FM"):gs_themes[i].name;
            lcd.printf("%c %s",active?'>':' ',label);
        }
        lcd.setCursor(4,123);lcd.setTextColor(theme.chrome,theme.background);
        lcd.print(nav.section==2?"Arrows preview OK save Esc back":"Arrows select OK open Esc home");
        }
    }else if(controls.diagnostics){
        lcd.setCursor(5,40);lcd.printf("DSP max %lu / 8000 us",(unsigned long)renderMaxUs.load());
        lcd.setCursor(5,54);lcd.printf("Late %lu  Q gap %lu",(unsigned long)lateBlocks.load(),(unsigned long)starvations.load());
        lcd.setCursor(5,68);lcd.printf("Voices %lu/4  Heap %u",(unsigned long)voiceCount.load(),ESP.getFreeHeap());
        lcd.setCursor(5,82);lcd.printf("Queue overflow %lu",(unsigned long)overflows.load());
        lcd.setCursor(5,98);lcd.print("Hold A D G J: 4-note test");
    }else{
        if(controls.nav.page==gs::Page::LFO){
            static float history[96]{};static unsigned write=0,lastShape=99;
            unsigned shape=unsigned(controls.parameters[18]+.5f);uint32_t status=lfoStatus.load();
            float phase=(status>>16)/65536.f,raw=int16_t(status&65535)/32767.f,depth=controls.parameters[7];
            if(shape!=lastShape){for(float &v:history)v=0;write=0;lastShape=shape;}
            history[write++%96]=raw;
            lcd.setCursor(5,16);lcd.setTextColor(theme.chrome,theme.background);lcd.print(shape==4?"S&H HISTORY / DEPTH":"AUDIO PHASE + DEPTH");
            lcd.drawFastHLine(5,43,230,theme.surface);
            int previous=43,refPrevious=43;
            for(unsigned i=0;i<128;i++){
                float value=shape==4?history[(write+i*95/127)%96]:gs_lfo_wave(i/128.f,shape,0);
                int x=5+i*230/127,y=43-int(value*depth*17),ref=43-int(value*17);
                if(i){int px=5+(i-1)*230/127;lcd.drawLine(px,refPrevious,x,ref,theme.chrome);lcd.drawLine(px,previous,x,y,theme.accent);}
                previous=y;refPrevious=ref;
            }
            int cursor=shape==4?235:5+int(phase*230);
            lcd.drawFastVLine(cursor,25,38,theme.alternate);lcd.fillRect(cursor-1,42-int(raw*depth*17),3,3,theme.text);
        }else{
        auto samples=scope.snapshot();float amplitude=.02f;
        for(float sample:samples)amplitude=std::max(amplitude,std::abs(sample));
        lcd.setTextColor(theme.chrome,theme.background);lcd.setCursor(5,15);
        if(controls.nav.page==gs::Page::Arp){
            uint32_t status=arpStatus.load();unsigned step=(status>>8)&0x7fffff;
            lcd.printf("%s %.1f BPM STEP %u",(status&0x80000000)?"PLAY":"STOP",controls.parameters[14],step);
            for(unsigned i=0;i<8;i++)lcd.fillRect(5+i*29,66,25,3,step&&((step-1)%8)==i&&(status&0x80000000)?theme.accent:theme.surface);
        }else lcd.print("OUT / AUTO");
        lcd.drawFastHLine(5,43,230,theme.surface);
        for(int x=5;x<=235;x+=46)lcd.drawFastVLine(x,25,38,theme.surface);
        int px=5,py=43-int(samples[0]/amplitude*17);
        for(unsigned i=1;i<128;i++){
            int x=5+int(i)*230/127,y=43-int(samples[i]/amplitude*17);
            lcd.drawLine(px,py,x,y,theme.accent);px=x;py=y;
        }
        
        }
        static const char* labels[]={"SHAPE","ATTACK","RELEASE","CUTOFF","LEVEL","RES","RATE","DEPTH","PLAY","PATTERN","RATE","GATE","OCTAVES","LATCH","TEMPO","RATE","SYNC","RATE","SHAPE"};
        for(unsigned slot=0;slot<8;slot++){
            int address=gs::Navigation::binding(controls.nav.page,int(slot));
            if(address==17&&controls.parameters[16]<.5f)address=6;
            int x=int(slot%4)*60+3,y=int(slot/4)*29+75;
            if(address<0){lcd.setTextColor(theme.chrome,theme.background);lcd.setCursor(x,y);lcd.printf("%u UNUSED",slot+1);lcd.setCursor(x,y+11);lcd.print("--");continue;}
            unsigned p=unsigned(address);bool selected=int(slot)==controls.nav.selected();
            if(selected)lcd.fillRoundRect(x-2,y-2,58,26,2,theme.surface);
            lcd.setTextColor(selected?theme.accent:theme.chrome,selected?theme.surface:theme.background);
            lcd.setCursor(x,y);lcd.printf("%u %s",slot+1,p==0?(controls.instrument?"RATIO":"WAVE"):labels[p]);
            lcd.setTextColor(theme.text,selected?theme.surface:theme.background);lcd.setCursor(x,y+11);
            float value=controls.parameters[p];
            if(p==0){static const char* analog[]={"SINE","SAW","SQUARE"};static const char* bell[]={"1:1","2:1","3:1"};lcd.print(controls.instrument?bell[int(value)]:analog[int(value)]);}
            else if(p==1||p==2)lcd.printf("%.0fms",value*1000);
            else if(p==8)lcd.print(value>=.5f?"PLAY":"STOP");
            else if(p==9){static const char* patterns[]={"UP","DOWN","UP/DOWN","RANDOM"};lcd.print(patterns[int(value+.5f)]);}
            else if(p==12)lcd.printf("%u OCT",unsigned(value+.5f));
            else if(p==13)lcd.print(value>=.5f?"ON":"OFF");
            else if(p==16)lcd.print(value>=.5f?"SYNC":"FREE");
            else if(p==18){static const char* shapes[]={"SINE","TRI","SQUARE","SAW","S&H"};lcd.print(shapes[int(value+.5f)]);}
            else if(p==15||p==17){static const char *divisions[]={"1/1","1/2","1/4","1/8","1/8T","1/16","1/16T","1/32","1/32T"};lcd.print(divisions[int(value+.5f)]);}
            else if(p==6||p==10)lcd.printf("%.2fHz",value);
            else if(p==3){if(value>=1000)lcd.printf("%.2fkHz",value/1000);else lcd.printf("%.0fHz",value);}
            else lcd.printf("%.0f%%",value*((p==5||p==7||p==11)?100:200));
        }
    }
    if(controls.diagnostics){
    lcd.setTextColor(audioError.load()?TFT_RED:theme.chrome,theme.background);lcd.setCursor(5,113);
    lcd.print(audioError.load()?"AUDIO ERROR - reboot":"C V B N pages  Fn8 info");lcd.setCursor(5,125);lcd.print("Z/X octave  Space panic");
    }
    if(audioError.load()){lcd.fillRect(0,65,240,10,TFT_RED);lcd.setCursor(5,66);lcd.setTextColor(TFT_WHITE,TFT_RED);lcd.print("AUDIO ERROR - reboot");}
    lcd.endWrite();screen.pushSprite(0,0);
}
static void fatal(const char* message){
    failed=true;M5Cardputer.Display.fillScreen(TFT_BLACK);M5Cardputer.Display.setTextColor(TFT_RED);M5Cardputer.Display.setCursor(4,4);M5Cardputer.Display.println(message);if(Serial)Serial.println(message);
}
void setup(){
    Serial.begin(115200);
    auto cfg=M5.config();cfg.internal_mic=false;cfg.internal_spk=true;cfg.clear_display=true;cfg.fallback_board=m5::board_t::board_M5CardputerADV;
    M5Cardputer.begin(cfg,true);M5Cardputer.Display.setRotation(1);M5Cardputer.Display.setBrightness(100);
    if(M5.getBoard()!=m5::board_t::board_M5CardputerADV){fatal("ADV not detected.\nDo not use original Cardputer.");return;}
    M5Cardputer.Mic.end();M5Cardputer.Speaker.end();
    auto speaker=M5Cardputer.Speaker.config();speaker.sample_rate=adv::sampleRate;speaker.dma_buf_len=128;speaker.dma_buf_count=4;speaker.task_priority=5;speaker.task_pinned_core=1;
    M5Cardputer.Speaker.config(speaker);M5Cardputer.Speaker.setBufferReleaseCallback(nullptr,releaseBuffer);
    if(!M5Cardputer.Speaker.begin()){fatal("Audio startup failed.");return;}
    preferences.begin("gs-loader",false);
    unsigned savedTheme=preferences.getUInt("theme",GS_DEFAULT_THEME);
    controls.theme=savedTheme<GS_THEME_COUNT?int(savedTheme):GS_DEFAULT_THEME;
    controls.nav.theme=controls.nav.savedTheme=controls.nav.themeBefore=controls.theme;
    if(!screen.createSprite(240,135)){fatal("Display allocation failed");return;}
    M5Cardputer.Speaker.setVolume(160);
    BaseType_t created=xTaskCreatePinnedToCore(audioTask,"gs-audio",8192,nullptr,4,nullptr,1);
    if(created!=pdPASS){M5Cardputer.Speaker.end();fatal("Audio task allocation failed.");return;}
    if(Serial)Serial.println("AMSynth ADV 0.10.1 native / 4 voices / 32000 Hz / 256 frames / UI core 0, audio core 1");draw();
}
void loop(){
    if(failed){delay(100);return;}
    M5Cardputer.update();
    auto before=controls;controls.scan(input(),millis(),enqueue);
    if(controls.nav.saveTheme)themeSavePending=true;
    if(themeSavePending&&!voiceCount.load()&&!controls.testChord&&controls.parameters[8]<.5f){
        bool held=false;for(int note:controls.midi)held=held||note>=0;
        if(!held){preferences.putUInt("theme",controls.nav.savedTheme);themeSavePending=false;}
    }
    dirty=dirty||before.parameters!=controls.parameters||before.selected!=controls.selected||before.instrument!=controls.instrument||before.octave!=controls.octave||before.diagnostics!=controls.diagnostics||before.testChord!=controls.testChord||before.theme!=controls.theme||audioError.load();
    if(resync.exchange(0)){
        if(written.load()-readIndex.load()<queueSize-20){
            enqueue({adv::Kind::Instrument,uint8_t(controls.instrument),0});
            for(unsigned p=0;p<19;p++)enqueue({adv::Kind::Parameter,uint8_t(p),controls.parameters[p]});
        }else resync.store(1);
    }
    if((!controls.diagnostics&&millis()-lastFrame>=50)||(dirty&&millis()-lastFrame>=50)||(controls.diagnostics&&millis()-lastFrame>=500)){lastFrame=millis();draw();dirty=false;}
    if(Serial&&millis()-lastReport>=2000){lastReport=millis();Serial.printf("heap=%u dsp_max_us=%lu late=%lu q_gap=%lu overflow=%lu audio_error=%lu voices=%lu/4\n",ESP.getFreeHeap(),(unsigned long)renderMaxUs.load(),(unsigned long)lateBlocks.load(),(unsigned long)starvations.load(),(unsigned long)overflows.load(),(unsigned long)audioError.load(),(unsigned long)voiceCount.load());}
    delay(2);
}
