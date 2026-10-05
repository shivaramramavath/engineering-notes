# Bedrock

Amazon Bedrock is a managed API for foundation models (text, chat, embeddings, images) from several providers, behind one AWS-style interface: IAM for auth, CloudWatch for metrics, no model servers to run. From Node you call it like any other AWS service, and the main decision is which API shape to use: the model-agnostic **Converse** API, or the model-specific **InvokeModel**.

Prerequisites: [SDK v3 and credentials](../01-setup/01-sdk-v3-and-credentials.md).

```bash
npm install @aws-sdk/client-bedrock-runtime
# optional: LangChain.js integration
npm install @langchain/aws @langchain/core
```

> Model IDs, availability per region and pricing change often. This note never hard-codes a model ID; it reads one from config. Look up current IDs in the Bedrock console or API (below), and treat anything model-specific as "check the provider's docs".

---

## Two clients, two jobs

| Package | Purpose |
|---|---|
| `@aws-sdk/client-bedrock-runtime` | **Calling** models: `Converse`, `ConverseStream`, `InvokeModel`. What your app uses |
| `@aws-sdk/client-bedrock` | **Control plane**: list models and inference profiles, manage guardrails and customization. Used by tooling and scripts |

Find what you can call in your region:

```ts
import { BedrockClient, ListFoundationModelsCommand } from "@aws-sdk/client-bedrock";

const b = new BedrockClient({ region: process.env.AWS_REGION });
const { modelSummaries } = await b.send(new ListFoundationModelsCommand({}));
console.log(modelSummaries?.map((m) => m.modelId));
```

---

## Before it works: access and region

- **Model availability is per region.** A model ID that works in one region may not exist in yours.
- Depending on the model and account, you may have to **enable access** to the model (and for some providers accept terms or submit a use-case form) in the Bedrock console before calls succeed. An `AccessDeniedException` on a correct IAM policy often means model access, not IAM.
- Some models can't be called on demand with the plain model ID and need a **cross-region inference profile**. You pass the **inference profile ID** (it has a geographic prefix) as `modelId`. A `ValidationException` saying the model doesn't support on-demand throughput usually means exactly this. List them with `ListInferenceProfilesCommand` from the control-plane client.

---

## Converse: the default API

`Converse` gives one request/response format across models, so switching models is mostly changing `modelId`.

```ts
import { BedrockRuntimeClient, ConverseCommand } from "@aws-sdk/client-bedrock-runtime";

const bedrock = new BedrockRuntimeClient({ region: process.env.AWS_REGION });
const modelId = process.env.BEDROCK_MODEL_ID!; // model ID or inference profile ID

const res = await bedrock.send(new ConverseCommand({
  modelId,
  system: [{ text: "You are a concise assistant for a billing product." }],
  messages: [
    { role: "user", content: [{ text: "Explain proration in two sentences." }] },
  ],
  inferenceConfig: { maxTokens: 300, temperature: 0.2 },
}));

const text = res.output?.message?.content?.[0]?.text;
console.log(text);
console.log(res.stopReason, res.usage); // { inputTokens, outputTokens, totalTokens }
```

Points to know:

- `content` is an **array of blocks** (text, image, document, tool use, tool result), not a string. Read `content` blocks by type rather than assuming `[0].text` exists; a response can contain a tool-use block instead.
- `stopReason` tells you why it ended: `end_turn`, `max_tokens`, `tool_use`, `stop_sequence`, `guardrail_intervened`, … Handle `max_tokens` (truncated output) explicitly.
- **Always set `maxTokens`.** It caps cost and runaway output. `usage` is how you do cost accounting; log it.
- A conversation is stateless: to continue it, send the **full history** each call, appending the assistant message from the response and the next user message.

```ts
const history: Message[] = [/* user, assistant, ... */];
history.push(res.output!.message!);                       // the assistant turn
history.push({ role: "user", content: [{ text: "Now a one-line version." }] });
```

(`Message` is exported from `@aws-sdk/client-bedrock-runtime`.)

