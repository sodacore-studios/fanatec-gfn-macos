import Foundation

@main
struct InstallerTests {
    static func thin() -> Data {
        var data=Data(repeating:0,count:512)
        MachOPatcher.write32(0xfeedfacf,&data,0)
        MachOPatcher.write32(1,&data,16); MachOPatcher.write32(152,&data,20)
        MachOPatcher.write32(0x19,&data,32); MachOPatcher.write32(152,&data,36)
        MachOPatcher.write32(1,&data,96); MachOPatcher.write32(400,&data,152)
        return data
    }
    static func reject(_ data: Data) {
        do { _ = try MachOPatcher.patch(data); fatalError("Malformed input accepted") }
        catch { }
    }
    static func main() throws {
        let valid=thin(), patched=try MachOPatcher.patch(valid)
        let check1 = try MachOPatcher.read32(patched,16)==2; precondition(check1)
        let check2 = try MachOPatcher.patch(patched)==patched; precondition(check2)
        precondition(patched.suffix(112)==valid.suffix(112))
        for n in [0,1,3,4,8,16,31,100,183] { reject(Data(valid.prefix(n))) }
        var bad=valid; bad[0]=0; reject(bad)
        bad=valid; MachOPatcher.write32(5000,&bad,16); reject(bad)
        bad=valid; MachOPatcher.write32(5000,&bad,20); reject(bad)
        bad=valid; MachOPatcher.write32(4,&bad,36); reject(bad)
        bad=valid; MachOPatcher.write32(5000,&bad,96); reject(bad)
        bad=valid; MachOPatcher.write32(190,&bad,152); reject(bad)
        bad=valid; bad[184]=1; reject(bad)
        var fat=Data(repeating:0,count:2048)
        func big(_ value: UInt32,_ offset: Int) { MachOPatcher.write32(value.byteSwapped,&fat,offset) }
        big(0xcafebabe,0);big(2,4);big(512,16);big(512,20);big(1024,36);big(512,40)
        fat.replaceSubrange(512..<1024,with:valid);fat.replaceSubrange(1024..<1536,with:valid)
        let universal=try MachOPatcher.patch(fat)
        let check3 = try MachOPatcher.read32(universal,528)==2; precondition(check3)
        let check4 = try MachOPatcher.read32(universal,1040)==2; precondition(check4)
        let check5 = try MachOPatcher.patch(universal)==universal; precondition(check5)
        bad=fat;MachOPatcher.write32(UInt32(512).byteSwapped,&bad,36);reject(bad)
        bad=fat;MachOPatcher.write32(UInt32(9).byteSwapped,&bad,4);reject(bad)
        let temporary=FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        defer {try? FileManager.default.removeItem(at:temporary)}
        let source=temporary.appendingPathComponent("Unknown.app")
        try FileManager.default.createDirectory(at:source.appendingPathComponent("Contents"),withIntermediateDirectories:true)
        let info=try PropertyListSerialization.data(fromPropertyList:["CFBundleShortVersionString":"99.0.0"],format:.xml,options:0)
        try info.write(to:source.appendingPathComponent("Contents/Info.plist"))
        let destination=temporary.appendingPathComponent("Destination.app")
        do {try Installer.install(source:source,destination:destination,resources:temporary);fatalError("Unknown version accepted")}
        catch {precondition(!FileManager.default.fileExists(atPath:destination.path))}
        let check6 = try Data(contentsOf:source.appendingPathComponent("Contents/Info.plist"))==info; precondition(check6)
        let readiness = temporary.appendingPathComponent("Readiness.app")
        let fm = FileManager.default
        precondition(Installer.managedAppState(at: readiness) == .missing)
        try fm.createDirectory(at: readiness.appendingPathComponent("Contents/MacOS"), withIntermediateDirectories: true)
        precondition(Installer.managedAppState(at: readiness) == .needsRepair)
        let executable = readiness.appendingPathComponent("Contents/MacOS/GeForceNOW")
        try Data(MachOPatcher.library.utf8).write(to: executable)
        let launchInfo = try PropertyListSerialization.data(fromPropertyList: ["CFBundleExecutable": "GeForceNOW"], format: .xml, options: 0)
        try launchInfo.write(to: readiness.appendingPathComponent("Contents/Info.plist"))
        try fm.setAttributes([.posixPermissions: 0o755], ofItemAtPath: executable.path)
        precondition(Installer.managedAppState(at: readiness) == .needsRepair)
        try fm.createDirectory(at: readiness.appendingPathComponent("Contents/Frameworks"), withIntermediateDirectories: true)
        let bridge = readiness.appendingPathComponent("Contents/Frameworks/FanatecBridge.dylib")
        try Data("version=0.2".utf8).write(to: bridge)
        precondition(Installer.managedAppState(at: readiness) == .needsRepair)
        try Installer.bridgeProtocolMarker.write(to: bridge)
        precondition(Installer.managedAppState(at: readiness) == .ready)
        try Data("vendor updated executable without bridge".utf8).write(to: executable)
        precondition(Installer.managedAppState(at: readiness) == .needsRepair)
        try Data(MachOPatcher.library.utf8).write(to: executable)
        try fm.setAttributes([.posixPermissions: 0o644], ofItemAtPath: executable.path)
        precondition(Installer.managedAppState(at: readiness) == .needsRepair)
        try fm.setAttributes([.posixPermissions: 0o755], ofItemAtPath: executable.path)
        try Data("invalid plist".utf8).write(to: readiness.appendingPathComponent("Contents/Info.plist"))
        precondition(Installer.managedAppState(at: readiness) == .needsRepair)
        print("PASS: missing, incomplete, updated and ready managed-app states")
        print("PASS: native installer thin/fat patching, idempotency, malformed input, padding and unknown version rejection")
    }
}
