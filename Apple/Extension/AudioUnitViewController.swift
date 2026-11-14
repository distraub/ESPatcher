import CoreAudioKit
import SwiftUI

extension GSSynthAudioUnit: @unchecked Sendable {}

public final class AudioUnitViewController: AUViewController, AUAudioUnitFactory {
    private var unit: GSSynthAudioUnit?
    public nonisolated func createAudioUnit(with description: AudioComponentDescription) throws -> AUAudioUnit {
        let audio = try GSSynthAudioUnit(componentDescription: description, options: [])
        Task { @MainActor [weak self] in self?.unit = audio; self?.installEditor() }
        return audio
    }
    public override func viewDidLoad() { super.viewDidLoad(); installEditor() }
    private func installEditor() {
        guard isViewLoaded, children.isEmpty, let unit else { return }
        #if os(macOS)
        let host = NSHostingController(rootView: SynthView(unit: unit))
        #else
        let host = UIHostingController(rootView: SynthView(unit: unit))
        #endif
        addChild(host); host.view.translatesAutoresizingMaskIntoConstraints = false; view.addSubview(host.view)
        NSLayoutConstraint.activate([
            host.view.leadingAnchor.constraint(equalTo: view.leadingAnchor), host.view.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            host.view.topAnchor.constraint(equalTo: view.topAnchor), host.view.bottomAnchor.constraint(equalTo: view.bottomAnchor)
        ])
        #if !os(macOS)
        host.didMove(toParent: self)
        #endif
    }
}
