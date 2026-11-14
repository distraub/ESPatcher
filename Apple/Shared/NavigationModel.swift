import Foundation
import Observation

@MainActor @Observable
final class NavigationModel {
    @ObservationIgnored private let bridge = GSNavigation()
    @ObservationIgnored private var packageKeys: Set<Int> = []
    @ObservationIgnored private var packageFine = false
    @ObservationIgnored private var packageRepeatAt: UInt32 = 0
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
        if model.packageInfo != nil {
            self.theme = theme; page = 0; selected = 0; editing = false; packageKeys.removeAll(); syncPackage(model); return
        }
        bridge.configureTheme(theme, instrument: model.firmwareChoice == "fm_bell" ? 1 : 0)
        sync(model)
    }
    func go(_ page: Int, model: SynthModel) {
        if let info = model.packageInfo {
            let scene = page == 0 ? 0 : page - 1
            guard page == 1 || info.engine.scenes.indices.contains(scene) else { return }
            self.page = page; selected = 0; editing = false; packageKeys.removeAll(); syncPackage(model); return
        }
        bridge.setInstrument(model.firmwareChoice == "fm_bell" ? 1 : 0); bridge.go(page); sync(model) }
    func key(_ code: Int, down: Bool, fine: Bool, model: SynthModel) {
        if model.packageInfo != nil {
            packageFine = fine
            if !down { packageKeys.remove(code); return }
            guard packageKeys.insert(code).inserted else { return }
            if code < 8 { selected = code; editing = true }
            else if code == 12 { editing = true; selectPackageSlot(fine ? -1 : 1) }
            else if code == 14 { if editing { editing = false } else { go(0, model: model) } }
            else if (8...11).contains(code) { packageArrow(code, model); packageRepeatAt = now &+ 400 }
            return
        }
        bridge.key(code, down: down, fine: fine, time: now); consume(model)
    }
    func tick(_ model: SynthModel) {
        if model.packageInfo != nil {
            syncPackage(model)
            if let arrow = packageKeys.sorted().first(where: { (8...11).contains($0) }), Int32(bitPattern: now &- packageRepeatAt) >= 0 {
                packageArrow(arrow, model); packageRepeatAt = now &+ 80
            }
            return
        }
        bridge.tick(now); consume(model)
    }
    private func syncPackage(_ model: SynthModel) {
        let next = (model.packageScene?.bindings ?? Array(repeating: -1, count: 8)).map { address -> SynthParameter? in
            if address == 17 && model.lfoSync < 0.5 { return .lfoRate }
            return address < 0 ? nil : SynthParameter(rawValue: UInt64(address))
        }
        if bindings != next { bindings = next }
    }
    private func selectPackageSlot(_ direction: Int) {
        guard bindings.contains(where: { $0 != nil }) else { return }
        repeat { selected = (selected + direction + 8) % 8 } while bindings[selected] == nil
    }
    private func packageArrow(_ code: Int, _ model: SynthModel) {
        guard page != 1, editing else { return }
        let direction = code == 8 || code == 11 ? -1.0 : 1.0
        if (code == 8 || code == 9) && !packageKeys.contains(where: { $0 < 8 }) { selectPackageSlot(Int(direction)); return }
        guard let parameter = bindings[selected] else { return }
        let old = model.value(parameter)
        let value = parameter.isDiscrete ? min(parameter.range.upperBound, max(parameter.range.lowerBound, old + direction)) : parameter.denormalized(parameter.normalized(old) + direction * (packageFine ? 0.002 : 0.02))
        model.preview(parameter, value: value); model.commit(parameter, value: value)
    }
    func releaseKeys(_ model: SynthModel) {
        if model.packageInfo != nil { packageKeys.removeAll(); editing = false; syncPackage(model); return }
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
        if let scene = model.packageScene { return scene.labels[slot] }
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
