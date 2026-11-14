#import "SynthAudioUnit.h"
#include "../../Core/Synth.hpp"
#include "../../Core/Scope.hpp"
#include "../../Core/WasmInstrument.hpp"
#include "../../Core/MIDIMap.hpp"
#include "../../Core/InstrumentPackage.hpp"
#import <CommonCrypto/CommonDigest.h>
#include <mutex>
#include <atomic>
#include <memory>
#include <vector>

namespace {
constexpr int parameterCount=19, queueSize=256;
static_assert(std::atomic<float>::is_always_lock_free);
static_assert(std::atomic<unsigned>::is_always_lock_free);
static_assert(std::atomic<uint64_t>::is_always_lock_free);
struct Event { uint8_t note,velocity; };
struct Kernel {
    gs::Synth core;
    gs::Scope scope;
    gs::MIDIMap midi;
    std::atomic<uint32_t> midiRevision{0};
    struct MIDIEvent {uint8_t status,a,b;};
    MIDIEvent midiQueue[256]{};
    std::atomic<unsigned> midiRead{0},midiWrite{0};
    std::atomic<bool> midiOverflow{false};
    std::atomic<uint64_t> midiPanicCutoff{0};
    std::mutex midiProducer; // Input callbacks only; render never takes this lock.
    void enqueueMIDI(uint8_t status,uint8_t a,uint8_t b){
        std::lock_guard<std::mutex> lock(midiProducer);
        auto w=midiWrite.load();if(w-midiRead.load()>=256){midiOverflow.store(true);return;}
        midiQueue[w%256]={status,a,b};midiWrite.store(w+1,std::memory_order_release);
    }
    void receiveMIDI(uint8_t status,uint8_t a,uint8_t b){
        if(a>127||b>127)return;
        if((status&0xf0)==0x90&&b)noteOn(a,b);
        else if((status&0xf0)==0x80||((status&0xf0)==0x90&&!b))noteOff(a);
        else if((status&0xf0)==0xb0){
            if(a==120||a==123){panic();return;}
            unsigned address;float value;
            if(midi.receive(status&15,a,b,address,value)){parameter(address,value);midiRevision.fetch_add(1);}
        }else if(status==0xfa||status==0xfb){parameter(8,1);midiRevision.fetch_add(1);}
        else if(status==0xfc){parameter(8,0);midiRevision.fetch_add(1);}
    }
    std::unique_ptr<gs::WasmInstrument> modules[2];
    std::atomic<int> active{-1}, pending{-1};
    unsigned discardBefore[2]{};
    std::atomic<bool> firmwareFailed{false};
    std::atomic<int> voicesActive{-1};
    std::atomic<uint32_t> arpState{0},lfoState{0};
    std::mutex control;
    unsigned fadeRemaining=0;
    float lastSample=0,fadeFrom=0;
    void noteOn(int note,int vel){int i=active.load(std::memory_order_relaxed);if(i>=0){modules[i]->parameters({values[0].load(),values[1].load(),values[2].load(),values[3].load(),values[4].load(),values[5].load(),values[6].load(),values[7].load(),values[8].load(),values[9].load(),values[10].load(),values[11].load(),values[12].load(),values[13].load(),values[14].load(),values[15].load(),values[16].load(),values[17].load(),values[18].load()});modules[i]->noteOn(note,vel);}else core.noteOn(note,vel);}
    void noteOff(int note){int i=active.load(std::memory_order_relaxed);if(i>=0)modules[i]->noteOff(note);else core.noteOff(note);}
    void panic(){int i=active.load(std::memory_order_relaxed);if(i>=0)modules[i]->panic();else core.panic();}
    std::atomic<float> values[parameterCount]{{1},{.015f},{.25f},{5000},{.2f},{0},{1},{0},{0},{0},{4},{.5f},{1},{0},{120},{3},{1},{1},{1}};
    Event queue[queueSize]{};
    std::atomic<unsigned> read{0},write{0};
    std::atomic<uint64_t> panicRequest{0};
    std::vector<float> left,right; // Resized only in allocateRenderResources.
    void enqueue(Event event){ // Single producer: editor main thread only.
        unsigned w=write.load(std::memory_order_relaxed);
        if(w-read.load(std::memory_order_acquire)>=queueSize){panicRequest.store((uint64_t(1)<<32)|w,std::memory_order_release);return;}
        queue[w%queueSize]=event;write.store(w+1,std::memory_order_release);
    }
    void parameter(unsigned address,float value){
        if(address>=parameterCount||!std::isfinite(value))return;
        static const float lo[]={0,.005f,.01f,80,0,0,.05f,0,0,0,.25f,.05f,1,0,40,0,0,0,0},hi[]={2,2,3,16000,.5f,1,20,1,1,3,20,1,4,1,300,8,1,8,4};
        values[address].store(std::clamp(value,lo[address],hi[address]),std::memory_order_relaxed);
    }
    void begin(){
        int next=pending.load(std::memory_order_acquire);
        if(next>=0){
            core.panic();fadeRemaining=128;fadeFrom=lastSample;
            // Publish active before clearing pending: the control thread may only
            // reclaim the old slot after observing pending == -1 (acquire).
            active.store(next,std::memory_order_release);
            unsigned r=read.load(std::memory_order_relaxed),cutoff=discardBefore[next];
            if(cutoff-r<=queueSize)read.store(cutoff,std::memory_order_release);
            firmwareFailed.store(false);pending.store(-1,std::memory_order_release);
        }
        if(uint64_t request=panicRequest.exchange(0,std::memory_order_acquire)){
            panic();unsigned r=read.load(std::memory_order_relaxed),cutoff=unsigned(request);
            if(cutoff-r<=queueSize)read.store(cutoff,std::memory_order_release);
        }
        auto mw=midiWrite.load(std::memory_order_acquire),mr=midiRead.load();
        if(uint64_t cutoff=midiPanicCutoff.exchange(0)){
            panic();if(unsigned(cutoff)-mr<=256)mr=unsigned(cutoff);
        }
        if(midiOverflow.exchange(false)){panic();mr=mw;}
        while(mr!=mw){auto e=midiQueue[mr++%256];receiveMIDI(e.status,e.a,e.b);}
        midiRead.store(mr,std::memory_order_release);
        unsigned r=read.load(std::memory_order_relaxed),w=write.load(std::memory_order_acquire);
        while(r!=w){auto e=queue[r++%queueSize];if(e.velocity)noteOn(e.note,e.velocity);else noteOff(e.note);}
        read.store(r,std::memory_order_release);
    }
    void render(unsigned offset,unsigned count){
        core.params={int(std::round(values[0].load())),values[1].load(),values[2].load(),values[3].load(),values[4].load()};
        int index=active.load(std::memory_order_relaxed);
        if(index>=0){
            auto *module=modules[index].get();module->parameters({values[0].load(),values[1].load(),values[2].load(),values[3].load(),values[4].load(),values[5].load(),values[6].load(),values[7].load(),values[8].load(),values[9].load(),values[10].load(),values[11].load(),values[12].load(),values[13].load(),values[14].load(),values[15].load(),values[16].load(),values[17].load(),values[18].load()});
            module->render(left.data()+offset,right.data()+offset,count);firmwareFailed.store(module->faulted());voicesActive.store(module->voicesActive());arpState.store(module->arpStatus());lfoState.store(module->lfoStatus());
        }else core.render(left.data()+offset,right.data()+offset,count);
        for(unsigned i=offset;i<offset+count;i++){
            if(fadeRemaining){float old=fadeRemaining/128.f;left[i]=left[i]*(1-old)+fadeFrom*old;right[i]=left[i];--fadeRemaining;}
            lastSample=left[i];
        }
    }
};
}
@interface GSSynthAudioUnit ()
@property(nonatomic,readwrite) AUParameterTree *parameterTree;
- (BOOL)loadEngineData:(NSData *)data memory:(unsigned)memory defaults:(NSArray *)defaults package:(NSData *)package info:(NSDictionary *)info error:(NSError **)error;
@end
@implementation GSSynthAudioUnit {
    AUAudioUnitBusArray *_outputs;
    AUAudioUnitBus *_bus;
    std::unique_ptr<Kernel> _kernel;
    NSData *_firmwareData;
    NSData *_packageData;
    NSDictionary *_packageInfo;
    uint32_t _instrumentRevision;
    NSString *_firmwareName;
    NSString *_firmwareID;
    NSInteger _firmwareVoiceLimit;
    BOOL _firmwareHasResonance;
    BOOL _firmwareHasLFO;
    BOOL _firmwareHasArp;
    BOOL _firmwareHasTempoSync;
    BOOL _firmwareHasLFOShapes;
    NSArray<NSString *> *_firmwareKnobLabels;
}
@synthesize parameterTree=_parameterTree;
- (instancetype)initWithComponentDescription:(AudioComponentDescription)description options:(AudioComponentInstantiationOptions)options error:(NSError **)error {
    self=[super initWithComponentDescription:description options:options error:error];if(!self)return nil;
    _kernel=std::make_unique<Kernel>();
    AVAudioFormat *format=[[AVAudioFormat alloc] initStandardFormatWithSampleRate:48000 channels:2];
    _bus=[[AUAudioUnitBus alloc] initWithFormat:format error:error];if(!_bus)return nil;
    _outputs=[[AUAudioUnitBusArray alloc] initWithAudioUnit:self busType:AUAudioUnitBusTypeOutput busses:@[_bus]];
    NSMutableArray *parameters=[NSMutableArray array];
    NSArray *ids=@[@"wave",@"attack",@"release",@"cutoff",@"volume",@"resonance",@"lfoRate",@"lfoDepth",@"arpRunning",@"arpPattern",@"arpRate",@"arpGate",@"arpOctaves",@"arpLatch",@"tempo",@"arpDivision",@"lfoSync",@"lfoDivision",@"lfoShape"];
    NSArray *names=@[@"Shape",@"Attack",@"Release",@"Cutoff",@"Volume",@"Resonance",@"LFO Rate",@"LFO Depth",@"Arp Running",@"Arp Pattern",@"Arp Rate",@"Arp Gate",@"Arp Octaves",@"Arp Latch",@"Tempo",@"Arp Division",@"LFO Sync",@"LFO Division",@"LFO Shape"];
    float lo[]={0,.005f,.01f,80,0,0,.05f,0,0,0,.25f,.05f,1,0,40,0,0,0,0},hi[]={2,2,3,16000,.5f,1,20,1,1,3,20,1,4,1,300,8,1,8,4};
    AudioUnitParameterUnit units[]={kAudioUnitParameterUnit_Indexed,kAudioUnitParameterUnit_Seconds,kAudioUnitParameterUnit_Seconds,kAudioUnitParameterUnit_Hertz,kAudioUnitParameterUnit_LinearGain,kAudioUnitParameterUnit_Generic,kAudioUnitParameterUnit_Hertz,kAudioUnitParameterUnit_Generic,kAudioUnitParameterUnit_Boolean,kAudioUnitParameterUnit_Indexed,kAudioUnitParameterUnit_Hertz,kAudioUnitParameterUnit_Generic,kAudioUnitParameterUnit_Indexed,kAudioUnitParameterUnit_Boolean,kAudioUnitParameterUnit_BPM,kAudioUnitParameterUnit_Indexed,kAudioUnitParameterUnit_Boolean,kAudioUnitParameterUnit_Indexed,kAudioUnitParameterUnit_Indexed};
    for(unsigned i=0;i<parameterCount;i++){
        AUParameter *p=[AUParameterTree createParameterWithIdentifier:ids[i] name:names[i] address:i min:lo[i] max:hi[i] unit:units[i] unitName:nil flags:kAudioUnitParameterFlag_IsReadable|kAudioUnitParameterFlag_IsWritable valueStrings:i==0?@[@"Mode 1",@"Mode 2",@"Mode 3"]:nil dependentParameters:nil];
        p.value=_kernel->values[i].load();[parameters addObject:p];
    }
    _parameterTree=[AUParameterTree createTreeWithChildren:parameters];
    Kernel *k=_kernel.get();
    _parameterTree.implementorValueObserver=^(AUParameter *p,AUValue value){k->parameter(unsigned(p.address),value);};
    _parameterTree.implementorValueProvider=^AUValue(AUParameter *p){return p.address<parameterCount?k->values[p.address].load():0;};
    self.maximumFramesToRender=4096;
    // Hosts may render before ever requesting an editor. Bootstrap on the
    // construction thread, then adopt while no render thread can be active.
    NSURL *initialURL=[[NSBundle bundleForClass:self.class] URLForResource:@"pocket_analog" withExtension:@"gsm5" subdirectory:@"Modules"];
    if(initialURL){
        NSData *initial=[NSData dataWithContentsOfURL:initialURL options:0 error:error];
        if(!initial||![self loadFirmwareData:initial error:error])return nil;
        _kernel->begin();
    }
    return self;
}
- (AUAudioUnitBusArray *)outputBusses{return _outputs;}
- (NSArray<NSNumber *> *)channelCapabilities{return @[@0,@2];}
- (BOOL)canProcessInPlace{return NO;}
- (BOOL)allocateRenderResourcesAndReturnError:(NSError **)error {
    std::lock_guard<std::mutex> lock(_kernel->control);
    AVAudioFormat *f=_bus.format;
    if(f.channelCount!=2||f.commonFormat!=AVAudioPCMFormatFloat32||f.isInterleaved||f.sampleRate<8000||f.sampleRate>192000){if(error)*error=[NSError errorWithDomain:NSOSStatusErrorDomain code:kAudioUnitErr_FormatNotSupported userInfo:nil];return NO;}
    if(![super allocateRenderResourcesAndReturnError:error])return NO;
    _kernel->left.resize(self.maximumFramesToRender);_kernel->right.resize(self.maximumFramesToRender);
    _kernel->core.prepare(f.sampleRate);
    // AU resources are quiescent here, so reset loaded modules for the new rate.
    try{for(auto &module:_kernel->modules)if(module)module->prepare(f.sampleRate);}
    catch(const std::exception& e){if(error)*error=[NSError errorWithDomain:@"GlitchSynthFirmware" code:2 userInfo:@{NSLocalizedDescriptionKey:@(e.what())}];[super deallocateRenderResources];return NO;}
    return YES;
}
- (void)playNote:(uint8_t)note velocity:(uint8_t)velocity{_kernel->enqueue({note,velocity});}
- (void)releaseNote:(uint8_t)note{_kernel->enqueue({note,0});}
- (void)panic{_kernel->midiPanicCutoff.store((uint64_t(1)<<32)|_kernel->midiWrite.load(std::memory_order_acquire));_kernel->panicRequest.store((uint64_t(1)<<32)|_kernel->write.load(std::memory_order_acquire),std::memory_order_release);}
- (NSArray<NSNumber *> *)scopeSnapshot{
    auto samples=_kernel->scope.snapshot();
    NSMutableArray *values=[NSMutableArray arrayWithCapacity:128];
    for(float sample:samples)[values addObject:@(sample)];
    return values;
}
- (NSArray<NSString *> *)firmwareKnobLabels{std::lock_guard<std::mutex> lock(_kernel->control);return _firmwareKnobLabels?:@[@"CUTOFF",@"RES",@"WAVE",@"ATTACK",@"RELEASE",@"LEVEL",@"RATE",@"DEPTH"];}
- (AUScheduleMIDIEventBlock)scheduleMIDIEventBlock {
    Kernel *k=_kernel.get();
    return ^(AUEventSampleTime time,uint8_t cable,NSInteger length,const uint8_t *data){
        if(length<1||!data)return;
        uint8_t status=data[0];
        if((status&0xf0)<0xf0&&length<3)return;
        k->enqueueMIDI(status,length>1?data[1]:0,length>2?data[2]:0);
    };
}
- (void)beginMIDILearn:(uint32_t)address{_kernel->midi.learning.store(address<19?int(address):-1);}
- (void)refreshMIDIParameters{
    for(unsigned address=0;address<parameterCount;address++){
        AUParameter *p=[_parameterTree parameterWithAddress:address];float value=_kernel->values[address].load();
        if(p.value!=value)p.value=value;
    }
}
- (void)cancelMIDILearn{_kernel->midi.learning.store(-1);}
- (NSInteger)midiLearnTarget{return _kernel->midi.learning.load();}
- (uint32_t)midiRevision{return _kernel->midiRevision.load();}
- (uint32_t)midiMappingRevision{return _kernel->midi.revision.load();}
- (NSArray<NSNumber *> *)midiMappings{NSMutableArray *maps=[NSMutableArray array];for(auto &entry:_kernel->midi.entries)[maps addObject:@(entry.load())];return maps;}
- (void)restoreMIDIMappings:(NSArray<NSNumber *> *)maps{
    for(auto &entry:_kernel->midi.entries)entry.store(0);
    if([maps isKindOfClass:NSArray.class]&&maps.count==gs::MIDIMap::count){
        for(unsigned i=0;i<gs::MIDIMap::count;i++)if([maps[i] isKindOfClass:NSNumber.class]){
            double number=[maps[i] doubleValue];
            if(!std::isfinite(number)||number<0||number>65535||std::floor(number)!=number)continue;
            uint32_t value=uint32_t(number);if(!value||!gs::MIDIMap::valid(value))continue;
            bool duplicate=false;
            for(auto &entry:_kernel->midi.entries){auto e=entry.load();if(e&&((e>>11)==(value>>11)||(e&2047)==(value&2047)))duplicate=true;}
            if(!duplicate)_kernel->midi.entries[i].store(value);
        }
    }
    _kernel->midi.learning.store(-1);_kernel->midi.revision.fetch_add(1);
}
- (uint32_t)midiMapping:(uint32_t)address{return address<19?_kernel->midi.mapping(address):0;}
- (void)clearMIDIMapping:(uint32_t)address{if(address<19)_kernel->midi.clear(address);}
- (void)reset{[super reset];[self panic];}
- (NSDictionary *)fullState {
    NSMutableDictionary *state=[[super fullState] mutableCopy]?:[NSMutableDictionary dictionary];
    NSMutableArray *params=[NSMutableArray array];for(int i=0;i<parameterCount;i++)[params addObject:@(_kernel->values[i].load())];
    state[@"gs.parameters.v1"]=params;
    NSMutableArray *maps=[NSMutableArray array];for(auto &entry:_kernel->midi.entries)[maps addObject:@(entry.load())];
    state[@"espatcher.midi.v1"]=maps;
    std::lock_guard<std::mutex> lock(_kernel->control);
    if(_packageData){state[@"espatcher.package.v1"]=_packageData;state[@"espatcher.package.engine.v1"]=_packageInfo[@"selectedEngine"];}
    else if(_firmwareData)state[@"gs.firmware.v1"]=_firmwareData;
    return state;
}
- (void)setFullState:(NSDictionary *)state {
    id values=state[@"gs.parameters.v1"];if(![values isKindOfClass:NSArray.class]||([values count]!=5&&[values count]!=6&&[values count]!=8&&[values count]!=14&&[values count]!=16&&[values count]!=parameterCount))return;
    for(id value in values)if(![value isKindOfClass:NSNumber.class]||!std::isfinite([value floatValue]))return;
    id package=state[@"espatcher.package.v1"],module=state[@"gs.firmware.v1"];
    if(package){
        id index=state[@"espatcher.package.engine.v1"];
        if(![package isKindOfClass:NSData.class]||![index isKindOfClass:NSNumber.class]||[index doubleValue]!=[index unsignedIntegerValue]||[index unsignedIntegerValue]>=4||![self loadInstrumentPackageData:package engineIndex:[index unsignedIntegerValue] error:nil])return;
    }else if(module&&(![module isKindOfClass:NSData.class]||![self loadFirmwareData:module error:nil]))return;
    [self restoreMIDIMappings:state[@"espatcher.midi.v1"]?:@[]];
    [super setFullState:state];for(unsigned i=0;i<parameterCount;i++)[_parameterTree parameterWithAddress:i].value=i<[values count]?[values[i] floatValue]:(i==6||i==12?1:i==10?4:i==11?.5f:i==14?120:i==15?3:i>=16?1:0);[self panic];
}

