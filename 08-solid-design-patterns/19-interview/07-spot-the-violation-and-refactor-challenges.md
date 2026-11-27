# Spot the Violation & Refactor Challenges

### Challenge 1
```java
class Report {
    void calculateTotals() { /* ... */ }
    void saveToPostgres() { /* ... */ }
    String toHtml() { return "<html/>"; }
}
```
<details><summary>Solution</summary>
**Violates SRP** (3 actors: accounting, DBA, UI). Extract `ReportCalculator`, `ReportRepository`, and `HtmlReportFormatter`.
</details>

### Challenge 2
```ts
function pay(method: string, amt: number) {
  if (method === 'CARD') { /* ... */ }
  else if (method === 'PAYPAL') { /* ... */ }
  else if (method === 'CRYPTO') { /* ... */ }
}
```
<details><summary>Solution</summary>
**Violates OCP** when new methods are added frequently. Refactor to a **Strategy** map / `PaymentMethodRegistry`.
</details>

### Challenge 3
```python
def apply_discount(order):
    if order.get_customer().get_tier().get_name() == "VIP":
        order.set_total(order.get_total() * 0.8)
```
<details><summary>Solution</summary>
**Violates Law of Demeter & Tell, Don't Ask** (train wreck + anemic mutation). Use `order.apply_vip_discount()`.
</details>
