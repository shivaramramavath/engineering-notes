# Case Study 09: Game Engine Entities

**Patterns Used**: **Composition over Inheritance (Entity–Component–System / ECS)**, **Flyweight** (shared mesh/texture/sprite assets), **Object Pool** (bullets/particles avoiding GC pauses), **State** (Player `Grounded`, `Jumping`, `Stunned`).

## Key Design Insight
Deep OOP hierarchies (`FlyingArmoredShootingEnemy extends ShootingEnemy`) collapse in games. **ECS** composes entities from pure data components (`Position`, `Velocity`, `Health`) processed by cache-friendly Systems, while **Object Pool** + **Flyweight** eliminate per-frame allocations.
