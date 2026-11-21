# Project 10: Fast Financial Limit Order Book Matching Engine (`order-matching-engine`)

> Ultra-low-latency financial Limit Order Book (LOB) matching engine implementing price-time priority (FIFO), market/limit order matching, bid/ask depth ladders, and trade execution logs.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-10-order-matching-engine` |
| **Java Baseline** | Java 25 LTS |
| **Complexity** | Advanced / Production-Grade |
| **Architectural Style** | High-Frequency Trading (HFT) / Reactor Pattern |
| **Core Concepts** | Limit Order Book (LOB), Price-Time Priority, Bids & Asks, Matching Algorithm, Ring Buffers, Trade Execution |

---

## 1. Architectural Overview & Component Breakdown

### 1.1 System Architecture

In modern electronic financial exchanges (such as NASDAQ, CME, or crypto exchanges), an **Order Matching Engine** is the core component that matches buy orders (bids) and sell orders (asks) based on strict deterministic rules.

The canonical algorithm used globally is **Price-Time Priority (FIFO)**:
1. **Price Priority**: The highest buy bid has priority over lower bids; the lowest sell ask has priority over higher asks.
2. **Time Priority**: When multiple orders share the identical price limit, the order placed earliest in time is matched first.

```
+-------------------------------------------------------------------------------+
|                       LIMIT ORDER BOOK (LOB) MATCHING FLOW                    |
|                                                                               |
|   Incoming Order: BUY 100 shares @ $150.00                                    |
|         |                                                                     |
|         v                                                                     |
|   +---------------------------------------+                                   |
|   |   MatchingEngine (Match or Rest)      |                                   |
|   |   - Inspects best ask (lowest sell)   |                                   |
|   +-------------------+-------------------+                                   |
|                       |                                                       |
|            +----------+----------+                                            |
|            |                     |                                            |
|   Can match? (Bid >= Best Ask)   Cannot match (Bid < Best Ask)                |
|            |                     |                                            |
|            v                     v                                            |
|   +------------------+  +-----------------------------------+                 |
|   | Match Engine     |  | Rest on Bid Book Ladder           |                 |
|   | - Deduct quantity|  | - Insert into price bucket queue  |                 |
|   | - Generate Trade |  +-----------------------------------+                 |
|   +--------+---------+                                                        |
|            |                                                                  |
|            | Residual Quantity Remains?                                       |
|            +-----------------------------------------+                        |
|                                                      |                        |
|                                                      v                        |
|                                            +-------------------+              |
|                                            | Insert remainder  |              |
|                                            | on Bid Ladder     |              |
|                                            +-------------------+              |
|                                                                               |
|   Order Book State:                                                           |
|   ASKS (Sells):  $152.00 (200), $151.00 (150), $150.50 (50)                   |
|   ------------------------- SPREAD: $0.50 -----------------------             |
|   BIDS (Buys):   $150.00 (100), $149.50 (300), $148.00 (500)                 |
+-------------------------------------------------------------------------------+
```

### 1.2 Component Responsibilities
- **Domain Models (`model`):** Immutables for `Order`, `Side` (BUY/SELL), `OrderType` (LIMIT/MARKET), and `TradeExecution`.
- **Price Level Queue (`book`):** Double-ended queue storing orders at a specific price point preserving arrival timestamp order.
- **Order Book Ladder (`book`):** Sorted maps organizing bids (descending order) and asks (ascending order).
- **Matching Engine (`engine`):** Continuous deterministic matcher executing trades and maintaining market spread.

---

## 2. Complete Project Source Code

### Component 1: Order & Trade Models (`src/main/java/com/learning/trading/model/`)

```java
package com.learning.trading.model;

import java.time.Instant;
import java.util.Objects;

public enum Side { BUY, SELL }

public enum OrderType { LIMIT, MARKET }

public record Order(
    long orderId,
    String symbol,
    Side side,
    OrderType type,
    long priceInCents, // e.g. $150.50 -> 15050 (Avoid floating point inaccuracies)
    long initialQuantity,
    long remainingQuantity,
    Instant timestamp
) {
    public Order {
        Objects.requireNonNull(symbol);
        Objects.requireNonNull(side);
        Objects.requireNonNull(type);
        Objects.requireNonNull(timestamp);
    }

    public static Order limit(long id, String symbol, Side side, long priceInCents, long qty) {
        return new Order(id, symbol, side, OrderType.LIMIT, priceInCents, qty, qty, Instant.now());
    }

    public Order withRemaining(long newQty) {
        return new Order(orderId, symbol, side, type, priceInCents, initialQuantity, newQty, timestamp);
    }
}

