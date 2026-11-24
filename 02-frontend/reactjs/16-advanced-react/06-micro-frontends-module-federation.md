# 06 — Micro-Frontends & Module Federation with React 19

## Metadata
- **React Version**: 19.0.0+
- **Prerequisites**: [03-code-splitting-and-lazy-loading.md](../14-performance/03-code-splitting-and-lazy-loading.md), Webpack / Rspack / Vite Bundler Architecture
- **Difficulty**: Expert
- **Estimated Read Time**: 29 minutes
- **Tags**: `micro-frontends`, `module-federation`, `rspack`, `webpack-5`, `vite-federation`, `architecture`

---

## Architecture Diagram

The diagram below illustrates a production Micro-Frontend architecture powered by **Module Federation**: a Host shell application dynamically loads federated Remote applications (Billing, Analytics, User Management) at runtime while sharing a single shared instance of `react` and `react-dom` to prevent duplicate runtime crashes.

```mermaid
flowchart TD
    subgraph HostContainer["Host Application (Shell / Portal)"]
        Nav["Persistent App Shell & Navigation"]
        HostRouter["Global Router (React Router v7)"]
        SharedRegistry["Module Federation Shared Scope: [react, react-dom, tanstack-query]"]
        
        Nav --> HostRouter
        HostRouter --> Boundary["Scoped ErrorBoundary + Suspense"]
    end

    subgraph RemoteBilling["Remote App 1: Billing (Team Alpha)"]
        RemoteEntry1["remoteEntry.js (Manifest)"]
        CheckoutModule["./CheckoutWidget (Exposed Component)"]
        RemoteEntry1 --> CheckoutModule
    end

    subgraph RemoteAnalytics["Remote App 2: Analytics (Team Beta)"]
        RemoteEntry2["remoteEntry.js (Manifest)"]
        MetricsModule["./MetricsDashboard (Exposed Component)"]
        RemoteEntry2 --> MetricsModule
    end

    Boundary -->|Dynamic Import: import('billing/CheckoutWidget')| RemoteEntry1
    Boundary -->|Dynamic Import: import('analytics/MetricsDashboard')| RemoteEntry2

    SharedRegistry -.->|Enforces Singleton React Instance| CheckoutModule
    SharedRegistry -.->|Enforces Singleton React Instance| MetricsModule
```

---

## Concept Overview

As enterprise engineering organizations grow to dozens or hundreds of developers, maintaining a massive monolithic React repository introduces friction:
- **Deployment Coupling**: A bug in the settings tab blocks deployment of a critical checkout fix.
- **CI/CD Bottlenecks**: Monolith test and build pipelines stretch to 45+ minutes.
- **Organizational Friction**: Multiple autonomous teams step on each other's code and dependencies.

**Micro-Frontends** decompose the frontend into autonomous applications owned by independent teams:
- **Module Federation (Webpack 5 / Rspack / Vite)**: The industry standard mechanism for micro-frontends. It allows an application to dynamically load code from another independent build over HTTP at runtime.
- **Host (Consumer)**: The root container shell providing navigation, authentication, and layout.
- **Remote (Producer)**: An independently deployed micro-app exposing specific components or routes.
- **Shared Dependencies**: Critical libraries (`react`, `react-dom`, `@tanstack/react-query`) are marked as **singletons**. The host and remotes share one running instance in browser memory, avoiding 5MB duplicate React bundles and the fatal `"Hooks can only be called inside the body of a function component"` error.

---

## Practical Implementation

### Example 1: Remote Application Configuration (Rspack / Webpack 5)

Configuring a Remote micro-app that exposes a `./UserProfileCard` component and shares React as a singleton.

```ts
// remote-user/rspack.config.ts (or webpack.config.js)
import { defineConfig } from '@rspack/cli';
import { ModuleFederationPlugin } from '@rspack/core';

export default defineConfig({
  plugins: [
    new ModuleFederationPlugin({
      name: 'remoteUser',
      filename: 'remoteEntry.js',
      exposes: {
        // Expose component to outside consumers
        './UserProfileCard': './src/components/UserProfileCard.tsx',
      },
      shared: {
        react: {
          singleton: true,
          requiredVersion: '^19.0.0',
          eager: false,
        },
        'react-dom': {
          singleton: true,
          requiredVersion: '^19.0.0',
          eager: false,
        },
      },
    }),
  ],
});
```

*Expected Verification Output:*
```text
Build generates dist/remoteEntry.js (lightweight container manifest).
Exposes './UserProfileCard' with strict React 19 singleton contract.
```

---

### Example 2: Host Shell Configuration

Configuring the Host application to consume the Remote application's components.

