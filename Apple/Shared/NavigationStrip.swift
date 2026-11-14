import SwiftUI
struct NavigationStrip: View {
    let model: SynthModel
    @Environment(\.synthTheme) private var theme
    var body: some View {
        HStack {
            ForEach(0..<5) { page in
                Button("\(["C", "V", "B", "N", "M"][page]) · \(NavigationModel.titles[page])") { model.navigation.go(page, model: model) }
                    .buttonStyle(.bordered)
                    .tint(model.navigation.page == page ? theme.accent : theme.chrome)
                    .keyboardShortcut(KeyEquivalent(Character(String(page + 1))), modifiers: .command)
            }
            Spacer()
            Text("C/V/B/N/M · − Play · 1–8 + arrows")
                .font(.caption.monospaced()).foregroundStyle(.secondary)
        }
    }
}
