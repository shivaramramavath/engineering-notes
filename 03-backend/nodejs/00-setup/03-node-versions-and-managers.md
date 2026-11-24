# Node.js Versions & Version Managers (nvm, fnm, Volta)

> Comprehensive guide to the Node.js release lifecycle, LTS schedules, and enterprise version management using `fnm`, `nvm`, and `Volta`.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS ("Jod") / Node.js 24 |
| **Tooling** | `fnm`, `nvm`, `volta`, `.node-version`, `.nvmrc` |
| **Level** | Fundamental / DevOps |

---

## 1. Node.js Release Cycle & LTS Policy

Node.js follows an established release schedule:
- **Even-numbered versions** (e.g. Node.js 20, 22, 24) are released in April and transition to **Active Long Term Support (LTS)** in October of that year. They receive 30 months of active support and maintenance.
- **Odd-numbered versions** (e.g. Node.js 21, 23, 25) are released in October, receive experimental feature testing, and reach End of Life (EOL) within 6 months.
- **2026+ Transition**: Starting with Node.js 27, the project shifts to an annual single-version cadence dropping in April, moving to LTS every October.

```
+-------------------------------------------------------------------------+
|                        Node.js Release Timeline                         |
|                                                                         |
|  Node 20 (Iron):  [ Current ] ──> [ Active LTS ] ──> [ Maintenance ]──> EOL (Apr 2026)
|  Node 22 (Jod):   [ Current ] ──> [ Active LTS (Oct 2024 - Apr 2027) ]  |
|  Node 24 (Krypton): [ Current ] ──> [ Active LTS (Oct 2025 - Apr 2028) ]|
+-------------------------------------------------------------------------+
```

---

## 2. Comparison of Modern Version Managers

| Feature | `fnm` (Fast Node Manager) | `nvm` (Node Version Manager) | `Volta` |
| :--- | :--- | :--- | :--- |
| **Implementation Language** | Rust | POSIX Shell Script | Rust |
| **Execution Speed** | Extremely fast (< 10ms) | Slower (shell subshell overhead) | Instant (shims) |
| **Windows Support** | Native binary (PowerShell / CMD) | Requires `nvm-windows` fork | Native binary |
| **Configuration Files** | `.node-version`, `.nvmrc` | `.nvmrc` | `package.json` `"volta"` field |
| **Automatic Version Switching** | Via shell hook (`--use-on-cd`) | Manual `nvm use` (or custom hook) | Built-in via shims |

---

## 3. Practical Usage Examples

### 3.1 Using `fnm` (Recommended Modern Tool)
```bash
# Install Node 22 LTS
fnm install 22

# Set default global version
fnm default 22

# Automatically switch to directory's specified version
fnm use
```

### 3.2 Standardizing in Projects (`.node-version` / `.nvmrc`)
Create `.node-version` in the project root:
```text
22.18.0
```

And in `package.json`:
```json
{
  "engines": {
    "node": ">=22.0.0",
    "npm": ">=10.0.0"
  }
}
```
Enforce strict engine checks in npm:
```text
# .npmrc
engine-strict=true
```

---

## 4. Troubleshooting & Best Practices

1. **Avoid Sudo/Admin for Global Packages**: Node managers install binaries in user-space (`~/.fnm` or `~/.nvm`), avoiding permission errors (`EACCES`) when installing global CLIs.
2. **CI/CD Standardization**: In GitHub Actions, always pin Node version using `actions/setup-node` reading from `.node-version`:
   ```yaml
   - uses: actions/setup-node@v4
     with:
       node-version-file: '.node-version'
       cache: 'npm'
   ```
