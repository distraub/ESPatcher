#import "../Apple/Shared/SynthAudioUnit.h"
#include <cassert>
#include <iostream>
int main(){@autoreleasepool{
    AudioComponentDescription d={kAudioUnitType_MusicDevice,'gsm5','MSND',0,0};
    NSError *error=nil;
    GSSynthAudioUnit *unit=[[GSSynthAudioUnit alloc] initWithComponentDescription:d options:0 error:&error];
    assert(unit&&!error);
    assert([unit.firmwareID isEqual:@"pocket_analog"]&&unit.firmwareVoiceLimit==4);
    // A host can restore another engine before it starts rendering.
    NSData *fm=[NSData dataWithContentsOfFile:@"Firmware/Modules/fm_bell.gsm5"];
    assert([unit loadFirmwareData:fm error:&error]);
    assert([unit allocateRenderResourcesAndReturnError:&error]);
    auto render=unit.internalRenderBlock;
    struct Stereo {UInt32 count;AudioBuffer b[2];} out={2,{{1,512*sizeof(float),nullptr},{1,512*sizeof(float),nullptr}}};
    AudioTimeStamp time={};time.mFlags=kAudioTimeStampSampleTimeValid;
    AudioUnitRenderActionFlags flags=0;
    AURenderEvent notes[3]={};
    for(int i=0;i<3;i++){
        notes[i].MIDI.eventType=AURenderEventMIDI;
        notes[i].MIDI.eventSampleTime=0;
        notes[i].MIDI.length=3;
        notes[i].MIDI.data[0]=0x90;
        notes[i].MIDI.data[1]=60+i*4;
        notes[i].MIDI.data[2]=100;
        notes[i].head.next=i<2?&notes[i+1]:nullptr;
    }
    assert(render(&flags,&time,512,0,(AudioBufferList*)&out,notes,nil)==0);
    assert(unit.firmwareActiveVoices==3);
    [unit deallocateRenderResources];
    // Also prove the default itself handles a chord with no UI or restore.
    GSSynthAudioUnit *fresh=[[GSSynthAudioUnit alloc] initWithComponentDescription:d options:0 error:&error];
    assert([fresh allocateRenderResourcesAndReturnError:&error]);
    out.b[0].mData=out.b[1].mData=nullptr;
    assert(fresh.internalRenderBlock(&flags,&time,512,0,(AudioBufferList*)&out,notes,nil)==0);
    assert(fresh.firmwareActiveVoices==3);
    std::cout<<"PASS: headless AUv3 boots four-voice AMSynth, renders host MIDI chords and permits pre-render engine restore\n";
}}
