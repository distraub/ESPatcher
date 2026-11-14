import SwiftUI

struct PackageInstrumentScreen: View {
    let model: SynthModel
    @Environment(\.synthTheme) private var theme
    @Environment(\.scenePhase) private var scenePhase

    var body: some View {
        Group {
            if model.navigation.page == 1 { global }
            else if let scene = model.packageScene {
                TimelineView(.animation(minimumInterval: 1.0 / 30, paused: scenePhase != .active)) { _ in
                    Canvas { context, size in
                        context.scaleBy(x: size.width / 240, y: size.height / 135)
                        func text(_ value: String, _ x: Double, _ y: Double, _ font: Double, _ color: Color) {
                            context.draw(Text(value).font(.system(size: font, weight: .bold, design: .monospaced)).foregroundStyle(color), at: CGPoint(x: x, y: y), anchor: .topLeading)
                        }
                        context.fill(Path(CGRect(x: 0, y: 0, width: 240, height: 135)), with: .color(theme.background))
                        let title = scene.title + " / " + (model.packageInfo?.engine.name ?? "")
                        text(title.uppercased(), 5, 4, min(9, 170 / (Double(max(1, title.count)) * 0.62)), theme.text)
                        text("\(model.activeVoices)/\(model.voiceLimit) O\(model.octave)", 184, 5, 7, theme.alternate)
                        text(scene.subtitle.uppercased(), 5, 18, 6, theme.chrome)
                        if scene.layout == 0 {
                            let samples = model.unit.scopeSnapshot().map(\.doubleValue)
                            let amplitude = max(0.02, samples.map { abs($0) }.max() ?? 0.02)
                            var path = Path()
                            for i in samples.indices {
                                let point = CGPoint(x: 5 + Double(i) * 230 / 127, y: 48 - samples[i] / amplitude * 18)
                                if i == 0 { path.move(to: point) } else { path.addLine(to: point) }
                            }
                            context.stroke(path, with: .color(theme.accent), lineWidth: 0.8)
                        } else if scene.layout == 1 {
                            let status = model.unit.firmwareLFOStatus
                            let raw = Double(Int16(bitPattern: UInt16(status & 65535))) / 32767
                            let shape = Int(model.screenValue(.lfoShape).rounded()), depth = model.screenValue(.lfoDepth)
                            var path = Path()
                            for i in 0..<128 {
                                let value = shape == 4 ? raw : LFOWaveform.value(phase: Double(i) / 127, shape: shape)
                                let point = CGPoint(x: 5 + Double(i) * 230 / 127, y: 48 - value * depth * 18)
                                if i == 0 { path.move(to: point) } else { path.addLine(to: point) }
                            }
                            context.stroke(path, with: .color(theme.accent), lineWidth: 0.8)
                            let x = 5 + Double(status >> 16) * 230 / 65536
                            var cursor = Path(); cursor.move(to: CGPoint(x: x, y: 28)); cursor.addLine(to: CGPoint(x: x, y: 68))
                            context.stroke(cursor, with: .color(theme.alternate), lineWidth: 0.6)
                        } else if scene.layout == 3 {
                            for slot in 0..<8 {
                                guard let parameter = model.navigation.bindings[slot] else { continue }
                                let height = parameter.normalized(model.screenValue(parameter)) * 35
                                context.fill(Path(CGRect(x: 5 + Double(slot) * 29, y: 67 - height, width: 23, height: max(1, height))), with: .color(theme.accent))
                            }
                        }
                        for slot in 0..<8 {
                            let x = 4 + Double(slot % 4) * 60
                            let y = (scene.layout == 2 ? 45.0 : 77.0) + Double(slot / 4) * 27
                            text("\(slot + 1) \(scene.labels[slot])", x, y, 6.5, slot == model.navigation.selected ? theme.accent : theme.chrome)
                            let parameter = model.navigation.bindings[slot]
                            text(parameter.map { $0.formatted(model.screenValue($0), shapes: model.shapeLabels) } ?? "—", x, y + 11, 8, theme.text)
                        }
                        text("L LIBRARY · 1–8 + ARROWS", 5, 125, 5.5, theme.chrome)
                    }
                }
                .id("\(model.editorGeneration)-\(model.navigation.page)")
                .accessibilityElement(children: .ignore)
                .accessibilityLabel("\(scene.title), \(model.packageInfo?.name ?? "Instrument")")
                .accessibilityValue(model.navigation.bindings.enumerated().map { slot, parameter in
                    "Knob \(slot + 1): \(scene.labels[slot]) \(parameter.map { $0.formatted(model.value($0), shapes: model.shapeLabels) } ?? "unused")"
                }.joined(separator: ", "))
            }
        }
        .background(theme.background).clipShape(.rect(cornerRadius: 10))
        .overlay { RoundedRectangle(cornerRadius: 10).strokeBorder(theme.chrome.opacity(0.25), lineWidth: 1) }
    }

    private var global: some View {
        @Bindable var navigation = model.navigation
        @Bindable var model = model
        return VStack(alignment: .leading, spacing: 20) {
            Text("GLOBAL / ESPatcher").font(.headline.monospaced())
            Picker("Theme", selection: $navigation.theme) {
                ForEach(SynthTheme.all.indices, id: \.self) { index in Text(SynthTheme.all[index].name).tag(index) }
            }
            Stepper("Tempo \(model.tempo.formatted(.number.precision(.fractionLength(1)))) BPM", value: $model.tempo, in: 40...300, step: 1)
            Button("Instrument Library") { model.openLibrary() }
            Text("Screens and controls belong to the instrument. Library and global settings remain available in ESPatcher.").font(.caption).foregroundStyle(.secondary)
        }
        .padding(24).frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
        .onChange(of: navigation.theme) { _, _ in navigation.saveRevision += 1 }
    }
}
