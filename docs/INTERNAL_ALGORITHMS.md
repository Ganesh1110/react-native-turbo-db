# Internal Algorithms & Storage Roadmap

This document captures the core architectural design and algorithmic strategy for the Secure DB engine. 

---

## Security & Architecture Disclaimer

This project addresses critical flaws often found in mobile storage engines, such as WAL durability, mmap memory management, B+ Tree persistence, and key transport security.

---

## Storage Engine & `mmap`

### POSIX File I/O
The engine uses standard syscalls (`open`, `lseek`, `write`) to manage a `.db` file.

### Memory Mapping (`mmap`)
- We map the database file directly into memory using `<sys/mman.h>`.
- Provides a raw `void*` pointer for zero-copy data access.
- `msync()` is used to flush memory back to the physical disk.

### Block Management
The mapped memory is divided into 4KB chunks. A "Free Space Map" tracks empty vs. full blocks.

---

## Serialization & The B-Tree

### Binary Serialization
Data is stored in binary format (not JSON strings) to optimize space and parsing performance.

### B+ Tree Index
- A persistent B+ Tree structure manages indices.
- When a record is inserted, its ID is added to the B+ Tree, pointing to the exact memory offset.
- **Node Splitting**: Handled with batching to absorb latency spikes.

---

## OS-Level Security & Encryption

### C++ Cryptography
- AES-256-GCM encryption on 4KB blocks.
- Uses hardware-backed keys where possible (Secure Enclave on iOS, Keystore on Android).

---

## Reliability & Write-Ahead Log (WAL)

### The WAL File
A secondary `.wal` file captures mutations before they are applied to the main database.

### Commit Flow
1. Write to WAL.
2. Mark as committed.
3. Apply to main `mmap` file.

### Crash Recovery
On boot, the engine replays unapplied transactions from the WAL to ensure consistency.

---

## Timeline & Phases
1. **Phase 1**: C++ & Memory Foundation
2. **Phase 2**: JSI Skeleton
3. **Phase 3**: mmap Storage
4. **Phase 4**: B+ Tree Persistence
5. **Phase 5**: Encryption & Key Management
6. **Phase 6**: Reliability (WAL)
7. **Phase 7**: API Polish
