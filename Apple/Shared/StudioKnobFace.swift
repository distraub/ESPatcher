import SwiftUI

/// NF Studio's machined dial, scaled to the existing GlitchDrums control sizes.
/// Theme colors retain their identity under NF Studio-style material lighting.
/// Static decoration stays separate from the live arc and pointer layer.
struct StudioKnobFace: View {
    let tint: Color
    @Environment(\.synthTheme) private var theme

    var body: some View {
        GeometryReader { geometry in
            let diameter = min(geometry.size.width, geometry.size.height)
            // Scale the 124 pt CEILING reference uniformly, including its bevel.
            let detailScale = diameter / 124
            let isLight = theme.isLight
            ZStack {
                Canvas { context, size in
                    let center = CGPoint(x: size.width / 2, y: size.height / 2)
                    let divisions = diameter < 44 ? 10 : 30
                    for index in 0...divisions {
                        let major = index.isMultiple(of: 5)
                        let angle = (135 + Double(index) * 270 / Double(divisions)) * .pi / 180
                        let outer = diameter * 0.49
                        let inner = diameter * (major ? 0.44 : 0.46)
                        var path = Path()
                        path.move(to: CGPoint(x: center.x + cos(angle) * inner, y: center.y + sin(angle) * inner))
                        path.addLine(to: CGPoint(x: center.x + cos(angle) * outer, y: center.y + sin(angle) * outer))
                        context.stroke(path, with: .color(theme.text.opacity(major ? 0.55 : 0.20)), lineWidth: 1)
                    }
                }
                Circle().trim(from: 0, to: 0.75)
                    .stroke(tint.opacity(0.12), lineWidth: max(1.5, 3 * detailScale))
                    .rotationEffect(.degrees(135))
                    .padding(11 * detailScale)
                Circle().fill(theme.background)
                    .padding(16 * detailScale)
                    .shadow(color: .black.opacity(isLight ? 0.22 : 0.8), radius: 3 * detailScale, y: 4 * detailScale)
                Circle().fill(LinearGradient(
                    colors: [theme.text.opacity(0.24), theme.background, theme.text.opacity(0.07)],
                    startPoint: .topLeading, endPoint: .bottomTrailing))
                    .padding(17 * detailScale)
                Circle().fill(LinearGradient(
                    colors: [theme.surface, theme.background],
                    startPoint: .topLeading, endPoint: .bottomTrailing))
                    .overlay { Circle().strokeBorder(theme.text.opacity(0.09)) }
                    .padding(21 * detailScale)
            }
        }
        .allowsHitTesting(false)
        .accessibilityHidden(true)
    }
}
