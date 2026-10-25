import AppKit

struct Catalog: Decodable {
    struct Entry: Decodable {
        let filename: String
        let size: String
        let scale: String
    }
    let images: [Entry]
}

let directory = URL(fileURLWithPath: CommandLine.arguments[1])
let catalog = try JSONDecoder().decode(Catalog.self, from: Data(contentsOf: directory.appendingPathComponent("Contents.json")))
for entry in catalog.images {
    let data = try Data(contentsOf: directory.appendingPathComponent(entry.filename))
    guard let bitmap = NSBitmapImageRep(data: data) else { fatalError("Unreadable icon: \(entry.filename)") }
    let points = Double(entry.size.split(separator: "x")[0])!
    let scale = Double(entry.scale.dropLast())!
    precondition(bitmap.pixelsWide == Int(points * scale) && bitmap.pixelsHigh == Int(points * scale),
                 "Incorrect icon dimensions: \(entry.filename)")
    precondition(!bitmap.hasAlpha, "App Store icon must not have an alpha channel")
    // Skeuomorphic lens artwork: the amber and cyan coating flares carry the
    // brand colors. Thresholds scale with the sample count so the check holds
    // for every catalog size down to 20pt.
    var yellow = 0
    var cyan = 0
    var samples = 0
    for y in stride(from: 0, to: bitmap.pixelsHigh, by: max(1, bitmap.pixelsHigh / 64)) {
        for x in stride(from: 0, to: bitmap.pixelsWide, by: max(1, bitmap.pixelsWide / 64)) {
            samples += 1
            guard let color = bitmap.colorAt(x: x, y: y)?.usingColorSpace(.deviceRGB) else { continue }
            if color.redComponent > 0.7 && color.greenComponent > 0.5 && color.blueComponent < 0.4 { yellow += 1 }
            if color.redComponent < 0.5 && color.greenComponent > 0.6 && color.blueComponent > 0.6 { cyan += 1 }
        }
    }
    precondition(yellow >= max(2, samples / 350) && cyan >= max(2, samples / 400),
                 "Missing coating artwork (yellow + cyan reflections): \(entry.filename)")
    print("PASS: \(entry.filename), dimensions, opacity and coating artwork")
}
