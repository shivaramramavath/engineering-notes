---
title: "Extra Data Types: UUID, Datetime & Decimal"
track: "fastapi"
module: "01-fundamentals"
note_number: 11
prerequisites:
  - "01-fundamentals/02-path-and-query-parameters"
  - "01-fundamentals/03-request-body-and-pydantic-models"
python_version: "3.12+"
fastapi_version: "0.115.x+"
pydantic_version: "2.10.x+"
tested_with: "FastAPI 0.115.6, Pydantic 2.10.4, Python 3.12"
free_threading: "Supported"
status: "Completed"
---

# Extra Data Types: UUID, Datetime & Decimal

While primitive JSON only natively represents strings, numbers, booleans, arrays, and objects, real-world enterprise backends deal with rich domain data types: **cryptographic UUIDs**, **timezone-aware datetimes**, **arbitrary-precision financial decimals**, and **IP network addresses**.

FastAPI and Pydantic v2 automatically parse, validate, and serialize these advanced Python standard library types across path, query, and request body parameters without requiring manual conversion boilerplate.

---

## 1. Conceptual Foundation & Mental Model

### The Type Transformation Bridge

```mermaid
flowchart LR
    subgraph HTTP Wire Format (JSON / Query)
        RawUUID["String: '550e8400-e29b-41d4-a716-446655440000'"]
        RawDate["String: '2026-11-24T14:30:00Z' (ISO 8601)"]
        RawDec["String/Number: '199.99' (Financial)"]
        RawIP["String: '192.168.1.1'"]
    end

    subgraph Pydantic v2 Rust Coercion
        Core[Pydantic-Core Parser & Validator]
    end

    subgraph Python Route Handler
        PyUUID["uuid.UUID instance"]
        PyDate["datetime.datetime (timezone-aware)"]
        PyDec["decimal.Decimal (exact precision)"]
        PyIP["ipaddress.IPv4Address instance"]
    end

    RawUUID --> Core --> PyUUID
    RawDate --> Core --> PyDate
    RawDec --> Core --> PyDec
    RawIP --> Core --> PyIP
```

---

## 2. Supported Complex Data Types

| Python Type | Wire Format | Parsing Rules & Validation |
| :--- | :--- | :--- |
| **`uuid.UUID`** | String | Validates standard 36-char hexadecimal format (`8-4-4-4-12`) or 32-char hex. |
| **`datetime.datetime`**| String | Validates ISO 8601 format (`YYYY-MM-DDTHH:MM:SSZ`). Automatically normalizes timezone offsets. |
| **`datetime.date`** | String | Validates calendar date string (`YYYY-MM-DD`). |
| **`datetime.time`** | String | Validates standard time format (`HH:MM:SS`). |
| **`datetime.timedelta`**| Number or String | Parses seconds (e.g. `3600`) or ISO 8601 duration strings (`PT1H`). |
| **`decimal.Decimal`** | String or Number | Crucial for currency and finance. Avoids IEEE 754 binary floating-point roundoff errors. |
| **`ipaddress.IPv4Address`**| String | Validates valid IPv4 dot-decimal notation (`0.0.0.0` - `255.255.255.255`). |
| **`ipaddress.IPv6Address`**| String | Validates standard hexadecimal IPv6 notation. |
| **`pathlib.Path`** | String | Validates and converts to an OS-specific `pathlib.Path` object. |

---

## 3. Comprehensive Syntax & Core Mechanics

### Type Declarations in FastAPI Signatures

```python
from datetime import datetime, timezone
from decimal import Decimal
from uuid import UUID
from fastapi import FastAPI
from pydantic import BaseModel, Field

app = FastAPI()

class Invoice(BaseModel):
    id: UUID
    issued_at: datetime
    total_amount: Decimal = Field(..., max_digits=10, decimal_places=2)

@app.get("/invoices/{invoice_id}")
async def get_invoice(invoice_id: UUID) -> Invoice:
    # invoice_id is guaranteed to be a valid Python uuid.UUID instance
    return Invoice(
        id=invoice_id,
        issued_at=datetime.now(timezone.utc),
        total_amount=Decimal("1250.50"),
    )
```

---

## 4. Practical Implementation: 5 Verified Examples

### Example 1: UUID Path Parameters with Automatic 422 Rejection

