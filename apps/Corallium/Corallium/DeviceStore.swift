import Foundation
import CoreBluetooth
import Combine

struct NearbyDevice: Identifiable {
    let id: UUID
    var name: String
    var rssi: Int
}

enum DeviceKind: String, CaseIterable, Identifiable {
    case stopwatch = "m5stack-stopwatch"
    case mosaico = "espressif-esp-mosaico"
    var id: String { rawValue }
    var name: String { self == .stopwatch ? "M5Stack StopWatch" : "ESP-Mosaico" }
    var image: String { self == .stopwatch ? "StopWatch" : "Mosaico" }
    var subtitle: String { self == .stopwatch ? "腕间的陪伴时钟" : "桌上的灵感终端" }
    var source: URL {
        URL(string: self == .stopwatch ? "https://docs.m5stack.com/en/core/StopWatch" : "https://mosaico.espressif.com/")!
    }
}

@MainActor
final class DeviceStore: NSObject, ObservableObject {
    @Published private(set) var devices: [NearbyDevice] = []
    @Published private(set) var scanning = false
    @Published private(set) var connected = false
    @Published private(set) var connecting = false
    @Published private(set) var demo = false
    @Published private(set) var bluetoothState = "未开始扫描"
    @Published private(set) var deviceInfo: [String: JSONValue] = [:]
    @Published private(set) var status: [String: JSONValue] = [:]
    @Published private(set) var pendingOperation: String?
    @Published private(set) var lastUpdated: Date?
    @Published var notice: String?
    @Published var error: String?
    let log: EventLog

    private var central: CBCentralManager?
    private var peripherals: [UUID: CBPeripheral] = [:]
    private var peripheral: CBPeripheral?
    private var rx: CBCharacteristic?
    private var tx: CBCharacteristic?
    private var framer = LineFramer()
    private var writeChunks: [Data] = []
    private var correlation = RequestCorrelation()
    private var pendingID: String? { correlation.id }
    private var counter = 0
    private var timeout: Task<Void, Never>?
    private var scanTimeout: Task<Void, Never>?
    private var wifiRefresh: Task<Void, Never>?
    private var wantsScan = false
    private var sessionID = UUID()

    var kind: DeviceKind? { DeviceKind(rawValue: deviceInfo["model"]?.string ?? "") }
    var name: String { deviceInfo["name"]?.string ?? kind?.name ?? "设备" }
    var capabilities: Set<String> { Set(deviceInfo["capabilities"]?.strings ?? []) }
    var busy: Bool { pendingOperation != nil || connecting }
    var ready: Bool { connected && !busy }
    var time: JSONValue { .object(status["time"]?.object ?? [:]) }
    var wifi: JSONValue { .object(status["wifi"]?.object ?? [:]) }
    var battery: JSONValue { .object(status["battery"]?.object ?? [:]) }

    init(log: EventLog, demo: Bool = false) {
        self.log = log
        super.init()
        log.record("app.started", metadata: ["mode": demo ? "demo" : "live"])
        if demo { startDemo(.stopwatch) }
    }

    func scan() {
        if demo { disconnect() }
        wantsScan = true
        error = nil
        notice = nil
        if central == nil {
            central = CBCentralManager(delegate: self, queue: .main)
        } else if central?.state == .poweredOn { beginScan() }
        else { updateBluetoothState() }
    }

    func stopScan() {
        wantsScan = false
        scanning = false
        scanTimeout?.cancel()
        central?.stopScan()
    }

    private func beginScan() {
        stopScan()
        devices = []
        // Keep only the active peripheral; stale discoveries should not be connectable.
        peripherals = peripheral.map { [$0.identifier: $0] } ?? [:]
        scanning = true
        bluetoothState = "正在寻找附近设备"
        central?.scanForPeripherals(withServices: [CBUUID(string: CoralliumProtocol.service)])
        log.record("ble.scan.started")
        scanTimeout = Task {
            try? await Task.sleep(for: .seconds(15))
            guard !Task.isCancelled else { return }
            stopScan()
            bluetoothState = devices.isEmpty ? "未发现兼容设备" : "发现 \(devices.count) 台设备"
            log.record("ble.scan.finished", metadata: ["count": String(devices.count)])
        }
    }

    func connect(_ device: NearbyDevice) {
        guard let candidate = peripherals[device.id], central?.state == .poweredOn else { return }
        disconnect()
        stopScan()
        demo = false
        connecting = true
        error = nil
        notice = nil
        peripheral = candidate
        candidate.delegate = self
        bluetoothState = "正在连接 \(device.name)"
        central?.connect(candidate)
        log.record("ble.connect.started", metadata: ["device_id": device.id.uuidString])
        timeout = Task {
            try? await Task.sleep(for: .seconds(20))
            guard !Task.isCancelled, connecting else { return }
            fail("连接超时。请靠近设备，开启设备的蓝牙连接后重试。", code: "connect_timeout", disconnect: true)
        }
    }

