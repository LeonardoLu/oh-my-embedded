import SwiftUI
#if os(macOS)
import AppKit
#endif

extension Color {
    static let coral = Color(red: 0.88, green: 0.32, blue: 0.28)
    static let sea = Color(red: 0.16, green: 0.43, blue: 0.44)
}

struct ContentView: View {
    @ObservedObject var store: DeviceStore
    @ObservedObject var log: EventLog
    @State private var page: String? = "devices"
    @State private var preferredColumn: NavigationSplitViewColumn = .detail
    @State private var showWiFi = false
    @State private var confirmForget = false

    var body: some View {
        NavigationSplitView(preferredCompactColumn: $preferredColumn) {
            List(selection: $page) {
                Section {
                    NavigationLink(value: "devices") { Label("我的设备", systemImage: "square.stack.3d.up") }
                    NavigationLink(value: "logs") { Label("活动日志", systemImage: "text.alignleft") }
                }
                Section("附近的设备") {
                    if store.scanning {
                        HStack(spacing: 10) { ProgressView().controlSize(.small); Text("正在寻找设备…").foregroundStyle(.secondary) }
                    }
                    if store.devices.isEmpty && !store.scanning {
                        Text("开启设备的蓝牙连接，\n然后扫描附近设备。")
                            .font(.caption).foregroundStyle(.secondary).padding(.vertical, 5)
                    }
                    ForEach(store.devices) { device in
                        Button {
                            page = "devices"
                            preferredColumn = .detail
                            store.connect(device)
                        } label: {
                            VStack(alignment: .leading, spacing: 5) {
                                Text(device.name).font(.headline)
                                Text("\(device.rssi) dBm · 点击连接").font(.caption).foregroundStyle(.secondary)
                            }
                        }
                        .disabled(store.busy)
                    }
                    Button { store.scanning ? store.stopScan() : store.scan() } label: {
                        Label(store.scanning ? "停止扫描" : "扫描附近设备", systemImage: store.scanning ? "stop.circle" : "antenna.radiowaves.left.and.right")
                    }.disabled(store.connecting)
                }
                Section("体验") {
                    ForEach(DeviceKind.allCases) { kind in
                        Button { page = "devices"; preferredColumn = .detail; store.startDemo(kind) } label: {
                            Label("演示 · \(kind == .stopwatch ? "StopWatch" : "Mosaico")", systemImage: "play.rectangle")
                        }
                    }
                }
            }
            .navigationTitle("Corallium")
#if os(macOS)
            .navigationSplitViewColumnWidth(min: 210, ideal: 230, max: 280)
#endif
            .safeAreaInset(edge: .bottom) {
                HStack(spacing: 8) {
                    Circle().fill(store.connected ? Color.sea : Color.secondary).frame(width: 7, height: 7)
                    Text(store.bluetoothState).font(.caption).foregroundStyle(.secondary)
                    Spacer(minLength: 0)
                }.padding(16)
            }
        } detail: {
            Group {
                if page == "logs" { LogView(log: log) }
                else { devicePage }
            }
            .navigationTitle(page == "logs" ? "活动日志" : "我的设备")
            .toolbar {
                if page != "logs" {
                    ToolbarItemGroup {
                        if store.connected {
                            Button { store.refresh() } label: { Label("刷新状态", systemImage: "arrow.clockwise") }
                                .disabled(!store.ready).help("刷新设备状态")
                            Button { store.disconnect() } label: { Label("断开连接", systemImage: "xmark.circle") }
                                .help("断开当前设备")
                        }
                    }
                }
            }
        }
        .sheet(isPresented: $showWiFi) { WiFiSheet(store: store) }
        .onChange(of: store.capabilities) { _, capabilities in
            if !capabilities.contains("wifi.set") { showWiFi = false; confirmForget = false }
        }
        .confirmationDialog("移除设备保存的 Wi-Fi 配置？", isPresented: $confirmForget, titleVisibility: .visible) {
            Button("移除 Wi-Fi 配置", role: .destructive) { store.forgetWiFi() }
            Button("取消", role: .cancel) {}
        } message: { Text("设备会断开当前 Wi-Fi。蓝牙连接保持可用。") }
    }

