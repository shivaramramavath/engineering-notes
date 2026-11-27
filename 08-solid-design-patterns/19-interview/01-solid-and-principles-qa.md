# SOLID & Design Principles Q&A

<details><summary>1. Explain SRP without saying "a class should do one thing."</summary>
A module/class should have **one reason to change** (one actor/stakeholder). A method doing 3 cohesive steps for the same business capability doesn't violate SRP; mixing SQL, PDF formatting, and pricing rules in one class does.
</details>

<details><summary>2. How does OCP work in practice without editing existing code?</summary>
Program to an interface/function type (Strategy, Decorator, Plugin Registry) so adding a new variant means adding a new class/function rather than editing a central `switch`.
</details>

<details><summary>3. Give a real-world Liskov Substitution Principle (LSP) violation.</summary>
`ReadOnlyList` extending `MutableList` and throwing `UnsupportedOperationException` on `.add()`, or `Square extends Rectangle` breaking `r.setWidth(5); r.setHeight(4); assert r.area() == 20`.
</details>

<details><summary>4. Why does ISP matter even if a class implements an unused method as a no-op?</summary>
Fat interfaces force clients to depend on methods they don't use (causing unnecessary recompiles/mocks) and signal poor cohesion.
</details>

<details><summary>5. Explain DIP vs Dependency Injection.</summary>
**DIP** is the architectural principle that high-level policy owns the interface (`OrderRepo`) and low-level details implement it. **DI** is the wiring technique (constructor/function injection) that supplies the implementation at runtime.
</details>

<details><summary>6. When does DRY become harmful?</summary>
When two pieces of code look syntactically similar today (e.g., two DTOs or validations in different bounded contexts) and are prematurely merged—creating tight coupling between unrelated features. Follow the **Rule of Three**.
</details>

<details><summary>7. Explain Tell, Don't Ask and the Law of Demeter.</summary>
**Tell, Don't Ask**: call `order.ship()` instead of pulling `order.getStatus()` and mutating it externally. **Law of Demeter**: avoid train wrecks (`a.getB().getC().doX()`)—talk only to immediate collaborators.
</details>
