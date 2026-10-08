# LangChain JS Interview Questions

Short, accurate answers you could say out loud. Each links to the note that covers it in depth. Version context: `langchain@1.x`, `@langchain/core@1.x`, Node 20+.

---

## Fundamentals

**What is LangChain, and what problem does it solve?**
A set of libraries that gives you one interface over many model providers, plus the building blocks around a model: prompts, output parsing, retrieval, tools and agents. The value is swapping providers with little code change and having standard pieces (streaming, tracing, retries) instead of hand-rolling them. → [Setup](../01-core/01-setup.md)

**Which packages do you install, and why are they split?**
`@langchain/core` (base abstractions), `langchain` (higher-level APIs such as agents and `initChatModel`), and one package per provider such as `@langchain/openai`. They are split so you only install the providers you use, and so provider releases don't force core releases. `langchain` has `@langchain/core` as a peer dependency, so you install both. → [Setup](../01-core/01-setup.md)

**What goes wrong if two versions of `@langchain/core` are installed?**
Classes from one copy fail `instanceof` checks against the other, producing confusing type and runtime errors with messages and tools. Fix with `npm ls @langchain/core` and dedupe to one version.

**`initChatModel` vs `new ChatOpenAI(...)`?**
`initChatModel("provider:model")` picks the class from a string, which suits config-driven provider choice. The provider class gives direct access to provider-specific options. Both need the provider package installed. → [Models and Messages](../01-core/02-models-and-messages.md)

---

## Models and messages

**Are chat models stateful?**
No. A model only sees the messages you pass. "Memory" means you keep the list and resend it.

**What does `model.invoke()` return?**
An `AIMessage`, not a string. Use `.text` for the text. `.content` is the raw payload and can be a string or an array of content blocks. Other useful fields: `tool_calls`, `usage_metadata`, `response_metadata`, `contentBlocks`.

**What are the message types?**
`SystemMessage`, `HumanMessage`, `AIMessage`, `ToolMessage`. Plain `{ role, content }` objects are also accepted.

**What are content blocks?**
A provider-agnostic representation of message content (text, reasoning, tool calls, images, files). `message.contentBlocks` normalizes provider formats so one code path handles, for example, Anthropic thinking blocks and OpenAI reasoning summaries.

**How do you track token usage?**
Read `usage_metadata` on the `AIMessage` (`input_tokens`, `output_tokens`, `total_tokens`, plus cache and reasoning details when available). Log it per request.

**What are `invoke`, `stream` and `batch`?**
One response, chunks as generated, and many inputs in parallel (cap with `maxConcurrency`).

---

## Prompts

**Why use a prompt template instead of string interpolation?**
Variable validation, reuse, brace-safe handling, and it is a Runnable, so it composes with `.pipe()` and shows as its own step in traces. → [Prompt Templates](../01-core/03-prompt-templates.md)

**What is `MessagesPlaceholder` for?**
Injecting a list of messages (such as chat history) into a prompt, as opposed to a string variable.

**How do you put literal `{}` in a template?**
Double them: `{{` and `}}`. Unescaped braces are the most common template bug.

---

## Runnables and LCEL

**What is a Runnable?**
Anything with `invoke`, `stream` and `batch`. Models, prompts, parsers, retrievers and tools are all Runnables, and a chain is itself a Runnable. → [Runnables and LCEL](../01-core/04-runnables-and-lcel.md)

**What does LCEL give you?**
Composition with `.pipe()`, and, for free: streaming through the chain, batching, per-step tracing, and wrappers like `withRetry` and `withFallbacks`.

**Name some composition primitives.**
`RunnableLambda` (wrap a function), `RunnableParallel` (fan out and merge into an object), `RunnablePassthrough.assign` (keep the input and add keys), `RunnableSequence`.

**When would you not use LCEL?**
For loops, tool-use cycles, long-lived state or complex branching. Use plain code, an agent, or a graph. For a one-off call plain code is simpler.

**Why might a chain not stream token by token?**
A middle step needs the whole input before producing output, so it buffers. Streaming works end to end only if every step passes chunks along.

---

## Structured output

**How do you get typed JSON from a model?**
`model.withStructuredOutput(zodSchema)`. It returns a validated object instead of an `AIMessage`. → [Structured Output](../01-core/05-structured-output.md)

**How does it work under the hood?**
Either the provider's native structured-output API (most reliable) or tool calling, where the schema is a tool the model must call.

**Does raw JSON Schema get validated?**
No. Zod and Standard Schema are validated at runtime; with raw JSON Schema you validate yourself.

