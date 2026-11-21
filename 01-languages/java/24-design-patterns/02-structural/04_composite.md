# Composite Pattern in Modern Java

> Composing objects into tree hierarchies with uniform treatment: recursive tree structures, leaves vs. composites, filesystem representations, and UI component graphs.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core Design Pattern / Structural |
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

### What is the Composite Pattern?
The Composite pattern is a structural design pattern that allows you to compose objects into **tree structures to represent part-whole hierarchies**.

The critical advantage of Composite is that it enables client code to treat **individual objects (leaves)** and **compositions of objects (composite nodes)** uniformly through a shared interface.

Classic Real-World Applications:
- **Filesystem Hierarchies**: A `File` (leaf) and a `Directory` (composite containing files and sub-directories) both implement `FileSystemNode.getSize()`.
- **Graphical UI Trees**: Buttons, TextBoxes (leaves), and Panels (composites containing buttons and text boxes) all implement `UIComponent.render()`.
- **Organization Charts**: An `Employee` (leaf) and a `Department` (composite of employees and sub-departments) both calculate total salary budgets.

---

## 2. Core Concepts

### Component, Leaf, and Composite
1. **Component Interface**: Declares operations common to both simple and complex elements (`calculateTotal()`).
2. **Leaf**: The basic building block with no children. Executes the actual atomic operation.
3. **Composite (Container)**: Contains an internal collection of child `Component` elements. When asked to execute an operation, it delegates to its children and aggregates the results.

---

## 3. How It Works Internally

### Recursive Tree Aggregation
```
[ Directory: /root (Composite) ] -> size = 1500 KB
       |
       +---> [ File: readme.txt (Leaf) ] -> size = 100 KB
       |
       +---> [ Directory: /src (Composite) ] -> size = 1400 KB
                    |
                    +---> [ File: App.java (Leaf) ] -> size = 800 KB
                    +---> [ File: Utils.java (Leaf) ] -> size = 600 KB
```
When client calls `root.getSize()`, execution flows recursively:
```
root.getSize() = readme.getSize() + src.getSize()
                 100 KB           + (App.getSize() + Utils.getSize())
                 100 KB           + (800 KB        + 600 KB) = 1500 KB
```

---

## 4. Syntax & Usage

### 1. Classic Composite Hierarchy
```java
import java.util.ArrayList;
import java.util.List;

// Component
public interface FileSystemNode {
    long getSizeInBytes();
}

// Leaf
public record SimpleFile(String name, long size) implements FileSystemNode {
    @Override public long getSizeInBytes() { return size; }
}

// Composite
public class Directory implements FileSystemNode {
    private final String name;
    private final List<FileSystemNode> children = new ArrayList<>();

    public Directory(String name) { this.name = name; }
    public void add(FileSystemNode node) { children.add(node); }

    @Override
    public long getSizeInBytes() {
        return children.stream().mapToLong(FileSystemNode::getSizeInBytes).sum();
    }
}
```

---

## 5. Practical Examples

### Example 1: E-Commerce Product Box Packaging (Composite Order)
**Goal:** Calculate total price and weight for an order containing individual products and bundled gift boxes.

```java
import java.math.BigDecimal;
import java.util.ArrayList;
import java.util.List;

// Component Interface
public interface OrderItem {
    BigDecimal getPrice();
    double getWeightKg();
}

// Leaf: Individual Product
public record SingleProduct(String name, BigDecimal price, double weightKg) implements OrderItem {
    @Override public BigDecimal getPrice() { return price; }
    @Override public double getWeightKg() { return weightKg; }
}

// Composite: Gift Box containing multiple items and its own packaging weight
public class GiftBox implements OrderItem {
    private final String boxName;
    private final double packagingWeightKg;
    private final List<OrderItem> items = new ArrayList<>();

    public GiftBox(String boxName, double packagingWeightKg) {
        this.boxName = boxName;
        this.packagingWeightKg = packagingWeightKg;
    }

    public void addItem(OrderItem item) { items.add(item); }

    @Override
    public BigDecimal getPrice() {
        return items.stream()
                .map(OrderItem::getPrice)
                .reduce(BigDecimal.ZERO, BigDecimal::add);
    }

    @Override
    public double getWeightKg() {
        double contentsWeight = items.stream().mapToDouble(OrderItem::getWeightKg).sum();
        return contentsWeight + packagingWeightKg;
    }
}
```

### Example 2: Mathematical Expression Tree
**Goal:** Evaluate complex mathematical expressions using binary operator composites and number leaves.

