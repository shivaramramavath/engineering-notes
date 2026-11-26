# Abstract Factory

**Intent:** provide an interface for creating **families of related objects** without specifying their concrete classes.

Use it when several products must be used *together* and must be consistent with each other: all from the same family.

## The problem

An application supports two UI themes. Each theme has its own button and checkbox. Mixing a light button with a dark checkbox would look wrong, so the code must guarantee that products come from the same family.

Without the pattern:

```java
Button b;
Checkbox c;
if (theme == LIGHT) { b = new LightButton(); c = new LightCheckbox(); }
else                { b = new DarkButton();  c = new DarkCheckbox(); }
```

This repeats everywhere, and it's easy to mix families by accident.

## Structure

```text
              UiFactory (interface)
          + createButton(): Button
          + createCheckbox(): Checkbox
                  ▲
        ┌─────────┴─────────┐
 LightUiFactory        DarkUiFactory
   creates:               creates:
   LightButton            DarkButton
   LightCheckbox          DarkCheckbox

Client uses only: UiFactory, Button, Checkbox
```

## Example

```java
public interface Button   { void render(); }
public interface Checkbox { void render(); }

public interface UiFactory {
    Button createButton();
    Checkbox createCheckbox();
}

public class LightButton   implements Button   { public void render() { /* light style */ } }
public class LightCheckbox implements Checkbox { public void render() { /* light style */ } }
public class DarkButton    implements Button   { public void render() { /* dark style */ } }
public class DarkCheckbox  implements Checkbox { public void render() { /* dark style */ } }

public class LightUiFactory implements UiFactory {
    public Button createButton()     { return new LightButton(); }
    public Checkbox createCheckbox() { return new LightCheckbox(); }
}

public class DarkUiFactory implements UiFactory {
    public Button createButton()     { return new DarkButton(); }
    public Checkbox createCheckbox() { return new DarkCheckbox(); }
}
```

The client never names a concrete class:

```java
public class SettingsScreen {
    private final Button saveButton;
    private final Checkbox notifyCheckbox;

    public SettingsScreen(UiFactory factory) {
        this.saveButton = factory.createButton();
        this.notifyCheckbox = factory.createCheckbox();
    }

    public void render() {
        saveButton.render();
        notifyCheckbox.render();
    }
}

UiFactory factory = theme == Theme.DARK ? new DarkUiFactory() : new LightUiFactory();
new SettingsScreen(factory).render();
```

The family is chosen **once** at the composition root. Everything created from it is guaranteed to match.

## A more realistic case

A payment module that must use one provider's classes together:

```java
public interface PaymentFactory {
    PaymentProcessor createProcessor();
    RefundHandler createRefundHandler();
    WebhookVerifier createWebhookVerifier();
}
```

`StripePaymentFactory` and `PaypalPaymentFactory` each return objects that work with the same provider's API and credentials. Switching provider means changing which factory is wired in.

## Abstract Factory vs Factory Method

| | Factory Method | Abstract Factory |
|---|---|---|
| Creates | One product | A family of related products |
| Mechanism | One overridable method | An object with several creation methods |
| Typical size | Small | Larger |
| Adds a new product *variant* | Easy | Easy (a new factory class) |
| Adds a new product *type* | Easy | **Hard**: every factory must change |

Each method inside an abstract factory is often implemented as a factory method.

## Trade-offs

| Benefits | Costs |
|---|---|
| Guarantees products in a family are compatible | Many classes: interface, factories, product variants |
| Swap an entire family in one place | Adding a new *kind* of product changes the interface and every factory |
| Client is isolated from concrete classes | Overkill when there is only one family |

## When to use it

- There are two or more *families* and products must be used together.
- The family is chosen at runtime or by configuration (theme, provider, platform, database dialect).
- You want to enforce consistency.

Skip it when there's a single family, or only one product type, where [Factory Method](./02-factory-method.md) or a plain factory is enough.

## Common mistakes

- **Using it with one family**, adding layers for no benefit.
- **Product interfaces that differ so much** the family relationship is artificial.
- **Letting the client create products directly** and bypass the factory.
- **Ignoring the cost of adding a product type**: every factory must be updated. Choose the axis of change carefully.
- **A single "god factory"** creating unrelated products.

## Quick Summary

- Abstract Factory creates families of related objects through one interface.
- It guarantees consistency, and swapping the family is a one-line change at the composition root.
- Easy to add new families, hard to add new product types.
- Use only when real families exist; otherwise prefer Factory Method.

## Related

[Factory Method](./02-factory-method.md) · [Bridge](../05-structural-patterns/02-bridge.md) · [Dependency injection](../03-patterns-intro/02-dependency-injection.md)

## Next

[04-builder.md](./04-builder.md)