    func disconnect() {
        if connected || connecting { log.record("connection.closed", metadata: ["mode": demo ? "demo" : "live"]) }
        error = nil
        notice = nil
        sessionID = UUID()
        timeout?.cancel()
        wifiRefresh?.cancel()
        stopScan()
        if let peripheral { central?.cancelPeripheralConnection(peripheral) }
        self.peripheral = nil
        rx = nil
        tx = nil
        writeChunks.removeAll()
        framer.reset()
        correlation.reset()
        pendingOperation = nil
        connecting = false
        connected = false
        demo = false
        deviceInfo = [:]
        status = [:]
        lastUpdated = nil
        bluetoothState = "未连接"
    }

    func refresh() { request("device.status") }

    func syncTime() {
        let milliseconds = floor(Date().timeIntervalSince1970 * 1000)
        let offset = TimeZone.current.secondsFromGMT() / 60
        do {
            try CoralliumProtocol.validateTime(unixMilliseconds: milliseconds, offsetMinutes: offset)
            request("time.set", payload: ["unix_ms": .number(milliseconds), "utc_offset_min": .number(Double(offset))])
        } catch { self.error = error.localizedDescription }
    }

    func configureWiFi(ssid: String, password: String) {
        do {
            try CoralliumProtocol.validateWiFi(ssid: ssid, password: password)
            request("wifi.set", payload: ["ssid": .string(ssid), "password": .string(password)])
        } catch { self.error = error.localizedDescription }
    }

    func forgetWiFi() { request("wifi.forget") }

    private func request(_ operation: String, payload: [String: JSONValue] = [:]) {
        guard connected, !busy else { return }
        if operation != "device.info" && operation != "device.status" && !capabilities.contains(operation) {
            error = "当前固件未声明支持此操作。"
            return
        }
        counter += 1
        let id = String(counter)
        let message = WireEnvelope(id: id, op: operation, payload: payload)
        do {
            let data = try CoralliumProtocol.encode(message)
            error = nil
            notice = nil
            correlation.begin(id: id, operation: operation)
            pendingOperation = operation
            log.record("request.sent", metadata: ["operation": operation, "request_id": id, "mode": demo ? "demo" : "live"])
            timeout?.cancel()
            timeout = Task {
                try? await Task.sleep(for: .seconds(10))
                guard !Task.isCancelled, pendingID == id else { return }
                fail("设备响应超时，操作结果未知。请重新连接并刷新状态；设置不会自动重发。", code: "response_timeout", disconnect: true)
            }
            if demo {
                let requestSession = sessionID
                Task {
                    try? await Task.sleep(for: .milliseconds(250))
                    guard demo, pendingID == id, sessionID == requestSession else { return }
                    receiveDemo(message)
                }
            } else if let peripheral, let rx {
                let size = min(180, peripheral.maximumWriteValueLength(for: .withResponse))
                guard size > 0 else { throw ProtocolFailure.malformed }
                writeChunks = stride(from: 0, to: data.count, by: size).map { Data(data[$0..<min($0 + size, data.count)]) }
                peripheral.writeValue(writeChunks.removeFirst(), for: rx, type: .withResponse)
            } else {
                fail("设备通信通道尚未就绪，请重新连接。", code: "channel_missing", disconnect: true)
            }
        } catch { fail(error.localizedDescription, code: "encode_failed", disconnect: false) }
    }

    private func consume(_ data: Data) {
        for result in framer.consume(data) {
            do { handle(try CoralliumProtocol.decode(result.get())) }
            catch {
#if DEBUG
                let reason = (error as? ProtocolFailure)?.diagnosticCode ?? "frame.decode_failed"
                log.record("protocol.rejected", level: "error", metadata: ["code": reason, "bytes": String((try? result.get().count) ?? 0)])
#endif
                fail(error.localizedDescription, code: "invalid_frame", disconnect: true)
                break
            }
        }
    }

