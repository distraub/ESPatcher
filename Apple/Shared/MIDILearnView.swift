import SwiftUI

struct MIDILearnView: View {
    let model: SynthModel
    @State private var target: SynthParameter = .cutoff
    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("MIDI & Learn").font(.headline)
            Text("Standalone: connected MIDI sources and the ESPatcher virtual input. AUv3: route MIDI from your host.").font(.caption)
            Picker("Parameter", selection: $target) {
                ForEach(SynthParameter.allCases.filter { model.supports($0) }) { parameter in
                    Text(parameter.title + ([SynthParameter.lfoRate, .arpRate].contains(parameter) ? " (Hz)" : [SynthParameter.lfoDivision, .arpDivision].contains(parameter) ? " (sync)" : "")).tag(parameter)
                }
            }
            HStack {
                Button("Learn next CC") { model.unit.beginMIDILearn(UInt32(target.rawValue)) }
                Button("Cancel") { model.unit.cancelMIDILearn() }
                Button("Forget") { model.unit.clearMIDIMapping(UInt32(target.rawValue)) }
            }
            TimelineView(.periodic(from: .now, by: 0.25)) { _ in
                let entry = model.unit.midiMapping(UInt32(target.rawValue))
                let learning = model.unit.midiLearnTarget
                Text(learning == -2 ? "Mapping table full (16). Forget a mapping first." : learning >= 0 ? "Move a controller: waiting for a 7-bit CC…" : entry == 0 ? "No mapping for this parameter" : "Channel \(((entry >> 7) & 15) + 1) · CC \(entry & 127)")
                    .font(.caption.monospaced()).accessibilityAddTraits(.updatesFrequently)
            }
            Text("Mappings follow parameters across screens. The first CC binds without changing the sound. Later values are absolute; relative encoders and pickup are future work. Mappings save with AU sessions.").font(.caption).foregroundStyle(.secondary)
        }
        .onDisappear { model.unit.cancelMIDILearn() }
    }
}
