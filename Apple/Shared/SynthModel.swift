import AudioToolbox
import Observation
import Foundation

@MainActor @Observable
final class SynthModel {
    let navigation = NavigationModel()
    let unit: GSSynthAudioUnit
    var wave = 1 { didSet { write(0, Float(wave)) } }
    var attack = 0.015 { didSet { write(1, Float(attack)) } }
    var release = 0.25 { didSet { write(2, Float(release)) } }
    var cutoff = 5000.0 { didSet { write(3, Float(cutoff)) } }
    var volume = 0.2 { didSet { write(4, Float(volume)) } }
    var resonance = 0.0 { didSet { write(5, Float(resonance)) } }
    var lfoRate = 1.0 { didSet { write(6, Float(lfoRate)) } }
    var lfoDepth = 0.0 { didSet { write(7, Float(lfoDepth)) } }
    private(set) var activeParameter: SynthParameter?
    private(set) var knobLabels = ["CUTOFF", "RES", "SHAPE", "ATTACK", "RELEASE", "LEVEL", "RATE", "DEPTH"]
    var arpRunning = 0.0 { didSet { write(8, Float(arpRunning)) } }
    var arpPattern = 0.0 { didSet { write(9, Float(arpPattern)) } }
    var arpRate = 4.0 { didSet { write(10, Float(arpRate)) } }
    var arpGate = 0.5 { didSet { write(11, Float(arpGate)) } }
    var arpOctaves = 1.0 { didSet { write(12, Float(arpOctaves)) } }
    var arpLatch = 0.0 { didSet { write(13, Float(arpLatch)) } }
    var tempo = 120.0 { didSet { write(14, Float(tempo)) } }
    var arpDivision = 3.0 { didSet { write(15, Float(arpDivision)) } }
    var lfoSync = 1.0 { didSet { write(16, Float(lfoSync)) } }
    var lfoDivision = 1.0 { didSet { write(17, Float(lfoDivision)) } }
    var lfoShape = 1.0 { didSet { write(18, Float(lfoShape)) } }
    var firmwareChoice = "pocket_analog"
    private(set) var firmwareName = "Native voice"
    private(set) var firmwareLoading = false
    var firmwareError: String?
    var shapeLabels: [String] { firmwareChoice == "fm_bell" ? ["1:1", "2:1", "3:1"] : ["Sine", "Saw", "Square"] }
    var shapeTitle: String { firmwareChoice == "fm_bell" ? "FM ratio" : "Waveform" }
    private(set) var resonanceAvailable = false
    private(set) var lfoAvailable = false
    private(set) var arpAvailable = false
    private(set) var tempoSyncAvailable = false
    private(set) var lfoShapesAvailable = false
    private(set) var voiceLimit = 1
    private(set) var activeVoices = 0
    private(set) var testChordHeld = false
    static let chordKeys = ["⌥1", "⌥2", "⌥3", "⌥4"]
    var chords = (0..<4).map { ChordAssignment(quality: $0) }
    var chordMode: Int?
    var octave = 4
    private(set) var notes: [String: Int] = [:]
    @ObservationIgnored private var midiRevision: UInt32 = 0
    @ObservationIgnored private var adoptingParameters = false
    @ObservationIgnored private var token: AUParameterObserverToken?
    static let keys = Array("awsedftgyhujk")

    init(unit: GSSynthAudioUnit) {
        self.unit = unit
        resonanceAvailable = unit.firmwareHasResonance
        lfoAvailable = unit.firmwareHasLFO
        arpAvailable = unit.firmwareHasArp
        tempoSyncAvailable = unit.firmwareHasTempoSync
        lfoShapesAvailable = unit.firmwareHasLFOShapes
        voiceLimit = unit.firmwareVoiceLimit
        activeVoices = unit.firmwareActiveVoices
        firmwareName = unit.firmwareName
        knobLabels = unit.firmwareKnobLabels
        firmwareChoice = unit.firmwareID == "native" ? "pocket_analog" : unit.firmwareID
        adopt()
        let observer: @Sendable (AUParameterAddress, AUValue) -> Void = { [weak self] _, _ in
            Task { @MainActor [weak self] in self?.adopt() }
        }
        token = unit.parameterTree?.token(byAddingParameterObserver: observer)
    }

