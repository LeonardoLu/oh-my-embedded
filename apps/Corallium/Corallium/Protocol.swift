import Foundation

indirect enum JSONValue: Codable, Equatable {
    case string(String), number(Double), bool(Bool), object([String: JSONValue]), array([JSONValue]), null

    init(from decoder: Decoder) throws {
        let container = try decoder.singleValueContainer()
        if container.decodeNil() { self = .null }
        else if let value = try? container.decode(Bool.self) { self = .bool(value) }
        else if let value = try? container.decode(Double.self) { self = .number(value) }
        else if let value = try? container.decode(String.self) { self = .string(value) }
        else if let value = try? container.decode([String: JSONValue].self) { self = .object(value) }
        else { self = .array(try container.decode([JSONValue].self)) }
    }

    func encode(to encoder: Encoder) throws {
        var container = encoder.singleValueContainer()
        switch self {
        case .string(let value): try container.encode(value)
        case .number(let value): try container.encode(value)
        case .bool(let value): try container.encode(value)
        case .object(let value): try container.encode(value)
        case .array(let value): try container.encode(value)
        case .null: try container.encodeNil()
        }
    }

    var string: String? { if case .string(let value) = self { value } else { nil } }
    var number: Double? { if case .number(let value) = self { value } else { nil } }
    var bool: Bool? { if case .bool(let value) = self { value } else { nil } }
    var object: [String: JSONValue]? { if case .object(let value) = self { value } else { nil } }
    var strings: [String] { if case .array(let value) = self { value.compactMap(\.string) } else { [] } }
    subscript(_ key: String) -> JSONValue { object?[key] ?? .null }
}

struct WireEnvelope: Codable {
    let v: Int
    let id: String?
    let op: String
    var ok: Bool?
    var payload: [String: JSONValue]?
    var error: WireError?

    init(id: String?, op: String, payload: [String: JSONValue] = [:], ok: Bool? = nil) {
        self.v = 1
        self.id = id
        self.op = op
        self.payload = payload
        self.ok = ok
    }
}

struct WireError: Codable { let code: String; let message: String }

enum ProtocolFailure: LocalizedError {
    case oversized, malformed, unsupportedVersion, invalidWiFi, invalidTime
    case invalidField(String)
    var diagnosticCode: String {
        switch self {
        case .oversized: "frame.too_large"
        case .malformed: "frame.invalid_json"
        case .unsupportedVersion: "envelope.unsupported_version"
        case .invalidWiFi: "request.wifi_bounds"
        case .invalidTime: "request.time_bounds"
        case .invalidField(let field): "field.\(field)"
        }
    }
    var errorDescription: String? {
        switch self {
        case .oversized: "消息超过 2048 字节，请检查设备固件。"
        case .malformed, .invalidField: "设备发送了无效的协议消息。"
        case .unsupportedVersion: "设备协议版本不兼容。"
        case .invalidWiFi: "SSID 需为 1–32 UTF-8 字节；密码为空或 8–63 UTF-8 字节。"
        case .invalidTime: "设备时间需在 2020–2099 年内，时区偏移需在 −12:00 至 +14:00。"
        }
    }
}

enum CoralliumProtocol {
    static let service = "7D2A0001-7E6D-4C55-A8D2-6B6D5E8F9010"
    static let receive = "7D2A0002-7E6D-4C55-A8D2-6B6D5E8F9010"
    static let transmit = "7D2A0003-7E6D-4C55-A8D2-6B6D5E8F9010"
    static let maximumFrameSize = 2048

    static func encode(_ message: WireEnvelope) throws -> Data {
        let encoder = JSONEncoder()
        encoder.outputFormatting = [.sortedKeys, .withoutEscapingSlashes]
        var data = try encoder.encode(message)
        guard data.count <= maximumFrameSize else { throw ProtocolFailure.oversized }
        data.append(0x0A)
        return data
    }

