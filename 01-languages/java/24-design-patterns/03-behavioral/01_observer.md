# Observer Pattern in Modern Java

> Event-driven architectures and state change notifications: Subject-Observer contracts, Java Flow API (Reactive Streams), PropertyChangeSupport, and thread-safe event buses.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core Design Pattern / Behavioral |
| **Prerequisites** | [00_solid-principles](../23-design-and-clean-code/00_solid-principles.md), [14_concurrency](../../14-concurrency/README.md) |
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

### What is the Observer Pattern?
The Observer pattern is a behavioral design pattern that defines a **one-to-many dependency between objects**, so that when one object (the Subject / Observable) changes state, all its dependents (Observers / Listeners) are notified and updated automatically.

In enterprise software engineering:
- Decouples state producers from event consumers.
- Powers reactive UI frameworks and event-driven microservices.
- The foundation of Pub/Sub messaging, WebSocket broadcast channels, and audit trails.

### Why Deprecation of `java.util.Observer` Matters
In Java 9, the legacy `java.util.Observer` and `java.util.Observable` classes were deprecated:
- `Observable` was a class, not an interface, preventing classes from inheriting from any other class.
- It was not thread-safe and did not support generic types (`Object arg` required casting).
- Modern Java uses custom interfaces, **Java 9+ `java.util.concurrent.Flow` (Reactive Streams)**, or Spring Application Events.

---

## 2. Core Concepts

### Subject, Observer, and Notification Models
1. **Subject (Publisher / Observable)**: Maintains a list of registered observers; provides `subscribe()` and `unsubscribe()` methods.
2. **Observer (Subscriber / Listener)**: Implements an `onUpdate()` or `onEvent()` method.
3. **Push Model**: The subject passes rich event data directly in the notification (`onUpdate(EventData data)`).
4. **Pull Model**: The subject notifies observers that a change occurred (`onUpdate()`), and observers pull whatever data they need by querying the subject.

---

## 3. How It Works Internally

### Broadcast Notification Loop
```
[ Subject (StockMarket) ]
  - subscribers: CopyOnWriteArrayList<StockListener>
         |
         | price changes -> notifySubscribers(quote)
         |
         +--> [ TraderTerminal (Subscriber 1) ].onPriceUpdate(quote)
         +--> [ MobileApp (Subscriber 2)      ].onPriceUpdate(quote)
         +--> [ AlgorithmicBot (Subscriber 3) ].onPriceUpdate(quote)
```

In multi-threaded environments, storing subscribers in a `CopyOnWriteArrayList` ensures that broadcasting notifications never throws a `ConcurrentModificationException` if an observer unsubscribes while a broadcast is underway.

---

## 4. Syntax & Usage

### 1. Thread-Safe Observer Implementation
```java
import java.util.List;
import java.util.concurrent.CopyOnWriteArrayList;

// Observer Interface
public interface PriceListener {
    void onPriceChanged(String symbol, double newPrice);
}

// Subject
public class StockTicker {
    private final List<PriceListener> listeners = new CopyOnWriteArrayList<>();

    public void addListener(PriceListener l) { listeners.add(l); }
    public void removeListener(PriceListener l) { listeners.remove(l); }

    public void updatePrice(String symbol, double price) {
        for (PriceListener listener : listeners) {
            listener.onPriceChanged(symbol, price);
        }
    }
}
```

---

## 5. Practical Examples

### Example 1: Enterprise Order Event Publisher with Immutable Events
**Goal:** Publish order lifecycle events to inventory, notification, and analytics subscribers.

