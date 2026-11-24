# OpenAI and LLM Integration in NestJS

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 05-advanced / 08-ai-and-llm
- **Prerequisites**: [02-fundamentals/04-providers-and-services](../../02-fundamentals/04-providers-and-services.md), [05-advanced/04-realtime/06-server-sent-events-and-streaming](../04-realtime/06-server-sent-events-and-streaming.md)

---

## 1. Context & Purpose

Artificial Intelligence (AI) and Large Language Models (LLMs)—such as OpenAI's GPT-4o, Anthropic's Claude 3.5, and Google's Gemini 1.5—are transforming backend engineering. Applications now regularly incorporate generative capabilities: conversational assistants, automated document summarization, semantic tagging, and automated entity extraction.

Integrating LLMs into enterprise NestJS backends requires more than simple HTTP requests. It demands:
- Dedicated, injectable provider modules managing client lifecycles and API keys.
- **Streaming token responses** directly to client browsers using Server-Sent Events (SSE).
- Structured output parsing using JSON schema validation (Zod).
- Automated token rate limiting and error handling for model timeouts.

---

## 2. Theoretical Foundations

### 2.1 The LLM Interaction Paradigm
- **Chat Completion**: Submitting a sequence of system, user, and assistant messages and receiving a generated response string.
- **Token Streaming (Server-Sent Events)**: LLM inference generates text token-by-token. Instead of waiting 10–20 seconds for the entire paragraph, the server streams each token chunk as it is generated, yielding near-instantaneous Time-To-First-Token (TTFT $< 500\text{ms}$).
- **Function Calling / Tool Use**: Instructing the model to output a strict JSON structure matching an application schema, enabling the model to invoke backend services.

---

## 3. Architecture & Internal Mechanics

```
┌────────────────────────────────────────────────────────┐
│                   NestJS Application                   │
└───────────────────────────┬────────────────────────────┘
                            │
               Injects OpenAiService Provider
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│                   OpenAiService                        │
│   const stream = await openai.chat.completions.create( │
│     { model: 'gpt-4o', stream: true, ... }             │
│   )                                                    │
└───────────────────────────┬────────────────────────────┘
                            │
               AsyncIterable Stream of Chunks
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│                   SSE Route Handler                    │
│   Emits MessageEvent: { data: { token: 'Hello' } }     │
└───────────────────────────┬────────────────────────────┘
                            │
              HTTP text/event-stream (Chunked)
                            │
                            ▼
                 Browser Client (ChatGPT UI)
```

---

## 4. Concrete Implementation

### 4.1 Installing Dependencies

```bash
npm install --save openai
```

---

### 4.2 Building a Dedicated OpenAI Module

```typescript
// src/ai/openai.module.ts
import { Module, Global } from '@nestjs/common';
import { ConfigModule, ConfigService } from '@nestjs/config';
import OpenAI from 'openai';

export const OPENAI_CLIENT = Symbol('OPENAI_CLIENT');

@Global()
@Module({
  imports: [ConfigModule],
  providers: [
    {
      provide: OPENAI_CLIENT,
      useFactory: (config: ConfigService) => {
        return new OpenAI({
          apiKey: config.getOrThrow<string>('OPENAI_API_KEY'),
          maxRetries: 3,
          timeout: 30000, // 30s timeout
        });
      },
      inject: [ConfigService],
    },
  ],
  exports: [OPENAI_CLIENT],
})
export class OpenAiModule {}
```

---

### 4.3 Streaming LLM Tokens in a Service

```typescript
// src/ai/openai.service.ts
import { Injectable, Inject, Logger } from '@nestjs/common';
import OpenAI from 'openai';
import { Observable } from 'rxjs';
import { OPENAI_CLIENT } from './openai.module';

@Injectable()
export class OpenAiService {
  private readonly logger = new Logger(OpenAiService.name);

  constructor(@Inject(OPENAI_CLIENT) private readonly openai: OpenAI) {}

  // 1. Unary Chat Completion with Structured JSON Output
  async generateSummary(content: string): Promise<string> {
    const response = await this.openai.chat.completions.create({
      model: 'gpt-4o-mini',
      messages: [
        { role: 'system', content: 'You are an executive summary assistant. Keep summaries under 3 sentences.' },
        { role: 'user', content },
      ],
      temperature: 0.3,
    });

    return response.choices[0].message.content || '';
  }

  // 2. Real-Time Token Streaming via RxJS Observable
  streamChatCompletion(prompt: string): Observable<string> {
    return new Observable((subscriber) => {
      let isCancelled = false;

      (async () => {
        try {
          const stream = await this.openai.chat.completions.create({
            model: 'gpt-4o',
            messages: [{ role: 'user', content: prompt }],
            stream: true,
          });

          for await (const chunk of stream) {
            if (isCancelled) break;
            const token = chunk.choices[0]?.delta?.content || '';
            if (token) {
              subscriber.next(token);
            }
          }

          subscriber.complete();
        } catch (err) {
          subscriber.error(err);
        }
      })();

      return () => {
        isCancelled = true;
      };
    });
  }
}
```

