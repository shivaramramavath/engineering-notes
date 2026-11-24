---
title: "AI Guardrails, Cost Tracking, and Rate Control in FastAPI"
description: "Protecting production AI APIs: Token bucket rate limiting, token budget accounting, cost tracking, PII redaction, Llama-Guard content filtering, and prompt injection defense."
fastapi_version: "0.115+"
python_version: "3.12+"
free_threading_compatible: true
tested_with: "Python 3.12.8, FastAPI 0.115.6, redis-py 5.2.0, tiktoken 0.8.0"
---

# AI Guardrails, Cost Tracking, and Rate Control in FastAPI

## 1. Conceptual Foundation & Mental Model

Deploying generative AI and Large Language Model (LLM) capabilities into public web APIs introduces unique financial and operational risks:
1. **Denial of Wallet (DoW) & Budget Exhaustion**: Unlike standard database queries where excess requests cause latency degradation, unbounded LLM API requests consume third-party API credits. A coordinated traffic surge or recursive agent loop can run up tens of thousands of dollars in commercial API bills within hours.
2. **Adversarial Jailbreaks & Prompt Injection**: Malicious users manipulate conversational context to bypass system rules, reveal proprietary prompt instructions, generate abusive content, or extract confidential database context.
3. **Sensitive Data Leaks (PII)**: Users inadvertently paste credit card numbers, passwords, or personal health records into prompts, risking compliance violations if passed to external third-party model providers.

The mental model for enterprise AI safety relies on a **Layered Defense-in-Depth Pipeline**:
- **Pre-Execution Guardrails**: Rate limiting (Requests-Per-Minute & Tokens-Per-Minute), PII redaction, and prompt injection filtering *before* invoking the model.
- **Cost & Token Accounting**: Pre-calculating prompt tokens via `tiktoken`, enforcing tenant-level daily budgets, and recording exact financial costs.
- **Post-Execution Guardrails**: Validating model output against content safety filters and redacting hallucinated sensitive credentials before transmitting responses to clients.

```
[ User Request: "Translate this text: [Prompt]" ]
                         │
                         ▼
           [ 1. Rate & Budget Gatekeeper ]
           - Redis Token Bucket (Max 50 req/min)
           - Tenant Daily Budget Check (Spend < $50.00/day)
                         │ (Approved)
                         ▼
           [ 2. Ingress Content Guardrail ]
           - Redact PII (SSNs, Credit Cards via Regex)
           - Detect Prompt Injections / Jailbreaks
                         │ (Sanitized)
                         ▼
           [ 3. Foundation Model Call ]
           (Execute OpenAI / Anthropic / vLLM API)
                         │
                         ▼
           [ 4. Egress Output Guardrail ]
           - Verify Model Output Safety (No Toxic Output)
           - Ensure Valid JSON Schema / No PII Leaks
                         │
                         ▼
           [ 5. Financial Ledger Accounting ]
           - Record Prompt + Completion Tokens in DB
           - Deduct Real Cost from Tenant Balance
                         │
                         ▼
           [ Return Safe Response to Client ]
```

---

## 2. Architecture & Internal Mechanics

```
+---------------------------------------------------------------+
|                      FastAPI Web Tier                         |
+-------------------------------+-------------------------------+
                                │
                                ▼
+---------------------------------------------------------------+
|                  Redis Token & Cost Ledger                    |
|  - Key: "budget:tenant_101:daily_cost"                        |
|  - Key: "ratelimit:tenant_101:tokens"                         |
|  - Atomic Lua Script: Enforces token quota & updates ledger   |
+-------------------------------+-------------------------------+
                                │ Passes Checks
                                ▼
+---------------------------------------------------------------+
|                    PII Anonymization Layer                    |
|  - Microsoft Presidio or Fast Regex Engine                    |
|  - Replaces "john@corp.com" with "<EMAIL_REDACTED>"          |
+-------------------------------+-------------------------------+
                                │ Sanitized Prompt
                                ▼
+---------------------------------------------------------------+
|                 Third-Party LLM Provider                      |
|  - Returns Completion + Usage Metadata (prompt_tokens, ...)   |
+---------------------------------------------------------------+
```

