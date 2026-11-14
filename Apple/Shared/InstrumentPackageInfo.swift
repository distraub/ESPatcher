import Foundation

struct InstrumentPackageInfo: Decodable, Sendable {
    let id: String
    let name: String
    let version: String
    let selectedEngine: Int
    let engines: [PackageEngineInfo]
    var engine: PackageEngineInfo { engines[selectedEngine] }
}

struct PackageEngineInfo: Decodable, Sendable {
    let id: String
    let name: String
    let defaults: [Double]
    let scenes: [PackageSceneInfo]
}

struct PackageSceneInfo: Decodable, Sendable {
    let title: String
    let subtitle: String
    let layout: Int
    let bindings: [Int]
    let labels: [String]
    let pads: [Int]
}