    func refreshFirmwareStatus() {
        guard !firmwareLoading else { return }
        if midiRevision != unit.midiRevision { midiRevision = unit.midiRevision; unit.refreshMIDIParameters(); adopt() }
        resonanceAvailable = unit.firmwareHasResonance
        lfoAvailable = unit.firmwareHasLFO
        arpAvailable = unit.firmwareHasArp
        tempoSyncAvailable = unit.firmwareHasTempoSync
        lfoShapesAvailable = unit.firmwareHasLFOShapes
        voiceLimit = unit.firmwareVoiceLimit
        activeVoices = unit.firmwareActiveVoices
        firmwareName = unit.firmwareName
        knobLabels = unit.firmwareKnobLabels
        if unit.firmwareID != "native" { firmwareChoice = unit.firmwareID }
        if unit.firmwareFaulted { firmwareError = "The firmware halted. Load an instrument again to restart it." }
    }
    func loadBundled(_ id: String) async {
        guard let url = Bundle(for: GSSynthAudioUnit.self).url(forResource: id, withExtension: "gsm5", subdirectory: "Modules") else {
            firmwareError = "Bundled firmware is missing."; return
        }
        await loadFile(url)
    }
    func loadFile(_ url: URL) async {
        guard !firmwareLoading else { return }
        firmwareLoading = true
        defer { firmwareLoading = false }
        let audioUnit = unit
        do {
            guard url.pathExtension.lowercased() == "gsm5" else { throw FirmwareError.wrongType }
            try await Task.detached(priority: .userInitiated) {
                let access = url.startAccessingSecurityScopedResource()
                defer { if access { url.stopAccessingSecurityScopedResource() } }
                let size = try url.resourceValues(forKeys: [.fileSizeKey]).fileSize ?? 0
                guard size <= 1_048_576 else { throw FirmwareError.tooLarge }
                let data = try Data(contentsOf: url)
                try audioUnit.loadFirmware(data)
            }.value
            notes.removeAll(); testChordHeld = false
            resonanceAvailable = unit.firmwareHasResonance
            lfoAvailable = unit.firmwareHasLFO
        arpAvailable = unit.firmwareHasArp
        tempoSyncAvailable = unit.firmwareHasTempoSync
        lfoShapesAvailable = unit.firmwareHasLFOShapes
            voiceLimit = unit.firmwareVoiceLimit
            activeVoices = unit.firmwareActiveVoices
            firmwareName = unit.firmwareName
            knobLabels = unit.firmwareKnobLabels; firmwareChoice = unit.firmwareID
            firmwareError = nil
        } catch {
            firmwareError = error.localizedDescription
            firmwareChoice = unit.firmwareID == "native" ? "pocket_analog" : unit.firmwareID
        }
    }
    private enum FirmwareError: LocalizedError {
        case wrongType, tooLarge
        var errorDescription: String? {
            switch self {
            case .wrongType: "Choose a .gsm5 firmware module."
            case .tooLarge: "Firmware files must be no larger than 1 MiB."
            }
        }
    }

