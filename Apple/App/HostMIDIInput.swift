import CoreMIDI
import AudioToolbox

/// Standalone host only. AUv3 receives MIDI from its host instead.
@MainActor
final class HostMIDIInput {
    private var client = MIDIClientRef()
    private var port = MIDIPortRef()
    private var destination = MIDIEndpointRef()
    private var sources = Set<MIDIEndpointRef>()
    private var mappingTask: Task<Void, Never>?
    private let unit: GSSynthAudioUnit

    init(unit: GSSynthAudioUnit) throws {
        self.unit = unit
        if let maps = UserDefaults.standard.array(forKey: "espatcher.midi.v1") as? [NSNumber] {
            unit.restoreMIDIMappings(maps)
        }
        guard let schedule = unit.scheduleMIDIEventBlock else { throw MIDIError.unavailable }
        let receiver = Receiver(schedule)
        try check(MIDIClientCreateWithBlock("ESPatcher" as CFString, &client) { [weak self] _ in
            Task { @MainActor [weak self] in self?.refresh() }
        })
        do {
            try check(MIDIInputPortCreateWithProtocol(client, "ESPatcher Input" as CFString, ._1_0, &port) { list, _ in receiver.receive(list) })
            try check(MIDIDestinationCreateWithProtocol(client, "ESPatcher" as CFString, ._1_0, &destination) { list, _ in receiver.receive(list) })
            refresh()
            mappingTask = Task { @MainActor [unit] in
                var revision = unit.midiMappingRevision
                while !Task.isCancelled {
                    do { try await Task.sleep(for: .milliseconds(250)) } catch { return }
                    if revision != unit.midiMappingRevision {
                        revision = unit.midiMappingRevision
                        UserDefaults.standard.set(unit.midiMappings(), forKey: "espatcher.midi.v1")
                    }
                }
            }
        } catch { close(); throw error }
    }
    private enum MIDIError: Error { case unavailable }
    private func check(_ code: OSStatus) throws { if code != noErr { throw NSError(domain: NSOSStatusErrorDomain, code: Int(code)) } }
    private func refresh() {
        let available = Set((0..<MIDIGetNumberOfSources()).map { MIDIGetSource($0) }.filter { $0 != 0 })
        if !sources.subtracting(available).isEmpty { unit.panic() }
        for source in sources.subtracting(available) { MIDIPortDisconnectSource(port, source) }
        for source in available.subtracting(sources) { if MIDIPortConnectSource(port, source, nil) == noErr { sources.insert(source) } }
        sources.formIntersection(available)
    }
    func close() {
        mappingTask?.cancel(); mappingTask = nil
        UserDefaults.standard.set(unit.midiMappings(), forKey: "espatcher.midi.v1")
        unit.panic()
        if destination != 0 { MIDIEndpointDispose(destination); destination = 0 }
        if port != 0 { MIDIPortDispose(port); port = 0 }
        if client != 0 { MIDIClientDispose(client); client = 0 }
        sources.removeAll()
    }
    deinit {
        mappingTask?.cancel()
        if destination != 0 { MIDIEndpointDispose(destination) }
        if port != 0 { MIDIPortDispose(port) }
        if client != 0 { MIDIClientDispose(client) }
    }
}

private final class Receiver: @unchecked Sendable {
    let schedule: AUScheduleMIDIEventBlock
    init(_ schedule: @escaping AUScheduleMIDIEventBlock) { self.schedule = schedule }
    func receive(_ list: UnsafePointer<MIDIEventList>) {
        var packet = UnsafeRawPointer(list).advanced(by: MemoryLayout<MIDIEventList>.offset(of: \.packet)!).assumingMemoryBound(to: MIDIEventPacket.self)
        for _ in 0..<list.pointee.numPackets {
            let words = UnsafeRawPointer(packet).advanced(by: MemoryLayout<MIDIEventPacket>.offset(of: \.words)!).assumingMemoryBound(to: UInt32.self)
            var index = 0
            while index < Int(packet.pointee.wordCount) {
                let word = words[index], type = word >> 28
                let lengths = [1,1,1,2,2,4,1,1,2,2,2,3,3,4,4,4]
                let count = lengths[Int(type)]
                guard index + count <= Int(packet.pointee.wordCount) else { break }
                if type == 2 || type == 1 {
                    var bytes = (UInt8((word >> 16) & 255), UInt8((word >> 8) & 127), UInt8(word & 127))
                    withUnsafeBytes(of: &bytes) { data in
                        schedule(AUEventSampleTimeImmediate, 0, type == 1 ? 1 : 3, data.baseAddress!.assumingMemoryBound(to: UInt8.self))
                    }
                }
                index += count
            }
            packet = UnsafePointer(MIDIEventPacketNext(packet))
        }
    }
}
