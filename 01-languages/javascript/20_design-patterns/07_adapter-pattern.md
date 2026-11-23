# The Adapter Pattern in JavaScript

> Interface harmonization, third-party API wrappers, legacy bridging, and asynchronous promisification adapters.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate to Advanced |
| **Prerequisites** | [Objects](file:///e:/Learning/engineering-notes/01-languages/javascript/03_objects-and-arrays/01_object-fundamentals.md), [Classes](file:///e:/Learning/engineering-notes/01-languages/javascript/05_this-and-oop/05_classes.md) |
| **Time to read** | 22 minutes |
| **Status** | Complete |
| **Standard / Runtime** | ECMAScript 2015+, Node.js & Evergreen Browsers |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals](#9-engine-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

In software engineering, you frequently encounter libraries, third-party SDKs, or legacy systems whose public interface does not match the interface expected by your application. For example, your application architecture might expect all analytics trackers to implement `track(eventName, properties)`, but a new vendor SDK provides `logCustomEvent(name, dictionary, timestamp)`.

Modifying the vendor SDK is impossible or impractical, and rewriting your entire application codebase to match the vendor's signature pollutes business logic with vendor-specific coupling.

The **Adapter Pattern** (also known as a **Wrapper**) acts as an intermediary bridge between two incompatible interfaces. It wraps an existing object (the **Adaptee**) and exposes the interface expected by the client (the **Target**), translating calls, remapping parameters, and converting data types transparently.

---

## 2. Core Concepts

### Core Components
1. **Target Interface**: The standardized contract that client application code expects to interact with.
2. **Adaptee**: The existing, third-party, or legacy object with an incompatible interface.
3. **Adapter**: The wrapper object that implements the Target interface and translates incoming calls into method invocations on the Adaptee.
4. **Client**: The calling application code that consumes the Target interface.

### Object Adapter vs Class Adapter
- **Object Adapter**: Uses object composition. The Adapter holds an instance of the Adaptee as a private member. This is the idiomatic pattern in JavaScript.
- **Class Adapter**: Uses multiple inheritance. Because JavaScript only supports single inheritance, Class Adapters subclass the Adaptee and implement the target interface methods.

### Promisification as an Adapter
One of the most ubiquitous native adapters in JavaScript is `node:util.promisify`. It wraps legacy Node.js error-first callback APIs (`fs.readFile(path, (err, data) => {})`) and adapts them into standard ES6 Promises (`await fs.readFile(path)`).

---

## 3. How It Works Internally

```
+--------------------------------------------------------------------------+
| CLIENT APPLICATION CODE                                                  |
| Expects Standard Target Interface:                                       |
| paymentGateway.charge(userId, amountInCents)                             |
+------------------------------------+-------------------------------------+
                                     |
                                     v
+--------------------------------------------------------------------------+
| ADAPTER (StripePaymentAdapter)                                           |
| Implements Target Interface: charge(userId, amountInCents)               |
|                                                                          |
|   1. Translates amountInCents -> dollars ($amount / 100)                 |
|   2. Looks up customer token from userId                                 |
|   3. Calls Adaptee: stripeSDK.charges.create(...)                        |
|   4. Maps response to unified TransactionReceipt                         |
+------------------------------------+-------------------------------------+
                                     |
                                     v
+--------------------------------------------------------------------------+
| ADAPTEE (Vendor SDK: stripeSDK)                                          |
| Incompatible API: charges.create({ customer, amount, currency })         |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & API Reference

### Classic Object Adapter Pattern
```javascript
// Target Interface: { fetchUser(id) => Promise<{ id, name, email }> }

// Incompatible Third-Party Adaptee
class LegacyUserApi {
  getUserRecord(userId, callback) {
    // Legacy XML/callback API
    setTimeout(() => callback(null, { u_id: userId, full_name: 'Ada Lovelace', mail: 'ada@example.com' }), 50);
  }
}

// The Adapter
export class UserApiAdapter {
  constructor(legacyApi = new LegacyUserApi()) {
    this.legacyApi = legacyApi;
  }

  async fetchUser(id) {
    return new Promise((resolve, reject) => {
      this.legacyApi.getUserRecord(id, (err, rawRecord) => {
        if (err) return reject(err);
        // Transform incompatible property keys to match Target interface
        resolve({
          id: rawRecord.u_id,
          name: rawRecord.full_name,
          email: rawRecord.mail,
        });
      });
    });
  }
}
```

---

## 5. Practical Examples

### Example 1: LocalStorage to Async Storage Adapter
```javascript
// Target Interface: AsyncStorage { get(key), set(key, val), remove(key) }
export class LocalStorageAdapter {
  constructor(storage = window.localStorage) {
    this.storage = storage;
  }

  async get(key) {
    const raw = this.storage.getItem(key);
    return raw ? JSON.parse(raw) : null;
  }

  async set(key, value) {
    this.storage.setItem(key, JSON.stringify(value));
  }

  async remove(key) {
    this.storage.removeItem(key);
  }
}
```

### Example 2: Multi-Vendor Analytics Adapter
```javascript
// Unified Target Interface
export class SegmentAnalyticsAdapter {
  constructor(segmentSdk) {
    this.sdk = segmentSdk;
  }

  trackEvent(name, payload = {}) {
    this.sdk.track(name, payload);
  }

  identifyUser(userId, traits = {}) {
    this.sdk.identify(userId, traits);
  }
}

export class GoogleAnalyticsAdapter {
  constructor(gtagFunction = window.gtag) {
    this.gtag = gtagFunction;
  }

  trackEvent(name, payload = {}) {
    this.gtag('event', name, payload);
  }

  identifyUser(userId, traits = {}) {
    this.gtag('set', 'user_properties', { user_id: userId, ...traits });
  }
}
```

### Example 3: Custom Callback-to-Promise Adapter (`promisify`)
```javascript
export function promisifyAdapter(originalFunction) {
  return function (...args) {
    return new Promise((resolve, reject) => {
      originalFunction.call(this, ...args, (err, result) => {
        if (err) reject(err);
        else resolve(result);
      });
    });
  };
}

// Adapting legacy Node.js DNS lookup
import dns from 'node:dns';
const lookupAsync = promisifyAdapter(dns.lookup);
const { address } = await lookupAsync('example.com');
```

### Example 4: Fetch API to Axios-Compatible Interface Adapter
```javascript
export class FetchToAxiosAdapter {
  async get(url, config = {}) {
    return this.#request(url, { ...config, method: 'GET' });
  }

  async post(url, data, config = {}) {
    return this.#request(url, {
      ...config,
      method: 'POST',
      body: JSON.stringify(data),
      headers: { 'Content-Type': 'application/json', ...(config.headers || {}) },
    });
  }

  async #request(url, options) {
    const res = await fetch(url, options);
    const data = await res.json();

    // Map fetch Response into Axios-like response format
    return {
      data,
      status: res.status,
      statusText: res.statusText,
      headers: Object.fromEntries(res.headers.entries()),
    };
  }
}
```

### Example 5: Date Library Adapter (Luxon/Dayjs to Native Interface)
```javascript
// Adapt third-party Day.js into standardized app date contract
export class DateFormatterAdapter {
  constructor(dayjsLibrary) {
    this.dayjs = dayjsLibrary;
  }

