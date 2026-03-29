# TizenClaw PSS Memory Profiling

This document outlines the memory footprint optimizations implemented in Phase 19.4 for the TizenClaw daemon, measured using the Proportional Set Size (PSS) metric. PSS provides a more accurate representation of the daemon's actual memory footprint than Resident Set Size (RSS) because it proportionally accounts for memory shared with other processes (like shared libraries).

## Methodology

We measured the PSS using the `smaps` structure on the `x86_64` Tizen emulator:
```bash
sdb shell "cat /proc/<PID>/smaps | grep -i pss | awk '{sum+=\$2} END {print sum}'"
```

## Results

### Baseline Idle State
After a fresh daemon restart with unloaded channels:
- **Baseline PSS:** ~8.8 MB

### Simulated Active State
After interacting with the `tizenclaw-cli` to trigger the LLM backend operations and cache allocations (especially in SQLite):
- **Peak PSS:** ~10.1 MB

### Idle Memory Flush
We introduced a background maintenance loop in `AgentCore` that tracks the last activity time. When the system remains idle for a configured timeout (5 minutes), the daemon forcefully returns memory to the OS:
1. `sqlite3_release_memory(1024 * 1024 * 50)`: Flushes the SQLite query caches and pager memory.
2. `malloc_trim(0)`: Returns unused heap blocks to the OS.

After the idle timeout successfully triggered:
- **Post-Flush PSS:** ~8.5 MB

## Conclusion
The `malloc_trim` and SQLite cache flushing successfully reclaim memory allocated during active bursts. The daemon's idle memory footprint now converges back to under **9 MB**, ensuring it meets the stringent resource constraints of Tizen edge devices.
