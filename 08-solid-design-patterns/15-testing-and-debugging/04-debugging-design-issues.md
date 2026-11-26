# Debugging Design Issues

Some problems aren't a wrong line of code. They come from **structure**: hidden dependencies, tangled responsibilities, shared state, unclear boundaries. Fixing the symptom (a patch in one place) leaves the cause, and the problem returns in another form.

This note covers how to recognize structural problems, how to investigate them, and how to make a poorly designed codebase safe to change.

## Symptoms that point to design, not a typo

| Symptom | Likely structural cause |
|---|---|
| Every change breaks something unrelated | Tight coupling, hidden dependencies |
| A small feature touches many files | Scattered responsibility (shotgun surgery) |
| The same bug keeps reappearing in new forms | A rule duplicated in several places |
| Tests pass individually but fail together | Shared mutable or global state |
| "Works on my machine" / fails only in production | Hidden environment dependencies (time, config, ordering) |
| Nobody dares touch a particular class | Too many responsibilities, no tests |
| Flaky behavior that depends on call order | Temporal coupling |
| Code can't be tested without a database | Concrete dependencies, mixed concerns |
| Deep stack traces through many wrappers | Over-layering or proxy/decorator chains |

## An investigation routine

```text
1. Reproduce the problem reliably (a failing test if possible)
        ↓
2. Locate WHERE behavior goes wrong (trace, log, debugger)
        ↓
3. Ask WHY it was possible: what structure allowed this?
        ↓
4. Fix the cause, not just the symptom
        ↓
5. Add a test (and/or an architecture rule) so it can't regress
```

Step 3 is what separates debugging a design from patching a bug. Questions to ask:

- Is this rule implemented in more than one place?
- Who is allowed to change this state? Why are there many writers?
- What did this class secretly depend on?
- Could the compiler or a test have caught this? Why not?
- Is the boundary here in the right place?

## Finding structural problems with tools

### Dependency analysis

| Tool | What it shows |
|---|---|
| `jdeps` (JDK tool) | Dependencies between packages and modules; helps find unexpected coupling |
| IDE dependency views | Package and class dependency graphs, cycles |
| Static analysis (for example SonarQube) | Cyclic dependencies, complexity, duplication |
| Architecture tests (ArchUnit) | Executable rules about allowed dependencies |

```bash
# Package-level dependencies of a compiled jar
jdeps -verbose:package app.jar
```

```java
// Fail the build if there are cycles between feature packages
@ArchTest
static final ArchRule no_cycles =
        slices().matching("com.shop.(*)..").should().beFreeOfCycles();
```

### Change history ("hotspots" and co-change)

Version control reveals design problems:

```bash
# Files changed most often (candidates for SRP problems)
git log --name-only --pretty=format: | sort | uniq -c | sort -rn | head -20
```

- **Files that change very often** are hotspots: they likely mix responsibilities.
- **Files that always change together** are coupled, even if the code doesn't show it.
- **Files changed in many bug-fix commits** are fragile.

### Complexity and coverage

Large methods, deep nesting and untested code concentrate risk. A hotspot with low coverage is the best place to start improving.

## Runtime problems with design causes

### Hidden dependencies

Symptom: behavior differs by environment or order, with no code difference.

Causes: static access, global singletons, `System.getenv`, `LocalDateTime.now()`, thread-locals, classpath scanning.

Debugging: search for static calls and `new` inside business classes; replace with injected dependencies ([testable design](./01-testable-design.md)).

### Shared mutable state

Symptom: intermittent wrong values, especially under concurrency or between requests.

```java
@Service
public class PriceCalculator {
    private double lastDiscount;                       // shared by every request (singleton scope)

    public double price(Order o) {
        lastDiscount = discountFor(o);
        return o.subtotal() - lastDiscount;             // another thread may overwrite lastDiscount
    }
}
```

Fix: keep per-request data in local variables or immutable objects, not fields of shared objects.

### Temporal coupling

Symptom: methods must be called in a certain order or the object breaks.

```java
var exporter = new Exporter();
exporter.setTarget(file);
exporter.open();          // forgetting open() causes NullPointerException later
exporter.write(data);
```

Fix: make invalid states unrepresentable. Require what's needed at construction time, or use a [Builder](../04-creational-patterns/04-builder.md) or typed stages.

```java
try (Exporter exporter = Exporter.open(file)) {
    exporter.write(data);
}
```

### Proxy and wiring surprises

