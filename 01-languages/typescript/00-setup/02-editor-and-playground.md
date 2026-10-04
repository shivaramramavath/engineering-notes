# Editor and Playground

## Definition
The tools you use to see types while you write: an editor backed by TypeScript's language server (tsserver), and the online TypeScript Playground for quick experiments and sharing.

## Why It Matters
Most of learning TypeScript is reading what the compiler thinks a type is. Hover information, go-to-definition and quick fixes are the fastest feedback loop you have, faster than running `tsc`.

## Prerequisites
[01-first-project.md](01-first-project.md)

## VS Code
VS Code ships with TypeScript support; no extension is needed.

### Features worth learning on day one

| Feature | How | Use |
|---|---|---|
| Hover | Mouse over any identifier | See its inferred type |
| Go to definition | `F12` / Ctrl+Click | Jump to source or `.d.ts` |
| Go to type definition | Right-click menu | Jump to the type, not the value |
| Rename symbol | `F2` | Safe rename across files |
| Quick fix | `Ctrl+.` / `Cmd+.` | Auto-import, add missing property, etc. |
| Problems panel | `Ctrl+Shift+M` | All errors in the project |

### Use the workspace TypeScript version
By default VS Code uses its bundled TypeScript, which may differ from the version in `package.json`.

1. Open any `.ts` file.
2. Command Palette -> "TypeScript: Select TypeScript Version".
3. Choose "Use Workspace Version".

Many teams commit this to `.vscode/settings.json`:
```json
{
  "typescript.tsdk": "node_modules/typescript/lib"
}
```

### Restart when things look wrong
If errors do not match what `tsc` reports, run "TypeScript: Restart TS Server" from the Command Palette. This is the first thing to try when the editor is stale after a config or `node_modules` change.

## Inspecting types
A trick you will use constantly:

```ts
const user = { id: 1, name: "Ada" };
type User = typeof user;
//   ^? hover here: { id: number; name: string }
```

Other editors with TypeScript support (WebStorm, Neovim, Zed, etc.) use the same underlying language server, so the concepts carry over.

## The TypeScript Playground
The Playground is a browser editor at typescriptlang.org/play.

### Why use it
- Zero setup; try an idea in seconds.
- Switch TypeScript versions to check whether a behaviour changed.
- Toggle compiler options (`strict`, `target`, etc.) from the settings panel.
- See the emitted JavaScript and `.d.ts` output.
- Share a link that preserves code and options, ideal for bug reports and questions.

### Useful panels
| Panel | Shows |
|---|---|
| `.JS` | What your code compiles to (proves types are erased) |
| `.D.TS` | Declaration output |
| Errors | Full diagnostics |
| Logs | Console output |

### Good habits
- Reproduce a confusing type error in the Playground before asking for help; keep the example minimal.
- Make sure the options in the Playground match your project (especially `strict`).

## Common Mistakes
- Trusting the editor when it has gone stale; restart the TS server or run `tsc --noEmit`.
- Playground code passes but the project fails because the compiler options or TypeScript versions differ.
- Reading hover output on a complex type and not expanding it. Hover on the alias, then on the properties, to inspect step by step.

## Best Practices
- Pin the workspace TypeScript version.
- Use hover constantly while learning; it is the best teacher of inference.
- Keep a scratch file (`src/scratch.ts`) for experiments, and use the Playground for sharing.

## Related Topics
- [01-first-project.md](01-first-project.md)
- [18 reading type errors](../18-testing-and-debugging/06-reading-type-errors.md)
- [14 compiler architecture](../14-type-system-internals/05-compiler-architecture.md) (what the language server is)
