# State Pattern in Modern Java

> Modeling state-dependent behavior and Finite State Machines: State transitions, Context delegation, Sealed Interfaces with Pattern Matching, and Order Lifecycle modeling.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core Design Pattern / Behavioral |
| **Prerequisites** | [00_solid-principles](../23-design-and-clean-code/00_solid-principles.md), [12_sealed-classes](../../12-modern-java/01_sealed-classes.md) |
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

### What is the State Pattern?
The State pattern is a behavioral design pattern that allows an object to **alter its behavior when its internal state changes**. The object appears to change its class at runtime.

In enterprise software engineering:
- E-Commerce Order Lifecycles: `Created -> Paid -> Shipped -> Delivered -> Cancelled`.
- Document Workflows: `Draft -> InReview -> Approved -> Published`.
- TCP Connection States: `LISTEN -> SYN_SENT -> ESTABLISHED -> CLOSE_WAIT`.
- Vending Machines and ATM State Machines.

### Why State Replaces Giant Conditionals
Without the State pattern, methods are polluted with massive nested `switch` or `if (status == ORDER_PAID)` checks:
- Adding a new state requires editing every single method across the class.
- Invalid state transitions (e.g. paying for an already cancelled order) are easy to miss.
- State encapsulates each state\'s behavior and allowable transitions into dedicated classes.

---

## 2. Core Concepts

### Context, State Interface, and Concrete States
1. **Context**: The class that maintains a current state reference (`Order`).
2. **State Interface**: Declares state-specific operations (`pay()`, `ship()`, `cancel()`).
3. **Concrete States**: Implement behavior for that specific state and transition the context to the next state (`PaidState`, `ShippedState`).

### Modern Evolution: Sealed Hierarchies & Pattern Matching
In modern Java (Java 17–25), states can be modeled as **Sealed Interfaces** holding immutable records, with transitions governed by exhaustive pattern matching switch expressions.

---

## 3. How It Works Internally

### State Transition Diagram
```
[ CreatedState ] --- pay() ---> [ PaidState ] --- ship() ---> [ ShippedState ]
       |                               |
    cancel()                        cancel()
       |                               |
       +------------> [ CancelledState ] <-----+
```

When a method is called on the Context, it delegates directly to `currentState.action(this)`. The state object validates whether the action is permitted in that state, executes business logic, and mutates `context.setState(newState)`.

---

## 4. Syntax & Usage

### 1. Classic State Interface and Context
```java
public interface OrderState {
    void pay(OrderContext context);
    void ship(OrderContext context);
    void cancel(OrderContext context);
}

public class OrderContext {
    private OrderState state;

    public OrderContext(OrderState initial) { this.state = initial; }
    public void setState(OrderState state) { this.state = state; }

    public void pay() { state.pay(this); }
    public void ship() { state.ship(this); }
    public void cancel() { state.cancel(this); }
}
```

---

## 5. Practical Examples

### Example 1: Robust E-Commerce Order Lifecycle State Machine
**Goal:** Prevent illegal state transitions (e.g. shipping an unpaid order).

```java
// State Interface
public interface OrderState {
    void pay(Order order);
    void ship(Order order);
    void cancel(Order order);
}

// Concrete State 1: New / Created
public class CreatedState implements OrderState {
    @Override
    public void pay(Order order) {
        System.out.println("Payment accepted. Transitioning to PAID.");
        order.setState(new PaidState());
    }

    @Override
    public void ship(Order order) {
        throw new IllegalStateException("Cannot ship an unpaid order!");
    }

    @Override
    public void cancel(Order order) {
        System.out.println("Order cancelled.");
        order.setState(new CancelledState());
    }
}

// Concrete State 2: Paid
public class PaidState implements OrderState {
    @Override
    public void pay(Order order) {
        throw new IllegalStateException("Order is already paid!");
    }

    @Override
    public void ship(Order order) {
        System.out.println("Order dispatched. Transitioning to SHIPPED.");
        order.setState(new ShippedState());
    }

    @Override
    public void cancel(Order order) {
        System.out.println("Refunding payment and cancelling order.");
        order.setState(new CancelledState());
    }
}

// Concrete State 3: Shipped (Terminal)
public class ShippedState implements OrderState {
    @Override public void pay(Order order) { throw new IllegalStateException("Already shipped!"); }
    @Override public void ship(Order order) { throw new IllegalStateException("Already shipped!"); }
    @Override public void cancel(Order order) { throw new IllegalStateException("Cannot cancel an order that has already shipped!"); }
}

// Concrete State 4: Cancelled (Terminal)
public class CancelledState implements OrderState {
    @Override public void pay(Order order) { throw new IllegalStateException("Order is cancelled!"); }
    @Override public void ship(Order order) { throw new IllegalStateException("Order is cancelled!"); }
    @Override public void cancel(Order order) { throw new IllegalStateException("Already cancelled!"); }
}

// The Context Class
public class Order {
    private OrderState state = new CreatedState();

    public void setState(OrderState state) { this.state = state; }
    public void pay() { state.pay(this); }
    public void ship() { state.ship(this); }
    public void cancel() { state.cancel(this); }
}
```

