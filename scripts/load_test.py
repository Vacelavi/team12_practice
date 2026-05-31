#!/usr/bin/env python3
"""HTTP load test for the ITMO notification service.

Scenario: mixed scheduler workload with due polling, new notifications, sent
marks and cancellations.
"""
from __future__ import annotations

import argparse
import asyncio
import json
import random
import re
import sys
import time
import uuid
from pathlib import Path
from typing import Tuple

try:
    import aiohttp
except ImportError:
    print("aiohttp is required: pip install aiohttp", file=sys.stderr)
    sys.exit(1)


def parse_duration(s: str) -> float:
    m = re.fullmatch(r"(\d+(?:\.\d+)?)(s|m|h)?", s.strip())
    if not m:
        raise argparse.ArgumentTypeError(f"bad duration: {s}")
    v = float(m.group(1))
    return v * {"s": 1, "m": 60, "h": 3600, None: 1}[m.group(2)]


def percentile(values: list[float], p: float) -> float:
    if not values:
        return 0.0
    s = sorted(values)
    k = (len(s) - 1) * (p / 100.0)
    f = int(k)
    c = min(f + 1, len(s) - 1)
    if f == c:
        return s[f]
    return s[f] + (s[c] - s[f]) * (k - f)


class Stats:
    def __init__(self) -> None:
        self.latencies_due: list[float] = []
        self.latencies_add: list[float] = []
        self.latencies_sent: list[float] = []
        self.latencies_cancel: list[float] = []
        self.errors: int = 0

    def report(self, duration: float) -> str:
        def block(name: str, lats: list[float]) -> str:
            if not lats:
                return f"  {name:8s}: no samples"
            rps = len(lats) / duration
            return (f"  {name:8s}: n={len(lats):>7d}  rps={rps:>8.1f}  "
                    f"p50={percentile(lats,50)*1000:>7.2f}ms  "
                    f"p95={percentile(lats,95)*1000:>7.2f}ms  "
                    f"p99={percentile(lats,99)*1000:>7.2f}ms  "
                    f"max={max(lats)*1000:>7.2f}ms")

        total = (len(self.latencies_due) + len(self.latencies_add) +
                 len(self.latencies_sent) + len(self.latencies_cancel))
        return "\n".join([
            "=" * 72,
            f"Load test report  (duration={duration:.1f}s, "
            f"total={total}, errors={self.errors}, rps={total/duration:.1f})",
            "-" * 72,
            block("due", self.latencies_due),
            block("add", self.latencies_add),
            block("sent", self.latencies_sent),
            block("cancel", self.latencies_cancel),
            "=" * 72,
        ])


def make_notification(rng: random.Random, due_now: int) -> dict:
    channel = rng.choice(["email", "push", "sms"])
    user_no = rng.randint(1, 100_000)
    return {
        "id": str(uuid.uuid4()),
        "user_id": f"u-{user_no}",
        "channel": channel,
        "recipient": f"user{user_no}@example.com" if channel == "email" else f"+7900{user_no:07d}",
        "template": rng.choice(["payment_reminder", "promo", "weekly_digest"]),
        "payload": {"order_id": f"o-{rng.randint(1, 500_000)}"},
        "send_at": due_now + rng.randint(-3600, 24 * 3600),
        "priority": rng.randint(0, 9),
        "created_at": due_now - rng.randint(0, 3600),
    }


async def worker(session: aiohttp.ClientSession, url: str, deadline: float,
                 due_now: int, sample_ids: list[str], stats: Stats,
                 rng: random.Random) -> None:
    while time.monotonic() < deadline:
        x = rng.random()
        if x < 0.45:
            t0 = time.perf_counter()
            try:
                async with session.get(f"{url}/v1/due",
                                       params={"now": due_now, "limit": 100}) as r:
                    body = await r.json(content_type=None)
                    if r.status >= 400:
                        stats.errors += 1
                    else:
                        stats.latencies_due.append(time.perf_counter() - t0)
                        if body and rng.random() < 0.25:
                            nid = body[0]["id"]
                            t1 = time.perf_counter()
                            async with session.post(f"{url}/v1/notifications/{nid}/sent") as sr:
                                await sr.read()
                                if sr.status >= 400:
                                    stats.errors += 1
                                else:
                                    stats.latencies_sent.append(time.perf_counter() - t1)
            except Exception:
                stats.errors += 1
        elif x < 0.80:
            t0 = time.perf_counter()
            try:
                async with session.post(f"{url}/v1/notifications",
                                        json=make_notification(rng, due_now)) as r:
                    await r.read()
                    if r.status >= 400:
                        stats.errors += 1
                    else:
                        stats.latencies_add.append(time.perf_counter() - t0)
            except Exception:
                stats.errors += 1
        else:
            nid = rng.choice(sample_ids)
            t0 = time.perf_counter()
            try:
                async with session.delete(f"{url}/v1/notifications/{nid}") as r:
                    await r.read()
                    if r.status >= 400 and r.status != 404:
                        stats.errors += 1
                    else:
                        stats.latencies_cancel.append(time.perf_counter() - t0)
            except Exception:
                stats.errors += 1


async def amain(args: argparse.Namespace) -> Tuple[Stats, float]:
    meta_path = Path(args.meta)
    if not meta_path.exists():
        print(f"meta file not found: {meta_path} — run gen_dataset.py first",
              file=sys.stderr)
        sys.exit(1)
    payload = json.loads(meta_path.read_text())
    due_now = int(payload["due_now"])
    sample_ids = payload["sample_ids"]

    rng = random.Random(args.seed)
    stats = Stats()
    timeout = aiohttp.ClientTimeout(total=10)
    connector = aiohttp.TCPConnector(limit=args.concurrency)
    started = time.monotonic()
    deadline = started + args.duration

    async with aiohttp.ClientSession(timeout=timeout, connector=connector) as session:
        workers = [
            worker(session, args.url, deadline, due_now, sample_ids, stats,
                   random.Random(rng.getrandbits(64)))
            for _ in range(args.concurrency)
        ]
        await asyncio.gather(*workers)
    return stats, time.monotonic() - started


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--url", default="http://localhost:8080")
    ap.add_argument("--duration", type=parse_duration, default=60.0)
    ap.add_argument("--concurrency", type=int, default=50)
    ap.add_argument("--meta", default="data/meta.json")
    ap.add_argument("--seed", type=int, default=1)
    args = ap.parse_args()

    stats, elapsed = asyncio.run(amain(args))
    print(stats.report(elapsed))
    return 0


if __name__ == "__main__":
    sys.exit(main())