```java
public sealed interface Expression permits NumberLeaf, AddComposite, MultiplyComposite {
    double evaluate();
}

public record NumberLeaf(double value) implements Expression {
    @Override public double evaluate() { return value; }
}

public record AddComposite(Expression left, Expression right) implements Expression {
    @Override public double evaluate() { return left.evaluate() + right.evaluate(); }
}

public record MultiplyComposite(Expression left, Expression right) implements Expression {
    @Override public double evaluate() { return left.evaluate() * right.evaluate(); }
}

// ((5 + 3) * 2) = 16.0
// Expression expr = new MultiplyComposite(new AddComposite(new NumberLeaf(5), new NumberLeaf(3)), new NumberLeaf(2));
```

---

### Example 3: Non-Recursive Iterative Composite Traversal
**Goal:** Avoid recursion stack overflow on 50,000-node trees using an explicit `ArrayDeque`.

```java
import java.util.ArrayDeque;
import java.util.Deque;
import java.util.List;

public class IterativeTreeScanner {
    public interface Node { List<Node> getChildren(); long getValue(); }

    public static long calculateTotalIterative(Node root) {
        long total = 0;
        Deque<Node> stack = new ArrayDeque<>();
        stack.push(root);

        while (!stack.isEmpty()) {
            Node current = stack.pop();
            total += current.getValue();
            for (Node child : current.getChildren()) {
                stack.push(child);
            }
        }
        return total;
    }
}
```

## 6. Best Practices

1. **Keep Component Interface Universal**: Include only operations that make sense for both leaves and composites (`getSize()`, `render()`, `evaluate()`).
2. **Declare Child Management in Composite Only**: Place `add()`, `remove()`, and `getChildren()` on the `Composite` class rather than the base `Component` interface to preserve type safety.
3. **Use Sealed Interfaces for Mathematical ASTs**: Java 21 sealed hierarchies ensure exhaustive compiler verification for expression trees.
4. **Beware of Circular References**: Prevent a directory from adding its parent directory as a child, which creates infinite recursive loops during traversal.
5. **Cache Aggregate Results if Trees are Large**: If traversing a 100,000-node tree is frequent, cache calculated weights/prices and invalidate on child mutations.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Secure Fix |
| :--- | :--- | :--- |
| **Declaring `add()` on Component interface** | Leaves must throw `UnsupportedOperationException`, violating LSP. | Place `add()` only on the `Composite` class. |
| **Circular Tree References** | Adding parent as child causes `StackOverflowError` on traversal. | Implement cycle detection during `add()`. |
| **Deep Tree Stack Overflows** | Deep recursion exceeding 10,000 levels blows the JVM thread stack. | Use iterative traversal with an explicit queue/stack. |
| **Checking `instanceof Leaf` in client code** | Defeats the entire purpose of uniform Composite polymorphism. | Treat all nodes uniformly via the Component interface. |

---

## 8. Performance & Security Notes

### Stack Depth in Deep Recursion
Traversing composite trees uses recursive method calls.
- In normal hierarchies ($< 100$ levels deep), recursive overhead is negligible and easily optimized by JIT.
- In unbalanced trees with 15,000+ nested nodes, recursion triggers `java.lang.StackOverflowError`.
- For deep trees, implement an iterative visitor or traversal using an explicit Java `ArrayDeque`.

---

## 9. Comparison / When to Use What

| Pattern | Structure | Focus |
| :--- | :--- | :--- |
| **Composite** | Recursive Tree (Part-Whole) | Treating leaves and composites uniformly |
| **Decorator** | Linear Wrapper Pipeline | Dynamically adding behaviors |
| **Chain of Responsibility** | Linear Handler Chain | Passing requests sequentially |
| **Flyweight** | Shared Pool | Memory optimization of repeated state |

---

## 10. Interview Questions & Answers

### Q1: What is the primary benefit of the Composite design pattern?
<details>
<summary>View Answer</summary>
The primary benefit is that client code does not need to worry whether it is dealing with a single leaf object or a composite collection of objects. The client treats both uniformly through the Component interface. This eliminates repetitive `if (node instanceof Directory)` branching and simplifies client algorithms.
</details>

### Q2: Should child management methods (`add()`, `remove()`) be declared in the Component interface or in the Composite class?
<details>
<summary>View Answer</summary>
This represents the classic tradeoff between **Transparency** and **Safety**:
- **Transparency**: Placing `add()` in `Component` allows all elements to be treated identically, but forces `Leaf` to throw `UnsupportedOperationException`, violating the Liskov Substitution Principle.
- **Safety**: Placing `add()` strictly in `Composite` preserves type safety (clients cannot call `add()` on a leaf), but requires clients to know when they are dealing with a Composite.
Modern Java strongly favors **Safety**: declare child mutators on the `Composite` class.
</details>

### Q3: How do you prevent circular dependencies in a Composite hierarchy?
<details>
<summary>View Answer</summary>
When adding a child node to a composite, traverse up the parent chain of the composite to ensure the proposed child is not already an ancestor of the composite. If it is, throw an `IllegalArgumentException` to prevent cyclic loops that cause infinite recursion.
</details>

