import Photos
import PhotosUI
import SwiftUI
import UIKit
import UniformTypeIdentifiers

struct ContentView: View {
    @StateObject var viewModel = RawEditorViewModel()
    @State var settings = RawSettings.default
    @State private var didAutoLoadSample = false
    @State var showingBefore = false
    @State var selectedMode: EditorMode = .adjust
    @State var selectedAdjustment: AdjustmentKind = .exposure
    @State var isAdjustingSlider = false
    @State var selectedPhotoItem: PhotosPickerItem?

    var body: some View {
        ZStack {
            Color(.systemBackground).ignoresSafeArea()

            VStack(spacing: 14) {
                topBar
                previewSection
                    .frame(maxWidth: .infinity)
                    .padding(.horizontal, -16)
                    .ignoresSafeArea(edges: .horizontal)

                if let statusMessage = viewModel.statusMessage {
                    Text(statusMessage)
                        .font(.footnote)
                        .foregroundStyle(.red)
                        .frame(maxWidth: .infinity, alignment: .leading)
                }

                modeBar
                toolPanel
            }
            .padding(.horizontal, 16)
            .padding(.top, 8)
            .padding(.bottom, 12)
        }
        .onChange(of: settings) { _, newValue in
            showingBefore = false
            if isAdjustingSlider {
                viewModel.schedulePreviewUpdate(
                    settings: newValue,
                    delay: 0.08,
                    includeHistogram: false,
                    quality: .interactive
                )
            } else {
                viewModel.updatePreview(settings: newValue, includeHistogram: true, quality: .final)
            }
        }
        .onChange(of: selectedPhotoItem) { _, newItem in
            guard let newItem else { return }
            Task {
                await importFromPhotoItem(newItem)
            }
        }
        .onChange(of: selectedMode) { _, newMode in
            if newMode == .lut {
                viewModel.loadLUTsIfNeeded()
            }
        }
        .onAppear {
            guard !didAutoLoadSample else { return }
            if ProcessInfo.processInfo.environment["RAWLAB_SMOKE_TEST"] == "1" {
                didAutoLoadSample = true
                viewModel.runBundledSampleSmokeTest(settings: settings)
            }
        }
    }
}

#Preview {
    ContentView()
}
