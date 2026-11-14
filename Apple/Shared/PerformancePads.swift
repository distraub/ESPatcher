import SwiftUI

/// Virtual counterpart of a 4×4 hardware pad controller. Notes rise left to right,
/// starting at the bottom row, with the same note ownership as the keyboard.
struct PerformancePads: View {
    let model: SynthModel
    let height: CGFloat
    @Environment(\.synthTheme) private var theme
    private let columns = Array(repeating: GridItem(.flexible(), spacing: 8), count: 4)

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("PADS · NOTES").font(.caption.monospaced().bold())
            LazyVGrid(columns: columns, spacing: 8) {
                ForEach(0..<16) { position in
                    let note = model.packagePadNotes?[position] ?? ((3 - position / 4) * 4 + position % 4)
                    PerformancePad(index: note, slot: position, model: model, height: (height - 91) / 4)
                }
            }
            Text(model.packageInfo == nil ? "Chromatic · lowest note at bottom left" : "Package note layout · lowest note at bottom left")
                .font(.caption2.monospaced()).foregroundStyle(.secondary)
        }
        .padding(14).frame(width: 344, height: height)
        .background(theme.surface).clipShape(.rect(cornerRadius: 12))
    }
}

private struct PerformancePad: View {
    let index: Int
    let slot: Int
    let model: SynthModel
    let height: CGFloat
    @Environment(\.synthTheme) private var theme
    @Environment(\.scenePhase) private var scenePhase
    @State private var held = false
    @State private var accessibleRelease: Task<Void, Never>?
    private static let names = ["C", "C♯", "D", "D♯", "E", "F", "F♯", "G", "G♯", "A", "A♯", "B"]
    private var source: String { "pad\(slot)" }
    private var name: String { Self.names[index % 12] + "\(model.octave + index / 12)" }
    private var sounding: Bool { model.notes.values.contains(12 * (model.octave + 1) + index) }
    private var root: Bool { index % 12 == 0 }

    var body: some View {
        Text(name)
            .font(.body.monospaced().bold())
            .frame(maxWidth: .infinity).frame(height: height)
            .foregroundStyle(sounding ? theme.inverse : theme.text)
            .background(sounding ? theme.accent : theme.accent.opacity(root ? 0.32 : 0.10))
            .clipShape(.rect(cornerRadius: 12))
            .overlay(RoundedRectangle(cornerRadius: 12).strokeBorder(theme.accent.opacity(sounding ? 1 : root ? 0.65 : 0.22), lineWidth: 2))
            .contentShape(.rect)
            .gesture(DragGesture(minimumDistance: 0)
                .onChanged { _ in
                    guard !held else { return }
                    accessibleRelease?.cancel()
                    held = true
                    model.down(index, source: source)
                }
                .onEnded { _ in release() })
            .accessibilityLabel("Play \(name)")
            .accessibilityAddTraits(.isButton)
            .accessibilityValue(sounding ? "Held" : "")
            .accessibilityAction {
                release()
                held = true
                model.down(index, source: source)
                accessibleRelease = Task { @MainActor in
                    do { try await Task.sleep(for: .milliseconds(300)) } catch { return }
                    release()
                }
            }
            .onChange(of: scenePhase) { _, phase in if phase != .active { release() } }
            .onChange(of: model.editorGeneration) { _, _ in release() }
            .onChange(of: model.octave) { _, _ in release() }
            .onChange(of: model.navigation.page) { _, _ in release() }
            .onDisappear(perform: release)
    }

    private func release() {
        accessibleRelease?.cancel(); accessibleRelease = nil
        held = false
        model.up(source)
    }
}