    static func decode(_ data: Data) throws -> WireEnvelope {
        guard data.count <= maximumFrameSize else { throw ProtocolFailure.oversized }
        let message: WireEnvelope
        do { message = try JSONDecoder().decode(WireEnvelope.self, from: data) }
        catch let error as DecodingError {
            let path: [CodingKey]
            switch error {
            case .keyNotFound(let key, let context): path = context.codingPath + [key]
            case .typeMismatch(_, let context), .valueNotFound(_, let context), .dataCorrupted(let context): path = context.codingPath
            @unknown default: path = []
            }
            // Only schema-owned field names may reach diagnostics; never arbitrary device keys or values.
            let names: Set<String> = ["v", "id", "op", "ok", "payload", "error", "code", "message", "time", "unix_ms", "utc_offset_min", "valid", "source", "quality", "wifi", "state", "ssid", "ip", "rssi", "battery", "percent", "charging", "millivolts", "power_mw", "runtime_min", "uptime_ms", "device_id", "model", "name", "firmware", "capabilities", "channels"]
            if path.isEmpty { throw ProtocolFailure.malformed }
            throw ProtocolFailure.invalidField(path.map { names.contains($0.stringValue) ? $0.stringValue : "value" }.joined(separator: "."))
        } catch { throw ProtocolFailure.malformed }
        guard (1...64).contains(message.op.count) else { throw ProtocolFailure.invalidField("op") }
        guard message.v == 1 else { throw ProtocolFailure.unsupportedVersion }
        // Inbound packets are responses or status events, never requests.
        if let id = message.id {
            guard (1...64).contains(id.utf8.count),
                  id.utf8.allSatisfy({ (65...90).contains($0) || (97...122).contains($0) || (48...57).contains($0) || [45, 46, 95].contains($0) }),
                  message.ok != nil,
                  message.ok == true ? (message.payload != nil && message.error == nil) : (message.error != nil && message.payload == nil) else {
                throw ProtocolFailure.invalidField("response.envelope")
            }
        } else if message.payload == nil || message.ok != nil || message.error != nil {
            throw ProtocolFailure.invalidField("event.envelope")
        }
        if let failure = message.error {
            guard ["invalid_request", "unsupported", "busy", "internal", "not_authorized"].contains(failure.code),
                  !failure.message.isEmpty else { throw ProtocolFailure.invalidField("error") }
        }
        try validatePayload(message)
        return message
    }

    static func validateWiFi(ssid: String, password: String) throws {
        guard (1...32).contains(ssid.utf8.count), !ssid.contains("\0"), !password.contains("\0"),
              password.isEmpty || (8...63).contains(password.utf8.count) else { throw ProtocolFailure.invalidWiFi }
    }

    static func validateTime(unixMilliseconds: Double, offsetMinutes: Int) throws {
        guard unixMilliseconds.isFinite, unixMilliseconds.rounded(.towardZero) == unixMilliseconds,
              unixMilliseconds >= 1_577_836_800_000, unixMilliseconds < 4_102_444_800_000,
              (-720...840).contains(offsetMinutes) else { throw ProtocolFailure.invalidTime }
    }
}

struct LineFramer {
    private var buffer = Data()
    private var discarding = false

    mutating func reset() { buffer.removeAll(keepingCapacity: false); discarding = false }

    mutating func consume(_ bytes: Data) -> [Result<Data, ProtocolFailure>] {
        var results: [Result<Data, ProtocolFailure>] = []
        for byte in bytes {
            if byte == 0x0A {
                if !discarding && !buffer.isEmpty { results.append(.success(buffer)) }
                buffer.removeAll(keepingCapacity: true)
                discarding = false
            } else if !discarding {
                if buffer.count >= CoralliumProtocol.maximumFrameSize {
                    buffer.removeAll(keepingCapacity: true)
                    discarding = true
                    results.append(.failure(.oversized))
                } else { buffer.append(byte) }
            }
        }
        return results
    }
}

struct RequestCorrelation {
    private(set) var id: String?
    private(set) var operation: String?
    mutating func begin(id: String, operation: String) { self.id = id; self.operation = operation }
    mutating func reset() { id = nil; operation = nil }
    mutating func finish(_ message: WireEnvelope) -> Bool {
        guard let responseID = message.id, responseID == id, message.op == operation else { return false }
        reset()
        return true
    }
}

