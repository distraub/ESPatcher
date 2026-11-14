import SwiftUI
import UniformTypeIdentifiers

/// A compact performance header; management details appear only on demand.
struct FirmwarePicker: View {
    @Bindable var model: SynthModel
    @Environment(\.synthTheme) private var theme
    @State private var importing = false
    @State private var managing = false
    @State private var importAfterDismiss = false

    var body: some View {
        HStack(spacing: 12) {
            Text("ESPatcher").font(.title3.bold()).fixedSize()
            instrumentPicker.frame(maxWidth: 300)
            Spacer(minLength: 0)
            if model.firmwareLoading {
                ProgressView().controlSize(.small).accessibilityLabel("Preparing instrument")
            }
            Button {
                model.panic()
                managing = true
            } label: {
                Label(model.firmwareError == nil ? "Firmware…" : "Firmware error…",
                      systemImage: model.firmwareError == nil ? "slider.horizontal.3" : "exclamationmark.triangle")
            }
        }
        .frame(minHeight: 36)
        .sheet(isPresented: $managing, onDismiss: {
            // Present the file picker only after the management sheet has closed.
            if importAfterDismiss {
                importAfterDismiss = false
                importing = true
            }
        }) {
            ScrollView { management }
                .frame(idealWidth: 480, idealHeight: 620)
                .foregroundStyle(theme.text).tint(theme.accent)
                .preferredColorScheme(theme.isLight ? .light : .dark)
        }
        .onChange(of: model.firmwareChoice) { _, choice in
            guard !model.firmwareLoading, model.unit.firmwareID != choice else { return }
            Task { await model.loadBundled(choice) }
        }
        .task {
            if model.unit.firmwareID == "native" { await model.loadBundled("pocket_analog") }
        }
        .task {
            while !Task.isCancelled {
                do { try await Task.sleep(for: .milliseconds(250)) } catch { return }
                model.refreshFirmwareStatus()
            }
        }
        .fileImporter(isPresented: $importing, allowedContentTypes: [.data]) { result in
            switch result {
            case .success(let url): Task { await model.loadFile(url) }
            case .failure(let error): model.firmwareError = error.localizedDescription
            }
        }
    }

    private var instrumentPicker: some View {
        Picker("Instrument", selection: $model.firmwareChoice) {
            Text("AMSynth · Analog").tag("pocket_analog")
            Text("AMSynth · FM").tag("fm_bell")
            if !["pocket_analog", "fm_bell"].contains(model.firmwareChoice) {
                Text(model.firmwareName).tag(model.firmwareChoice)
            }
        }
        .labelsHidden()
        .disabled(model.firmwareLoading)
        .accessibilityLabel("Instrument")
    }

    private var management: some View {
        VStack(alignment: .leading, spacing: 18) {
            HStack {
                Text("Firmware").font(.title2.bold())
                Spacer()
                Button("Done") { managing = false }.keyboardShortcut(.cancelAction)
            }
            VStack(alignment: .leading, spacing: 12) {
                Text(model.firmwareName).font(.headline)
                Text("Switch instrument").font(.caption).foregroundStyle(.secondary)
                instrumentPicker
                Button("Load custom firmware…", systemImage: "folder") {
                    model.panic()
                    importAfterDismiss = true
                    managing = false
                }
                .disabled(model.firmwareLoading)
                if model.firmwareLoading { ProgressView("Preparing instrument…") }
                if let error = model.firmwareError {
                    Text(error).font(.callout).foregroundStyle(theme.alternate)
                        .textSelection(.enabled)
                }
            }
            .padding(16).frame(maxWidth: .infinity, alignment: .leading)
            .background(theme.surface).clipShape(.rect(cornerRadius: 12))
            MIDILearnView(model: model)
            Text("ESPatcher runs portable .gsm5 instrument modules on this computer or device.")
                .font(.callout)
            Text("MODULE FORMAT\n.gsm5 · ABI 1 · isolated 256 KiB memory")
                .font(.caption.monospaced()).foregroundStyle(.secondary)
            Text("ESPatcher · Alex Matheu / Instrument Lab")
                .font(.caption).foregroundStyle(.secondary)
        }
        .padding(24).frame(minWidth: 340, idealWidth: 460, maxWidth: 500)
        .background(theme.background)
    }
}
