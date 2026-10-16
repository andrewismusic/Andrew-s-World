// swift-tools-version: 5.9
// AWCore holds everything in Andrew's World that has no UI: the save model, its JSON codec and storage.
// It builds for macOS too, so `swift test` runs without a simulator.
import PackageDescription

let package = Package(
    name: "AWCore",
    platforms: [.iOS(.v17), .macOS(.v14)],
    products: [
        .library(name: "AWCore", targets: ["AWCore"]),
    ],
    targets: [
        .target(
            name: "AWCore",
            resources: [.copy("Resources/DefaultState.json")]
        ),
        .testTarget(
            name: "AWCoreTests",
            dependencies: ["AWCore"],
            resources: [.copy("Fixtures")]
        ),
    ]
)
