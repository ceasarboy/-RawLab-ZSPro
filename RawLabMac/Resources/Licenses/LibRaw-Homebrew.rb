class Libraw < Formula
  desc "Library for reading RAW files from digital photo cameras"
  homepage "https://www.libraw.org/"
  url "https://www.libraw.org/data/LibRaw-0.21.5.tar.gz"
  sha256 "20ecfa6aad943d383ab3f66d303727ef542b042cbba2bb7ddaeff0bb2e9ff916"
  license any_of: ["LGPL-2.1-only", "CDDL-1.0"]

  livecheck do
    url "https://www.libraw.org/download/"
    regex(/href=.*?LibRaw[._-]v?(\d+(?:\.\d+)+)\.t/i)
  end

  depends_on "autoconf" => :build
  depends_on "automake" => :build
  depends_on "libtool" => :build
  depends_on "pkgconf" => :build
  depends_on "jasper"
  depends_on "jpeg-turbo"
  depends_on "little-cms2"

  uses_from_macos "zlib"

  on_macos do
    depends_on "libomp"
  end

  def install
    args = []
    if OS.mac?
      # Work around "checking for OpenMP flag of C compiler... unknown"
      args += [
        "ac_cv_prog_c_openmp=-Xpreprocessor -fopenmp",
        "ac_cv_prog_cxx_openmp=-Xpreprocessor -fopenmp",
        "LDFLAGS=-lomp",
      ]
    end
    system "autoreconf", "--force", "--install", "--verbose"
    system "./configure", *args, *std_configure_args
    system "make"
    system "make", "install"
    doc.install Dir["doc/*"]
    prefix.install "samples"
  end

  test do
    resource "homebrew-librawtestfile" do
      url "https://www.rawsamples.ch/raws/nikon/d1/RAW_NIKON_D1.NEF"
      mirror "https://web.archive.org/web/20200703103724/https://www.rawsamples.ch/raws/nikon/d1/RAW_NIKON_D1.NEF"
      sha256 "7886d8b0e1257897faa7404b98fe1086ee2d95606531b6285aed83a0939b768f"
    end

    resource("homebrew-librawtestfile").stage do
      filename = "RAW_NIKON_D1.NEF"
      system bin/"raw-identify", "-u", filename
      system bin/"simple_dcraw", "-v", "-T", filename
    end
  end
end
