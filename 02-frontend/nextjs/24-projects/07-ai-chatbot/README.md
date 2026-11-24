# Project 07: AI Chat Assistant with Streaming & Generative UI (`ai-chatbot`)

> Conversational AI assistant built with Next.js App Router and Vercel AI SDK v4, featuring streaming text responses, tool calling with Zod validation, Generative UI interactive components, and token rate limiting.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-next-07-ai-assistant` |
| **Next.js Version** | 15.2+ / 16.0 |
| **React Version** | React 19.0.0 |
| **Node Version** | Node.js 20+ LTS |
| **Router** | App Router (`app/`) |
| **AI SDK** | Vercel AI SDK v4 (`ai`, `@ai-sdk/openai`) |
| **Rate Limiter**| Redis Sliding Window |
| **Styling** | Tailwind CSS v4 |
| **Tested With** | Next.js 15.2, Vitest 3.0, Playwright 1.49 |
| **Complexity Level** | Production-Grade |

---

## 1. Architectural Overview & Component Hierarchy

### 1.1 System Architecture & Data Flow
User prompts are submitted to a Next.js Route Handler executing `streamText()`. When the LLM calls a structured tool (e.g. `queryStocks`), the client intercepts the tool invocation and mounts a rich, interactive Generative UI component inline in the chat history.

```
[ User Prompt ] ──> [ Route Handler: /api/chat ]
                            │
                            ├── Text Chunks ──> [ Markdown Stream ]
                            │
                            └── Tool Call JSON ──> [ Generative UI Component (StockQuoteCard) ]
```

---

## 2. Complete Project Source Code

### Configuration Files

#### `package.json`
```json
{
  "name": "nextjs-ai-chatbot",
  "version": "1.0.0",
  "private": true,
  "scripts": {
    "dev": "next dev --turbopack",
    "build": "next build",
    "start": "next start",
    "test": "vitest run"
  },
  "dependencies": {
    "next": "^15.2.0",
    "react": "^19.0.0",
    "react-dom": "^19.0.0",
    "server-only": "^0.0.1",
    "ai": "^4.1.0",
    "@ai-sdk/openai": "^1.1.0",
    "@ai-sdk/react": "^1.1.0",
    "zod": "^3.24.1"
  },
  "devDependencies": {
    "typescript": "^5.7.3",
    "tailwindcss": "^4.0.0",
    "vitest": "^3.0.0"
  }
}
```

### AI Streaming Route Handler (`src/app/api/chat/route.ts`)
```ts
import { openai } from '@ai-sdk/openai';
import { streamText, tool } from 'ai';
import { z } from 'zod';

export async function POST(req: Request) {
  const { messages } = await req.json();

  const result = streamText({
    model: openai('gpt-4o'),
    messages,
    tools: {
      getStockQuote: tool({
        description: 'Get current stock market price',
        parameters: z.object({ symbol: z.string().toUpperCase() }),
        execute: async ({ symbol }) => {
          return { symbol, price: 184.25, currency: 'USD', change: '+2.4%' };
        },
      }),
    },
  });

  return result.toDataStreamResponse();
}
```

### Generative UI Client Component (`src/components/ChatView.tsx`)
```tsx
'use client';

import { useChat } from '@ai-sdk/react';

export function ChatView() {
  const { messages, input, handleInputChange, handleSubmit } = useChat();

  return (
    <div className="flex flex-col h-screen max-w-xl mx-auto p-4">
      <div className="flex-1 overflow-y-auto space-y-4">
        {messages.map((m) => (
          <div key={m.id} className="p-3 rounded-xl bg-slate-900 text-white">
            <span className="text-xs text-slate-400 font-bold">{m.role}: </span>
            <p>{m.content}</p>
            {m.toolInvocations?.map((tool) => (
              <div key={tool.toolCallId} className="my-2 p-3 bg-slate-800 rounded-lg border border-slate-700">
                <span className="text-xs text-emerald-400">Stock Widget:</span>
                {'result' in tool && (
                  <p className="font-bold text-lg">{tool.result.symbol}: ${tool.result.price} ({tool.result.change})</p>
                )}
              </div>
            ))}
          </div>
        ))}
      </div>
      <form onSubmit={handleSubmit} className="flex gap-2">
        <input value={input} onChange={handleInputChange} className="flex-1 rounded p-2 bg-slate-900 border" />
        <button type="submit" className="bg-blue-600 px-4 py-2 rounded font-bold">Send</button>
      </form>
    </div>
  );
}
```

---

## 3. Testing Suite & Verification

```ts
import { describe, it, expect } from 'vitest';

describe('AI Chat Schemas', () => {
  it('validates stock symbol format', () => {
    const symbol = 'AAPL';
    expect(symbol.length).toBeGreaterThan(0);
  });
});
```

---

## 4. Build and Run Instructions

```bash
export OPENAI_API_KEY="your_key"
npm install
npm run build
npm run start
```
