// SPDX-License-Identifier: GPL-2.0-only
import Foundation
import CryptoKit

enum InstallError: LocalizedError {
    case invalid(String)
    var errorDescription: String? { if case .invalid(let message) = self { return message }; return nil }
}

struct MachOPatcher {
    static let library = "@executable_path/../Frameworks/FanatecBridge.dylib"
    static func read32(_ data: Data, _ offset: Int, big: Bool = false) throws -> UInt32 {
        guard offset >= 0, offset <= data.count - 4 else { throw InstallError.invalid("Truncated Mach-O field") }
        let bytes = Array(data[offset..<offset+4])
        return (0..<4).reduce(UInt32(0)) { $0 | UInt32(bytes[$1]) << (8 * (big ? 3-$1 : $1)) }
    }
    static func write32(_ value: UInt32, _ data: inout Data, _ offset: Int) {
        for i in 0..<4 { data[offset+i] = UInt8(truncatingIfNeeded: value >> (8*i)) }
    }
    static func patch(_ input: Data) throws -> Data {
        var data = input
        let magic = try read32(data, 0)
        var slices: [(Int,Int)] = []
        if magic == 0xbebafeca {
            let count = Int(try read32(data, 4, big: true))
            guard (1...8).contains(count) else { throw InstallError.invalid("Unexpected fat slice count") }
            for i in 0..<count {
                let offset = Int(try read32(data, 8+i*20+8, big: true))
                let size = Int(try read32(data, 8+i*20+12, big: true))
                guard offset >= 8+count*20, size >= 32, offset <= data.count-size,
                      !slices.contains(where: { offset < $0.0+$0.1 && $0.0 < offset+size }) else {
                    throw InstallError.invalid("Invalid or overlapping Mach-O slices")
                }
                slices.append((offset,size))
            }
        } else if magic == 0xfeedfacf { slices = [(0,data.count)] }
        else { throw InstallError.invalid("Unsupported Mach-O format") }
        for (base,size) in slices {
            guard try read32(data,base) == 0xfeedfacf else { throw InstallError.invalid("Expected 64-bit Mach-O") }
            let count = Int(try read32(data,base+16)), total = Int(try read32(data,base+20))
            guard count <= 4096, total <= size-32 else { throw InstallError.invalid("Invalid load-command size") }
            var cursor = base+32, firstSection = size, found = false
            for _ in 0..<count {
                let cmd = try read32(data,cursor), length = Int(try read32(data,cursor+4))
                guard length >= 8, cursor+length <= base+32+total else { throw InstallError.invalid("Invalid load command") }
                if cmd == 0xc {
                    guard length >= 24 else { throw InstallError.invalid("Invalid dylib command") }
                    let relative = Int(try read32(data,cursor+8))
                    guard relative >= 24, relative < length else { throw InstallError.invalid("Invalid dylib name") }
                    let name = data[(cursor+relative)..<(cursor+length)].prefix(while: { $0 != 0 })
                    found = found || String(decoding:name,as:UTF8.self) == library
                }
                if cmd == 0x19 {
                    guard length >= 72 else { throw InstallError.invalid("Invalid segment") }
                    let sections = Int(try read32(data,cursor+64))
                    guard sections <= (length-72)/80 else { throw InstallError.invalid("Invalid section count") }
                    for section in 0..<sections {
                        let offset = Int(try read32(data,cursor+72+section*80+48))
                        if offset > 0 { firstSection = min(firstSection,offset) }
                    }
                }
                cursor += length
            }
            guard cursor == base+32+total else { throw InstallError.invalid("Inconsistent command table") }
            if found { continue }
            let name = Array(library.utf8)+[UInt8(0)], length = (24+name.count+7) & ~7
            guard firstSection <= size, cursor+length <= base+firstSection,
                  data[cursor..<cursor+length].allSatisfy({ $0 == 0 }) else {
                throw InstallError.invalid("No unused load-command padding; app not modified")
            }
            var command = Data(repeating:0,count:length)
            write32(0xc,&command,0); write32(UInt32(length),&command,4); write32(24,&command,8)
            command.replaceSubrange(24..<24+name.count,with:name)
            data.replaceSubrange(cursor..<cursor+length,with:command)
            write32(UInt32(count+1),&data,base+16); write32(UInt32(total+length),&data,base+20)
        }
        return data
    }
}