```ts
// host-shell/rspack.config.ts
import { defineConfig } from '@rspack/cli';
import { ModuleFederationPlugin } from '@rspack/core';

export default defineConfig({
  plugins: [
    new ModuleFederationPlugin({
      name: 'hostShell',
      remotes: {
        // Points to deployed remoteEntry.js URL
        remoteUser: 'remoteUser@http://localhost:3002/remoteEntry.js',
      },
      shared: {
        react: {
          singleton: true,
          requiredVersion: '^19.0.0',
        },
        'react-dom': {
          singleton: true,
          requiredVersion: '^19.0.0',
        },
      },
    }),
  ],
});
```

*Expected Verification Output:*
```text
Host application builds with remote alias 'remoteUser' mapped to localhost:3002.
Shared scope ensures Host provides React singleton to Remote.
```

---

### Example 3: Consuming Federated Remote Components with Suspense and Error Boundaries

Federated modules load over the network. If the remote server is down, an Error Boundary prevents the entire host application from crashing.

```tsx
// host-shell/src/components/UserSection.tsx
import React, { Suspense, lazy } from 'react';
import { ErrorBoundary } from 'react-error-boundary';

// TypeScript declaration for federated remote module
// declare module 'remoteUser/UserProfileCard';
const RemoteUserProfileCard = lazy(() => import('remoteUser/UserProfileCard'));

function RemoteFallbackError({ resetErrorBoundary }: { resetErrorBoundary: () => void }) {
  return (
    <div className="p-4 border border-amber-300 bg-amber-50 rounded text-amber-800">
      <h4 className="font-bold">User Service Temporarily Unavailable</h4>
      <p className="text-sm my-2">Could not connect to the remote User micro-frontend.</p>
      <button
        onClick={resetErrorBoundary}
        className="px-3 py-1 bg-amber-600 text-white rounded text-xs hover:bg-amber-700"
      >
        Retry Connection
      </button>
    </div>
  );
}

export function UserSection({ userId }: { userId: string }) {
  return (
    <div className="my-6">
      <h2 className="text-xl font-bold mb-3">Account Overview</h2>

      <ErrorBoundary FallbackComponent={RemoteFallbackError}>
        <Suspense fallback={<div className="p-4 border rounded animate-pulse">Loading remote user widget...</div>}>
          <RemoteUserProfileCard userId={userId} />
        </Suspense>
      </ErrorBoundary>
    </div>
  );
}
```

*Expected Verification Output:*
```text
Host renders Account Overview header.
Fetches http://localhost:3002/remoteEntry.js and UserProfileCard.js.
If remote is online -> Component renders seamlessly.
If remote returns 500 or timeout -> ErrorBoundary renders clean fallback without crashing navigation or layout.
```

---

### Example 4: Dynamic Remote Loading at Runtime (Multi-Environment URLs)

In production, remote URLs vary by environment (dev, staging, prod) or tenant. Dynamic remote loading injects remotes at runtime without rebuilding the host.

```tsx
// host-shell/src/utils/dynamicFederation.ts
import { loadRemote } from '@module-federation/runtime';
import React, { lazy } from 'react';

// Dynamically loads a component from a remote URL determined at runtime
export function dynamicImportRemote<T = any>(
  remoteName: string,
  remoteUrl: string,
  moduleName: string
) {
  return lazy(async () => {
    // 1. Initialize or register remote container dynamically
    const container = await loadRemote(`${remoteName}/${moduleName}`, {
      from: 'runtime',
    });
    return container as { default: React.ComponentType<T> };
  });
}
```

*Expected Verification Output:*
```text
Host loads remote micro-app from dynamic CDN URL (e.g. https://cdn.company.com/remotes/v2.1/remoteEntry.js).
Zero environment-specific rebuilds required.
```

---

### Example 5: Cross-Micro-Frontend Event Communication via Custom Events

Remotes and hosts should avoid tightly coupled props or shared memory stores. Decoupled custom browser events enable cross-remote messaging.

```tsx
// Remote Component (Analytics Widget)
export function CheckoutCompleteButton({ orderId }: { orderId: string }) {
  const handleClick = () => {
    // Dispatch native custom event on window
    const event = new CustomEvent('app:checkout_completed', {
      detail: { orderId, timestamp: Date.now() },
    });
    window.dispatchEvent(event);
  };

  return (
    <button onClick={handleClick} className="btn-checkout">
      Complete Order
    </button>
  );
}

// Host Component (Global Notification Header)
export function GlobalActivityListener() {
  React.useEffect(() => {
    const handleOrder = (e: Event) => {
      const customEvent = e as CustomEvent<{ orderId: string }>;
      alert(`Global Shell: Order completed! ID: ${customEvent.detail.orderId}`);
    };

    window.addEventListener('app:checkout_completed', handleOrder);
    return () => window.removeEventListener('app:checkout_completed', handleOrder);
  }, []);

  return null;
}
```

