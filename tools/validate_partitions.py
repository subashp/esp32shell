"""Validate the ESP32 partition contract without requiring a board."""

from pathlib import Path
import csv
import sys


FLASH_SIZE = 0x2000000  # 32 MiB
REQUIRED = {
    "nvs": (0x9000, 0x5000),
    "otadata": (0xE000, 0x2000),
    "app0": (0x10000, 0x800000),
    "app1": (0x810000, 0x800000),
    "coredump": (0x1010000, 0x10000),
    "littlefs": (0x1020000, 0xFE0000),
}


def parse(path: Path):
    rows = []
    with path.open(encoding="utf-8") as handle:
        for row in csv.reader(line for line in handle if line.strip() and not line.lstrip().startswith("#")):
            name, _kind, _subtype, offset, size, *_ = [item.strip() for item in row]
            rows.append((name, int(offset, 0), int(size, 0)))
    return rows


def validate(path: Path) -> list[str]:
    errors = []
    rows = parse(path)
    actual = {name: (offset, size) for name, offset, size in rows}
    for name, expected in REQUIRED.items():
        if actual.get(name) != expected:
            errors.append(f"{name}: expected {expected}, got {actual.get(name)}")
    ranges = sorted((offset, offset + size, name) for name, offset, size in rows)
    for (start, end, name), (next_start, _next_end, next_name) in zip(ranges, ranges[1:]):
        if end > next_start:
            errors.append(f"overlap: {name} and {next_name}")
    for name, offset, size in rows:
        if offset + size > FLASH_SIZE:
            errors.append(f"{name}: ends at 0x{offset + size:x}, beyond 32MiB flash")
    return errors


if __name__ == "__main__":
    partition_path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("firmware/arduino/esp32shell/partitions.csv")
    problems = validate(partition_path)
    if problems:
        for problem in problems:
            print(f"error: {problem}")
        raise SystemExit(1)
    print(f"partition contract passed: {partition_path}")