### Streaming

`ConverseStreamCommand` returns an async iterable of events. Stream when a human is watching the output.

```ts
import { ConverseStreamCommand } from "@aws-sdk/client-bedrock-runtime";

const out = await bedrock.send(new ConverseStreamCommand({ modelId, messages, inferenceConfig: { maxTokens: 500 } }));

let usage;
for await (const ev of out.stream!) {
  const delta = ev.contentBlockDelta?.delta?.text;
  if (delta) process.stdout.write(delta);
  if (ev.metadata?.usage) usage = ev.metadata.usage; // final token counts arrive in the last events
}
```

In an HTTP API you pipe these deltas to the client as server-sent events or a chunked response. API Gateway's Lambda integrations don't stream by default, so streaming to browsers usually means ECS/Express, or Lambda response streaming (see [Lambda handlers](../03-lambda/01-lambda-handlers.md)).

### Tool use (function calling)

Declare tools; when the model wants one it stops with `stopReason: "tool_use"` and a `toolUse` block. You run the function and send the result back.

```ts
const toolConfig = {
  tools: [{
    toolSpec: {
      name: "get_order_status",
      description: "Look up the status of an order by id",
      inputSchema: { json: { type: "object", properties: { orderId: { type: "string" } }, required: ["orderId"] } },
    },
  }],
};

const messages: Message[] = [{ role: "user", content: [{ text: "Where is order 01J9A?" }] }];

while (true) {
  const r = await bedrock.send(new ConverseCommand({ modelId, messages, toolConfig, inferenceConfig: { maxTokens: 500 } }));
  const msg = r.output!.message!;
  messages.push(msg);
  if (r.stopReason !== "tool_use") break;

  const results = [];
  for (const block of msg.content ?? []) {
    if (block.toolUse) {
      const data = await getOrderStatus(block.toolUse.input as { orderId: string }); // YOUR code
      results.push({ toolResult: { toolUseId: block.toolUse.toolUseId, content: [{ json: data }] } });
    }
  }
  messages.push({ role: "user", content: results });
}
```

Validate the model's `input` before using it (it's model-generated JSON, so treat it as untrusted), and cap loop iterations in real code.

---

## InvokeModel: when you need the raw API

`InvokeModelCommand` sends a **model-specific JSON body** as bytes and returns bytes. Use it for things Converse doesn't cover (embedding models, image generation, some provider-specific features).

```ts
import { InvokeModelCommand } from "@aws-sdk/client-bedrock-runtime";

const res = await bedrock.send(new InvokeModelCommand({
  modelId: process.env.BEDROCK_EMBED_MODEL_ID!,
  contentType: "application/json",
  accept: "application/json",
  body: JSON.stringify(/* request body in THAT model's documented format */),
}));

const parsed = JSON.parse(new TextDecoder().decode(res.body));
```

The body schema differs per model and provider, so follow that model's documentation in the Bedrock user guide. `InvokeModelWithResponseStream` is the streaming variant.

---

## Via LangChain.js

LangChain's AWS package wraps the Converse API as a chat model, so you can drop Bedrock into chains, retrievers and agents.

```ts
import { ChatBedrockConverse } from "@langchain/aws";

const model = new ChatBedrockConverse({
  model: process.env.BEDROCK_MODEL_ID!,
  region: process.env.AWS_REGION,
  temperature: 0,
  maxTokens: 500,
});

const res = await model.invoke([
  ["system", "You answer in one sentence."],
  ["human", "What is a dead-letter queue?"],
]);
console.log(res.content);

for await (const chunk of await model.stream("Name three AWS messaging services.")) {
  process.stdout.write(typeof chunk.content === "string" ? chunk.content : "");
}
```

Embeddings for retrieval use `BedrockEmbeddings` from the same package (pass the embedding model ID and region).

Credentials come from the default chain, the same as everywhere else, so no extra setup in Lambda/ECS or with an SSO profile locally.

When to use which:

