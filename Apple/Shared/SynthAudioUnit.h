#import <AudioToolbox/AudioToolbox.h>
#import <AVFoundation/AVFoundation.h>
NS_ASSUME_NONNULL_BEGIN
@interface GSSynthAudioUnit : AUAudioUnit
- (void)playNote:(uint8_t)note velocity:(uint8_t)velocity;
- (void)releaseNote:(uint8_t)note;
- (void)panic;
- (void)beginMIDILearn:(uint32_t)address;
- (void)cancelMIDILearn;
- (void)refreshMIDIParameters;
- (void)clearMIDIMapping:(uint32_t)address;
- (uint32_t)midiMapping:(uint32_t)address;
@property(nonatomic, readonly) NSInteger midiLearnTarget;
@property(nonatomic, readonly) uint32_t midiRevision;
@property(nonatomic, readonly) uint32_t midiMappingRevision;
- (NSArray<NSNumber *> *)midiMappings;
- (void)restoreMIDIMappings:(NSArray<NSNumber *> *)mappings;
// UI-only allocation; audio capture is fixed-size and lock-free.
- (NSArray<NSNumber *> *)scopeSnapshot;
@property(nonatomic, readonly) NSArray<NSString *> *firmwareKnobLabels;
// Control-side load. Validation/compilation never runs in the audio callback.
- (BOOL)loadFirmwareData:(NSData *)data error:(NSError **)error NS_SWIFT_NAME(loadFirmware(_:));
@property(nonatomic, readonly) NSString *firmwareName;
@property(nonatomic, readonly) NSString *firmwareID;
@property(nonatomic, readonly) BOOL firmwareFaulted;
@property(nonatomic, readonly) BOOL firmwareHasResonance;
@property(nonatomic, readonly) BOOL firmwareHasLFO;
@property(nonatomic, readonly) BOOL firmwareHasArp;
@property(nonatomic, readonly) BOOL firmwareHasTempoSync;
@property(nonatomic, readonly) BOOL firmwareHasLFOShapes;
@property(nonatomic, readonly) uint32_t firmwareLFOStatus;
@property(nonatomic, readonly) uint32_t firmwareArpStatus;
@property(nonatomic, readonly) NSInteger firmwareVoiceLimit;
@property(nonatomic, readonly) NSInteger firmwareActiveVoices;
@end
NS_ASSUME_NONNULL_END