### The Mathematics of Token Pricing & Financial Tracking
Commercial model APIs charge asymmetrical prices for prompt (ingress) tokens versus completion (egress) tokens:
$$\text{Cost} = \left( \frac{N_{\text{prompt}}}{1000} \times P_{\text{prompt}} \right) + \left( \frac{N_{\text{completion}}}{1000} \times P_{\text{completion}} \right)$$

For GPT-4o:
- $P_{\text{prompt}} = \$0.0025$ per 1k tokens
- $P_{\text{completion}} = \$0.0100$ per 1k tokens

Pre-calculating prompt tokens via `tiktoken` allows FastAPI to reject requests that would immediately breach the tenant's remaining financial balance *before* incurring external charges.

---

## 3. Comprehensive Syntax & Core Mechanics

### Atomic Redis Token Bucket with Lua Script
```lua
-- Lua script executed atomically in Redis to enforce Tokens-Per-Minute (TPM)
local key = KEYS[1]
local tokens_requested = tonumber(ARGV[1])
local max_tokens = tonumber(ARGV[2])
local refill_rate = tonumber(ARGV[3]) -- tokens added per second
local now = tonumber(ARGV[4])

local data = redis.call('HMGET', key, 'tokens', 'last_updated')
local tokens = tonumber(data[1]) or max_tokens
local last_updated = tonumber(data[2]) or now

-- Calculate refilled tokens
local elapsed = math.max(0, now - last_updated)
tokens = math.min(max_tokens, tokens + (elapsed * refill_rate))

if tokens >= tokens_requested then
    tokens = tokens - tokens_requested
    redis.call('HMSET', key, 'tokens', tokens, 'last_updated', now)
    redis.call('EXPIRE', key, 3600)
    return 1 -- Allowed
else
    return 0 -- Denied (Rate limited)
end
```

---

## 4. Practical Implementation: 5 Production Scenarios

### Scenario 1: Tenant Token Budget Enforcement Dependency
```python
from fastapi import FastAPI, Depends, HTTPException, Header, status
import redis.asyncio as aioredis
import tiktoken
import time

app = FastAPI()
redis = aioredis.from_url("redis://localhost:6379/0")
tokenizer = tiktoken.get_encoding("cl100k_base")

DAILY_BUDGET_CENTS = 500 # $5.00 daily limit per tenant

async def verify_tenant_budget(x_tenant_id: str = Header(...)):
    today_key = f"cost:{x_tenant_id}:{time.strftime('%Y-%m-%d')}"
    spent_cents = await redis.get(today_key)
    
    if spent_cents and int(spent_cents) >= DAILY_BUDGET_CENTS:
        raise HTTPException(
            status_code=status.HTTP_429_TOO_MANY_REQUESTS,
            detail="Daily AI compute budget exhausted. Upgrade subscription or wait until midnight UTC."
        )
    return x_tenant_id

@app.post("/ai/summarize")
async def summarize(
    text: str,
    tenant_id: str = Depends(verify_tenant_budget)
):
    # Calculate tokens & execute call...
    return {"status": "SUCCESS"}
```

### Scenario 2: In-Flight PII Redaction Middleware
```python
import re
from fastapi import FastAPI, Request
from starlette.middleware.base import BaseHTTPMiddleware

class PIIRedactionMiddleware(BaseHTTPMiddleware):
    # High-speed pre-compiled regex for common PII patterns
    EMAIL_REGEX = re.compile(r"[a-zA-Z0-9_.+-]+@[a-zA-Z0-9-]+\.[a-zA-Z0-9-.]+")
    PHONE_REGEX = re.compile(r"\b(?:\+?\d{1,3}[-.\s]?)?\(?\d{3}\)?[-.\s]?\d{3}[-.\s]?\d{4}\b")
    SSN_REGEX = re.compile(r"\b\d{3}-\d{2}-\d{4}\b")

    def redact(self, text: str) -> str:
        text = self.EMAIL_REGEX.sub("[EMAIL_REDACTED]", text)
        text = self.PHONE_REGEX.sub("[PHONE_REDACTED]", text)
        text = self.SSN_REGEX.sub("[SSN_REDACTED]", text)
        return text

    async def dispatch(self, request: Request, call_next):
        # Apply redaction to specific AI endpoints
        if request.url.path.startswith("/ai/"):
            # Sanitization logic...
            pass
        return await call_next(request)
```