```java
import java.math.BigDecimal;
import java.time.Instant;
import java.util.List;
import java.util.Objects;
import java.util.concurrent.CopyOnWriteArrayList;

// Immutable Event Record
public record OrderPlacedEvent(String orderId, String customerEmail, BigDecimal total, Instant timestamp) {}

// Observer Interface
public interface OrderEventListener {
    void onOrderPlaced(OrderPlacedEvent event);
}

// Concrete Subscriber 1: Inventory
public class InventoryListener implements OrderEventListener {
    @Override
    public void onOrderPlaced(OrderPlacedEvent event) {
        System.out.println("INVENTORY: Reserving items for order " + event.orderId());
    }
}

// Concrete Subscriber 2: Email Confirmation
public class EmailListener implements OrderEventListener {
    @Override
    public void onOrderPlaced(OrderPlacedEvent event) {
        System.out.println("EMAIL: Sending receipt to " + event.customerEmail());
    }
}

// Subject / Event Publisher
public class OrderEventPublisher {
    private final List<OrderEventListener> listeners = new CopyOnWriteArrayList<>();

    public void subscribe(OrderEventListener listener) {
        listeners.add(Objects.requireNonNull(listener));
    }

    public void unsubscribe(OrderEventListener listener) {
        listeners.remove(listener);
    }

    public void publishOrder(OrderPlacedEvent event) {
        for (OrderEventListener listener : listeners) {
            try {
                listener.onOrderPlaced(event);
            } catch (Exception ex) {
                System.err.println("Listener error: " + ex.getMessage());
            }
        }
    }
}
```

### Example 2: Java 9+ Reactive Flow API (`SubmissionPublisher`)
**Goal:** Use modern standard library reactive streams with backpressure.

```java
import java.util.concurrent.Flow;
import java.util.concurrent.SubmissionPublisher;

public class ReactiveObserverDemo {
    public static void main(String[] args) throws InterruptedException {
        // Built-in Flow Publisher (Subject)
        try (SubmissionPublisher<String> publisher = new SubmissionPublisher<>()) {
            
            // Flow Subscriber (Observer)
            Flow.Subscriber<String> subscriber = new Flow.Subscriber<>() {
                private Flow.Subscription subscription;

                @Override
                public void onSubscribe(Flow.Subscription subscription) {
                    this.subscription = subscription;
                    subscription.request(1); // Request first item (Backpressure!)
                }

                @Override
                public void onNext(String item) {
                    System.out.println("REACTIVE CONSUMER: " + item);
                    subscription.request(1); // Request next item
                }

                @Override public void onError(Throwable t) { t.printStackTrace(); }
                @Override public void onComplete() { System.out.println("Stream complete."); }
            };

            publisher.subscribe(subscriber);
            publisher.submit("Event 1: User Login");
            publisher.submit("Event 2: File Uploaded");

            Thread.sleep(500); // Allow async delivery
        }
    }
}
```

---

### Example 3: Thread-Safe PropertyChangeSupport (Java Beans Event Model)
**Goal:** Use standard JDK `PropertyChangeSupport` to broadcast bean attribute modifications.

```java
import java.beans.PropertyChangeListener;
import java.beans.PropertyChangeSupport;

public class ObservableConfiguration {
    private final PropertyChangeSupport support = new PropertyChangeSupport(this);
    private String environment = "DEVELOPMENT";

    public void addPropertyChangeListener(PropertyChangeListener pcl) {
        support.addPropertyChangeListener(pcl);
    }

    public void setEnvironment(String newEnv) {
        support.firePropertyChange("environment", this.environment, newEnv);
        this.environment = newEnv;
    }
}
```

## 6. Best Practices

1. **Use `CopyOnWriteArrayList` for Subscriber Lists**: Prevents `ConcurrentModificationException` when subscribers register or unregister during event broadcasts.
2. **Isolate Listener Exceptions**: Catch and log exceptions inside the broadcast loop so one failing subscriber does not abort notifications to subsequent subscribers.
3. **Pass Immutable Event Records**: Always publish immutable data records to prevent observers from corrupting the event state.
4. **Beware of the Lapsed Listener Memory Leak**: If subscribers fail to unsubscribe, the subject retains strong references to them indefinitely. Use `WeakReference` or explicit lifecycle unsubscription.
5. **Prefer Asynchronous Dispatch for Heavy Listeners**: If a subscriber performs slow disk I/O, execute the notification on a background executor or virtual thread.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Secure Fix |
| :--- | :--- | :--- |
| **The Lapsed Listener Memory Leak** | Observers register but never unregister; subject keeps them in memory forever. | Provide explicit `close()` or unregister on shutdown. |
| **Blocking the Publisher Thread** | One slow subscriber hangs the entire broadcast loop for all other subscribers. | Dispatch notifications asynchronously via thread pool. |
| **Passing Mutable Events** | Subscriber A modifies the event; Subscriber B receives corrupted data. | Use immutable Java records for event payloads. |
| **Using `ArrayList` without synchronization** | Calling `subscribe()` while a broadcast is running throws `ConcurrentModificationException`. | Use `CopyOnWriteArrayList`. |