**Does valid output mean correct output?**
No. Validation guarantees shape and types, not factual accuracy. Make unknowns representable with `.nullable()` or `.optional()` or the model will guess.

**How do agents do structured output?**
`createAgent({ responseFormat })`, with `providerStrategy` or `toolStrategy`; the result is in `structuredResponse`.

---

## Streaming

**How do you stream from a chat model?**
`for await (const chunk of await model.stream(...))`, reading `chunk.text`. Chunks are `AIMessageChunk`s you can combine with `.concat()`.

**Why not `JSON.parse` a streamed tool-call chunk?**
Tool-call args arrive as partial JSON. Accumulate the chunks first.

**`stream` vs `streamEvents`?**
`stream` yields the final step's output. `streamEvents` yields typed events from every step, so you can see intermediate progress or filter by component.

**What are the agent stream modes?**
`updates` (state after each step), `messages` (LLM tokens plus metadata), `custom` (data tools emit). There is also a newer event-streaming API; check the current docs for its shape. → [Streaming](../01-core/06-streaming.md)

---

## Memory and chat history

**How does conversation memory work in LangChain JS 1.x?**
With an agent, a `checkpointer` plus a `thread_id` persists the message state per conversation. Without agents, you keep the message list yourself (for example through `MessagesPlaceholder`). → [Chat History](../01-core/07-chat-history.md)

**Why not use `MemorySaver` in production?**
It lives in process memory and disappears on restart or across instances. Use a database-backed checkpointer such as `PostgresSaver`.

**How do you stop history from growing without bound?**
Trim (`trimMessages`), delete, or summarize (`summarizationMiddleware`). Keep tool-call and tool-result messages together, and start with a human message where the provider requires it.

**Is `RunnableWithMessageHistory` still the way?**
It's the older pattern. The current docs route conversation memory through checkpointers and `thread_id`.

---

## RAG

**Explain the RAG pipeline.**
Index once: load, split, embed, store. Per question: embed the query, retrieve top-k chunks, put them in the prompt, generate. → [Retrieval and RAG](../02-rag/03-retrieval-and-rag.md)

**What is an embedding?**
A fixed-length vector that represents meaning; similar texts land close together. Use the same model for indexing and querying, and re-embed if you change it.

**How do you choose chunk size?**
By content and by measurement. `RecursiveCharacterTextSplitter` with `chunkSize` (characters by default) and `chunkOverlap` is the starting point; tune against real questions. → [Loading and Splitting](../02-rag/01-loading-and-splitting.md)

**Why keep metadata on documents?**
Citations, filtering (tenant, date, access), and updates or deletes later. `splitDocuments` copies metadata onto each chunk.

**What does a retriever return, and what is MMR?**
A list of `Document`s for a query string; retrievers are Runnables. MMR (maximal marginal relevance) trades some similarity for diversity to avoid near-duplicate chunks.

**2-step RAG vs agentic RAG?**
2-step always retrieves before the model answers: fast, predictable, easy to test. Agentic RAG exposes search as a tool so the model decides when and how often to search: flexible but slower, costlier and less predictable.

**A RAG answer is wrong. How do you debug?**
Look at the retrieved chunks first. Is the answer in the index, was it retrieved, was it in the prompt, did the model use it? Fix the first "no". Most failures are retrieval failures.

**Limits of vector search?**
Exact identifiers and rare terms retrieve poorly (hybrid search helps), aggregation questions aren't retrieval problems, and chunks lacking context embed poorly.

**Are similarity scores comparable across stores?**
No. Meaning (similarity vs distance) and scale vary by store; calibrate on your own data.

---

## Tools and agents

**What is a tool?**
A function plus a name, description and Zod schema. The model reads the metadata and can request a call; it never executes anything itself. → [Tools](../03-tools-and-agents/01-tools.md)

**Walk through the tool-calling loop.**
Model returns an `AIMessage` with `tool_calls`; you run each and append a `ToolMessage` with the matching `tool_call_id`; you call the model again and it writes the final answer. An agent automates this loop.

**What does `returnDirect` do?**
Ends the agent loop after the tool and returns its output without another model call. Don't use it when the result needs reasoning.

**Why does the description matter so much?**
It is the model's main signal for when to use the tool and how to fill arguments; it is effectively a prompt.

**What is an agent, and agent vs chain?**
A model in a loop with tools: decide, call, read result, repeat. Use a chain when the steps are known; use an agent when the path depends on the input. Agents cost more calls, latency and predictability. → [Agents](../03-tools-and-agents/02-agents.md)