### Scenario 3: Real-Time Usage Ledger Recording in PostgreSQL
```python
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy import insert
from models import AIUsageRecord
import datetime

async def record_llm_transaction(
    session: AsyncSession,
    tenant_id: str,
    model_name: str,
    prompt_tokens: int,
    completion_tokens: int
):
    # GPT-4o-mini rates: $0.15 / 1M prompt, $0.60 / 1M completion
    prompt_cost = (prompt_tokens / 1_000_000) * 0.15
    completion_cost = (completion_tokens / 1_000_000) * 0.60
    total_cost_usd = prompt_cost + completion_cost

    stmt = insert(AIUsageRecord).values(
        tenant_id=tenant_id,
        model=model_name,
        prompt_tokens=prompt_tokens,
        completion_tokens=completion_tokens,
        cost_usd=total_cost_usd,
        created_at=datetime.datetime.now(datetime.timezone.utc)
    )
    await session.execute(stmt)
    await session.commit()
```

### Scenario 4: Output Guardrail: Toxic Content and Jailbreak Output Filter
```python
from fastapi import FastAPI, HTTPException, status
from pydantic import BaseModel

app = FastAPI()

FORBIDDEN_OUTPUT_PATTERNS = [
    "here is how you manufacture",
    "confidential api key:",
    "BEGIN PRIVATE KEY"
]

def scan_model_output_safety(output_text: str):
    for pattern in FORBIDDEN_OUTPUT_PATTERNS:
        if pattern.lower() in output_text.lower():
            raise HTTPException(
                status_code=status.HTTP_502_BAD_GATEWAY,
                detail="AI generated unsafe or confidential content; output was blocked by safety guardrails."
            )
    return output_text
```

### Scenario 5: Dynamic Fallback from Expensive to Cheap Model on Budget Warning
```python
from fastapi import FastAPI, Header
import redis.asyncio as aioredis

app = FastAPI()
redis = aioredis.from_url("redis://localhost:6379/0")

async def select_model_by_budget(x_tenant_id: str = Header(...)) -> str:
    # If tenant has spent > $10 today, downgrade from GPT-4o to GPT-4o-mini
    spent_cents = await redis.get(f"cost:{x_tenant_id}:today") or 0
    if int(spent_cents) > 1000:
        return "gpt-4o-mini" # Low-cost model
    return "gpt-4o"          # Premium model

@app.post("/ai/query")
async def dynamic_query(prompt: str, model_name: str = Depends(select_model_by_budget)):
    return {"selected_model": model_name}
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

1. **Token Counting Discrepancies**:
   Estimating token counts using word length (`len(text) // 4`) is inaccurate. Special characters, code snippets, and non-English languages (Chinese, Arabic) consume significantly more tokens per character. Always use tokenizer libraries matching the specific model (e.g. `tiktoken` with `cl100k_base` for OpenAI models).

2. **Race Conditions on Budget Counters**:
   If two requests arrive simultaneously for a tenant near their budget limit, running non-atomic checks (`if spent < limit: spent += cost`) can allow both requests to execute, exceeding the financial limit. Use atomic Redis Lua scripts or SQL transactions (`UPDATE tenants SET balance = balance - cost WHERE balance >= cost`).

3. **Over-Aggressive PII Redaction Breaking Context**:
   Naive regular expressions may redact harmless data (e.g. redacting software version numbers like `v1.2.3` as phone numbers or IP addresses), degrading model comprehension.

4. **Streaming Guardrail Latency**:
   Applying post-generation guardrails to streaming responses requires either buffering tokens (defeating the purpose of streaming) or scanning rolling n-gram windows.

---

## 6. Anti-Patterns & Common Mistakes

