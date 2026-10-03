# 18 — Design Patterns

A design pattern is a **named, reusable solution to a problem that keeps showing up** in software. The value isn't the code itself — it's the shared vocabulary. Saying "that's a strategy" or "wrap it in an adapter" communicates a whole structure in a few words, and it points you at a solution that has already survived many codebases.

This folder covers the patterns that appear most often in Node.js backends, with examples drawn from the things you've already seen in this repo (database connections, payment providers, event emitters, middleware).

## What this folder covers

| File | Patterns | Problem they solve |
|------|----------|--------------------|
| `01-singleton-and-factory.md` | Singleton, Factory | "One shared instance" and "create the right object for me" |
| `02-strategy-and-observer.md` | Strategy, Observer | "Swap behavior at runtime" and "notify many listeners of a change" |
| `03-adapter-and-decorator.md` | Adapter, Decorator | "Make incompatible things fit" and "add behavior without modifying the original" |

## The three families (for orientation)

Patterns are traditionally grouped by intent:

- **Creational** — how objects get created (Singleton, Factory)
- **Behavioral** — how objects communicate and share responsibility (Strategy, Observer)
- **Structural** — how objects are composed into larger structures (Adapter, Decorator)

You don't need to memorize the categories. They're a filing system, not a requirement.

## Patterns you've already met

Many of these show up elsewhere in the repo, just without the label:

| Where | Pattern in disguise |
|-------|---------------------|
| `02-core-modules/04-events.md` — `EventEmitter` | **Observer** |
| `06-express/02-middleware.md` — chained `app.use(...)` | **Decorator** / chain of responsibility |
| `07-databases/` — one shared connection or pool | **Singleton** (via the module cache) |
| `08-authentication-security/` — password/JWT/OAuth login options | **Strategy** (Passport.js is literally built on it) |
| `10-architecture/03-repository-and-service-pattern.md` | **Adapter**-like wrapping of a data source |
| `10-architecture/04-dependency-injection.md` | Makes Strategy and Factory easy to swap and test |

## A word of caution

Patterns are tools, not goals. The classic failure mode is **pattern overuse**: wrapping a five-line function in a factory, a strategy, and an interface because "that's good design." JavaScript's first-class functions, closures, and module system make several classic patterns far lighter than their Java-flavored textbook versions.

A useful test before reaching for any pattern: *what concrete problem am I solving right now?* If you can't name it, don't add the pattern yet. Each file in this folder includes a **"When *not* to use it"** section for exactly this reason.

## Suggested reading order

1. `01-singleton-and-factory.md`
2. `02-strategy-and-observer.md`
3. `03-adapter-and-decorator.md`

They're independent, but the later files lean on the earlier ones (a factory often builds the strategy you pick at runtime).

## Next

**`01-singleton-and-factory.md`** starts with the two creational patterns: sharing exactly one instance, and centralizing how objects get created.