```python
# uuid_route.py
from uuid import UUID
from fastapi import FastAPI, Path, status
from pydantic import BaseModel

app = FastAPI()

class Resource(BaseModel):
    id: UUID
    name: str

DATABASE: dict[UUID, str] = {}

@app.get("/resources/{resource_id}", response_model=Resource)
async def read_resource(resource_id: UUID) -> Resource:
    # If the client supplies an invalid UUID string, FastAPI aborts with 422
    name = DATABASE.get(resource_id, "Sample Resource")
    return Resource(id=resource_id, name=name)
```

Verified curl test with valid UUID:
```bash
curl http://127.0.0.1:8000/resources/a0eebc99-9c0b-4ef8-bb6d-6bb9bd380a11
# Output: {"id":"a0eebc99-9c0b-4ef8-bb6d-6bb9bd380a11","name":"Sample Resource"}
```

Verified curl test with invalid UUID:
```bash
curl -i http://127.0.0.1:8000/resources/not-a-valid-uuid
# Output: HTTP/1.1 422 Unprocessable Entity
```

### Example 2: Financial Precision with `Decimal`

Demonstrating why `Decimal` is mandatory for monetary ledgers:

```python
# financial_ledger.py
from decimal import Decimal
from fastapi import FastAPI
from pydantic import BaseModel, Field

app = FastAPI()

class LedgerEntry(BaseModel):
    account_id: str
    credit: Decimal = Field(..., max_digits=12, decimal_places=4)
    debit: Decimal = Field(..., max_digits=12, decimal_places=4)

@app.post("/ledger/reconcile")
async def reconcile_ledger(entry: LedgerEntry) -> dict[str, str | Decimal]:
    # Exact decimal arithmetic without float rounding errors:
    # 0.1 + 0.2 in float is 0.30000000000000004!
    # In Decimal, Decimal("0.1") + Decimal("0.2") == Decimal("0.3")
    net_balance = entry.credit - entry.debit
    return {
        "account_id": entry.account_id,
        "net_balance": net_balance,
        "is_balanced": net_balance == Decimal("0.0000"),
    }
```

### Example 3: ISO 8601 Timestamps & Timedelta Durations

```python
# time_params.py
from datetime import datetime, timedelta, timezone
from typing import Annotated
from fastapi import FastAPI, Query
from pydantic import BaseModel

app = FastAPI()

class AuditEvent(BaseModel):
    event_name: str
    occurred_at: datetime
    execution_duration: timedelta

@app.get("/events/search")
async def search_events(
    start_time: Annotated[datetime, Query(description="Start time in ISO 8601 format")],
    window: Annotated[timedelta, Query(description="Duration window (e.g. 3600 or PT1H)")] = timedelta(hours=1),
) -> list[AuditEvent]:
    end_time = start_time + window
    return [
        AuditEvent(
            event_name="DATABASE_BACKUP",
            occurred_at=start_time,
            execution_duration=timedelta(seconds=142.5),
        )
    ]
```

Request:
```bash
curl "http://127.0.0.1:8000/events/search?start_time=2026-11-24T12:00:00Z&window=PT2H"
```
Response:
```json
[
  {
    "event_name": "DATABASE_BACKUP",
    "occurred_at": "2026-11-24T12:00:00Z",
    "execution_duration": 142.5
  }
]
```

### Example 4: IP Address Filtering with `ipaddress.IPv4Address`

```python
# ip_firewall.py
from ipaddress import IPv4Address, IPv4Network
from fastapi import FastAPI, HTTPException, status
from pydantic import BaseModel

app = FastAPI()

TRUSTED_SUBNET = IPv4Network("10.0.0.0/8")

class IngressRule(BaseModel):
    source_ip: IPv4Address
    action: str

@app.post("/firewall/rules")
async def evaluate_firewall(rule: IngressRule) -> dict[str, str | bool]:
    is_trusted = rule.source_ip in TRUSTED_SUBNET
    return {
        "ip": str(rule.source_ip),
        "is_internal": is_trusted,
        "is_private": rule.source_ip.is_private,
    }
```

Passing `"source_ip": "10.240.1.5"`:
```json
{
  "ip": "10.240.1.5",
  "is_internal": true,
  "is_private": true
}
```

