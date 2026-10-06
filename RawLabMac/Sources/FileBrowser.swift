import SwiftUI

struct FileBrowser: View {
    @ObservedObject var library: PhotoLibrary
    let selected: URL?
    let open: (URL) -> Void
    var body: some View {
        VStack(spacing: 0) {
            HStack {
                Text("文件夹").font(.headline)
                Spacer()
                Button(action: library.addDirectories) { Image(systemName: "folder.badge.plus") }
                    .buttonStyle(.borderless).help("添加目录").accessibilityLabel("添加目录")
            }.padding(14)
            Divider()
            if library.roots.isEmpty {
                VStack(spacing: 14) {
                    Image(systemName: "folder").font(.system(size: 28)).foregroundStyle(.secondary)
                    Button("添加目录…", action: library.addDirectories)
                }.frame(maxWidth: .infinity, maxHeight: .infinity)
            } else {
                ScrollView {
                    LazyVStack(alignment: .leading, spacing: 10) {
                        ForEach(library.roots) { root in
                            FolderBranch(folder: root, selected: selected, open: open)
                                .contextMenu {
                                    Button("从侧栏移除此目录") { library.remove(root) }
                                }
                        }
                    }.padding(10)
                }
            }
        }.background(Color(nsColor: .windowBackgroundColor))
    }
}

private struct FolderBranch: View {
    @ObservedObject var folder: PhotoFolder
    let selected: URL?
    let open: (URL) -> Void
    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Button(action: folder.toggle) {
                HStack(spacing: 7) {
                    Image(systemName: folder.expanded ? "chevron.down" : "chevron.right")
                        .font(.system(size: 9, weight: .semibold)).frame(width: 10)
                    Image(systemName: folder.expanded ? "folder.fill" : "folder").foregroundStyle(.secondary)
                    Text(folder.url.lastPathComponent).lineLimit(1).truncationMode(.middle)
                    Spacer(minLength: 0)
                    if folder.loading { ProgressView().controlSize(.mini) }
                }.font(.system(size: 12, weight: .medium)).frame(height: 26).contentShape(Rectangle())
            }.buttonStyle(.plain).help(folder.url.path)
                .accessibilityValue(folder.expanded ? "已展开" : "已收起")
            if folder.expanded {
                if let error = folder.error {
                    HStack(alignment: .top) {
                        Text(error).font(.caption).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
                        Button(action: folder.load) { Image(systemName: "arrow.clockwise") }
                            .buttonStyle(.borderless).help("重新读取目录").accessibilityLabel("重新读取目录")
                    }
                }
                ForEach(folder.folders) { child in
                    AnyView(FolderBranch(folder: child, selected: selected, open: open)).padding(.leading, 8)
                }
                LazyVGrid(columns: [GridItem(.flexible()), GridItem(.flexible())], spacing: 12) {
                    ForEach(folder.photos, id: \.self) { url in
                        PhotoThumbnail(url: url, selected: selected == url) { open(url) }
                    }
                }
                if !folder.loading && folder.error == nil && folder.photos.isEmpty && folder.folders.isEmpty {
                    Text("无 RAW 照片").font(.caption).foregroundStyle(.secondary).padding(.leading, 18)
                }
            }
        }
    }
}

private struct PhotoThumbnail: View {
    let url: URL
    let selected: Bool
    let action: () -> Void
    @State private var image: NSImage?
    var body: some View {
        Button(action: action) {
            VStack(spacing: 5) {
                ZStack {
                    Color.black.opacity(0.35)
                    if let image {
                        Image(nsImage: image).resizable().scaledToFit().padding(3)
                    } else {
                        Image(systemName: "photo").foregroundStyle(.secondary)
                    }
                }.frame(height: 82).clipShape(RoundedRectangle(cornerRadius: 4))
                    .overlay { RoundedRectangle(cornerRadius: 4).strokeBorder(selected ? Color.yellow : .clear, lineWidth: 2) }
                Text(url.deletingPathExtension().lastPathComponent).font(.system(size: 11))
                    .foregroundStyle(selected ? Color.yellow : Color.primary).lineLimit(1).truncationMode(.middle)
            }.frame(maxWidth: .infinity).contentShape(Rectangle())
        }.buttonStyle(.plain).help(url.lastPathComponent).accessibilityLabel(url.lastPathComponent)
            .accessibilityAddTraits(selected ? .isSelected : [])
            .task(id: url) {
                let loaded = await PhotoThumbnails.shared.image(for: url)
                if !Task.isCancelled { image = loaded }
            }
    }
}
