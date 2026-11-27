# Low-Level Design (LLD) Interview Problems

| # | Problem | Core Entities & Patterns |
| :--- | :--- | :--- |
| 1 | **Parking Lot** | `ParkingLot`, `Floor`, `Spot`, `Ticket` + **Strategy** (Spot/Pricing) + **Factory** |
| 2 | **Elevator System** | `Elevator`, `Dispatcher` + **State** + **Strategy** (SCAN) + **Observer** |
| 3 | **Rate Limiter** | `RateLimiter` + **Strategy** (Token Bucket / Sliding Window) + **Atomic Store** |
| 4 | **Notification Service** | `Notification` + **Strategy** (Email/SMS/Push) + **Decorator** (Retry) + **Outbox** |
| 5 | **Vending Machine** | `VendingMachine` + **State** (`Idle`, `HasMoney`, `Dispensing`) |
| 6 | **Splitwise / Expense Sharing** | `Expense`, `Split` + **Strategy** (`Equal`, `Exact`, `Percent`) + **Command** |
| 7 | **LRU Cache** | `DoublyLinkedList` + `HashMap` + **Proxy/Decorator** + Thread-safety lock |
| 8 | **Logging Framework** | `Logger`, `Appender` + **Chain of Responsibility** + **Strategy** (Formatter) + **Async Queue** |
| 9 | **Chess / Tic-Tac-Toe** | `Board`, `Piece`, `Move` + **Command** (Undo) + **Strategy** (Piece movement) |
| 10 | **Pub-Sub / Task Scheduler** | `Topic`, `Subscriber` + **Observer** + **Thread Pool / PriorityQueue** |
