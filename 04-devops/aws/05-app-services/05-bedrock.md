# Bedrock: Foundation Models as an AWS Service

Amazon Bedrock gives you access to foundation models (text, chat, image, embeddings) from several providers, including Anthropic's Claude family, Amazon's own Nova models, Meta, Mistral and others, through **one AWS API**, with IAM for access control, CloudWatch/CloudTrail for observability, and your data staying inside AWS's security boundary. You don't host or scale anything: you call an API and pay per token.

Why use it instead of calling a model provider directly? Mostly **AWS-native integration**: IAM roles instead of API keys, VPC endpoints, consolidated billing, regional data handling, and a built-in toolbox (Guardrails, Knowledge Bases for RAG, agents). The trade-off: the newest model or feature sometimes arrives on the provider's own API first, and model IDs and availability vary by region.

This is the fastest-moving service in this repo. **Verify model IDs, regions and features in the Bedrock docs before building.** The patterns below are stable; the specific IDs are not.

Prerequisites: [IAM](../01-foundations/03-iam.md); [Lambda](../02-compute/02-lambda.md) for the serverless example; [S3](../03-storage-and-databases/01-s3.md) for RAG.

---

## The two API surfaces

| API | Use for |
|---|---|
| **Converse / ConverseStream** | **The default.** One uniform request/response format across models for chat, system prompts, images/documents, **tool use**, and streaming. Swap models by changing the model ID. |
| **InvokeModel / InvokeModelWithResponseStream** | Native, model-specific request bodies. Needed for some non-chat models (embeddings, image generation) and provider-specific features. |