    private var devicePage: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 20) {
                if store.demo {
                    banner("演示模式 · 所有数据和操作均为模拟，不会连接真实设备。", icon: "play.rectangle.fill", color: .orange)
                }
                if let error = store.error { banner(error, icon: "exclamationmark.triangle.fill", color: .red) }
                if let notice = store.notice { banner(notice, icon: "checkmark.circle.fill", color: .sea) }
                if let failure = log.failure { banner(failure, icon: "doc.badge.ellipsis", color: .red) }
                if store.connected, let kind = store.kind { connectedDevice(kind) }
                else { welcome }
            }
            .padding(24)
            .frame(maxWidth: 1100)
            .frame(maxWidth: .infinity)
        }
        .background(.background.secondary)
    }

    private var welcome: some View {
        VStack(alignment: .leading, spacing: 24) {
            VStack(alignment: .leading, spacing: 10) {
                Text("让设备，彼此靠近。")
                    .font(.system(size: 30, weight: .bold, design: .rounded))
                Text("连接你的随身时钟与桌面终端。同步时间、查看状态，并为支持网络的设备配置 Wi-Fi。")
                    .font(.body).foregroundStyle(.secondary)
            }.padding(.top, 8)
            ViewThatFits(in: .horizontal) {
                HStack(spacing: 18) { ForEach(DeviceKind.allCases) { deviceCard($0) } }
                VStack(spacing: 18) { ForEach(DeviceKind.allCases) { deviceCard($0) } }
            }
            VStack(alignment: .leading, spacing: 16) {
                Label("从一次近距离连接开始", systemImage: "antenna.radiowaves.left.and.right")
                    .font(.title3.weight(.semibold))
                Text("在设备的连接设置中开启蓝牙，保持设备靠近 Mac 或 iPhone，再扫描附近设备。")
                    .foregroundStyle(.secondary)
                HStack(spacing: 12) {
                    Button { store.scan() } label: { Label("扫描附近设备", systemImage: "magnifyingglass") }
                        .buttonStyle(.borderedProminent).disabled(store.scanning || store.connecting)
                    if store.connecting || store.scanning { ProgressView().controlSize(.small) }
                }
                Text("\(store.bluetoothState) · 仅发现支持 Corallium 协议的固件")
                    .font(.caption).foregroundStyle(.secondary)
            }.padding(22).frame(maxWidth: .infinity, alignment: .leading).background(.background, in: RoundedRectangle(cornerRadius: 18))
        }
    }

    private func deviceCard(_ kind: DeviceKind) -> some View {
        VStack(alignment: .leading, spacing: 10) {
            Image(kind.image).resizable().scaledToFit().frame(maxWidth: .infinity).frame(height: 190)
                .padding(12).background(Color.white, in: RoundedRectangle(cornerRadius: 12))
                .accessibilityLabel("\(kind.name) 官方产品图片")
            Text(kind.name).font(.title3.weight(.semibold))
            HStack {
                Text(kind.subtitle).font(.subheadline).foregroundStyle(.secondary)
                Spacer()
                Link(destination: kind.source) { Image(systemName: "arrow.up.right") }
                    .accessibilityLabel("\(kind.name) 图片来源及官方资料")
            }
        }.padding(18).frame(minWidth: 230, maxWidth: .infinity)
            .background(.background, in: RoundedRectangle(cornerRadius: 18))
    }

    private func connectedDevice(_ kind: DeviceKind) -> some View {
        VStack(alignment: .leading, spacing: 20) {
            HStack(spacing: 22) {
                Image(kind.image).resizable().scaledToFit().frame(width: 130, height: 130)
                    .padding(10).background(Color.white, in: RoundedRectangle(cornerRadius: 16))
                    .accessibilityLabel("\(kind.name) 官方产品图片")
                VStack(alignment: .leading, spacing: 10) {
                    Label(store.demo ? "模拟设备" : "已连接 · BLE", systemImage: "checkmark.circle.fill")
                        .font(.caption.weight(.semibold)).foregroundStyle(Color.sea)
                    Text(store.name).font(.system(.title, design: .rounded).weight(.bold))
                    Text(store.deviceInfo["firmware"]?.string ?? "固件版本未知").font(.caption).foregroundStyle(.secondary)
                    if let date = store.lastUpdated {
                        Text("更新于 \(date.formatted(date: .omitted, time: .standard))").font(.caption).foregroundStyle(.secondary)
                    }
                }
                Spacer(minLength: 0)
            }.padding(20).frame(maxWidth: .infinity, alignment: .leading)
                .background(.background, in: RoundedRectangle(cornerRadius: 18))
            if let operation = store.pendingOperation {
                HStack { ProgressView().controlSize(.small); Text("正在与设备通信… \(operation)").font(.caption) }
            }
            LazyVGrid(columns: [GridItem(.adaptive(minimum: 270), alignment: .top)], spacing: 18) {
                timeCard
                if store.capabilities.contains("wifi.set") { wifiCard }
                if store.capabilities.contains("battery") { batteryCard }
                infoCard(kind)
            }
        }
    }

    private var timeCard: some View {
        panel("设备时间", symbol: "clock") {
            Text(deviceTime).font(.system(size: 34, weight: .medium, design: .rounded)).monospacedDigit()
            Text(timeDescription).font(.caption).foregroundStyle(.secondary)
            Divider()
            LabeledContent("时间来源", value: sourceName)
            LabeledContent("时区偏移", value: offsetName)
            Button { store.syncTime() } label: { Label("与本机同步时间", systemImage: "arrow.triangle.2.circlepath") }
                .buttonStyle(.borderedProminent).disabled(!store.ready || !store.capabilities.contains("time.set"))
            if !store.capabilities.contains("time.set") { Text("此固件未开放时间设置。").font(.caption).foregroundStyle(.secondary) }
        }
    }

    private var wifiCard: some View {
        panel("无线网络", symbol: "wifi") {
            Text(wifiState).font(.title2.weight(.semibold))
            Text(store.wifi["ssid"].string ?? "尚未连接网络").foregroundStyle(.secondary).lineLimit(2)
            Divider()
            LabeledContent("IP 地址", value: store.wifi["ip"].string ?? "—")
            LabeledContent("信号", value: store.wifi["rssi"].number.map { "\(Int($0)) dBm" } ?? "—")
            HStack {
                Button("配置 Wi-Fi") { showWiFi = true }.buttonStyle(.bordered)
                    .disabled(!store.ready || !store.capabilities.contains("wifi.set"))
                if store.capabilities.contains("wifi.forget") {
                    Button("移除", role: .destructive) { confirmForget = true }.disabled(!store.ready)
                }
            }
        }
    }

    private var batteryCard: some View {
        panel("电池与功耗", symbol: "battery.75percent") {
            Text(store.battery["percent"].number.map { "\(Int($0))%" } ?? "—")
                .font(.system(size: 34, weight: .medium, design: .rounded))
            Text(store.battery["charging"].bool.map { $0 ? "正在充电" : "电池供电" } ?? "供电状态未知")
                .font(.caption).foregroundStyle(.secondary)
            Divider()
            LabeledContent("电压", value: store.battery["millivolts"].number.map { String(format: "%.2f V", $0 / 1000) } ?? "未提供")
            LabeledContent("功耗", value: store.battery["power_mw"].number.map { String(format: "%.0f mW", $0) } ?? "未提供")
            LabeledContent("预计剩余", value: store.battery["runtime_min"].number.map { String(format: "%.0f 分钟", $0) } ?? "未提供")
            Text("功耗与续航仅显示设备报告值；未提供的数值不作推算。")
                .font(.caption).foregroundStyle(.secondary)
        }
    }

    private func infoCard(_ kind: DeviceKind) -> some View {
        panel("设备信息", symbol: "info.circle") {
            Text(kind.name).font(.headline)
            Text(store.deviceInfo["device_id"]?.string ?? "—").font(.caption.monospaced()).textSelection(.enabled)
            Divider()
            LabeledContent("运行时长", value: uptime)
            LabeledContent("协议", value: "Corallium v1")
            Text("\(store.capabilities.count) 项能力 · \((store.deviceInfo["channels"]?.strings ?? []).joined(separator: ", "))")
                .font(.caption).foregroundStyle(.secondary)
            Link("官方资料与图片来源", destination: kind.source).font(.caption)
        }
    }

    private func panel<Content: View>(_ title: String, symbol: String, @ViewBuilder content: () -> Content) -> some View {
        VStack(alignment: .leading, spacing: 14) {
            Label(title, systemImage: symbol).font(.headline).foregroundStyle(Color.sea)
            content()
        }.padding(20).frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
            .background(.background, in: RoundedRectangle(cornerRadius: 18))
    }

    private func banner(_ text: String, icon: String, color: Color) -> some View {
        Label(text, systemImage: icon).font(.callout).foregroundStyle(color)
            .padding(13).frame(maxWidth: .infinity, alignment: .leading)
            .background(color.opacity(0.09), in: RoundedRectangle(cornerRadius: 12))
    }

    private var deviceTime: String {
        guard store.time["valid"].bool == true, let milliseconds = store.time["unix_ms"].number else { return "尚未校时" }
        let formatter = DateFormatter()
        formatter.dateFormat = "HH:mm:ss"
        formatter.timeZone = TimeZone(secondsFromGMT: Int(store.time["utc_offset_min"].number ?? 0) * 60)
        return formatter.string(from: Date(timeIntervalSince1970: milliseconds / 1000))
    }
    private var timeDescription: String {
        store.time["quality"].string == "estimated" ? "估算时间 · 建议同步以校准" : "上次收到的设备时间快照"
    }
    private var sourceName: String {
        switch store.time["source"].string {
        case "rtc": "硬件 RTC"
        case "network": "网络校时"
        case "app": "Corallium"
        case "checkpoint": "存储的时间检查点"
        default: "尚未设置"
        }
    }
    private var offsetName: String {
        let offset = Int(store.time["utc_offset_min"].number ?? 0)
        return String(format: "UTC%@%02d:%02d", offset >= 0 ? "+" : "−", abs(offset) / 60, abs(offset) % 60)
    }
    private var wifiState: String {
        switch store.wifi["state"].string {
        case "connected": "已连接"
        case "connecting": "正在连接…"
        case "failed": "连接失败，请检查配置"
        default: "未连接"
        }
    }
    private var uptime: String {
        guard let milliseconds = store.status["uptime_ms"]?.number else { return "—" }
        let minutes = Int(milliseconds / 60000)
        return "\(minutes / 60) 小时 \(minutes % 60) 分钟"
    }
}

