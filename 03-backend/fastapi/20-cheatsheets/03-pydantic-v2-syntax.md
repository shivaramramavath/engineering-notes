# Quick Reference: Pydantic v2 Syntax & Core Migrations Cheatsheet

## V1 to V2 Syntax Replacement Matrix

| Feature | Pydantic v1 (Deprecated) | Pydantic v2 (Modern Standard) |
|:---|:---|:---|
| **Parse Raw JSON** | `Model.parse_raw(json_str)` | `Model.model_validate_json(json_bytes_or_str)` |
| **Parse Python Dict** | `Model.parse_obj(dict_obj)` | `Model.model_validate(dict_obj)` |
| **Dump to Dict** | `model.dict()` | `model.model_dump(mode="python")` |
| **Dump to JSON String** | `model.json()` | `model.model_dump_json(indent=2)` |
| **Schema Generation** | `Model.schema()` | `Model.model_json_schema()` |
| **Model Configuration** | `class Config:` inside model | `model_config = ConfigDict(...)` |
| **Field Validator** | `@validator("x")` | `@field_validator("x", mode="after")` |
| **Root Validator** | `@root_validator` | `@model_validator(mode="after")` |
| **ORM Object Mapping**| `orm_mode = True` | `ConfigDict(from_attributes=True)` |

---

## Complete Model Declaration Template

```python
from datetime import datetime, timezone
from decimal import Decimal
import uuid
from pydantic import (
    BaseModel,
    ConfigDict,
    EmailStr,
    Field,
    field_validator,
    model_validator,
)

class UserRegisterSchema(BaseModel):
    # Model Configuration in V2
    model_config = ConfigDict(
        from_attributes=True,
        populate_by_name=True,
        str_strip_whitespace=True,
        validate_default=True,
        extra="forbid",  # Rejects unexpected JSON properties
    )

    id: uuid.UUID = Field(default_factory=uuid.uuid4)
    username: str = Field(..., min_length=3, max_length=30, pattern=r"^[a-zA-Z0-9_]+$")
    email: EmailStr
    balance: Decimal = Field(default=Decimal("0.00"), ge=0)
    created_at: datetime = Field(default_factory=lambda: datetime.now(timezone.utc))

    @field_validator("username", mode="after")
    @classmethod
    def normalize_username(cls, v: str) -> str:
        return v.lower()

    @model_validator(mode="after")
    def check_invariants(self) -> "UserRegisterSchema":
        if self.username == "admin" and not self.email.endswith("@company.com"):
            raise ValueError("Admin account must use corporate email")
        return self
```
