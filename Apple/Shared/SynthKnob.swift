import SwiftUI

/// GlitchDrums VoiceParameterKnob gesture/face, rebound to the synth AU.
struct SynthKnob: View {
    let parameter: SynthParameter
    let model: SynthModel
    let slot: Int
    @Environment(\.synthTheme) private var theme
    @Environment(\.isEnabled) private var enabled
    @Environment(\.scenePhase) private var scenePhase
    @ScaledMetric(relativeTo: .body) private var diameter = 72.0
    @State private var dragGeneration: UInt32?
    @State private var dragStartValue: Double?
    @State private var dragValue: Double?
    private var value: Double { dragValue ?? model.value(parameter) }
    private var title: String { model.packageScene?.labels[slot] ?? (parameter == .shape ? model.shapeTitle : parameter.title) }
    private var readout: String { parameter.formatted(value, shapes: model.shapeLabels) }
    var body: some View {
        VStack(spacing: 4) {
            Text("\(slot + 1) " + title.uppercased()).font(.caption.monospaced().bold())
                .kerning(0.4).foregroundStyle(theme.text.opacity(0.78))
                .lineLimit(1).minimumScaleFactor(0.6).frame(minHeight: 20)
            ZStack {
                // The decorative layers ignore input; keep an explicit drag surface.
                Circle().fill(Color.clear).contentShape(.circle)
                StudioKnobFace(tint: theme.accent)
                StudioKnobIndicator(fraction: parameter.normalized(value), tint: theme.accent, isEditing: dragValue != nil)
            }
            .frame(width: diameter, height: diameter).contentShape(.circle)
            .highPriorityGesture(drag.simultaneously(with: TapGesture(count: 2).onEnded(reset)))
            Text(readout).font(.body.monospaced().bold()).foregroundStyle(theme.accent)
                .opacity(dragValue == nil ? 0.86 : 1).lineLimit(1).minimumScaleFactor(0.62).frame(minHeight: 24)
        }
        .padding(.vertical, 2).frame(maxWidth: .infinity, minHeight: 126)
        .opacity(enabled ? 1 : 0.4).allowsHitTesting(enabled)
        .accessibilityElement(children: .ignore).accessibilityLabel(title)
        .accessibilityValue(enabled ? readout : "Not supported by this firmware")
        .accessibilityHint("Drag up or down to adjust. Double tap to reset.")
        .accessibilityAdjustableAction { direction in
            guard enabled else { return }
            let step = parameter.isDiscrete ? 1 / (parameter.range.upperBound - parameter.range.lowerBound) : 0.02
            let next = parameter.denormalized(parameter.normalized(value) + (direction == .increment ? step : -step))
            model.preview(parameter, value: next); model.commit(parameter, value: next)
        }
        .accessibilityAction(named: "Reset", reset)
        .onDisappear(perform: finish)
        .onChange(of: scenePhase) { _, phase in if phase != .active { finish() } }
    }
    private var drag: some Gesture {
        DragGesture(minimumDistance: 0)
            .onChanged { gesture in
                if dragStartValue == nil { dragStartValue = value; dragGeneration = model.editorGeneration }
                let fraction = parameter.normalized(dragStartValue ?? value) - Double(gesture.translation.height) / 180
                let next = parameter.denormalized(fraction)
                dragValue = next; model.preview(parameter, value: next)
            }
            .onEnded { _ in finish() }
    }
    private func finish() {
        if let dragValue, dragGeneration == model.editorGeneration { model.commit(parameter, value: dragValue) }
        dragValue = nil; dragStartValue = nil; dragGeneration = nil
    }
    private func reset() {
        guard enabled else { return }
        dragValue = nil; dragStartValue = nil; dragGeneration = nil
        model.preview(parameter, value: parameter.defaultValue)
        model.commit(parameter, value: parameter.defaultValue)
    }
}