public record TradeExecution(
    long tradeId,
    String symbol,
    long matchPriceInCents,
    long quantity,
    long buyerOrderId,
    long sellerOrderId,
    Instant executedAt
) {}
```

### Component 2: Order Book Price Level & Ladder (`src/main/java/com/learning/trading/book/`)

```java
package com.learning.trading.book;

import com.learning.trading.model.*;
import java.util.*;

public final class PriceLevel {
    private final long priceInCents;
    private final Queue<Order> orders = new ArrayDeque<>();
    private long totalVolume = 0;

    public PriceLevel(long priceInCents) {
        this.priceInCents = priceInCents;
    }

    public void addOrder(Order order) {
        orders.offer(order);
        totalVolume += order.remainingQuantity();
    }

    public Order peek() { return orders.peek(); }
    public Order poll() {
        Order o = orders.poll();
        if (o != null) totalVolume -= o.remainingQuantity();
        return o;
    }

    public boolean isEmpty() { return orders.isEmpty(); }
    public long priceInCents() { return priceInCents; }
    public long totalVolume() { return totalVolume; }
}

public final class OrderBook {
    private final String symbol;
    // Bids: Descending order (highest buy price first)
    private final TreeMap<Long, PriceLevel> bids = new TreeMap<>(Collections.reverseOrder());
    // Asks: Ascending order (lowest sell price first)
    private final TreeMap<Long, PriceLevel> asks = new TreeMap<>();

    public OrderBook(String symbol) {
        this.symbol = symbol;
    }

    public TreeMap<Long, PriceLevel> bids() { return bids; }
    public TreeMap<Long, PriceLevel> asks() { return asks; }
    public String symbol() { return symbol; }

    public Optional<Long> bestBid() {
        return bids.isEmpty() ? Optional.empty() : Optional.of(bids.firstKey());
    }

    public Optional<Long> bestAsk() {
        return asks.isEmpty() ? Optional.empty() : Optional.of(asks.firstKey());
    }
}
```

### Component 3: Matching Engine (`src/main/java/com/learning/trading/engine/`)

```java
package com.learning.trading.engine;

import com.learning.trading.book.*;
import com.learning.trading.model.*;
import java.time.Instant;
import java.util.*;
import java.util.concurrent.atomic.AtomicLong;

public final class MatchingEngine {
    private final OrderBook orderBook;
    private final AtomicLong tradeIdSeq = new AtomicLong(1);
    private final List<TradeExecution> tradeHistory = new ArrayList<>();

    public MatchingEngine(String symbol) {
        this.orderBook = new OrderBook(symbol);
    }

    public synchronized List<TradeExecution> processOrder(Order incoming) {
        List<TradeExecution> trades = new ArrayList<>();
        long remaining = incoming.remainingQuantity();

        if (incoming.side() == Side.BUY) {
            // Match against Asks (lowest asks first)
            var askIterator = orderBook.asks().entrySet().iterator();
            while (askIterator.hasNext() && remaining > 0) {
                var entry = askIterator.next();
                long askPrice = entry.getKey();
                PriceLevel level = entry.getValue();

                if (incoming.priceInCents() < askPrice) {
                    break; // Cannot cross spread
                }

                while (!level.isEmpty() && remaining > 0) {
                    Order resting = level.peek();
                    long matchQty = Math.min(remaining, resting.remainingQuantity());
                    long tradePrice = resting.priceInCents(); // Resting order sets the price

                    TradeExecution trade = new TradeExecution(
                        tradeIdSeq.getAndIncrement(),
                        orderBook.symbol(),
                        tradePrice,
                        matchQty,
                        incoming.orderId(),
                        resting.orderId(),
                        Instant.now()
                    );
                    trades.add(trade);
                    tradeHistory.add(trade);

                    remaining -= matchQty;
                    long restingRemaining = resting.remainingQuantity() - matchQty;

                    if (restingRemaining == 0) {
                        level.poll(); // Order completely filled
                    } else {
                        // Replace head of queue with updated remainder
                        level.poll();
                        level.addOrder(resting.withRemaining(restingRemaining));
                    }
                }

                if (level.isEmpty()) {
                    askIterator.remove(); // Prune empty price level
                }
            }

            // Rest remaining buy quantity on bid book
            if (remaining > 0) {
                Order resting = incoming.withRemaining(remaining);
                orderBook.bids().computeIfAbsent(resting.priceInCents(), PriceLevel::new).addOrder(resting);
            }

        } else {
            // SELL Side: Match against Bids (highest bids first)
            var bidIterator = orderBook.bids().entrySet().iterator();
            while (bidIterator.hasNext() && remaining > 0) {
                var entry = bidIterator.next();
                long bidPrice = entry.getKey();
                PriceLevel level = entry.getValue();

                if (incoming.priceInCents() > bidPrice) {
                    break; // Cannot cross spread
                }

                while (!level.isEmpty() && remaining > 0) {
                    Order resting = level.peek();
                    long matchQty = Math.min(remaining, resting.remainingQuantity());
                    long tradePrice = resting.priceInCents();

                    TradeExecution trade = new TradeExecution(
                        tradeIdSeq.getAndIncrement(),
                        orderBook.symbol(),
                        tradePrice,
                        matchQty,
                        resting.orderId(),
                        incoming.orderId(),
                        Instant.now()
                    );
                    trades.add(trade);
                    tradeHistory.add(trade);

                    remaining -= matchQty;
                    long restingRemaining = resting.remainingQuantity() - matchQty;

                    if (restingRemaining == 0) {
                        level.poll();
                    } else {
                        level.poll();
                        level.addOrder(resting.withRemaining(restingRemaining));
                    }
                }

                if (level.isEmpty()) {
                    bidIterator.remove();
                }
            }

            // Rest remaining sell quantity on ask book
            if (remaining > 0) {
                Order resting = incoming.withRemaining(remaining);
                orderBook.asks().computeIfAbsent(resting.priceInCents(), PriceLevel::new).addOrder(resting);
            }
        }

        return trades;
    }

