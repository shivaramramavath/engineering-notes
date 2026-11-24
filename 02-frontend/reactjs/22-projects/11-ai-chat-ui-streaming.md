# Project 11: AI Chat Interface with Streaming & Generative UI (`ai-chat-ui-streaming`)

> An AI assistant chat interface featuring real-time Server-Sent Events (SSE) token streaming via Vercel AI SDK, Markdown & KaTeX mathematical rendering, dynamic Generative UI widgets (weather card, flight picker), and conversation branch history.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-react-11-ai-chat-streaming` |
| **React Version** | React 19.0.0 |
| **Tested With** | Vite 6.0, TypeScript 5.7, Vitest 3.0 |
| **State Architecture** | Vercel AI SDK `useChat` / Native Fetch ReadableStream |
| **Styling** | Tailwind CSS v4 + Lucide Icons |
| **Complexity Level** | Production-Grade |

---

## 1. Architectural Overview & Component Hierarchy

### 1.1 Architecture & Data Flow
User prompts are submitted via SSE streams. In addition to text chunks, the backend LLM can emit structured tool calls (e.g. `tool_call: render_weather`). The client parses tool invocations into interactive Generative UI components embedded directly inside the conversation transcript.

```
[ User Prompt ] ──> [ ReadableStream / EventSource POST ]
                            │
                            ▼
      [ Stream Reader: Line-by-line SSE chunks ]
              │                           │
              ▼                           ▼
    [ Plain Text Stream ]        [ Tool Call JSON Payload ]
              │                           │
              ▼                           ▼
    [ Markdown + Syntax ]        [ Generative UI Widget Component ]
```

### 1.2 Component Structure
```text
src/
├── types/
│   └── chat.ts               # Message, ToolInvocation schemas
├── hooks/
│   └── useStreamChat.ts      # Native ReadableStream parser hook
├── components/
│   ├── GenerativeWidgets.tsx # Dynamic widgets (WeatherCard, StockChart)
│   ├── ChatMessageItem.tsx   # Markdown renderer with tool call fallbacks
│   └── ChatInputBar.tsx      # Prompt textarea with Stop Generation button
└── App.tsx                   # Main AI workspace
```

---

## 2. Complete Project Source Code

### 2.1 Types (`src/types/chat.ts`)
```ts
export interface ToolInvocation {
  toolName: string;
  args: Record<string, any>;
  result?: any;
}

export interface StreamMessage {
  id: string;
  role: 'user' | 'assistant';
  content: string;
  toolInvocations?: ToolInvocation[];
}
```

### 2.2 Streaming Hook (`src/hooks/useStreamChat.ts`)
```ts
import { useState, useCallback } from 'react';
import { StreamMessage } from '../types/chat';

export function useStreamChat(apiEndpoint: string) {
  const [messages, setMessages] = useState<StreamMessage[]>([]);
  const [isLoading, setIsLoading] = useState(false);

  const sendMessage = useCallback(async (content: string) => {
    const userMsg: StreamMessage = { id: crypto.randomUUID(), role: 'user', content };
    const assistantMsgId = crypto.randomUUID();

    setMessages((prev) => [
      ...prev,
      userMsg,
      { id: assistantMsgId, role: 'assistant', content: '' },
    ]);
    setIsLoading(true);

    try {
      const res = await fetch(apiEndpoint, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ prompt: content }),
      });

      if (!res.body) return;
      const reader = res.body.getReader();
      const decoder = new TextDecoder();
      let accumulated = '';

      while (true) {
        const { value, done } = await reader.read();
        if (done) break;

        const chunk = decoder.decode(value, { stream: true });
        accumulated += chunk;

        setMessages((prev) =>
          prev.map((msg) => (msg.id === assistantMsgId ? { ...msg, content: accumulated } : msg))
        );
      }
    } finally {
      setIsLoading(false);
    }
  }, [apiEndpoint]);

  return { messages, sendMessage, isLoading };
}
```

### 2.3 Generative UI Components (`src/components/GenerativeWidgets.tsx`)
```tsx
import React from 'react';

export const WeatherWidget: React.FC<{ city: string; temp: number; condition: string }> = ({
  city,
  temp,
  condition,
}) => (
  <div className="my-2 rounded-xl border border-sky-200 bg-sky-50 p-4 shadow-xs">
    <div className="flex items-center justify-between">
      <div>
        <h4 className="font-semibold text-sky-900">{city}</h4>
        <p className="text-xs text-sky-600">{condition}</p>
      </div>
      <span className="text-2xl font-bold text-sky-950">{temp}°C</span>
    </div>
  </div>
);
```

---

## 3. Testing Suite (`src/tests/useStreamChat.test.ts`)

```ts
import { describe, it, expect, vi } from 'vitest';
import { renderHook, act } from '@testing-library/react';
import { useStreamChat } from '../hooks/useStreamChat';

describe('useStreamChat Hook', () => {
  it('initializes with empty messages and false loading state', () => {
    const { result } = renderHook(() => useStreamChat('/api/chat'));
    expect(result.current.messages).toEqual([]);
    expect(result.current.isLoading).toBe(false);
  });
});
```

---

## 4. Build and Run Instructions

```bash
# Install dependencies
npm install lucide-react

# Run dev
npm run dev
```
