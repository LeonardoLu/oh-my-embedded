import Foundation
import Combine

struct LogEntry: Identifiable, Codable {
    let id: UUID
    let timestamp: String
    let level: String
    let event: String
    let metadata: [String: String]
}

@MainActor
final class EventLog: ObservableObject {
    @Published private(set) var entries: [LogEntry] = []
    @Published private(set) var failure: String?
    let directory: URL
    private let iso = ISO8601DateFormatter()
    private let day = DateFormatter()
    private let allowedKeys: Set<String> = ["operation", "request_id", "device_id", "model", "code", "domain", "mode", "state", "bytes", "count", "result"]

    init(directory: URL? = nil) {
        let bundleID = Bundle.main.bundleIdentifier ?? "com.github.leonardolu.Corallium"
        self.directory = directory ?? FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
            .appendingPathComponent(bundleID, isDirectory: true).appendingPathComponent("logs", isDirectory: true)
        iso.formatOptions = [.withInternetDateTime, .withFractionalSeconds]
        day.locale = Locale(identifier: "en_US_POSIX")
        day.dateFormat = "yyyy-MM-dd"
    }

    func record(_ event: String, level: String = "info", metadata: [String: String] = [:], date: Date = Date()) {
        let safe = metadata.filter { allowedKeys.contains($0.key) }.mapValues { String($0.prefix(120)) }
        let entry = LogEntry(id: UUID(), timestamp: iso.string(from: date), level: level, event: event, metadata: safe)
        entries.insert(entry, at: 0)
        if entries.count > 200 { entries.removeLast(entries.count - 200) }
        do {
            try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true,
                                                    attributes: [.posixPermissions: 0o700])
            day.timeZone = .current
            let file = directory.appendingPathComponent(day.string(from: date) + ".jsonl")
            if !FileManager.default.fileExists(atPath: file.path) {
                guard FileManager.default.createFile(atPath: file.path, contents: nil,
                                                     attributes: [.posixPermissions: 0o600]) else {
                    throw CocoaError(.fileWriteUnknown)
                }
            }
            var data = try JSONEncoder().encode(entry)
            data.append(0x0A)
            let handle = try FileHandle(forWritingTo: file)
            defer { try? handle.close() }
            try handle.seekToEnd()
            try handle.write(contentsOf: data)
            failure = nil
        } catch {
            failure = "无法写入日志：\(error.localizedDescription)"
        }
    }
}