struct Installer {
    static let versions = [
        "2.0.86.124":"b7d936c4e51601a08c0a101682801e66232e1eceece5a4ace47386390264906f",
        "2.0.89.154":"d82b1d3b26d9d998b78b7879f879daf9df6694e5e3f63767ebcb6cf6982c005f"
    ]
    static let runtime = FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent("Library/Application Support/FanatecGFN")
    static let destination = FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent("Applications/Fanatec GFN/GeForceNOW Fanatec.app")
    @discardableResult static func command(_ executable: String, _ arguments: [String]) throws -> Data {
        let process = Process(); process.executableURL = URL(fileURLWithPath:executable); process.arguments = arguments
        let output = Pipe(), errors = Pipe(); process.standardOutput = output; process.standardError = errors
        // Command output is small (metadata/signing); errors are collected after stdout closes.
        try process.run()
        let data = output.fileHandleForReading.readDataToEndOfFile()
        let error = errors.fileHandleForReading.readDataToEndOfFile(); process.waitUntilExit()
        guard process.terminationStatus == 0 else {
            throw InstallError.invalid("\(URL(fileURLWithPath:executable).lastPathComponent) failed: \(String(decoding:error,as:UTF8.self))")
        }
        return data
    }
    static func install(source: URL, destination: URL = destination, resources: URL) throws {
        let fm = FileManager.default
        guard source.standardizedFileURL != destination.standardizedFileURL else { throw InstallError.invalid("Select the original NVIDIA app") }
        let infoData = try Data(contentsOf:source.appendingPathComponent("Contents/Info.plist"))
        let info = try PropertyListSerialization.propertyList(from:infoData,format:nil) as? [String:Any]
        guard let version = info?["CFBundleShortVersionString"] as? String, let expected = versions[version] else {
            throw InstallError.invalid("This GFN version has not been validated. Installation was not performed.")
        }
        let library = try Data(contentsOf:source.appendingPathComponent("Contents/Frameworks/libGeronimo.dylib"))
        guard SHA256.hash(data:library).map({String(format:"%02x",$0)}).joined() == expected else {
            throw InstallError.invalid("GFN library checksum mismatch. Installation was not performed.")
        }
        let running = try command("/bin/ps",["-axo","comm="])
        guard !String(decoding:running,as:UTF8.self).contains(destination.path) else {
            throw InstallError.invalid("Close the managed GFN copy and its helper processes before installing.")
        }
        let existing = fm.fileExists(atPath:destination.path)
        if existing && !fm.fileExists(atPath:destination.appendingPathComponent("Contents/Resources/fanatec-managed.txt").path) {
            throw InstallError.invalid("Destination is not a managed copy; refusing to replace it.")
        }
        let parent = destination.deletingLastPathComponent()
        try fm.createDirectory(at:parent,withIntermediateDirectories:true)
        let staging = parent.appendingPathComponent(".install-\(UUID().uuidString).app")
        let backup = parent.appendingPathComponent("Previous-\(UUID().uuidString).app")
        defer { if fm.fileExists(atPath:staging.path) { try? fm.removeItem(at:staging) } }
        let entitlementData = try command("/usr/bin/codesign",["-d","--entitlements","-","--xml",source.path])
        guard var entitlements = try PropertyListSerialization.propertyList(from:entitlementData,format:nil) as? [String:Any] else {
            throw InstallError.invalid("Cannot read original signing entitlements")
        }
        entitlements["com.apple.security.cs.disable-library-validation"] = true
        try command("/usr/bin/ditto",[source.path,staging.path])
        let bridge = staging.appendingPathComponent("Contents/Frameworks/FanatecBridge.dylib")
        if fm.fileExists(atPath:bridge.path) { try fm.removeItem(at:bridge) }
        try fm.copyItem(at:resources.appendingPathComponent("FanatecBridge.dylib"),to:bridge)
        let executable = staging.appendingPathComponent("Contents/MacOS/GeForceNOW")
        try MachOPatcher.patch(Data(contentsOf:executable)).write(to:executable)
        try "Managed locally by Fanatec GFN. Original NVIDIA app is unchanged.\n".write(to:staging.appendingPathComponent("Contents/Resources/fanatec-managed.txt"),atomically:true,encoding:.utf8)
        let entitlementFile = parent.appendingPathComponent(".entitlements-\(UUID().uuidString).plist")
        defer { try? fm.removeItem(at:entitlementFile) }
        try PropertyListSerialization.data(fromPropertyList:entitlements,format:.xml,options:0).write(to:entitlementFile)
        try command("/usr/bin/codesign",["--force","--sign","-","--timestamp=none",bridge.path])
        try command("/usr/bin/codesign",["--force","--sign","-","--options","runtime","--timestamp=none","--entitlements",entitlementFile.path,staging.path])
        try command("/usr/bin/codesign",["--verify","--deep","--strict",staging.path])
        if existing { try fm.moveItem(at:destination,to:backup) }
        do { try fm.moveItem(at:staging,to:destination) }
        catch { if existing { try? fm.moveItem(at:backup,to:destination) }; throw error }
    }
}
