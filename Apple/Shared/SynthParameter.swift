import Foundation

enum SynthParameter: UInt64, CaseIterable, Identifiable {
    case shape = 0, attack = 1, release = 2, cutoff = 3, volume = 4, resonance = 5, lfoRate = 6, lfoDepth = 7, arpRunning = 8, arpPattern = 9, arpRate = 10, arpGate = 11, arpOctaves = 12, arpLatch = 13, tempo = 14, arpDivision = 15, lfoSync = 16, lfoDivision = 17, lfoShape = 18
    var id: UInt64 { rawValue }
    static let ordered: [Self] = [.cutoff, .resonance, .shape, .attack, .release, .volume, .lfoRate, .lfoDepth]
    var title: String {
        switch self {
        case .shape: "Shape"
        case .attack: "Attack"
        case .release: "Release"
        case .cutoff: "Cutoff"
        case .volume: "Volume"
        case .resonance: "Resonance"
        case .lfoRate: "LFO Rate"
        case .lfoDepth: "LFO Depth"
        case .arpRunning: "Play"
        case .arpPattern: "Pattern"
        case .arpRate: "Arp Rate"
        case .arpGate: "Gate"
        case .arpOctaves: "Octaves"
        case .arpLatch: "Latch"
        case .tempo: "Tempo"
        case .arpDivision: "Arp Rate"
        case .lfoSync: "Sync"
        case .lfoDivision: "LFO Rate"
        case .lfoShape: "LFO Shape"
        }
    }
    var range: ClosedRange<Double> {
        switch self {
        case .shape: 0...2
        case .attack: 0.005...2
        case .release: 0.01...3
        case .cutoff: 80...16000
        case .volume: 0...0.5
        case .resonance, .lfoDepth: 0...1
        case .lfoRate: 0.05...20
        case .arpRunning, .arpLatch: 0...1
        case .arpPattern: 0...3
        case .arpRate: 0.25...20
        case .arpGate: 0.05...1
        case .arpOctaves: 1...4
        case .tempo: 40...300
        case .arpDivision, .lfoDivision: 0...8
        case .lfoSync: 0...1
        case .lfoShape: 0...4
        }
    }
    var defaultValue: Double {
        switch self {
        case .shape: 1
        case .attack: 0.015
        case .release: 0.25
        case .cutoff: 5000
        case .volume: 0.2
        case .resonance, .lfoDepth: 0
        case .lfoRate: 1
        case .arpRunning, .arpPattern, .arpLatch: 0
        case .arpRate: 4
        case .arpGate: 0.5
        case .arpOctaves: 1
        case .tempo: 120
        case .arpDivision: 3
        case .lfoSync, .lfoDivision, .lfoShape: 1
        }
    }
    var isDiscrete: Bool { [.shape, .arpRunning, .arpPattern, .arpOctaves, .arpLatch, .arpDivision, .lfoSync, .lfoDivision, .lfoShape].contains(self) }
    var logarithmic: Bool { self == .cutoff || self == .attack || self == .release || self == .lfoRate || self == .arpRate }
    func normalized(_ value: Double) -> Double {
        let value = min(range.upperBound, max(range.lowerBound, value))
        if logarithmic { return (log(value) - log(range.lowerBound)) / (log(range.upperBound) - log(range.lowerBound)) }
        return (value - range.lowerBound) / (range.upperBound - range.lowerBound)
    }
    func denormalized(_ fraction: Double) -> Double {
        let fraction = min(1, max(0, fraction))
        let value = logarithmic ? exp(log(range.lowerBound) + fraction * (log(range.upperBound) - log(range.lowerBound))) : range.lowerBound + fraction * (range.upperBound - range.lowerBound)
        return isDiscrete ? value.rounded() : value
    }
    func formatted(_ value: Double, shapes: [String]) -> String {
        switch self {
        case .shape: shapes[min(2, max(0, Int(value.rounded())))].uppercased()
        case .attack, .release: "\(Int((value * 1000).rounded())) ms"
        case .cutoff: value >= 1000 ? (value / 1000).formatted(.number.precision(.fractionLength(2))) + " kHz" : "\(Int(value)) Hz"
        case .volume: "\(Int((value * 200).rounded())) %"
        case .tempo: value.formatted(.number.precision(.fractionLength(1))) + " BPM"
        case .lfoSync: value >= 0.5 ? "SYNC" : "FREE"
        case .lfoShape: ["SINE", "TRI", "SQUARE", "SAW", "S&H"][min(4, max(0, Int(value.rounded())))]
        case .arpDivision, .lfoDivision: ["1/1", "1/2", "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32", "1/32T"][min(8, max(0, Int(value.rounded())))]
        case .arpRunning: value >= 0.5 ? "PLAY" : "STOP"
        case .arpLatch: value >= 0.5 ? "ON" : "OFF"
        case .arpPattern: ["UP", "DOWN", "UP/DOWN", "RANDOM"][min(3, max(0, Int(value.rounded())))]
        case .arpOctaves: "\(Int(value.rounded())) OCT"
        case .arpGate: "\(Int((value * 100).rounded())) %"
        case .lfoRate, .arpRate: value.formatted(.number.precision(.fractionLength(2))) + " Hz"
        case .resonance, .lfoDepth: "\(Int((value * 100).rounded())) %"
        }
    }
}
