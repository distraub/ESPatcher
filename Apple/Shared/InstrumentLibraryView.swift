import SwiftUI

struct InstrumentLibraryView: View {
    let model: SynthModel
    let openFile: () -> Void
    let done: () -> Void
    @Environment(\.synthTheme) private var theme
    private let entries = [("amsynth", "AMSynth", "Analog and FM · four screen layouts"),
                           ("orbit-study", "Orbit / UI study", "Bell clouds · custom controls and scale pads"),
                           ("signal-check", "Signal Check", "Minimal Tone and Pulse engines")]
    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            HStack {
                Text("Instrument Library").font(.title2.bold())
                Spacer()
                Button("Done", action: done).keyboardShortcut(.cancelAction)
            }
            Text("Load an instrument, then select an engine from the header. Its screens, knobs and pad layout come from the package.")
                .font(.callout)
            ForEach(entries, id: \.0) { entry in
                Button {
                    Task {
                        await model.loadBundledPackage(entry.0)
                        if model.firmwareError == nil { done() }
                    }
                } label: {
                    VStack(alignment: .leading, spacing: 5) {
                        Text(entry.1).font(.headline)
                        Text(entry.2).font(.caption).foregroundStyle(.secondary)
                    }
                    .frame(maxWidth: .infinity, alignment: .leading).padding(12)
                }
                .buttonStyle(.bordered)
            }
            Button("Open .espkg package…", systemImage: "folder", action: openFile)
            if model.firmwareLoading { ProgressView("Preparing instrument…") }
            if let error = model.firmwareError { Text(error).font(.callout).foregroundStyle(theme.alternate) }
            Text("L always opens Library. Package UIs currently use bounded scene layouts; physical ADV performance is still unverified.")
                .font(.caption).foregroundStyle(.secondary)
        }
        .padding(24).frame(minWidth: 360, idealWidth: 480, maxWidth: 520)
        .disabled(model.firmwareLoading)
        .background(theme.background).foregroundStyle(theme.text).tint(theme.accent)
        .preferredColorScheme(theme.isLight ? .light : .dark)
    }
}
