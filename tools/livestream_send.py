"""
Live screen-mirror sender for the APM32E103ZE EVAL board.

Captures a region of the PC screen (a full monitor by default, or a chosen
sub-rectangle - e.g. an Edge/browser window playing YouTube), downscales it
to the LCD's native landscape resolution (see livestream.h), converts to
RGB565, and streams it over the same USART1/CH340 serial link already used
for status logging (see serial.c - baud bumped to 2,000,000 for this).

Only sends what changed: each capture is diffed against the previous one
and just the bounding box of the changed pixels goes out over the wire
(full quality, no downscale-for-speed tradeoff). A mostly-static desktop
(text, a paused window) updates far faster than a full 280x240 frame would;
full-motion content just falls back to sending the whole frame every time,
same cost as before. Every 30th update is a forced full frame regardless of
the diff, so a dropped/corrupted update over the (unacknowledged) serial
link can't leave a permanently stale patch of screen.

Wire format matches livestream.c's parser exactly:
    4-byte sync marker 0xAA 0x55 0xAA 0x55
    2 bytes x1, 2 bytes y1, 2 bytes w, 2 bytes h (all uint16 little-endian,
    coordinates in the native LIVESTREAM_FRAME_W/H space)
    w*h*2 bytes of RGB565 pixel data, little-endian per pixel (matches
    extract_native.py's convention - the MCU is little-endian and reads
    this straight as uint16_t*, no byte-swap).

Usage:
    python tools/livestream_send.py --port COM25
    python tools/livestream_send.py --port COM25 --monitor 1
    python tools/livestream_send.py --port COM25 --region 100 100 900 700
"""

import argparse
import struct
import sys
import time

import mss
import numpy as np
import serial
from PIL import Image

FRAME_W = 280
FRAME_H = 240
BAUD = 2000000
SYNC = bytes([0xAA, 0x55, 0xAA, 0x55])
FORCE_FULL_EVERY = 30  # periodic self-healing keyframe


def capture_rgb565(sct, box):
    """Grab + LANCZOS-resize to FRAME_W x FRAME_H + pack to RGB565.
    Returns an HxW uint16 array (values only - byte order handled at send
    time), not yet flattened/sliced."""
    shot = sct.grab(box)  # BGRA, HxWx4
    bgra = np.frombuffer(shot.raw, dtype=np.uint8).reshape(shot.height, shot.width, 4)
    rgb = bgra[:, :, [2, 1, 0]]  # BGRA -> RGB, drop alpha

    img = Image.fromarray(rgb, mode="RGB").resize((FRAME_W, FRAME_H), Image.LANCZOS)
    small = np.asarray(img)

    r = (small[:, :, 0] & 0xF8).astype(np.uint16) << 8
    g = (small[:, :, 1] & 0xFC).astype(np.uint16) << 3
    b = (small[:, :, 2] & 0xF8).astype(np.uint16) >> 3
    return r | g | b


def dirty_bbox(prev, curr):
    """Bounding box (x1, y1, w, h) covering every pixel that changed, or
    None if nothing changed at all."""
    diff = curr != prev
    if not diff.any():
        return None

    rows = np.nonzero(np.any(diff, axis=1))[0]
    cols = np.nonzero(np.any(diff, axis=0))[0]
    y1, y2 = int(rows[0]), int(rows[-1])
    x1, x2 = int(cols[0]), int(cols[-1])
    return x1, y1, (x2 - x1 + 1), (y2 - y1 + 1)


def send_rect(ser, curr, x1, y1, w, h):
    header = struct.pack("<HHHH", x1, y1, w, h)
    sub = curr[y1:y1 + h, x1:x1 + w]
    payload = np.ascontiguousarray(sub, dtype="<u2").tobytes()
    ser.write(SYNC)
    ser.write(header)
    ser.write(payload)
    return len(header) + len(payload)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--port", required=True, help="Serial port, e.g. COM25")
    ap.add_argument("--monitor", type=int, default=1,
                     help="mss monitor index to capture (1 = primary). Ignored if --region given.")
    ap.add_argument("--region", type=int, nargs=4, metavar=("LEFT", "TOP", "RIGHT", "BOTTOM"),
                     help="Capture this exact screen rectangle instead of a full monitor")
    ap.add_argument("--fps", type=float, default=3.0, help="Target capture rate (upper bound)")
    args = ap.parse_args()

    ser = serial.Serial(args.port, BAUD, timeout=1)
    sct = mss.mss()

    if args.region:
        left, top, right, bottom = args.region
        box = {"left": left, "top": top, "width": right - left, "height": bottom - top}
    else:
        box = sct.monitors[args.monitor]

    print(f"Capturing {box} -> {FRAME_W}x{FRAME_H} RGB565 (dirty-rect) -> {args.port} @ {BAUD}")
    print("Ctrl+C to stop.")

    period = 1.0 / args.fps
    update_count = 0
    bytes_sent = 0
    t_start = time.time()
    prev = None

    try:
        while True:
            t0 = time.time()

            curr = capture_rgb565(sct, box)

            if prev is None or (update_count % FORCE_FULL_EVERY) == 0:
                x1, y1, w, h = 0, 0, FRAME_W, FRAME_H
            else:
                bbox = dirty_bbox(prev, curr)
                if bbox is None:
                    dt = time.time() - t0
                    if dt < period:
                        time.sleep(period - dt)
                    continue
                x1, y1, w, h = bbox

            bytes_sent += send_rect(ser, curr, x1, y1, w, h)
            prev = curr
            update_count += 1

            if update_count % 10 == 0:
                elapsed = time.time() - t_start
                print(f"\r{update_count} updates, {update_count / elapsed:.1f}/s, "
                      f"{bytes_sent / elapsed / 1024:.0f} KB/s, last rect {w}x{h}", end="", flush=True)

            dt = time.time() - t0
            if dt < period:
                time.sleep(period - dt)
    except KeyboardInterrupt:
        pass
    finally:
        ser.close()
        print("\nStopped.")


if __name__ == "__main__":
    sys.exit(main())