Bedrock also exposes **provider-compatible endpoints** (for example Anthropic's Messages API shape and OpenAI-compatible endpoints) so existing SDK code can point at Bedrock. Those are convenient for porting, but Converse is the portable AWS-native choice.

Control plane vs runtime: management calls (list models, create guardrails, knowledge bases) use the **`bedrock`** service; inference uses **`bedrock-runtime`**. Mixing these up is a classic "operation doesn't exist" error.

---

## First call

```ts
import { BedrockRuntimeClient, ConverseCommand } from "@aws-sdk/client-bedrock-runtime";

const client = new BedrockRuntimeClient({ region: "us-east-1" });

const res = await client.send(new ConverseCommand({
  modelId: "global.anthropic.claude-sonnet-5",   // an inference profile ID (see below). Verify current IDs
  system: [{ text: "You are a concise assistant for an AWS-learning site." }],
  messages: [
    { role: "user", content: [{ text: "Explain SQS visibility timeout in two sentences." }] },
  ],
  inferenceConfig: { maxTokens: 300, temperature: 0.2 },
}));

console.log(res.output?.message?.content?.[0]?.text);
console.log(res.usage);          // { inputTokens, outputTokens, totalTokens }
console.log(res.stopReason);     // "end_turn" | "max_tokens" | "tool_use" | ...
```

Always check **`stopReason`**: `max_tokens` means the answer was cut off, and `tool_use` means the model wants you to run a tool (below). Always log **`usage`**, since tokens are what you pay for.

Streaming for responsive UIs uses `ConverseStreamCommand` and yields content deltas as they're generated:

```ts
import { ConverseStreamCommand } from "@aws-sdk/client-bedrock-runtime";

const stream = await client.send(new ConverseStreamCommand({ modelId, messages }));
for await (const chunk of stream.stream ?? []) {
  const text = chunk.contentBlockDelta?.delta?.text;
  if (text) process.stdout.write(text);
}
```

Find what's available to *your* account and region:

```bash
aws bedrock list-foundation-models --region us-east-1 \
  --query 'modelSummaries[].[providerName,modelId]' --output table
aws bedrock list-inference-profiles --region us-east-1
```

---

## Model IDs and inference profiles (the #1 source of confusion)

A model can be addressed several ways:

| Form | Example shape | Meaning |
|---|---|---|
| **Base model ID** | `anthropic.claude-sonnet-5` | Run in the region you call (where supported) |
| **Geographic inference profile** | `us.anthropic.…`, `eu.anthropic.…`, `apac.anthropic.…` | Cross-Region inference within a geography: Bedrock routes to a region with capacity for better throughput and resilience |
| **Global inference profile** | `global.anthropic.…` | Routes across commercial regions worldwide for maximum capacity |

Many newer models are offered **only** through inference profiles, and calling the bare model ID returns an error such as *"on-demand throughput isn't supported"*. In that case use the profile ID. Cross-Region inference means **your prompt may be processed in another region** within the profile's scope. If you have data-residency requirements, pick the in-region or geographic option deliberately and read the docs on where data is processed.

Model IDs include version suffixes and **models get deprecated on a lifecycle schedule** (legacy → end of life). Keep the model ID in **configuration**, not scattered in code, so upgrading is a config change, and subscribe to deprecation notices.

---

## Access and permissions

- **Model access:** Bedrock foundation models are now enabled by default in an account when the caller has the right IAM permissions (older docs describe a manual "Model access → Request access" step; that has changed). Some providers, notably Anthropic, may still ask you to submit use-case details the first time. If your first call returns `AccessDeniedException`, check both IAM and that provider step.
- **IAM:** the caller needs `bedrock:InvokeModel` (and `bedrock:InvokeModelWithResponseStream`) for Converse/streaming too. When using an inference profile, the policy must allow **both the profile ARN and the underlying foundation-model ARNs** (in every region the profile can route to). A frequent cause of "I allowed it but it's denied".

```json
{
  "Effect": "Allow",
  "Action": ["bedrock:InvokeModel", "bedrock:InvokeModelWithResponseStream"],
  "Resource": [
    "arn:aws:bedrock:*:111122223333:inference-profile/global.anthropic.claude-sonnet-5",
    "arn:aws:bedrock:*::foundation-model/anthropic.claude-sonnet-5"
  ]
}
```

- **Credentials:** use a **role** (Lambda execution role, ECS task role). Bedrock also supports **API keys** (bearer tokens) for quick starts; prefer short-lived keys or roles for anything real and store keys as secrets ([Security and Secrets](../06-operations/03-security-and-secrets.md)).
- **Privacy posture:** AWS states that prompts and completions are not used to train the underlying models and aren't shared with model providers. Verify the current data-protection documentation for your compliance needs. You can use **VPC interface endpoints** to keep traffic off the public internet ([VPC](../04-networking/01-vpc.md)) and enable **model invocation logging** (to CloudWatch/S3) for audit, remembering that logs may then contain sensitive prompt data.

---

## Tool use (function calling)

Converse lets the model request that *you* call a function, then you return the result and it continues:

```ts
const toolConfig = {
  tools: [{
    toolSpec: {
      name: "get_order_status",
      description: "Look up an order's status by id",
      inputSchema: { json: {
        type: "object",
        properties: { orderId: { type: "string" } },
        required: ["orderId"],
      } },
    },
  }],
};

let messages = [{ role: "user", content: [{ text: "Where is order o-123?" }] }];
let res = await client.send(new ConverseCommand({ modelId, messages, toolConfig }));

while (res.stopReason === "tool_use") {
  const assistantMsg = res.output!.message!;
  messages.push(assistantMsg);

  const results = [];
  for (const block of assistantMsg.content ?? []) {
    if (block.toolUse) {
      const out = await runTool(block.toolUse.name, block.toolUse.input);   // YOUR code, with validation
      results.push({ toolResult: { toolUseId: block.toolUse.toolUseId, content: [{ json: out }] } });
    }
  }
  messages.push({ role: "user", content: results });
  res = await client.send(new ConverseCommand({ modelId, messages, toolConfig }));
}
```

Rules: **the model never executes anything**. Your code does, so validate inputs, enforce authorisation as the *end user* (not as an all-powerful service role), and cap the loop iterations. Treat model output as untrusted input.

---

## Common building blocks

### Embeddings and RAG (Knowledge Bases)

**Retrieval-Augmented Generation** grounds answers in your own data: split documents into chunks, embed them as vectors, retrieve the most similar chunks for a question, and put them in the prompt.

```
docs in S3 ─► chunk ─► embedding model ─► vector store ─┐
                                                        ├─► top-k chunks ─► prompt ─► model ─► answer + citations
question ─► embedding model ─► similarity search ───────┘
```

**Bedrock Knowledge Bases** manages that pipeline: connect an S3 data source, choose an embedding model and a vector store (options include OpenSearch Serverless, Aurora PostgreSQL with pgvector, and S3 Vectors, among others, so check the current list), then query with `RetrieveAndGenerate` or just `Retrieve` (to build your own prompt). You can also build RAG yourself with embeddings from `InvokeModel` and a store such as pgvector on [Aurora](../03-storage-and-databases/03-rds-and-aurora.md); that gives more control, at the cost of more code. Retrieval quality (chunking, metadata filters, re-ranking) matters more than which model you pick.

### Guardrails

**Bedrock Guardrails** apply configurable safety policies to inputs and outputs: content filters, denied topics, word filters, PII detection/redaction, and grounding checks. They can be attached to Converse calls (`guardrailConfig`) or used standalone via `ApplyGuardrail` with any model, even outside Bedrock. They're a defence layer, not a guarantee. Keep your own validation, least privilege and monitoring too.

### Agents

Bedrock offers managed **agent** capabilities (and AgentCore services for building and operating agents at scale) that orchestrate multi-step tool use, memory and knowledge bases. These are evolving quickly, so read the current docs. For many applications, a simple **Converse tool-use loop in your own code** is easier to test and debug than a managed agent, so adopt the heavier abstraction only when you need what it gives you.

### Other features

- **Prompt caching**: reuse a long, stable prompt prefix (system prompt, documents) across calls at lower cost and latency, on supported models.
- **Batch inference**: cheaper, asynchronous processing of large offline jobs (input/output in S3).
- **Provisioned throughput**: reserved capacity for steady, high-volume workloads, a commitment you should justify with measured usage.
- **Prompt management / flows / evaluations / model customisation (fine-tuning)**: available for specific models; reach for them after prompting and RAG, not before.

---

## Production considerations

- **Throttling is normal.** Quotas (requests and tokens per minute, per model and region) apply; you'll see `ThrottlingException`. Use **exponential backoff with jitter** (the SDK retries some automatically), spread load via cross-Region inference profiles, queue bursty work through [SQS](./01-sqs.md), and request quota increases early. Quotas can differ a lot between models and new accounts.
- **Latency is seconds, not milliseconds.** Stream tokens to the UI, choose smaller/faster models where quality allows, and **don't put a model call synchronously behind an API Gateway request** with its ~29-second integration timeout ([API Gateway](../04-networking/03-load-balancing-and-api-gateway.md)). Prefer streaming (via a Lambda function URL with response streaming or an ALB/ECS service) or an async job pattern.
- **Cost = tokens in + tokens out** (output is typically priced higher). Control it: set `maxTokens`, trim conversation history, cache stable prefixes, route easy tasks to smaller models, and log `usage` per feature/tenant. Long contexts and agent loops multiply cost quickly.
- **Non-determinism:** the same prompt can yield different outputs; lower `temperature` reduces (not eliminates) variation. **Build evals**: a fixed set of example inputs with checks (exact match, schema validation, LLM-as-judge, human review), run before changing a prompt or upgrading a model.
- **Structured output:** ask for JSON, validate it with a schema (e.g. Zod), and retry or repair on failure, rather than trusting free text. Tool use with a schema is another way to get structured results.
- **Prompt injection:** any text from users, documents or web pages can contain instructions aimed at your model. Keep untrusted content clearly delimited, restrict what tools can do, never let model output directly drive privileged actions, and apply Guardrails where appropriate.
- **Observability:** log request IDs, model ID, token usage, latency, stop reason and errors (and prompts/completions only if your data policy allows). CloudWatch publishes Bedrock runtime metrics such as invocations, latency and throttles. See [Observability](../06-operations/04-observability.md).
- **Model lifecycle:** pin versions, keep IDs in config, test replacements via your eval set, and schedule migrations ahead of end-of-life dates.

A minimal Lambda wrapper (note the client is created **outside** the handler, as always, see [Lambda](../02-compute/02-lambda.md)):

```ts
import { BedrockRuntimeClient, ConverseCommand } from "@aws-sdk/client-bedrock-runtime";
const client = new BedrockRuntimeClient({});
const MODEL_ID = process.env.MODEL_ID!;

export const handler = async (event) => {
  const { question } = JSON.parse(event.body ?? "{}");
  const res = await client.send(new ConverseCommand({
    modelId: MODEL_ID,
    messages: [{ role: "user", content: [{ text: String(question).slice(0, 4000) }] }],
    inferenceConfig: { maxTokens: 500, temperature: 0.2 },
  }));
  return {
    statusCode: 200,
    body: JSON.stringify({ answer: res.output?.message?.content?.[0]?.text, usage: res.usage }),
  };
};
```

Set the Lambda timeout generously (model calls take seconds) and give its role only the specific `bedrock:InvokeModel` ARNs it needs.

---

## Common mistakes

- Calling a **base model ID** that requires an **inference profile**.
- IAM policy allowing the model but **not the inference-profile ARN** (or the underlying regions).
- **Wrong region** for the model/profile, or assuming every model is in every region.
- Hardcoding **model IDs** across the codebase, then scrambling at deprecation time.
- Ignoring **`stopReason`** (truncated output treated as complete) and **`usage`** (surprise bills).
- **Unbounded conversation history**, so cost and latency grow with each turn.
- **Synchronous model calls behind a short timeout**, or blocking UIs without streaming.
- No **backoff** on throttling; no queue for bursts.
- Trusting **model output** as safe or well-formed: no schema validation, no authorisation on tool calls.
- **Prompt injection** via retrieved documents or user content.
- Shipping prompt changes with **no evals**.
- Logging full prompts containing **PII** without a policy.
- Over-building (agents, fine-tuning) before trying **good prompts + RAG**.
- Mixing up **`bedrock`** (control plane) and **`bedrock-runtime`** clients.

---

## Debugging

| Symptom / error | Likely cause |
|---|---|
| `AccessDeniedException` | IAM lacks `bedrock:InvokeModel` on the **profile + foundation-model ARNs**; provider first-time access/use-case form; SCP blocking Bedrock or a region |
| `ValidationException: … on-demand throughput isn't supported` | Use an **inference profile** ID instead of the base model ID |
| `ResourceNotFoundException` / model not found | Wrong ID or region; model **deprecated/legacy**; not available in that region |
| `ValidationException` on the request | Malformed Converse payload: roles must alternate sensibly, content is a list of blocks, `maxTokens` exceeds the model's limit, input too long for the context window |
| `ThrottlingException` / `ServiceQuotaExceededException` | Rate/token quota: backoff, queue, other regions/profiles, quota increase |
| `ModelTimeoutException` / client timeout | Long generations; raise SDK/Lambda timeouts, stream, reduce `maxTokens` |
| Truncated answer | `stopReason = max_tokens`: raise the limit or ask for brevity |
| Answers ignore your data (RAG) | Retrieval is poor: check chunking, the embedding model matches at index and query time, metadata filters, `numberOfResults`; inspect retrieved chunks with `Retrieve` |
| Output not valid JSON | Use schema validation + retry, or tool use with an input schema |
| Responses blocked/refused | Guardrail intervened (check `stopReason`/trace) or the model's own safety refusal; adjust the guardrail or reword the task |
| Unexpectedly high bill | Token usage per request (history growth, giant retrieved context, agent loops), wrong model tier, no caching |

---

## Quick Summary

- Bedrock = many foundation models behind one AWS API with IAM, VPC endpoints, and AWS-native tooling. Use **Converse / ConverseStream** by default; **`bedrock-runtime`** for inference, **`bedrock`** for management.
- Newer models are usually called through **inference profiles** (`us.` / `eu.` / `global.` prefixes): they route across regions, so mind data residency, and IAM must allow the **profile and foundation-model ARNs**.
- Keep **model IDs in config**, watch **deprecations**, and check **`stopReason`** and **`usage`** on every call.
- **Tool use**: the model asks, **your code executes**, so validate and authorise. Treat output (and retrieved content) as untrusted.
- Building blocks: **Knowledge Bases** (managed RAG), **Guardrails**, agents, **prompt caching**, **batch inference**.
- Production: backoff for **throttling**, stream for latency, cap tokens for cost, **evals** for quality, schema-validate structured output, defend against **prompt injection**.
- Models, IDs, regions and features change fast. Re-check the docs before building.

**Next:** [Infrastructure as Code](../06-operations/01-infrastructure-as-code.md)
