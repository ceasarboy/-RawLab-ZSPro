import CoreGraphics
import Foundation
import ImageIO
import UIKit

final class RawEditorViewModel: ObservableObject {
    enum PreviewQuality: Equatable {
        case interactive
        case final
    }

    @Published var previewImage: UIImage?
    @Published var basePreviewImage: UIImage?
    @Published var hasImage = false
    @Published var statusMessage: String?
    @Published var isBusy = false
    @Published var histogram: [CGFloat] = []
    @Published var baseHistogram: [CGFloat] = []
    @Published var availableLUTs: [LUTOption] = []
    @Published var isLoadingLUTs = false

    struct PreviewRequest: Equatable {
        let settings: RawSettings
        let quality: PreviewQuality
        let includeHistogram: Bool
    }

    let processor = Sony2FujiProcessor()
    let renderQueue = DispatchQueue(label: "com.rawlab.preview", qos: .userInitiated)
    let renderStateLock = NSLock()
    var sourceURL: URL?
    var sourceKind: ImageSourceKind?
    var sourceOrientation: CGImagePropertyOrientation?
    var draftBuffer: Sony2FujiProcessor.Buffer?
    var draftPreviewBuffer: Sony2FujiProcessor.Buffer?
    var latestRenderID = UUID()
    var previewWorkItem: DispatchWorkItem?
    var histogramWorkItem: DispatchWorkItem?
    var lastPreviewRequest: PreviewRequest?
    let sampleResourceName = "sample"
    let sampleResourceExtension = "ARW"
    let previewMaxDimension: CGFloat = 1280
    let interactivePreviewMaxDimension: CGFloat = 720
    let histogramDelay: TimeInterval = 0.25
    let histogramBins = 128
    var lutMap: [String: LUTReference] = [:]
    var rawLUTID: String?
    var rawLUTApplied = false

}