    private func handle(_ message: WireEnvelope) {
        if message.id == nil && message.op != "device.status" { return }
        if let id = message.id {
            guard correlation.finish(message) else {
                log.record("response.ignored", level: "warning", metadata: ["code": "unexpected_id_or_operation"])
                return
            }
            timeout?.cancel()
            pendingOperation = nil
            writeChunks.removeAll()
            guard message.ok == true else {
                let code = message.error?.code ?? "internal"
                log.record("request.rejected", level: "error", metadata: ["operation": message.op, "code": code])
                error = "设备拒绝操作（\(code)）：\(message.error?.message ?? "未知错误")"
                if message.op == "device.info" { disconnect() }
                return
            }
            log.record("request.succeeded", metadata: ["operation": message.op, "request_id": id])
        }
        let payload = message.payload ?? [:]
        switch message.op {
        case "device.info":
            guard DeviceKind(rawValue: payload["model"]?.string ?? "") != nil,
                  payload["device_id"]?.string?.isEmpty == false,
                  payload["capabilities"] != nil else {
                fail("此设备未返回有效的受支持设备信息。", code: "unsupported_device", disconnect: true)
                return
            }
            deviceInfo = payload
            bluetoothState = demo ? "演示连接" : "已连接"
            refresh()
        case "device.status":
            status = payload
            lastUpdated = Date()
        case "time.set":
            status["time"] = .object(payload)
            lastUpdated = Date()
            notice = demo ? "演示：时间已同步" : "设备时间已同步"
        case "wifi.set":
            var value = wifi.object ?? [:]
            value["state"] = payload["state"] ?? .string("connecting")
            status["wifi"] = .object(value)
            notice = "配置已接收，等待设备连接 Wi-Fi。"
            scheduleWiFiRefresh()
        case "wifi.forget":
            notice = demo ? "演示：已移除 Wi-Fi 配置" : "设备已移除 Wi-Fi 配置"
            refresh()
        default: break
        }
    }

    private func scheduleWiFiRefresh() {
        wifiRefresh?.cancel()
        wifiRefresh = Task {
            for _ in 0..<6 {
                try? await Task.sleep(for: .seconds(3))
                guard !Task.isCancelled, connected else { return }
                if ready { refresh() }
            }
        }
    }

    private func fail(_ message: String, code: String, disconnect shouldDisconnect: Bool) {
        log.record("connection.error", level: "error", metadata: ["code": code])
        if shouldDisconnect { disconnect() }
        else { timeout?.cancel(); correlation.reset(); pendingOperation = nil; writeChunks.removeAll() }
        error = message
    }

    func startDemo(_ kind: DeviceKind) {
        disconnect()
        demo = true
        connected = true
        error = nil
        notice = nil
        let demoCapabilities = kind == .stopwatch ? ["time.set", "battery"] : ["time.set", "wifi.set", "wifi.forget", "battery", "power", "runtime"]
        deviceInfo = ["device_id": .string("demo-\(kind.rawValue)"), "model": .string(kind.rawValue),
                      "name": .string(kind.name), "firmware": .string("demo / protocol v1"),
                      "capabilities": .array(demoCapabilities.map(JSONValue.string)),
                      "channels": .array([.string("ble")])]
        status = ["time": .object(["unix_ms": .number(floor(Date().timeIntervalSince1970 * 1000)),
                                   "utc_offset_min": .number(Double(TimeZone.current.secondsFromGMT() / 60)),
                                   "valid": .bool(kind == .stopwatch), "source": .string(kind == .stopwatch ? "rtc" : "checkpoint"),
                                   "quality": .string(kind == .stopwatch ? "synchronized" : "estimated")]),
                  "wifi": .object(["state": .string("disconnected"), "ssid": .null, "ip": .null, "rssi": .null]),
                  "battery": .object(["percent": .number(78), "charging": .bool(false), "millivolts": .number(3920),
                                      "power_mw": .null, "runtime_min": .null]), "uptime_ms": .number(124_000)]
        bluetoothState = "演示连接"
        lastUpdated = Date()
        log.record("demo.started", metadata: ["model": kind.rawValue, "mode": "demo"])
    }

    private func receiveDemo(_ request: WireEnvelope) {
        var payload: [String: JSONValue] = [:]
        switch request.op {
        case "device.status": payload = status
        case "device.info": payload = deviceInfo
        case "time.set":
            payload = request.payload ?? [:]
            payload["valid"] = .bool(true)
            payload["source"] = .string("app")
            payload["quality"] = .string("synchronized")
        case "wifi.set":
            payload = ["state": .string("connecting")]
            status["wifi"] = .object(["state": .string("connecting"), "ssid": request.payload?["ssid"] ?? .null, "ip": .null, "rssi": .null])
        case "wifi.forget":
            payload = ["state": .string("disconnected")]
            status["wifi"] = .object(["state": .string("disconnected"), "ssid": .null, "ip": .null, "rssi": .null])
        default: break
        }
        if let data = try? CoralliumProtocol.encode(WireEnvelope(id: request.id, op: request.op, payload: payload, ok: true)) {
            // Demo exercises the same framing and decoding path, including split UTF-8 bytes.
            for chunk in stride(from: 0, to: data.count, by: 7) { consume(Data(data[chunk..<min(chunk + 7, data.count)])) }
        }
        if request.op == "wifi.set" {
            let requestSession = sessionID
            Task {
                try? await Task.sleep(for: .seconds(1))
                guard demo, sessionID == requestSession else { return }
                var value = wifi.object ?? [:]
                value["state"] = .string("connected")
                value["ip"] = .string("192.0.2.10")
                value["rssi"] = .number(-48)
                status["wifi"] = .object(value)
                lastUpdated = Date()
                notice = "演示：Wi-Fi 已连接（模拟数据）"
            }
        }
    }