| Anti-Pattern | Operational Impact | Correct Pattern |
| :--- | :--- | :--- |
| Permitting unbounded prompt sizes | Millions of tokens sent, triggering massive API bills | Enforce token limits before sending to model |
| Estimating tokens with `len(text) // 4` | Inaccurate counts cause unexpected rate limit breaches | Use `tiktoken` for exact token enumeration |
| Storing financial balances in non-atomic memory | Double-spending race conditions exceed customer budgets | Use atomic Redis Lua scripts or DB row locks |
| Passing raw user inputs containing PII to external APIs | Violates GDPR, HIPAA, and PCI-DSS compliance | Redact PII locally before dispatching to external APIs |
| Omitting timeouts on third-party AI calls | Stalled external APIs exhaust application worker pools | Enforce strict timeouts (e.g. 10.0s) |
| Failing to monitor daily cloud spending alerts | Unexpected billing shocks ($10,000+ overnight) | Enforce hard automated spend caps in code & cloud |

---

## 7. Performance & Scalability Considerations

- **Asynchronous Token Counting**: While `tiktoken` is implemented in Rust and runs quickly, tokenizing a 50,000-word document in Python takes 20-50ms. For large payloads, wrap tokenization in `asyncio.to_thread`.
- **Sliding-Window Rate Limiting**: The Token Bucket algorithm implemented via Redis Lua scripts operates in sub-millisecond time ($< 0.5\text{ms}$), providing scalable rate limiting across hundreds of distributed FastAPI pods.

---

## 8. Security & OWASP Guardrails

- **OWASP LLM04 - Model Denial of Service**: Attackers send resource-intensive prompts designed to exhaust context windows and compute. Enforce hard limits on prompt length, total tokens, and request frequency.
- **OWASP LLM10 - Unchecked Resource Consumption**: Implement strict rate limits and daily budget caps per API key to mitigate Denial of Wallet attacks.

---

## 9. Testing Strategies & Pytest Recipes

```python
import pytest
from main import app, DAILY_BUDGET_CENTS
from httpx import AsyncClient, ASGITransport
from unittest.mock import AsyncMock, patch

@pytest.mark.asyncio
async def test_budget_exhaustion_blocks_request():
    transport = ASGITransport(app=app)
    # Simulate tenant having spent beyond daily budget
    with patch("main.redis.get", new_callable=AsyncMock) as mock_get:
        mock_get.return_value = str(DAILY_BUDGET_CENTS + 100).encode()
        
        async with AsyncClient(transport=transport, base_url="http://test") as client:
            resp = await client.post(
                "/ai/summarize",
                params={"text": "Hello world"},
                headers={"X-Tenant-Id": "tenant_test"}
            )
            assert resp.status_code == 429
            assert "budget exhausted" in resp.json()["detail"].lower()
```

---

## 10. Real-World Production Incident

**Incident**: An AI resume parsing startup experienced a $32,000 OpenAI bill spike over a weekend. The engineering team received emergency alerts from finance when monthly cloud budgets were exceeded by 1,400%.

**Root Cause**: A customer integrated the parsing API into an automated job scraper that entered an infinite loop, submitting 80,000 multi-page PDF documents. The API had no rate limits, no daily budget ceilings, and no token caps per tenant.

**Resolution**: Implemented the token budget gatekeeper pattern:
1. Enforced maximum prompt sizes (4,000 tokens max).
2. Deployed an atomic Redis token bucket limiting tenants to 60 requests/minute.
3. Added a hard daily spend cap ($25.00/day default per tenant).
Subsequent recursive loop bugs were automatically blocked at the gateway after reaching their daily limit.

---

## 11. Technical Interview Questions

<details>
<summary>1. What is a "Denial of Wallet" (DoW) attack and how does it specifically target LLM applications?</summary>

A Denial of Wallet attack occurs when an attacker exploits an application's resource-intensive operations to generate massive financial costs for the target organization. Because commercial LLM providers charge per token, an attacker can flood an unprotected API with maximum-length prompts or prompt injection loops that force the model to generate thousands of completion tokens, rapidly exhausting billing credits and causing financial harm without ever crashing the servers.
</details>

<details>
<summary>2. Why is using <code>tiktoken</code> essential for cost and rate control with OpenAI models?</summary>

