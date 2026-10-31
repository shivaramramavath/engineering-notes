# Editor Setup

A good editor setup for C gives you instant error feedback, jump-to-definition, and consistent formatting. The goal is not a fancy IDE but a language server that understands your actual build.

## How editor support works

```text
Editor (VS Code, Neovim, ...)
   │  Language Server Protocol
   ▼
clangd  ──reads──▶  compile_commands.json
   │                (the exact compiler flags per file)
   ▼
diagnostics, completion, go-to-definition, rename
```

`clangd` parses your code the same way the compiler would, so it needs to know your include paths and `-D` defines. That information comes from `compile_commands.json`. Without it, clangd guesses and often reports false errors on headers it can't find.

## VS Code setup

1. Install the **clangd** extension (`llvm-vs-code-extensions.vscode-clangd`).
2. Install `clangd` itself (`sudo apt install clangd`, or it ships with Xcode tools / LLVM).
3. Disable the Microsoft **C/C++** extension's IntelliSense, or uninstall it. Running both gives duplicate and conflicting diagnostics.
4. Generate `compile_commands.json` (below).
5. Install a debugger front end: **CodeLLDB** or the C/C++ extension's debugging features with gdb.

## Generating `compile_commands.json`

```bash
# CMake projects
cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
ln -s build/compile_commands.json .

# Make projects (needs the `bear` tool)
bear -- make

# A single file, no build system: use a compile_flags.txt instead
printf -- '-std=c17\n-Wall\n-Wextra\n' > compile_flags.txt
```

`compile_flags.txt` (one flag per line, in the project root) is the simplest option for small exercises that have no build system.

## Formatting and linting

Automate style so nobody argues about it.

`.clang-format`:

```yaml
BasedOnStyle: LLVM
IndentWidth: 4
ColumnLimit: 100
```

`.editorconfig`:

```ini
root = true

[*.{c,h}]
indent_style = space
indent_size = 4
end_of_line = lf
insert_final_newline = true
trim_trailing_whitespace = true
```

```bash
clang-format -i src/*.c include/*.h   # format in place
clang-tidy src/main.c -- -std=c17      # static checks (see static-analysis)
```

Enable "format on save" in your editor once the `.clang-format` file is committed.

## Other editors

Any editor with LSP support works with clangd: Neovim (`nvim-lspconfig`), Emacs (`eglot`), Helix, Zed, and CLion (which has its own engine). The `compile_commands.json` approach is the same everywhere.

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| `'stdio.h' file not found` in editor only | clangd can't find system headers; check `clangd --version` and your toolchain install |
| Errors for macros or includes that compile fine | Stale or missing `compile_commands.json` |
| Duplicate red squiggles | Two language servers active (clangd + MS IntelliSense) |
| Header shows errors but `.c` files don't | Headers aren't in the compile database; clangd infers from a neighboring `.c` file, so keep headers next to or under the source tree |

## Summary

- Use `clangd` + `compile_commands.json`; avoid running two language servers.
- Commit `.clang-format` and `.editorconfig` so formatting is automatic and consistent.
- For tiny single-file programs, `compile_flags.txt` is enough.

**Next:** [The compilation process](./03-compilation-process.md)
