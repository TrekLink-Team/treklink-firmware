#!/usr/bin/env python3
"""Generate the manifest consumed by TrekLink firmware release APIs."""

import argparse
import hashlib
import json
import tempfile
from pathlib import Path


VARIANTS = (
    {
        "variantCode": "v2",
        "pioEnv": "treklink-v2",
        "chip": "esp32s3",
        "app": {"fileName": "firmware-treklink-v2.bin", "offset": 0x10000},
        "factory": {
            "fileName": "firmware-treklink-v2.factory.bin",
            "offset": 0x0,
        },
    },
    {
        "variantCode": "v3",
        "pioEnv": "treklink-v3-tbeam",
        "chip": "esp32",
        "app": {"fileName": "firmware-treklink-v3-tbeam.bin", "offset": 0x10000},
        "factory": {
            "fileName": "firmware-treklink-v3-tbeam.factory.bin",
            "offset": 0x0,
        },
    },
    {
        "variantCode": "v4",
        "pioEnv": "treklink-v4-supreme",
        "chip": "esp32s3",
        "app": {"fileName": "firmware-treklink-v4-supreme.bin", "offset": 0x10000},
        "factory": {
            "fileName": "firmware-treklink-v4-supreme.factory.bin",
            "offset": 0x0,
        },
    },
)


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as image:
        for chunk in iter(lambda: image.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def image_entry(input_dir, image):
    path = input_dir / image["fileName"]
    if not path.is_file():
        raise FileNotFoundError(f"Firmware image not found: {path}")
    return {
        "fileName": image["fileName"],
        "offset": image["offset"],
        "size": path.stat().st_size,
        "sha256": sha256(path),
    }


def build_manifest(version, input_dir, variants=VARIANTS):
    if not version:
        raise ValueError("Version must not be empty")

    entries = []
    for variant in variants:
        entries.append(
            {
                "variantCode": variant["variantCode"],
                "pioEnv": variant["pioEnv"],
                "chip": variant["chip"],
                "app": image_entry(input_dir, variant["app"]),
                "factory": image_entry(input_dir, variant["factory"]),
            }
        )
    return {"version": version, "variants": entries}


def write_manifest(manifest, output):
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


def self_test():
    test_variants = (
        {
            "variantCode": "test",
            "pioEnv": "treklink-test",
            "chip": "esp32s3",
            "app": {"fileName": "app.bin", "offset": 0x10000},
            "factory": {"fileName": "factory.bin", "offset": 0x0},
        },
    )
    with tempfile.TemporaryDirectory() as temp_dir_name:
        temp_dir = Path(temp_dir_name)
        app_data = b"treklink-app"
        factory_data = b"treklink-factory"
        (temp_dir / "app.bin").write_bytes(app_data)
        (temp_dir / "factory.bin").write_bytes(factory_data)

        output = temp_dir / "treklink-manifest.json"
        manifest = build_manifest("v0.0.0-treklink.0", temp_dir, test_variants)
        write_manifest(manifest, output)
        loaded = json.loads(output.read_text(encoding="utf-8"))
        expected = {
            "version": "v0.0.0-treklink.0",
            "variants": [
                {
                    "variantCode": "test",
                    "pioEnv": "treklink-test",
                    "chip": "esp32s3",
                    "app": {
                        "fileName": "app.bin",
                        "offset": 0x10000,
                        "size": len(app_data),
                        "sha256": hashlib.sha256(app_data).hexdigest(),
                    },
                    "factory": {
                        "fileName": "factory.bin",
                        "offset": 0x0,
                        "size": len(factory_data),
                        "sha256": hashlib.sha256(factory_data).hexdigest(),
                    },
                }
            ],
        }
        if loaded != expected:
            raise AssertionError(f"Unexpected self-test manifest: {loaded!r}")
    print("self-test passed")


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", help="Release tag stored in the manifest")
    parser.add_argument(
        "--input-dir",
        type=Path,
        default=Path("."),
        help="Directory containing the six release images",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("treklink-manifest.json"),
        help="Manifest output path",
    )
    parser.add_argument("--self-test", action="store_true")
    return parser.parse_args(), parser


def main():
    args, parser = parse_args()
    if args.self_test:
        self_test()
        return
    if not args.version:
        parser.error("--version is required unless --self-test is used")

    manifest = build_manifest(args.version, args.input_dir)
    write_manifest(manifest, args.output)
    print(f"Wrote {args.output}")


if __name__ == "__main__":
    main()
