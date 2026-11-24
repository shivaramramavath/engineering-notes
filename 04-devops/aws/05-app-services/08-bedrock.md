# Amazon Bedrock: Enterprise Generative AI & Foundation Models

> **Module**: `05-app-services`  
> **Topic**: Foundation Models, Retrieval Augmented Generation (RAG), Bedrock Agents & Guardrails  
> **Target Audience**: AI/ML Engineers, Cloud Architects, Full-Stack Developers  
> **Last Updated**: November 2026

---

## Navigation & Contents

- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes](#7-common-mistakes)
- [8. Performance, Security & Cost Notes](#8-performance-security--cost-notes)
- [9. Comparison Table](#9-comparison-table)
- [10. Interview Questions & Answers](#10-interview-questions--answers)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary & Quick Reference](#12-summary--quick-reference)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

Deploying, managing, and fine-tuning large language models (LLMs) from scratch requires massive GPU infrastructure investments (NVIDIA H100/A100 clusters), complex distributed serving frameworks (vLLM, Triton), and continuous operational patching. Furthermore, using consumer public AI APIs introduces severe enterprise compliance risks: sensitive corporate intellectual property and customer personally identifiable information (PII) may be logged or used to train public foundation models.

**Amazon Bedrock** is a fully managed service that offers a choice of high-performing foundation models (FMs) from leading AI companies (Anthropic, Meta, Mistral AI, Cohere, AI21 Labs, and Amazon) via a single, unified API, along with a broad set of capabilities to build generative AI applications with security, privacy, and responsible AI. With Amazon Bedrock:
1. **Zero Infrastructure Management**: Access state-of-the-art LLMs serverlessly without managing GPU clusters.
2. **Guaranteed Data Privacy**: Your data is **never** used to train the base foundation models, and remains completely encrypted inside your AWS virtual private perimeter.
3. **Enterprise End-to-End Capabilities**: Native integration with **Knowledge Bases** for Retrieval-Augmented Generation (RAG), **Bedrock Agents** for multi-step task execution, and **Bedrock Guardrails** for hallucination checks and content safety.

---

## 2. Core Concepts

### 2.1 Multi-Model Foundation Hub
Bedrock provides a unified SDK and API interface across diverse foundation models:
- **Anthropic Claude 3 & 3.5 (Opus, Sonnet, Haiku)**: Industry-leading reasoning, complex code generation, document parsing, and vision understanding.
- **Amazon Titan (Text, Embeddings, Image Generator)**: Cost-effective, high-throughput text generation and vector embeddings.
- **Meta Llama 3 & 3.1 (8B, 70B, 405B)**: Leading open-weights models optimized for low latency and custom fine-tuning.
- **Mistral AI (Mistral Large, Pixtral)**: Exceptional multilingual capabilities and reasoning.
- **Cohere (Command, Embed, Rerank)**: Specialized for semantic search and document ranking.

### 2.2 Ingestion & Streaming APIs
- `InvokeModel`: Synchronous API that returns the complete generated completion once finished.
- `InvokeModelWithResponseStream`: Streaming API using HTTP/2 chunked transfer encoding, streaming tokens back to the client in real time for responsive user experiences.
- **Converse API**: A standardized, unified request/response format across *all* Bedrock models, eliminating model-specific JSON prompt formatting differences.

### 2.3 Knowledge Bases for Amazon Bedrock (RAG)
- Fully managed **Retrieval-Augmented Generation (RAG)** pipeline.
- Automatically connects to document repositories in Amazon S3, parses PDFs/Word documents, chunks text, generates vector embeddings using Amazon Titan or Cohere Embed, and indexes vectors into a vector store (Amazon OpenSearch Serverless, Amazon Aurora PostgreSQL with `pgvector`, Pinecone, or Redis Enterprise).
- When a user submits a prompt, Bedrock queries the vector store, retrieves relevant passages, and appends them to the prompt context to generate grounded, factual responses with citations.

### 2.4 Bedrock Agents & Action Groups
- **Bedrock Agents**: Autonomous reasoning engines that break down user goals into multi-step execution plans using ReAct (Reasoning + Acting) logic.
- **Action Groups**: Defines tools the agent can invoke. The agent reads an OpenAPI specification, determines which API to call, extracts parameters from natural conversation, and invokes an AWS Lambda function to execute transactions against backend databases or external APIs.

### 2.5 Bedrock Guardrails
- Implements Responsible AI safeguards independent of the underlying model:
  - **Denied Topics**: Blocks discussions on prohibited corporate subjects.
  - **Content Filters**: Detects hate speech, sexual content, insults, and violence.
  - **Sensitive Information Filters (PII)**: Automatically masks or blocks Social Security numbers, credit card numbers, and custom regex PII.
  - **Contextual Grounding Check**: Detects hallucinations by comparing the generated response against the retrieved reference documents.

---

## 3. How It Works Internally

```
+---------------------------------------------------------------------------------------------------+
|                        AMAZON BEDROCK RAG & AGENT INTERNAL EXECUTION FLOW                         |
|                                                                                                   |
|  [Enterprise User]                                                                                |
|         |                                                                                         |
|         v (Prompt: "What is our company paternity leave policy and file my request?")             |
|  +---------------------------------------------------------------------------------------------+  |
|  | BEDROCK GUARDRAILS (Input Inspection)                                                       |  |
|  | - Checks for prompt injection & toxic content -> PASS                                       |  |
|  | - Detects PII -> Anonymizes sensitive employee ID -> PASS                                   |  |
|  +---------------------------------------------------------------------------------------------+  |
|         |                                                                                         |
|         v                                                                                         |
|  +---------------------------------------------------------------------------------------------+  |
|  | BEDROCK AGENT (ReAct Reasoning Engine)                                                      |  |
|  | 1. Evaluates Goal: Needs document lookup + backend API transaction                         |  |
|  +---------------------------------------------------------------------------------------------+  |
|         |                                              |                                          |
|         | Step 1: Query Knowledge Base                 | Step 2: Invoke Action Group              |
|         v                                              v                                          |
|  +-----------------------------+               +-----------------------------------------------+  |
|  | BEDROCK KNOWLEDGE BASE      |               | ACTION GROUP (AWS Lambda Function)            |  |
|  | 1. Generates Query Vector   |               | 1. Receives structured JSON:                  |  |
|  | 2. K-NN Search in OpenSearch|               |    `{"action": "submitLeave", "days": 14}`    |  |
|  | 3. Returns Relevant Chunks  |               | 2. Inserts record into HR DynamoDB Table      |  |
|  +--------------+--------------+               +-----------------------+-----------------------+  |
|                 |                                                      |                          |
|                 +-----------------------+------------------------------+                          |
|                                         |                                                         |
|                                         v (Augmented Prompt + Context + Action Output)            |
|  +---------------------------------------------------------------------------------------------+  |
|  | FOUNDATION MODEL (Anthropic Claude 3.5 Sonnet / Llama 3)                                    |  |
|  | Synthesizes natural response based strictly on retrieved facts                              |  |
|  +---------------------------------------------------------------------------------------------+  |
|         |                                                                                         |
|         v                                                                                         |
|  +---------------------------------------------------------------------------------------------+  |
|  | BEDROCK GUARDRAILS (Output Inspection)                                                      |  |
|  | Contextual Grounding Check: Is response supported by retrieved text? (Hallucination score)    |  |
|  +---------------------------------------------------------------------------------------------+  |
|         |                                                                                         |
|         v (Real-Time Chunked Streaming via SSE)                                                   |
|  [User Receives Answer: "Our policy allows 12 weeks. Your request #8812 has been submitted."]     |
+---------------------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 4.1 Invoking a Foundation Model with Converse API (Python SDK)

```python
import boto3

bedrock = boto3.client("bedrock-runtime", region_name="us-east-1")

response = bedrock.converse(
    modelId="anthropic.claude-3-5-sonnet-20240620-v1:0",
    messages=[
        {
            "role": "user",
            "content": [{"text": "Explain Kubernetes pods vs ECS tasks in 2 bullet points."}]
        }
    ],
    inferenceConfig={
        "maxTokens": 300,
        "temperature": 0.2,
        "topP": 0.9
    }
)

output_text = response["output"]["message"]["content"][0]["text"]
print(output_text)
```

### 4.2 Streaming Response Tokens with `InvokeModelWithResponseStream`

```python
import boto3
import json

bedrock = boto3.client("bedrock-runtime", region_name="us-east-1")

body = json.dumps({
    "anthropic_version": "bedrock-2023-05-31",
    "max_tokens": 500,
    "messages": [{"role": "user", "content": "Write a Python script to parse S3 bucket tags."}]
})

response = bedrock.invoke_model_with_response_stream(
    modelId="anthropic.claude-3-5-sonnet-20240620-v1:0",
    body=body
)

for event in response.get("body"):
    chunk = json.loads(event["chunk"]["bytes"].decode("utf-8"))
    if chunk.get("type") == "content_block_delta":
        print(chunk["delta"]["text"], end="", flush=True)
```

---

## 5. Practical Examples

### 5.1 Bedrock Knowledge Base with OpenSearch Serverless Vector Index (Terraform)

```hcl
# Bedrock Knowledge Base
resource "aws_bedrockagent_knowledge_base" "hr_kb" {
  name     = "hr-policies-knowledge-base"
  role_arn = var.bedrock_kb_role_arn

  knowledge_base_configuration {
    type = "VECTOR"
    vector_knowledge_base_configuration {
      embedding_model_arn = "arn:aws:bedrock:us-east-1::foundation-model/amazon.titan-embed-text-v2:0"
    }
  }

  storage_configuration {
    type = "OPENSEARCH_SERVERLESS"
    opensearch_serverless_configuration {
      collection_arn    = var.aoss_collection_arn
      vector_index_name = "bedrock-knowledge-base-index"
      field_mapping {
        vector_field   = "bedrock-knowledge-base-vector"
        text_field     = "AMAZON_BEDROCK_TEXT_CHUNK"
        metadata_field = "AMAZON_BEDROCK_METADATA"
      }
    }
  }
}

# S3 Data Source syncing documents to Knowledge Base
resource "aws_bedrockagent_data_source" "s3_source" {
  knowledge_base_id = aws_bedrockagent_knowledge_base.hr_kb.id
  name              = "hr-docs-s3-data-source"

  data_source_configuration {
    type = "S3"
    s3_data_source_configuration {
      bucket_arn = var.hr_documents_bucket_arn
    }
  }
}
```

### 5.2 RAG Query with RetrieveAndGenerate API (Python)

```python
"""
Queries a Bedrock Knowledge Base using RetrieveAndGenerate API.
"""
import boto3

agent_runtime = boto3.client("bedrock-agent-runtime", region_name="us-east-1")
KNOWLEDGE_BASE_ID = "AB1C2D3EF4"

response = agent_runtime.retrieve_and_generate(
    input={"text": "What is the policy for tuition reimbursement?"},
    retrieveAndGenerateConfiguration={
        "type": "KNOWLEDGE_BASE",
        "knowledgeBaseConfiguration": {
            "knowledgeBaseId": KNOWLEDGE_BASE_ID,
            "modelArn": "arn:aws:bedrock:us-east-1::foundation-model/anthropic.claude-3-5-sonnet-20240620-v1:0"
        }
    }
)

answer = response["output"]["text"]
citations = response.get("citations", [])
print(f"Answer: {answer}\n")
for cite in citations:
    for ref in cite["retrievedReferences"]:
        print(f"Citation Source: {ref['location']['s3Location']['uri']}")
```

### 5.3 Bedrock Guardrail for PII Masking and Hallucination Prevention (Terraform)

```hcl
resource "aws_bedrock_guardrail" "enterprise_safety" {
  name        = "corporate-safe-guardrail"
  description = "Block PII and prevent hallucinated answers"

  # Block hate speech and violence
  content_policy_config {
    filters_config {
      type            = "HATE"
      input_strength  = "HIGH"
      output_strength = "HIGH"
    }
    filters_config {
      type            = "VIOLENCE"
      input_strength  = "HIGH"
      output_strength = "HIGH"
    }
  }

  # Mask Social Security Numbers and Credit Card numbers in outputs
  sensitive_information_policy_config {
    pii_entities_config {
      type   = "US_SOCIAL_SECURITY_NUMBER"
      action = "ANONYMIZE"
    }
    pii_entities_config {
      type   = "CREDIT_DEBIT_CARD_NUMBER"
      action = "BLOCK"
    }
  }

  # Contextual Grounding: Ensure output is grounded in retrieved facts
  contextual_grounding_policy_config {
    filters_config {
      type      = "GROUNDING"
      threshold = 0.75 # Must have >= 75% factual adherence to source text
    }
  }

  blocked_input_messaging   = "Request blocked by enterprise security policy."
  blocked_outputs_messaging = "Response blocked due to sensitive corporate data policies."
}
```

### 5.4 Tool Use / Function Calling with Claude 3.5 Sonnet Converse API

```python
"""
Demonstrates function calling / tool use via the Bedrock Converse API.
"""
tool_config = {
    "tools": [
        {
            "toolSpec": {
                "name": "get_stock_price",
                "description": "Fetch current stock price for a given ticker symbol.",
                "inputSchema": {
                    "json": {
                        "type": "object",
                        "properties": {
                            "ticker": {"type": "string", "description": "Stock symbol (e.g., AMZN)"}
                        },
                        "required": ["ticker"]
                    }
                }
            }
        }
    ]
}

response = bedrock.converse(
    modelId="anthropic.claude-3-5-sonnet-20240620-v1:0",
    messages=[{"role": "user", "content": [{"text": "What is the price of Amazon stock?"}]}],
    toolConfig=tool_config
)

# Claude returns a toolUse request
message = response["output"]["message"]
for content in message["content"]:
    if "toolUse" in content:
        tool_call = content["toolUse"]
        print(f"Model requested tool execution: {tool_call['name']} with args {tool_call['input']}")
```

---

## 6. Best Practices

1. **Always Use the Converse API**: Standardizes messages and parameters across all model providers. Switching from Claude to Llama or Titan requires updating only the `modelId` without changing JSON body structures.
2. **Implement Guardrails for RAG Hallucination Checks**: Enable Contextual Grounding filters in Bedrock Guardrails. This guarantees that generated answers are strictly derived from the retrieved documents, preventing fabricated facts.
3. **Use Cohere Rerank in Knowledge Bases**: Vector search returns semantically similar chunks, but not necessarily the most relevant. Apply Cohere Rerank to re-order the top 20 retrieved chunks, significantly boosting RAG accuracy.
4. **Encrypt Vector Indexes with Customer Managed KMS Keys**: In OpenSearch Serverless or Aurora pgvector, always enforce KMS CMK encryption to maintain compliance.
5. **Stream Tokens for User-Facing Applications**: Use `InvokeModelWithResponseStream` for chatbots and search interfaces. First-token latency drops from 4 seconds down to ~400 milliseconds.

---

## 7. Common Mistakes

| Mistake | Root Cause | Impact | Fix |
|---|---|---|---|
| Model Access Not Requested in Console | Foundation models require explicit one-time activation. | `AccessDeniedException`: Model access is not granted. | Navigate to Bedrock Console -> Model Access -> Request access. |
| Hardcoding Model-Specific Payload Formats | Manually crafting Anthropic JSON bodies for Llama models. | `ValidationException`: Invalid input format. | Migrate to the unified `Converse` API. |
| Neglecting Vector Store Shard Costs | Creating multi-OCU OpenSearch Serverless collections for tiny test data. | Incurs ~$700/month baseline OCU costs unexpectedly. | Use Aurora Serverless v2 pgvector or Pinecone for low-volume testing. |
| Missing Chunking Strategy in RAG | Uploading 200-page PDF with default 300-token chunks. | Context fragments mid-sentence; answer quality degrades. | Configure Hierarchical Chunking or Semantic Chunking in Knowledge Base. |
| Prompt Injection Bypassing Security | Users submitting `Ignore all previous instructions...`. | AI reveals system prompts or unauthorized corporate data. | Deploy Bedrock Guardrails with prompt attack filters enabled. |

---

## 8. Performance, Security & Cost Notes

### 8.1 Performance & Throughput
- **Provisioned Throughput**: By default, models are billed on-demand per token. For guaranteed sub-second response times and dedicated throughput during enterprise peak hours, purchase **Provisioned Throughput Units (PTUs)** on 1-month or 6-month commitments.

### 8.2 Security & Data Privacy
- **Zero Training on Customer Data**: AWS contracts strictly prohibit foundation model providers from using customer prompts, completions, or embeddings to train base models.
- **VPC Endpoints (AWS PrivateLink)**: Connect to Amazon Bedrock control and runtime APIs entirely over private AWS network paths without public Internet exposure.

### 8.3 Cost Breakdown
- **Anthropic Claude 3.5 Sonnet**: \$0.003 per 1,000 input tokens; \$0.015 per 1,000 output tokens.
- **Amazon Titan Text Express**: \$0.0002 per 1,000 input tokens; \$0.0006 per 1,000 output tokens.
- **Amazon Titan Embeddings**: \$0.00002 per 1,000 tokens.
- **Bedrock Guardrails**: \$0.75 per 1,000 text policies evaluated.

---

## 9. Comparison Table

| Dimension | Amazon Bedrock | Amazon SageMaker JumpStart | OpenAI Direct API |
|---|---|---|---|
| **Management Model** | Fully Serverless API | Managed EC2/GPU Instances | Third-party SaaS |
| **Model Choice** | Anthropic, Meta, Mistral, Titan| Open-source HuggingFace models| OpenAI models only (GPT-4o) |
| **Data Privacy** | Strict AWS VPC boundaries | Strict AWS VPC boundaries | Data processed in external cloud |
| **GPU Infrastructure**| None (Zero cluster setup) | Provisions dedicated GPU EC2 | External |
| **RAG & Agents** | Managed Knowledge Bases & Agents| Requires LangChain/LlamaIndex code| Assistants API (external storage) |
| **IAM Integration** | Native AWS IAM & SigV4 | Native AWS IAM & SigV4 | API Key / Bearer Tokens |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: What is the architectural difference between Amazon Bedrock and Amazon SageMaker JumpStart?</strong></summary>

- **Amazon Bedrock** is a 100% serverless, cloud-native API service. Developers invoke models (Claude, Llama, Titan) via API calls without provisioning, scaling, or managing underlying GPU instances. Billing is pay-per-token.
- **Amazon SageMaker JumpStart** deploys open-weights foundation models onto **dedicated EC2 GPU instances** (e.g., `ml.g5.12xlarge` or `ml.p4de.24xlarge`) inside your VPC. You have full root control over model weights, CUDA drivers, and container runtimes, but you pay continuous hourly instance fees even when idle.
</details>

<details>
<summary><strong>Q2: How does Retrieval-Augmented Generation (RAG) work in Bedrock Knowledge Bases?</strong></summary>

1. **Ingestion Phase**: Bedrock crawls an S3 bucket, chunks documents (e.g., 300-token chunks with 20% overlap), generates vector embeddings using an embedding model (like Titan Embeddings), and writes the vectors and text into an OpenSearch Serverless vector collection.
2. **Retrieval Phase**: A user query is converted into a vector embedding, and a k-Nearest Neighbors (k-NN) vector search finds the top $k$ most relevant text chunks.
3. **Generation Phase**: Bedrock constructs an augmented prompt containing the original user query and the retrieved context chunks, passing it to an LLM (such as Claude 3.5 Sonnet). The LLM synthesizes an accurate answer with citations.
</details>

<details>
<summary><strong>Q3: How do Bedrock Agents use the ReAct (Reasoning + Acting) framework to execute tasks?</strong></summary>

Bedrock Agents leverage the LLM's reasoning capabilities to evaluate multi-step goals. The agent analyzes the user prompt, determines what information is missing, and formulates an **Action Plan**:
1. It selects an **Action Group** mapped to an OpenAPI schema.
2. It generates structured JSON parameters extracted from the user's conversation.
3. It invokes an AWS Lambda function that executes the action against an API or database.
4. It ingests the Lambda output, reasons about the next step, and either executes another action or returns the final formatted answer to the user.
</details>

<details>
<summary><strong>Q4: What is the Bedrock Converse API and why was it introduced?</strong></summary>

Previously, every foundation model provider in Bedrock had its own unique request and response JSON schema (e.g., Anthropic required `{"anthropic_version": "...", "messages": [...]}`, while Cohere and Titan used completely different schemas). 
The **Converse API** provides a unified, standardized interface across all Bedrock models. It standardizes conversation turns, system prompts, inference parameters (temperature, maxTokens), and tool use definitions, allowing developers to switch between Claude, Llama, and Titan with zero code refactoring.
</details>

<details>
<summary><strong>Q5: How does Bedrock ensure enterprise data privacy and model governance?</strong></summary>

1. **No Data Training**: Customer prompts and generated outputs are never used to train or improve any base foundation model from AWS or third parties.
2. **Perimeter Isolation**: All traffic remains within the AWS network; Bedrock can be accessed via VPC Interface Endpoints without traversing the public Internet.
3. **Encryption**: Prompts and data in Knowledge Bases are encrypted at rest using AWS KMS Customer Managed Keys.
4. **Audit Logging**: Comprehensive Model Invocation Logging records every prompt, completion, and metadata tag into Amazon S3 and CloudWatch Logs.
</details>

<details>
<summary><strong>Q6: What are Bedrock Guardrails and what are Contextual Grounding Checks?</strong></summary>

Bedrock Guardrails enforce safety and compliance policies across any foundation model. 
A **Contextual Grounding Check** is an automated hallucination detector: it analyzes the model's generated response against the reference documents retrieved during RAG. If the generated text makes assertions not supported by the retrieved facts (hallucinations), the guardrail blocks or flags the response.
</details>

<details>
<summary><strong>Q7: What is Provisioned Throughput in Bedrock and when should it be purchased?</strong></summary>

On-demand Bedrock invocations share multi-tenant GPU capacity across AWS customers. During global traffic surges, on-demand requests may experience throttling or higher latency. 
**Provisioned Throughput** allocates dedicated model capacity measured in Model Units (MUs) under a 1-month or 6-month commitment, guaranteeing consistent throughput (transactions per second) and zero throttling for mission-critical enterprise workloads.
</details>

<details>
<summary><strong>Q8: How does Cohere Rerank improve RAG performance in Bedrock?</strong></summary>

Vector similarity search (k-NN) relies on dense embeddings, which can return passages that share general semantic themes but lack the specific keywords or nuances needed to answer the question. A **Reranker** model performs a deep cross-encoder comparison between the query and the retrieved documents, re-ranking the top chunks by true relevance before passing them to the generative LLM. This drastically improves answer precision.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Enable Model Access and Execute Converse API</strong></summary>

**Goal**: Request model access and execute a basic prompt.
1. In the Bedrock Console, request access to Amazon Titan and Anthropic Claude.
2. Use Python `boto3` to call `bedrock.converse()` with model `anthropic.claude-3-5-sonnet-20240620-v1:0`.
3. Print the generated response and token usage metrics.
</details>

<details>
<summary><strong>Exercise 2: Implement Real-Time Token Streaming</strong></summary>

**Goal**: Stream tokens to terminal standard output.
1. Use `invoke_model_with_response_stream` with Claude 3.5 Sonnet.
2. Iterate over the returned event stream chunks.
3. Decode delta tokens and print them with `flush=True`.
</details>

<details>
<summary><strong>Exercise 3: Build a Bedrock Guardrail for PII Anonymization</strong></summary>

**Goal**: Automatically mask sensitive PII in model outputs.
1. Create a Guardrail in the Bedrock Console masking US Social Security Numbers.
2. Test the guardrail in the playground: prompt the model to generate a mock user profile.
3. Verify that SSN values are replaced with `[US_SOCIAL_SECURITY_NUMBER]`.
</details>

<details>
<summary><strong>Exercise 4: Function Calling with Tool Use</strong></summary>

**Goal**: Have the model execute a database lookup tool.
1. Define a tool schema for `get_account_balance(account_id: String)`.
2. Pass the user prompt: "What is the balance of account 9912?".
3. Intercept the model's `toolUse` response, execute mock logic, and send the `toolResult` back to complete the conversation.
</details>

<details>
<summary><strong>Exercise 5: Create a Knowledge Base from S3 Documents</strong></summary>

**Goal**: Build a functional RAG search engine.
1. Upload 3 corporate PDF documents to an S3 bucket.
2. Create a Bedrock Knowledge Base pointing to the S3 bucket using Titan Embeddings.
3. Start the ingestion job and wait for sync completion.
4. Query the Knowledge Base in the console playground and inspect source citations.
</details>

<details>
<summary><strong>Exercise 6: Enable Model Invocation Logging to S3</strong></summary>

**Goal**: Create an immutable audit trail of all AI conversations.
1. In Bedrock Settings, enable Model Invocation Logging.
2. Select S3 destination bucket and CloudWatch Log Group.
3. Execute several model calls.
4. Open the S3 bucket and inspect the JSON log files containing prompts, completions, and timestamps.
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------------------+
| AMAZON BEDROCK CAPABILITY STACK                                                                   |
+--------------------------+-----------------------+------------------------------------------------+
| Component                | Role                  | Supported Engines                              |
+--------------------------+-----------------------+------------------------------------------------+
| Converse API             | Unified Prompt Hub    | Claude 3.5, Llama 3.1, Titan, Mistral, Cohere   |
| Knowledge Bases (RAG)    | Factual Grounding     | OpenSearch Serverless, Aurora pgvector, Pinecone|
| Bedrock Agents           | Multi-step ReAct Logic| Automated planning + Lambda Action Groups      |
| Bedrock Guardrails       | Responsible AI Safety | PII masking, toxic content block, hallucinations|
| Model Evaluation         | Benchmark Accuracy    | Automated LLM-as-a-judge or human evaluations  |
+--------------------------+-----------------------+------------------------------------------------+
```

- **Serverless Foundation Models**: State-of-the-art AI without GPU cluster management.
- **Data Privacy**: Enterprise data is never used to train base foundation models.
- **RAG & Agents**: Built-in Knowledge Bases and autonomous action execution.

---

## 13. Further Reading

- [Amazon Bedrock User Guide](https://docs.aws.amazon.com/bedrock/latest/userguide/what-is-bedrock.html)
- [Amazon Bedrock Converse API Reference](https://docs.aws.amazon.com/bedrock/latest/userguide/conversation-inference.html)
- [Building RAG Applications with Bedrock Knowledge Bases](https://aws.amazon.com/blogs/aws/knowledge-bases-now-available-for-amazon-bedrock/)
- [Anthropic Claude on Amazon Bedrock Documentation](https://docs.anthropic.com/en/docs/welcome)
- [05-app-services/04-step-functions.md](file:///04-devops/aws/05-app-services/04-step-functions.md)
