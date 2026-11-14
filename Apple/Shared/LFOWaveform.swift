import Foundation
/// Matches Firmware/lfo.h; phase is read from DSP, never advanced by the UI.
enum LFOWaveform {
    static func value(phase: Double, shape: Int, held: Double = 0) -> Double {
        switch shape {
        case 0:
            let x = phase * 2 > 1 ? phase * 2 - 2 : phase * 2
            let y = 4 * x * (1 - abs(x))
            return y + 0.225 * (y * abs(y) - y)
        case 1: return 1 - 4 * abs(phase - 0.5)
        case 2: return phase < 0.5 ? 1 : -1
        case 3: return 2 * phase - 1
        default: return held
        }
    }
}
