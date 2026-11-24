# Structured Outputs and Type-Safe Extraction in Spring AI

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring AI Version** | 1.0.x / 1.1.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.x |
| **Tested With** | BeanOutputConverter, MapOutputConverter, ListOutputConverter, OpenAI JSON Schema |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance & Security Notes](#8-performance--security-notes)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions & Deep Dives](#10-interview-questions--deep-dives)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### The Problem
Large Language Models are naturally non-deterministic free-form text generators. When an enterprise backend prompts an LLM to extract customer information, summarize a financial invoice, or generate a product catalog, the model frequently returns markdown code fences (` ```json ... ``` `), conversational introductory chatter (*"Sure, here is your JSON output:"*), trailing commas, or missing attributes. Writing custom regex cleaners and fragile string parsers to deserialize LLM responses into Java domain objects leads to frequent runtime `JsonParseException` crashes in production.

### The Evolution
To solve this, modern LLM architectures and frameworks introduced **Structured Outputs (Schema-Constrained Generation)**:
1. **Prompt-Engineered Converters**: Spring AI provides `BeanOutputConverter<T>`, `ListOutputConverter`, and `MapOutputConverter`, which append strict JSON Schema formatting instructions to the prompt and automatically deserialize the resulting JSON string into typed Java 21 `record`s.
2. **Native Provider Constrained Decoding**: Modern APIs (e.g., OpenAI Structured Outputs, Anthropic JSON mode) accept a formal JSON Schema directly in the API payload, mathematically constraining the model's token sampling probabilities so it is physically incapable of emitting invalid JSON tokens or missing required fields.

### Production Relevance
Structured output extraction is the backbone of automated document parsing (receipts, medical records, resumes), data pipeline enrichment, synthetic test data generation, and sentiment classification in Spring Boot applications.

---

## 2. Core Concepts

### Structured Output Flow with `ChatClient`

```
+-----------------------------------------------------------------------------------------+
|                              STRUCTURED OUTPUT PIPELINE                                 |
+-----------------------------------------------------------------------------------------+
|                                                                                         |
|  [ Target Java 21 Record Definition ]                                                   |
|  public record InvoiceAnalysis(String vendor, BigDecimal total, List<Item> items) {}    |
|                                |                                                        |
|                                v                                                        |
|  [ BeanOutputConverter<InvoiceAnalysis> ]                                               |
|    - Generates JSON Schema:                                                             |
|      {"type": "object", "properties": {"vendor": {"type": "string"}, ...}}              |
|    - Injects formatting instruction into System/User Prompt                             |
|                                |                                                        |
|                                v                                                        |
|  [ ChatModel Invocation (OpenAI / Claude / Ollama) ]                                    |
|    - Constrained generation ensures 100% syntactically valid JSON                       |
|                                |                                                        |
|                                v                                                        |
|  [ Spring AI Automatic Deserializer (Jackson) ]                                         |
|    - Converts raw JSON string directly into strongly typed Java instance                |
|                                |                                                        |
|                                v                                                        |
|  [ Type-Safe Return: InvoiceAnalysis object ready for Business Service ]                |
|                                                                                         |
+-----------------------------------------------------------------------------------------+
```

### Key Architectural Definitions
1. **`StructuredOutputConverter<T>`**: The base Spring AI interface responsible for generating schema instructions and deserializing model text into Java type `T`.
2. **`BeanOutputConverter<T>`**: Converts model completions into arbitrary complex Java beans, POJOs, or Java 21 `record`s.
3. **`ListOutputConverter`**: Formats output as a comma-separated list or JSON array converted directly into a Java `List<String>`.
4. **`MapOutputConverter`**: Formats output as a key-value dictionary converted directly into a Java `Map<String, Object>`.
5. **Native JSON Schema Mode**: Leveraging model-level constrained decoding (`response_format: {type: "json_schema"}`) for zero-error guarantees.

---

## 3. How It Works Internally

### The `BeanOutputConverter` Schema Injection & Deserialization Engine

```
[ chatClient.prompt().user("Analyze receipt").call().entity(Receipt.class) ]
                                |
                                v
[ 1. Schema Generation Phase ]
   - Spring AI inspects Receipt.class (and any nested records/enums).
   - Generates JSON Schema representation via Jackson.
   - Appends formatting instructions to the prompt:
     "Your response must be strictly valid JSON matching this schema:
      {"$schema": "...", "type": "object", "properties": {...}, "required": [...]}
      Do NOT wrap in markdown code blocks or add conversational text."
                                |
                                v
[ 2. Model Generation Phase ]
   - Model receives prompt.
   - Constrained decoding forces token generation to strictly follow the schema.
                                |
                                v
[ 3. Parsing Phase ]
   - Output string arrives: '{"vendor": "Acme Corp", "total": 42.50}'
   - Spring AI's BeanOutputConverter passes string to Jackson ObjectMapper.
   - Returns instantiated, immutable Receipt record instance.
```

If the model response contains leading/trailing markdown fences (e.g. ` ```json ... ``` `), Spring AI's parser automatically strips the markdown delimiters before passing the JSON string to Jackson.

---

## 4. Syntax & Usage

### 1. Fluent Structured Extraction via `ChatClient.entity()`

The modern Spring AI 1.0+ approach uses the fluent `.entity()` terminal operator on `ChatClient`:

```java
package com.example.ai.structured;

import org.springframework.ai.chat.client.ChatClient;
import org.springframework.stereotype.Service;

import java.math.BigDecimal;
import java.util.List;

public record ProductReviewSummary(
    String sentiment,
    int ratingOutOfFive,
    List<String> pros,
    List<String> cons,
    boolean recommendsPurchase
) {}

@Service
public class ReviewAnalysisService {

    private final ChatClient chatClient;

    public ReviewAnalysisService(ChatClient.Builder builder) {
        this.chatClient = builder.build();
    }

    public ProductReviewSummary analyzeReview(String rawReviewText) {
        // Automatically injects schema and converts completion to ProductReviewSummary record!
        return chatClient.prompt()
                .user("Analyze the following customer product review:\n" + rawReviewText)
                .call()
                .entity(ProductReviewSummary.class);
    }
}
```

---

## 5. Practical Examples

### Example 1: Extracting Financial Invoices into Nested Records

```java
package com.example.ai.invoice;

import com.fasterxml.jackson.annotation.JsonPropertyDescription;
import org.springframework.ai.chat.client.ChatClient;
import org.springframework.stereotype.Service;

import java.math.BigDecimal;
import java.time.LocalDate;
import java.util.List;

public record InvoiceItem(
    @JsonPropertyDescription("Line item description") String description,
    @JsonPropertyDescription("Quantity of units purchased") int quantity,
    @JsonPropertyDescription("Unit price") BigDecimal unitPrice,
    @JsonPropertyDescription("Total price for this line item") BigDecimal lineTotal
) {}

public record ParsedInvoice(
    @JsonPropertyDescription("Vendor or company issuing the invoice") String vendorName,
    @JsonPropertyDescription("Invoice number or identifier") String invoiceNumber,
    @JsonPropertyDescription("Date invoice was issued") LocalDate issueDate,
    @JsonPropertyDescription("Total balance due") BigDecimal totalAmount,
    @JsonPropertyDescription("Currency code (USD, EUR, GBP)") String currency,
    @JsonPropertyDescription("List of individual line items") List<InvoiceItem> items
) {}

@Service
public class InvoiceParsingService {

    private final ChatClient chatClient;

    public InvoiceParsingService(ChatClient.Builder builder) {
        this.chatClient = builder.build();
    }

    public ParsedInvoice parseInvoicePdfText(String invoiceRawText) {
        return chatClient.prompt()
                .system("You are an automated accounting clerk. Extract all financial invoice metadata with complete precision.")
                .user(invoiceRawText)
                .call()
                .entity(ParsedInvoice.class);
    }
}
```

### Example 2: Extracting Lists using `ParameterizedTypeReference`

Extracting a list of records rather than a single wrapper object:

```java
package com.example.ai.extraction;

import org.springframework.ai.chat.client.ChatClient;
import org.springframework.core.ParameterizedTypeReference;
import org.springframework.stereotype.Service;

import java.util.List;

public record CompetitorProfile(String companyName, String primaryProduct, String pricingModel) {}

@Service
public class MarketResearchService {

    private final ChatClient chatClient;

    public MarketResearchService(ChatClient.Builder builder) {
        this.chatClient = builder.build();
    }

    public List<CompetitorProfile> extractCompetitors(String marketResearchArticle) {
        // ParameterizedTypeReference enables extracting List<T> directly
        return chatClient.prompt()
                .user("Extract all competing companies mentioned in this market research:\n" + marketResearchArticle)
                .call()
                .entity(new ParameterizedTypeReference<List<CompetitorProfile>>() {});
    }
}
```

### Example 3: Low-Level `BeanOutputConverter` with Prompt Template

```java
package com.example.ai.converter;

import org.springframework.ai.chat.model.ChatModel;
import org.springframework.ai.chat.prompt.Prompt;
import org.springframework.ai.chat.prompt.PromptTemplate;
import org.springframework.ai.converter.BeanOutputConverter;
import org.springframework.stereotype.Service;

import java.util.Map;

public record JobPostingData(String title, String seniorityLevel, List<String> requiredSkills, int minSalary) {}

@Service
public class JobExtractionService {

    private final ChatModel chatModel;

    public JobExtractionService(ChatModel chatModel) {
        this.chatModel = chatModel;
    }

    public JobPostingData extractJobData(String rawJobHtml) {
        BeanOutputConverter<JobPostingData> converter = new BeanOutputConverter<>(JobPostingData.class);

        String template = """
            Extract the job details from the following HTML description.
            {format}
            HTML:
            {html}
            """;

        PromptTemplate promptTemplate = new PromptTemplate(template);
        Prompt prompt = promptTemplate.create(Map.of(
                "format", converter.getFormat(), // Injects JSON Schema instructions
                "html", rawJobHtml
        ));

        String rawOutput = chatModel.call(prompt).getResult().getOutput().getContent();
        return converter.convert(rawOutput); // Parses JSON to Java record
    }
}
```

### Example 4: Anti-Pattern (Manual Regex JSON Scraping) vs Type-Safe `.entity()`

#### The Anti-Pattern: Brittle Regex and Jackson Boilerplate
```java
// BAD: Prone to production crashes when LLM emits conversational chatter or markdown fences
@Service
public class BrittleParsingService {
    @Autowired private ChatModel chatModel;
    @Autowired private ObjectMapper objectMapper;

    public UserProfile parseProfile(String text) {
        String response = chatModel.call(new Prompt(text)).getResult().getOutput().getContent();

        // Fragile manual string cleaning!
        if (response.startsWith("```json")) {
            response = response.substring(7, response.length() - 3);
        }
        
        try {
            // Throws JsonParseException when model outputs: "Here is your JSON: { ... }"
            return objectMapper.readValue(response, UserProfile.class);
        } catch (JsonProcessingException e) {
            throw new RuntimeException("Failed to parse LLM output", e);
        }
    }
}
```

#### Production Fix: Native `ChatClient.entity()`
```java
// GOOD: Automatically formats prompt, cleans markdown, validates schema, and parses type-safely
@Service
public class RobustParsingService {
    private final ChatClient chatClient;

