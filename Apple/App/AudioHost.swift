import AVFoundation
import Observation

extension GSSynthAudioUnit: @unchecked Sendable {}

@MainActor @Observable
final class AudioHost {
    private(set) var unit: GSSynthAudioUnit?
    private(set) var error: String?
    @ObservationIgnored private let engine = AVAudioEngine()
    @ObservationIgnored private var node: AVAudioUnit?
    @ObservationIgnored private var loading = false
    func start() async {
        guard node == nil, !loading else { return }
        loading = true
        defer { loading = false }
        do {
            #if os(iOS) && !targetEnvironment(macCatalyst)
            try AVAudioSession.sharedInstance().setCategory(.playback, mode: .default)
            try AVAudioSession.sharedInstance().setActive(true)
            #endif
            let description = AudioComponentDescription(componentType: kAudioUnitType_MusicDevice, componentSubType: 0x67734476, componentManufacturer: 0x4d534e44, componentFlags: 0, componentFlagsMask: 0)
            AUAudioUnit.registerSubclass(GSSynthAudioUnit.self, as: description, name: "Alex Matheu: ESPatcher", version: 11)
            #if os(macOS)
            let options: AudioComponentInstantiationOptions = [.loadInProcess]
            #else
            let options: AudioComponentInstantiationOptions = []
            #endif
            let audio = try await AVAudioUnit.instantiate(with: description, options: options)
            guard let synth = audio.auAudioUnit as? GSSynthAudioUnit else { throw HostError.wrongUnit }
            engine.attach(audio)
            engine.connect(audio, to: engine.mainMixerNode, format: audio.outputFormat(forBus: 0))
            try engine.start()
            node = audio; unit = synth; error = nil
        } catch { self.error = error.localizedDescription }
    }
    private enum HostError: Error { case wrongUnit }
}
