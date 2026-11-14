import SwiftUI

struct NoteKey: View {
    @Environment(\.synthTheme) private var theme
    let index: Int
    let model: SynthModel
    @State private var pressed = false
    private static let names = ["C", "C♯", "D", "D♯", "E", "F", "F♯", "G", "G♯", "A", "A♯", "B", "C"]

    var body: some View {
        let black = [1, 3, 6, 8, 10].contains(index)
        Text(Self.names[index])
            .font(.system(.caption, design: .monospaced))
            .frame(maxWidth: .infinity, minHeight: 90)
            .foregroundStyle(pressed ? theme.inverse : theme.text)
            .background(pressed ? theme.accent : (black ? theme.background : theme.surface))
            .clipShape(.rect(cornerRadius: 5))
            .contentShape(.rect)
            .gesture(DragGesture(minimumDistance: 0)
                .onChanged { _ in
                    if !pressed { pressed = true; model.down(index, source: "touch\(index)") }
                }
                .onEnded { _ in pressed = false; model.up("touch\(index)") })
            .accessibilityLabel("Play \(Self.names[index])")
            .accessibilityAddTraits(.isButton)
            .accessibilityAction {
                model.down(index, source: "accessible\(index)")
                Task { @MainActor in
                    try? await Task.sleep(for: .milliseconds(300))
                    model.up("accessible\(index)")
                }
            }
            .onDisappear { pressed = false; model.up("touch\(index)") }
    }
}