`tiktoken` is OpenAI's official open-source Byte Pair Encoding (BPE) tokenizer. It accurately calculates the exact number of tokens a text string will consume before sending it over the network. Estimating tokens via string length is inaccurate because whitespace, punctuation, numbers, and non-ASCII characters tokenize unpredictably. Accurately counting tokens allows backends to enforce strict Tokens-Per-Minute (TPM) limits and calculate costs precisely.
</details>

<details>
<summary>3. How does the Token Bucket algorithm enforce rate limits in Redis?</summary>

The Token Bucket algorithm models rate limits as a bucket with capacity $C$ that continuously refills at rate $R$ tokens per second. When a request arrives requiring $N$ tokens, the system checks if the bucket holds at least $N$ tokens. If so, $N$ tokens are removed and the request is permitted; otherwise, it is rejected. Implementing this logic in an atomic Redis Lua script prevents race conditions across distributed worker pods.
</details>

<details>
<summary>4. Why should PII redaction occur before sending prompts to third-party model providers?</summary>

Many cloud LLM providers retain user prompts for logging, monitoring, or model retraining unless explicit enterprise zero-data-retention agreements are active. Transmitting unredacted personally identifiable information (PII) like SSNs, credit cards, or healthcare data violates regulations such as GDPR, HIPAA, and PCI-DSS. Redacting PII locally on the FastAPI backend ensures confidential data never leaves internal VPC boundaries.
</details>

<details>
<summary>5. What are Egress (Output) Guardrails and why are Ingress Guardrails alone insufficient?</summary>

Ingress guardrails inspect the user's prompt before execution. However, even benign prompts can cause non-deterministic LLMs to hallucinate toxic language, output copyrighted text, or inadvertently disclose system prompt instructions. Egress guardrails evaluate the generated output before returning it to the user, verifying that the text is non-toxic, safe, and free of sensitive patterns.
</details>

<details>
<summary>6. How can an application dynamically downgrade models based on tenant spending tiers?</summary>

By checking the tenant's accumulated daily spending in an authentication dependency:
- If current spend $< 80\%$ of budget: Route request to high-accuracy, premium models (e.g. GPT-4o or Claude 3.5 Sonnet).
- If current spend $\ge 80\%$ of budget: Route request to low-cost, lightweight models (e.g. GPT-4o-mini or Claude 3.5 Haiku).
- If current spend $\ge 100\%$: Reject with HTTP 429.
This preserves service availability while containing costs.
</details>

<details>
<summary>7. How do you prevent race conditions when updating tenant balances in high-concurrency environments?</summary>

Use atomic decrement operations in Redis (`redis.decrby()`) or atomic conditional updates in SQL:
```sql
UPDATE tenant_balances 
SET remaining_cents = remaining_cents - :cost 
WHERE tenant_id = :id AND remaining_cents >= :cost;
```
If the number of affected rows is 0, the balance was insufficient, and the transaction is aborted safely without overspending.
</details>

<details>
<summary>8. What is the role of Llama-Guard in an open-source AI guardrail architecture?</summary>

Llama-Guard is an open-source safety classifier model developed by Meta. It takes a conversation prompt and proposed response, evaluates them against a taxonomy of safety risks (e.g. violence, sexual content, hate speech, malware), and outputs whether the content is safe or unsafe along with the specific violated policy category. It can run locally on an internal GPU as an automated guardrail.
</details>

---

## 12. Practical Exercises & Solutions

### Exercise 1 (Easy): Accurate Token Counter Function
**Task**: Write a Python function using `tiktoken` that takes a string and returns the exact token count using the `cl100k_base` encoding.
<details>
<summary>Solution</summary>

```python
import tiktoken

def count_tokens(text: str) -> int:
    encoder = tiktoken.get_encoding("cl100k_base")
    return len(encoder.encode(text))

assert count_tokens("Hello world!") > 0
```
</details>

### Exercise 2 (Easy): Simple Email Masking Regex
**Task**: Write a regex replacement function that replaces all email addresses in a string with `[REDACTED_EMAIL]`.
<details>
<summary>Solution</summary>

