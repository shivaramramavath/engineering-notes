# Module 04: The NPM Ecosystem & Package Engineering

> Modern package management, SemVer rigor, subpath exports/imports, and enterprise monorepo workspace architectures.

---

## Module Overview

Node.js dependency management has evolved from simple nested `node_modules` into advanced content-addressable storage engines (`pnpm`), strict encapsulation via the `package.json` `"exports"` field, internal subpath aliases (`#`), and high-performance monorepo build pipelines.

---

## Contents

| Index | Note | Core Focus |
| :---: | :--- | :--- |
| **01** | [01-semantic-versioning.md](./01-semantic-versioning.md) | SemVer specifications, caret (`^`) vs tilde (`~`), lockfile determinism. |
| **02** | [02-dependencies-and-scripts.md](./02-dependencies-and-scripts.md) | `dependencies`, `devDependencies`, `peerDependencies`, lifecycle hooks. |
| **03** | [03-publishing-a-package.md](./03-publishing-a-package.md) | Publishing to npm, 2FA, scoped packages (`@org/pkg`), automated release. |
| **04** | [04-package-json-deep-dive-exports-imports.md](./04-package-json-deep-dive-exports-imports.md) | Conditional `"exports"`, subpath encapsulation, private `#` imports. |
| **05** | [05-npm-pnpm-yarn-bun-and-workspaces.md](./05-npm-pnpm-yarn-bun-and-workspaces.md) | Package manager comparison, phantom dependencies, pnpm workspaces. |

---

## Next Steps
Proceed to [05-http-web](../05-http-web/README.md) to explore low-level HTTP protocols, keep-alive connections, streaming uploads, and Server-Sent Events.