*Expected Verification Output:*
```text
Clicking button inside federated Remote publishes event across browser window.
Host shell receives event and responds without direct code dependency or shared store coupling.
```

---

## Mental Model & Analogies

### The Microservices of the Browser

- **Monolithic React**: A giant skyscraper where all departments work on the same floor. If the plumbers turn off the main water valve to fix a leak in the 4th-floor restroom, the entire building must evacuate.
- **Module Federation (Micro-Frontends)**: A **shopping mall plaza**:
  - The Host Shell is the **mall corridors, security, and restrooms**.
  - Team Alpha runs the **Bookstore** (`Remote 1`).
  - Team Beta runs the **Coffee Shop** (`Remote 2`).
  - If the coffee shop has an espresso machine malfunction, the bookstore continues selling books normally.
  - All stores share the same municipal power grid and water supply (`shared: { react: { singleton: true } }`).

---

## Under the Hood / Internal Mechanics

### 1. The Shared Scope Negotiation Protocol

When the host and remotes initialize:
1. The host initializes an internal JavaScript object on `window`: `__webpack_share_scopes__.default`.
2. As each container loads, it registers its installed dependencies and version semvers into the shared scope dictionary:
   ```json
   {
     "react": {
       "19.0.0": { "get": () => Promise.resolve(ReactInstance), "loaded": 1 }
     }
   }
   ```
3. When `RemoteUserProfileCard` executes `import React from 'react'`, the Module Federation runtime intercepts the import and checks the shared scope:
   - If a compatible version of `react` is already loaded (marked `loaded: 1`), it reuses that exact module reference!
   - If `singleton: true` is violated (e.g. Host has React 19, Remote insists on React 18 with `strictVersion: true`), Module Federation logs a console warning or throws an error.

---

## Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Breaks | Recommended Solution |
| :--- | :--- | :--- |
| **Omitting `singleton: true` on `react` and `react-dom`**| Loads duplicate React instances into memory; breaks hook state with fatal React error #321. | Always set `singleton: true` and `requiredVersion` for `react` and `react-dom`. |
| **Unbounded CSS Collisions (Global Styles Leakage)** | CSS classes from Remote A override styling in Host or Remote B. | Use Tailwind prefixes, CSS Modules, or CSS `@scope` to isolate remote styles. |
| **Directly Sharing Complex Redux / Zustand Stores**| Creates tight coupling across micro-frontend deployments, recreating the monolith anti-pattern. | Communicate across micro-frontends via custom events, URL query params, or lightweight contexts. |
| **Missing `<ErrorBoundary>` around Remote Imports** | A 404 or network error loading `remoteEntry.js` crashes the entire host application root. | Always wrap federated `lazy()` imports in an `<ErrorBoundary>`. |
| **Synchronous Top-Level Imports of Remotes** | Importing a remote statically (`import Widget from 'remote/Widget'`) executes before the shared scope initializes. | Use asynchronous dynamic imports (`lazy(() => import('remote/Widget'))`). |
| **Different React Major Versions in Singleton Scope** | If Host is on React 19 and Remote uses React 17, runtime behavior breaks. | Align major React versions across teams or isolate legacy versions in IFrames. |

---

## Performance & Optimization

### Preloading Remote Manifests

Avoid network latency when users navigate to a micro-frontend route:
```html
<!-- Preload remoteEntry.js in index.html -->
<link rel="preload" href="https://billing.company.com/remoteEntry.js" as="script" />
```
Preloading the manifest ensures that by the time the user clicks on the "Billing" tab, the federated container metadata is already cached in browser memory.

---

## Edge Cases & Debugging

### Version Skew Warnings: "Unsatisfied version of shared singleton module react"
- **Symptom**: Console warning: `Unsatisfied version 18.2.0 from remoteUser of shared singleton module react (required ^19.0.0)`.
- **Root Cause**: The remote micro-app was built with an older major version of React.
- **Remedy**: Upgrade the remote to React 19 or relax `strictVersion: false` if APIs are compatible.

---

## Ecosystem & Alternative Approaches

- **`@module-federation/enhanced` (Rspack / Webpack)**: Next-generation Module Federation 2.0 with TypeScript type-hinting across remotes and SSR support.
- **Single-SPA**: A framework-agnostic micro-frontend router using JavaScript entry manifests.
- **Micro-Frontends via IFrames**: Complete isolation with zero CSS/JS leakage, but difficult to style and slow to load.

---

## Exercises & Challenges

<details>
<summary><strong>Exercise 1: Configure a Shared Singleton Scope for TanStack Query</strong></summary>

