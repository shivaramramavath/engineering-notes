# Vercel AI SDK, Streaming UI & MCP Clients (`vercel-ai-sdk-core-and-ui-hooks`)

> Building production AI applications in Next.js App Router: Vercel AI SDK Core (`ai`), UI streaming hooks (`useChat`), Generative UI widgets with React Server Components, Model Context Protocol (MCP) clients, and Redis token bucket rate limiting.

| Metadata | Details |
| :--- | :--- |
| **Next.js Version** | 15.2+ / 16.0 |
| **React Version** | React 19.0.0 |
| **Node Version** | Node.js 20+ LTS |
| **Router** | App Router (`app/`) |
| **AI Stack** | Vercel AI SDK v4, OpenAI / Anthropic, MCP |
| **Tested With** | Next.js 15.2, Vitest 3.0, TypeScript 5.7 |

---

## 1. Concept Overview & Mental Model

### 1.1 The Shift: From Text Streams to Generative UI
Traditional AI interfaces stream plain text tokens. Modern Next.js AI applications leverage **Generative UI**:
- The LLM can invoke structured tools (e.g. `getWeather({ city: "Tokyo" })`).
- Rather than waiting for text descriptions, Next.js renders **interactive React Server Components** directly inside the streamed conversation flow!
- **Model Context Protocol (MCP)**: Universal standard allowing Next.js applications to connect LLMs to external tool servers (databases, GitHub, Slack, local files).

```
[ User Prompt ] ──> [ Route Handler: streamText() ]
                             │
                             ├── Streams Text Tokens ──> [ Markdown Renderer ]
                             │
                             └── LLM Triggers Tool ──> [ Generative UI Component (RSC) ]
                                                            ├── Interactive Weather Widget
                                                            └── Flight Seat Selector
```

---

## 2. Naive vs. Idiomatic Comparison

### Naive: Client-Side Direct OpenAI Calls
```tsx
// app/chat/page.tsx (Naive - LEAKS API KEYS TO BROWSER!)
'use client';

import OpenAI from 'openai';

// CRITICAL VULNERABILITY: Bundling private API key in client JavaScript!
const openai = new OpenAI({ apiKey: process.env.NEXT_PUBLIC_OPENAI_KEY, dangerouslyAllowBrowser: true });
```

### Idiomatic: Vercel AI SDK with Server Route & Rate Limiting
```ts
// app/api/chat/route.ts (Idiomatic)
import { openai } from '@ai-sdk/openai';
import { streamText } from 'ai';
import { verifySession } from '@/dal/session';
import { checkAIRateLimit } from '@/lib/rateLimiter';

export async function POST(req: Request) {
  const session = await verifySession();
  if (!session) return new Response('Unauthorized', { status: 401 });

  // Token bucket rate limiting via Redis
  const allowed = await checkAIRateLimit(session.user.id);
  if (!allowed) return new Response('Rate limit exceeded', { status: 429 });

  const { messages } = await req.json();

  const result = streamText({
    model: openai('gpt-4o'),
    messages,
  });

  return result.toDataStreamResponse();
}
```

---

## 3. Five Progressive Examples

### Example 1: `useChat` Client Component
```tsx
// app/chat/ChatInterface.tsx
'use client';

import { useChat } from '@ai-sdk/react';

export function ChatInterface() {
  const { messages, input, handleInputChange, handleSubmit, isLoading } = useChat();

  return (
    <div className="flex flex-col h-screen max-w-2xl mx-auto p-4">
      <div className="flex-1 overflow-y-auto space-y-4">
        {messages.map((m) => (
          <div key={m.id} className={m.role === 'user' ? 'text-blue-500' : 'text-slate-200'}>
            <strong>{m.role === 'user' ? 'You: ' : 'AI: '}</strong>
            {m.content}
          </div>
        ))}
      </div>

      <form onSubmit={handleSubmit} className="flex gap-2">
        <input
          value={input}
          onChange={handleInputChange}
          placeholder="Ask anything..."
          className="flex-1 rounded-lg border p-2 bg-slate-900 text-white"
        />
        <button type="submit" disabled={isLoading} className="btn-primary">
          Send
        </button>
      </form>
    </div>
  );
}
```

