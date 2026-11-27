# Anti-Patterns & Smells — Quick Reference

| Smell / Anti-Pattern | 1-Line Fix |
| :--- | :--- |
| **God Object** | Extract Class by cohesion clusters |
| **Spaghetti Code** | Guard Clauses + Extract Method |
| **Primitive Obsession** | Value Objects (`Money`, `Email`, `UserId`) |
| **Feature Envy** | Move Method onto the data owner |
| **Divergent Change** | Extract Class (1 actor per class) |
| **Shotgun Surgery** | Move Method / Inline Class |
| **Anemic Domain Model** | Replace `setStatus()` with domain verbs (`pay()`, `ship()`) |
| **Singleton / Service Locator Abuse** | Constructor / Functional DI |
| **Deep Inheritance** | Replace with Composition / Strategy / Decorator |
| **Big Ball of Mud** | Modular Monolith + CI Fitness Functions + Strangler Fig |
