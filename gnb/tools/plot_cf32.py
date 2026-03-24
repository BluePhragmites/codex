#!/usr/bin/env python3

import argparse
import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")

import matplotlib.pyplot as plt
import numpy as np


def load_cf32(path: Path) -> np.ndarray:
    raw = np.fromfile(path, dtype=np.float32)
    if raw.size == 0:
        raise ValueError(f"{path} is empty")
    if raw.size % 2 != 0:
        raise ValueError(f"{path} does not contain interleaved IQ float32 pairs")
    return raw[0::2] + 1j * raw[1::2]


def load_metadata(cf32_path: Path, explicit_meta: Path | None) -> dict:
    meta_path = explicit_meta if explicit_meta is not None else cf32_path.with_suffix(".json")
    if not meta_path.exists():
        return {}
    return json.loads(meta_path.read_text(encoding="utf-8"))


def default_output_path(cf32_path: Path) -> Path:
    return cf32_path.parent.parent / "plots" / f"{cf32_path.stem}.png"


def plot_iq(cf32_path: Path, output_path: Path, metadata: dict, time_samples: int) -> None:
    iq = load_cf32(cf32_path)
    time_samples = max(1, min(time_samples, iq.size))
    time_axis = np.arange(time_samples)

    spectrum = np.fft.fftshift(np.fft.fft(iq))
    spectrum_db = 20.0 * np.log10(np.maximum(np.abs(spectrum), 1e-9))
    freq_axis = np.linspace(-0.5, 0.5, spectrum.size, endpoint=False)

    title_parts = [cf32_path.name]
    if metadata:
        summary = []
        for key in ("type", "abs_slot", "rnti", "sample_rate_hz", "fft_size", "cp_len"):
            if key in metadata:
                summary.append(f"{key}={metadata[key]}")
        if summary:
            title_parts.append(", ".join(summary))

    fig, axes = plt.subplots(3, 1, figsize=(12, 10), constrained_layout=True)
    fig.suptitle("\n".join(title_parts), fontsize=11)

    axes[0].plot(time_axis, iq.real[:time_samples], label="I", linewidth=1.0)
    axes[0].plot(time_axis, iq.imag[:time_samples], label="Q", linewidth=1.0)
    axes[0].set_title("Time Domain IQ")
    axes[0].set_xlabel("Sample Index")
    axes[0].set_ylabel("Amplitude")
    axes[0].grid(True, alpha=0.3)
    axes[0].legend()

    axes[1].plot(time_axis, np.abs(iq[:time_samples]), linewidth=1.0)
    axes[1].set_title("Magnitude")
    axes[1].set_xlabel("Sample Index")
    axes[1].set_ylabel("|IQ|")
    axes[1].grid(True, alpha=0.3)

    axes[2].plot(freq_axis, spectrum_db, linewidth=1.0)
    axes[2].set_title("FFT Magnitude")
    axes[2].set_xlabel("Normalized Frequency")
    axes[2].set_ylabel("Magnitude (dB)")
    axes[2].grid(True, alpha=0.3)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output_path, dpi=150)
    plt.close(fig)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Plot time-domain and spectrum views for a .cf32 IQ file.")
    parser.add_argument("input_cf32", type=Path, help="Path to interleaved float32 IQ samples (.cf32).")
    parser.add_argument("--meta", type=Path, default=None, help="Optional metadata JSON path. Defaults to sibling .json.")
    parser.add_argument(
        "--output",
        type=Path,
        default=None,
        help="Output PNG path. Defaults to out/plots/<cf32-stem>.png.",
    )
    parser.add_argument(
        "--time-samples",
        type=int,
        default=512,
        help="Number of initial samples to show in time-domain plots.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    cf32_path = args.input_cf32.resolve()
    if not cf32_path.exists():
        raise FileNotFoundError(f"Input IQ file not found: {cf32_path}")

    metadata = load_metadata(cf32_path, args.meta)
    output_path = args.output.resolve() if args.output is not None else default_output_path(cf32_path)
    plot_iq(cf32_path, output_path, metadata, args.time_samples)
    print(output_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
