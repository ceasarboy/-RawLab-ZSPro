import Foundation

struct AdjustmentPanelLayout {
    var collapsed = false
    private var preferredHeight: CGFloat = 260

    func height(available: CGFloat) -> CGFloat {
        collapsed ? 0 : min(preferredHeight, Self.maximum(available))
    }

    mutating func resize(to height: CGFloat, available: CGFloat) {
        preferredHeight = min(max(height, 220), Self.maximum(available))
    }

    private static func maximum(_ available: CGFloat) -> CGFloat {
        max(220, min(380, available * 0.5))
    }
}