---

### 4.4 Exposing Token Stream via `@Sse()` Endpoint

```typescript
// src/ai/ai.controller.ts
import { Controller, Sse, Post, Body, MessageEvent } from '@nestjs/common';
import { Observable } from 'rxjs';
import { map } from 'rxjs/operators';
import { OpenAiService } from './openai.service';

@Controller('ai')
export class AiController {
  constructor(private readonly openAiService: OpenAiService) {}

  @Sse('chat')
  streamChat(@Body('prompt') prompt: string): Observable<MessageEvent> {
    return this.openAiService.streamChatCompletion(prompt).pipe(
      map((token) => ({
        data: { token },
        type: 'chunk',
      })),
    );
  }
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Client Disconnect Midway Through Streaming
If a user closes their browser tab 2 seconds into a 20-second generation, continuing generation burns OpenAI API tokens unnecessarily.
- **Handling**: In RxJS, returning a teardown function in `new Observable(...)` allows aborting the stream generator via an `AbortController`.

### 5.2 Rate Limiting (HTTP 429) and Token Quotas
Hitting OpenAI organization rate limits (TPM: Tokens Per Minute, RPM: Requests Per Minute) causes sudden 429 errors.
- **Handling**: Configure `maxRetries: 3` in `OpenAI` options and wrap requests in an internal queue or circuit breaker.

---

## 6. Performance & Memory Implications

- **Stream Memory**: Streaming tokens chunk-by-chunk consumes almost zero RAM, as each tiny string token is immediately flushed across the network socket.
- **Latency**: Streaming reduces perceived latency from 15 seconds to under 400ms.

---

## 7. Anti-Patterns & Common Mistakes

- **Waiting for Complete Response Before Returning**: Collecting all tokens into a giant string before returning via a standard HTTP POST. The user stares at a blank loading spinner for 15 seconds. Always stream tokens!
- **Exposing OpenAI API Key in Client Code**: Calling OpenAI directly from React or Next.js client bundles. Always route AI requests through your authenticated NestJS backend.

---

## 8. Production Checklist & Operational Readiness

- [ ] OpenAI API keys stored strictly in environment secrets.
- [ ] Token usage logged to monitor monthly budget caps.
- [ ] Timeouts configured on client initialization (`timeout: 30000`).
- [ ] Input prompts sanitized against Prompt Injection attacks.

---

## 9. Migration & Version History

- **OpenAI Node SDK v4**: Migrated to official TypeScript SDK with native async iteration and streaming support.

---

## 10. Verification & Testing

Unit testing OpenAI service with mock stream:

```typescript
import { OpenAiService } from './openai.service';
import { firstValueFrom } from 'rxjs';
import { toArray } from 'rxjs/operators';

describe('OpenAiService', () => {
  let service: OpenAiService;
  let mockOpenAi: any;

  beforeEach(() => {
    mockOpenAi = {
      chat: {
        completions: {
          create: vi.fn(),
        },
      },
    };
    service = new OpenAiService(mockOpenAi);
  });

  it('generates summary cleanly', async () => {
    mockOpenAi.chat.completions.create.mockResolvedValueOnce({
      choices: [{ message: { content: 'Test summary' } }],
    });

    const res = await service.generateSummary('Long article...');
    expect(res).toBe('Test summary');
  });
});
```

---

## 11. Comprehensive Examples

### Prompt Injection Sanitizer Helper

```typescript
// src/common/ai/prompt-sanitizer.util.ts
export function sanitizeUserPrompt(rawInput: string): string {
  // Strip control characters and excessive length
  const trimmed = rawInput.trim().slice(0, 4000);

  // Guard against common system prompt override phrases
  const sanitized = trimmed.replace(
    /ignore previous instructions|system prompt|you are now/gi,
    '[filtered]',
  );

  return sanitized;
}
```

---

## 12. Cross-References & Further Reading

- [05-advanced/04-realtime/06-server-sent-events-and-streaming](../04-realtime/06-server-sent-events-and-streaming.md) - SSE streaming mechanics
- [02-embeddings-and-vector-search.md](./02-embeddings-and-vector-search.md) - Vector embeddings
- [03-rag.md](./03-rag.md) - Retrieval-Augmented Generation
- [OpenAI Node.js SDK Reference](https://github.com/openai/openai-node)