    public RobustParsingService(ChatClient.Builder builder) {
        this.chatClient = builder.build();
    }

    public UserProfile parseProfile(String text) {
        return chatClient.prompt()
                .user(text)
                .call()
                .entity(UserProfile.class);
    }
}
```

### Example 5: Native OpenAI Strict JSON Schema Mode

Enforcing 100% deterministic schema compliance using OpenAI's native API parameter:

```java
package com.example.ai.nativejson;

import org.springframework.ai.chat.client.ChatClient;
import org.springframework.ai.openai.OpenAiChatOptions;
import org.springframework.stereotype.Service;

public record SecurityAuditReport(String severity, int score, List<String> vulnerabilities) {}

@Service
public class StrictJsonAuditService {

    private final ChatClient chatClient;

    public StrictJsonAuditService(ChatClient.Builder builder) {
        this.chatClient = builder.build();
    }

    public SecurityAuditReport runAudit(String codeSnippet) {
        // Forces the OpenAI engine into native strict structured output mode
        OpenAiChatOptions options = OpenAiChatOptions.builder()
                .withResponseFormat(new OpenAiChatOptions.ResponseFormat(
                        OpenAiChatOptions.ResponseFormat.Type.JSON_SCHEMA,
                        """
                        {
                          "name": "SecurityAuditReport",
                          "strict": true,
                          "schema": {
                            "type": "object",
                            "properties": {
                              "severity": {"type": "string", "enum": ["LOW", "MEDIUM", "HIGH", "CRITICAL"]},
                              "score": {"type": "integer"},
                              "vulnerabilities": {"type": "array", "items": {"type": "string"}}
                            },
                            "required": ["severity", "score", "vulnerabilities"],
                            "additionalProperties": false
                          }
                        }
                        """
                ))
                .build();

        return chatClient.prompt()
                .user("Audit the following code for security vulnerabilities:\n" + codeSnippet)
                .options(options)
                .call()
                .entity(SecurityAuditReport.class);
    }
}
```

---

## 6. Best Practices

1. **Use Immutable Java 21 Records**: Records provide clear, canonical constructors and transparent components that Jackson and Spring AI map effortlessly.
2. **Annotate Record Fields with `@JsonPropertyDescription`**: Detailed field descriptions guide the LLM on expected formats (e.g., `"ISO-8601 Date String"`, `"Two-letter country code (US, UK, DE)"`).
3. **Use `.entity()` Operator for Cleaner Code**: Favor `chatClient.prompt().call().entity(MyRecord.class)` over manually instantiating `BeanOutputConverter`.
4. **Use Enums for Fixed Categories**: If a field has a limited set of valid options (e.g., `Sentiment { POSITIVE, NEUTRAL, NEGATIVE }`), use a Java `enum`. Spring AI automatically emits the `enum` constraints in the JSON Schema.
5. **Handle Extraction Failures with Retries**: LLMs may rarely produce invalid JSON under high temperature settings. Configure `spring.ai.retry` with at least 2 retry attempts.
6. **Set Temperature to Zero (`0.0`) for Extraction**: When extracting structured data from documents, set `temperature=0.0` to maximize determinism and minimize creative hallucinations.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| **High Temperature on Extraction Tasks** | Setting `temperature=0.8` causes the model to invent creative JSON fields or violate schema rules. | Always set `temperature=0.0` or `0.1` for structured extraction. |
| **Omitting `@JsonPropertyDescription` on Ambiguous Fields** | A field named `status` can be formatted as integer `0`, string `"active"`, or boolean `true`. | Add `@JsonPropertyDescription("Valid values: PENDING, APPROVED, REJECTED")`. |
| **Using Complex Inheritance in DTOs** | Polymorphic class hierarchies (`abstract class BaseEntity`) confuse JSON schema generators. | Keep extraction records flat, composable, and record-based. |
| **Forgetting `@JsonFormat` for Dates** | Models format dates in random formats (`"November 24th, 2026"` vs `"2026-11-24"`), causing Jackson deserialization exceptions. | Use Java `LocalDate` or annotate with `@JsonFormat(pattern = "yyyy-MM-dd")`. |
| **Parsing Primitive Types Directly** | Calling `.entity(Integer.class)` on a prompt often fails because LLMs wrap answers in text. | Wrap primitives in a dedicated record: `record CountResult(int count) {}`. |
| **Missing Default Constructors on Standard Classes** | Jackson throws `InvalidDefinitionException` if a standard POJO lacks a no-arg constructor. | Use Java 21 `record`s which require no zero-arg constructors. |

---

## 8. Performance & Security Notes

### Performance
- **Faster Token Generation**: Structured outputs restrict the token vocabulary tree during generation, often resulting in slightly faster token generation times compared to unbounded creative text.
- **Immediate Deserialization**: Because the output is guaranteed to match the JSON schema, Jackson deserializes the JSON string in sub-millisecond time.

### Security
- **Injection Sanitization**: LLMs can extract malicious scripts (e.g., `<script>alert(1)</script>`) from untrusted user documents into the JSON fields. Apply HTML escaping and validation when persisting extracted strings to databases.

---

## 9. Comparison / When to Use What

| Technique | Setup Complexity | Reliability | Supported Providers |
|---|---|---|---|
| **Free-form Text + Regex Parsing** | Low | Very Poor (~70% success) | All Providers |
| **`BeanOutputConverter` (Prompt-Engineered)** | Minimal (Spring AI Default)| High (~95% success) | All Providers (OpenAI, Claude, Ollama)|
| **Native Strict JSON Schema Mode** | Moderate | 100% Deterministic | OpenAI, Azure OpenAI, Gemini |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>1. How does Spring AI's `BeanOutputConverter` enforce structured output across any LLM provider?</strong></summary>

`BeanOutputConverter`:
1. Uses Jackson's `JsonSchemaGenerator` to inspect the Java target class and generate a standardized JSON Schema.
2. Invokes `getFormat()`, which constructs an explicit instruction block:
   *"Your response should be in JSON format. Do not include any explanations, only provide a RFC8259 compliant JSON response following this format without markdown fences: [SCHEMA]"*.
3. Injects this format string into the prompt sent to the LLM.
4. When the response arrives, the converter removes any accidental markdown code fences (` ```json `) and delegates deserialization to Jackson's `ObjectMapper.readValue()`.
</details>

