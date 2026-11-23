# Asynchronous JavaScript & Promises Cheatsheet

> Quick-reference guide covering Promise states, combinator behaviors (`all`, `allSettled`, `race`, `any`), `withResolvers`, `async/await` patterns, and `AbortSignal` timeouts.

| Metadata | Details |
| :--- | :--- |
| **Track** | Reference Cheatsheets (`27_cheatsheets`) |
| **Baseline** | ECMAScript 2024 / Node.js 22 LTS |
| **Level** | Reference / Quick Lookup |

---

## 1. Promise Combinators Comparison Table

| Combinator | Fulfills When... | Rejects When... | Short-Circuits? | Return Type |
| :--- | :--- | :--- | :--- | :--- |
| `Promise.all` | **All** inputs fulfill | **Any** input rejects | Yes (on 1st rejection) | `Array<values>` |
| `Promise.allSettled` | **All** inputs settle | **Never** rejects | No | `Array<{ status, value/reason }>` |
| `Promise.race` | **First** input settles | **First** input settles | Yes (on 1st settlement) | `value` or `reason` of fastest |
| `Promise.any` | **First** input fulfills | **All** inputs reject | Yes (on 1st fulfillment) | `value` of fastest; throws `AggregateError` if all fail |

---

## 2. Promise Construction & Modern Utilities (ES2024+)

### `Promise.withResolvers()`
Extracts the promise and its resolver closures into a single flat object without nesting constructors:

```javascript
// Modern ES2024:
const { promise, resolve, reject } = Promise.withResolvers();

// Usage in event listeners or streams:
button.addEventListener('click', resolve, { once: true });
const result = await promise;
```

---

## 3. Asynchronous Cancellation & Timeouts

```javascript
// AbortSignal.timeout (Modern built-in timer cancellation)
try {
  const res = await fetch('https://api.example.com/data', {
    signal: AbortSignal.timeout(3000) // Rejects with TimeoutError after 3s
  });
  const data = await res.json();
} catch (err) {
  if (err.name === 'TimeoutError') {
    console.error('Request timed out after 3000ms');
  }
}

// AbortSignal.any (Combine multiple cancellation triggers)
const userCancel = new AbortController();
const combinedSignal = AbortSignal.any([
  userCancel.signal,
  AbortSignal.timeout(5000)
]);
```

---

## 4. Async Iteration & Generators

```javascript
// Consuming ReadableStreams or async iterables:
async function processStream(asyncIterable) {
  for await (const chunk of asyncIterable) {
    console.log('Received chunk:', chunk);
  }
}

// Defining Async Generator:
async function* fetchPages(url) {
  let nextUrl = url;
  while (nextUrl) {
    const res = await fetch(nextUrl);
    const data = await res.json();
    yield data.items;
    nextUrl = data.nextPageUrl;
  }
}
```
