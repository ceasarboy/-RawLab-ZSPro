import Foundation
import CoreImage
import CoreGraphics
import ImageIO

guard let rawPath = CommandLine.arguments.dropFirst().first ?? ProcessInfo.processInfo.environment["RAWLAB_TEST_RAW"] else {
    print("Pass an external RAW path or set RAWLAB_TEST_RAW")
    exit(1)
}
let url = URL(fileURLWithPath: rawPath)

guard FileManager.default.fileExists(atPath: url.path) else {
    print("RAW file not found at \(rawPath)")
    exit(1)
}

if let source = CGImageSourceCreateWithURL(url as CFURL, nil),
   let props = CGImageSourceCopyPropertiesAtIndex(source, 0, nil) as? [CFString: Any] {
    let width = props[kCGImagePropertyPixelWidth] ?? "?"
    let height = props[kCGImagePropertyPixelHeight] ?? "?"
    print("ImageIO pixel size: \(width)x\(height)")
    let thumbnailOptions: [CFString: Any] = [
        kCGImageSourceCreateThumbnailFromImageAlways: true,
        kCGImageSourceCreateThumbnailWithTransform: true,
        kCGImageSourceThumbnailMaxPixelSize: 1024,
    ]
    if let cgThumb = CGImageSourceCreateThumbnailAtIndex(source, 0, thumbnailOptions as CFDictionary) {
        print("ImageIO thumbnail OK: \(cgThumb.width)x\(cgThumb.height)")
    } else {
        print("ImageIO thumbnail failed")
    }

    if let cgImage = CGImageSourceCreateImageAtIndex(source, 0, nil) {
        print("ImageIO full decode OK: \(cgImage.width)x\(cgImage.height)")
    } else {
        print("ImageIO full decode failed")
    }
} else {
    print("ImageIO: failed to read properties")
}

guard let rawFilter = CIRAWFilter(imageURL: url) else {
    print("Failed to create CIRAWFilter")
    exit(2)
}

rawFilter.isDraftModeEnabled = true

let props = rawFilter.properties
let width = props[kCGImagePropertyPixelWidth as String] ?? "?"
let height = props[kCGImagePropertyPixelHeight as String] ?? "?"
print("CIRAWFilter properties pixel size: \(width)x\(height)")
print("nativeSize: \(rawFilter.nativeSize)")

guard let outputImage = rawFilter.outputImage else {
    print("Failed to get output image from CIRAWFilter")
    exit(3)
}

let extent = outputImage.extent
print("output extent: \(extent)")

let colorSpace = CGColorSpace(name: CGColorSpace.sRGB)!
let context = CIContext(options: [
    .workingColorSpace: colorSpace,
    .outputColorSpace: colorSpace,
    .useSoftwareRenderer: true,
])

var cropRect = extent
if extent.isInfinite || extent.width == 0 || extent.height == 0 {
    if let w = (props[kCGImagePropertyPixelWidth as String] as? NSNumber)?.doubleValue,
       let h = (props[kCGImagePropertyPixelHeight as String] as? NSNumber)?.doubleValue,
       w > 0, h > 0 {
        cropRect = CGRect(x: 0, y: 0, width: w, height: h)
        print("Using fallback cropRect: \(cropRect)")
    } else if rawFilter.nativeSize.width > 0, rawFilter.nativeSize.height > 0 {
        cropRect = CGRect(origin: .zero, size: rawFilter.nativeSize)
        print("Using nativeSize cropRect: \(cropRect)")
    } else {
        print("No valid extent or fallback size")
        exit(4)
    }
}

let croppedImage = outputImage.cropped(to: cropRect)

if let cgImage = context.createCGImage(croppedImage, from: cropRect) {
    print("RAW decode OK (default): \(cgImage.width)x\(cgImage.height)")
    exit(0)
}

if let cgImage = context.createCGImage(croppedImage, from: cropRect, format: .RGBAh, colorSpace: nil) {
    print("RAW decode OK (RGBAh): \(cgImage.width)x\(cgImage.height)")
    exit(0)
}

if let cgImage = context.createCGImage(croppedImage, from: cropRect, format: .RGBAf, colorSpace: nil) {
    print("RAW decode OK (RGBAf): \(cgImage.width)x\(cgImage.height)")
    exit(0)
}

if let cgImage = context.createCGImage(croppedImage, from: cropRect, format: .RGBA8, colorSpace: colorSpace) {
    print("RAW decode OK (RGBA8): \(cgImage.width)x\(cgImage.height)")
    exit(0)
}

if let data = context.jpegRepresentation(
    of: croppedImage,
    colorSpace: colorSpace,
    options: [kCGImageDestinationLossyCompressionQuality as CIImageRepresentationOption: 0.9]
) {
    print("RAW decode OK (jpegRepresentation): \(data.count) bytes")
    exit(0)
}

print("Failed to render CGImage and JPEG representation")
exit(5)