private struct WiFiSheet: View {
    @ObservedObject var store: DeviceStore
    @Environment(\.dismiss) private var dismiss
    @State private var ssid = ""
    @State private var password = ""
    @State private var openNetwork = false
    private var valid: Bool { (try? CoralliumProtocol.validateWiFi(ssid: ssid, password: openNetwork ? "" : password)) != nil && (openNetwork || !password.isEmpty) }

    var body: some View {
        NavigationStack {
            Form {
                Section("2.4 GHz 网络") {
                    TextField("网络名称（SSID）", text: $ssid)
                        .autocorrectionDisabled()
                    Toggle("开放网络（无密码）", isOn: $openNetwork)
                    if !openNetwork { SecureField("Wi-Fi 密码", text: $password) }
                }
                Section {
                    Text("配置通过当前蓝牙连接发送。密码仅用于本次发送，不写入 Corallium 日志或偏好设置。")
                        .font(.caption).foregroundStyle(.secondary)
                    if store.demo { Text("演示模式：不会配置真实网络。").foregroundStyle(.orange) }
                }
            }
            .formStyle(.grouped)
            .navigationTitle("配置 Wi-Fi")
            .toolbar {
                ToolbarItem(placement: .cancellationAction) { Button("取消") { password = ""; dismiss() } }
                ToolbarItem(placement: .confirmationAction) {
                    Button("发送配置") {
                        store.configureWiFi(ssid: ssid, password: openNetwork ? "" : password)
                        password = ""
                        dismiss()
                    }.disabled(!valid || !store.ready || !store.capabilities.contains("wifi.set"))
                }
            }
        }
#if os(macOS)
        .frame(width: 480, height: 350)
#endif
        .onDisappear { password = "" }
    }
}

