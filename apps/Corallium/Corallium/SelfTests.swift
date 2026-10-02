import Foundation

@MainActor
enum CoralliumSelfTests {
    struct Failure: Error, CustomStringConvertible { let description: String }
    private static func check(_ value: @autoclosure () -> Bool, _ label: String) throws {
        if !value() { throw Failure(description: label) }
    }
    private static func rejects(_ label: String, _ block: () throws -> Void) throws {
        do { try block() } catch { return }
        throw Failure(description: "Expected rejection: \(label)")
    }
    private static func eventually(_ label: String, _ condition: () -> Bool) async throws {
        let deadline = ContinuousClock.now.advanced(by: .seconds(5))
        while !condition(), ContinuousClock.now < deadline {
            try await Task.sleep(for: .milliseconds(50))
        }
        try check(condition(), label)
    }

    static func run() throws {
        let envelope = WireEnvelope(id: "17", op: "test.echo", payload: ["name": .string("珊瑚 / 时钟")], ok: true)
        let encoded = try CoralliumProtocol.encode(envelope)
        var framer = LineFramer()
        var frames: [Data] = []
        for byte in encoded {
            for result in framer.consume(Data([byte])) { frames.append(try result.get()) }
        }
        try check(frames.count == 1, "UTF-8 split framing")
        let decoded = try CoralliumProtocol.decode(frames[0])
        try check(decoded.payload?["name"]?.string == "珊瑚 / 时钟", "UTF-8 payload round trip")
        try check(framer.consume(encoded + encoded).count == 2, "multiple frames per notification")
        try check(framer.consume(Data([10])).isEmpty, "empty separators ignored")

        let huge = Data(repeating: 0x78, count: 4096) + Data([10]) + encoded
        let recovered = framer.consume(huge)
        try check(recovered.count == 2, "oversized frame discarded until LF")
        if case .success = recovered[0] { throw Failure(description: "oversized accepted") }
        _ = try CoralliumProtocol.decode(recovered[1].get())
        _ = framer.consume(Data("partial".utf8))
        framer.reset()
        try check(framer.consume(encoded).count == 1, "disconnect resets partial frame")

        try rejects("unknown version") { _ = try CoralliumProtocol.decode(Data("{\"v\":2,\"id\":\"1\",\"op\":\"device.info\",\"ok\":true,\"payload\":{}}".utf8)) }
        try rejects("boolean version") { _ = try CoralliumProtocol.decode(Data("{\"v\":true,\"id\":\"1\",\"op\":\"device.info\",\"ok\":true,\"payload\":{}}".utf8)) }
        try rejects("request on response channel") { _ = try CoralliumProtocol.decode(Data("{\"v\":1,\"id\":\"1\",\"op\":\"time.set\",\"payload\":{}}".utf8)) }
        try rejects("invalid JSON") { _ = try CoralliumProtocol.decode(Data([0xFF])) }
        try rejects("outbound size limit") { _ = try CoralliumProtocol.encode(WireEnvelope(id: "1", op: "wifi.set", payload: ["ssid": .string(String(repeating: "a", count: 2048))])) }
        _ = try CoralliumProtocol.decode(Data("{\"v\":1,\"op\":\"future.event\",\"payload\":{}}".utf8))
        _ = try CoralliumProtocol.decode(Data("{\"v\":1,\"id\":\"1\",\"op\":\"time.set\",\"ok\":false,\"error\":{\"code\":\"unsupported\",\"message\":\"not supported\"}}".utf8))
        let retainedRTC = WireEnvelope(id: "rtc", op: "time.set", payload: [
            "unix_ms": .number(1_800_000_000_000), "utc_offset_min": .number(480),
            "valid": .bool(true), "source": .string("rtc"), "quality": .string("estimated")
        ], ok: true)
        let retainedTime = try CoralliumProtocol.decode(CoralliumProtocol.encode(retainedRTC).dropLast())
        try check(retainedTime.payload?["valid"]?.bool == true && retainedTime.payload?["quality"]?.string == "estimated", "retained RTC accepts estimated drift quality")
        var coldCheckpoint = retainedRTC
        coldCheckpoint.payload?["source"] = .string("checkpoint")
        try rejects("cold checkpoint cannot claim continuous time") {
            _ = try CoralliumProtocol.decode(CoralliumProtocol.encode(coldCheckpoint).dropLast())
        }
        try CoralliumProtocol.validateWiFi(ssid: "家里的网络", password: "12345678")
        try CoralliumProtocol.validateWiFi(ssid: "open", password: "")
        try rejects("multibyte SSID limit") { try CoralliumProtocol.validateWiFi(ssid: String(repeating: "界", count: 11), password: "12345678") }
        try rejects("short password") { try CoralliumProtocol.validateWiFi(ssid: "network", password: "1234567") }
        try rejects("long password") { try CoralliumProtocol.validateWiFi(ssid: "network", password: String(repeating: "x", count: 64)) }
        try rejects("empty SSID") { try CoralliumProtocol.validateWiFi(ssid: "", password: "12345678") }
        try rejects("NUL SSID") { try CoralliumProtocol.validateWiFi(ssid: "a\0b", password: "12345678") }
        try CoralliumProtocol.validateTime(unixMilliseconds: 1_577_836_800_000, offsetMinutes: 840)
        try rejects("invalid time") { try CoralliumProtocol.validateTime(unixMilliseconds: 0, offsetMinutes: 0) }
        try rejects("invalid timezone") { try CoralliumProtocol.validateTime(unixMilliseconds: 1_800_000_000_000, offsetMinutes: 900) }

        try rejects("fractional time") { try CoralliumProtocol.validateTime(unixMilliseconds: 1_800_000_000_000.5, offsetMinutes: 0) }
        try rejects("non-finite time") { try CoralliumProtocol.validateTime(unixMilliseconds: .infinity, offsetMinutes: 0) }
        for identifier in ["", "你好", "has space", String(repeating: "x", count: 65)] {
            let invalid = WireEnvelope(id: identifier, op: "future.operation", payload: [:], ok: true)
            try rejects("invalid request id") { _ = try CoralliumProtocol.decode(CoralliumProtocol.encode(invalid).dropLast()) }
        }
        var mixed = WireEnvelope(id: "1", op: "test.echo", payload: [:], ok: true)
        mixed.error = WireError(code: "internal", message: "mixed envelopes")
        try rejects("mixed success/error") { _ = try CoralliumProtocol.decode(CoralliumProtocol.encode(mixed).dropLast()) }
        var tracker = RequestCorrelation()
        tracker.begin(id: "old", operation: "time.set")
        tracker.reset() // Same reset used when a request times out or disconnects.
        tracker.begin(id: "new", operation: "time.set")
        try check(!tracker.finish(WireEnvelope(id: "old", op: "time.set", ok: true)), "late response rejected")
        try check(tracker.id == "new", "late response preserves current request")
        try check(!tracker.finish(WireEnvelope(id: nil, op: "device.status")), "event does not complete request")
        try check(!tracker.finish(WireEnvelope(id: "new", op: "wifi.set", ok: true)), "wrong operation rejected")
        try check(tracker.finish(WireEnvelope(id: "new", op: "time.set", ok: true)), "matching response completes request")
        try check(!tracker.finish(WireEnvelope(id: "new", op: "time.set", ok: true)), "duplicate response rejected")

        let arguments = ProcessInfo.processInfo.arguments
        if let index = arguments.firstIndex(of: "--fixtures"), index + 1 < arguments.count {
            let fixtures = URL(fileURLWithPath: arguments[index + 1], isDirectory: true)
            let lines = try String(contentsOf: fixtures.appendingPathComponent("session.jsonl"), encoding: .utf8)
            var inboundCount = 0
            for line in lines.split(separator: "\n") {
                let data = Data(line.utf8)
                let object = try JSONSerialization.jsonObject(with: data) as! [String: Any]
                if object["ok"] != nil || object["id"] == nil {
                    _ = try CoralliumProtocol.decode(data)
                    inboundCount += 1
                }
            }
            _ = try CoralliumProtocol.decode(Data(contentsOf: fixtures.appendingPathComponent("unknown-status.json")))
            let stopwatch = try CoralliumProtocol.decode(Data(contentsOf: fixtures.appendingPathComponent("stopwatch-info.json")))
            try check(Set(stopwatch.payload?["capabilities"]?.strings ?? []) == ["time.set", "battery"], "shared StopWatch fixture has no Wi-Fi capability")
            try check(inboundCount == 7, "shared protocol session fixtures")
            print("Validated shared protocol fixtures: 7 session responses/events + unknown status + StopWatch info.")
        }

        let directory = FileManager.default.temporaryDirectory.appendingPathComponent("corallium-tests-\(UUID().uuidString)")
        defer { try? FileManager.default.removeItem(at: directory) }
        let log = EventLog(directory: directory)
        let date = Date(timeIntervalSince1970: 1_800_000_000)
        log.record("request.sent", metadata: ["operation": "wifi.set", "password": "secret-do-not-log", "ssid": "private-network", "payload": "secret"], date: date)
        log.record("request.succeeded", date: date)
        log.record("app.started", date: date.addingTimeInterval(86400))
        let files = try FileManager.default.contentsOfDirectory(at: directory, includingPropertiesForKeys: nil)
        try check(files.count == 2, "daily rotation")
        var count = 0
        for file in files {
            let text = try String(contentsOf: file, encoding: .utf8)
            try check(!text.contains("secret") && !text.contains("private-network") && !text.contains("password"), "credential redaction")
            for line in text.split(separator: "\n") {
                _ = try JSONDecoder().decode(LogEntry.self, from: Data(line.utf8))
                count += 1
            }
        }
        try check(count == 3, "append without replacing earlier events")
        print("Validated framing, malformed input, size bounds, Wi-Fi/time validation, JSONL rotation and credential exclusion.")
    }

