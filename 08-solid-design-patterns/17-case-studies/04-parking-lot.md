# Case Study 04: Parking Lot System

**Patterns Used**: **Strategy** (`HourlyPricing`, `FlatPricing`; `NearestSpotStrategy`), **Factory** (Ticket creation by `VehicleType`), **Observer** (Display board updates when spots free up), **Optimistic/Synchronized Spot Locking**.

```mermaid
classDiagram
    ParkingLot "1" *-- "*" Floor
    Floor "1" *-- "*" ParkingSpot
    ParkingLot --> SpotAllocationStrategy
    ParkingLot --> PricingStrategy
    ParkingLot --> DisplayObserver
```

## Key Design Insight
Separate **Spot Allocation** (`NearestToEntry` vs `EvChargerFirst`) and **Pricing** (`Hourly` vs `WeekendFlat`) as injectable Strategies, and guard concurrent spot assignment atomically so two cars never get assigned the same spot.
