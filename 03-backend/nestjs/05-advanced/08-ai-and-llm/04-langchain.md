---
title: "LangChain.js and LangGraph in NestJS"
description: "Architecting agentic workflows, LCEL pipelines, tool calling, and stateful LangGraph state machines within enterprise NestJS applications"
tags: [nestjs, ai, langchain, langgraph, agents, lcel, tool-calling]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# LangChain.js and LangGraph in NestJS

## Conceptual Overview
As LLM-driven applications evolve beyond simple prompt-response interactions, backend architectures require structured orchestration frameworks to coordinate multi-step reasoning, external tool execution, memory management, and cyclic agent loops. **LangChain.js** and **LangGraph.js** provide standardized abstractions for composing these AI workflows.

Integrating LangChain and LangGraph with **NestJS** bridges the gap between raw AI orchestration and enterprise backend engineering:
- **Dependency Injection**: Injecting database repositories, third-party API clients, and security guards directly into custom AI agent tools.
- **LCEL (LangChain Expression Language)**: Declarative, composable streaming pipelines that naturally align with RxJS streams and NestJS interceptors.
- **LangGraph State Machines**: Multi-agent cyclic graphs with durable state persistence in PostgreSQL or Redis for long-running workflows.

```
+-----------------------------------------------------------------------------------+
|                           NestJS Agent Architecture                               |
|                                                                                   |
|  [ Inbound Request ] --> [ Agent Controller ]                                     |
|                                  |                                                |
|                                  v                                                |
|                     [ LangGraph Orchestrator ]                                    |
|                       (State Machine / Nodes)                                     |
|                         /        |        \                                       |
|                        v         v         v                                      |
|                 [ Node: Plan ] [ Node: Model ] [ Node: Tool Executor ]            |
|                                       ^                     |                     |
|                                       |                     v                     |
|                              (tool_calls response)   [ Injected NestJS Services ] |
|                                                      - OrdersService              |
|                                                      - PaymentService             |
|                                                      - InventoryService           |
+-----------------------------------------------------------------------------------+
```

---

## Core Mechanisms & Architecture

### 1. LangChain Expression Language (LCEL)
LCEL provides a functional composition interface built around the `Runnable` protocol:
- Every component (`PromptTemplate`, `ChatModel`, `OutputParser`, `Retriever`) implements `invoke()`, `batch()`, and `stream()`.
- Chaining is expressed via the `.pipe()` method:
  $$\text{Chain} = \text{Prompt} \xrightarrow{\text{pipe}} \text{Model} \xrightarrow{\text{pipe}} \text{StringParser}$$

### 2. Tool Calling and Structured Outputs
Modern models (OpenAI, Anthropic, Gemini) support native tool calling via schema specifications:
- Agents emit structured JSON arguments conforming to a JSON Schema or Zod specification.
- In NestJS, tools wrap existing domain services, allowing models to invoke business logic under strict schema constraints.

### 3. LangGraph.js: Stateful, Cyclic Multi-Agent Systems
Traditional DAGs (Directed Acyclic Graphs) cannot accommodate iterative agent loops (e.g., plan $\to$ execute tool $\to$ observe result $\to$ re-plan). LangGraph models agents as finite state machines:
- **State**: A shared typed object passed between nodes.
- **Nodes**: Plain async functions that receive state and return state updates.
- **Edges**: Conditional or direct transitions directing workflow flow based on model outputs or validation errors.
- **Checkpointers**: Durable state storage (Postgres, Redis, MongoDB) that saves workflow progress at each step, enabling human-in-the-loop approvals and resilient resumption after failures.

---

## Implementation Details & NestJS APIs

### 1. NestJS Dynamic Tool Registration

Tools must leverage NestJS dependency injection so they can call internal repositories and domain services:

