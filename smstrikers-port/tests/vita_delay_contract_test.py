from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[1]
SOURCE_ROOTS = (ROOT / "src",)
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hpp"}


def fail(message: str) -> None:
    print(f"FAIL: {message}", file=sys.stderr)
    raise SystemExit(1)


delay_call = re.compile(r"sceKernelDelayThread\s*\(")
allowed_delay_files = {
    ROOT / "src" / "platform" / "host.c",
}

for root in SOURCE_ROOTS:
    for path in root.rglob("*"):
        if not path.is_file() or path.suffix.lower() not in SOURCE_SUFFIXES:
            continue
        if any(part.startswith("build-") or part in {"_deps", ".git"} for part in path.relative_to(root).parts):
            continue
        text = path.read_text(encoding="utf-8", errors="ignore")
        for match in delay_call.finditer(text):
            if path not in allowed_delay_files:
                rel = path.relative_to(ROOT)
                line = text.count("\n", 0, match.start()) + 1
                fail(f"direct Vita delay syscall outside checked wrapper in {rel}:{line}")

host = (ROOT / "src" / "platform" / "host.c").read_text(encoding="utf-8")
checked_start = host.index("static void vita_delay_us_checked(long long requested_us)")
checked_end = host.index("void port_sleep_ns(unsigned long long ns)", checked_start)
checked_body = host[checked_start:checked_end]
if "if (requested_us <= 0)" not in checked_body:
    fail("Vita checked delay wrapper must reject every non-positive request")
if checked_body.count("sceKernelDelayThread") != 1:
    fail("Vita checked delay wrapper must be the only Strikers delay syscall site")

sleep_start = host.index("void port_sleep_ns(unsigned long long ns)")
sleep_end = host.index("void port_yield(void)", sleep_start)
sleep_body = host[sleep_start:sleep_end]
if "if (ns == 0)" not in sleep_body:
    fail("Vita port_sleep_ns must reject a zero-duration request before the syscall")

yield_start = sleep_end
yield_end = host.index("void* port_aligned_alloc", yield_start)
yield_body = host[yield_start:yield_end]
if "sceKernelDelayThread" in yield_body:
    fail("Vita port_yield must not use sceKernelDelayThread")
if 'volatile("yield"' not in yield_body:
    fail("Vita port_yield must use the ARM yield hint")

print("vita delay contract: OK")