    static func runDemo(_ store: DeviceStore) async throws {
        store.startDemo(.stopwatch)
        try check(store.demo && store.ready, "explicit demo starts ready")
        try check(store.capabilities == ["time.set", "battery"], "StopWatch exposes no Wi-Fi capabilities")
        store.syncTime()
        try check(store.pendingOperation == "time.set", "time request starts")
        store.refresh()
        try check(store.pendingOperation == "time.set", "single request in flight")
        try await eventually("time response applied") { store.time["source"].string == "app" && store.ready }
        store.configureWiFi(ssid: "Corallium Test", password: "sensitive-test-password")
        try check(store.pendingOperation == nil && store.wifi["state"].string == "disconnected", "StopWatch rejects unsupported Wi-Fi without sending")
        store.startDemo(.mosaico)
        try check(store.time["quality"].string == "estimated", "checkpoint displayed as estimated")
        try check(store.capabilities.contains("wifi.set") && store.capabilities.contains("wifi.forget"), "Mosaico exposes ordinary Wi-Fi")
        store.configureWiFi(ssid: "Corallium Test", password: "sensitive-test-password")
        try await eventually("async Wi-Fi status applied") { store.wifi["state"].string == "connected" && store.ready }
        try check(store.wifi["ssid"].string == "Corallium Test", "SSID status propagated")
        store.forgetWiFi()
        try await eventually("forget refreshes status") { store.wifi["state"].string == "disconnected" && store.ready }
        store.disconnect()
        try check(!store.connected && store.status.isEmpty && !store.demo, "disconnect clears session")
        store.log.record("self_test.integration", metadata: ["result": "passed", "mode": "demo"])
        print("Corallium demo integration: PASS (simulated, no hardware acceptance)")
    }
}
