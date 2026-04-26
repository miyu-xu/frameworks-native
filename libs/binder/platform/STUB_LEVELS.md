# Windows Platform Stub Levels

> Source: `windows_stubs.cpp`
> Date: 2026-04-25

## Maturity Levels

| Level | Definition |
|-------|-----------|
| **L0 (stub)** | Compiles but fails at runtime (returns `ENOSYS` or equivalent) |
| **L1 (partial)** | Basic functionality works but has semantic differences vs Linux/Android |
| **L2 (full)** | Full native Windows implementation |

## Stub Functions

### ashmem (Android shared memory)

| Function | Level | Notes |
|----------|-------|-------|
| `ashmem_valid()` | L1 | Accepts any valid CRT fd as "ashmem-valid enough" for host RPC use |
| `ashmem_create_region()` | L0 | No ashmem on Windows; returns `ENOSYS` |
| `ashmem_set_prot_region()` | L0 | No ashmem on Windows; returns `ENOSYS` |
| `ashmem_pin_region()` | L0 | No ashmem on Windows; returns `ENOSYS` |
| `ashmem_unpin_region()` | L0 | No ashmem on Windows; returns `ENOSYS` |
| `ashmem_get_size_region()` | L0 | No ashmem on Windows; returns `ENOSYS` |

### native_handle

| Function | Level | Notes |
|----------|-------|-------|
| `native_handle_close()` | L2 | Full implementation: iterates fd slots and calls `_close()` |
| `native_handle_close_with_tag()` | L2 | Delegates to `native_handle_close()` |
| `native_handle_init()` | L2 | Full implementation: init from storage buffer |
| `native_handle_create()` | L2 | Full implementation: allocate + init |
| `native_handle_set_fdsan_tag()` | L1 | Safe no-op (no fdsan on Windows) |
| `native_handle_unset_fdsan_tag()` | L1 | Safe no-op (no fdsan on Windows) |
| `native_handle_clone()` | L2 | Full implementation: `_dup()` for fd slots, copy for int slots |

### Threads & Binder

| Function | Level | Notes |
|----------|-------|-------|
| `androidSetThreadName()` | L2 | Full implementation using `SetThreadDescription` via `kernel32.dll` |
| `AIBinder_isHandlingTransaction()` | L1 | Returns `false` (kernel binder not available on host) |