Passing `"source_ip": "999.999.999.999"` returns immediate 422 Unprocessable Entity!

### Example 5: Pathlib Validation for Server Asset Verification

```python
# path_type.py
from pathlib import Path
from fastapi import FastAPI, HTTPException, status
from pydantic import BaseModel

app = FastAPI()

class FileMetadata(BaseModel):
    file_path: Path

@app.post("/system/inspect-file")
async def inspect_file(payload: FileMetadata) -> dict[str, str | bool]:
    path = payload.file_path
    return {
        "name": path.name,
        "suffix": path.suffix,
        "is_absolute": path.is_absolute(),
    }
```

---

## 5. Anti-Patterns & Common Traps

| Anti-Pattern | Why It Fails | Production Fix |
| :--- | :--- | :--- |
| **Using `float` for currency or pricing** | Binary floating point cannot accurately represent base-10 decimals, causing fractional cent losses. | Always use `Decimal` with explicit `max_digits` and `decimal_places`. |
| **Using naive `datetime.now()` without timezone** | Produces ambiguous server-local timestamps that corrupt across daylight savings or multi-region servers. | Always use `datetime.now(timezone.utc)`. |
| **Parsing UUIDs manually via `uuid.UUID(str_id)` in route body** | If the string is invalid, raises `ValueError`, resulting in an unhandled 500 error instead of a 422. | Declare parameter type as `uuid.UUID` in the route signature. |
| **Assuming `timedelta` is always serialized as an ISO string** | Default serialization converts `timedelta` into total seconds as a float. | Use a field serializer if ISO-8601 duration strings are required by clients. |
| **Comparing string representations of UUIDs directly** | Inconsistent capitalization or hyphens causes false mismatches. | Compare parsed `UUID` instances directly (`id_a == id_b`). |

---

## 6. Performance & Resource Characteristics

- **Rust-Backed Parsing**: Pydantic v2 validates UUID strings in native C/Rust machine code. Validating a UUID takes less than 80 nanoseconds.
- **Decimal Serialization**: `Decimal` serialization in Pydantic v2 converts decimals to string or float based on `json_encoders` settings. Use string encoding for currency to prevent client-side JavaScript floating point truncation (JavaScript `Number.MAX_SAFE_INTEGER`).

---

## 7. Edge Cases, Failure Modes & Gotchas

1. **JavaScript Float Precision Limit**: JavaScript cannot safely represent numbers greater than $2^{53} - 1$ (`9007199254740991`). When serializing large `Decimal` or 64-bit integers, serialize them as JSON strings.
2. **Timezone Offset Notation**: FastAPI accepts both `Z` (Zulu/UTC) and explicit offsets like `+02:00` or `-05:00`. Parsed `datetime` objects retain their original offset information.

---

## 8. Security Implications & OWASP Alignment

- **Denial of Service via Massive Decimals**: An attacker can supply a decimal with 100,000 digits. Protect models by enforcing `max_digits` and `decimal_places` in `Field()`.
- **IP Address Spoofing**: Ensure `IPv4Address` parameters come from validated payload data or verified proxy headers (`request.client.host`), not spoofable client headers.

---

## 9. Modern Tooling Integration

- **OpenAPI Schema Formats**: Pydantic maps complex types to OpenAPI standard `format` identifiers:
  - `uuid.UUID` -> `{"type": "string", "format": "uuid"}`
  - `datetime.datetime` -> `{"type": "string", "format": "date-time"}`
  - `datetime.date` -> `{"type": "string", "format": "date"}`
  - `IPv4Address` -> `{"type": "string", "format": "ipv4"}`

---

## 10. Real-World Case Study: Production Incident

### The Incident
An international accounting platform suffered an audit discrepancy where debit and credit totals across 1,000,000 transactions differed by \$412.34.

### The Root Cause
The transaction payload was typed using standard Python `float`:
```python
class Transaction(BaseModel):
    amount: float
```
When calculating running account totals in Python using `float`, IEEE 754 floating-point inaccuracies accumulated across hundreds of thousands of transactions:
`0.1 + 0.2 - 0.3 = 5.551115123125783e-17`
The slight rounding differences compounded into significant monetary errors.