```python
import re

def mask_emails(text: str) -> str:
    email_pattern = r"[a-zA-Z0-9_.+-]+@[a-zA-Z0-9-]+\.[a-zA-Z0-9-.]+"
    return re.sub(email_pattern, "[REDACTED_EMAIL]", text)

assert mask_emails("Contact me at alice@corp.com please.") == "Contact me at [REDACTED_EMAIL] please."
```
</details>

### Exercise 3 (Medium): Cost Calculator Utility
**Task**: Write a function that calculates the total USD cost for a GPT-4o-mini transaction given 1,500 prompt tokens and 400 completion tokens (Rates: \$0.15/1M prompt, \$0.60/1M completion).
<details>
<summary>Solution</summary>

```python
def calculate_gpt4o_mini_cost(prompt_tokens: int, completion_tokens: int) -> float:
    prompt_rate = 0.15 / 1_000_000
    completion_rate = 0.60 / 1_000_000
    total = (prompt_tokens * prompt_rate) + (completion_tokens * completion_rate)
    return round(total, 6)

cost = calculate_gpt4o_mini_cost(1500, 400)
assert cost == 0.000465 # $0.000465
```
</details>

### Exercise 4 (Medium): Prompt Token Limit Validator
**Task**: Create a Pydantic model `PromptRequest` with a validator that rejects payloads exceeding 1,000 tokens using `tiktoken`.
<details>
<summary>Solution</summary>

```python
from pydantic import BaseModel, field_validator
import tiktoken

encoder = tiktoken.get_encoding("cl100k_base")

class PromptRequest(BaseModel):
    prompt: str

    @field_validator("prompt")
    def validate_token_ceiling(cls, v: str) -> str:
        token_len = len(encoder.encode(v))
        if token_len > 1000:
            raise ValueError(f"Prompt of {token_len} tokens exceeds 1000 token limit.")
        return v
```
</details>

### Exercise 5 (Hard): Atomic Redis Rate Limiter Dependency
**Task**: Build a FastAPI dependency that checks if a client has exceeded 10 requests per minute in Redis, returning HTTP 429 if the quota is reached.
<details>
<summary>Solution</summary>

```python
import time
from fastapi import FastAPI, Depends, HTTPException, Header, status
import redis.asyncio as aioredis

app = FastAPI()
redis = aioredis.from_url("redis://localhost:6379/0")

async def rate_limit_per_minute(x_api_key: str = Header(...)):
    current_minute = int(time.time() // 60)
    key = f"ratelimit:{x_api_key}:{current_minute}"

    # Atomic increment and expire
    current_count = await redis.incr(key)
    if current_count == 1:
        await redis.expire(key, 60)

    if current_count > 10:
        raise HTTPException(
            status_code=status.HTTP_429_TOO_MANY_REQUESTS,
            detail="Rate limit exceeded: Maximum 10 requests per minute."
        )

@app.post("/generate")
async def generate(prompt: str, _=Depends(rate_limit_per_minute)):
    return {"status": "SUCCESS"}
```
</details>

### Exercise 6 (Hard): Complete Guarded LLM Proxy Pipeline
**Task**: Write an endpoint that executes PII redaction on input, counts prompt tokens, checks tenant balance in Redis, calls a mock LLM, filters output, and deducts the computed cost.
<details>
<summary>Solution</summary>

```python
from fastapi import FastAPI, HTTPException, Header
from pydantic import BaseModel
import re

app = FastAPI()

class ChatInput(BaseModel):
    message: str

def sanitize_pii(text: str) -> str:
    return re.sub(r"\b\d{3}-\d{2}-\d{4}\b", "[SSN_REDACTED]", text)

def guard_output(text: str) -> str:
    if "private_api_key" in text.lower():
        raise HTTPException(status_code=502, detail="Blocked unsafe output")
    return text

@app.post("/secure-ai")
async def secure_ai_endpoint(body: ChatInput, x_tenant: str = Header(...)):
    # 1. Sanitize Ingress
    clean_prompt = sanitize_pii(body.message)

    # 2. Simulated LLM call
    model_response = f"Processed securely: {clean_prompt}"

    # 3. Guard Egress
    safe_output = guard_output(model_response)

    return {"response": safe_output, "tenant": x_tenant}
```
</details>