  formatIso(dateInput) {
    return this.dayjs(dateInput).toISOString();
  }

  formatHuman(dateInput) {
    return this.dayjs(dateInput).format('MMMM D, YYYY');
  }

  isPast(dateInput) {
    return this.dayjs(dateInput).isBefore(this.dayjs());
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Leaking Adaptee Exceptions**: If the Adaptee throws vendor-specific errors (e.g. `StripeCardError` or `AWS.RequestTimeoutException`), allowing them to bubble up unhandled breaks the abstraction. The Adapter should intercept Adaptee exceptions and re-throw standardized domain errors (`PaymentFailedError`).
2. **Imperfect Interface Mappings**: Some Adaptee features cannot be mapped cleanly into the Target interface. Resist the temptation to expose vendor-specific methods directly on the Adapter, as this breaks polymorphism.
3. **Performance Overhead on Binary/Stream Transformations**: When adapting data streams or large binary buffers, converting formats repeatedly in the adapter layer adds CPU latency and memory allocation churn.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Why It Breaks | Correct Pattern |
| :--- | :--- | :--- |
| Exposing Adaptee-specific methods on the Adapter | Leaks vendor coupling to the client | Keep Adapter strictly limited to Target interface |
| Letting Adaptee-specific errors leak out | Clients must know vendor details to handle errors | Catch and re-map to application domain errors |
| Modifying the Adaptee's prototype directly (Monkey Patching) | Introduces global side effects and collision risks | Wrap the Adaptee via composition inside an Adapter |
| Using an Adapter when interfaces already match | Adds unnecessary layer of indirection | Use the original class directly |
| Forgetting to adapt return values | Client receives vendor-specific object layouts | Transform input parameters AND output responses |

---

## 8. Performance Characteristics

- **Method Call Indirection**: A modern JavaScript adapter call adds approximately 1 to 2 nanoseconds of function invocation overhead in V8.
- **TurboFan Optimization**: Inlining heuristics in TurboFan frequently inline simple adapter wrappers directly into calling code, completely eliminating the wrapper overhead.

---

## 9. Engine Internals

1. **Monomorphic Delegation**:
   - When an Adapter delegates `this.adaptee.doWork(arg)`, V8 optimizes the delegation as a monomorphic property lookup if `this.adaptee` retains a consistent Hidden Class.

---

## 10. Best Practices & Design Guidelines

- **Define the Target Interface First**: Design the cleanest, most idiomatic interface for your application domain before writing the adapter.
- **Keep Adapters Thin**: An Adapter's sole responsibility is translation. Business logic should reside in services, not inside adapters.
- **Combine with Factory Pattern**: Use a Factory to instantiate the appropriate Adapter based on runtime configuration.

---

## 11. Real-World Case Study

### Swapping a Cloud Queue Service: SQS to RabbitMQ
```javascript
// Target Interface: IMessageQueue { publish(queue, message), poll(queue, handler) }

// Adapter for AWS SQS
export class SqsQueueAdapter {
  constructor(sqsClient) {
    this.sqs = sqsClient;
  }

  async publish(queueUrl, message) {
    await this.sqs.sendMessage({
      QueueUrl: queueUrl,
      MessageBody: JSON.stringify(message),
    });
  }

  async poll(queueUrl, handler) {
    const res = await this.sqs.receiveMessage({ QueueUrl: queueUrl, MaxNumberOfMessages: 1 });
    if (res.Messages?.[0]) {
      const msg = res.Messages[0];
      await handler(JSON.parse(msg.Body));
      await this.sqs.deleteMessage({ QueueUrl: queueUrl, ReceiptHandle: msg.ReceiptHandle });
    }
  }
}

// Adapter for RabbitMQ (amqplib)
export class RabbitMqAdapter {
  constructor(channel) {
    this.channel = channel;
  }

  async publish(queueName, message) {
    await this.channel.assertQueue(queueName);
    this.channel.sendToQueue(queueName, Buffer.from(JSON.stringify(message)));
  }

  async poll(queueName, handler) {
    await this.channel.assertQueue(queueName);
    this.channel.consume(queueName, async (msg) => {
      if (msg) {
        await handler(JSON.parse(msg.content.toString()));
        this.channel.ack(msg);
      }
    });
  }
}
```

---

## 12. Interview Questions

<details>
<summary>1. What is the intent of the Adapter Pattern and when should it be used?</summary>
The Adapter pattern allows classes with incompatible interfaces to collaborate by wrapping an existing class with a new interface. It is used when integrating third-party libraries, legacy code, or external APIs whose method signatures and data formats do not match the interface expected by your application.
</details>

<details>
<summary>2. What is the difference between an Object Adapter and a Class Adapter?</summary>
An <b>Object Adapter</b> uses object composition: the adapter holds a reference to the adaptee instance and delegates calls to it. A <b>Class Adapter</b> uses multiple inheritance to inherit both the target interface and the adaptee implementation. Because JavaScript supports only single inheritance, Object Adapters are the standard approach.
</details>

<details>
<summary>3. How does the Adapter pattern differ from the Decorator and Proxy patterns?</summary>
- <b>Adapter</b>: Changes the interface of an object to make it compatible with a client.
- <b>Decorator</b>: Keeps the same interface but dynamically enhances or adds new behavior to the object.
- <b>Proxy</b>: Keeps the exact same interface but controls or guards access to the underlying object (e.g. caching, lazy loading, logging).
</details>

<details>
<summary>4. How does util.promisify in Node.js act as an Adapter?</summary>
<code>util.promisify</code> takes a function following the Node.js error-first callback contract <code>(args, (err, val) => {})</code> and adapts its interface to return a standard ECMAScript Promise, allowing it to be used with <code>await</code> without altering the original function's implementation.
</details>

<details>
<summary>5. Why should an Adapter catch and translate Adaptee-specific errors?</summary>
If the Adaptee throws proprietary errors, the client application must import vendor-specific error classes to catch them, breaking the decoupling goal of the Adapter. Adapters should catch vendor errors and re-throw application domain-specific errors.
</details>

<details>
<summary>6. Can an Adapter adapt multiple Adaptees into a single Target interface?</summary>
Yes. An adapter can coordinate and map data across multiple underlying services (e.g., combining a User Profile API and an Authentication API) into a single unified client interface.
</details>

<details>
<summary>7. What is the risk of "Two-Way Adapters"?</summary>
A Two-Way Adapter implements both interfaces simultaneously, allowing it to be used as either the Target or the Adaptee. While flexible, it often leads to high complexity and tight coupling between the two systems.
</details>

<details>
<summary>8. How do polyfills relate to the Adapter pattern?</summary>
A polyfill is a specialized adapter that adapts older, non-compliant browser runtimes to support modern standardized ECMAScript or Web API interfaces (e.g., polyfilling <code>Array.prototype.at</code>).
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Build a Legacy Callback-to-Async Cache Adapter
Implement an adapter `AsyncCacheAdapter` that wraps a legacy callback-based cache object (`{ read(k, cb), write(k, v, cb) }`) into an asynchronous promise-based interface (`{ get(k), set(k, v) }`).

<details>
<summary>View Solution</summary>

```javascript
export class AsyncCacheAdapter {
  constructor(legacyCache) {
    this.legacyCache = legacyCache;
  }

  get(key) {
    return new Promise((resolve, reject) => {
      this.legacyCache.read(key, (err, data) => {
        if (err) reject(err);
        else resolve(data);
      });
    });
  }

  set(key, value) {
    return new Promise((resolve, reject) => {
      this.legacyCache.write(key, value, (err) => {
        if (err) reject(err);
        else resolve();
      });
    });
  }
}
```
</details>

### Exercise 2: Geolocation Coordinates Adapter
Implement an adapter `GeoCoordAdapter` that accepts coordinates in various formats (e.g. `[lat, lng]`, `{ latitude, longitude }`, or `"lat,lng"` string) and normalizes them into a consistent `{ lat: number, lng: number }` object.

<details>
<summary>View Solution</summary>

```javascript
export class GeoCoordAdapter {
  static normalize(raw) {
    // Format 1: Array [lat, lng]
    if (Array.isArray(raw)) {
      return { lat: Number(raw[0]), lng: Number(raw[1]) };
    }

    // Format 2: String "40.7128,-74.0060"
    if (typeof raw === 'string') {
      const [lat, lng] = raw.split(',').map(s => Number(s.trim()));
      return { lat, lng };
    }

    // Format 3: Object with full keys { latitude, longitude }
    if (raw && typeof raw === 'object') {
      const lat = raw.lat ?? raw.latitude;
      const lng = raw.lng ?? raw.longitude;
      if (lat !== undefined && lng !== undefined) {
        return { lat: Number(lat), lng: Number(lng) };
      }
    }

    throw new TypeError('Invalid coordinate format');
  }
}
```
</details>