<details>
<summary><strong>2. What is "Constrained Decoding" in native model-level Structured Outputs?</strong></summary>

In standard autoregressive generation, the model predicts the next token by sampling from the entire vocabulary (~100,000 tokens) based on probability distributions.
In **Constrained Decoding** (e.g., OpenAI Strict Mode or llama.cpp grammar-based sampling):
- The model's sampling engine enforces a Context-Free Grammar (CFG) or JSON Schema state machine.
- At any point during generation, any token that would make the output invalid JSON (or violate the schema) has its logit probability set to $-\infty$.
- As a result, the model is physically incapable of emitting an invalid character, missing a closing bracket, or hallucinating a non-existent property name.
</details>

<details>
<summary><strong>3. Why are Java 21 Records particularly well-suited for Structured Output extraction in Spring AI?</strong></summary>

1. **Deterministic Components**: Records have a canonical constructor whose parameter names and types match component accessors 1:1, making schema introspection unambiguous.
2. **Immutability**: Guarantees that extracted data cannot be accidentally mutated by intermediate processing steps.
3. **No Boilerplate**: Eliminates getters, setters, and constructors, keeping DTO definitions concise and readable.
4. **Jackson Compatibility**: Jackson natively supports Record deserialization without requiring `@JsonCreator` annotations.
</details>

