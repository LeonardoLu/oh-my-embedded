import SwiftUI
#if os(macOS)
import AppKit
#endif

@main
struct CoralliumApp: App {
    @StateObject private var store: DeviceStore
    @StateObject private var log: EventLog

    init() {
        if ProcessInfo.processInfo.arguments.contains("--self-test") {
            do { try CoralliumSelfTests.run(); print("Corallium self-test: PASS"); exit(0) }
            catch { print("Corallium self-test: FAIL: \(error)"); exit(1) }
        }
        let log = EventLog()
        _log = StateObject(wrappedValue: log)
        let store = DeviceStore(log: log, demo: ProcessInfo.processInfo.arguments.contains("--demo"))
        _store = StateObject(wrappedValue: store)
        if ProcessInfo.processInfo.arguments.contains("--integration-test") {
            Task { @MainActor in
                do { try await CoralliumSelfTests.runDemo(store); exit(0) }
                catch { print("Corallium demo integration: FAIL: \(error)"); exit(1) }
            }
        }
    }

    var body: some Scene {
        WindowGroup("Corallium", id: "main") {
            ContentView(store: store, log: log)
                .tint(Color.coral)
#if os(macOS)
                .frame(minWidth: 860, minHeight: 620)
#endif
        }
        .defaultSize(width: 1060, height: 760)
#if os(macOS)
        MenuBarExtra("Corallium", systemImage: store.connected ? "circle.hexagongrid.fill" : "circle.hexagongrid") {
            StatusMenu(store: store)
        }
#endif
    }
}

#if os(macOS)
private struct StatusMenu: View {
    @ObservedObject var store: DeviceStore
    @Environment(\.openWindow) private var openWindow
    var body: some View {
        Text(store.demo ? "Corallium · 演示模式" : "Corallium")
        Text(store.connected ? store.name : store.bluetoothState)
        Divider()
        Button("打开 Corallium") { openWindow(id: "main"); NSApp.activate(ignoringOtherApps: true) }
            .keyboardShortcut("o")
        Button("同步设备时间") { store.syncTime() }
            .disabled(!store.ready || !store.capabilities.contains("time.set"))
        Button("刷新设备状态") { store.refresh() }.disabled(!store.ready)
        if store.connected { Button("断开连接") { store.disconnect() } }
        Divider()
        Button("退出 Corallium") { NSApp.terminate(nil) }.keyboardShortcut("q")
    }
}
#endif