- (NSString *)firmwareName{std::lock_guard<std::mutex> lock(_kernel->control);return _firmwareName?:@"Native voice";}
- (NSString *)firmwareID{std::lock_guard<std::mutex> lock(_kernel->control);return _firmwareID?:@"native";}
- (BOOL)firmwareFaulted{return _kernel->firmwareFailed.load();}
- (uint32_t)firmwareArpStatus{return _kernel->arpState.load();}
- (uint32_t)firmwareLFOStatus{return _kernel->lfoState.load();}
- (BOOL)firmwareHasLFOShapes{std::lock_guard<std::mutex> lock(_kernel->control);return _firmwareHasLFOShapes;}
- (BOOL)firmwareHasTempoSync{std::lock_guard<std::mutex> lock(_kernel->control);return _firmwareHasTempoSync;}
- (BOOL)firmwareHasArp{std::lock_guard<std::mutex> lock(_kernel->control);return _firmwareHasArp;}
- (BOOL)firmwareHasLFO{std::lock_guard<std::mutex> lock(_kernel->control);return _firmwareHasLFO;}
- (BOOL)firmwareHasResonance{std::lock_guard<std::mutex> lock(_kernel->control);return _firmwareHasResonance;}
- (NSInteger)firmwareVoiceLimit{std::lock_guard<std::mutex> lock(_kernel->control);return _firmwareVoiceLimit?:1;}
- (NSInteger)firmwareActiveVoices{return _kernel->voicesActive.load();}
- (BOOL)loadFirmwareData:(NSData *)data error:(NSError **)error {
    return [self loadEngineData:data memory:262144 defaults:nil package:nil info:nil error:error];
}
- (NSDictionary *)instrumentPackageInfo{std::lock_guard<std::mutex> lock(_kernel->control);return _packageInfo;}
- (uint32_t)instrumentRevision{std::lock_guard<std::mutex> lock(_kernel->control);return _instrumentRevision;}
- (BOOL)loadInstrumentPackageData:(NSData *)data engineIndex:(NSUInteger)index error:(NSError **)error {
    try{
        if(data.length>gs::packageLimit)throw std::runtime_error("Package exceeds 4 MiB");
        gs::InstrumentPackage package;
        gs::readInstrumentPackage([&](uint32_t offset,void *target,uint32_t count){if(offset>data.length||count>data.length-offset)return false;memcpy(target,(const uint8_t*)data.bytes+offset,count);return true;},uint32_t(data.length),package);
        if(index>=package.engineCount)throw std::runtime_error("Engine index out of range");
        NSMutableArray *engines=[NSMutableArray array];
        for(unsigned e=0;e<package.engineCount;e++){
            const auto &engine=package.engines[e];uint8_t digest[32];
            CC_SHA256((const uint8_t*)data.bytes+engine.moduleOffset,engine.moduleSize,digest);
            if(memcmp(digest,engine.digest.data(),32))throw std::runtime_error("Engine checksum mismatch");
            NSMutableArray *scenes=[NSMutableArray array],*defaults=[NSMutableArray array];
            for(float value:engine.defaults)[defaults addObject:@(value)];
            for(unsigned s=0;s<engine.sceneCount;s++){
                const auto &scene=engine.scenes[s];NSMutableArray *bindings=[NSMutableArray array],*labels=[NSMutableArray array],*pads=[NSMutableArray array];
                for(auto binding:scene.bindings)[bindings addObject:@(binding==255?-1:int(binding))];
                for(const auto &label:scene.labels)[labels addObject:@(label.c_str())];
                for(auto note:scene.pads)[pads addObject:@(note)];
                [scenes addObject:@{@"title":@(scene.title.c_str()),@"subtitle":@(scene.subtitle.c_str()),@"layout":@(scene.layout),@"bindings":[bindings copy],@"labels":[labels copy],@"pads":[pads copy]}];
            }
            [engines addObject:@{@"id":@(engine.id.c_str()),@"name":@(engine.name.c_str()),@"defaults":[defaults copy],@"scenes":[scenes copy]}];
        }
        NSDictionary *info=@{@"id":@(package.id.c_str()),@"name":@(package.name.c_str()),@"version":@(package.version.c_str()),@"selectedEngine":@(index),@"engines":[engines copy]};
        const auto &engine=package.engines[index];
        NSData *module=[data subdataWithRange:NSMakeRange(engine.moduleOffset,engine.moduleSize)];
        // Export identity is checked against the engine manifest by the normal loader.
        auto text=gs::WasmInstrument::inspect(std::vector<uint8_t>((const uint8_t*)module.bytes,(const uint8_t*)module.bytes+module.length),engine.memory);
        NSDictionary *manifest=[NSJSONSerialization JSONObjectWithData:[NSData dataWithBytes:text.data() length:text.size()] options:0 error:nil];
        if(![manifest isKindOfClass:NSDictionary.class]||![manifest[@"id"] isEqual:@(engine.id.c_str())]||![manifest[@"parameter_profile"] isEqual:@"synth-v6"])throw std::runtime_error("Package and engine manifest disagree");
        BOOL loaded=[self loadEngineData:module memory:engine.memory defaults:engines[index][@"defaults"] package:data info:info error:error];
        if(loaded)[self refreshMIDIParameters];return loaded;
    }catch(const std::exception& e){if(error)*error=[NSError errorWithDomain:@"ESPatcherPackage" code:1 userInfo:@{NSLocalizedDescriptionKey:@(e.what())}];return NO;}
}
- (BOOL)loadEngineData:(NSData *)data memory:(unsigned)memory defaults:(NSArray *)defaults package:(NSData *)package info:(NSDictionary *)info error:(NSError **)error {
    try {
        if(data.length>1024*1024)throw std::runtime_error("Firmware file exceeds 1 MiB");
        std::vector<uint8_t> bytes(data.length);if(data.length)memcpy(bytes.data(),data.bytes,data.length);
        std::string text=gs::WasmInstrument::inspect(bytes,memory);
        NSData *json=[NSData dataWithBytes:text.data() length:text.size()];
        id manifest=[NSJSONSerialization JSONObjectWithData:json options:0 error:nil];
        if(![manifest isKindOfClass:NSDictionary.class])throw std::runtime_error("Invalid firmware manifest JSON");
        auto integer=[](id v,int expected){return [v isKindOfClass:NSNumber.class]&&CFGetTypeID((__bridge CFTypeRef)v)!=CFBooleanGetTypeID()&&[v doubleValue]==expected;};
        if(!integer(manifest[@"format"],1)||!integer(manifest[@"abi"],1)||!integer(manifest[@"memory_bytes"],int(memory))||![manifest[@"target"] isEqual:@"cardputer-adv"]||(![manifest[@"parameter_profile"] isEqual:@"mono-v1"]&&![manifest[@"parameter_profile"] isEqual:@"synth-v2"]&&![manifest[@"parameter_profile"] isEqual:@"synth-v3"]&&![manifest[@"parameter_profile"] isEqual:@"synth-v4"]&&![manifest[@"parameter_profile"] isEqual:@"synth-v5"]&&![manifest[@"parameter_profile"] isEqual:@"synth-v6"]))
            throw std::runtime_error("Unsupported format, ABI, target, memory, or parameter profile");
        NSString *identifier=manifest[@"id"],*name=manifest[@"name"],*version=manifest[@"version"];
        if(![identifier isKindOfClass:NSString.class]||identifier.length<1||identifier.length>(package?48:31)||![name isKindOfClass:NSString.class]||name.length<1||name.length>64||![version isKindOfClass:NSString.class]||version.length<1||version.length>32)
            throw std::runtime_error("Missing or invalid firmware identity");
        NSCharacterSet *allowed=[NSCharacterSet characterSetWithCharactersInString:package?@"abcdefghijklmnopqrstuvwxyz0123456789_-":@"abcdefghijklmnopqrstuvwxyz0123456789_"];
        if([identifier rangeOfCharacterFromSet:allowed.invertedSet].location!=NSNotFound)throw std::runtime_error("Invalid firmware ID");
        id labels=manifest[@"knob_labels"];
        if(labels){
            if(![labels isKindOfClass:NSArray.class]||[labels count]!=8)throw std::runtime_error("Expected eight knob labels");
            NSCharacterSet *labelChars=[NSCharacterSet characterSetWithCharactersInString:@"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 /-"].invertedSet;
            for(id label in labels)if(![label isKindOfClass:NSString.class]||[label length]<1||[label length]>7||[label rangeOfCharacterFromSet:labelChars].location!=NSNotFound)throw std::runtime_error("Invalid knob label");
        }
        std::lock_guard<std::mutex> lock(_kernel->control);
        if(_kernel->pending.load(std::memory_order_acquire)>=0)throw std::runtime_error("A firmware switch is still pending; try again");
        int active=_kernel->active.load(std::memory_order_acquire),slot=active==0?1:0;
        auto candidate=std::make_unique<gs::WasmInstrument>(std::move(bytes),identifier.UTF8String,_bus.format.sampleRate,[manifest[@"parameter_profile"] isEqual:@"synth-v6"]?19:([manifest[@"parameter_profile"] isEqual:@"synth-v5"]?16:([manifest[@"parameter_profile"] isEqual:@"synth-v4"]?14:([manifest[@"parameter_profile"] isEqual:@"synth-v3"]?8:([manifest[@"parameter_profile"] isEqual:@"synth-v2"]?6:5)))),memory,memory<262144?16384:65536);
        id declared=manifest[@"polyphony"];
        if(declared&&!integer(declared,int(candidate->voiceLimit())))throw std::runtime_error("Manifest and firmware polyphony disagree");
        if(defaults){
            std::array<float,19> values;for(unsigned p=0;p<19;p++)values[p]=[defaults[p] floatValue];
            candidate->parameters(values);
            if(candidate->faulted())throw std::runtime_error("Engine rejected package defaults");
        }
        _firmwareVoiceLimit=candidate->voiceLimit();
        _firmwareHasLFOShapes=[manifest[@"parameter_profile"] isEqual:@"synth-v6"];
        _firmwareHasTempoSync=_firmwareHasLFOShapes||[manifest[@"parameter_profile"] isEqual:@"synth-v5"];
        _firmwareHasArp=_firmwareHasTempoSync||[manifest[@"parameter_profile"] isEqual:@"synth-v4"];
        _firmwareHasLFO=_firmwareHasArp||[manifest[@"parameter_profile"] isEqual:@"synth-v3"];
        _firmwareHasResonance=_firmwareHasLFO||[manifest[@"parameter_profile"] isEqual:@"synth-v2"];
        _kernel->modules[slot]=std::move(candidate);
        _firmwareKnobLabels=labels?[labels copy]:@[@"CUTOFF",@"RES",@"SHAPE",@"ATTACK",@"RELEASE",@"LEVEL",@"RATE",@"DEPTH"];
        _firmwareData=[data copy];_firmwareName=[name copy];_firmwareID=[identifier copy];
        _kernel->discardBefore[slot]=_kernel->write.load(std::memory_order_acquire);
        if(defaults)for(unsigned p=0;p<19;p++){
            _kernel->parameter(p,[defaults[p] floatValue]);
        }
        _packageData=[package copy];_packageInfo=[info copy];_instrumentRevision++;
        _kernel->pending.store(slot,std::memory_order_release);
        return YES;
    }catch(const std::exception& e){if(error)*error=[NSError errorWithDomain:@"GlitchSynthFirmware" code:1 userInfo:@{NSLocalizedDescriptionKey:@(e.what())}];return NO;}
}

