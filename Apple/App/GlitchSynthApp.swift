import SwiftUI

@main
struct ESPatcherApp: App {
    @State private var host = AudioHost()
    var body: some Scene {
        WindowGroup("ESPatcher") {
            Group {
                if let unit = host.unit { SynthView(unit: unit) }
                else if let error = host.error { ContentUnavailableView("Audio could not start", systemImage: "speaker.slash", description: Text(error)) }
                else { ProgressView("Starting synth…") }
            }
            #if os(macOS)
            .frame(minWidth: 1024, minHeight: 600)
            #else
            .frame(minWidth: 360, minHeight: 600)
            #endif
            .task { await host.start() }
        }
        #if os(macOS)
        .defaultSize(width: 1120, height: 820)
        #endif
    }
}
