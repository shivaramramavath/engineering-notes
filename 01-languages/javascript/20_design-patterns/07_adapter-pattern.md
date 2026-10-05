# Adapter Pattern

An adapter wraps something with an **incompatible interface** and exposes the interface your code expects. Callers talk to the adapter; the adapter translates to the underlying library, API, or legacy code.

It matters most at boundaries: third-party SDKs, external HTTP APIs, legacy modules, and platform differences (browser vs Node). Keeping the translation in one place means a vendor change touches one file, not the whole codebase.

**Prerequisites:** [Classes](../05_this-and-oop/05_classes.md), [Promises](../11_asynchronous-javascript/03_promises.md), [Callbacks](../11_asynchronous-javascript/02_callbacks.md)

---

## The Shape of the Problem

Your app wants this:

```js
// what the app expects
gateway.charge({ amountCents: 1500, currency: "USD", token: "tok_123" });
// → Promise<{ id: string, status: "paid" | "failed" }>
```

The existing code you have to use looks like this:

```js
// legacy module you can't (or shouldn't) change
legacyPay.makePayment(15.0, "tok_123", (err, receipt) => {
  // receipt: { receiptNo: "R-9", ok: true }
});
```

Different argument order, different units, a callback instead of a promise, a different result shape. Adapting once beats translating at every call site.

---

## Writing the Adapter

```js
class LegacyPayAdapter {
  #legacy;

  constructor(legacy) {
    this.#legacy = legacy;
  }

  charge({ amountCents, currency, token }) {
    if (currency !== "USD") {
      return Promise.reject(new Error(`Unsupported currency: ${currency}`));
    }

    return new Promise((resolve, reject) => {
      this.#legacy.makePayment(amountCents / 100, token, (err, receipt) => {
        if (err) return reject(err);
        resolve({ id: receipt.receiptNo, status: receipt.ok ? "paid" : "failed" });
      });
    });
  }
}

const gateway = new LegacyPayAdapter(legacyPay);
const result = await gateway.charge({ amountCents: 1500, currency: "USD", token: "tok_123" });
```

What the adapter does here: converts units, reorders arguments, turns a callback into a promise, normalizes the result, and rejects cases the legacy code can't handle. It does **not** contain business rules like "retry when the card is declined". That belongs in the caller or a decorator ([Decorator](./08_decorator-pattern.md)).

For plain Node-style callbacks, `util.promisify` does the callback-to-promise part for you:

```js
import { promisify } from "node:util";
const readAsync = promisify(legacyFs.readFile);
```

---

## Adapting Platform Differences

A storage interface with interchangeable backends is a common and useful adapter use.

```js
// the interface the app depends on: get(key), set(key, value), remove(key)
const localStorageAdapter = {
  get: (k) => JSON.parse(localStorage.getItem(k) ?? "null"),
  set: (k, v) => localStorage.setItem(k, JSON.stringify(v)),
  remove: (k) => localStorage.removeItem(k),
};

function memoryAdapter() {
  const map = new Map();
  return {
    get: (k) => map.get(k) ?? null,
    set: (k, v) => void map.set(k, v),
    remove: (k) => void map.delete(k),
  };
}
```

The app depends on `get/set/remove`. In the browser you pass `localStorageAdapter`; in tests or on the server you pass `memoryAdapter()`. This also makes [Dependency Injection](./09_dependency-injection.md) straightforward.

---

## Adapter vs Similar Patterns

All three wrap another object. The difference is intent.

| Pattern | Interface of the wrapper | Purpose |
| --- | --- | --- |
| Adapter | **Different** from the wrapped one | Make something fit an expected interface |
| [Decorator](./08_decorator-pattern.md) | **Same** as the wrapped one | Add behavior (logging, caching) |
| Facade | New, simpler | Hide a complex subsystem behind a few calls |

---

## Common Mistakes

- **Leaking the underlying shape.** If the adapter returns the vendor's raw response, callers start depending on vendor fields and the adapter protects nothing. Map to your own types.
- **Business logic inside the adapter.** Keep it a translator.
- **Lowest-common-denominator interfaces.** When adapting two providers, design the interface around what your app needs, and decide explicitly how to handle features only one provider has.
- **Losing errors.** Convert vendor errors into your own error types but keep the original as `cause` (`new Error("charge failed", { cause: err })`). See [Custom Errors](../10_error-handling/03_custom-errors.md).
- **Adapting too early.** One provider with no plans to change it doesn't need an adapter, unless the legacy interface is genuinely awkward.

---

## Quick Summary

- An adapter translates one interface into another so the rest of your code only sees the interface it wants.
- Typical jobs: callback → promise, unit/shape conversion, swapping backends.
- Keep it thin: no business rules, no vendor types leaking out.
- Adapter changes the interface, decorator keeps it, facade simplifies it.

**Next:** [Decorator Pattern](./08_decorator-pattern.md)