- (AUInternalRenderBlock)internalRenderBlock {
    Kernel *k=_kernel.get();
    return ^AUAudioUnitStatus(AudioUnitRenderActionFlags *flags,const AudioTimeStamp *time,AUAudioFrameCount frames,NSInteger bus,AudioBufferList *out,const AURenderEvent *events,AURenderPullInputBlock pull){
        if(bus!=0)return kAudioUnitErr_InvalidElement;
        if(frames>k->left.size())return kAudioUnitErr_TooManyFramesToProcess;
        if(!out||out->mNumberBuffers!=2)return kAudioUnitErr_FormatNotSupported;
        for(unsigned c=0;c<2;c++)if(out->mBuffers[c].mData&&out->mBuffers[c].mDataByteSize<frames*sizeof(float))return kAudio_ParamError;
        k->begin();unsigned offset=0;
        const int64_t start=time?int64_t(time->mSampleTime):0;
        for(const AURenderEvent *e=events;e;e=e->head.next){
            unsigned position=e->head.eventSampleTime==AUEventSampleTimeImmediate?offset:unsigned(std::clamp<int64_t>(e->head.eventSampleTime-start,offset,frames));
            if(position>offset){k->render(offset,position-offset);offset=position;}
            switch(e->head.eventType){
                case AURenderEventParameter:k->parameter(unsigned(e->parameter.parameterAddress),e->parameter.value);break;
                case AURenderEventParameterRamp:k->parameter(unsigned(e->parameter.parameterAddress),e->parameter.value);break; // v0: adopts ramp target at event time.
                case AURenderEventMIDI:{auto m=e->MIDI.data;if(e->MIDI.length>=3||(e->MIDI.length>=1&&m[0]>=0xf8))k->receiveMIDI(m[0],e->MIDI.length>=2?m[1]:0,e->MIDI.length>=3?m[2]:0);break;}
                default:break;
            }
        }
        if(offset<frames)k->render(offset,frames-offset);
        for(unsigned c=0;c<2;c++){
            float *src=c?k->right.data():k->left.data();if(out->mBuffers[c].mData)std::copy_n(src,frames,(float *)out->mBuffers[c].mData);else out->mBuffers[c].mData=src;
            out->mBuffers[c].mDataByteSize=frames*sizeof(float);out->mBuffers[c].mNumberChannels=1;
        }
        k->scope.push(k->left.data(),frames);
        if(flags)*flags&=~kAudioUnitRenderAction_OutputIsSilence;return noErr;
    };
}
@end
