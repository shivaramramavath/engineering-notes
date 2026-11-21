# Project Name

> Brief summary of the project, its architectural goals, and system programming capabilities demonstrated.

| Project Metric | Value |
|---|---|
| **Complexity** | Intermediate / Advanced |
| **Lines of Code** | ~500–1,500 LOC |
| **Dependencies** | Standard ISO C11 Library (POSIX APIs where noted) |
| **Build System** | GNU Make / CMake |
| **Testing** | Unity Test Framework / Custom Test Harness |

---

## Project Structure
```
project-name/
├── README.md               # Architecture, specification, build instructions
├── Makefile                # Automated targets: all, debug, test, clean
├── include/
│   └── project_api.h       # Public interface definitions
├── src/
│   ├── main.c              # Application entry point
│   └── project_impl.c      # Modular implementation (no global state)
└── tests/
    └── test_suite.c        # Automated unit and integration tests
```

---

## Functional Specification & Architecture
Detailed component diagram, state machine, data structures, and memory ownership contracts.

---

## Building and Testing
```bash
make clean
make
make test
```
