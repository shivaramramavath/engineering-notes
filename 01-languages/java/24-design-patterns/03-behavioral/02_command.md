# Command Pattern in Modern Java

> Encapsulating requests as standalone objects: undo/redo stacks, macro recording, queue-based transaction execution, and lambda-based commands with `Runnable`.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core Design Pattern / Behavioral |
| **Prerequisites** | [00_solid-principles](../23-design-and-clean-code/00_solid-principles.md), [08_collections](../../08-collections/README.md) |
| **Time to Read** | 35 minutes |
| **Status** | Verified Production Guide |
| **Java Baseline** | Java 17 LTS / Java 21 LTS / Java 25 LTS |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance & Security Notes](#8-performance--security-notes)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions & Answers](#10-interview-questions--answers)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### What is the Command Pattern?
The Command pattern is a behavioral design pattern that turns a **request or action into a standalone object containing all information about the request**.

By encapsulating an operation as an object:
- You can parameterize clients with different requests.
- You can queue, schedule, or delay execution across threads or background workers.
- You can support **reversible operations (Undo / Redo)** by maintaining an execution history stack.
- You can log commands to disk for crash recovery and audit trails (Command Query Responsibility Segregation - CQRS).

### Why the Command Pattern is Ubiquitous
In graphical user interfaces (GUIs), text editors, and transaction managers:
- The UI button (`Invoker`) should not know how to cut text, save files, or charge cards.
- The button merely holds a `Command` object and invokes `command.execute()`.
- The concrete command holds the receiver (`Document`, `Account`) and coordinates the operation.

---

## 2. Core Concepts

### Command, Receiver, Invoker, and Client
1. **Command Interface**: Declares execution method (`execute()`) and optional reversal (`undo()`).
2. **Receiver**: The business object that performs the actual domain work (`TextEditor`, `BankAccount`).
3. **Concrete Command**: Binds a receiver to an action and holds parameter state (`InsertTextCommand`).
4. **Invoker**: Triggers the command without knowing the receiver (`MenuButton`, `JobScheduler`).
5. **Client**: Instantiates receiver and command objects, and assigns them to the invoker.

### Modern Evolution: Lambdas as Commands
In Java 8+, any command without an `undo()` method is identical to `java.lang.Runnable` or `java.util.function.Consumer`. You can pass method references directly (`editor::save`) instead of creating full command classes.

---

## 3. How It Works Internally

### Reversible Execution Stack (Undo / Redo)
```
[ Client calls: invoker.execute(new WriteCommand("Hello")) ]
       |
       v
[ Invoker ]
  1. command.execute() ---> mutates Receiver state
  2. historyStack.push(command)
       |
       v
[ User presses Ctrl+Z (Undo) ]
  1. Command lastCmd = historyStack.pop();
  2. lastCmd.undo(); ---> restores Receiver previous state!
```

---

## 4. Syntax & Usage

### 1. Reversible Command Interface
```java
public interface Command {
    void execute();
    void undo();
}
```

---

## 5. Practical Examples

### Example 1: Text Editor Undo/Redo Engine
**Goal:** Implement a fully reversible text editor with undo and redo history stacks.

```java
import java.util.ArrayDeque;
import java.util.Deque;

// Receiver
public class TextDocument {
    private final StringBuilder content = new StringBuilder();

    public void insert(int offset, String text) { content.insert(offset, text); }
    public void delete(int offset, int length) { content.delete(offset, offset + length); }
    public String getText() { return content.toString(); }
}

// Reversible Command Interface
public interface DocumentCommand {
    void execute();
    void undo();
}

// Concrete Command: Insert
public class InsertTextCommand implements DocumentCommand {
    private final TextDocument doc;
    private final String text;
    private final int offset;

    public InsertTextCommand(TextDocument doc, int offset, String text) {
        this.doc = doc;
        this.offset = offset;
        this.text = text;
    }

    @Override
    public void execute() {
        doc.insert(offset, text);
    }

    @Override
    public void undo() {
        doc.delete(offset, text.length());
    }
}

// Invoker: Command Manager with Undo/Redo Stacks
public class CommandManager {
    private final Deque<DocumentCommand> undoStack = new ArrayDeque<>();
    private final Deque<DocumentCommand> redoStack = new ArrayDeque<>();

    public void executeCommand(DocumentCommand cmd) {
        cmd.execute();
        undoStack.push(cmd);
        redoStack.clear(); // Clear redo on new action
    }

    public void undo() {
        if (!undoStack.isEmpty()) {
            DocumentCommand cmd = undoStack.pop();
            cmd.undo();
            redoStack.push(cmd);
        }
    }

    public void redo() {
        if (!redoStack.isEmpty()) {
            DocumentCommand cmd = redoStack.pop();
            cmd.execute();
            undoStack.push(cmd);
        }
    }
}
```

### Example 2: Thread Pool Task Queueing (Commands as Runnables)
```java
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class TaskScheduler {
    private static final ExecutorService POOL = Executors.newVirtualThreadPerTaskExecutor();

    public static void schedule(Runnable command) {
        // Runnable is the ultimate Java Command pattern!
        POOL.submit(command);
    }
}
```

---

### Example 3: Command Queue Executor with Priority Execution
**Goal:** Enqueue transaction commands into a prioritized bounded work queue.

```java
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.PriorityBlockingQueue;

public record PrioritizedCommand(Runnable action, int priority) implements Comparable<PrioritizedCommand> {
    @Override
    public int compareTo(PrioritizedCommand other) {
        return Integer.compare(other.priority, this.priority); // High priority first
    }
}

public class PriorityCommandDispatcher {
    private final BlockingQueue<PrioritizedCommand> queue = new PriorityBlockingQueue<>();

    public void submit(Runnable action, int priority) {
        queue.offer(new PrioritizedCommand(action, priority));
    }

    public void processNext() throws InterruptedException {
        PrioritizedCommand cmd = queue.take();
        cmd.action().run();
    }
}
```

## 6. Best Practices

1. **Keep Commands Self-Contained**: Commands must capture all parameter state necessary to execute and undo at instantiation.
2. **Use `Runnable` for Irreversible Commands**: If an operation does not support `undo()`, use `Runnable` or `Consumer` rather than a custom interface.
3. **Bound the History Stack**: Prevent OutOfMemoryErrors by limiting undo stack size (e.g. maximum 50 steps using an `ArrayDeque`).
4. **Decouple Invokers Completely**: Invokers should only depend on the Command interface; they must have zero direct knowledge of receivers.
5. **Serialize Commands for Audit / Event Sourcing**: Serializing commands allows building replayable audit logs and CQRS systems.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Secure Fix |
| :--- | :--- | :--- |
| **Unbounded Undo Stacks** | Storing thousands of commands causes heap exhaustion. | Bound stack depth (e.g. max 50). |
| **Invoker executing receiver directly** | Tight coupling; defeats parameterization and undo. | Route all actions through Command interface. |
| **State mutation during `undo()` that breaks Redo** | `undo()` logic introduces new side effects. | `undo()` must be the exact inverse of `execute()`. |
| **Commands depending on external mutable state** | External changes between creation and execution cause incorrect runs. | Snapshot all parameters in command constructor. |

---

## 8. Performance & Security Notes

### Memory Footprint of Command Objects
Command objects are typically lightweight instances allocated on the heap.
- In high-throughput event sourcing ($> 100,000\text{ cmds/sec}$), avoid heavy fields in commands.
- Use compact immutable Java records for command event storage.

---

## 9. Comparison / When to Use What

| Pattern | Primary Intent | Reversibility |
| :--- | :--- | :--- |
| **Command** | Encapsulates request into an object | Full (via `undo()`) |
| **Strategy** | Encapsulates algorithm variant | None (replaces logic) |
| **Memento** | Captures snapshot of internal state | Restores state snapshot |
| **Chain of Responsibility** | Passes request along a handler chain | Sequential execution |

---

## 10. Interview Questions & Answers

### Q1: What are the key components of the Command pattern?
<details>
<summary>View Answer</summary>
1. **Command Interface**: Declares `execute()` and `undo()`.
2. **Concrete Command**: Encapsulates specific parameters and receiver delegation.
3. **Receiver**: The domain entity containing business logic.
4. **Invoker**: Triggers command execution and manages undo/redo history.
5. **Client**: Assembles commands and assigns them to invokers.
</details>

### Q2: How does the Command pattern implement Undo and Redo?
<details>
<summary>View Answer</summary>
The invoker maintains two stacks: an `undoStack` and a `redoStack`. When a command executes, it is pushed onto the `undoStack`. When the user requests Undo, the top command is popped from `undoStack`, its `undo()` method is executed, and it is pushed onto `redoStack`. If Redo is requested, the top command is popped from `redoStack`, its `execute()` method is re-run, and it is pushed back onto `undoStack`.
</details>

### Q3: What is the relationship between the Command pattern and CQRS (Command Query Responsibility Segregation)?
<details>
<summary>View Answer</summary>
In CQRS architectures, the Command pattern models all state-mutating requests (e.g. `CreateOrderCommand`, `TransferFundsCommand`). Commands are validated and dispatched to command handlers, mutating domain state and generating domain events, while queries are handled completely independently through dedicated read-only data models.
</details>

### Q4: Give an example of the Command pattern in the Java Standard Library.
<details>
<summary>View Answer</summary>
1. `java.lang.Runnable`: The quintessential Command interface (`run()` method).
2. `java.util.concurrent.Callable`: Command with a return value and checked exceptions (`call()` method).
3. `javax.swing.Action`: Swing GUI command binding buttons to actions.
</details>

### Q5: How can Java Records be used with the Command pattern?
<details>
<summary>View Answer</summary>
In modern Java and CQRS, commands are often modeled as immutable records holding input parameters (e.g. `record DepositMoneyCommand(AccountId id, BigDecimal amount) implements Command`).
</details>

### Q6: What is a Macro Command?
<details>
<summary>View Answer</summary>
A Macro Command (or Composite Command) is a command that contains an internal list of other commands. When `execute()` is called on the macro, it executes all child commands sequentially. When `undo()` is called, it reverses all child commands in reverse order.
</details>

### Q7: What is the difference between Command and Memento?
<details>
<summary>View Answer</summary>
Command uses an algorithmic approach to undo (executing the inverse mathematical operation). Memento uses a state-snapshot approach to undo (saving a full snapshot of the object\'s internal state and restoring it).
</details>

### Q8: Why must the redo stack be cleared when a new command executes?
<details>
<summary>View Answer</summary>
Executing a brand-new action after undoing previous steps branches history. The previously undone commands in the redo stack are no longer valid continuations of the new state timeline.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Macro Command
**Task:** Create a `MacroCommand` that executes a list of commands sequentially and reverses them in reverse order.
<details>
<summary>View Solution</summary>

```java
import java.util.ArrayList;
import java.util.List;

public class MacroCommand implements DocumentCommand {
    private final List<DocumentCommand> commands = new ArrayList<>();

    public void add(DocumentCommand cmd) { commands.add(cmd); }

    @Override
    public void execute() {
        for (var cmd : commands) cmd.execute();
    }

    @Override
    public void undo() {
        // Reverse order
        for (int i = commands.size() - 1; i >= 0; i--) {
            commands.get(i).undo();
        }
    }
}
```
</details>

### Exercise 2: Light Switch Command Invoker
**Task:** Implement a simple remote control with On and Off slots.
<details>
<summary>View Solution</summary>

```java
public class RemoteControl {
    private Runnable onCommand = () -> {};
    private Runnable offCommand = () -> {};

    public void setCommands(Runnable on, Runnable off) {
        this.onCommand = on; this.offCommand = off;
    }

    public void pressOn() { onCommand.run(); }
    public void pressOff() { offCommand.run(); }
}
```
</details>

### Exercise 3: Bank Account Reversible Transaction
**Task:** Implement a `DepositCommand` with full `undo()` support.
<details>
<summary>View Solution</summary>

```java
public class Account {
    public int balance = 0;
    public void deposit(int n) { balance += n; }
    public void withdraw(int n) { balance -= n; }
}

public record DepositCommand(Account acc, int amount) {
    public void execute() { acc.deposit(amount); }
    public void undo() { acc.withdraw(amount); }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Core Goal**: Encapsulate an action and its parameters into an executable object.
- **Undo/Redo**: Maintain execution stacks using `ArrayDeque`.
- **Decoupling**: Invokers know only the Command interface, never the receiver.
- **Functional Idiom**: `Runnable` and `Callable` are native command interfaces.
- **Macro Command**: Composite commands execute multiple child actions sequentially.
- **CQRS**: Commands model state mutations in distributed domain architectures.

---

## 13. Further Reading
- GoF Design Patterns: [Command Pattern](https://www.oreilly.com/library/view/design-patterns-elements/0201633612/)
- Refactoring Guru: [Command Design Pattern](https://refactoring.guru/design-patterns/command)
- Martin Fowler: [CQRS Architectural Pattern](https://martinfowler.com/bliki/CQRS.html)
