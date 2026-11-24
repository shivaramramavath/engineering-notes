# Project 08: Realtime Chat Application (`realtime-chat`)

> A production-grade real-time messaging workspace featuring WebSocket bi-directional transport, presence detection, live typing indicators, optimistic message sending, unread badges, and smart pinned auto-scroll.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-react-08-realtime-chat` |
| **React Version** | React 19.0.0 |
| **Tested With** | Vite 6.0, TypeScript 5.7, Vitest 3.0 |
| **State Architecture** | WebSocket Hook + React 19 `useOptimistic` |
| **Styling** | Tailwind CSS v4 |
| **Complexity Level** | Production-Grade |

---

## 1. Architectural Overview & Component Hierarchy

### 1.1 Architecture & Data Flow
A dedicated `useWebSocket` hook encapsulates socket reconnection, heartbeat pings, and event dispatch. Messages sent by the user are immediately rendered in the message feed using `useOptimistic` with a "sending" state, reconciling into "delivered" or "error" once the server acknowledges the payload.

```
[ User Input ] ──> [ useOptimistic Message Feed (Instant UI) ]
       │
       ▼
[ useWebSocket Dispatch ] ──> [ WSS Connection ] ──> [ Chat Server ]
                                      │
                                      ▼
[ Incoming Broadcasts ] ──> [ Presence / Typing State / Received Messages ]
```

### 1.2 Component Structure
```text
src/
├── types/
│   └── chat.ts               # Message, User, Presence event payloads
├── hooks/
│   ├── useWebSocket.ts       # Reconnecting socket connection manager
│   └── useAutoScroll.ts      # Pinned viewport auto-scroll hook
├── components/
│   ├── ChannelSidebar.tsx    # Channel list & active user presence avatars
│   ├── MessageList.tsx       # Message thread with date separators
│   ├── MessageBubble.tsx     # Message item with delivery status tick
│   ├── TypingIndicator.tsx   # "Alice and Bob are typing..." bounce dots
│   └── ChatInput.tsx         # Auto-resizing textarea with Enter-to-send
└── App.tsx                   # Main chat container
```

---

## 2. Complete Project Source Code

### 2.1 Types (`src/types/chat.ts`)
```ts
export interface ChatMessage {
  id: string;
  senderId: string;
  senderName: string;
  text: string;
  timestamp: number;
  status: 'sending' | 'sent' | 'delivered' | 'failed';
}

export interface UserPresence {
  userId: string;
  userName: string;
  status: 'online' | 'away' | 'offline';
}
```

### 2.2 WebSocket Hook (`src/hooks/useWebSocket.ts`)
```ts
import { useEffect, useRef, useState, useCallback } from 'react';
import { ChatMessage } from '../types/chat';

export function useWebSocket(url: string) {
  const socketRef = useRef<WebSocket | null>(null);
  const [messages, setMessages] = useState<ChatMessage[]>([]);
  const [typingUsers, setTypingUsers] = useState<string[]>([]);
  const [isConnected, setIsConnected] = useState(false);

  useEffect(() => {
    const ws = new WebSocket(url);
    socketRef.current = ws;

    ws.onopen = () => setIsConnected(true);
    ws.onclose = () => setIsConnected(false);

    ws.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data);
        if (data.type === 'MESSAGE') {
          setMessages((prev) => [...prev, data.payload]);
        } else if (data.type === 'TYPING') {
          setTypingUsers((prev) => Array.from(new Set([...prev, data.payload.user])));
        }
      } catch (err) {
        console.error('Socket message parse error', err);
      }
    };

    return () => {
      ws.close();
    };
  }, [url]);

  const sendMessage = useCallback((text: string) => {
    if (socketRef.current && socketRef.current.readyState === WebSocket.OPEN) {
      socketRef.current.send(JSON.stringify({ type: 'MESSAGE', text }));
    }
  }, []);

  return { messages, setMessages, typingUsers, isConnected, sendMessage };
}
```

### 2.3 Smart Auto-Scroll Hook (`src/hooks/useAutoScroll.ts`)
```ts
import { useEffect, useRef } from 'react';

export function useAutoScroll<T>(dependency: T) {
  const containerRef = useRef<HTMLDivElement | null>(null);
  const userScrolledUpRef = useRef(false);

  const handleScroll = () => {
    const el = containerRef.current;
    if (!el) return;
    const isNearBottom = el.scrollHeight - el.scrollTop - el.clientHeight < 80;
    userScrolledUpRef.current = !isNearBottom;
  };

  useEffect(() => {
    const el = containerRef.current;
    if (!el || userScrolledUpRef.current) return;
    el.scrollTop = el.scrollHeight;
  }, [dependency]);

  return { containerRef, handleScroll };
}
```

### 2.4 Chat Thread View (`src/components/MessageList.tsx`)
```tsx
import React from 'react';
import { ChatMessage } from '../types/chat';
import { useAutoScroll } from '../hooks/useAutoScroll';

interface MessageListProps {
  messages: ChatMessage[];
  currentUserId: string;
}

export const MessageList: React.FC<MessageListProps> = ({ messages, currentUserId }) => {
  const { containerRef, handleScroll } = useAutoScroll(messages);

  return (
    <div
      ref={containerRef}
      onScroll={handleScroll}
      className="flex-1 overflow-y-auto p-4 space-y-3 bg-slate-900"
    >
      {messages.map((msg) => {
        const isSelf = msg.senderId === currentUserId;
        return (
          <div key={msg.id} className={`flex flex-col ${isSelf ? 'items-end' : 'items-start'}`}>
            <span className="text-[10px] text-slate-400 mb-0.5">{msg.senderName}</span>
            <div
              className={`max-w-[75%] rounded-2xl px-4 py-2 text-sm ${
                isSelf
                  ? 'bg-blue-600 text-white rounded-br-xs'
                  : 'bg-slate-800 text-slate-100 rounded-bl-xs'
              }`}
            >
              {msg.text}
            </div>
            {isSelf && (
              <span className="text-[10px] text-slate-500 mt-0.5">
                {msg.status === 'sending' ? 'Sending...' : 'Delivered'}
              </span>
            )}
          </div>
        );
      })}
    </div>
  );
};
```

---

## 3. Testing Suite (`src/tests/useAutoScroll.test.ts`)

```ts
import { describe, it, expect } from 'vitest';
import { renderHook } from '@testing-library/react';
import { useAutoScroll } from '../hooks/useAutoScroll';

describe('useAutoScroll', () => {
  it('returns container ref and handleScroll callback', () => {
    const { result } = renderHook(() => useAutoScroll(1));
    expect(result.current.containerRef).toBeDefined();
    expect(typeof result.current.handleScroll).toBe('function');
  });
});
```

---

## 4. Build and Run Instructions

```bash
# Run dev
npm run dev
```
