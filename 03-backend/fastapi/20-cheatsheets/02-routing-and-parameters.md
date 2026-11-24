# Quick Reference: Routing & Parameter Declarations Cheatsheet

## Path, Query, Header, Cookie & Body Parameters

```python
from typing import Annotated
import uuid
from fastapi import APIRouter, Body, Cookie, Header, Path, Query
from pydantic import BaseModel, Field

router = APIRouter(prefix="/api/v1/items", tags=["Items"])

class ItemPayload(BaseModel):
    name: str = Field(..., min_length=2, max_length=100)
    price: float = Field(..., gt=0)

@router.put("/{item_id}")
async def update_item(
    # 1. Path Parameter with validation
    item_id: Annotated[uuid.UUID, Path(title="Item ID", description="UUID4 of item")],
    
    # 2. Query Parameters
    q: Annotated[str | None, Query(max_length=50)] = None,
    page: Annotated[int, Query(ge=1)] = 1,
    size: Annotated[int, Query(ge=1, le=100)] = 20,
    
    # 3. Request Header
    idempotency_key: Annotated[str, Header(alias="Idempotency-Key")],
    
    # 4. HTTP Cookie
    session_id: Annotated[str | None, Cookie()] = None,
    
    # 5. Request Body (JSON)
    payload: Annotated[ItemPayload, Body(embed=False)],
):
    return {"id": item_id, "name": payload.name, "page": page}
```

---

## Sub-Router Inclusion & Prefix Nesting

```python
from fastapi import FastAPI, APIRouter

app = FastAPI()
api_v1 = APIRouter(prefix="/api/v1")
users_router = APIRouter(prefix="/users", tags=["Users"])
orders_router = APIRouter(prefix="/orders", tags=["Orders"])

api_v1.include_router(users_router)
api_v1.include_router(orders_router)
app.include_router(api_v1)
```

---

## Route Metadata & Documentation Tags

```python
@router.post(
    "/",
    status_code=201,
    summary="Create a new resource",
    description="Full Markdown description displayed in Swagger UI.",
    response_description="The freshly created database entity",
    deprecated=False,
    include_in_schema=True,  # Set False to hide from /docs
    openapi_extra={"x-internal-team": "billing"},
)
async def create_entity():
    pass
```
