import Foundation
import Observation

@MainActor @Observable
final class NavigationModel {
    @ObservationIgnored private let bridge = GSNavigation()
    var page = 0
    var selected = 0
    var section = 0
    var menu = 0
    var browser = 0
    var theme = SynthTheme.defaultIndex
    var editing = false
    var saveRevision = 0
    var bindings: [SynthParameter?] = SynthParameter.ordered.map(Optional.some)
    static let titles = ["Home", "Global", "OSC", "LFO", "Arp"]
    private var now: UInt32 { UInt32(truncatingIfNeeded: DispatchTime.now().uptimeNanoseconds / 1_000_000) }
    func configure(theme: Int, model: SynthModel) {
        bridge.configureTheme(theme, instrument: model.firmwareChoice == "fm_bell" ? 1 : 0)
        sync(model)
    }
    func go(_ page: Int, model: SynthModel) { bridge.setInstrument(model.firmwareChoice == "fm_bell" ? 1 : 0); bridge.go(page); sync(model) }
    func key(_ code: Int, down: Bool, fine: Bool, model: SynthModel) {
        bridge.key(code, down: down, fine: fine, time: now); consume(model)
    }
    func tick(_ model: SynthModel) { bridge.tick(now); consume(model) }
    func releaseKeys(_ model: SynthModel) {
        for key in 0...14 { bridge.key(key, down: false, fine: false, time: now) }
        sync(model)
    }
    private func consume(_ model: SynthModel) {
        if bridge.editAddress >= 0, let parameter = SynthParameter(rawValue: UInt64(bridge.editAddress)), model.supports(parameter) {
            let old = model.value(parameter)
            let value = parameter.isDiscrete ? min(parameter.range.upperBound, max(parameter.range.lowerBound, old + (bridge.delta > 0 ? 1 : -1))) : parameter.denormalized(parameter.normalized(old) + Double(bridge.delta))
            model.preview(parameter, value: value); model.commit(parameter, value: value)
        }
        if bridge.loadInstrument >= 0 {
            let id = bridge.loadInstrument == 1 ? "fm_bell" : "pocket_analog"
            Task { await model.loadBundled(id) }
        }
        if bridge.saveTheme { saveRevision += 1 }
        sync(model)
    }
    private func sync(_ model: SynthModel) {
        if page != bridge.page { page = bridge.page }
        if selected != bridge.selected { selected = bridge.selected }
        if section != bridge.section { section = bridge.section }
        if menu != bridge.menu { menu = bridge.menu }
        if browser != bridge.browser { browser = bridge.browser }
        if theme != bridge.theme { theme = bridge.theme }
        if editing != bridge.editing { editing = bridge.editing }
        let next = (0..<8).map { slot -> SynthParameter? in
            if bridge.page == 3 && !model.lfoShapesAvailable {
                return slot == 0 ? .lfoRate : slot == 1 ? .lfoDepth : nil
            }
            let address = bridge.binding(slot)
            if address == 17 && (!model.lfoShapesAvailable || model.lfoSync < 0.5) { return .lfoRate }
            if address == 15 && !model.tempoSyncAvailable && model.arpAvailable { return .arpRate }
            return address < 0 ? nil : SynthParameter(rawValue: UInt64(address))
        }
        if bindings != next { bindings = next }
    }
    func label(_ slot: Int, model: SynthModel) -> String {
        guard let parameter = bindings[slot], model.supports(parameter) else { return "UNUSED" }
        if page == 0 || page == 1 { return model.knobLabels[slot] }
        switch parameter {
        case .shape: return model.firmwareChoice == "fm_bell" ? "RATIO" : "WAVE"
        case .attack: return "ATTACK"
        case .release: return "RELEASE"
        case .cutoff: return "CUTOFF"
        case .resonance: return "RES"
        case .volume: return "LEVEL"
        case .lfoRate: return "RATE"
        case .lfoDepth: return "DEPTH"
        case .arpRunning: return "PLAY"
        case .arpPattern: return "PATTERN"
        case .arpRate: return "RATE"
        case .arpGate: return "GATE"
        case .arpOctaves: return "OCTAVES"
        case .arpLatch: return "LATCH"
        case .tempo: return "TEMPO"
        case .arpDivision: return "RATE"
        case .lfoSync: return "SYNC"
        case .lfoDivision: return "RATE"
        case .lfoShape: return "SHAPE"
        }
    }
}
