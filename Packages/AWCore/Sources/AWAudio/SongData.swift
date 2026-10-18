import AWCore

/// The JSON the sound engine reads: one location exactly as the save writes it, and the master node.
enum SongData {
    static func json(for state: AppState) throws -> (location: String, master: String) {
        let encoder = JSONValueEncoder()
        let location = try encoder.encode(state.locations[state.currentLocation])
        let master = try encoder.encode(state.master)
        return (JSONWriter.string(location), JSONWriter.string(master))
    }
}