| Symptom | Cause |
|---|---|
| Annotation (`@Transactional`, `@Cacheable`) has no effect | Self-invocation bypasses the [proxy](../05-structural-patterns/07-proxy.md) |
| "No qualifying bean" or ambiguity | DI configuration issue ([dependency injection](../03-patterns-intro/02-dependency-injection.md)) |
| `ClassCastException` after injection | The injected object is a proxy of the interface, not of the concrete class |
| Circular dependency at startup | Two components depend on each other: redesign |
| Behavior differs between tests and production | Different wiring or decorators in each |

### Event and async flows

When behavior is spread across listeners ([Observer](../06-behavioral-patterns/02-observer.md), [events](../08-architecture/05-event-driven.md)), no single stack trace tells the story. Use **correlation IDs** in logs, record event IDs, and write down the main flows.

### Leaky abstractions

Symptom: swapping an implementation breaks callers. The abstraction exposes details (specific exceptions, ordering, performance) that callers came to rely on. Fix the contract, and test every implementation against the same checks ([contract tests](./02-test-doubles.md)).

## Making legacy code safe to change

Code without tests and with tangled structure is the hardest case. A standard approach:

### 1. Characterization tests

Capture what the code **actually does today**, whether or not it's correct.

```java
@Test
void characterizeInvoiceTotal() {
    var invoice = legacyBuildInvoice(sampleOrder());
    // Run it once, then lock in what it returns, so later changes that alter results are noticed
    assertEquals(new BigDecimal("118.40"), invoice.total());
}
```

### 2. Find or create a seam

A seam is a place to alter behavior without editing the code under test. Typical moves:

- Extract the hard-to-test call (database, clock) into a method, then override it in a test subclass.
- Introduce an interface around a static or concrete dependency.
- Pass collaborators in through a new constructor, keeping the old one for existing callers.

### 3. Break dependencies minimally

Make the smallest change that lets you get the code under test. Don't redesign yet.

### 4. Refactor under the safety net

Now apply [refactoring to patterns](./03-refactoring-to-patterns.md) in small, tested steps.

### 5. Strangle gradually (for large pieces)

Build the new structure beside the old one and route behavior to it piece by piece (a [Facade](../05-structural-patterns/05-facade.md) or [Adapter](../05-structural-patterns/01-adapter.md) at the boundary), retiring the old code over time. This is safer than a big rewrite.

## Preventing regressions in structure

Once fixed, make the rule enforceable:

| Problem fixed | Guard |
|---|---|
| Layer or module boundary violated | Architecture test (ArchUnit) |
| Dependency cycle | `beFreeOfCycles()` rule |
| Rule duplicated | A single named location (for example a [Specification](../07-application-patterns/03-specification.md)) with tests |
| Hidden time or randomness | Ban `LocalDateTime.now()` outside the clock provider, using a static-analysis rule |
| Flaky concurrency | A dedicated test, plus immutability by design |

## Checklist for a suspicious design

- Can I name this class's single purpose without "and"?
- Who can modify this state? Can I reduce it to one place?
- Is any rule duplicated?
- Does it depend on something it doesn't declare (static, global, environment)?
- Can I test it without infrastructure?
- Do changes here ripple elsewhere? Why?
- Are the dependency arrows pointing toward stable, abstract things?
- Would a new teammate find the flow, or do they need to trace through five wrappers?

## Common mistakes

- **Patching the symptom** and moving on, so the underlying cause strikes again.
- **Blaming the last person to touch the code** instead of the structure that allowed the mistake.
- **Rewriting from scratch** without characterization tests, and reproducing old bugs while introducing new ones.
- **Debugging only with a debugger**, missing patterns that show up in change history or dependency graphs.
- **Fixing a design problem without a guard**, so it regresses silently.
- **Redesigning everything at once** instead of improving the area you're touching.
- **Ignoring concurrency and state** until production.
- **Skipping logging context** (IDs) so flows can't be traced later.

## Quick Summary

- Structural problems show up as ripple effects, recurring bugs, order or environment dependence, and untestable code.
- Investigate *why* the structure allowed the bug, then fix the cause and add a guard.
- Use dependency analysis, change history and architecture tests to find problems objectively.
- For legacy code: characterization tests, then seams, then small refactorings.
- Watch for hidden dependencies, shared state, temporal coupling and proxy/wiring surprises.

## Related

[Testable design](./01-testable-design.md) · [Refactoring to patterns](./03-refactoring-to-patterns.md) · [Coupling and cohesion](../01-foundations/03-coupling-and-cohesion.md) · [Avoiding overengineering](../10-production/05-avoiding-overengineering.md)

## Next

Production concerns: [../10-production/00-overview.md](../10-production/00-overview.md)
