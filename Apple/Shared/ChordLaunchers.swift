import SwiftUI

struct ChordAssignment {
    var root = 0
    var quality: Int
    var inversion = 0
    var spread = false
    static let roots = ["C", "C♯", "D", "D♯", "E", "F", "F♯", "G", "G♯", "A", "A♯", "B"]
    static let qualities = ["Major", "Minor", "7", "m7"]
    static let intervals = [[0, 4, 7], [0, 3, 7], [0, 4, 7, 10], [0, 3, 7, 10]]
    var name: String { Self.roots[root] + " " + Self.qualities[quality] }
    var notes: [Int] {
        var result = Self.intervals[quality]
        for _ in 0..<min(inversion, result.count - 1) { result.append(result.removeFirst() + 12) }
        if spread { result[1] += 12 }
        return result.map { $0 + root }
    }
}

struct ChordLaunchers: View {
    let model: SynthModel
    @Environment(\.synthTheme) private var theme
    @State private var editing = 0
    var body: some View {
        @Bindable var model = model
        VStack(alignment: .leading, spacing: 8) {
            HStack {
                Text("CHORD MODE").font(.caption.monospaced().bold())
                Button("Off") { model.selectChordMode(nil) }
                    .tint(model.chordMode == nil ? theme.accent : theme.chrome)
                ForEach(0..<4) { slot in
                    Button {
                        model.selectChordMode(slot)
                    } label: {
                        Text("\(SynthModel.chordKeys[slot]) · \(ChordAssignment.qualities[slot])")
                            .font(.callout.monospaced().bold())
                            .frame(maxWidth: .infinity, minHeight: 36)
                    }
                    .buttonStyle(.bordered)
                    .tint(model.chordMode == slot ? theme.accent : theme.chrome)
                    .accessibilityValue(model.chordMode == slot ? "Selected" : "Off")
                }
            }
            DisclosureGroup("Chord voicing · play any note or pad to set the root") {
                HStack {
                    Picker("Chord", selection: $editing) {
                        ForEach(0..<4) { slot in Text(ChordAssignment.qualities[slot]).tag(slot) }
                    }
                    Picker("Inversion", selection: $model.chords[editing].inversion) {
                        Text("Root").tag(0); Text("First").tag(1); Text("Second").tag(2)
                    }
                    Toggle("Spread", isOn: $model.chords[editing].spread)
                }.padding(.top, 6)
            }.font(.caption)
        }.padding(12).background(theme.surface).clipShape(.rect(cornerRadius: 10))
        .onChange(of: model.chords.map { $0.notes }) { _, _ in model.panic() }
    }
}
