# Third-Party Types

Most packages you install are JavaScript. TypeScript needs type information for them, and that information reaches you in one of three ways: the package ships its own, the community provides it in `@types/*`, or nobody does and you write a minimal declaration yourself. This note explains how TypeScript finds types for a package, which option to prefer, and what to do when types are missing, outdated, or wrong.

**Prerequisites:**
- [Declaration files](./00-declaration-files.md)
- [Ambient declarations](./01-ambient-declarations.md)
- [Global and module augmentation](./02-global-and-module-augmentation.md) (for patching wrong types)

---

## How TypeScript finds types for `import "pkg"`

For a bare specifier like `"lodash"`, the compiler looks, roughly in this order:

```text
1. The package's own types
   node_modules/pkg/package.json  ->  "types" / "typings" field, or an "exports" "types" condition,
   or an index.d.ts next to the main file
        |
        v  (not found)
2. DefinitelyTyped
   node_modules/@types/pkg
        |
        v  (not found)
3. Error TS7016  (implicit any, when noImplicitAny is on)
   "Could not find a declaration file for module 'pkg'"
```

You can confirm what happened for a given import:

```bash
tsc --traceResolution | grep "pkg"
```

## Case 1: the package ships its own types

Many modern packages are written in TypeScript or include declarations. There is nothing to install. Check the package's `package.json` for a `types`, `typings`, or `exports` entry with a `types` condition, or look for `.d.ts` files in the published folder.

When a package ships types, an `@types/pkg` package is normally unnecessary. If the community package still exists, it is often marked deprecated on npm. Installing both can cause confusing conflicts.

## Case 2: DefinitelyTyped (`@types/*`)

