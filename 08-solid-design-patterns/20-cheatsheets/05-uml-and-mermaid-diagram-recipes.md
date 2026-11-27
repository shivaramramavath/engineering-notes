# UML & Mermaid Diagram Recipes

## 1. Class Diagram (Interface + Composition)
````markdown
```mermaid
classDiagram
    class OrderService { -OrderRepo repo }
    class OrderRepo { <<interface>> +save(Order) }
    OrderService --> OrderRepo
    OrderRepo <|.. PostgresOrderRepo
```
````

## 2. Sequence Diagram
````markdown
```mermaid
sequenceDiagram
    Client->>API: POST /orders
    API->>DB: BEGIN -> INSERT -> COMMIT
    API-->>Client: 201 Created
```
````

## 3. State Machine
````markdown
```mermaid
stateDiagram-v2
    [*] --> CLOSED
    CLOSED --> OPEN: Failures >= threshold
    OPEN --> HALF_OPEN: Cooldown elapsed
    HALF_OPEN --> CLOSED: Probe OK
```
````

## 4. Architecture Flowchart
````markdown
```mermaid
flowchart LR
    Adapter["REST Adapter"] --> Port["UseCase Port"] --> Core["Domain Core"]
```
````