### Q4: Give an example of the Composite pattern in the Java Standard Library.
<details>
<summary>View Answer</summary>
In Java AWT/Swing:
- `java.awt.Component` is the base component.
- `java.awt.Button`, `java.awt.Label` are leaves.
- `java.awt.Container` (and subclasses like `JPanel`) is the composite containing other components. Calling `container.paint()` recursively paints all child components.
</details>

### Q5: How can Java Records and Sealed Interfaces modernize Composite expression trees?
<details>
<summary>View Answer</summary>
By declaring `public sealed interface Expr permits Val, Add, Mul` and modeling operands as records, the entire AST becomes deeply immutable and pattern-matchable. Evaluators can use pattern matching switch expressions (`switch (expr) { case Val v -> ... }`) with compile-time exhaustiveness checks.
</details>

### Q6: What is the difference between Composite and Decorator?
<details>
<summary>View Answer</summary>
Both patterns rely on recursive composition. However, Decorator has only one child component and focuses on adding new responsibilities or behaviors to it. Composite has multiple child components and focuses on structuring data into part-whole hierarchies.
</details>

### Q7: How can the Visitor pattern be combined with Composite?
<details>
<summary>View Answer</summary>
When operations on the composite tree change frequently, adding new methods to all nodes causes code churn. The Visitor pattern moves operations outside the tree nodes into a `Visitor` class that traverses the composite structure, separating tree data from operations.
</details>

### Q8: How do you handle caching in a dynamic Composite tree?
<details>
<summary>View Answer</summary>
Nodes maintain a cached aggregate value (e.g. `cachedSize`). When a child is added or modified, an invalidation signal is propagated up the parent pointer chain to root, resetting cached values along the ancestral path.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Department Organization Hierarchy
**Task:** Calculate total employee count for an organization containing individual `Worker` leaves and `Department` composites.
<details>
<summary>View Solution</summary>

```java
import java.util.ArrayList;
import java.util.List;

public interface OrgUnit { int getHeadcount(); }

public record Worker(String name) implements OrgUnit {
    @Override public int getHeadcount() { return 1; }
}

public class Department implements OrgUnit {
    private final List<OrgUnit> units = new ArrayList<>();
    public void add(OrgUnit u) { units.add(u); }

    @Override
    public int getHeadcount() {
        return units.stream().mapToInt(OrgUnit::getHeadcount).sum();
    }
}
```
</details>

### Exercise 2: Cycle Detection in Composite Add
**Task:** Write an `add(Node child)` method that checks parent references to prevent cycles.
<details>
<summary>View Solution</summary>

```java
import java.util.ArrayList;
import java.util.List;

public class SafeNode {
    private SafeNode parent;
    private final List<SafeNode> children = new ArrayList<>();

    public void addChild(SafeNode child) {
        // Cycle check: verify 'child' is not an ancestor of 'this'
        SafeNode current = this;
        while (current != null) {
            if (current == child) throw new IllegalArgumentException("Cycle detected!");
            current = current.parent;
        }
        child.parent = this;
        children.add(child);
    }
}
```
</details>

### Exercise 3: XML Tag Composite Renderer
**Task:** Render nested XML nodes using Composite.
<details>
<summary>View Solution</summary>

```java
import java.util.ArrayList;
import java.util.List;

public interface XmlNode { String render(); }
public record TextNode(String text) implements XmlNode { @Override public String render() { return text; } }

public class TagNode implements XmlNode {
    private final String tag;
    private final List<XmlNode> children = new ArrayList<>();
    public TagNode(String tag) { this.tag = tag; }
    public void add(XmlNode n) { children.add(n); }

    @Override
    public String render() {
        StringBuilder sb = new StringBuilder("<" + tag + ">");
        for (XmlNode c : children) sb.append(c.render());
        sb.append("</" + tag + ">");
        return sb.toString();
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Core Goal**: Compose objects into tree structures; treat leaves and composites uniformly.
- **Component Interface**: Declares shared business operations.
- **Leaf**: Atomic building block without children.
- **Composite**: Aggregates children and delegates operations recursively.
- **Safety**: Place `add()` and `remove()` in the `Composite` class, not the interface.
- **Avoid Cycles**: Implement cycle detection on parent references to prevent infinite loops.

---

## 13. Further Reading
- GoF Design Patterns: [Composite Pattern](https://www.oreilly.com/library/view/design-patterns-elements/0201633612/)
- Refactoring Guru: [Composite Design Pattern](https://refactoring.guru/design-patterns/composite)
- Effective Java: [Item 18: Favor composition over inheritance](https://www.oreilly.com/library/view/effective-java-3rd/9780134686097/)