### Example 2: Structured Tool Calling with Zod
```ts
// app/api/chat/route.ts
import { openai } from '@ai-sdk/openai';
import { streamText, tool } from 'ai';
import { z } from 'zod';

export async function POST(req: Request) {
  const { messages } = await req.json();

  const result = streamText({
    model: openai('gpt-4o'),
    messages,
    tools: {
      checkFlightPrice: tool({
        description: 'Get live flight pricing between cities',
        parameters: z.object({
          origin: z.string().length(3),
          destination: z.string().length(3),
        }),
        execute: async ({ origin, destination }) => {
          return { price: 349, currency: 'USD', airline: 'SkyAir' };
        },
      }),
    },
  });

  return result.toDataStreamResponse();
}
```

### Example 3: Generative UI Rendering in Client
```tsx
// components/ToolInvocationView.tsx
import { Message } from 'ai';

export function ToolInvocationView({ message }: { message: Message }) {
  return (
    <div>
      {message.toolInvocations?.map((toolInvocation) => {
        if (toolInvocation.toolName === 'checkFlightPrice') {
          if ('result' in toolInvocation) {
            return (
              <div key={toolInvocation.toolCallId} className="card-flight">
                <h4>Flight Quote</h4>
                <p>${toolInvocation.result.price} on {toolInvocation.result.airline}</p>
              </div>
            );
          }
          return <p key={toolInvocation.toolCallId}>Searching live flights...</p>;
        }
      })}
    </div>
  );
}
```

### Example 4: Redis Token Bucket Rate Limiting
```ts
// lib/rateLimiter.ts
import 'server-only';
import { Redis } from '@upstash/redis';
import { Ratelimit } from '@upstash/ratelimit';

const redis = Redis.fromEnv();

const ratelimit = new Ratelimit({
  redis,
  limiter: Ratelimit.slidingWindow(20, '1 m'), // 20 requests per minute
});

export async function checkAIRateLimit(userId: string) {
  const { success } = await ratelimit.limit(`ai-ratelimit:${userId}`);
  return success;
}
```

### Example 5: Model Context Protocol (MCP) Client in Next.js
```ts
// lib/mcpClient.ts
import 'server-only';
import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { StdioClientTransport } from '@modelcontextprotocol/sdk/client/stdio.js';

export async function connectToDatabaseMCP() {
  const transport = new StdioClientTransport({
    command: 'npx',
    args: ['-y', '@modelcontextprotocol/server-postgres', process.env.DATABASE_URL!],
  });

  const client = new Client({ name: 'nextjs-client', version: '1.0.0' }, { capabilities: {} });
  await client.connect(transport);
  return client;
}
```

---

## 4. Under the Hood & Internals
The Vercel AI SDK converts LLM chunks into a standardized Data Stream Protocol over an HTTP Server-Sent Events (SSE) connection:
```text
0:"Hello "
0:"world!"
9:{"toolCallId":"c1","toolName":"getWeather","args":{"city":"Tokyo"}}
a:{"toolCallId":"c1","result":{"temp":18}}
```
The client parser (`useChat`) continuously decodes text tokens (`0:`) into state, immediately mounting interactive React components when tool events (`9:` and `a:`) arrive.

---

## 5. Security & Cost Protection
- **Prompt Injection Defense**: Never execute database updates or financial mutations directly from unvalidated LLM outputs without human confirmation.
- **Budget Capping**: Enforce per-user token and cost budgets in Redis before invoking expensive models (e.g. Claude 3.5 Sonnet or GPT-4o).

---

## 6. Common Mistakes & Anti-Patterns (Table)

| Anti-Pattern | Root Cause | Observable Symptom | Correct Architecture |
| :--- | :--- | :--- | :--- |
| Exposing API keys in client | Using `NEXT_PUBLIC_` for LLM keys | Leaked keys in browser JS, massive bill | Keep keys in Route Handlers with `server-only` |
| No rate limiting | Unprotected public AI endpoint | Cost exhaustion denial of wallet | Implement Redis token bucket limiter |
| Missing error boundaries on tools | Tool execution error crashes stream | Blank white screen on tool failure | Catch and return `{ error: msg }` in tool |
| Long-running sync tool | Blocking LLM stream | Timeout after 15s | Offload heavy tools or stream status |

