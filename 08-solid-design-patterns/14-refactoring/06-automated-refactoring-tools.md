# Automated Refactoring Tools

| Field | Value |
| :--- | :--- |
| **Category** | Refactoring (Tooling & AST Safety) |

> **TL;DR**: Never do manual find-and-replace for structural refactors. Use **AST-aware IDE refactors** (IntelliJ, VS Code TypeScript Language Server, PyCharm) for local moves, and **codemods** (OpenRewrite, `jscodeshift`, Semgrep, Rector) for repo-wide migrations.

## Tooling Matrix
| Ecosystem | Safe Local Refactoring | Repo-Wide Codemods / Migrations |
| :--- | :--- | :--- |
| **Java** | IntelliJ IDEA (Rename, Extract, Safe Delete) | **OpenRewrite** (Java 8→25, JUnit 4→5, Spring Boot upgrades) |
| **TypeScript** | TS Language Server (`F2` Rename, Move to File) | **ts-morph**, **jscodeshift**, **Semgrep** |
| **Python** | PyCharm / Pylance / Rope | **LibCST**, **Ruff** (`--fix`), **Semgrep** |
