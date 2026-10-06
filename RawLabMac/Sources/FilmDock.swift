import SwiftUI

struct FilmDock: View {
    @ObservedObject var model: EditorModel
    var body: some View {
        GeometryReader { geometry in
            let imageSize = max(32, min(96, geometry.size.height - 44))
            let itemHeight = max(66, geometry.size.height - 6)
            ScrollViewReader { proxy in
                ScrollView(.horizontal) {
                    HStack(spacing: 8) {
                        filmButton(name: "中性", id: "", imageSize: imageSize, height: itemHeight)
                        ForEach(model.films) { film in
                            filmButton(name: film.name, id: film.id, imageSize: imageSize, height: itemHeight)
                        }
                        Button(action: model.importLUT) {
                            VStack(spacing: 5) {
                                Image(systemName: "plus.circle").font(.system(size: 26)).frame(height: imageSize)
                                Text("导入 LUT").font(.system(size: 11))
                            }.frame(width: max(88, imageSize + 12), height: itemHeight)
                        }.buttonStyle(.plain).help("导入 LUT…")
                    }.padding(.horizontal, 16).frame(minWidth: geometry.size.width)
                }.scrollIndicators(.visible)
                    .onAppear { proxy.scrollTo(model.selectedFilmID, anchor: .center) }
                    .onChange(of: model.selectedFilmID) { _, id in proxy.scrollTo(id, anchor: .center) }
            }
        }.frame(maxWidth: .infinity, maxHeight: .infinity).tint(.yellow)
    }

    private func filmButton(name: String, id: String, imageSize: CGFloat, height: CGFloat) -> some View {
        let selected = model.selectedFilmID == id
        return Button { model.selectedFilmID = id } label: {
            VStack(spacing: 5) {
                FilmPackageImage(name: name, neutral: id.isEmpty).frame(width: imageSize, height: imageSize)
                    .clipShape(RoundedRectangle(cornerRadius: 3))
                Text(name).font(.system(size: 11, weight: selected ? .semibold : .regular))
                    .lineLimit(2).multilineTextAlignment(.center).fixedSize(horizontal: false, vertical: true)
                    .frame(width: 88, height: 28)
            }.frame(width: max(88, imageSize + 12), height: height)
                .foregroundStyle(selected ? Color.yellow : Color.primary)
                .background(selected ? Color.yellow.opacity(0.08) : .clear, in: RoundedRectangle(cornerRadius: 6))
                .overlay { RoundedRectangle(cornerRadius: 6).strokeBorder(selected ? Color.yellow : .clear, lineWidth: 1) }
        }.buttonStyle(.plain).id(id).help(name).accessibilityLabel(name)
            .accessibilityAddTraits(selected ? .isSelected : [])
    }
}

private struct FilmPackageImage: View {
    let name: String
    let neutral: Bool
    var body: some View {
        if neutral {
            Image(systemName: "circle.lefthalf.filled").font(.system(size: 28)).foregroundStyle(.secondary)
        } else if let image = FilmArtwork.image(for: name) {
            Image(nsImage: image).resizable().interpolation(.high).scaledToFit().accessibilityHidden(true)
        } else {
            Image(systemName: "shippingbox").font(.system(size: 28)).foregroundStyle(.secondary)
        }
    }
}