<details>
<summary><strong>4. How do you extract a `List<MyRecord>` directly using Spring AI?</strong></summary>

By passing a `ParameterizedTypeReference`:
```java
List<MyRecord> items = chatClient.prompt()
        .user("Extract all items...")
        .call()
        .entity(new ParameterizedTypeReference<List<MyRecord>>() {});
```
This preserves the generic type information at runtime (avoiding Java type erasure), allowing Spring AI to construct a JSON Schema representing an array of `MyRecord` objects.
</details>

<details>
<summary><strong>5. What is the difference between `BeanOutputConverter`, `ListOutputConverter`, and `MapOutputConverter`?</strong></summary>

- **`BeanOutputConverter<T>`**: Generates a complex JSON Schema and deserializes into a custom Java POJO or `record`.
- **`ListOutputConverter`**: Formats output as a comma-separated list of values (e.g., `["item1", "item2"]`) and returns a `List<String>`.
- **`MapOutputConverter`**: Formats output as a flat key-value dictionary and returns a `Map<String, Object>`.
</details>

<details>
<summary><strong>6. How does temperature setting affect Structured Output reliability?</strong></summary>

Higher temperatures ($>0.7$) introduce randomness into token sampling. This increases the probability that the model will invent creative deviations from the schema, misspell property names, or emit extra conversational comments.
For extraction and structured generation, **always set temperature to `0.0` or `0.1`** to maximize deterministic compliance with the JSON schema.
</details>

