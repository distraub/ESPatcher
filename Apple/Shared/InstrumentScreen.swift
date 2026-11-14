import SwiftUI

/// Draw at the ADV's actual logical resolution; only this leaf redraws at scope cadence.
struct InstrumentScreen: View {
    let model: SynthModel
    @State private var lfoHistory = Array(repeating: 0.0, count: 96)
    @Environment(\.synthTheme) private var theme
    @Environment(\.scenePhase) private var scenePhase

    var body: some View {
        TimelineView(.animation(minimumInterval: 1.0 / 30, paused: scenePhase != .active)) { _ in
            let samples = model.navigation.page == 3 ? [] : model.unit.scopeSnapshot().map(\.doubleValue)
            Canvas { context, size in
                context.scaleBy(x: size.width / 240, y: size.height / 135)
                context.fill(Path(CGRect(x: 0, y: 0, width: 240, height: 135)), with: .color(theme.background))
                func text(_ value: String, x: Double, y: Double, size: Double, color: Color, bold: Bool = false) {
                    context.draw(Text(value).font(.system(size: size, weight: bold ? .bold : .regular, design: .monospaced)).foregroundStyle(color), at: CGPoint(x: x, y: y), anchor: .topLeading)
                }
                let header = NavigationModel.titles[model.navigation.page].uppercased() + " / " + model.firmwareName.uppercased()
                let headerSize = min(9, 170 / (Double(max(1, header.count)) * 0.62))
                text(header, x: 5, y: 4, size: headerSize, color: theme.text, bold: true)
                text("\(model.activeVoices < 0 ? "–" : String(model.activeVoices))/\(model.voiceLimit) · O\(model.octave)", x: 181, y: 5, size: 7, color: theme.alternate)
                if model.navigation.page == 1 {
                    let nav = model.navigation
                    if nav.section == 3 {
                        text("GLOBAL / TEMPO", x: 5, y: 25, size: 10, color: theme.accent, bold: true)
                        text(SynthParameter.tempo.formatted(model.screenValue(.tempo), shapes: []), x: 12, y: 56, size: 22, color: theme.text, bold: true)
                        text("ARROWS 1 BPM / SHIFT 0.1", x: 5, y: 102, size: 7, color: theme.chrome)
                        text("OK BACK / ESC BACK", x: 5, y: 123, size: 6, color: theme.chrome)
                        return
                    }
                    let title = nav.section == 0 ? "GLOBAL" : nav.section == 1 ? "INSTRUMENTS" : "THEMES / PREVIEW"
                    text(title, x: 5, y: 22, size: 10, color: theme.accent, bold: true)
                    let entries = nav.section == 0 ? ["INSTRUMENT", "THEME", "TEMPO"] : nav.section == 1 ? ["AMSYNTH ANALOG", "AMSYNTH FM"] : SynthTheme.all.map(\.name)
                    let selected = nav.section == 0 ? nav.menu : nav.section == 1 ? nav.browser : nav.theme
                    let first = max(0, min(entries.count - 4, selected - 1))
                    for index in first..<min(first + 4, entries.count) {
                        let y = Double(index - first) * 17 + 44
                        let active = index == selected
                        if active { context.fill(Path(CGRect(x: 4, y: y - 2, width: 232, height: 16)), with: .color(theme.surface)) }
                        text((active ? "> " : "  ") + entries[index].uppercased(), x: 7, y: y, size: 8, color: active ? theme.accent : theme.text, bold: active)
                    }
                    text(nav.section == 2 ? "ARROWS PREVIEW / OK SAVE / ESC CANCEL" : "ARROWS SELECT / OK OPEN / ESC HOME", x: 5, y: 123, size: 6, color: theme.chrome)
                    return
                }
                var grid = Path()
                grid.move(to: CGPoint(x: 5, y: 43)); grid.addLine(to: CGPoint(x: 235, y: 43))
                for x in stride(from: 5, through: 235, by: 46) {
                    grid.move(to: CGPoint(x: x, y: 24)); grid.addLine(to: CGPoint(x: x, y: 64))
                }
                context.stroke(grid, with: .color(theme.chrome.opacity(0.24)), lineWidth: 0.5)
                if model.navigation.page == 3 {
                    let status = model.unit.firmwareLFOStatus
                    let phase = Double(status >> 16) / 65536
                    let raw = Double(Int16(bitPattern: UInt16(status & 65535))) / 32767
                    let shape = model.lfoShapesAvailable ? Int(model.screenValue(.lfoShape).rounded()) : 1
                    let depth = model.screenValue(.lfoDepth)
                    var reference = Path(), modulation = Path()
                    for index in 0..<128 {
                        let x = 5 + Double(index) * 230 / 127
                        let value = shape == 4 ? lfoHistory[index * 95 / 127] : LFOWaveform.value(phase: Double(index) / 128, shape: shape)
                        let ref = CGPoint(x: x, y: 43 - value * 17)
                        let actual = CGPoint(x: x, y: 43 - value * depth * 17)
                        if index == 0 { reference.move(to: ref); modulation.move(to: actual) }
                        else { reference.addLine(to: ref); modulation.addLine(to: actual) }
                    }
                    context.stroke(reference, with: .color(theme.chrome.opacity(0.5)), lineWidth: 0.6)
                    context.stroke(modulation, with: .color(theme.accent), lineWidth: 1)
                    if model.lfoShapesAvailable {
                        let x = shape == 4 ? 235 : 5 + phase * 230
                        var cursor = Path(); cursor.move(to: CGPoint(x: x, y: 25)); cursor.addLine(to: CGPoint(x: x, y: 62))
                        context.stroke(cursor, with: .color(theme.alternate), lineWidth: 0.5)
                        context.fill(Path(ellipseIn: CGRect(x: x - 1.5, y: 43 - raw * depth * 17 - 1.5, width: 3, height: 3)), with: .color(theme.text))
                    }
                } else {
                let amplitude = max(0.02, samples.map(abs).max() ?? 0)
                var wave = Path()
                for (index, sample) in samples.enumerated() {
                    let point = CGPoint(x: 5 + Double(index) * 230 / 127, y: 43 - sample / amplitude * 17)
                    if index == 0 { wave.move(to: point) } else { wave.addLine(to: point) }
                }
                context.stroke(wave, with: .color(theme.accent.opacity(0.16)), lineWidth: 3)
                context.stroke(wave, with: .color(theme.accent), lineWidth: 0.8)
                }
                if model.navigation.page == 4 {
                    let status = model.unit.firmwareArpStatus
                    let running = status & 0x80000000 != 0
                    let step = Int((status >> 8) & 0x7fffff)
                    text(model.arpAvailable ? "\(running ? "PLAY" : "STOP") \(model.tempo.formatted(.number.precision(.fractionLength(0...1)))) BPM / STEP \(step)" : "ARP NOT SUPPORTED", x: 5, y: 16, size: 6, color: theme.alternate)
                    for index in 0..<8 {
                        context.fill(Path(CGRect(x: 5 + index * 29, y: 66, width: 25, height: 3)), with: .color(running && step > 0 && (step - 1) % 8 == index ? theme.accent : theme.surface))
                    }
                } else if model.navigation.page == 3 {
                    let shape = SynthParameter.lfoShape.formatted(model.lfoShape, shapes: [])
                    text(model.lfoShapesAvailable ? "\(shape) / \(model.lfoSync >= 0.5 ? "SYNC" : "FREE") / \(model.lfoShape.rounded() == 4 ? "HISTORY + DEPTH" : "AUDIO PHASE + DEPTH")" : "LEGACY / NO PHASE MONITOR", x: 5, y: 16, size: 6, color: theme.chrome)
                } else {
                    text("OUT / AUTO", x: 5, y: 16, size: 5.5, color: theme.chrome)
                }
                for (index, parameter) in model.navigation.bindings.enumerated() {
                    let x = Double(index % 4) * 60 + 5
                    let y = Double(index / 4) * 29 + 75
                    let supported = parameter.map(model.supports) ?? false
                    let active = supported && (model.navigation.editing ? model.navigation.selected == index : model.activeParameter == parameter)
                    if active {
                        context.fill(Path(roundedRect: CGRect(x: x - 2, y: y - 2, width: 57, height: 26), cornerRadius: 2), with: .color(theme.accent.opacity(0.13)))
                    }
                    let label = supported ? model.navigation.label(index, model: model) : "UNUSED"
                    text("\(index + 1) \(label)", x: x, y: y, size: 6.5, color: active ? theme.accent : theme.chrome, bold: true)
                    text(supported ? parameter!.formatted(model.screenValue(parameter!), shapes: model.shapeLabels) : "—", x: x, y: y + 10, size: 8, color: supported ? theme.text : theme.chrome, bold: true)
                }
            }.id(model.navigation.page)
        }
        .task(id: "\(model.navigation.page)-\(Int(model.lfoShape.rounded()))-\(model.firmwareChoice)") {
            guard model.navigation.page == 3 else { return }
            lfoHistory = Array(repeating: 0, count: 96)
            while !Task.isCancelled {
                do { try await Task.sleep(for: .milliseconds(33)) } catch { return }
                if scenePhase == .active && model.lfoShape.rounded() == 4 {
                    let raw = Double(Int16(bitPattern: UInt16(model.unit.firmwareLFOStatus & 65535))) / 32767
                    lfoHistory.removeFirst(); lfoHistory.append(raw)
                }
            }
        }
        .background(theme.background).clipShape(.rect(cornerRadius: 10))
        .overlay { RoundedRectangle(cornerRadius: 10).strokeBorder(theme.chrome.opacity(0.25), lineWidth: 1) }
        .accessibilityElement(children: .ignore)
        .accessibilityLabel("\(NavigationModel.titles[model.navigation.page]), \(model.firmwareName)")
        .accessibilityValue(model.navigation.page == 1 ? "Global menu, section \(model.navigation.section), theme \(SynthTheme.all[model.navigation.theme].name), tempo \(model.tempo.formatted(.number.precision(.fractionLength(1)))) BPM" : model.navigation.bindings.enumerated().map { index, parameter in
            guard let parameter, model.supports(parameter) else { return "Knob \(index + 1): unused" }
            return "Knob \(index + 1): \(model.navigation.label(index, model: model)) \(parameter.formatted(model.value(parameter), shapes: model.shapeLabels))"
        }.joined(separator: ", "))
    }
}
