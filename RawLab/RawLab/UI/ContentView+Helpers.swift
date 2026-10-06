import ImageIO
import Photos
import PhotosUI
import SwiftUI
import UIKit
import UniformTypeIdentifiers

extension ContentView {
    func circleIconButton(
        systemName: String,
        isEnabled: Bool,
        action: @escaping () -> Void
    ) -> some View {
        Button(action: action) {
            circleIconLabel(systemName: systemName)
        }
        .opacity(isEnabled ? 1 : 0.35)
        .disabled(!isEnabled)
    }

    func circleIconLabel(systemName: String) -> some View {
        Image(systemName: systemName)
            .font(.system(size: 15, weight: .semibold))
            .foregroundStyle(.primary)
            .frame(width: 32, height: 32)
            .background(Color.primary.opacity(0.12))
            .clipShape(Circle())
    }

    func lutButton(title: String, isSelected: Bool, action: @escaping () -> Void) -> some View {
        Button(action: action) {
            Text(title)
                .font(.footnote)
                .foregroundStyle(isSelected ? Color(.systemBackground) : .primary)
                .padding(.horizontal, 12)
                .padding(.vertical, 6)
                .background(isSelected ? Color.primary : Color.primary.opacity(0.12))
                .clipShape(Capsule())
        }
    }

    var currentPreviewImage: UIImage? {
        if showingBefore {
            return viewModel.basePreviewImage ?? viewModel.previewImage
        }
        return viewModel.previewImage ?? viewModel.basePreviewImage
    }

    var previewMaxHeight: CGFloat { 460 }

    var previewIsLandscape: Bool {
        guard let image = currentPreviewImage else {
            return true
        }
        let width = image.cgImage?.width ?? Int(image.size.width)
        let height = image.cgImage?.height ?? Int(image.size.height)
        let isRotated = viewModel.sourceOrientation?.isQuarterTurn ?? false
        let displayWidth = isRotated ? height : width
        let displayHeight = isRotated ? width : height
        return displayWidth >= displayHeight
    }

    var currentHistogram: [CGFloat] {
        if showingBefore {
            return viewModel.baseHistogram.isEmpty ? viewModel.histogram : viewModel.baseHistogram
        }
        return viewModel.histogram.isEmpty ? viewModel.baseHistogram : viewModel.histogram
    }

    func exportJPEG() {
        viewModel.exportJPEG(settings: settings) { result in
            switch result {
            case .success(let data):
                saveToPhotoLibrary(data)
            case .failure(let error):
                viewModel.statusMessage = error.localizedDescription
            }
        }
    }

    func applySettingsChange(_ newSettings: RawSettings) {
        settings = newSettings
    }

    func photoImportButton(isEnabled: Bool) -> some View {
        PhotosPicker(selection: $selectedPhotoItem, matching: .images) {
            circleIconLabel(systemName: "square.and.arrow.down")
        }
        .opacity(isEnabled ? 1 : 0.35)
        .disabled(!isEnabled)
    }

    func importFromPhotoItem(_ item: PhotosPickerItem) async {
        do {
            let source = try await loadPhotoSource(from: item)
            await MainActor.run {
                viewModel.importImage(from: source, settings: settings)
                selectedPhotoItem = nil
            }
        } catch {
            await MainActor.run {
                viewModel.statusMessage = error.localizedDescription
                selectedPhotoItem = nil
            }
        }
    }

    func loadPhotoSource(from item: PhotosPickerItem) async throws -> PhotoImportSource {
        let supportsRaw = item.supportedContentTypes.contains { $0.conforms(to: .rawImage) }
        if supportsRaw, let file = try await item.loadTransferable(type: RawPhotoFile.self) {
            return .raw(file.url)
        }

        if let file = try await item.loadTransferable(type: ImagePhotoFile.self) {
            return .raster(file.url)
        }

        throw PhotoImportError.unsupportedItem
    }

    func saveToPhotoLibrary(_ data: Data) {
        PHPhotoLibrary.requestAuthorization(for: .addOnly) { status in
            guard status == .authorized || status == .limited else {
                DispatchQueue.main.async {
                    viewModel.statusMessage = "Photo library access denied."
                }
                return
            }

            PHPhotoLibrary.shared().performChanges({
                let request = PHAssetCreationRequest.forAsset()
                request.addResource(with: .photo, data: data, options: nil)
            }, completionHandler: { success, error in
                DispatchQueue.main.async {
                    if let error = error {
                        viewModel.statusMessage = error.localizedDescription
                    } else if success {
                        viewModel.statusMessage = "Saved to Photos."
                    } else {
                        viewModel.statusMessage = "Failed to save to Photos."
                    }
                }
            })
        }
    }
}

private extension CGImagePropertyOrientation {
    var isQuarterTurn: Bool {
        switch self {
        case .left, .leftMirrored, .right, .rightMirrored:
            return true
        default:
            return false
        }
    }
}