<details>
<summary><strong>7. How do you handle schema evolution when parsing legacy documents with new fields?</strong></summary>

In your Java record, you can provide default values in a compact constructor or configure Jackson's `ObjectMapper`:
```java
public record UserData(String name, String email, String tier) {
    public UserData {
        if (tier == null) tier = "STANDARD"; // Fallback default
    }
}
```
Ensure that `FAIL_ON_UNKNOWN_PROPERTIES` is disabled if the model might generate unexpected extra attributes.
</details>

<details>
<summary><strong>8. Can Structured Output be combined with Tool Calling in Spring AI?</strong></summary>

Yes! An AI Agent can execute multiple intermediate tool calls (e.g., querying databases, checking external APIs), and then format its *final* answer to the user as a strongly-typed Java record using `.entity(MyReport.class)`.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Extract a Simple Sentiment Record</strong></summary>

**Objective**: Write a service method that classifies user feedback into a `SentimentReport` record.

**Solution**:
```java
public enum SentimentType { POSITIVE, NEUTRAL, NEGATIVE }

public record SentimentReport(SentimentType sentiment, int confidencePercent) {}

@Service
public class SentimentAnalysisService {
    private final ChatClient chatClient;

    public SentimentAnalysisService(ChatClient.Builder builder) {
        this.chatClient = builder.build();
    }

    public SentimentReport analyze(String feedback) {
        return chatClient.prompt()
                .user("Analyze the sentiment of: " + feedback)
                .call()
                .entity(SentimentReport.class);
    }
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Extract Comma-Separated List via `ListOutputConverter`</strong></summary>

**Objective**: Extract a list of keywords from a blog post as a `List<String>`.

**Solution**:
```java
@Service
public class KeywordExtractorService {
    private final ChatClient chatClient;

