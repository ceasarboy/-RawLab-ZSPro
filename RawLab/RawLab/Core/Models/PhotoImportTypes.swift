import CoreTransferable
import Foundation
import UniformTypeIdentifiers

enum PhotoImportSource {
    case raw(URL)
    case raster(URL)

    var url: URL {
        switch self {
        case .raw(let url), .raster(let url):
            return url
        }
    }

    var isRaw: Bool {
        switch self {
        case .raw:
            return true
        case .raster:
            return false
        }
    }
}

enum PhotoImportError: LocalizedError {
    case unsupportedItem
    case loadFailed

    var errorDescription: String? {
        switch self {
        case .unsupportedItem:
            return "Selected item is not a supported image."
        case .loadFailed:
            return "Failed to load the selected photo."
        }
    }
}

struct RawPhotoFile: Transferable {
    let url: URL

    static var transferRepresentation: some TransferRepresentation {
        FileRepresentation(importedContentType: .rawImage) { file in
            RawPhotoFile(url: file.file)
        }
    }
}

struct ImagePhotoFile: Transferable {
    let url: URL

    static var transferRepresentation: some TransferRepresentation {
        FileRepresentation(importedContentType: .image) { file in
            ImagePhotoFile(url: file.file)
        }
    }
}
