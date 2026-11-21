# C Track Master Index (`INDEX.md`)

## Quick Topic Jump
- [Atomics & Concurrency](#atomics--concurrency)
- [Build Systems & Tooling](#build-systems--tooling)
- [Data Structures & Algorithms](#data-structures--algorithms)
- [Memory Management & Allocators](#memory-management--allocators)
- [Networking & Sockets](#networking--sockets)
- [Performance & Hardware Optimization](#performance--hardware-optimization)
- [Projects & Production Implementations](#projects--production-implementations)
- [Security & Safe Coding](#security--safe-coding)
- [Systems Programming & POSIX](#systems-programming--posix)

---

### Atomics & Concurrency
- [04-advanced/01-custom-allocators-and-arenas.md](./04-advanced/01-custom-allocators-and-arenas.md)
- [05-systems-programming/05-pthreads.md](./05-systems-programming/05-pthreads.md)
- [05-systems-programming/06-mutex-condvar-semaphores.md](./05-systems-programming/06-mutex-condvar-semaphores.md)
- [05-systems-programming/07-c11-atomics-and-memory-order.md](./05-systems-programming/07-c11-atomics-and-memory-order.md)
- [05-systems-programming/08-c11-threads-h.md](./05-systems-programming/08-c11-threads-h.md)
- [09-performance/08-false-sharing-and-concurrency-performance.md](./09-performance/08-false-sharing-and-concurrency-performance.md)
- [11-projects/07-thread-pool/README.md](./11-projects/07-thread-pool/README.md)
- [11-projects/13-ring-buffer-logger/README.md](./11-projects/13-ring-buffer-logger/README.md)

### Build Systems & Tooling
- [00-setup/01-toolchain.md](./00-setup/01-toolchain.md)
- [00-setup/03-compilation-process.md](./00-setup/03-compilation-process.md)
- [06-build-and-tooling/01-make-in-depth.md](./06-build-and-tooling/01-make-in-depth.md)
- [06-build-and-tooling/02-cmake.md](./06-build-and-tooling/02-cmake.md)
- [06-build-and-tooling/03-static-analysis-clang-tidy-cppcheck.md](./06-build-and-tooling/03-static-analysis-clang-tidy-cppcheck.md)
- [06-build-and-tooling/04-clang-format.md](./06-build-and-tooling/04-clang-format.md)
- [06-build-and-tooling/05-gcov-coverage.md](./06-build-and-tooling/05-gcov-coverage.md)
- [06-build-and-tooling/07-cross-compilation-intro.md](./06-build-and-tooling/07-cross-compilation-intro.md)
- [06-build-and-tooling/11-debug-vs-release-builds-and-sanitizer-ci.md](./06-build-and-tooling/11-debug-vs-release-builds-and-sanitizer-ci.md)

### Memory Management & Allocators
- [02-core-concepts/08-memory-management.md](./02-core-concepts/08-memory-management.md)
- [04-advanced/01-custom-allocators-and-arenas.md](./04-advanced/01-custom-allocators-and-arenas.md)
- [04-advanced/02-memory-pools.md](./04-advanced/02-memory-pools.md)
- [04-advanced/03-memory-debugging-valgrind-asan.md](./04-advanced/03-memory-debugging-valgrind-asan.md)
- [10-design-and-patterns/05-resource-management-patterns.md](./10-design-and-patterns/05-resource-management-patterns.md)
- [10-design-and-patterns/07-ownership-and-lifetime-conventions.md](./10-design-and-patterns/07-ownership-and-lifetime-conventions.md)
- [11-projects/04-memory-allocator/README.md](./11-projects/04-memory-allocator/README.md)

### Performance & Hardware Optimization
- [09-performance/01-cache-and-memory-hierarchy.md](./09-performance/01-cache-and-memory-hierarchy.md)
- [09-performance/02-profiling-perf-gprof-valgrind-callgrind.md](./09-performance/02-profiling-perf-gprof-valgrind-callgrind.md)
- [09-performance/03-compiler-optimizations.md](./09-performance/03-compiler-optimizations.md)
- [09-performance/04-branch-prediction-and-data-layout.md](./09-performance/04-branch-prediction-and-data-layout.md)
- [09-performance/05-simd-intrinsics-intro.md](./09-performance/05-simd-intrinsics-intro.md)
- [09-performance/06-benchmarking-correctly.md](./09-performance/06-benchmarking-correctly.md)
- [09-performance/07-struct-of-arrays-vs-array-of-structs.md](./09-performance/07-struct-of-arrays-vs-array-of-structs.md)
- [09-performance/08-false-sharing-and-concurrency-performance.md](./09-performance/08-false-sharing-and-concurrency-performance.md)
- [09-performance/09-link-time-optimization-and-pgo.md](./09-performance/09-link-time-optimization-and-pgo.md)
- [09-performance/10-inlining-and-code-size.md](./09-performance/10-inlining-and-code-size.md)

### Security & Safe Coding
- [03-intermediate/10-undefined-behavior-and-sanitizers.md](./03-intermediate/10-undefined-behavior-and-sanitizers.md)
- [08-security-and-safety/01-buffer-overflows.md](./08-security-and-safety/01-buffer-overflows.md)
- [08-security-and-safety/02-format-string-vulnerabilities.md](./08-security-and-safety/02-format-string-vulnerabilities.md)
- [08-security-and-safety/03-integer-overflow-and-wraparound.md](./08-security-and-safety/03-integer-overflow-and-wraparound.md)
- [08-security-and-safety/04-use-after-free-and-double-free.md](./08-security-and-safety/04-use-after-free-and-double-free.md)
- [08-security-and-safety/05-secure-coding-cert-c.md](./08-security-and-safety/05-secure-coding-cert-c.md)
- [08-security-and-safety/06-misra-c-overview.md](./08-security-and-safety/06-misra-c-overview.md)
- [08-security-and-safety/07-hardening-flags.md](./08-security-and-safety/07-hardening-flags.md)
- [08-security-and-safety/08-race-conditions-and-toctou.md](./08-security-and-safety/08-race-conditions-and-toctou.md)
- [08-security-and-safety/09-safe-string-handling-patterns.md](./08-security-and-safety/09-safe-string-handling-patterns.md)
- [08-security-and-safety/10-secure-random-and-crypto-pitfalls.md](./08-security-and-safety/10-secure-random-and-crypto-pitfalls.md)
- [08-security-and-safety/11-exploit-mitigations-explained.md](./08-security-and-safety/11-exploit-mitigations-explained.md)
