# SOLID Principles — One-Page Cheatsheet

| Letter | Principle | One-Line Meaning | Classic Violation | Fix |
| :--- | :--- | :--- | :--- | :--- |
| **S** | Single Responsibility | 1 actor / 1 reason to change | `OrderManager` doing SQL + PDF + Email | Split into `OrderRepo`, `PdfExporter`, `Notifier` |
| **O** | Open/Closed | Extend without modifying core | Giant `switch (paymentType)` | Strategy / Plugin Registry |
| **L** | Liskov Substitution | Subtypes must honor base contracts | `Square extends Rectangle` or `UnsupportedOperationException` | Fix hierarchy or use composition |
| **I** | Interface Segregation | Small, client-specific interfaces | Fat `Worker` forcing `Robot.eat()` | Split into `Workable` + `Feedable` |
| **D** | Dependency Inversion | Depend on abstractions, not concretions | `OrderSvc` instantiating `new MySqlRepo()` | Inject `OrderRepo` port interface |
