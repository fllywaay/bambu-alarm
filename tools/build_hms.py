#!/usr/bin/env python3
"""Build data/hms_zh.bin and data/hms_en.bin: the official Bambu HMS / print_error texts, packed for the H2D alarm firmware.

Source: Bambu Studio's open-source repo (resources/hms/hms_<lang>_<devtype>.json). The device type is the
first 3 characters of the printer serial: 094 = H2D, 093 = H2S (same platform, newer texts).

Usage (run from the project folder):
    python tools/build_hms.py                      # downloads from GitHub, writes data/hms_zh.bin AND data/hms_en.bin
    python tools/build_hms.py --lang en            # only the English file (--lang zh-cn for only the Chinese one)
    python tools/build_hms.py --src C:\\path\\to\\BambuStudio\\resources\\hms   # use a local clone, no download

The firmware picks the file that matches its language setting (cfg.lang). Upload each one with ?lang=zh / ?lang=en.

Codes missing in the chosen language fall back to English. Earlier device types in --dev win on conflicts.

File format (little-endian):
    header   7 x u32: magic 'HMS1', nHms, nErr, hmsOff, errOff, blobOff, blobLen
    hms idx  nHms x {u64 key, u32 off}   sorted by key; key = attr<<32 | code  (the 16 hex digits)
    err idx  nErr x {u32 key, u32 off}   sorted by key; key = print_error      (the 8 hex digits)
    blob     NUL-terminated UTF-8 strings (deduplicated); off is relative to blobOff
"""
import argparse
import json
import struct
import sys
import urllib.error
import urllib.request
from pathlib import Path

RAW = "https://raw.githubusercontent.com/bambulab/BambuStudio/master/resources/hms/hms_{lang}_{dev}.json"
MAGIC = int.from_bytes(b"HMS1", "little")


def fetch(lang, dev, src):
    name = f"hms_{lang}_{dev}.json"
    if src:
        p = Path(src) / name
        return json.loads(p.read_text(encoding="utf-8")) if p.exists() else None
    url = RAW.format(lang=lang, dev=dev)
    try:
        with urllib.request.urlopen(url, timeout=60) as r:
            return json.loads(r.read().decode("utf-8"))
    except urllib.error.HTTPError as e:
        if e.code == 404:
            return None
        raise


def entries(blob, section):
    """{ecode_int: text} for one section of one file (the language key inside is the only non-'ver' key)."""
    sec = blob.get("data", {}).get(section, {})
    out = {}
    for k, v in sec.items():
        if k == "ver" or not isinstance(v, list):
            continue
        for it in v:
            ec, txt = it.get("ecode"), (it.get("intro") or "").strip()
            if ec and txt:
                out[int(ec, 16)] = " ".join(txt.split())     # one line, single spaces
    return out


def collect(lang, devs, src):
    tables = {"device_hms": {}, "device_error": {}}
    for dev in devs:
        blob = fetch(lang, dev, src)
        if blob is None:
            print(f"  (no file for {lang}/{dev})", file=sys.stderr)
            continue
        for sec in tables:
            for k, v in entries(blob, sec).items():
                tables[sec].setdefault(k, v)
        print(f"  loaded {lang}/{dev}")
    return tables


def build(lang, devs, src, out_path):
    main_t = collect(lang, devs, src)
    if lang != "en":
        en_t = collect("en", devs, src)
        for sec in main_t:
            for k, v in en_t[sec].items():
                main_t[sec].setdefault(k, v)

    hms, err = main_t["device_hms"], main_t["device_error"]
    if not hms and not err:
        sys.exit("nothing loaded - check your network or --src")
    if any(k >= 1 << 64 for k in hms) or any(k >= 1 << 32 for k in err):
        sys.exit("ecode out of range")

    blob = bytearray()
    off_of = {}
    for txt in sorted(set(hms.values()) | set(err.values())):
        off_of[txt] = len(blob)
        blob += txt.encode("utf-8") + b"\0"

    n_h, n_e = len(hms), len(err)
    hms_off = 28
    err_off = hms_off + 12 * n_h
    blob_off = err_off + 8 * n_e
    out = bytearray(struct.pack("<7I", MAGIC, n_h, n_e, hms_off, err_off, blob_off, len(blob)))
    for k in sorted(hms):
        out += struct.pack("<QI", k, off_of[hms[k]])
    for k in sorted(err):
        out += struct.pack("<II", k, off_of[err[k]])
    out += blob

    p = Path(out_path)
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_bytes(out)
    print(f"wrote {p}  ({len(out) / 1024:.0f} KB): {n_h} HMS codes, {n_e} print_error codes, lang={lang}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--lang", default="both", help="zh-cn, en, or both (default): one file per language")
    ap.add_argument("--dev", default="094,093", help="device types, highest priority first")
    ap.add_argument("--src", help="folder with hms_*.json (a local BambuStudio clone); default = download")
    ap.add_argument("--out", help="output file (only with a single --lang); default data/hms_zh.bin and data/hms_en.bin")
    a = ap.parse_args()
    devs = a.dev.split(",")

    langs = ["zh-cn", "en"] if a.lang == "both" else [a.lang]
    if a.out and len(langs) > 1:
        sys.exit("--out needs a single --lang (zh-cn or en)")
    data_dir = Path(__file__).resolve().parent.parent / "data"
    for lang in langs:
        out_path = a.out or str(data_dir / ("hms_en.bin" if lang == "en" else "hms_zh.bin"))
        build(lang, devs, a.src, out_path)


if __name__ == "__main__":
    main()
