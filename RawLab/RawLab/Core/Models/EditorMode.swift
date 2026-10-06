import Foundation

enum EditorMode: String, CaseIterable, Identifiable {
    case adjust
    case lut
    case crop

    var id: String { rawValue }

    var iconName: String {
        switch self {
        case .adjust:
            return "slider.horizontal.3"
        case .lut:
            return "circle.lefthalf.filled"
        case .crop:
            return "crop"
        }
    }
}
