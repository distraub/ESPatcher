import SwiftUI

/// Value-only layer shared by voice, mixer, and humanize controls.
struct StudioKnobIndicator: View {
    let fraction: Double
    let tint: Color
    var centerFraction: Double? = nil
    var isEditing = false

    var body: some View {
        GeometryReader { geometry in
            let diameter = min(geometry.size.width, geometry.size.height)
            let detailScale = diameter / 124
            let origin = centerFraction ?? 0
            ZStack {
                Circle()
                    .trim(from: min(origin, fraction) * 0.75, to: max(origin, fraction) * 0.75)
                    .stroke(tint, style: StrokeStyle(lineWidth: max(1.5, 3 * detailScale), lineCap: .round))
                    .rotationEffect(.degrees(135))
                    .padding(11 * detailScale)
                    .shadow(color: tint.opacity(isEditing ? 0.55 : 0.4), radius: 3 * detailScale)
                if let centerFraction {
                    Capsule().fill(tint.opacity(0.6))
                        .frame(width: 1, height: diameter * 0.05)
                        .offset(y: -diameter * 0.465)
                        .rotationEffect(.degrees(-135 + 270 * centerFraction))
                }
                Capsule().fill(tint)
                    .frame(width: max(1.5, 3 * detailScale), height: diameter * 0.15)
                    .offset(y: -diameter * 0.24)
                    .rotationEffect(.degrees(-135 + 270 * fraction))
                    .shadow(color: tint.opacity(0.35), radius: 2 * detailScale)
            }
        }
        .allowsHitTesting(false)
        .accessibilityHidden(true)
    }
}
