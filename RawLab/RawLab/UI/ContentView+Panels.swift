import SwiftUI
import UIKit

extension ContentView {
    var topBar: some View {
        HStack {
            HStack(spacing: 14) {
                photoImportButton(isEnabled: !viewModel.isBusy)
                circleIconButton(systemName: "arrow.counterclockwise", isEnabled: !viewModel.isBusy) {
                    applySettingsChange(.default)
                }
            }

            Spacer()

            Text("Adjust")
                .font(.headline)
                .foregroundStyle(.primary)

            Spacer()

            HStack(spacing: 14) {
                circleIconButton(systemName: "circle.lefthalf.filled", isEnabled: viewModel.hasImage) {
                    showingBefore.toggle()
                }
                circleIconButton(systemName: "square.and.arrow.up", isEnabled: viewModel.hasImage && !viewModel.isBusy) {
                    exportJPEG()
                }
#if DEBUG
                Menu {
                    Button("Decode Sample") {
                        viewModel.runBundledSampleSmokeTest(settings: settings)
                    }
                } label: {
                    circleIconLabel(systemName: "ellipsis.circle")
                }
                .opacity(viewModel.isBusy ? 0.35 : 1)
                .disabled(viewModel.isBusy)
#endif
            }
        }
    }

    var previewSection: some View {
        ZStack(alignment: .bottom) {
            if let image = currentPreviewImage {
                GeometryReader { geometry in
                    let isLandscape = previewIsLandscape
                    Image(uiImage: image)
                        .resizable()
                        .aspectRatio(contentMode: .fit)
                        .frame(
                            width: isLandscape ? geometry.size.width : nil,
                            height: isLandscape ? nil : geometry.size.height
                        )
                        .frame(maxWidth: .infinity, maxHeight: .infinity)
                        .background(Color(.secondarySystemBackground))
                }
                .frame(height: previewMaxHeight)
            } else {
                VStack(spacing: 12) {
                    Image(systemName: "photo")
                        .font(.system(size: 40))
                        .foregroundStyle(.secondary)
                    Text("Import a photo to preview.")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }
                .frame(maxWidth: .infinity, minHeight: 220)
                .background(Color(.secondarySystemBackground))
            }

            Button(showingBefore ? "Before" : "After") {
                showingBefore.toggle()
            }
            .font(.footnote)
            .foregroundStyle(.primary)
            .padding(.horizontal, 12)
            .padding(.vertical, 6)
            .background(Color(.systemBackground).opacity(0.85))
            .clipShape(Capsule())
            .padding(.bottom, 8)
            .disabled(!viewModel.hasImage)
        }
        .overlay {
            if viewModel.isBusy {
                ProgressView()
                    .progressViewStyle(.circular)
                    .tint(.primary)
            }
        }
    }

    var modeBar: some View {
        HStack(spacing: 20) {
            ForEach(EditorMode.allCases) { mode in
                Button {
                    selectedMode = mode
                } label: {
                    Image(systemName: mode.iconName)
                        .font(.system(size: 18, weight: .semibold))
                        .foregroundStyle(selectedMode == mode ? .primary : .secondary)
                        .frame(width: 44, height: 44)
                        .background(selectedMode == mode ? Color.primary.opacity(0.18) : Color.primary.opacity(0.08))
                        .clipShape(Circle())
                }
            }
        }
    }

    @ViewBuilder
    var toolPanel: some View {
        switch selectedMode {
        case .adjust:
            adjustmentsPanel
        case .lut:
            lutPanel
        case .crop:
            cropPanel
        }
    }

