# Command

**Intent:** encapsulate a request as an object, so you can parameterize clients with different requests, queue or log requests, and support undoable operations.

Instead of calling a method *now*, you create an object describing *what to do*, and hand it to something that decides when, where and whether to run it.

## The problem

An editor has buttons, menu items and keyboard shortcuts that trigger the same actions (copy, paste, bold). You also want undo/redo and macros. If each UI element calls the business logic directly, the logic gets duplicated, and undo is impossible because nothing records what was done.

## Structure

```text
Invoker ──► Command (interface)         Receiver
 (button,     + execute()                (does the real work)
  queue,      + undo()                       ▲
  history)         ▲                         │
            ┌──────┴───────┐                 │
        InsertText      DeleteText ──────────┘
        (holds receiver + parameters)
```

- **Command**: interface with `execute()` (and optionally `undo()`).
- **Concrete command**: holds the receiver and the data needed to act.
- **Receiver**: the object that does the real work.
- **Invoker**: triggers commands, and may keep history.

## Example: text editor with undo

```java
public class Document {
    private final StringBuilder text = new StringBuilder();

    public void insert(int pos, String s) { text.insert(pos, s); }
    public void delete(int pos, int len)  { text.delete(pos, pos + len); }
    public String text() { return text.toString(); }
}

public interface Command {
    void execute();
    void undo();
}

public class InsertCommand implements Command {
    private final Document doc;
    private final int pos;
    private final String text;

    public InsertCommand(Document doc, int pos, String text) {
        this.doc = doc; this.pos = pos; this.text = text;
    }

    public void execute() { doc.insert(pos, text); }
    public void undo()    { doc.delete(pos, text.length()); }
}

public class DeleteCommand implements Command {
    private final Document doc;
    private final int pos, len;
    private String removed;                       // saved so undo can restore it

    public DeleteCommand(Document doc, int pos, int len) {
        this.doc = doc; this.pos = pos; this.len = len;
    }

    public void execute() {
        removed = doc.text().substring(pos, pos + len);
        doc.delete(pos, len);
    }
    public void undo() { doc.insert(pos, removed); }
}

public class Editor {                              // invoker with history
    private final Deque<Command> undoStack = new ArrayDeque<>();
    private final Deque<Command> redoStack = new ArrayDeque<>();

    public void run(Command c) {
        c.execute();
        undoStack.push(c);
        redoStack.clear();                         // a new action invalidates redo
    }

    public void undo() {
        if (undoStack.isEmpty()) return;
        Command c = undoStack.pop();
        c.undo();
        redoStack.push(c);
    }

    public void redo() {
        if (redoStack.isEmpty()) return;
        Command c = redoStack.pop();
        c.execute();
        undoStack.push(c);
    }
}
```

```java
var doc = new Document();
var editor = new Editor();
editor.run(new InsertCommand(doc, 0, "Hello"));
editor.run(new InsertCommand(doc, 5, " world"));
editor.undo();        // text is "Hello"
editor.redo();        // text is "Hello world"
```

## What it enables

| Capability | How |
|---|---|
| Undo / redo | Store executed commands; call `undo()` |
| Queueing and scheduling | Put commands in a queue or executor |
| Logging and replay | Persist commands; replay them after a crash |
| Macros | A `MacroCommand` holding a list of commands (a [Composite](../05-structural-patterns/03-composite.md)) |
| Decoupling UI from logic | Buttons trigger commands, not business methods |
| Retries and transactions | Re-run or roll back commands |

```java
public class MacroCommand implements Command {
    private final List<Command> commands;
    public MacroCommand(List<Command> commands) { this.commands = List.copyOf(commands); }

    public void execute() { commands.forEach(Command::execute); }
    public void undo() {
        for (int i = commands.size() - 1; i >= 0; i--) commands.get(i).undo();   // reverse order
    }
}
```

## Lightweight form

If you only need "something to run later", with no undo, Java already has command objects:

```java
Runnable task = () -> emailService.send(message);
executor.submit(task);
```

`Runnable` and `Callable` are essentially commands. A full Command class is worth it when you need undo, serialization, inspection or logging of the request.

## Command vs similar patterns

| Pattern | Difference |
|---|---|
| [Strategy](./01-strategy.md) | Strategy = *how* to do a fixed task; Command = a *specific request*, with its data, to run now or later |
| [Memento](./09-memento.md) | Undo by restoring a snapshot, whereas commands undo by reversing the operation |
| [Chain of Responsibility](./06-chain-of-responsibility.md) | Commands can be passed along a chain of handlers |

## Common mistakes

- **Undo that doesn't fully reverse** the operation (for example, not storing the deleted text).
- **Commands holding stale state** after other commands changed the document. Each command must be correct given the state at its time of execution.
- **Undoing macros in the wrong order.** Reverse them.
- **Unbounded history**, which leaks memory. Cap the undo stack.
- **Commands with side effects that can't be reversed** (sending an email), yet marked as undoable.
- **Making every method call a Command** when a direct call works.

## Quick Summary

- Command turns a request into an object that can be stored, queued, logged and undone.
- An invoker triggers it; a receiver does the work.
- Key uses: undo/redo, task queues, macros, replay.
- `Runnable` or a lambda is enough when you don't need undo or inspection.

## Related

[Memento](./09-memento.md) · [Strategy](./01-strategy.md) · [Composite](../05-structural-patterns/03-composite.md)

## Next

[04-state.md](./04-state.md)