extension CoralliumProtocol {
    static func validatePayload(_ message: WireEnvelope) throws {
        guard message.ok != false, let payload = message.payload else { return }
        let object = JSONValue.object(payload)
        func integer(_ value: JSONValue, in range: ClosedRange<Double> = -9_007_199_254_740_991...9_007_199_254_740_991, nullable: Bool = false) -> Bool {
            if nullable && value == .null { return true }
            guard let value = value.number else { return false }
            return value.isFinite && value.rounded(.towardZero) == value && range.contains(value)
        }
        func require(_ valid: Bool, _ field: String) throws {
            guard valid else { throw ProtocolFailure.invalidField(field) }
        }
        func time(_ value: JSONValue) throws {
            try require(value.object != nil, "time")
            try require(value.object?["unix_ms"] != nil && integer(value["unix_ms"], in: 1_577_836_800_000...4_102_444_799_999, nullable: true), "time.unix_ms")
            try require(integer(value["utc_offset_min"], in: -720...840), "time.utc_offset_min")
            try require(value["valid"].bool != nil, "time.valid")
            try require(["rtc", "network", "app", "unset", "checkpoint"].contains(value["source"].string ?? ""), "time.source")
            try require(["synchronized", "estimated", "unset"].contains(value["quality"].string ?? ""), "time.quality")
            if value["valid"].bool == true {
                try require(value["unix_ms"] != .null, "time.valid_without_time")
                try require(["synchronized", "estimated"].contains(value["quality"].string ?? ""), "time.valid_quality")
                try require(["rtc", "network", "app"].contains(value["source"].string ?? ""), "time.valid_source")
            }
        }
        func stringOrNull(_ value: JSONValue) -> Bool { value == .null || value.string != nil }
        func nonnegativeOrNull(_ value: JSONValue) -> Bool { value == .null || value.number.map { $0.isFinite && $0 >= 0 } == true }
        func arrayOfUniqueStrings(_ value: JSONValue) -> Bool {
            guard case .array(let items) = value else { return false }
            return items.allSatisfy { $0.string != nil } && Set(items.compactMap(\.string)).count == items.count
        }
        switch message.op {
        case "device.info":
            for key in ["device_id", "model", "name", "firmware"] {
                try require(object[key].string?.isEmpty == false, "info." + key)
            }
            try require((object["device_id"].string?.count ?? 0) <= 128, "info.device_id")
            try require(arrayOfUniqueStrings(object["capabilities"]), "info.capabilities")
            try require(arrayOfUniqueStrings(object["channels"]), "info.channels")
        case "time.set": try time(object)
        case "wifi.set": try require(object["state"].string == "connecting", "wifi.set.state")
        case "wifi.forget": try require(object["state"].string == "disconnected", "wifi.forget.state")
        case "device.status":
            try time(object["time"])
            try require(integer(object["uptime_ms"], in: 0...9_007_199_254_740_991), "uptime_ms")
            let wifi = object["wifi"], battery = object["battery"]
            for key in ["state", "ssid", "ip", "rssi"] { try require(wifi.object?[key] != nil, "wifi." + key) }
            try require(["disconnected", "connecting", "connected", "failed"].contains(wifi["state"].string ?? ""), "wifi.state")
            try require(stringOrNull(wifi["ssid"]), "wifi.ssid")
            try require(stringOrNull(wifi["ip"]), "wifi.ip")
            try require(integer(wifi["rssi"], nullable: true), "wifi.rssi")
            for key in ["percent", "charging", "millivolts", "power_mw", "runtime_min"] { try require(battery.object?[key] != nil, "battery." + key) }
            try require(integer(battery["percent"], in: 0...100, nullable: true), "battery.percent")
            try require(battery["charging"] == .null || battery["charging"].bool != nil, "battery.charging")
            try require(integer(battery["millivolts"], in: 0...9_007_199_254_740_991, nullable: true), "battery.millivolts")
            try require(nonnegativeOrNull(battery["power_mw"]), "battery.power_mw")
            try require(nonnegativeOrNull(battery["runtime_min"]), "battery.runtime_min")
        default: break // Forward-compatible, unknown events are ignored by the store.
        }
    }
}