| Use the raw SDK when | Use LangChain.js when |
|---|---|
| You make plain model calls, want few dependencies, need every Converse feature immediately | You're building chains, RAG pipelines or agents and want its abstractions, vector-store and tool integrations |
| You want precise control over retries, streaming and usage accounting | You want to swap model providers behind one interface |

LangChain.js's API evolves quickly; check its current docs for option names rather than trusting examples from older tutorials.

---

## Production notes

- **Throttling is normal.** Models have per-region quotas (requests and tokens per minute). Expect `ThrottlingException`; retry with backoff ([errors and retries](../04-production/01-errors-and-retries.md)) and request quota increases if sustained.
- **Latency and timeouts.** Long generations can run for tens of seconds. Raise client request timeouts and the Lambda/API Gateway timeouts to match, or stream.
- **Cost = tokens.** Billed per input and output token. Cap `maxTokens`, trim history, and log `usage` per request to spot expensive callers.
- **Guardrails** (a Bedrock feature) can filter content and PII; you attach one via `guardrailConfig` on Converse. Treat model output as untrusted text regardless.
- **Prompts and responses are application data.** Decide what you log; don't write user prompts to logs by accident.
- Keep model IDs in config so you can change them without a deploy, and test new models behind a flag.

---

## Permissions

```json
{
  "Effect": "Allow",
  "Action": ["bedrock:InvokeModel", "bedrock:InvokeModelWithResponseStream"],
  "Resource": [
    "arn:aws:bedrock:ap-south-1::foundation-model/<model-id>",
    "arn:aws:bedrock:ap-south-1:111122223333:inference-profile/<profile-id>"
  ]
}
```

- `Converse` and `ConverseStream` are authorized by the same `bedrock:InvokeModel` / `InvokeModelWithResponseStream` actions.
- With an **inference profile**, allow the profile *and* the underlying foundation-model ARNs in every region the profile routes to; a missing region is a classic `AccessDeniedException`.
- Foundation-model ARNs have no account ID (note the empty field).

---

## Common mistakes and debugging

| Symptom | Likely cause | Fix |
|---|---|---|
| `AccessDeniedException` with a correct-looking policy | Model access not enabled for the account/region, or inference-profile regions not allowed | Enable model access; add all relevant ARNs |
| `ValidationException`: model doesn't support on-demand / invalid model identifier | Needs an inference profile ID, or the ID doesn't exist in this region | `ListInferenceProfiles` / `ListFoundationModels` in your region |
| `ResourceNotFoundException` | Wrong model ID or region | Check ID per region |
| `ThrottlingException` | Quota hit | Backoff, reduce concurrency, request quota |
| Empty or odd `content[0].text` | Response block was a tool-use block | Branch on block type and `stopReason` |
| Output cut off mid-sentence | `stopReason: "max_tokens"` | Raise `maxTokens` or ask for shorter output |
| Works locally, fails in Lambda | Different identity/region, or timeout too low | `GetCallerIdentity`; raise Lambda timeout |
| `body` is bytes, `JSON.parse` fails | Forgot to decode | `new TextDecoder().decode(res.body)` |
| Different models behave differently with the same prompt | They're different models | Test prompts per model; keep the model in config |

---

## Quick summary

- `client-bedrock-runtime` to call models, `client-bedrock` to list/manage them.
- Use **Converse** (and `ConverseStream`) as the default: one format across models. Drop to **InvokeModel** for model-specific cases like embeddings.
- Messages hold content *blocks*; check `stopReason`; always set `maxTokens`; log `usage`.
- Tool use is a loop: model returns `tool_use`, you run the function and send `toolResult`.
- Model access, region and inference profiles cause most first-run errors, not IAM alone.
- LangChain.js: `ChatBedrockConverse` / `BedrockEmbeddings` from `@langchain/aws` when you want chains and agents.
- Expect throttling, long latencies and token-based cost.

## Next

[Secrets and parameters](./08-secrets-and-parameters.md): keeping config and credentials like the model ID and API keys out of your code.
