import SwiftUI

@main
struct AndrewsWorldApp: App {
    @State private var store = AppStore()
    @Environment(\.scenePhase) private var scenePhase

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environment(store)
                .task { await store.load() }
        }
        .onChange(of: scenePhase) { _, phase in
            if phase != .active { store.flush() }
        }
    }
}
