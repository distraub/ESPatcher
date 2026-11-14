import SwiftUI

struct ParameterSlider: View {
    let title: String
    @Binding var value: Double
    let range: ClosedRange<Double>
    let unit: String
    var body: some View {
        HStack {
            Text(title).frame(width: 70, alignment: .leading)
            Slider(value: $value, in: range).accessibilityLabel(title)
            Text(value.formatted(.number.precision(.fractionLength(unit == "Hz" ? 0 : 3))) + " " + unit)
                .font(.caption.monospaced()).frame(width: 100, alignment: .trailing)
        }
    }
}