    public KeywordExtractorService(ChatClient.Builder builder) {
        this.chatClient = builder.build();
    }

    public List<String> extractKeywords(String text) {
        return chatClient.prompt()
                .user("Extract the top 5 technical keywords from this text: " + text)
                .call()
                .entity(new ParameterizedTypeReference<List<String>>() {});
    }
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Extract Recipe with Nested Ingredients Record</strong></summary>

**Objective**: Extract a structured recipe containing a title, cooking time, and a list of ingredients with quantities.

**Solution**:
```java
public record Ingredient(String name, double amount, String unit) {}

public record Recipe(String title, int cookingTimeMinutes, List<Ingredient> ingredients, List<String> instructions) {}

@Service
public class RecipeParsingService {
    private final ChatClient chatClient;

    public RecipeParsingService(ChatClient.Builder builder) {
        this.chatClient = builder.build();
    }

    public Recipe parseRecipe(String rawText) {
        return chatClient.prompt()
                .system("You are an expert culinary parser.")
                .user(rawText)
                .call()
                .entity(Recipe.class);
    }
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Enforce Zero Temperature in Extraction Request</strong></summary>

**Objective**: Configure `ChatClient` prompt options so that extraction runs with `temperature = 0.0`.

**Solution**:
```java
@Service
public class DeterministicExtractor {
    private final ChatClient chatClient;

    public DeterministicExtractor(ChatClient.Builder builder) {
        this.chatClient = builder.build();
    }