### The Resolution
Migrated all financial schemas and database columns to `Decimal`:
```python
class Transaction(BaseModel):
    amount: Decimal = Field(..., max_digits=14, decimal_places=4)
```
Database columns were converted to `NUMERIC(14, 4)`, completely eliminating floating-point rounding discrepancies.

---

## 11. Hands-On Exercises & Challenge Problems

### Warm-Up Exercises
1. **UUID Path Validator**: Build a route `@app.get("/items/{id}")` typed with `UUID`. Test with valid UUIDv4 strings and verify that non-UUID strings return HTTP 422.
2. **ISO Date Filter**: Build an endpoint that accepts a query parameter `start_date: date` and returns whether the date falls on a weekend.

### Intermediate Challenges
3. **Exact Currency Calculator**: Create a `POST /checkout` endpoint that accepts a list of items with `Decimal` prices and tax percentages. Calculate the subtotal, tax total, and grand total strictly in `Decimal`.
4. **Time Window Validator**: Build a Pydantic model with `start_time: datetime` and `end_time: datetime`. Use a `@model_validator(mode="after")` to verify that `end_time` is strictly after `start_time`.

### Production-Grade Challenges
5. **CIDR Network Subnet Matcher**: Implement an endpoint that accepts an `IPv4Address` and checks it against a list of blocked CIDR network subnets (`IPv4Network`). If matched, reject the request with HTTP 403 Forbidden.
6. **Timezone Converter Pipeline**: Create an API that receives events with timestamps in arbitrary international timezones, normalizes all timestamps to UTC (`datetime.now(timezone.utc)`), and returns the elapsed time delta since the event occurred.

---

## 12. Interview Mastery: Deep-Dive Questions & Answers

#### Q1: Why should monetary values never be stored or parsed as `float` in Python APIs?
**Answer**: Standard floating-point numbers (`float`) use IEEE 754 binary floating-point representation, which cannot accurately represent common base-10 fractions (such as 0.1 or 0.01). Arithmetic operations on floats introduce minute rounding errors that compound over thousands of transactions. `decimal.Decimal` uses exact base-10 arithmetic, guaranteeing fixed precision and preventing rounding errors.

#### Q2: What happens if a client passes a non-UUID string to an endpoint typed with `uuid.UUID`?
**Answer**: Pydantic v2's parser fails to coerce the string into a valid UUID object and raises a `RequestValidationError`. FastAPI automatically catches this and returns an **HTTP 422 Unprocessable Entity** response detailing that the input could not be parsed as a valid UUID.

#### Q3: What format does FastAPI expect for `datetime` parameters?
**Answer**: FastAPI expects standard **ISO 8601** formatted strings (e.g. `2026-11-24T14:30:00Z` or `2026-11-24T16:30:00+02:00`). Pydantic v2 can also parse standard Unix epoch timestamps (integers/floats) into datetime objects.

#### Q4: How does Pydantic v2 serialize `uuid.UUID` and `datetime` objects into JSON?
**Answer**: In default serialization (`model.model_dump(mode="json")`), `UUID` objects are serialized into standard 36-character hyphenated strings, and `datetime` objects are serialized into ISO 8601 formatted strings.

#### Q5: Can `datetime.timedelta` be passed as a query parameter in FastAPI?
**Answer**: Yes. Pydantic parses `timedelta` from either total seconds (e.g. `3600`) or ISO 8601 duration strings (e.g. `PT1H` for 1 hour, `P1D` for 1 day).

#### Q6: How does FastAPI document `uuid.UUID` and `IPv4Address` in OpenAPI?
**Answer**: FastAPI sets the OpenAPI schema `type: "string"` and applies standard format indicators: `format: "uuid"` for UUIDs, `format: "date-time"` for datetimes, and `format: "ipv4"` for IPv4 addresses.

#### Q7: Why is it critical to ensure datetimes are timezone-aware?
**Answer**: Naive datetimes (without `tzinfo`) lack timezone context and are interpreted according to the local machine's system clock. When deployed across distributed multi-region clouds or during daylight savings transitions, comparing naive datetimes causes incorrect ordering and scheduling bugs. Always enforce timezone awareness (e.g. UTC).

#### Q8: How can you enforce that a `Decimal` field has no more than two decimal places?
**Answer**: Use Pydantic's `Field()` with the `decimal_places` constraint: `price: Decimal = Field(..., decimal_places=2, max_digits=10)`.