    private func updateBluetoothState() {
        switch central?.state {
        case .poweredOn: bluetoothState = "蓝牙已开启"
        case .poweredOff: bluetoothState = "蓝牙已关闭，请在系统设置中开启"
        case .unauthorized: bluetoothState = "请在系统设置 → 隐私与安全性 → 蓝牙中允许 Corallium"
        case .unsupported: bluetoothState = "此设备不支持蓝牙低功耗连接"
        case .resetting: bluetoothState = "蓝牙正在重置，请稍后重试"
        default: bluetoothState = "正在检查蓝牙"
        }
    }
}

extension DeviceStore: CBCentralManagerDelegate {
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        log.record("ble.state.changed", metadata: ["state": String(central.state.rawValue), "code": String(CBManager.authorization.rawValue)])
        if demo { return }
        if central.state != .poweredOn, connected || connecting { disconnect() }
        updateBluetoothState()
        if central.state == .poweredOn && wantsScan { beginScan() }
    }

    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral,
                        advertisementData: [String: Any], rssi RSSI: NSNumber) {
        let device = NearbyDevice(id: peripheral.identifier,
                                  name: advertisementData[CBAdvertisementDataLocalNameKey] as? String ?? peripheral.name ?? "Corallium 设备",
                                  rssi: RSSI.intValue)
        peripherals[device.id] = peripheral
        if let index = devices.firstIndex(where: { $0.id == device.id }) { devices[index] = device }
        else { devices.append(device) }
    }

    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        guard self.peripheral === peripheral else { return }
        peripheral.discoverServices([CBUUID(string: CoralliumProtocol.service)])
    }

    func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?) {
        guard self.peripheral === peripheral else { return }
        fail("无法连接设备，请确认设备已开启蓝牙连接并重试。", code: "connect_failed", disconnect: true)
    }

    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        guard self.peripheral === peripheral else { return }
        fail(pendingID == nil ? "设备已断开连接。" : "通信中断，操作结果未知。请重新连接并刷新状态。", code: "disconnected", disconnect: true)
    }
}

extension DeviceStore: CBPeripheralDelegate {
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard self.peripheral === peripheral else { return }
        guard error == nil, let service = peripheral.services?.first(where: { $0.uuid == CBUUID(string: CoralliumProtocol.service) }) else {
            fail("未找到 Corallium 通信服务，请确认固件版本。", code: "service_missing", disconnect: true); return
        }
        peripheral.discoverCharacteristics([CBUUID(string: CoralliumProtocol.receive), CBUUID(string: CoralliumProtocol.transmit)], for: service)
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        guard self.peripheral === peripheral else { return }
        rx = service.characteristics?.first { $0.uuid == CBUUID(string: CoralliumProtocol.receive) }
        tx = service.characteristics?.first { $0.uuid == CBUUID(string: CoralliumProtocol.transmit) }
        guard error == nil, rx?.properties.contains(.write) == true, let tx, tx.properties.contains(.notify) else {
            fail("设备缺少所需的读写通道。", code: "characteristic_missing", disconnect: true); return
        }
        peripheral.setNotifyValue(true, for: tx)
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateNotificationStateFor characteristic: CBCharacteristic, error: Error?) {
        guard self.peripheral === peripheral, characteristic.uuid == tx?.uuid else { return }
        guard error == nil, characteristic.isNotifying else {
            fail("无法订阅设备状态，请重新连接。", code: "notify_failed", disconnect: true); return
        }
        timeout?.cancel()
        connecting = false
        connected = true
        request("device.info")
    }

    func peripheral(_ peripheral: CBPeripheral, didWriteValueFor characteristic: CBCharacteristic, error: Error?) {
        guard self.peripheral === peripheral, characteristic.uuid == rx?.uuid else { return }
        if error != nil {
            fail("写入失败。请确认系统蓝牙配对已完成，然后重新连接。", code: "write_failed", disconnect: true)
        } else if !writeChunks.isEmpty, let rx {
            peripheral.writeValue(writeChunks.removeFirst(), for: rx, type: .withResponse)
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        guard self.peripheral === peripheral, characteristic.uuid == tx?.uuid else { return }
        guard error == nil, let data = characteristic.value else {
            fail("读取设备响应失败。", code: "read_failed", disconnect: true); return
        }
        consume(data)
    }
}