    public ExtractedData extract(String text) {
        return chatClient.prompt()
                .user(text)
                .options(ChatOptionsBuilder.builder().withTemperature(0.0).build())
                .call()
                .entity(ExtractedData.class);
    }
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Custom Post-Processing Validator on Extracted Record</strong></summary>

**Objective**: Write a service that extracts a record and runs Jakarta Bean Validation programmatically, throwing an exception if constraints are violated.

**Solution**:
```java
public record AccountApplication(
    @NotBlank String fullName,
    @Email String email,
    @Min(18) int age
) {}

@Service
public class ValidatedExtractionService {

    private final ChatClient chatClient;
    private final Validator validator;

    public ValidatedExtractionService(ChatClient.Builder builder, Validator validator) {
        this.chatClient = builder.build();
        this.validator = validator;
    }

    public AccountApplication extractAndValidate(String applicantText) {
        AccountApplication application = chatClient.prompt()
                .user(applicantText)
                .call()
                .entity(AccountApplication.class);

        var violations = validator.validate(application);
        if (!violations.isEmpty()) {
            throw new ConstraintViolationException("Extracted data violated constraints: " + violations, violations);
        }

        return application;
    }
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Integration Test Verifying Structured Deserialization</strong></summary>

**Objective**: Write a Spring Boot integration test asserting that extracting from a mock response yields a populated, non-null Java record.

**Solution**:
```java
@SpringBootTest
class StructuredOutputIntegrationTest {

    @Autowired private ChatClient.Builder builder;

    record BookDetails(String title, String author, int publicationYear) {}

    @Test
    void shouldExtractStructuredBookRecord() {
        ChatClient client = builder.build();

        BookDetails book = client.prompt()
                .user("Extract book details from: 'Clean Code was authored by Robert C. Martin and published in 2008.'")
                .call()
                .entity(BookDetails.class);

        assertNotNull(book);
        assertEquals("Clean Code", book.title());
        assertTrue(book.author().contains("Martin"));
        assertEquals(2008, book.publicationYear());
    }
}
```
</details>

---

## 12. Summary & Cheat Sheet

```
+----------------------------------------------------------------------------------------------------+
| STRUCTURED OUTPUTS IN SPRING AI CHEAT SHEET                                                        |
+----------------------------------------------------------------------------------------------------+
| Operator / Class    | Syntax                                    | Purpose                          |
+---------------------+-------------------------------------------+----------------------------------+
| Fluent Entity Call  | chatClient.prompt().call().entity(Class)  | One-line schema injection and    |
|                     |                                           | automatic Jackson deserialization|
+---------------------+-------------------------------------------+----------------------------------+
| Generic Collections | .entity(new ParameterizedTypeReference<   | Deserializes List<T> directly    |
|                     |         List<MyRecord>>() {})             | without wrapper classes          |
+---------------------+-------------------------------------------+----------------------------------+
| Low-level Converter | new BeanOutputConverter<>(Class.class)    | Generates getFormat() string for |
|                     |                                           | custom prompt templates          |
+---------------------+-------------------------------------------+----------------------------------+
| Field Descriptions  | @JsonPropertyDescription("...")           | Guides model on required format  |
|                     |                                           | and valid values                 |
+---------------------+-------------------------------------------+----------------------------------+
| Temperature Rule    | Always set temperature = 0.0              | Maximizes schema adherence       |
+---------------------+-------------------------------------------+----------------------------------+
| Target Model        | Java 21 Record                            | Immutable, clean JSON mapping    |
+----------------------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Spring AI Structured Output API Documentation](https://docs.spring.io/spring-ai/reference/api/structured-output-converter.html)
- [OpenAI Structured Outputs Guide and JSON Schema Mode](https://platform.openai.com/docs/guides/structured-outputs)
- [Jackson JSON Schema Module Documentation](https://github.com/FasterXML/jackson-module-jsonSchema)
- [Constrained Decoding in Large Language Models (Grammar-Based Sampling)](https://arxiv.org/abs/2307.09702)
- [Baeldung: Guide to Spring AI Structured Output](https://www.baeldung.com/spring-ai-structured-output)
