import SwiftUI

struct SynthView: View {
    @AppStorage("glitchSynth.themeIndex") private var themeIndex = SynthTheme.defaultIndex
    @AppStorage("espatcher.showPiano") private var showPiano = false
    @State private var heldChordKeys: Set<Int> = []
    @State private var performanceWidth: CGFloat = 1088
    @State private var model: SynthModel
    @Environment(\.scenePhase) private var scenePhase
    @FocusState private var keyboardFocused: Bool

    init(unit: GSSynthAudioUnit) { _model = State(initialValue: SynthModel(unit: unit)) }
    private var performanceHeight: CGFloat { (performanceWidth - 344 - 16) * 135 / 240 }
    private var palette: SynthTheme { SynthTheme.all[min(SynthTheme.all.count - 1, max(0, model.navigation.theme))] }
    private var scrollAxes: Axis.Set {
        #if os(macOS)
        .vertical
        #else
        [.horizontal, .vertical]
        #endif
    }
    var body: some View {
        @Bindable var model = model
        ScrollView(scrollAxes) {
            VStack(alignment: .leading, spacing: 12) {
                FirmwarePicker(model: model)
                NavigationStrip(model: model)
                HStack(alignment: .top, spacing: 16) {
                    Group {
                        if model.packageInfo != nil { PackageInstrumentScreen(model: model) }
                        else { InstrumentScreen(model: model) }
                    }
                        .frame(maxWidth: .infinity)
                    PerformancePads(model: model, height: performanceHeight)
                }
                .frame(height: performanceHeight)
                .onGeometryChange(for: CGFloat.self) { $0.size.width } action: { width in
                    if width > 0 { performanceWidth = width }
                }
                SynthKnobPanel(model: model)
                InstrumentControls(model: model)
                ChordLaunchers(model: model)
                if showPiano {
                    HStack(spacing: 3) { ForEach(0..<13) { index in NoteKey(index: index, model: model) } }
                }
            }.padding(16).frame(minWidth: 1024, maxWidth: 1120)
        }
        .background {
            palette.background.contentShape(.rect)
                .onTapGesture { keyboardFocused = true }
        }
        .foregroundStyle(palette.text).tint(palette.accent)
        .preferredColorScheme(palette.isLight ? .light : .dark)
        .environment(\.synthTheme, palette)
        .focusable().focusEffectDisabled().focused($keyboardFocused)
        .onKeyPress(phases: [.down, .repeat, .up]) { press in
            let key = press.characters.lowercased()
            if press.modifiers.contains(.command) { return .ignored }
            // Option-number produces these characters on a US Mac keyboard.
            // Also accept the unmodified number reported by other layouts.
            if let slot = ["1": 0, "2": 1, "3": 2, "4": 3, "¡": 0, "™": 1, "£": 2, "¢": 3][key], press.modifiers.contains(.option) || heldChordKeys.contains(slot) {
                if press.phase == .down, !heldChordKeys.contains(slot) {
                    heldChordKeys.insert(slot); model.selectChordMode(slot)
                } else if press.phase == .up {
                    heldChordKeys.remove(slot)
                }
                return .handled
            }
            if key == "l" {
                if press.phase == .down { model.openLibrary() }
                return .handled
            }
            if let page = ["c", "v", "b", "n", "m"].firstIndex(of: key) {
                if press.phase == .down { model.navigation.go(model.packageInfo == nil || page == 0 ? page : page + 1, model: model) }
                return .handled
            }
            if key == "-" || key == "_" {
                if press.phase == .down { model.toggleArp() }
                return .handled
            }
            // Record remains reserved for the future sequencer.
            if ["-", "=", "_", "+"].contains(key) { return .handled }
            let code: Int? = switch press.key {
            case .leftArrow: 8
            case .rightArrow: 9
            case .upArrow: 10
            case .downArrow: 11
            case .tab: 12
            case .return: 13
            case .escape, .delete: 14
            default: ["\u{19}": 12, ",": 8, "/": 9, ";": 10, ".": 11, "`": 14, "!": 0, "@": 1, "#": 2, "$": 3, "%": 4, "^": 5, "&": 6, "*": 7][key] ?? (key.count == 1 ? Int(key).flatMap { (1...8).contains($0) ? $0 - 1 : nil } : nil)
            }
            if let code {
                if press.phase != .repeat { model.navigation.key(code, down: press.phase == .down, fine: press.modifiers.contains(.shift), model: model) }
                return .handled
            }
            if let char = key.first, let index = SynthModel.keys.firstIndex(of: char), key.count == 1 {
                if press.phase == .down { model.down(index, source: "key\(index)") }
                else if press.phase == .up { model.up("key\(index)") }
                return .handled
            }
            // Consume auto-repeat without retriggering notes or one-shot commands.
            if press.phase == .repeat, ["8", "z", "x", " "].contains(key) { return .handled }
            if press.phase == .down {
                if key == "z" { model.shift(-1); return .handled }
                if key == "x" { model.shift(1); return .handled }
                if key == " " { model.panic(); return .handled }
            }
            return .ignored
        }
        .task {
            model.navigation.configure(theme: themeIndex, model: model)
            while !Task.isCancelled {
                do { try await Task.sleep(for: .milliseconds(25)) } catch { return }
                model.navigation.tick(model)
            }
        }
        .onChange(of: model.navigation.saveRevision) { _, _ in themeIndex = model.navigation.theme }
        .onChange(of: scenePhase) { _, phase in if phase != .active { heldChordKeys.removeAll(); model.navigation.releaseKeys(model); model.panic() } }
        .onChange(of: keyboardFocused) { _, focused in if !focused { heldChordKeys.removeAll(); model.navigation.releaseKeys(model); if !model.testChordHeld { model.panic() } } }
        .onDisappear { model.detach() }
    }
}
