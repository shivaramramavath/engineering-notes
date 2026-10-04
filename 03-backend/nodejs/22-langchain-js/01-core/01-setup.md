# Setup

LangChain JS is a set of packages for building LLM apps: a common interface over model providers, plus the pieces you compose around them (prompts, parsers, retrievers, tools, agents). Setup is mostly about understanding **which package does what**, because that is where most install and version errors come from.

> Checked against `langchain@1.5.15` (npm `latest` when this note was written). Requires **Node.js 20+**. Package versions move fast, so re-check `npm view langchain version` if something here looks off.

---

## The package layout

LangChain is split so you only install the provider you use.

```text
your app
  ├── langchain            high-level APIs (agents, universal model loader)
  ├── @langchain/core      base abstractions: Runnable, messages, prompts, tools
  └── @langchain/<provider>   one per provider: openai, anthropic, ...
```

| Package | Role | Install it when |
|---|---|---|
| `@langchain/core` | Base interfaces and primitives (`Runnable`, messages, prompt templates, output parsers) | Always. Providers and `langchain` depend on it |
| `langchain` | Higher-level building blocks on top of core | You use agents or `initChatModel` |
| `@langchain/openai`, `@langchain/anthropic`, ... | Chat models and embeddings for one provider | You call that provider |

`langchain` declares `@langchain/core` as a **peer dependency**, so you install both yourself.

---

## Install

```bash
mkdir lc-demo && cd lc-demo
npm init -y
npm install langchain @langchain/core @langchain/openai
npm install -D typescript tsx @types/node
```

Swap `@langchain/openai` for `@langchain/anthropic` (or install both). The same commands work with `pnpm add` / `yarn add`.

Make the project ESM, since `langchain` itself ships as an ES module with a CJS build as well:

```json
{
  "type": "module"
}
```

---

## API keys

Providers read keys from environment variables. Don't hard-code them.

```bash
# .env  (add to .gitignore)
OPENAI_API_KEY=sk-...
ANTHROPIC_API_KEY=sk-ant-...
```

Node 20.6+ can load the file with no extra package:

```bash
npx tsx --env-file=.env src/hello.ts
```

Or use `dotenv` (`import "dotenv/config"` at the top of your entry file) if you prefer.

---

## First call

```ts
// src/hello.ts
import { ChatOpenAI } from "@langchain/openai";

const model = new ChatOpenAI({ model: "gpt-4o", temperature: 0 });

const res = await model.invoke("Say hello in five words.");
console.log(res.text);
```

What happened:

- `invoke` takes a string (or a list of messages) and returns an **`AIMessage`**, not a plain string. The text is on `.text` (`.content` is the raw payload, which can be a string or content blocks).
- `ChatOpenAI` reads `OPENAI_API_KEY` from the environment automatically.
- Model names change over time; use whatever your provider currently offers.

Same shape with Anthropic, only the class and model id differ:

```ts
import { ChatAnthropic } from "@langchain/anthropic";

const model = new ChatAnthropic({ model: "<anthropic-model-id>", temperature: 0 });
```

That identical `invoke` interface across providers is the main reason to use LangChain at all.

---

## Provider-agnostic loading

If you want to choose the provider by config rather than by import, `langchain` ships a universal loader:

```ts
import { initChatModel } from "langchain";

const model = await initChatModel("gpt-4o");              // provider inferred from the name
const model2 = await initChatModel("openai:gpt-4o");      // or be explicit with "provider:model"
```

You still need the matching provider package installed. The loader picks the class; it does not bundle the SDK.

---

## Tracing with LangSmith (optional, worth enabling early)

LangSmith records every model call, prompt and latency, which makes debugging chains far easier later.

```bash
LANGSMITH_TRACING=true
LANGSMITH_API_KEY=lsv2_...
```

No code changes needed. LangChain components pick these up automatically. Covered properly in `04-production/01-tracing-and-debugging.md`.

---

## Common mistakes

| Symptom | Cause | Fix |
|---|---|---|
| Error about missing API key | `.env` not loaded | Use `--env-file=.env` or `import "dotenv/config"` before creating the model |
| Type or runtime errors between packages, "instanceof" failing on messages | Two different `@langchain/core` versions installed | Run `npm ls @langchain/core`; dedupe so one version is resolved (peer range for `langchain@1.5.15` is `^1.2.14`) |
| `Cannot find module '@langchain/openai'` | Provider package not installed | Install the provider package explicitly; `langchain` does not include it |
| Syntax or `require` errors | Old Node or wrong module mode | Use Node 20+ and `"type": "module"` |
| `res` prints as an object | `invoke` returns `AIMessage` | Use `res.text` (or `res.content`) |
| An old tutorial's imports don't resolve | Written for an older major version | Import models from `@langchain/<provider>`, primitives from `@langchain/core/...`, and check the current docs |

### Debugging checklist

1. `node -v` is 20 or higher.
2. `npm ls @langchain/core` shows a single version.
3. The key is actually in `process.env` (print `Boolean(process.env.OPENAI_API_KEY)`, never the key itself).
4. Try the smallest possible `model.invoke("hi")` before debugging a bigger chain.

---

## Quick Summary

- Install `langchain`, `@langchain/core`, and **one package per provider**.
- Node 20+, ESM project, keys in environment variables.
- `model.invoke(...)` returns an `AIMessage`; read `.text`.
- One `@langchain/core` version only. Duplicates cause confusing errors.
- Turn on LangSmith tracing early.

**Next:** [Models and Messages](./02-models-and-messages.md)
