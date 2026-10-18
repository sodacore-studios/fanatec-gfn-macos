// SPDX-License-Identifier: GPL-2.0-only
import Cocoa
import UniformTypeIdentifiers

final class Controller: NSObject, NSApplicationDelegate {
    let root = Installer.runtime
    var window: NSWindow!
    var process: Process?
    var quitting = false
    var installing = false
    let installButton = NSButton(title: "Install / repair GFN bridge…", target: nil, action: nil)
    let openButton = NSButton(title: "Open GeForce NOW", target: nil, action: nil)
    var log: FileHandle?
    var buffer = ""
    let state = NSTextField(wrappingLabelWithString: "Ready. Force feedback is off.")
    let gain = NSPopUpButton(frame: .zero, pullsDown: false)
    let experimental = NSButton(checkboxWithTitle: "Enable experimental bases (community testing)", target: nil, action: nil)
    let duration = NSPopUpButton(frame: .zero, pullsDown: false)
    let start = NSButton(title: "Start force feedback", target: nil, action: nil)
    let stop = NSButton(title: "STOP", target: nil, action: nil)
    func applicationDidFinishLaunching(_ notification: Notification) {
        window = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 620, height: 520), styleMask: [.titled, .closable, .miniaturizable], backing: .buffered, defer: false)
        window.title = "Fanatec Force Feedback"
        window.center()
        let stack = NSStackView()
        stack.orientation = .vertical; stack.alignment = .leading; stack.spacing = 16
        stack.translatesAutoresizingMaskIntoConstraints = false
        window.contentView!.addSubview(stack)
        NSLayoutConstraint.activate([stack.leadingAnchor.constraint(equalTo: window.contentView!.leadingAnchor, constant: 24), stack.trailingAnchor.constraint(equalTo: window.contentView!.trailingAnchor, constant: -24), stack.topAnchor.constraint(equalTo: window.contentView!.topAnchor, constant: 24)])
        let title = NSTextField(labelWithString: "Fanatec · GeForce NOW")
        title.font = .boldSystemFont(ofSize: 22)
        stack.addArrangedSubview(title)
        let help = NSTextField(wrappingLabelWithString: "Use native PC mode and connect one base. CSL DD has physical test results; other profiles are experimental. Start community tests at 3%.")
        help.preferredMaxLayoutWidth = 550
        stack.addArrangedSubview(help)
        installButton.target = self; installButton.action = #selector(installBridge)
        let diagnose = NSButton(title: "Export device diagnostics…", target: self, action: #selector(exportDiagnostics))
        stack.addArrangedSubview(NSStackView(views: [installButton, diagnose]))
        do { try FileManager.default.createDirectory(at:root,withIntermediateDirectories:true) }
        catch { state.stringValue = "Cannot create runtime directory: \(error.localizedDescription)" }
        gain.addItems(withTitles: ["3 %", "10 %", "25 %", "50 %", "75 %", "100 %"]); gain.selectItem(at: 5)
        duration.addItems(withTitles: ["Unlimited", "30 seconds", "5 minutes", "15 minutes", "30 minutes", "60 minutes"]); duration.selectItem(at: 0)
        let settings = NSStackView(views: [NSTextField(labelWithString: "Passthrough:"), gain, NSTextField(labelWithString: "Duration:"), duration])
        settings.spacing = 10; stack.addArrangedSubview(settings)
        experimental.target = self; experimental.action = #selector(experimentalChanged)
        stack.addArrangedSubview(experimental)
        openButton.target = self; openButton.action = #selector(openGame)
        start.target = self; start.action = #selector(begin)
        stop.target = self; stop.action = #selector(end); stop.isEnabled = false
        let buttons = NSStackView(views: [openButton, start, stop]); buttons.spacing = 8
        stack.addArrangedSubview(buttons)
        state.preferredMaxLayoutWidth = 550; stack.addArrangedSubview(state)
        let hint = NSTextField(wrappingLabelWithString: "Game forces are forwarded immediately. Unlimited runs until stopped or closed. The data-loss watchdog remains active. Power off the base if behavior is unexpected.")
        hint.preferredMaxLayoutWidth = 550; hint.font = .systemFont(ofSize: 11); hint.textColor = .secondaryLabelColor
        stack.addArrangedSubview(hint)
        switch Installer.managedAppState() {
        case .missing: state.stringValue = "Setup required. Click Open GeForce NOW to prepare your local copy."; start.isEnabled = false
        case .needsRepair: state.stringValue = "GFN copy needs repair. Click Open GeForce NOW to repair it."; start.isEnabled = false
        case .ready: break
        }
        window.makeKeyAndOrderFront(nil); NSApp.activate(ignoringOtherApps: true)
    }
    @objc func experimentalChanged() {
        if experimental.state == .on { gain.selectItem(at: 0); duration.selectItem(at: 1) }
    }
    @objc func exportDiagnostics() {
        guard process == nil, !installing else { state.stringValue = "Stop force feedback before exporting diagnostics."; return }
        let panel = NSSavePanel(); panel.nameFieldStringValue = "Fanatec-device-diagnostics.txt"
        panel.allowedContentTypes = [.plainText]
        guard panel.runModal() == .OK, let destination = panel.url else { return }
        installing = true; start.isEnabled = false; installButton.isEnabled = false; openButton.isEnabled = false
        state.stringValue = "Reading device descriptors. No motor commands are sent…"
        let helper = Bundle.main.resourceURL!.appendingPathComponent("FFBReceiver")
        let version = Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? "unknown"
        DispatchQueue.global(qos: .userInitiated).async {
            let result: String
            do {
                let task = Process(); task.executableURL = helper; task.arguments = ["--diagnose"]
                let pipe = Pipe(); task.standardOutput = pipe; task.standardError = pipe
                try task.run()
                let data = pipe.fileHandleForReading.readDataToEndOfFile(); task.waitUntilExit()
                let text = "Fanatec GFN \(version)\n\(ProcessInfo.processInfo.operatingSystemVersionString)\nNo serial numbers or host paths collected. No automatic upload.\n" + String(decoding: data, as: UTF8.self)
                try text.write(to: destination, atomically: true, encoding: .utf8)
                result = task.terminationStatus == 0 ? "Diagnostics exported. Review the file before sharing it in a community report." : "Diagnostic error recorded in the exported file."
            } catch { result = "Could not export diagnostics: \(error.localizedDescription)" }
            DispatchQueue.main.async {
                self.installing = false; self.installButton.isEnabled = true; self.openButton.isEnabled = true
                self.start.isEnabled = Installer.managedAppState() == .ready; self.state.stringValue = result
            }
        }
    }
    @objc func installBridge() { prepareGame(openAfterInstall: false) }
    func prepareGame(openAfterInstall: Bool) {
        guard process == nil, !installing else { return }
        let panel = NSOpenPanel(); panel.title = "Select the original GeForceNOW.app"
        panel.canChooseDirectories = false; panel.canChooseFiles = true
        panel.allowedContentTypes = [.applicationBundle]; panel.directoryURL = URL(fileURLWithPath:"/Applications")
        guard panel.runModal() == .OK, let source = panel.url else { return }
        let confirm = NSAlert(); confirm.messageText = "Install a local GFN copy?"
        confirm.informativeText = "The original app is unchanged. A separate copy in your Applications folder will load this bridge and be locally signed. No global security settings are changed. An existing managed copy is preserved as a backup."
        confirm.addButton(withTitle:"Install"); confirm.addButton(withTitle:"Cancel")
        guard confirm.runModal() == .alertFirstButtonReturn else { return }
        installing = true; installButton.isEnabled = false; openButton.isEnabled = false; start.isEnabled = false
        state.stringValue = "Validating, copying and signing GFN…"
        DispatchQueue.global(qos:.userInitiated).async {
            let result: String
            var succeeded = false
            do {
                try Installer.install(source:source,resources:Bundle.main.resourceURL!)
                succeeded = true
                result = "Installed. Open GeForce NOW, start the game, then enable force feedback."
            } catch { result = "Installation failed: \(error.localizedDescription)" }
            DispatchQueue.main.async {
                self.installing = false; self.installButton.isEnabled = true; self.openButton.isEnabled = true
                self.start.isEnabled = Installer.managedAppState() == .ready; self.state.stringValue = result
                if succeeded && openAfterInstall { self.openGame() }
            }
        }
    }
    @objc func openGame() {
        guard !installing else { return }
        let readiness = Installer.managedAppState()
        guard readiness == .ready else {
            guard process == nil else { state.stringValue = "Stop force feedback before repairing GFN."; return }
            start.isEnabled = false
            let alert = NSAlert()
            alert.messageText = readiness == .missing ? "Set up GeForce NOW first" : "Repair the GFN copy"
            alert.informativeText = "Select your original NVIDIA GeForceNOW.app to prepare a separate local copy. GeForce NOW will open after setup. Force feedback stays off until you start it."
            alert.addButton(withTitle: "Set up…"); alert.addButton(withTitle: "Cancel")
            state.stringValue = "Setup required. Use Open GeForce NOW or Install / repair to continue."
            if alert.runModal() == .alertFirstButtonReturn { prepareGame(openAfterInstall: true) }
            return
        }
        let url = Installer.destination
        NSWorkspace.shared.openApplication(at: url, configuration: NSWorkspace.OpenConfiguration()) { _, error in
            DispatchQueue.main.async {
                if let error = error { self.state.stringValue = "Could not open GFN: \(error.localizedDescription)" }
                else if self.process == nil { self.state.stringValue = "GeForce NOW opened. Start your game, then enable force feedback." }
            }
        }
    }
    @objc func begin() {
        guard process == nil, !installing else { return }
        let helper = Bundle.main.resourceURL!.appendingPathComponent("FFBReceiver")
        // A vendor update may replace the executable and remove the bridge.
        guard Installer.managedAppState() == .ready else {
            state.stringValue = "Bridge missing. Use Install / repair to prepare a supported GFN copy."; return
        }
        let value = [0.03, 0.10, 0.25, 0.50, 0.75, 1.0][gain.indexOfSelectedItem]
        let seconds = [0, 30, 300, 900, 1800, 3600][duration.indexOfSelectedItem]
        let p = Process(); p.executableURL = helper
        p.arguments = ["--live", "--session", "--gain", String(value), "--seconds", String(seconds), "--parent-pid", String(ProcessInfo.processInfo.processIdentifier)]
        if experimental.state == .on { p.arguments!.append("--allow-experimental") }
        let pipe = Pipe(); p.standardOutput = pipe; p.standardError = pipe
        let logURL = root.appendingPathComponent("session.log")
        guard FileManager.default.createFile(atPath: logURL.path, contents: nil), let file = try? FileHandle(forWritingTo: logURL) else {
            state.stringValue = "Cannot create the session log."; return
        }
        log = file; buffer = ""
        pipe.fileHandleForReading.readabilityHandler = { handle in
            let chunk = handle.availableData
            if chunk.isEmpty { handle.readabilityHandler = nil; return }
            DispatchQueue.main.async {
                try? self.log?.write(contentsOf: chunk)
                self.buffer += String(decoding: chunk, as: UTF8.self)
                while let range = self.buffer.range(of: "\n") {
                    let line = String(self.buffer[..<range.lowerBound]); self.buffer.removeSubrange(..<range.upperBound)
                    if line.hasPrefix("force packet="), self.process === p {
                        let count = line.dropFirst(13).prefix(while: { $0.isNumber })
                        self.state.stringValue = "Active · \(Int(value*100)) % target · \(count) force/stop packets"
                    } else if line == "watchdog stop", self.process === p {
                        self.state.stringValue = "Data pause: force stopped. Waiting for valid game forces."
                    }
                }
            }
        }
        p.terminationHandler = { ended in
            DispatchQueue.main.async {
                self.process = nil; self.installButton.isEnabled = true; self.start.isEnabled = true; self.stop.isEnabled = false
                self.gain.isEnabled = true; self.duration.isEnabled = true; self.experimental.isEnabled = true
                self.state.stringValue = ended.terminationStatus == 0 ? "Stopped. Stop commands sent." : "Receiver exited (code \(ended.terminationStatus)). Details: session.log"
                try? self.log?.close(); self.log = nil
                if self.quitting { NSApp.reply(toApplicationShouldTerminate: true) }
            }
        }
        do {
            try p.run(); process = p
            installButton.isEnabled = false; start.isEnabled = false; stop.isEnabled = true; gain.isEnabled = false; duration.isEnabled = false; experimental.isEnabled = false
            state.stringValue = "Started · immediate \(Int(value*100)) % passthrough · waiting for game forces."
        } catch {
            pipe.fileHandleForReading.readabilityHandler = nil
            try? log?.close(); log = nil
            state.stringValue = "Start failed: \(error.localizedDescription)"
        }
    }
    @objc func end() { if let p = process, p.isRunning { state.stringValue = "Stopping …"; stop.isEnabled = false; p.terminate() } }
    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { true }
    func applicationShouldTerminate(_ sender: NSApplication) -> NSApplication.TerminateReply {
        if installing { return .terminateCancel }
        guard let p = process, p.isRunning else { return .terminateNow }
        quitting = true; end(); return .terminateLater
    }
}
@main
struct FanatecApp {
    static func main() {
        let app = NSApplication.shared
        let delegate = Controller()
        app.delegate = delegate
        app.setActivationPolicy(.regular)
        withExtendedLifetime(delegate) { app.run() }
    }
}