private struct LogView: View {
    @ObservedObject var log: EventLog
    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            VStack(alignment: .leading, spacing: 10) {
                Text("连接的每一步，都有迹可循。")
                    .font(.title2.weight(.semibold))
                Text("JSONL · 每天一个文件 · 不记录 Wi-Fi 凭据")
                    .foregroundStyle(.secondary)
#if os(macOS)
                Button("在 Finder 中显示日志") { NSWorkspace.shared.open(log.directory) }
#else
                ShareLink("导出今日日志", item: todayLog)
#endif
                Text(log.directory.path).font(.caption.monospaced()).foregroundStyle(.secondary).textSelection(.enabled)
                if let failure = log.failure { Text(failure).foregroundStyle(.red) }
            }.padding(24)
            Divider()
            List(log.entries) { entry in
                VStack(alignment: .leading, spacing: 6) {
                    HStack {
                        Circle().fill(entry.level == "error" ? Color.red : Color.sea).frame(width: 6, height: 6)
                        Text(entry.event).font(.callout.monospaced())
                        Spacer()
                        Text(entry.timestamp).font(.caption.monospaced()).foregroundStyle(.secondary)
                    }
                    if !entry.metadata.isEmpty {
                        Text(entry.metadata.sorted { $0.key < $1.key }.map { "\($0.key)=\($0.value)" }.joined(separator: "  "))
                            .font(.caption.monospaced()).foregroundStyle(.secondary).textSelection(.enabled)
                    }
                }.padding(.vertical, 5)
            }
            Text("显示本次启动最近 200 条；完整记录保存在每日文件中。")
                .font(.caption).foregroundStyle(.secondary).padding(16)
        }
    }
    private var todayLog: URL {
        let formatter = DateFormatter()
        formatter.locale = Locale(identifier: "en_US_POSIX")
        formatter.dateFormat = "yyyy-MM-dd"
        return log.directory.appendingPathComponent(formatter.string(from: Date()) + ".jsonl")
    }
}
