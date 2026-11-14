import SwiftUI

struct InstrumentControls: View {
    let model: SynthModel
    var body: some View {
        ViewThatFits(in: .horizontal) {
            HStack { octave; Spacer(); performance }
            VStack(spacing: 12) { octave; performance }
        }
    }
    private var octave: some View {
        HStack {
            Button("Octave −") { model.shift(-1) }
            Text("\(model.octave)").monospacedDigit()
            Button("Octave +") { model.shift(1) }
        }
    }
    private var performance: some View {
        HStack {
            Button(model.arpRunning >= 0.5 ? "− Stop" : "− Play") { model.toggleArp() }.disabled(!model.arpAvailable)
            Button(model.testChordHeld ? "Release chord" : "Test chord") { model.toggleTestChord() }
            Button("Panic") { model.panic() }
        }
    }
}
