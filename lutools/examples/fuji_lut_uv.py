from __future__ import annotations

import argparse
from pathlib import Path

import colour
import imageio.v3 as imageio
import numpy as np
import rawpy

ROOT_DIR = Path(__file__).resolve().parents[1]

# =============================
# DEFAULT PATHS
# =============================
DEFAULT_LUT = ROOT_DIR / "F-Log2" / "X100VI_FLog2_FGamut_to_ETERNA_BT.709_33grid_V.1.00.cube"
DEFAULT_OUTPUT = ROOT_DIR / "output_eterna_uv.jpg"

F_GAMUT_PRIMARIES = np.array(
    [
        [0.70800, 0.29200],
        [0.17000, 0.79700],
        [0.13100, 0.04600],
    ],
    dtype=np.float64,
)
F_GAMUT_WHITEPOINT = colour.CCS_ILLUMINANTS["CIE 1931 2 Degree Standard Observer"]["D65"]

RGB_COLOURSPACE_ADOBE = colour.models.RGB_COLOURSPACE_ADOBE_RGB1998
RGB_COLOURSPACE_F_GAMUT = colour.RGB_Colourspace(
    "F-Gamut",
    F_GAMUT_PRIMARIES,
    F_GAMUT_WHITEPOINT,
    use_derived_matrix_RGB_to_XYZ=True,
    use_derived_matrix_XYZ_to_RGB=True,
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Apply a Fujifilm F-Log2 LUT to a Sony ARW using rawpy + colour."
    )
    parser.add_argument("--input", type=Path, required=True, help="External RAW fixture path")
    parser.add_argument("--lut", type=Path, default=DEFAULT_LUT)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--exposure-shift", type=float, default=0.0)
    return parser.parse_args()


def ensure_exists(path: Path, label: str) -> None:
    if not path.exists():
        raise SystemExit(f"Missing {label}: {path}")


def load_lut(path: Path):
    if path.suffix.lower() == ".spi3d":
        return colour.io.read_LUT_SonySPI3D(str(path))
    return colour.io.read_LUT_IridasCube(str(path))


def read_raw_linear(arw_path: Path) -> np.ndarray:
    with rawpy.imread(str(arw_path)) as raw:
        rgb_linear = raw.postprocess(
            gamma=(1, 1),
            no_auto_bright=True,
            use_camera_wb=True,
            bright=1.0,
            user_sat=None,
            output_color=rawpy.ColorSpace.Adobe,
            output_bps=16,
        )
    return rgb_linear.astype(np.float32) / 65535.0


def to_fgamut_linear(img: np.ndarray) -> np.ndarray:
    fgamut = colour.RGB_to_RGB(
        img,
        RGB_COLOURSPACE_ADOBE,
        RGB_COLOURSPACE_F_GAMUT,
        chromatic_adaptation_transform="Bradford",
    )
    return np.clip(fgamut, 0.0, 1.0)


def encode_flog2(img: np.ndarray) -> np.ndarray:
    return colour.models.log_encoding_FLog2(img)


def apply_lut(lut, img: np.ndarray) -> np.ndarray:
    return lut.apply(np.clip(img, 0.0, 1.0))


def process_arw(arw_path: Path, lut_path: Path, output_path: Path, exposure_shift: float) -> None:
    img = read_raw_linear(arw_path)
    if exposure_shift != 0.0:
        img = img * (2.0 ** exposure_shift)
    img = to_fgamut_linear(img)
    img = encode_flog2(img)
    lut = load_lut(lut_path)
    img = apply_lut(lut, img)
    output = np.clip(img, 0.0, 1.0)
    imageio.imwrite(str(output_path), (output * 255.0).astype(np.uint8))


def main() -> None:
    args = parse_args()
    ensure_exists(args.input, "input ARW")
    ensure_exists(args.lut, "LUT")
    process_arw(args.input, args.lut, args.output, args.exposure_shift)
    print(f"Done: {args.output}")


if __name__ == "__main__":
    main()