    func detach() {
        panic()
        if let token { unit.parameterTree?.removeParameterObserver(token); self.token = nil }
    }
    private func write(_ address: UInt64, _ value: Float) {
        guard !adoptingParameters, let parameter = unit.parameterTree?.parameter(withAddress: address), parameter.value != value else { return }
        parameter.setValue(value, originator: token)
    }
    func adopt() {
        guard let tree = unit.parameterTree else { return }
        adoptingParameters = true
        defer { adoptingParameters = false }
        wave = Int((tree.parameter(withAddress: 0)?.value ?? 1).rounded())
        attack = Double(tree.parameter(withAddress: 1)?.value ?? 0.015)
        release = Double(tree.parameter(withAddress: 2)?.value ?? 0.25)
        cutoff = Double(tree.parameter(withAddress: 3)?.value ?? 5000)
        volume = Double(tree.parameter(withAddress: 4)?.value ?? 0.2)
        resonance = Double(tree.parameter(withAddress: 5)?.value ?? 0)
        lfoRate = Double(tree.parameter(withAddress: 6)?.value ?? 1)
        lfoDepth = Double(tree.parameter(withAddress: 7)?.value ?? 0)
        arpRunning = Double(tree.parameter(withAddress: 8)?.value ?? 0)
        arpPattern = Double(tree.parameter(withAddress: 9)?.value ?? 0)
        arpRate = Double(tree.parameter(withAddress: 10)?.value ?? 4)
        arpGate = Double(tree.parameter(withAddress: 11)?.value ?? 0.5)
        arpOctaves = Double(tree.parameter(withAddress: 12)?.value ?? 1)
        arpLatch = Double(tree.parameter(withAddress: 13)?.value ?? 0)
        tempo = Double(tree.parameter(withAddress: 14)?.value ?? 120)
        arpDivision = Double(tree.parameter(withAddress: 15)?.value ?? 3)
        lfoSync = Double(tree.parameter(withAddress: 16)?.value ?? 1)
        lfoDivision = Double(tree.parameter(withAddress: 17)?.value ?? 1)
        lfoShape = Double(tree.parameter(withAddress: 18)?.value ?? 1)
    }
    func value(_ parameter: SynthParameter) -> Double {
        switch parameter {
        case .shape: Double(wave)
        case .attack: attack
        case .release: release
        case .cutoff: cutoff
        case .volume: volume
        case .resonance: resonance
        case .lfoRate: lfoRate
        case .lfoDepth: lfoDepth
        case .arpRunning: arpRunning
        case .arpPattern: arpPattern
        case .arpRate: arpRate
        case .arpGate: arpGate
        case .arpOctaves: arpOctaves
        case .arpLatch: arpLatch
        case .tempo: tempo
        case .arpDivision: arpDivision
        case .lfoSync: lfoSync
        case .lfoDivision: lfoDivision
        case .lfoShape: lfoShape
        }
    }
    func screenValue(_ parameter: SynthParameter) -> Double {
        Double(unit.parameterTree?.parameter(withAddress: parameter.rawValue)?.value ?? Float(value(parameter)))
    }
    func toggleArp() { if arpAvailable { arpRunning = arpRunning >= 0.5 ? 0 : 1 } }
    func supports(_ parameter: SynthParameter) -> Bool {
        if parameter.rawValue >= 16 { return lfoShapesAvailable }
        if parameter == .tempo { return true }
        if parameter == .arpDivision { return tempoSyncAvailable }
        if parameter.rawValue >= 8 { return arpAvailable }
        if parameter == .resonance { return resonanceAvailable }
        if parameter == .lfoRate || parameter == .lfoDepth { return lfoAvailable }
        return true
    }
    func preview(_ parameter: SynthParameter, value: Double) { activeParameter = parameter; write(parameter.rawValue, Float(value)) }
    func commit(_ parameter: SynthParameter, value: Double) {
        switch parameter {
        case .shape: wave = Int(value.rounded())
        case .attack: attack = value
        case .release: release = value
        case .cutoff: cutoff = value
        case .volume: volume = value
        case .resonance: resonance = value
        case .lfoRate: lfoRate = value
        case .lfoDepth: lfoDepth = value
        case .arpRunning: arpRunning = value
        case .arpPattern: arpPattern = value
        case .arpRate: arpRate = value
        case .arpGate: arpGate = value
        case .arpOctaves: arpOctaves = value
        case .arpLatch: arpLatch = value
        case .tempo: tempo = value
        case .arpDivision: arpDivision = value
        case .lfoSync: lfoSync = value
        case .lfoDivision: lfoDivision = value
        case .lfoShape: lfoShape = value
        }
    }
    func down(_ index: Int, source: String) {
        guard !notes.keys.contains(where: { $0 == source || $0.hasPrefix(source + "-voice") }) else { return }
        let intervals = chordMode.map { chords[$0].notes } ?? [0]
        for (voice, offset) in intervals.enumerated() {
            singleDown(index + offset, source: source + "-voice\(voice)")
        }
    }
    private func singleDown(_ index: Int, source: String) {
        guard notes[source] == nil else { return }
        let midi = 12 * (octave + 1) + index
        guard (0...127).contains(midi) else { return }
        if !notes.values.contains(midi) { unit.playNote(UInt8(midi), velocity: 100) }
        notes[source] = midi
    }
    func up(_ source: String) {
        let owned = notes.keys.filter { $0 == source || $0.hasPrefix(source + "-voice") }
        for key in owned {
            guard let midi = notes.removeValue(forKey: key) else { continue }
            if !notes.values.contains(midi) { unit.releaseNote(UInt8(midi)) }
        }
    }
    func selectChordMode(_ slot: Int?) {
        panic()
        chordMode = chordMode == slot ? nil : slot
    }
    func toggleTestChord() {
        let enable = !testChordHeld
        panic()
        if enable { for index in [0, 4, 7, 11] { singleDown(index, source: "chord\(index)") } }
        testChordHeld = enable
    }
    func panic() { notes.removeAll(); testChordHeld = false; unit.panic() }
    func shift(_ amount: Int) { panic(); octave = min(7, max(1, octave + amount)) }
}
