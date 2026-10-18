import AWAudio
import AWCore
import Foundation
import Observation

/// The one store the app hangs off. Views read `state`; every edit goes through `update` so it gets saved.
/// Every change is also posted to the sound engine, which builds its own snapshot off the main thread.
/// (Undo snapshots join it in milestone 5.)
@Observable
@MainActor
final class AppStore {
    private(set) var state: AppState?
    /// A short message for the banner: load recovery, import results, errors.
    var notice: String?
    /// The state before the last import, so an import can be undone without asking first.
    private(set) var beforeImport: AppState?

    /// Play, stop and the playhead.
    let transport = Transport()

    @ObservationIgnored private let saves: SaveStore?

    init() {
        saves = try? SaveStore(url: SaveStore.defaultURL())
    }

    func load() async {
        guard state == nil else { return }
        guard let saves else {
            notice = "Couldn't open the app's storage folder."
            state = try? DefaultState.load()
            if let state { transport.post(state) }
            return
        }
        do {
            let (s, outcome) = try await saves.load()
            state = s
            transport.post(s)
            if case .recovered(let backup, _) = outcome {
                notice = "Your last save couldn't be read, so the demo songs are loaded. The old file was kept as \(backup.lastPathComponent)."
            }
        } catch {
            notice = "Couldn't load: \(error)"
        }
    }

    func update(_ edit: (inout AppState) -> Void) {
        guard var s = state else { return }
        edit(&s)
        state = s
        save()
    }

    private func save() {
        guard let state else { return }
        transport.post(state)
        guard let saves else { return }
        Task { await saves.scheduleSave(state) }
    }

    /// Call when the app leaves the foreground so a pending save isn't lost.
    func flush() {
        guard let saves else { return }
        Task { try? await saves.flush() }
    }

    // MARK: Files app import and export

    func importSave(from url: URL) {
        let scoped = url.startAccessingSecurityScopedResource()
        defer { if scoped { url.stopAccessingSecurityScopedResource() } }
        do {
            let loaded = try SaveCodec.load(Data(contentsOf: url))
            beforeImport = state
            state = loaded
            save()
            notice = "Imported \(url.lastPathComponent)."
        } catch {
            notice = "Couldn't import \(url.lastPathComponent). \(error)"
        }
    }

    func undoImport() {
        guard let previous = beforeImport else { return }
        state = previous
        beforeImport = nil
        save()
        notice = nil
    }

    func exportDocument() -> SaveDocument? {
        guard let state, let data = try? SaveCodec.encode(state) else { return nil }
        return SaveDocument(data: data)
    }
}