```typescript
// src/ai/tools/order-lookup.tool.ts
import { Injectable } from '@nestjs/common';
import { tool } from '@langchain/core/tools';
import { z } from 'zod';
import { OrdersService } from '../../orders/orders.service';

@Injectable()
export class OrderLookupToolFactory {
  constructor(private readonly ordersService: OrdersService) {}

  createTool() {
    return tool(
      async ({ orderId }: { orderId: string }) => {
        const order = await this.ordersService.findById(orderId);
        if (!order) {
          return JSON.stringify({ error: `Order ${orderId} not found.` });
        }
        return JSON.stringify({
          orderId: order.id,
          status: order.status,
          total: order.totalAmount,
          itemsCount: order.items.length,
          estimatedDelivery: order.estimatedDeliveryDate,
        });
      },
      {
        name: 'lookup_order_status',
        description: 'Look up the current status, delivery date, and total of an existing customer order by its ID.',
        schema: z.object({
          orderId: z.string().describe('The UUID of the order to query'),
        }),
      },
    );
  }
}
```

### 2. LCEL Chain with Streaming in NestJS

```typescript
// src/ai/services/lcel-chain.service.ts
import { Injectable } from '@nestjs/common';
import { ConfigService } from '@nestjs/config';
import { ChatOpenAI } from '@langchain/openai';
import { ChatPromptTemplate } from '@langchain/core/prompts';
import { StringOutputParser } from '@langchain/core/output_parsers';
import { Observable } from 'rxjs';

@Injectable()
export class LcelChainService {
  private readonly model: ChatOpenAI;

  constructor(private readonly configService: ConfigService) {
    this.model = new ChatOpenAI({
      modelName: 'gpt-4o-mini',
      temperature: 0.2,
      apiKey: this.configService.getOrThrow<string>('OPENAI_API_KEY'),
      streaming: true,
    });
  }

  async runSimpleChain(topic: string): Promise<string> {
    const prompt = ChatPromptTemplate.fromMessages([
      ['system', 'You are a senior software architect. Provide a concise technical summary.'],
      ['user', 'Explain the architectural tradeoffs of: {topic}'],
    ]);

    const chain = prompt.pipe(this.model).pipe(new StringOutputParser());
    return chain.invoke({ topic });
  }

  streamChain(topic: string): Observable<string> {
    const prompt = ChatPromptTemplate.fromMessages([
      ['system', 'You are a senior software architect. Provide a concise technical summary.'],
      ['user', 'Explain the architectural tradeoffs of: {topic}'],
    ]);

    const chain = prompt.pipe(this.model).pipe(new StringOutputParser());

    return new Observable((subscriber) => {
      (async () => {
        try {
          const stream = await chain.stream({ topic });
          for await (const chunk of stream) {
            subscriber.next(chunk);
          }
          subscriber.complete();
        } catch (err) {
          subscriber.error(err);
        }
      })();
    });
  }
}
```

### 3. Stateful LangGraph Agent State Machine

