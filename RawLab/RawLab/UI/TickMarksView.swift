import SwiftUI
import UIKit

struct TickMarksView: View {
    let range: ClosedRange<Double>
    let step: Double
    @Binding var value: Double
    let originalValue: Double
    var onEditingChanged: (Bool) -> Void = { _ in }

    @State private var animatedIndex = 0
    @State private var coloredIndex = 0
    @State private var dragStartIndex: Int?
    @State private var isDragging = false
    @State private var lastHapticIndex = 0
    @State private var stepFeedback = UIImpactFeedbackGenerator(style: .light)
    @State private var majorFeedback = UIImpactFeedbackGenerator(style: .heavy)

    var body: some View {
        GeometryReader { geometry in
            let width = geometry.size.width
            let height = geometry.size.height
            let tickWidth: CGFloat = 2
            let trackWidth = max(width - tickWidth, 1)
            let spacing = trackWidth / CGFloat(max(tickCount - 1, 1))
            let shortHeight = height * 0.45
            let longHeight = height * 0.8
            let dotRadius: CGFloat = 3

            ZStack(alignment: .bottomLeading) {
                ForEach(0..<tickCount, id: \.self) { index in
                    let tickHeight = index == animatedIndex ? longHeight : shortHeight
                    Rectangle()
                        .fill(index == coloredIndex ? Color.yellow : Color.secondary.opacity(0.6))
                        .frame(width: tickWidth, height: tickHeight)
                        .position(
                            x: tickWidth / 2 + CGFloat(index) * spacing,
                            y: height - tickHeight / 2
                        )
                }

                if isModified {
                    let dotY = max(height - longHeight - 6, dotRadius)
                    Circle()
                        .fill(Color.secondary.opacity(0.7))
                        .frame(width: dotRadius * 2, height: dotRadius * 2)
                        .position(
                            x: tickWidth / 2 + CGFloat(originalIndex) * spacing,
                            y: dotY
                        )
                }
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            .contentShape(Rectangle())
            .gesture(dragGesture(stepSpacing: spacing))
        }
        .frame(height: 28)
        .onAppear {
            syncIndices(animated: false)
            lastHapticIndex = currentIndex
        }
        .onChange(of: currentIndex) { _, newIndex in
            syncIndices(animated: true, index: newIndex)
            if isDragging {
                handleHaptic(for: newIndex)
            }
        }
        .onChange(of: range) { _, _ in
            syncIndices(animated: false)
        }
        .onChange(of: step) { _, _ in
            syncIndices(animated: false)
        }
    }

    private var tickCount: Int {
        let span = range.upperBound - range.lowerBound
        let steps = Int((span / step).rounded())
        return max(steps + 1, 2)
    }

    private var currentIndex: Int {
        index(for: value)
    }

    private var originalIndex: Int {
        index(for: originalValue)
    }

    private var isModified: Bool {
        let tolerance = max(step * 0.001, 0.000001)
        return abs(value - originalValue) > tolerance
    }

    private func syncIndices(animated: Bool, index: Int? = nil) {
        let target = index ?? currentIndex
        coloredIndex = target
        if animated {
            withAnimation(.easeOut(duration: 0.18)) {
                animatedIndex = target
            }
        } else {
            animatedIndex = target
        }
    }

    private func handleHaptic(for index: Int) {
        guard index != lastHapticIndex else { return }
        if index == 0 || index == tickCount - 1 || index == originalIndex {
            majorFeedback.impactOccurred()
        } else {
            stepFeedback.impactOccurred()
        }
        lastHapticIndex = index
    }

    private func dragGesture(stepSpacing: CGFloat) -> some Gesture {
        DragGesture(minimumDistance: 0)
            .onChanged { gesture in
                if dragStartIndex == nil {
                    dragStartIndex = currentIndex
                    isDragging = true
                    onEditingChanged(true)
                    stepFeedback.prepare()
                    majorFeedback.prepare()
                    lastHapticIndex = currentIndex
                }
                let safeSpacing = max(stepSpacing, 1)
                let delta = Int((gesture.translation.width / safeSpacing).rounded())
                let targetIndex = min(max((dragStartIndex ?? 0) + delta, 0), tickCount - 1)
                let targetValue = value(for: targetIndex)
                if targetValue != value {
                    value = targetValue
                }
            }
            .onEnded { _ in
                dragStartIndex = nil
                if isDragging {
                    isDragging = false
                    onEditingChanged(false)
                }
            }
    }

    private func index(for rawValue: Double) -> Int {
        let clamped = min(max(rawValue, range.lowerBound), range.upperBound)
        let offset = (clamped - range.lowerBound) / step
        let index = Int(offset.rounded())
        return min(max(index, 0), tickCount - 1)
    }

    private func value(for index: Int) -> Double {
        let clampedIndex = min(max(index, 0), tickCount - 1)
        return range.lowerBound + Double(clampedIndex) * step
    }
}