### Example 2: Modern Sealed Interface Functional State Machine
**Goal:** Pure functional state transitions using Java 21 sealed types and pattern matching.

```java
public sealed interface OrderStatus permits Draft, Paid, Shipped, Cancelled {}

public record Draft(String orderId) implements OrderStatus {}
public record Paid(String orderId, String transactionId) implements OrderStatus {}
public record Shipped(String orderId, String trackingNumber) implements OrderStatus {}
public record Cancelled(String orderId, String reason) implements OrderStatus {}

public class FunctionalStateMachine {
    public static OrderStatus pay(OrderStatus current, String txId) {
        return switch (current) {
            case Draft d -> new Paid(d.orderId(), txId);
            case Paid p -> throw new IllegalStateException("Already paid");
            case Shipped s -> throw new IllegalStateException("Already shipped");
            case Cancelled c -> throw new IllegalStateException("Cannot pay cancelled order");
        };
    }
}
```

---

### Example 3: State Machine Persistence with Enums in Databases
**Goal:** Map state machine states directly to database columns using clean String conversions.

```java
public class StatePersistenceBridge {
    public static String toDatabaseColumn(OrderState state) {
        return state.getClass().getSimpleName().replace("State", "").toUpperCase();
    }
}

### Example 4: Spring StateMachine Framework Event Triggering
**Goal:** Understand commercial enterprise workflow event triggering in distributed environments.

```java
public class StateEventPublisher {
    public enum Events { PAY, SHIP, CANCEL }
    public void publishEvent(String orderId, Events event) {
        System.out.printf("Publishing event %s for order %s%n", event, orderId);
    }
}

## 6. Best Practices

1. **Throw `IllegalStateException` on Disallowed Transitions**: Enforce business rules loudly; never allow invalid transitions to pass silently.
2. **Make Stateless States Singletons**: If a concrete state contains no mutable fields, use a shared `public static final` constant to avoid object churn.
3. **Keep Context Clean**: The Context should only hold the state reference and forward calls.
4. **Consider Sealed Hierarchies for Pure Models**: Use sealed interfaces and record patterns when building immutable, functional event-driven systems.
5. **Decide Transition Ownership**: Either Context manages transitions, or State objects manage transitions. Be consistent across the codebase.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Secure Fix |
| :--- | :--- | :--- |
| **Context and States circularly dependent with leaks** | Context passes `this` to states, preventing clean lifecycle garbage collection. | Make state instances stateless singletons. |
| **Silently ignoring illegal actions** | Calling `ship()` on an unpaid order does nothing, confusing caller. | Throw `IllegalStateException`. |
| **State explosion for minor variables** | Creating 50 state classes for minor orthogonal flags. | Use State pattern for major lifecycle transitions only. |

---

## 8. Performance & Security Notes

### Zero Allocation with Flyweight State Instances
If states contain no instance fields, they are stateless.
- Instantiate each state once as `static final STATE_A = new StateA()`.
- State transitions simply update the Context reference pointer (`this.state = STATE_B`), creating zero garbage collector pressure.

---

## 9. Comparison / When to Use What

| Pattern | Focus | Transition Driver |
| :--- | :--- | :--- |
| **State** | Behavior changes based on internal lifecycle | States or Context internally |
| **Strategy** | Swaps how a single algorithm executes | Client explicitly sets strategy |
| **Command** | Encapsulates single action | Client executes command |

---

## 10. Interview Questions & Answers

### Q1: What is the primary difference between the State pattern and the Strategy pattern?
<details>
<summary>View Answer</summary>
While both patterns rely on composition and delegate work to an interface:
- **Strategy**: The client typically configures the strategy explicitly from the outside, and strategies are generally independent of each other with no awareness of alternative strategies.
- **State**: The context transitions between different concrete state objects automatically as operations occur. Concrete states frequently know about one another and initiate transitions (`context.setState(new PaidState())`).
</details>

