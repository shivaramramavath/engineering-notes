# State

**Intent:** allow an object to alter its behavior when its internal state changes. The object appears to change its class.

Instead of one class full of `if (state == X)` checks, each state becomes its own class, and the object delegates to its current state.

## The problem

A document goes through a workflow: draft, in review, published. Actions like `publish()` and `edit()` mean different things (or are forbidden) depending on the state.

```java
public class Document {
    private String state = "DRAFT";

    public void publish() {
        if (state.equals("DRAFT")) throw new IllegalStateException("Needs review first");
        else if (state.equals("REVIEW")) state = "PUBLISHED";
        else if (state.equals("PUBLISHED")) { /* already published */ }
    }

    public void edit(String text) {
        if (state.equals("PUBLISHED")) throw new IllegalStateException("Read-only");
        // ...
    }
    // every method repeats the same state checks
}
```

Each new state touches every method, and transition rules are scattered.

## Structure

```text
Context (Document) ──► State (interface)
  + request()             + handle(context)
  holds current state           ▲
                      ┌─────────┼──────────┐
                 DraftState  ReviewState  PublishedState
                 (each knows its behavior and which state comes next)
```

## Example

```java
public interface DocumentState {
    void edit(Document doc, String text);
    void submitForReview(Document doc);
    void publish(Document doc);
}

public class Document {
    private DocumentState state = new DraftState();
    private String content = "";

    void setState(DocumentState s) { this.state = s; }
    void setContent(String c) { this.content = c; }

    public void edit(String text)   { state.edit(this, text); }
    public void submitForReview()   { state.submitForReview(this); }
    public void publish()           { state.publish(this); }
}

class DraftState implements DocumentState {
    public void edit(Document doc, String text) { doc.setContent(text); }
    public void submitForReview(Document doc)   { doc.setState(new ReviewState()); }
    public void publish(Document doc)           { throw new IllegalStateException("Review first"); }
}

class ReviewState implements DocumentState {
    public void edit(Document doc, String text) { throw new IllegalStateException("Under review"); }
    public void submitForReview(Document doc)   { /* already in review, ignore */ }
    public void publish(Document doc)           { doc.setState(new PublishedState()); }
}

class PublishedState implements DocumentState {
    public void edit(Document doc, String text) { throw new IllegalStateException("Read-only"); }
    public void submitForReview(Document doc)   { throw new IllegalStateException("Already published"); }
    public void publish(Document doc)           { /* no-op */ }
}
```

The workflow rules now live in the state classes: each class answers "what can happen here and what comes next". `Document` has no `if` on state.

## A lighter alternative: enum state

For simple state machines with little per-state behavior, an enum can be enough:

```java
enum Status {
    DRAFT { Status next() { return REVIEW; } },
    REVIEW { Status next() { return PUBLISHED; } },
    PUBLISHED { Status next() { return this; } };

    abstract Status next();
}
```

Enums with abstract methods give you polymorphism without separate files. Use full State classes when each state has substantial behavior or its own data.

## State vs Strategy

Their structures are nearly identical. The difference is intent:

| | State | [Strategy](./01-strategy.md) |
|---|---|---|
| Who changes it | The object itself, via transitions | The client |
| Awareness | States often know about other states | Strategies are independent |
| Purpose | Behavior depends on lifecycle | Choose an algorithm |
| Typical number active | One current state that evolves | One chosen algorithm |

## Where transitions should live

| Option | Trade-off |
|---|---|
| In the state classes (as above) | Distributed, easy to extend per state; harder to see the whole workflow |
| In the context | Central view of all transitions; context grows and knows all states |
| A transition table | Compact and declarative for simple machines; less flexible for rich behavior |

Whichever you choose, document the diagram:

```text
DRAFT ──submit──► REVIEW ──publish──► PUBLISHED
  ▲                  │
  └────reject────────┘
```

## When to use it

- Behavior varies significantly with state, and the object has many state-dependent methods.
- A `switch` or `if` chain on a state field is duplicated across methods.
- The state machine is likely to grow with new states or transitions.

Examples: order lifecycle, document workflow, connection (connecting, open, closed), vending machine, media player, TCP connection.

Skip it when there are two or three states with trivial differences. A simple enum and a couple of conditions are clearer.

## Common mistakes

- **Allowing invalid transitions** because states don't check what's legal. Make illegal actions explicit.
- **States sharing mutable data** unsafely. Keep shared data in the context.
- **Creating a new state object on every transition** when states are stateless. Reuse single instances.
- **State explosion** from combining independent concerns. Split them into separate state machines.
- **Persisting state as an object**; store an identifier (enum or name) and rebuild the object when loading.
- **Hiding the overall workflow** so nobody can see the valid paths. Keep a diagram or test of the transitions.

## Quick Summary

- State replaces state-based conditionals with one class per state.
- The context delegates to its current state, and states trigger transitions.
- Same shape as Strategy, different intent: self-changing behavior vs chosen algorithm.
- Use enums for small machines, and State classes for rich per-state behavior.

## Related

[Strategy](./01-strategy.md) · [Command](./03-command.md) · [Memento](./09-memento.md)

## Next

[05-template-method.md](./05-template-method.md)
