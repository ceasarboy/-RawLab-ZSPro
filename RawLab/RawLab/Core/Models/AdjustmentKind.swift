import Foundation

enum AdjustmentKind: String, CaseIterable, Identifiable {
    case exposure
    case contrast
    case highlights
    case shadows
    case toneCurve
    case saturation
    case temperature
    case tint
    case noiseReduction
    case sharpening

    var id: String { rawValue }

    var title: String {
        switch self {
        case .exposure:
            return "Exposure"
        case .contrast:
            return "Contrast"
        case .highlights:
            return "Highlights"
        case .shadows:
            return "Shadows"
        case .toneCurve:
            return "Tone Curve"
        case .saturation:
            return "Saturation"
        case .temperature:
            return "Temperature"
        case .tint:
            return "Tint"
        case .noiseReduction:
            return "Noise Reduction"
        case .sharpening:
            return "Sharpening"
        }
    }

    var iconName: String {
        switch self {
        case .exposure:
            return "sun.max"
        case .contrast:
            return "circle.lefthalf.filled"
        case .highlights:
            return "sun.max.fill"
        case .shadows:
            return "sun.min.fill"
        case .toneCurve:
            return "waveform.path.ecg"
        case .saturation:
            return "paintpalette"
        case .temperature:
            return "thermometer"
        case .tint:
            return "eyedropper"
        case .noiseReduction:
            return "sparkles"
        case .sharpening:
            return "wand.and.stars"
        }
    }

    var range: ClosedRange<Double> {
        switch self {
        case .exposure:
            return -2...2
        case .contrast:
            return 0.5...1.5
        case .highlights:
            return -1...1
        case .shadows:
            return -1...1
        case .toneCurve:
            return -1...1
        case .saturation:
            return 0...2
        case .temperature:
            return 2000...10000
        case .tint:
            return -100...100
        case .noiseReduction:
            return 0...1
        case .sharpening:
            return 0...1.5
        }
    }

    var step: Double {
        switch self {
        case .exposure:
            return 0.1
        case .contrast:
            return 0.05
        case .highlights, .shadows:
            return 0.05
        case .toneCurve:
            return 0.02
        case .saturation:
            return 0.05
        case .temperature:
            return 100
        case .tint:
            return 1
        case .noiseReduction:
            return 0.05
        case .sharpening:
            return 0.05
        }
    }

    func value(from settings: RawSettings) -> Double {
        switch self {
        case .exposure:
            return settings.exposure
        case .contrast:
            return settings.contrast
        case .highlights:
            return settings.highlights
        case .shadows:
            return settings.shadows
        case .toneCurve:
            return settings.toneCurve
        case .saturation:
            return settings.saturation
        case .temperature:
            return settings.temperature
        case .tint:
            return settings.tint
        case .noiseReduction:
            return settings.noiseReduction
        case .sharpening:
            return settings.sharpening
        }
    }

    func setValue(_ value: Double, in settings: inout RawSettings) {
        switch self {
        case .exposure:
            settings.exposure = value
        case .contrast:
            settings.contrast = value
        case .highlights:
            settings.highlights = value
        case .shadows:
            settings.shadows = value
        case .toneCurve:
            settings.toneCurve = value
        case .saturation:
            settings.saturation = value
        case .temperature:
            settings.temperature = value
        case .tint:
            settings.tint = value
        case .noiseReduction:
            settings.noiseReduction = value
        case .sharpening:
            settings.sharpening = value
        }
    }

    func valueLabel(for value: Double) -> String {
        switch self {
        case .exposure:
            return String(format: "%.1f", value)
        case .contrast, .saturation, .noiseReduction, .sharpening:
            return String(format: "%.2f", value)
        case .highlights, .shadows, .toneCurve:
            return String(format: "%+.2f", value)
        case .temperature:
            return String(format: "%.0fK", value)
        case .tint:
            return String(format: "%.0f", value)
        }
    }
}