---

## 7. Performance & Optimization Guide
Cache identical embedding vectors and deterministic prompt responses in Redis using semantic caching (Upstash Vector or Redis LangChain cache), cutting LLM API costs by up to 40%.

---

## 8. TypeScript & Strict Typing Recipes
```ts
import type { Tool } from 'ai';
import { z } from 'zod';

export type TypedTool<TInput, TOutput> = Tool<z.ZodType<TInput>, TOutput>;
```

---

## 9. Testing & Quality Verification

```ts
import { describe, it, expect } from 'vitest';
import { POST } from './route';

describe('AI Chat Route Handler', () => {
  it('rejects unauthorized requests with 401', async () => {
    const req = new Request('http://localhost:3000/api/chat', { method: 'POST' });
    const res = await POST(req);
    expect(res.status).toBe(401);
  });
});
```

---

## 10. When NOT to Use This Pattern
- If displaying static or factual documentation, use pre-built static search (Meilisearch or Algolia) rather than generative LLM streaming.

---

## 11. Frequently Asked Interview Questions

<details>
<summary>1. How does Generative UI differ from traditional LLM chat?</summary>
Traditional chat outputs plain text. Generative UI renders interactive, stateful React components (graphs, booking widgets, forms) streamed dynamically when the model triggers a tool call.
</details>

<details>
<summary>2. What is the Data Stream Protocol in the Vercel AI SDK?</summary>
It is an SSE-based protocol that transmits text deltas, tool call invocations, tool results, and metadata across a single streaming HTTP connection.
</details>

<details>
<summary>3. What is the Model Context Protocol (MCP)?</summary>
MCP is an open standard that allows LLM applications to securely discover and invoke external tools and data sources (databases, local file systems, third-party APIs) through standardized client-server interfaces.
</details>

<details>
<summary>4. How do you prevent denial-of-wallet attacks on Next.js AI endpoints?</summary>
By authenticating users before generation, enforcing token bucket rate limiting via Redis, and establishing hard monthly budget alerts in provider dashboards.
</details>

<details>
<summary>5. How does `useChat` handle streaming on the client?</summary>
`useChat` connects to the route handler, parses incoming text deltas into local React state, coordinates tool invocations, and manages abort controllers.
</details>

<details>
<summary>6. Can AI responses be cached in Next.js?</summary>
Yes. Deterministic tool outputs or static question answers can be cached using Next.js `"use cache"` or Upstash semantic vector caching.
</details>

<details>
<summary>7. What role does Zod play in AI tool calling?</summary>
Zod schemas define the exact JSON Schema parameters expected by the model, guaranteeing that tool arguments are strictly typed and validated before execution.
</details>

<details>
<summary>8. How do you cancel an ongoing LLM generation in the UI?</summary>
`useChat` exposes a `stop()` function that triggers an `AbortController.abort()`, terminating the server generation and network stream immediately.
</details>

---

## 12. Hands-on Practice Exercises

<details>
<summary>Exercise 1 (Easy): Create a streaming chat endpoint</summary>
Implement `app/api/chat/route.ts` using `streamText()` and `toDataStreamResponse()`.
</details>

<details>
<summary>Exercise 2 (Medium): Build a weather tool with Generative UI</summary>
Define a `getWeather` tool and render a custom card when the tool finishes.
</details>

<details>
<summary>Exercise 3 (Hard): Implement Redis token bucket rate limiting</summary>
Protect an AI route handler using `@upstash/ratelimit` with sliding window rules.
</details>

---

## 13. Cross-References & Further Reading
- [Route Handlers: Streaming Responses](file:///E:/Learning/engineering-notes/02-frontend/nextjs/08-route-handlers-and-proxy/05-proxy-patterns-and-cve-2025-29927.md)
- [Official Vercel AI SDK Documentation](https://sdk.vercel.ai/docs)