    var adjustmentsPanel: some View {
        VStack(spacing: 12) {
            if !currentHistogram.isEmpty {
                HistogramView(values: currentHistogram)
            }

            HStack {
                Text(selectedAdjustment.title)
                    .font(.subheadline)
                    .foregroundStyle(.primary)
                Spacer()
                Text(selectedAdjustment.valueLabel(for: selectedAdjustment.value(from: settings)))
                    .font(.subheadline)
                    .foregroundStyle(.secondary)
                    .monospacedDigit()
            }

            TickMarksView(
                range: selectedAdjustment.range,
                step: selectedAdjustment.step,
                value: selectedAdjustmentBinding,
                originalValue: selectedAdjustment.value(from: .default),
                onEditingChanged: { editing in
                    isAdjustingSlider = editing
                    if !editing {
                        viewModel.updatePreview(settings: settings, includeHistogram: true, quality: .final)
                    }
                }
            )

            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: 18) {
                    ForEach(AdjustmentKind.allCases) { adjustment in
                        Button {
                            selectedAdjustment = adjustment
                        } label: {
                            VStack(spacing: 6) {
                                Image(systemName: adjustment.iconName)
                                    .font(.system(size: 16, weight: .semibold))
                                Text(adjustment.title)
                                    .font(.caption2)
                            }
                            .foregroundStyle(selectedAdjustment == adjustment ? .primary : .secondary)
                            .padding(.vertical, 6)
                            .padding(.horizontal, 6)
                        }
                    }
                }
                .padding(.horizontal, 4)
            }
        }
    }

    var lutPanel: some View {
        let isStrengthEnabled = settings.lutID != nil && !viewModel.isLoadingLUTs
        return VStack(spacing: 12) {
            Text("Fuji LUTs (F-Log2)")
                .font(.subheadline)
                .foregroundStyle(.secondary)
                .frame(maxWidth: .infinity, alignment: .leading)

            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: 10) {
                    lutButton(title: "None", isSelected: settings.lutID == nil) {
                        var updated = settings
                        updated.lutID = nil
                        applySettingsChange(updated)
                    }

                    ForEach(viewModel.availableLUTs) { lut in
                        lutButton(title: lut.name, isSelected: settings.lutID == lut.id) {
                            var updated = settings
                            updated.lutID = lut.id
                            applySettingsChange(updated)
                        }
                    }
                }
                .padding(.horizontal, 4)
            }
            .opacity(viewModel.isLoadingLUTs ? 0.4 : 1)
            .disabled(viewModel.isLoadingLUTs)

            VStack(spacing: 8) {
                HStack {
                    Text("LUT Strength")
                        .font(.subheadline)
                        .foregroundStyle(.primary)
                    Spacer()
                    Text(String(format: "%.0f%%", settings.lutStrength * 100))
                        .font(.subheadline)
                        .foregroundStyle(.secondary)
                        .monospacedDigit()
                }

                Slider(
                    value: lutStrengthBinding,
                    in: 0...1,
                    step: 0.01,
                    onEditingChanged: { editing in
                        isAdjustingSlider = editing
                        if !editing {
                            viewModel.updatePreview(settings: settings, includeHistogram: true, quality: .final)
                        }
                    }
                )
                .tint(.primary)
            }
            .opacity(isStrengthEnabled ? 1 : 0.4)
            .disabled(!isStrengthEnabled)

            if viewModel.isLoadingLUTs {
                Text("Loading LUTs...")
                    .font(.footnote)
                    .foregroundStyle(.secondary)
            } else if viewModel.availableLUTs.isEmpty {
                Text("No F-Log2 LUTs found in bundle.")
                    .font(.footnote)
                    .foregroundStyle(.secondary)
            }
        }
    }

    var cropPanel: some View {
        VStack(spacing: 12) {
            Image(systemName: "crop")
                .font(.system(size: 22, weight: .semibold))
                .foregroundStyle(.secondary)
            Text("Crop tools coming soon.")
                .font(.footnote)
                .foregroundStyle(.secondary)
        }
        .frame(maxWidth: .infinity)
        .padding(.vertical, 12)
    }

    var selectedAdjustmentBinding: Binding<Double> {
        Binding(
            get: {
                selectedAdjustment.value(from: settings)
            },
            set: { newValue in
                var updated = settings
                selectedAdjustment.setValue(newValue, in: &updated)
                applySettingsChange(updated)
            }
        )
    }

    var lutStrengthBinding: Binding<Double> {
        Binding(
            get: {
                settings.lutStrength
            },
            set: { newValue in
                var updated = settings
                updated.lutStrength = newValue
                applySettingsChange(updated)
            }
        )
    }
}

struct HistogramView: View {
    let values: [CGFloat]

    var body: some View {
        GeometryReader { geometry in
            Canvas { context, size in
                guard !values.isEmpty else { return }

                let maxValue = values.max() ?? 1
                let barWidth = max(size.width / CGFloat(values.count), 1)
                var path = Path()

                for (index, value) in values.enumerated() {
                    let height = maxValue > 0 ? (value / maxValue) * size.height : 0
                    let x = CGFloat(index) * barWidth
                    let y = size.height - height
                    path.addRect(CGRect(x: x, y: y, width: barWidth, height: height))
                }

                context.fill(path, with: .color(.primary.opacity(0.7)))
            }
        }
        .frame(height: 60)
        .background(Color.primary.opacity(0.08))
        .clipShape(RoundedRectangle(cornerRadius: 6))
    }
}
