#!/usr/bin/env python3
"""Generate a synthetic notification dataset for benchmarking the scheduler.

Layout per line (JSONL):
    {"id": "...", "user_id": "...", "channel": "email",
     "recipient": "...", "template": "...", "payload": {...},
     "send_at": <unix_seconds>, "priority": <int>, "created_at": <unix_seconds>}

The distribution intentionally contains many equal send_at values and a large
mix of due/future notifications so ordering, top-K and full scans are visible.
"""
from __future__ import annotations

import argparse
import json
import random
import sys
import time
import uuid
from pathlib import Path


CHANNELS = ["email", "push", "sms"]
TEMPLATES = [
    "payment_reminder", "delivery_update", "promo", "security_alert",
    "weekly_digest", "cart_reminder", "subscription_renewal", "event_invite",
]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="data/notifications.jsonl", type=Path)
    ap.add_argument("--count", type=int, default=100_000)
    ap.add_argument("--seed", type=int, default=42)
    args = ap.parse_args()

    rng = random.Random(args.seed)
    uuid_rng = random.Random(args.seed ^ 0xA11CE)
    base_ts = 1_700_000_000
    due_now = base_ts + 3600

    args.out.parent.mkdir(parents=True, exist_ok=True)
    sample_ids: list[str] = []
    started = time.time()
    with args.out.open("w", encoding="utf-8") as f:
        for i in range(args.count):
            notification_id = str(uuid.UUID(int=uuid_rng.getrandbits(128), version=4))
            if i < 20_000:
                sample_ids.append(notification_id)

            bucket = rng.random()
            if bucket < 0.25:
                send_at = due_now - rng.randint(0, 3600)
            elif bucket < 0.35:
                # Many identical timestamps expose unstable/incomplete comparators.
                send_at = due_now
            else:
                send_at = due_now + rng.randint(1, 7 * 24 * 3600)

            channel = rng.choice(CHANNELS)
            user_no = rng.randint(1, 50_000)
            doc = {
                "id": notification_id,
                "user_id": f"u-{user_no}",
                "channel": channel,
                "recipient": f"user{user_no}@example.com" if channel == "email" else f"+7900{user_no:07d}",
                "template": rng.choice(TEMPLATES),
                "payload": {"order_id": f"o-{rng.randint(1, 200_000)}"},
                "send_at": send_at,
                "priority": rng.randint(0, 9),
                "created_at": base_ts - rng.randint(0, 24 * 3600),
            }
            f.write(json.dumps(doc, ensure_ascii=False))
            f.write("\n")

    meta = {
        "due_now": due_now,
        "sample_ids": sample_ids,
        "channels": CHANNELS,
        "templates": TEMPLATES,
    }
    meta_path = args.out.parent / "meta.json"
    meta_path.write_text(json.dumps(meta, ensure_ascii=False, indent=2))

    elapsed = time.time() - started
    size_mb = args.out.stat().st_size / (1024 * 1024)
    print(f"wrote {args.count} notifications -> {args.out} ({size_mb:.1f} MB, {elapsed:.1f}s)")
    print(f"wrote meta -> {meta_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
