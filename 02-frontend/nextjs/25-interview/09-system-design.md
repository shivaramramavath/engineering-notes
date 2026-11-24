# Senior & Staff Interview Guide: Next.js System Design and Architecture Scenarios

> Comprehensive technical interview mastery guide covering full-scale system design scenarios: Real-Time Collaborative Canvas, Multi-Tenant SaaS with Custom Domains, High-Scale E-Commerce Storefront with PPR, and AI Agent Chatbot with Streaming Tool Calls.

| Metadata | Details |
| :--- | :--- |
| **Category** | Technical Interview Preparation |
| **Target Level** | Staff / Principal Software Engineer |
| **Prerequisites** | [23-architecture/README.md](../23-architecture/README.md), [24-projects/README.md](../24-projects/README.md) |
| **Next.js Baseline** | Next.js 15.2+ / 16.0, React 19.0.0, Node.js 20+ LTS |

---

## Table of Contents
1. [System Design Scenario 1: High-Concurrency E-Commerce Storefront](#1-system-design-scenario-1-high-concurrency-e-commerce-storefront)
2. [System Design Scenario 2: Multi-Tenant Enterprise SaaS with Custom Domains](#2-system-design-scenario-2-multi-tenant-enterprise-saas-with-custom-domains)
3. [System Design Scenario 3: Real-Time Collaborative Document Editor](#3-system-design-scenario-3-real-time-collaborative-document-editor)
4. [System Design Scenario 4: AI Agent Chatbot with Tool Calling & Streaming](#4-system-design-scenario-4-ai-agent-chatbot-with-tool-calling--streaming)
5. [Staff Engineering Evaluation Criteria](#5-staff-engineering-evaluation-criteria)

---

## 1. System Design Scenario 1: High-Concurrency E-Commerce Storefront

### Problem Statement
Design an e-commerce product catalog and checkout engine capable of handling **100,000 requests per second (RPS)** during flash sale events (e.g. Black Friday) with:
- Global TTFB $< 50$ms for 500,000 distinct product SKUs.
- Real-time stock counters updating within $< 2$s.
- Zero overselling during high-concurrency checkout bursts.

### Architectural Solution
```
[ Global Edge CDN (Cloudflare) ]
              │
    ┌─────────┴─────────┐
    ▼                   ▼
Static Shell (PPR)   Edge Dynamic Stream
(HTML/CSS/Images)    (Stock Counter / User Cart)
                        │
                        ▼
           [ Next.js Node.js Serverless Pool ]
                        │
         ┌──────────────┴──────────────┐
         ▼                             ▼
  [ Redis Cluster ]          [ PostgreSQL Cluster ]
  (Atomic DECR stock)        (Drizzle ORM via PgBouncer)
```

1. **Rendering & Caching Strategy**:
   - **Partial Prerendering (PPR `◐`)**: Product layout, descriptions, high-resolution WebP images, and reviews are prerendered and cached globally at edge POPs.
   - **Dynamic Stock Component**: Wrapped in `<Suspense fallback={<StockSkeleton />}>`. Uses `"use cache"` with `cacheLife({ expire: 2 })` and tagged with `cacheTag('stock-${sku}')`.
2. **Preventing Overselling (Flash Sale Engine)**:
   - Do **not** hit PostgreSQL directly for checkout inventory checks.
   - Use Redis with an atomic Lua script:
     ```lua
     local stock = redis.call('get', KEYS[1])
     if tonumber(stock) >= tonumber(ARGV[1]) then
       redis.call('decrby', KEYS[1], ARGV[1])
       return 1
     else
       return 0
     end
     ```
   - Successful Redis deductions enqueue a persistent order-creation job to BullMQ, which writes asynchronously to PostgreSQL.
3. **Cache Invalidation**:
   - When stock drops to zero or updates, a webhook dispatches `revalidateTag('stock-${sku}')`, immediately updating edge caches worldwide.

---

## 2. System Design Scenario 2: Multi-Tenant Enterprise SaaS with Custom Domains

### Problem Statement
Design a B2B SaaS platform (like Shopify or Notion) where:
- Each enterprise tenant gets a unique subdomain (`tenant.platform.com`) or custom vanity domain (`docs.enterprise.com`).
- Tenants can customize branding, custom CSS tokens, and RBAC policies.
- Data must be strictly isolated (SOC 2 Type II compliance).

### Architectural Solution
1. **Edge Host Header Rewriting (`proxy.ts` / `middleware.ts`)**:
   - Ingress matches incoming `Host` header.
   - If `host` is custom (`docs.enterprise.com`), middleware checks an in-memory or edge Redis cache (`upstash`) to resolve `customDomain -> tenantId`.
   - Rewrites internally to `/tenants/[tenantId]/[...route]`.
2. **Strict Tenant Scoping via DAL**:
   - The Data Access Layer enforces Row-Level Security (RLS) on PostgreSQL.
   - Every database transaction executes `SET LOCAL app.current_tenant_id = 'tenant_123'` before running queries.
   - Prevents SQL injection or accidental IDOR from leaking tenant data.
3. **Dynamic Theming without CSS Rebuilds**:
   - Server Component layout queries tenant theme configuration from the database.
   - Injects dynamic CSS variables into `:root` in the root layout:
     ```tsx
     <style>{`:root { --brand-primary: ${tenant.theme.primaryColor}; }`}</style>
     ```
   - Client components consume Tailwind classes like `bg-[var(--brand-primary)]`.

---

## 3. System Design Scenario 3: Real-Time Collaborative Document Editor

### Problem Statement
Build a collaborative document editor (like Google Docs / Figma) where:
- Hundreds of users can edit the same document simultaneously with conflict-free merging.
- Low-latency presence cursors ($< 50$ms).
- Immediate SEO indexing for public view-only links.

### Architectural Solution
1. **Hybrid Rendering Strategy**:
   - **View-Only Mode**: Next.js App Router renders the document statically using Server Components with `"use cache"`. Googlebot and unauthenticated visitors receive instant, lightweight HTML with zero JavaScript overhead.
   - **Edit Mode**: Navigating to `/doc/[id]/edit` renders a dynamic Client Island mounting a Yjs / CRDT (Conflict-free Replicated Data Type) canvas.
2. **Network Topology**:
   - Use Next.js Route Handlers and Server Actions strictly for authentication, permission checks, and document revision snapshots.
   - Offload high-frequency real-time WebSocket traffic (keystrokes, cursor coordinates) to an external WebSocket microservice (e.g. Node.js `ws` on AWS ECS or PartyKit).
3. **Snapshot Persistence**:
   - The WebSocket cluster periodically persists compressed CRDT binary snapshots to S3 every 30 seconds and notifies Next.js to call `revalidateTag('doc-${id}')`.

---

## 4. System Design Scenario 4: AI Agent Chatbot with Tool Calling & Streaming

### Problem Statement
Design an enterprise AI customer support assistant capable of:
- Streaming markdown answers with syntax highlighting.
- Invoking backend tools (checking order status, cancelling subscriptions, scheduling returns) with confirmation dialogs.
- Multi-turn conversation persistence with sub-second latency.

### Architectural Solution
1. **API Layer (Vercel AI SDK Core)**:
   - Use a Next.js Route Handler (`app/api/chat/route.ts`) executing on the Node.js runtime.
   - `streamText({ model: anthropic('claude-3-5-sonnet'), tools: { ... }, messages })`.
   - Returns a `DataStreamResponse` that streams tokens and tool call events simultaneously over an HTTP/2 chunked transfer stream.
2. **Interactive UI Client (AI SDK UI)**:
   - Client component uses `useChat({ api: '/api/chat' })`.
   - Renders messages incrementally as tokens arrive.
   - When a tool call requires confirmation (e.g. `cancelSubscription`), the client intercepts the tool invocation event and renders an interactive React UI card with "Confirm" and "Reject" buttons.
3. **Tool Security**:
   - Tool execution functions live in the Data Access Layer with `import 'server-only'`.
   - Each tool execution validates user session cookies and ensures the user has ownership over the entity being queried or modified.

---

## 5. Staff Engineering Evaluation Criteria

| Dimension | Expectations for Staff / Principal Candidates |
| :--- | :--- |
| **Domain Decoupling** | Clearly isolates stateful real-time protocols (WebSockets) from stateless HTTP rendering (Next.js). |
| **Resilience & Fault Tolerance** | Designs graceful degradation modes (e.g. falling back from WebSocket to HTTP polling, serving stale CDN cache on origin failure). |
| **Cost Optimization** | Balances serverless invocations against edge caching and long-lived persistent container costs. |
| **Security Governance** | Zero trust across architectural tiers; never trusts client tokens or edge proxy headers implicitly. |