```typescript
// src/ai/services/agent-workflow.service.ts
import { Injectable, Logger } from '@nestjs/common';
import { ConfigService } from '@nestjs/config';
import { ChatOpenAI } from '@langchain/openai';
import { StateGraph, END, START, Annotation } from '@langchain/langgraph';
import { BaseMessage, AIMessage, ToolMessage } from '@langchain/core/messages';
import { OrderLookupToolFactory } from '../tools/order-lookup.tool';

// Define typed workflow state using LangGraph Annotation
const AgentState = Annotation.Root({
  messages: Annotation<BaseMessage[]>({
    reducer: (curr, update) => curr.concat(update),
    default: () => [],
  }),
});

@Injectable()
export class AgentWorkflowService {
  private readonly logger = new Logger(AgentWorkflowService.name);
  private appGraph: any;

  constructor(
    private readonly configService: ConfigService,
    private readonly orderToolFactory: OrderLookupToolFactory,
  ) {
    this.initializeGraph();
  }

  private initializeGraph() {
    const orderTool = this.orderToolFactory.createTool();
    const tools = [orderTool];

    const model = new ChatOpenAI({
      modelName: 'gpt-4o',
      temperature: 0,
      apiKey: this.configService.getOrThrow<string>('OPENAI_API_KEY'),
    }).bindTools(tools);

    // Node 1: Call Model
    const callModel = async (state: typeof AgentState.State) => {
      const response = await model.invoke(state.messages);
      return { messages: [response] };
    };

    // Node 2: Execute Tools
    const executeTools = async (state: typeof AgentState.State) => {
      const lastMessage = state.messages[state.messages.length - 1] as AIMessage;
      const toolCalls = lastMessage.tool_calls || [];
      const toolResponses: ToolMessage[] = [];

      for (const call of toolCalls) {
        if (call.name === 'lookup_order_status') {
          const result = await orderTool.invoke(call.args as any);
          toolResponses.push(
            new ToolMessage({
              tool_call_id: call.id!,
              content: result,
            }),
          );
        }
      }
      return { messages: toolResponses };
    };

    // Conditional routing function
    const shouldContinue = (state: typeof AgentState.State) => {
      const lastMessage = state.messages[state.messages.length - 1] as AIMessage;
      if (lastMessage.tool_calls && lastMessage.tool_calls.length > 0) {
        return 'execute_tools';
      }
      return END;
    };

    // Build the LangGraph workflow
    const workflow = new StateGraph(AgentState)
      .addNode('agent', callModel)
      .addNode('execute_tools', executeTools)
      .addEdge(START, 'agent')
      .addConditionalEdges('agent', shouldContinue, {
        execute_tools: 'execute_tools',
        [END]: END,
      })
      .addEdge('execute_tools', 'agent'); // Cycle back to agent after tools

    this.appGraph = workflow.compile();
  }

  async runAgent(userPrompt: string): Promise<string> {
    const result = await this.appGraph.invoke({
      messages: [{ role: 'user', content: userPrompt }],
    });

    const finalMessage = result.messages[result.messages.length - 1];
    return finalMessage.content as string;
  }
}
```

---

## Production Patterns & Best Practices

### 1. Persistent Checkpointing with PostgreSQL
In long-running agent workflows (e.g., ticket resolution spanning human approval), persist execution state across worker restarts:

```typescript
import { PostgresSaver } from '@langchain/langgraph-checkpoint-postgres';
import { Pool } from 'pg';

const pool = new Pool({ connectionString: process.env.DATABASE_URL });
const checkpointer = new PostgresSaver(pool);
await checkpointer.setup(); // Creates checkpoints and checkpoint_writes tables

const app = workflow.compile({ checkpointer });
// Invoke with thread_id for state continuity
const result = await app.invoke(inputs, { configurable: { thread_id: 'session_user_42' } });
```

### 2. Guardrails and Input/Output Sanitization
Wrap tools and prompt outputs in NestJS validation pipes or Zod schemas. If an agent emits a structured tool argument that fails validation, catch the validation error and return it to the model as a `ToolMessage` with instructions to correct parameters, rather than crashing the HTTP request.

---

## Performance & Scale Considerations

- **Agent Recursion Limit**: Always configure `recursionLimit` (default is 25 in LangGraph) to prevent infinite loops when an agent repeatedly misfires tools:
  ```typescript
  await app.invoke(inputs, { recursionLimit: 10 });
  ```
- **Context Pruning**: Multi-turn agent loops accumulate large conversation arrays. Implement a trimming node using `@langchain/core/messages` `trimMessages()` to keep context within the model's optimal token window.

---

## Security & Failure Modes

- **Unrestricted Tool Permissions**: Never expose tools that perform destructive write operations (e.g., `deleteUser`, `refundOrder`) without human-in-the-loop validation or strict user role authorization checks.
- **Leaking Sensitive Model Keys in Tools**: Tools should never pass raw credentials or environment variables back to the model context.
- **Model Non-Determinism in Branching**: Guard conditional edges with explicit fallback branches to guarantee graph termination even when model formatting degrades.