[DefinitelyTyped](https://github.com/DefinitelyTyped/DefinitelyTyped) is the community repository of declaration files, published to npm under the `@types` scope.

```bash
npm install --save-dev @types/express @types/node
```

Details worth knowing:

- Install as **devDependencies**. They are build-time only. (Exception: if your *published library's* public `.d.ts` files import from an `@types` package, consumers need it, so list it in `dependencies`.)
- Scoped packages map `@scope/pkg` to `@types/scope__pkg`.
- Match the **major (and often minor) version** of the types to the library or runtime: `@types/react` to your `react` version, `@types/node` to the Node version you target. A mismatch shows up as APIs that exist at runtime but not in the types, or the other way around.
- The `@types` version number follows the library's `major.minor`, not an independent scheme. Patch versions are the types' own.
- Check availability with `npm view @types/<name> version` before assuming it does not exist.

### Automatic inclusion and the `types` option

Every package under `node_modules/@types` is included globally by default, which is how `describe`, `process`, and `Buffer` appear without any import. Two compiler options change that:

```json
{
  "compilerOptions": {
    "types": ["node", "vitest/globals"],
    "typeRoots": ["./node_modules/@types", "./types"]
  }
}
```

- `types`: **restricts** the automatically included global packages to this list. Setting it (even to `[]`) disables auto-inclusion for the rest. This only affects global inclusion. An explicit `import` of a package always loads its types.
- `typeRoots`: where to look for those global type packages instead of `node_modules/@types`.

A common surprise: someone adds `"types": ["node"]` to fix one conflict, and suddenly `describe` and `expect` are undefined.

## Case 3: no types exist

You get TS7016 under `noImplicitAny`. Options, from best to worst:

1. **Check for an alternative package** that ships types, if the library is unmaintained.
2. **Write a minimal declaration for the parts you use.** This is usually the best trade-off:

```ts
// types/legacy-chart.d.ts  (a script file, so this declares the module)
declare module "legacy-chart" {
  export interface ChartOptions { width: number; height: number }
  export function draw(el: HTMLElement, options: ChartOptions): void;
}
```

Make sure the file is covered by `include` or `typeRoots`. Declare only what you call. You can grow it later.

3. **Shorthand declaration** if you cannot spare the time:

```ts
declare module "legacy-chart";   // everything from it is `any`
```

This silences the error and removes all checking for that package. Use it knowingly, and leave a comment with a reason.

4. **Contribute types to DefinitelyTyped.** Worth it for a package other people use. Follow the repository's contribution guide.

## Case 4: types exist but are wrong or incomplete

Do not edit files in `node_modules`. They are overwritten on the next install. Instead:

- **Augment them** with [module augmentation](./02-global-and-module-augmentation.md) when you only need to add members to an interface the library declares:

```ts
import "some-lib";

declare module "some-lib" {
  interface Options {
    experimentalFlag?: boolean;
  }
}
```

- **Wrap the library** in a small module of your own with correct types, and import only that wrapper elsewhere. Casting happens in one place.
- **Narrow with a cast at the boundary** (`as unknown as Correct`) if you must, and add a comment and a test.
- **Patch the package** with a tool such as `patch-package` or your package manager's patching feature, if the fix has to live in the package itself.
- **Report or fix upstream.** For `@types/*` packages, a pull request to DefinitelyTyped is the real fix.

Types can also be *too loose*: a library returning `any` leaks `any` into your code. Wrap such calls with a validated type (see [runtime validation](../15-runtime-validation/00-trust-boundaries.md)) and read [unsafe types and assertions](../23-security/00-unsafe-types-and-assertions.md).

## Library authors: shipping types

If you publish a package, ship the types with it rather than relying on DefinitelyTyped:

- Set `types` (and a `types` condition in `exports`) and publish the `.d.ts` files. See [declaration files](./00-declaration-files.md).
- If you ship both ESM and CommonJS, provide matching `.d.mts` / `.d.cts` declarations.
- Run the `@arethetypeswrong/cli` tool on the packed output to catch resolution problems under different module modes.
- If your public types reference another package's types, make that package a `dependency` (or a `peerDependency`), not only a `devDependency`.

## `skipLibCheck`

```json
{ "compilerOptions": { "skipLibCheck": true } }
```

Skips checking of **all** `.d.ts` files, including those in `node_modules`. It fixes errors caused by two libraries declaring conflicting types and speeds up builds, so it is common. The cost is that real mistakes in declarations (including ones you write) are no longer reported. It does not change how your own code is checked against those types.

## Common mistakes

- **Installing `@types/x` for a package that already ships types.** Duplicate or conflicting declarations.
- **Mismatched `@types` and library versions.** Missing or phantom APIs.
- **Setting `types` in `tsconfig` and losing globals** like `describe` or `process`.
- **Leaving `declare module "x";` in place long term.** The package is `any`, and typos go unseen.
- **Editing `node_modules/**/*.d.ts`.** Changes vanish on reinstall.
- **Putting a hand-written declaration outside `include`/`typeRoots`.** It is ignored.
- **Augmenting a re-exporting module** instead of the module that declares the interface.
- **Forgetting that `@types` packages are build-time only** when shipping a library whose public API uses them.

## Debugging

- TS7016 on import: check whether the package ships types (`types` in its `package.json`), install `@types/<name>`, or add a declaration.
- Types seem old or wrong: `npm ls @types/<name>` shows which copy is installed. Duplicate copies in a monorepo or nested `node_modules` can load two versions.
- Global is missing (`describe`, `process`, `Buffer`): check `types` and `typeRoots`, and that the matching `@types/*` package is installed.
- `tsc --traceResolution` shows each lookup. `tsc --listFiles` shows which declaration files were loaded.
- Different results in editor vs CLI: the editor may use a different TypeScript version or `tsconfig`. Select the workspace TypeScript version.

## Quick summary

- TypeScript looks for a package's own types first, then `@types/<name>`, and errors (TS7016) if neither exists under `noImplicitAny`.
- Do not install `@types` for packages that ship their own types. Do match `@types` versions to the library or runtime.
- `types` restricts which `@types` packages are auto-included as globals. `typeRoots` changes where they are found.
- For untyped packages, prefer a small hand-written declaration over the shorthand `declare module "x";` (which is `any`).
- Fix wrong types by augmentation, wrapping, or an upstream fix. Never by editing `node_modules`.
- Library authors should ship their own types and verify them across module modes.

**Next:** [10 Advanced Types](../10-advanced-types/README.md)