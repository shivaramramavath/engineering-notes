# Structural Patterns Q&A

<details><summary>1. Compare Adapter, Facade, Decorator, and Proxy.</summary>
**Adapter** translates an incompatible interface. **Facade** simplifies a complex subsystem behind a clean API. **Decorator** stacks extra behavior onto the same interface. **Proxy** controls access (lazy init, auth, remote RPC) behind the same interface.
</details>

<details><summary>2. Why use Decorator instead of inheritance for combinations (e.g., Compression + Encryption + Buffering)?</summary>
Inheritance causes a combinatorial class explosion ($2^N$ subclasses); Decorators compose dynamically at runtime (`new Buffered(new Encrypted(new FileStream()))`).
</details>

<details><summary>3. How does Composite simplify tree structures?</summary>
Leaf (`File`) and Container (`Folder`) implement the same `Node` interface (`size()`), so callers treat single items and nested trees uniformly without `instanceof` checks.
</details>

<details><summary>4. When is Bridge better than simple Strategy?</summary>
When **two independent hierarchies** (e.g., `Shape` × `Renderer`, or `NotificationType` × `DeliveryChannel`) must vary independently without $M \times N$ subclasses.
</details>

<details><summary>5. Give two examples of Proxy in modern frameworks.</summary>
Spring `@Transactional` / `@Cacheable` dynamic proxies, Hibernate lazy entity proxies, and JavaScript `Proxy` (Vue 3 reactivity / Immer).
</details>