---

## Anti-Patterns & Common Pitfalls

- **Creating New Model Instances per Request**: Instantiating `new ChatOpenAI()` on every HTTP turn causes memory leaks and destroys connection pooling. Manage models as NestJS singletons.
- **Blocking the Event Loop with Long Agent Loops**: Multi-step agent reasoning can take 10-30 seconds. Never execute long agent loops synchronously in the main HTTP request cycle; offload to background queues or stream via SSE.

---

## Testing Strategies

```typescript
// src/ai/services/agent-workflow.service.spec.ts
import { Test, TestingModule } from '@nestjs/testing';
import { AgentWorkflowService } from './agent-workflow.service';
import { ConfigService } from '@nestjs/config';
import { OrderLookupToolFactory } from '../tools/order-lookup.tool';

describe('AgentWorkflowService', () => {
  let service: AgentWorkflowService;

  beforeEach(async () => {
    const module: TestingModule = await Test.createTestingModule({
      providers: [
        AgentWorkflowService,
        {
          provide: ConfigService,
          useValue: { getOrThrow: jest.fn().mockReturnValue('mock-api-key') },
        },
        {
          provide: OrderLookupToolFactory,
          useValue: {
            createTool: jest.fn().mockReturnValue({
              name: 'lookup_order_status',
              invoke: jest.fn().mockResolvedValue(JSON.stringify({ status: 'DELIVERED' })),
            }),
          },
        },
      ],
    }).compile();

    service = module.get<AgentWorkflowService>(AgentWorkflowService);
  });

  it('should be defined', () => {
    expect(service).toBeDefined();
  });
});
```

---

## Migration & Version Evolution

- **LangChain 0.1 vs 0.2 / 0.3**: LangChain.js migrated core functionality into modular packages: `@langchain/core`, `@langchain/community`, `@langchain/openai`, and `@langchain/langgraph`. Legacy monolithic imports from `langchain/...` are deprecated.
- **StateGraph v0.2**: Modern LangGraph utilizes `Annotation.Root()` for state definition instead of untyped dictionary configurations.

---

## Real-World Case Study: Automated Support Agent

A multinational retail platform deployed an automated NestJS support agent handling return requests:
1. **Architecture**: LangGraph state machine with 3 nodes: Intent Classifier, Policy Checker, and Action Dispatcher.
2. **Tools**: Injected `OrdersService`, `ShippingLabelService`, and `RefundGateway`.
3. **Safety**: Return requests under $50 were automatically processed; requests above $50 transitioned to a `human_review` node pausing execution via Postgres checkpointing until a supervisor approved the action via web dashboard.
4. **Impact**: 68% of routine return requests were resolved autonomously with zero human intervention, cutting support response times from 14 hours to 30 seconds.

---

## Key Takeaways & Cheat Sheet

- Use **LCEL** (`.pipe()`) for linear, streaming transformation pipelines (prompts, retrievers, parsers).
- Use **LangGraph.js** for cyclic, multi-step agent reasoning loops with branches and tool execution.
- Register agent tools as NestJS providers so they inherit full access to domain services, repositories, and transaction managers.
- Always configure `recursionLimit` and state checkpointing to prevent runaway loops and ensure recoverability.
- Keep model instances as singletons in NestJS modules to preserve socket connections and token caches.

---

## Cross-References
- [01-openai-integration.md](file:///03-backend/nestjs/05-advanced/08-ai-and-llm/01-openai-integration.md) - OpenAI streaming and client configuration
- [03-rag.md](file:///03-backend/nestjs/05-advanced/08-ai-and-llm/03-rag.md) - Retrieval-Augmented Generation workflows
- [05-cqrs-module.md](file:///03-backend/nestjs/05-advanced/03-events-and-messaging/05-cqrs-module.md) - Command/Query responsibility segregation