---

## 8. Performance & Security Notes

### Asynchronous Event Dispatch with Virtual Threads
In high-throughput systems, synchronous observer notifications bottleneck the publishing thread.
By wrapping listener invocation in Java 21 Virtual Threads:
```java
Thread.ofVirtual().start(() -> listener.onOrderPlaced(event));
```
Thousands of subscribers are notified concurrently in sub-milliseconds without blocking the publisher or consuming carrier OS threads.

---

## 9. Comparison / When to Use What

| Pattern / Mechanism | Communication Style | Backpressure Support | Best For |
| :--- | :--- | :--- | :--- |
| **Classic Observer** | Synchronous 1-to-many | No | Simple in-memory event notifications |
| **Java Flow API** | Asynchronous Reactive | Yes (Reactive Streams) | High-volume data streaming with backpressure |
| **Spring ApplicationEvents** | Sync or Async | No | Enterprise domain events in Spring |
| **Kafka / RabbitMQ** | Distributed Pub/Sub | Yes | Cross-microservice event distribution |

---

## 10. Interview Questions & Answers

### Q1: Why were `java.util.Observer` and `java.util.Observable` deprecated in Java 9?
<details>
<summary>View Answer</summary>
1. Not an Interface: `Observable` was a class, meaning classes that needed to be observables could not extend any other superclass.
2. Lack of Generics: It was written in Java 1.0 before generics, requiring casting from `Object`.
3. Thread-Safety Limitations: The notification order was unspecified and synchronization was coarse-grained.
4. Modern Alternatives: Replaced by the `java.util.concurrent.Flow` reactive streams API and lightweight custom functional interfaces.
</details>

### Q2: What is the "Lapsed Listener Problem" and how do you prevent it?
<details>
<summary>View Answer</summary>
The lapsed listener problem is a notorious memory leak where a short-lived observer registers with a long-lived subject (e.g. a UI dialog registering with a global singleton event bus) and forgets to unregister. Because the subject holds a strong reference to the observer, the garbage collector cannot collect the observer, causing memory leaks. Solutions: (1) Always unsubscribe in cleanup blocks (`dispose()`, `close()`), or (2) Store observer references using `WeakReference` (`WeakHashMap`).
</details>

### Q3: What is the difference between the Push Model and the Pull Model in Observer?
<details>
<summary>View Answer</summary>
- **Push Model**: The subject sends detailed event information as an argument to `update(EventData)`. It is more cohesive because observers get everything they need without re-querying, but less flexible if different observers need different subsets of data.
- **Pull Model**: The subject sends minimal notification (`update()`) or passes a reference to itself. Observers pull only the specific data fields they require. It is more flexible, but couples observers to the subject\'s getter methods.
</details>

### Q4: Why is `CopyOnWriteArrayList` ideal for holding observers?
<details>
<summary>View Answer</summary>
In the Observer pattern, reads (iterating to notify observers) occur far more frequently than writes (subscribing or unsubscribing). `CopyOnWriteArrayList` creates a fresh copy of the underlying array only on mutations, allowing readers to traverse the list without holding locks and guaranteeing zero `ConcurrentModificationException` if a listener unsubscribes during a broadcast.
</details>

