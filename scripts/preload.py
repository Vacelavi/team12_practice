#!/usr/bin/env python3
"""Preload generated notifications into a running notification_service."""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

try:
    import requests
except ImportError:
    print("requests is required: pip install requests", file=sys.stderr)
    sys.exit(1)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--url", default="http://localhost:8080")
    ap.add_argument("--data", default="data/notifications.jsonl", type=Path)
    args = ap.parse_args()

    if not args.data.exists():
        print(f"dataset not found: {args.data} — run gen_dataset.py first", file=sys.stderr)
        return 1

    total = 0
    with args.data.open("r", encoding="utf-8") as f:
        for line in f:
            if not line.strip():
                continue
            notification = json.loads(line)
            r = requests.post(f"{args.url}/v1/notifications", json=notification, timeout=10)
            if r.status_code >= 400:
                print(f"failed to upload {notification.get('id')}: {r.status_code} {r.text}",
                      file=sys.stderr)
                return 1
            total += 1
            if total % 10_000 == 0:
                print(f"uploaded {total} notifications")
    print(f"uploaded {total} notifications")
    return 0


if __name__ == "__main__":
    sys.exit(main())