**What is middleware in `createAgent`?**
Hooks into the loop (`beforeModel`, `afterModel`, `wrapModelCall`, `wrapToolCall`). Built-ins cover call limits, retries, human approval, summarization and PII handling.

---

## Production

**How do you debug a misbehaving chain or agent?**
Trace it (LangSmith: set `LANGSMITH_TRACING` and `LANGSMITH_API_KEY`) and walk top-down: input, rendered prompt, retrieval, model call, tools, output handling. Stop at the first wrong step. → [Tracing and Debugging](../04-production/01-tracing-and-debugging.md)

**Traces are missing in serverless. Why?**
They're sent in the background and the function freezes first. Call `awaitAllCallbacks()` in `finally` and set `LANGCHAIN_CALLBACKS_BACKGROUND=false`.

**How do you test LLM apps?**
Layers: model-free unit tests (prompt rendering, tools, parsers), a few tolerant integration tests, and dataset-based evaluations with code, reference-based or LLM-judge evaluators. Evaluate RAG retrieval and generation separately. → [Testing and Evaluation](../04-production/02-testing-and-evaluation.md)

**What are the cautions with LLM-as-judge?**
Judges have biases and make mistakes. Give specific criteria, calibrate against human labels, and prefer several narrow evaluators.

**What retry behavior do chat models have?**
They retry network errors, 429s and 5xx with exponential backoff, 6 attempts by default; 401 and 404 aren't retried. Don't stack another retry layer on the same errors. → [Reliability and Cost](../04-production/03-reliability-and-cost.md)

**How do you control cost?**
Measure with `usage_metadata`, then shrink inputs (trim, fewer chunks), cap outputs (`maxTokens`), right-size models, use prompt caching with a stable prefix first, and avoid unnecessary agent loops.

**How do you prevent a runaway agent?**
Model and tool call limit middleware, plus timeouts and `maxTokens`.

---

## Security

**What is prompt injection, and what is indirect injection?**
Direct: the user's text tries to override instructions. Indirect: ingested content (documents, web pages, tool results) carries instructions the model follows. There is no complete prompt-level fix, so rely on architecture. → [Security](../04-production/04-security.md)

**How do you stop one tenant seeing another's data in a RAG or tool app?**
Enforce in code: identity from server-side `context` (never model-chosen arguments), metadata filters on every retrieval, authorization checks inside tools. The prompt is not a control.

**What are the core defensive principles?**
Least privilege and defense in depth: read-only scoped credentials, sandboxing, human approval for destructive actions, output validation, monitoring.

**What's risky about model output?**
It is untrusted: escape it before rendering as HTML, never eval it, parameterize SQL, and re-validate structured output against business rules.

**Why is `load()` from `@langchain/core/load` dangerous?**
It instantiates classes and runs constructors, so calling it on untrusted input is unsafe.

---

## Serving

**How do you keep per-user conversations separate?**
Authenticate, then build `thread_id` from the authenticated user id (never use a client-supplied id as-is), and use a persistent checkpointer. → [Serving and Integration](../04-production/05-serving-and-integration.md)

**Streaming works locally but arrives in one piece in production. Why?**
A proxy or CDN is buffering the response, or the platform limits streamed responses. Check buffering settings.

**What changes in serverless?**
Create clients at module scope, no in-memory state, flush traces, respect duration limits, and verify package compatibility with the runtime.

---

## Scenario questions

**Design a support bot over company docs.**
Ingest with cleaning and metadata (source, section, access level), `RecursiveCharacterTextSplitter`, persistent vector store, 2-step RAG with "answer only from context, else say you don't know", return sources, tenant filters in code, trace everything, and an evaluation dataset with out-of-scope and injection cases. Move to agentic RAG only if questions need multiple searches or other tools.

**Your agent loops and burns tokens. What do you do?**
Open the trace to see the repeating calls, then add call-limit middleware, tighten tool descriptions and errors so the model can recover, cap `maxTokens`, and add the case to your evaluation set.

**Answers got worse after a prompt change. How do you catch that earlier?**
Run an evaluation dataset on every prompt, model or retrieval change, compare experiments, inspect individual regressions, and add production failures to the dataset.

---

## Quick Summary

- Know the pieces: model, messages, prompt, Runnable/LCEL, structured output, streaming, memory, RAG, tools, agents.
- Know the failure modes: stateless models, duplicate core versions, retrieval failures, unbounded loops, injection.
- Be ready to say where enforcement lives: in code, not in prompts.
- Be honest about moving parts: the agent streaming and middleware APIs are the newest and change fastest.

**See also:** [Cheatsheet](./cheatsheet.md)