### Q5: How does the Java 9 Flow API implement backpressure?
<details>
<summary>View Answer</summary>
In `Flow`, subscribers control the flow of data using the `Subscription.request(n)` method. The publisher does not push items until the subscriber explicitly signals that it has capacity to handle $n$ items. This prevents fast publishers from overwhelming slow consumers with OutOfMemoryErrors.
</details>

### Q6: Can a Lambda expression be an Observer?
<details>
<summary>View Answer</summary>
Yes. If the Observer interface is a Single Abstract Method (SAM) functional interface (e.g. `PriceListener`), clients can subscribe using lambdas: `ticker.addListener((symbol, price) -> System.out.println(price));`.
</details>

### Q7: What is the difference between Observer and Mediator?
<details>
<summary>View Answer</summary>
Observer creates a one-way broadcast from a Subject to multiple Observers. Mediator centralizes complex, bidirectional, many-to-many communication between multiple peer colleague objects, preventing direct dependencies between peers.
</details>

### Q8: Should event notifications run synchronously or asynchronously?
<details>
<summary>View Answer</summary>
If observers require immediate transactional consistency with the publisher, synchronous notifications are appropriate. If observers perform slow operations (I/O, network calls, metrics), asynchronous execution is mandatory to prevent publisher thread starvation.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build an Event Bus with Generic Events
**Task:** Create a simple in-memory event bus that registers listeners by event class type.
<details>
<summary>View Solution</summary>

```java
import java.util.*;
import java.util.concurrent.*;

public class SimpleEventBus {
    private final Map<Class<?>, List<java.util.function.Consumer<?>>> listeners = new ConcurrentHashMap<>();

    @SuppressWarnings("unchecked")
    public <T> void register(Class<T> eventType, java.util.function.Consumer<T> listener) {
        listeners.computeIfAbsent(eventType, k -> new CopyOnWriteArrayList<>()).add(listener);
    }

    @SuppressWarnings("unchecked")
    public <T> void publish(T event) {
        List<java.util.function.Consumer<?>> handlers = listeners.get(event.getClass());
        if (handlers != null) {
            for (var handler : handlers) {
                ((java.util.function.Consumer<T>) handler).accept(event);
            }
        }
    }
}
```
</details>

### Exercise 2: Temperature Sensor Listener
**Task:** Implement a temperature sensor with alarm thresholds for high heat.
<details>
<summary>View Solution</summary>

```java
public class Thermostat {
    public interface AlarmListener { void onHighTemp(double temp); }
    private final List<AlarmListener> listeners = new CopyOnWriteArrayList<>();

    public void addListener(AlarmListener l) { listeners.add(l); }
    public void setTemperature(double temp) {
        if (temp > 100.0) {
            listeners.forEach(l -> l.onHighTemp(temp));
        }
    }
}
```
</details>

### Exercise 3: Weak Reference Observer Wrapper
**Task:** Create a wrapper to prevent the lapsed listener memory leak using `WeakReference`.
<details>
<summary>View Solution</summary>

```java
import java.lang.ref.WeakReference;

public class WeakObserver<T> {
    private final WeakReference<T> ref;
    public WeakObserver(T listener) { this.ref = new WeakReference<>(listener); }
    public T get() { return ref.get(); }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Core Goal**: Maintain 1-to-many relationship where state change triggers automatic updates.
- **Thread Safety**: Store listeners in `CopyOnWriteArrayList` to allow concurrent iteration.
- **Memory Leaks**: Beware of the Lapsed Listener problem; always provide unsubscription.
- **Modern API**: Use `java.util.concurrent.Flow` for reactive backpressure streams.
- **Immutable Events**: Always publish immutable records as event payloads.
- **Exception Isolation**: Catch listener errors inside the broadcast loop.

---

## 13. Further Reading
- GoF Design Patterns: [Observer Pattern](https://www.oreilly.com/library/view/design-patterns-elements/0201633612/)
- Oracle Java Documentation: [Flow API Specification](https://docs.oracle.com/en/java/javase/21/docs/api/java.base/java/util/concurrent/Flow.html)
- Reactive Streams: [Reactive Streams Specification for the JVM](https://www.reactive-streams.org/)