### Q2: Who should be responsible for managing state transitions: the Context or the State classes?
<details>
<summary>View Answer</summary>
There are two architectural approaches:
1. **States manage transitions**: Flexible and decentralized. Each state knows which states can follow it. The downside is that states become coupled to each other.
2. **Context manages transitions**: Centralized. The context evaluates state return values or criteria and updates its own reference. The downside is that the context class can accumulate complex transition logic.
In practice, states managing transitions is preferred for small-to-medium FSMs, while centralized transition tables or Spring State Machine are used for large enterprise workflows.
</details>

### Q3: How can the Flyweight pattern be combined with the State pattern?
<details>
<summary>View Answer</summary>
If concrete state classes maintain no internal mutable instance variables (their behavior depends entirely on the context passed as an argument), each state can be implemented as a singleton constant (Flyweight). This eliminates object instantiation overhead during high-frequency state transitions.
</details>

### Q4: How do modern Java Sealed Interfaces improve State Machine implementation?
<details>
<summary>View Answer</summary>
Sealed interfaces (`sealed interface State permits Draft, Paid, Shipped`) restrict the permitted state types to an explicit, closed set. When processing states using a `switch` expression with pattern matching, the Java compiler enforces exhaustiveness at compile time, guaranteeing that no state transitions are left unhandled without requiring a dummy `default` branch.
</details>

### Q5: What is a Terminal State?
<details>
<summary>View Answer</summary>
A terminal state is a final state in a state machine from which no further transitions are permitted (e.g. `CancelledState` or `DeliveredState`). All mutating operations on a terminal state throw `IllegalStateException`.
</details>

---

## 11. Practice Exercises

### Exercise 1: Vending Machine State Pattern
**Task:** Implement states: `NoCoinState` and `HasCoinState` for a vending machine.
<details>
<summary>View Solution</summary>

```java
public interface VendingState {
    void insertCoin(VendingMachine m);
    void ejectCoin(VendingMachine m);
}

public class NoCoinState implements VendingState {
    @Override public void insertCoin(VendingMachine m) {
        System.out.println("Coin inserted");
        m.setState(new HasCoinState());
    }
    @Override public void ejectCoin(VendingMachine m) {
        System.out.println("No coin to eject");
    }
}

public class HasCoinState implements VendingState {
    @Override public void insertCoin(VendingMachine m) {
        System.out.println("Coin already present");
    }
    @Override public void ejectCoin(VendingMachine m) {
        System.out.println("Coin returned");
        m.setState(new NoCoinState());
    }
}

public class VendingMachine {
    private VendingState state = new NoCoinState();
    public void setState(VendingState s) { this.state = s; }
    public void insertCoin() { state.insertCoin(this); }
    public void ejectCoin() { state.ejectCoin(this); }
}
```
</details>

### Exercise 2: Pure Functional State Transition
**Task:** Model an audio player state machine (`Stopped`, `Playing`, `Paused`) using a sealed interface.
<details>
<summary>View Solution</summary>

```java
public sealed interface PlayerState permits Stopped, Playing, Paused {}
public record Stopped() implements PlayerState {}
public record Playing() implements PlayerState {}
public record Paused() implements PlayerState {}

public class AudioController {
    public static PlayerState pressPlay(PlayerState cur) {
        return switch (cur) {
            case Stopped s -> new Playing();
            case Paused p -> new Playing();
            case Playing p -> new Paused();
        };
    }
}
```
</details>

### Exercise 3: Document Review Workflow
**Task:** Build a `Document` with states `Draft` and `Published`, allowing edit only in Draft.
<details>
<summary>View Solution</summary>

```java
public interface DocState { void edit(Doc d, String text); }
public class DraftState implements DocState {
    @Override public void edit(Doc d, String t) { d.text = t; }
}
public class PublishedState implements DocState {
    @Override public void edit(Doc d, String t) { throw new IllegalStateException("Cannot edit published document!"); }
}

public class Doc {
    public String text = "";
    private DocState state = new DraftState();
    public void setState(DocState s) { this.state = s; }
    public void edit(String t) { state.edit(this, t); }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Core Goal**: Object changes its behavior when internal state changes.
- **Encapsulation**: Removes massive `switch` and `if-else` blocks across methods.
- **Fail-Fast**: Throw `IllegalStateException` on forbidden state transitions.
- **Stateless States**: Share state instances as constants if they hold no state fields.
- **Modern Java**: Use Sealed Interfaces and Pattern Matching for pure functional state machines.
- **Transitions**: Can be initiated by concrete states or coordinated by Context.

---

## 13. Further Reading
- GoF Design Patterns: [State Pattern](https://www.oreilly.com/library/view/design-patterns-elements/0201633612/)
- Refactoring Guru: [State Design Pattern](https://refactoring.guru/design-patterns/state)
- Head First Design Patterns: [The State Pattern](https://www.oreilly.com/library/view/head-first-design/9781492077992/)
