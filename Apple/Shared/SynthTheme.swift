// Generated from Assets/themes.json by Tools/generate_themes.py.
import SwiftUI

struct SynthTheme: Identifiable, Sendable {
    let id: Int
    let name: String
    let colors: [UInt16]
    private func color(_ index: Int) -> Color {
        let raw = colors[index]
        return Color(red: Double((raw >> 11) & 31) / 31, green: Double((raw >> 5) & 63) / 63, blue: Double(raw & 31) / 31)
    }
    var accent: Color { color(0) }
    var alternate: Color { color(1) }
    var surface: Color { color(2) }
    var text: Color { color(3) }
    var inverse: Color { color(4) }
    var background: Color { color(5) }
    var chrome: Color { color(6) }
    var isLight: Bool {
        let raw = colors[5]
        let r = Double((raw >> 11) & 31) / 31, g = Double((raw >> 5) & 63) / 63, b = Double(raw & 31) / 31
        return 0.2126 * r + 0.7152 * g + 0.0722 * b > 0.5
    }
    static let defaultIndex = 24
    static let all: [SynthTheme] = [
        SynthTheme(id: 0, name: "GlitchBreaks", colors: [0x71f, 0xc69b, 0x2146, 0xc67b, 0x41, 0x862, 0x7412]),
        SynthTheme(id: 1, name: "BreaksGlitch", colors: [0xc69b, 0x71f, 0x2146, 0xc67b, 0x41, 0x862, 0x7412]),
        SynthTheme(id: 2, name: "GlitchStep", colors: [0x179f, 0xf87b, 0x2146, 0xc67b, 0x41, 0x862, 0x7412]),
        SynthTheme(id: 3, name: "StepGlitch", colors: [0xf87b, 0x179f, 0x2146, 0xc67b, 0x41, 0x862, 0x7412]),
        SynthTheme(id: 4, name: "Pine Wine", colors: [0x2ca, 0x58c6, 0xceda, 0x1944, 0xffff, 0xefbe, 0x63ee]),
        SynthTheme(id: 5, name: "Wine Pine", colors: [0x58c6, 0x2ca, 0xceda, 0x1944, 0xffff, 0xefbe, 0x63ee]),
        SynthTheme(id: 6, name: "Lemon Lime", colors: [0x6c6, 0xdd01, 0x2146, 0xc67b, 0x41, 0x862, 0x7412]),
        SynthTheme(id: 7, name: "Lime Lemon", colors: [0xdd01, 0x6c6, 0x2146, 0xc67b, 0x41, 0x862, 0x7412]),
        SynthTheme(id: 8, name: "Plum Cherry", colors: [0xc29f, 0xfa2c, 0x2146, 0xc67b, 0x41, 0x862, 0x7412]),
        SynthTheme(id: 9, name: "Cherry Plum", colors: [0xfa2c, 0xc29f, 0x2146, 0xc67b, 0x41, 0x862, 0x7412]),
        SynthTheme(id: 10, name: "Ember Flow", colors: [0xfae0, 0x4df, 0x2146, 0xc67b, 0x41, 0x862, 0x7412]),
        SynthTheme(id: 11, name: "Flow Ember", colors: [0x4df, 0xfae0, 0x2146, 0xc67b, 0x41, 0x862, 0x7412]),
        SynthTheme(id: 12, name: "Joker Rogue", colors: [0xc29f, 0xf00, 0x2146, 0xc67b, 0x41, 0x862, 0x7412]),
        SynthTheme(id: 13, name: "Rogue Joker", colors: [0xf00, 0xc29f, 0x2146, 0xc67b, 0x41, 0x862, 0x7412]),
        SynthTheme(id: 14, name: "Sun Harvest", colors: [0xfd60, 0xfb0b, 0x2146, 0xc67b, 0x41, 0x862, 0x7412]),
        SynthTheme(id: 15, name: "Harvest Sun", colors: [0xfb0b, 0xfd60, 0x2146, 0xc67b, 0x41, 0x862, 0x7412]),
        SynthTheme(id: 16, name: "Pale Ice", colors: [0x76bf, 0xd63e, 0x2146, 0xc67b, 0x41, 0x862, 0x7412]),
        SynthTheme(id: 17, name: "Ice Pale", colors: [0xd63e, 0x76bf, 0x2146, 0xc67b, 0x41, 0x862, 0x7412]),
        SynthTheme(id: 18, name: "Night Drive", colors: [0xfce3, 0x463f, 0x2968, 0xcebc, 0x42, 0x883, 0x73f3]),
        SynthTheme(id: 19, name: "Drive Night", colors: [0x463f, 0xfce3, 0x2968, 0xcebc, 0x42, 0x883, 0x73f3]),
        SynthTheme(id: 20, name: "Vapor Wave", colors: [0xfa77, 0x479b, 0x3948, 0xe69d, 0x1023, 0x1884, 0x8b93]),
        SynthTheme(id: 21, name: "Wave Vapor", colors: [0x479b, 0xfa77, 0x3948, 0xe69d, 0x1023, 0x1884, 0x8b93]),
        SynthTheme(id: 22, name: "Terminal Phosphor", colors: [0x37ec, 0x757c, 0x2185, 0xc6f9, 0x60, 0x8a1, 0x6c6e]),
        SynthTheme(id: 23, name: "Phosphor Terminal", colors: [0x757c, 0x37ec, 0x2185, 0xc6f9, 0x60, 0x8a1, 0x6c6e]),
        SynthTheme(id: 24, name: "Desert Dusk", colors: [0xf5ed, 0x1e16, 0x3923, 0xe677, 0x1061, 0x1861, 0x72e9]),
        SynthTheme(id: 25, name: "Dusk Desert", colors: [0x1e16, 0xf5ed, 0x3923, 0xe677, 0x1061, 0x1861, 0x72e9]),
        SynthTheme(id: 26, name: "Voltage Spark", colors: [0xfec0, 0xbb9f, 0x3186, 0xd6ba, 0x861, 0x10a3, 0x8410]),
        SynthTheme(id: 27, name: "Spark Voltage", colors: [0xbb9f, 0xfec0, 0x3186, 0xd6ba, 0x861, 0x10a3, 0x8410]),
        SynthTheme(id: 28, name: "Ghost Static", colors: [0xf79e, 0xad57, 0x2966, 0xd69b, 0x841, 0x1082, 0x7bd1]),
        SynthTheme(id: 29, name: "Static Ghost", colors: [0xad57, 0xf79e, 0x2966, 0xd69b, 0x841, 0x1082, 0x7bd1]),
        SynthTheme(id: 30, name: "Candy Land", colors: [0xff9e, 0xfa95, 0x3967, 0xe6dc, 0x1042, 0x18a4, 0x8bd1]),
        SynthTheme(id: 31, name: "Land Candy", colors: [0xfa95, 0xff9e, 0x3967, 0xe6dc, 0x1042, 0x18a4, 0x8bd1]),
        SynthTheme(id: 32, name: "Dark Knight", colors: [0x5cbd, 0x9d57, 0x2967, 0xd6dc, 0x41, 0x883, 0x7412]),
        SynthTheme(id: 33, name: "Knight Dark", colors: [0x9d57, 0x5cbd, 0x2967, 0xd6dc, 0x41, 0x883, 0x7412]),
        SynthTheme(id: 34, name: "Bubblegum", colors: [0xb8cc, 0x6196, 0xe67b, 0x30c5, 0xffdf, 0xf77e, 0x8b6f]),
        SynthTheme(id: 35, name: "Gum Bubble", colors: [0x6196, 0xb8cc, 0xe67b, 0x30c5, 0xffdf, 0xf77e, 0x8b6f]),
        SynthTheme(id: 36, name: "Seafoam", colors: [0x38c, 0xa9c5, 0xcefa, 0x1144, 0xffff, 0xefbe, 0x640e]),
        SynthTheme(id: 37, name: "Foam Sea", colors: [0xa9c5, 0x38c, 0xcefa, 0x1144, 0xffff, 0xefbe, 0x640e]),
    ]
}

extension EnvironmentValues {
    @Entry var synthTheme: SynthTheme = SynthTheme.all[SynthTheme.defaultIndex]
}
