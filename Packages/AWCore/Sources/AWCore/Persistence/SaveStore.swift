import Foundation

/// Keeps the state in one JSON file (in Application Support by default), written atomically a short while
/// after the last change, like the prototype's debounced localStorage save.
public actor SaveStore {
    public enum LoadOutcome: Equatable, Sendable {
        /// Loaded the saved file.
        case loaded
        /// No save yet: started from the demo state.
        case fresh
        /// The save couldn't be read; it was moved aside to `backup` and the demo state loaded instead.
        case recovered(backup: URL, reason: String)
    }

    public let url: URL
    private var pending: Task<Void, Never>?
    private var pendingState: AppState?

    public init(url: URL) {
        self.url = url
    }

    /// `Application Support/AndrewsWorld/state.json`
    public static func defaultURL() throws -> URL {
        let dir = try FileManager.default.url(for: .applicationSupportDirectory, in: .userDomainMask, appropriateFor: nil, create: true)
            .appendingPathComponent("AndrewsWorld", isDirectory: true)
        return dir.appendingPathComponent("state.json")
    }

    /// Reads the save, or the demo state if there is none. A save that can't be read is never overwritten:
    /// it is renamed with a timestamp first.
    public func load() throws -> (AppState, LoadOutcome) {
        guard FileManager.default.fileExists(atPath: url.path) else {
            return (try DefaultState.load(), .fresh)
        }
        do {
            return (try SaveCodec.load(Data(contentsOf: url)), .loaded)
        } catch {
            let stamp = ISO8601DateFormatter().string(from: Date()).replacingOccurrences(of: ":", with: "-")
            let backup = url.deletingPathExtension().appendingPathExtension("unreadable-\(stamp).json")
            try FileManager.default.moveItem(at: url, to: backup)
            return (try DefaultState.load(), .recovered(backup: backup, reason: String(describing: error)))
        }
    }

    /// Saves `state` after `delay`, replacing any save still waiting.
    public func scheduleSave(_ state: AppState, delay: Duration = .milliseconds(300)) {
        pending?.cancel()
        pendingState = state
        pending = Task {
            try? await Task.sleep(for: delay)
            guard !Task.isCancelled else { return }
            try? self.flush()  // the task runs on this actor, so no await
        }
    }

    /// Writes any waiting save now (call when the app goes to the background).
    public func flush() throws {
        guard let s = pendingState else { return }
        pendingState = nil
        pending?.cancel()
        pending = nil
        try write(s)
    }

    /// Writes immediately and atomically.
    public func write(_ state: AppState) throws {
        try FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
        try SaveCodec.encode(state).write(to: url, options: .atomic)
    }
}
