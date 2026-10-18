import AWAudio
import AWCore
import SwiftUI
import UniformTypeIdentifiers

/// The inspector for the loaded save, with Files import and export and, since milestone 2, playback: tap a song in
/// the current location to play it. The real chrome (header, rail, map, pane) replaces this from milestone 4.
struct ContentView: View {
    @Environment(AppStore.self) private var store
    @State private var importing = false
    @State private var exporting = false
    @State private var exportDoc: SaveDocument?

    var body: some View {
        NavigationStack {
            Group {
                if let state = store.state {
                    StateInspector(state: state)
                } else {
                    ProgressView()
                }
            }
            .navigationTitle("Andrew's World")
            .toolbar {
                ToolbarItemGroup(placement: .primaryAction) {
                    Button("Import", systemImage: "square.and.arrow.down") { importing = true }
                    Button("Export", systemImage: "square.and.arrow.up") {
                        exportDoc = store.exportDocument()
                        exporting = exportDoc != nil
                    }
                    .disabled(store.state == nil)
                }
            }
            .safeAreaInset(edge: .bottom) {
                VStack(spacing: 0) {
                    if let notice = store.notice {
                        NoticeBanner(text: notice, canUndo: store.beforeImport != nil)
                    }
                    if let state = store.state { TransportBar(state: state) }
                }
            }
        }
        .fileImporter(isPresented: $importing, allowedContentTypes: [.json]) { result in
            switch result {
            case .success(let url): store.importSave(from: url)
            case .failure(let e): store.notice = "Couldn't open the file. \(e.localizedDescription)"
            }
        }
        .fileExporter(isPresented: $exporting, document: exportDoc, contentType: .json, defaultFilename: "andrews-world-save") { result in
            if case .failure(let e) = result { store.notice = "Couldn't export. \(e.localizedDescription)" }
        }
    }
}

private struct NoticeBanner: View {
    @Environment(AppStore.self) private var store
    let text: String
    let canUndo: Bool

    var body: some View {
        HStack(alignment: .firstTextBaseline, spacing: 12) {
            Text(text).font(.footnote).frame(maxWidth: .infinity, alignment: .leading)
            if canUndo { Button("Undo import") { store.undoImport() }.font(.footnote.bold()) }
            Button("Dismiss", systemImage: "xmark") { store.notice = nil }
                .labelStyle(.iconOnly)
                .font(.footnote)
        }
        .padding(12)
        .background(.regularMaterial, in: RoundedRectangle(cornerRadius: 12))
        .padding(.horizontal, 16)
        .padding(.bottom, 8)
    }
}

/// Play/stop and where the playhead is: song, part letter, bar and beat.
private struct TransportBar: View {
    @Environment(AppStore.self) private var store
    let state: AppState

    var body: some View {
        let transport = store.transport
        let head = transport.playhead
        let songs = state.songs
        HStack(spacing: 16) {
            Button {
                if transport.isPlaying { transport.stop() } else { transport.play(PlayContext(level: 0, song: head.song)) }
            } label: {
                Image(systemName: transport.isPlaying ? "stop.fill" : "play.fill")
                    .font(.title2)
                    .frame(width: 44, height: 44)
            }
            .accessibilityLabel(transport.isPlaying ? "Stop" : "Play")
            VStack(alignment: .leading, spacing: 2) {
                if songs.indices.contains(head.song) {
                    let song = songs[head.song]
                    Text(song.name).font(.subheadline.bold())
                    let part = song.parts.indices.contains(head.part) ? song.parts[head.part].name : "–"
                    // 12 clock ticks to a quarter note
                    Text("Part \(part) · bar \(head.bar + 1) · beat \(head.tick / 12 + 1)")
                        .font(.caption.monospacedDigit())
                        .foregroundStyle(.secondary)
                }
            }
            Spacer()
        }
        .padding(.horizontal, 16)
        .padding(.vertical, 8)
        .background(.bar)
    }
}

private struct StateInspector: View {
    @Environment(AppStore.self) private var store
    let state: AppState

    var body: some View {
        List {
            ForEach(Array(state.locations.enumerated()), id: \.offset) { i, location in
                Section {
                    ForEach(Array(location.songs.enumerated()), id: \.offset) { s, song in
                        if i == state.currentLocation {
                            Button {
                                store.transport.play(PlayContext(level: 0, song: s))
                            } label: {
                                SongRow(song: song, master: state.master)
                            }
                            .buttonStyle(.plain)
                            .accessibilityHint("Plays this song")
                        } else {
                            SongRow(song: song, master: state.master)
                        }
                    }
                } header: {
                    HStack {
                        Text(location.name)
                        Spacer()
                        if i == state.currentLocation { Text("Current").foregroundStyle(.secondary) }
                    }
                }
            }
            Section("Save") {
                LabeledContent("Format version", value: "\(state.v)")
                LabeledContent("Notes", value: "\(state.allNotes.count)")
                LabeledContent("Next note id", value: "\(state.nextNoteID)")
            }
        }
    }
}

private struct SongRow: View {
    let song: Song
    let master: MasterNode

    /// Tempo as the prototype reads it: the deepest lock wins, defaulting to 70/150 (120 BPM).
    private var bpm: Int {
        let v = song.locks["g.bpm"] ?? master.locks["g.bpm"] ?? 70.0 / 150.0
        return 50 + Int((v * 150).rounded())
    }

    var body: some View {
        HStack(spacing: 12) {
            Circle()
                .fill(Color(hue: song.hue.truncatingRemainder(dividingBy: 360) / 360, saturation: 0.6, brightness: 0.85))
                .frame(width: 12, height: 12)
            VStack(alignment: .leading, spacing: 2) {
                Text(song.name)
                Text(song.parts.map(\.name).joined(separator: " · "))
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
            Spacer()
            Text("\(bpm) BPM")
                .font(.caption.monospacedDigit())
                .foregroundStyle(.secondary)
        }
    }
}
