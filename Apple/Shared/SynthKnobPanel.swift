import SwiftUI

struct SynthKnobPanel: View {
    let model: SynthModel
    @Environment(\.synthTheme) private var theme
    @ScaledMetric(relativeTo: .body) private var diameter = 72.0
    private let columns = Array(repeating: GridItem(.flexible(minimum: 92), spacing: 12), count: 8)
    var body: some View {
        LazyVGrid(columns: columns, spacing: 12) {
            ForEach(0..<8) { slot in
                if let parameter = model.navigation.bindings[slot] {
                    SynthKnob(parameter: parameter, model: model, slot: slot)
                        .id("\(model.navigation.page)-\(slot)-\(parameter.rawValue)")
                        .disabled(!model.supports(parameter))
                } else {
                    VStack(spacing: 4) {
                        Text("\(slot + 1) UNUSED").font(.caption.monospaced().bold()).frame(minHeight: 20)
                        ZStack {
                            StudioKnobFace(tint: theme.accent)
                            StudioKnobIndicator(fraction: 0, tint: theme.accent)
                        }.frame(width: diameter, height: diameter)
                        Text("—").font(.body.monospaced().bold()).frame(minHeight: 24)
                    }.padding(.vertical, 2).frame(maxWidth: .infinity, minHeight: 126)
                    .opacity(0.3).allowsHitTesting(false)
                    .accessibilityLabel("Knob \(slot + 1), unused")
                }
            }
        }
        .padding(12).background(theme.surface).clipShape(.rect(cornerRadius: 10))
    }
}
