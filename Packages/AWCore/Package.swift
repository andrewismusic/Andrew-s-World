// swift-tools-version: 5.9
// AWCore holds everything in Andrew's World that has no UI: the save model, its JSON codec and storage.
// AWDSP is the sound engine (C++ behind a C header) and AWAudio connects it to the device's audio.
// Everything builds for macOS too, so `swift test` runs without a simulator.
import PackageDescription

let package = Package(
    name: "AWCore",
    platforms: [.iOS(.v17), .macOS(.v14)],
    products: [
        .library(name: "AWCore", targets: ["AWCore"]),
        .library(name: "AWAudio", targets: ["AWAudio"]),
    ],
    targets: [
        .target(
            name: "AWCore",
            resources: [.copy("Resources/DefaultState.json")]
        ),
        .target(
            name: "AWDSP",
            exclude: ["CMakeLists.txt"],
            // the render path must be optimised even in Debug builds, or it can't keep up in real time
            cxxSettings: [.unsafeFlags(["-O2"])]
        ),
        .target(
            name: "AWAudio",
            dependencies: ["AWCore", "AWDSP"]
        ),
        .testTarget(
            name: "AWCoreTests",
            dependencies: ["AWCore"],
            resources: [.copy("Fixtures")]
        ),
        .testTarget(
            name: "AWDSPTests",
            dependencies: ["AWCore", "AWDSP"]
        ),
    ],
    cxxLanguageStandard: .cxx20
)