**Challenge**: Ensure both the host and remote micro-app share a single `QueryClientProvider` and cache.

```ts
// In shared configuration:
shared: {
  '@tanstack/react-query': {
    singleton: true,
    requiredVersion: '^5.0.0',
    eager: false,
  },
}
```
</details>

<details>
<summary><strong>Exercise 2: Type Declarations for Remote Modules</strong></summary>

**Challenge**: Create a TypeScript declaration file allowing the host project to import remote components with autocomplete.

```ts
// host-shell/src/remotes.d.ts
declare module 'remoteBilling/CheckoutWidget' {
  import React from 'react';
  export interface CheckoutProps {
    cartId: string;
    onSuccess: (orderId: string) => void;
  }
  const CheckoutWidget: React.ComponentType<CheckoutProps>;
  export default CheckoutWidget;
}
```
</details>

---

## Frequently Asked Interview Questions

<details>
<summary><strong>1. What is Module Federation and how does it solve micro-frontend challenges?</strong></summary>

Module Federation is a bundler architecture (introduced in Webpack 5 and supported in Rspack and Vite) that allows a JavaScript application to dynamically import code from another independently built and deployed application over HTTP at runtime. It eliminates the need to publish shared UI components to NPM and re-deploy monolithic applications, enabling truly independent deployment pipelines.
</details>

<details>
<summary><strong>2. Why is `singleton: true` mandatory for React in Module Federation?</strong></summary>

React relies on a single shared runtime in browser memory to manage its Fiber tree, hooks dispatcher, and event system. If a host and remote load separate instances of React, calling hooks inside a remote component will throw the fatal React error *"Hooks can only be called inside the body of a function component"* because the remote hook dispatcher points to an uninitialized second React instance.
</details>

<details>
<summary><strong>3. What happens if a remote micro-frontend fails to load (e.g. 500 error or network outage)?</strong></summary>

If the remote script fails to load and is not caught, it throws an unhandled Promise rejection that crashes the entire React application. To ensure high availability and resilience, all remote components must be wrapped in an **Error Boundary** and **`<Suspense>`**, rendering a graceful local fallback UI without affecting the rest of the application shell.
</details>

<details>
<summary><strong>4. How should micro-frontends communicate with each other?</strong></summary>

Micro-frontends should be loosely coupled. Communication should occur via:
1. **URL parameters and search queries** (the single source of truth for routing).
2. **Browser Custom Events (`window.dispatchEvent`)** for asynchronous notifications.
3. Lightweight publish/subscribe event buses. 
Directly sharing monolithic state stores (such as a shared Redux store) re-couples deployments and should be avoided.
</details>

<details>
<summary><strong>5. What is the role of `remoteEntry.js` in Module Federation?</strong></summary>

`remoteEntry.js` is the entry manifest generated by the remote application's build. It contains the catalog of exposed modules, chunk filenames, and shared dependency specifications. The host application downloads this small manifest first to understand where and how to fetch the remote's actual component chunks.
</details>

<details>
<summary><strong>6. How do you prevent CSS styling conflicts between micro-frontends?</strong></summary>

1. **Scoped CSS / CSS Modules**: Ensures unique generated class hashes.
2. **Tailwind CSS Prefixes**: Setting a unique prefix per team (`prefix: 'billing-'`).
3. **CSS `@scope` / Shadow DOM**: Enforces browser-level style encapsulation.
</details>

<details>
<summary><strong>7. What is the difference between an "eager" and "lazy" shared module?</strong></summary>

By default (`eager: false`), shared modules are loaded asynchronously, allowing Module Federation to negotiate which container provides the module before execution. Setting `eager: true` forces the module to be bundled directly into the initial entry chunk, which can lead to larger bundle sizes and should only be used when an application lacks an asynchronous bootstrap boundary.
</details>

<details>
<summary><strong>8. When should an enterprise NOT choose micro-frontends?</strong></summary>

Micro-frontends introduce significant operational complexity (distributed debugging, version drift, complex CI/CD, network latency overhead). Teams should avoid micro-frontends if they have fewer than 20-30 developers, a single product roadmap, or if a well-structured modular monolith or Turborepo monorepo meets their velocity requirements.
</details>

---

## References & Further Reading

- [Module Federation Official Documentation](https://module-federation.io/)
- [Zack Jackson: Module Federation Architecture](https://github.com/module-federation/module-federation-examples)
- [Rspack Module Federation Guide](https://rspack.dev/guide/features/module-federation)
- [Martin Fowler: Micro Frontends](https://martinfowler.com/articles/micro-frontends.html)
- [Vite Plugin Federation GitHub Repository](https://github.com/originjs/vite-plugin-federation)
