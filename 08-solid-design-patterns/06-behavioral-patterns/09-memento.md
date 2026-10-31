# Memento

**Intent:** without violating encapsulation, capture and externalize an object's internal state so the object can be restored to that state later.

Use it for **undo**, **checkpoints**, **snapshots** and **rollback**, when you need to save state but don't want other classes to know or touch the object's internals.

## The problem

You want to save an editor's state so the user can undo. Two bad options:

1. **Expose the internal fields** so an outside class can copy them. This breaks encapsulation and ties that class to the editor's structure.
2. **Have the editor manage its own history**, which mixes two responsibilities in one class.

Memento separates these: the object creates an opaque snapshot, and someone else stores it.

## Structure

```text
Originator ── creates ──► Memento (snapshot, opaque to others)
    ▲                         │
    │ restore(memento)        │ stored by
    │                         ▼
    └──────────────────── Caretaker (holds mementos, never looks inside)
```

- **Originator**: the object whose state is saved. It creates mementos and restores from them.
- **Memento**: an immutable snapshot of the originator's state.
- **Caretaker**: keeps the mementos (a history stack), but never reads or modifies them.

## Example: editor with snapshots

```java
public class TextEditor {                                 // Originator
    private String text = "";
    private int cursor = 0;

    public void type(String s) {
        text = text.substring(0, cursor) + s + text.substring(cursor);
        cursor += s.length();
    }

    public void moveCursor(int pos) { this.cursor = Math.max(0, Math.min(pos, text.length())); }

    public String text() { return text; }

    public Snapshot save() { return new Snapshot(text, cursor); }

    public void restore(Snapshot s) {
        this.text = s.text;
        this.cursor = s.cursor;
    }

    // Nested so only TextEditor can read the fields; outsiders see an opaque type
    public static final class Snapshot {
        private final String text;
        private final int cursor;
        private Snapshot(String text, int cursor) { this.text = text; this.cursor = cursor; }
    }
}

public class History {                                    // Caretaker
    private final Deque<TextEditor.Snapshot> snapshots = new ArrayDeque<>();

    public void push(TextEditor.Snapshot s) { snapshots.push(s); }

    public TextEditor.Snapshot pop() {
        return snapshots.isEmpty() ? null : snapshots.pop();
    }
}
```

Usage:

```java
var editor = new TextEditor();
var history = new History();

editor.type("Hello");
history.push(editor.save());        // checkpoint
editor.type(" world");
editor.type("!!!");

editor.restore(history.pop());      // back to "Hello"
```

`History` stores snapshots but can't read their contents. The nested class's private fields remain accessible only to `TextEditor`. This is the encapsulation guarantee Java gives you through nesting.

## Simpler form with records

When strict hiding isn't required, an immutable record is a compact memento:

```java
public record EditorState(String text, int cursor) {}

EditorState snapshot = new EditorState(text, cursor);
```

It's simple and safe because it's immutable, though the state is visible to anyone who holds it. Use the nested-class version when concealing the state matters.

## Memento vs Command for undo

| | Memento | [Command](./03-command.md) |
|---|---|---|
| Undo by | Restoring a saved **state snapshot** | Running a **reverse operation** |
| Memory | Stores whole (or large parts of) state each time | Stores only what's needed to reverse the action |
| Best when | State is small, or reversing is hard | Operations are easily reversible |
| Combination | Commonly used together: a command saves a memento before executing, and restores it on undo | |

## Cost control

Snapshots can be expensive. Ways to manage this:

| Technique | Detail |
|---|---|
| **Limit history size** | Keep only the last N snapshots |
| **Incremental mementos** | Store only the difference from the previous state |
| **Immutable/persistent data structures** | Share unchanged parts between snapshots |
| **Snapshot at meaningful points** | Not on every keystroke |
| **Compress or serialize** | Store snapshots in a compact form |

## Where it's used

- Undo/redo in editors and drawing tools.
- Game save points and checkpoints.
- Transaction rollback within an application.
- "Draft" or "version" history of a document.
- Wizard-style forms that restore previous steps.

## Common mistakes

- **Mutable mementos**, which can change after they're saved, breaking restore. Make them immutable.
- **Shallow snapshots**: saving a reference to a mutable list, not a copy. Later changes silently alter "old" snapshots. Copy mutable state (see shallow vs deep in [Prototype](../04-creational-patterns/05-prototype.md)).
- **Caretaker reading the memento's contents**, which defeats encapsulation.
- **Unbounded history** causing memory growth.
- **Snapshotting objects that hold external resources** (connections, files), which can't be restored by copying fields.
- **Restoring partial state**, leaving the originator inconsistent. Capture all state that matters.

## Quick Summary

- Memento saves an object's state in an opaque snapshot, to be restored later.
- Originator creates and restores; caretaker stores but doesn't inspect.
- Make snapshots immutable and deep-copy mutable data.
- Pair it with Command for undo, and bound the history size.

## Related

[Command](./03-command.md) · [Prototype](../04-creational-patterns/05-prototype.md) · [State](./04-state.md)

## Next

[10-visitor.md](./10-visitor.md)