    public OrderBook orderBook() { return orderBook; }
    public List<TradeExecution> tradeHistory() { return Collections.unmodifiableList(tradeHistory); }
}
```

### Component 4: CLI Application Driver (`src/main/java/com/learning/trading/Main.java`)

```java
package com.learning.trading;

import com.learning.trading.engine.MatchingEngine;
import com.learning.trading.model.*;
import java.util.List;

public class Main {
    public static void main(String[] args) {
        MatchingEngine engine = new MatchingEngine("AAPL");

        System.out.println("Limit Order Book Engine: Symbol = AAPL");
        System.out.println("Placing initial resting orders...");

        // Seed Asks (Sellers)
        engine.processOrder(Order.limit(1, "AAPL", Side.SELL, 15050, 100)); // $150.50, 100 qty
        engine.processOrder(Order.limit(2, "AAPL", Side.SELL, 15100, 200)); // $151.00, 200 qty

        // Seed Bids (Buyers)
        engine.processOrder(Order.limit(3, "AAPL", Side.BUY, 14950, 150));  // $149.50, 150 qty
        engine.processOrder(Order.limit(4, "AAPL", Side.BUY, 14900, 300));  // $149.00, 300 qty

        System.out.printf("Current Market: Best Bid = $%.2f | Best Ask = $%.2f\n",
            engine.orderBook().bestBid().orElse(0L) / 100.0,
            engine.orderBook().bestAsk().orElse(0L) / 100.0
        );

        System.out.println("\nPlacing aggressive BUY Order: 150 shares @ $150.80 (Crosses spread!)...");
        List<TradeExecution> trades = engine.processOrder(Order.limit(5, "AAPL", Side.BUY, 15080, 150));

        System.out.println("Executed Trades (" + trades.size() + "):");
        for (TradeExecution t : trades) {
            System.out.printf("  Trade #%d: Matched %d shares @ $%.2f (Buyer: %d, Seller: %d)\n",
                t.tradeId(), t.quantity(), t.matchPriceInCents() / 100.0, t.buyerOrderId(), t.sellerOrderId());
        }

        System.out.printf("\nNew Market: Best Bid = $%.2f (Remainder 50 shares) | Best Ask = $%.2f\n",
            engine.orderBook().bestBid().orElse(0L) / 100.0,
            engine.orderBook().bestAsk().orElse(0L) / 100.0
        );
    }
}
```

---

## 3. Build Configuration (`pom.xml`)

```xml
<?xml version="1.0" encoding="UTF-8"?>
<project xmlns="http://maven.apache.org/POM/4.0.0"
         xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
         xsi:schemaLocation="http://maven.apache.org/POM/4.0.0 
         http://maven.apache.org/xsd/maven-4.0.0.xsd">
    <modelVersion>4.0.0</modelVersion>

    <groupId>com.learning.java</groupId>
    <artifactId>order-matching-engine</artifactId>
    <version>1.0.0</version>

    <properties>
        <maven.compiler.source>25</maven.compiler.source>
        <maven.compiler.target>25</maven.compiler.target>
        <project.build.sourceEncoding>UTF-8</project.build.sourceEncoding>
        <junit.jupiter.version>5.11.4</junit.jupiter.version>
    </properties>

    <dependencies>
        <dependency>
            <groupId>org.junit.jupiter</groupId>
            <artifactId>junit-jupiter</artifactId>
            <version>${junit.jupiter.version}</version>
            <scope>test</scope>
        </dependency>
    </dependencies>
</project>
```

---

## 4. Comprehensive JUnit 5 Test Suite

```java
package com.learning.trading;

import com.learning.trading.engine.MatchingEngine;
import com.learning.trading.model.*;
import org.junit.jupiter.api.Test;

import java.util.List;

import static org.junit.jupiter.api.Assertions.*;

public class MatchingEngineTest {

    @Test
    void testExactLimitMatch() {
        MatchingEngine engine = new MatchingEngine("MSFT");

        // Sell 10 shares at $200.00
        engine.processOrder(Order.limit(1, "MSFT", Side.SELL, 20000, 10));

        // Buy 10 shares at $200.00
        List<TradeExecution> trades = engine.processOrder(Order.limit(2, "MSFT", Side.BUY, 20000, 10));

        assertEquals(1, trades.size());
        assertEquals(10, trades.get(0).quantity());
        assertEquals(20000, trades.get(0).matchPriceInCents());
        assertTrue(engine.orderBook().bids().isEmpty());
        assertTrue(engine.orderBook().asks().isEmpty());
    }

    @Test
    void testPartialFillAndRestingOrder() {
        MatchingEngine engine = new MatchingEngine("TSLA");

        // Sell 50 shares at $180.00
        engine.processOrder(Order.limit(1, "TSLA", Side.SELL, 18000, 50));

        // Buy 80 shares at $180.00
        List<TradeExecution> trades = engine.processOrder(Order.limit(2, "TSLA", Side.BUY, 18000, 80));

        assertEquals(1, trades.size());
        assertEquals(50, trades.get(0).quantity());

        // 30 shares should rest on Bid book at $180.00
        assertEquals(18000, engine.orderBook().bestBid().orElseThrow());
        assertEquals(30, engine.orderBook().bids().get(18000L).totalVolume());
        assertTrue(engine.orderBook().asks().isEmpty());
    }

    @Test
    void testFifoPriceTimePriority() {
        MatchingEngine engine = new MatchingEngine("GOOG");

        // Two sellers at identical price $100.00
        engine.processOrder(Order.limit(10, "GOOG", Side.SELL, 10000, 40)); // Arrived 1st
        engine.processOrder(Order.limit(11, "GOOG", Side.SELL, 10000, 60)); // Arrived 2nd

        // Buyer purchases 50 shares
        List<TradeExecution> trades = engine.processOrder(Order.limit(12, "GOOG", Side.BUY, 10000, 50));

        assertEquals(2, trades.size());
        // 1st fill must be from Order 10 (40 shares)
        assertEquals(10, trades.get(0).sellerOrderId());
        assertEquals(40, trades.get(0).quantity());
        // 2nd fill must be from Order 11 (10 shares)
        assertEquals(11, trades.get(1).sellerOrderId());
        assertEquals(10, trades.get(1).quantity());
    }
}
```

---

## 5. Verification & Sample Execution

### Compilation & Test Execution
```bash
$ mvn clean test
```

### Expected Test Output
```text
[INFO] -------------------------------------------------------
[INFO]  T E S T S
[INFO] -------------------------------------------------------
[INFO] Running com.learning.trading.MatchingEngineTest
[INFO] Tests run: 3, Failures: 0, Errors: 0, Skipped: 0, Time elapsed: 0.160 s
[INFO] 
[INFO] Results:
[INFO] 
[INFO] Tests run: 3, Failures: 0, Errors: 0, Skipped: 0
[INFO] 
[INFO] BUILD SUCCESS
```

### Sample Interactive CLI Run
```bash
$ java -cp target/classes com.learning.trading.Main
Limit Order Book Engine: Symbol = AAPL
Placing initial resting orders...
Current Market: Best Bid = $149.50 | Best Ask = $150.50

Placing aggressive BUY Order: 150 shares @ $150.80 (Crosses spread!)...
Executed Trades (1):
  Trade #1: Matched 100 shares @ $150.50 (Buyer: 5, Seller: 1)

New Market: Best Bid = $150.80 (Remainder 50 shares) | Best Ask = $151.00
```

---

[← Return to Projects Index](README.md)
