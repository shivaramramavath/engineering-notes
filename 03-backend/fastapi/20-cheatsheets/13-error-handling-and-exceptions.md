# Quick Reference: Error Handling & Custom Exception Handlers

## Custom Domain Exception Pattern

```python
class DomainError(Exception):
    """Base class for all domain business rule failures."""
    pass

class InsufficientFundsError(DomainError):
    def __init__(self, available: float, requested: float):
        self.available = available
        self.requested = requested
        super().__init__(f"Cannot withdraw ${requested:.2f}. Balance is ${available:.2f}")

class ResourceNotFoundError(DomainError):
    def __init__(self, resource: str, identifier: str):
        super().__init__(f"{resource} with identifier '{identifier}' was not found.")
```

---

## Global FastAPI Exception Handler Registration

```python
from fastapi import FastAPI, Request, status
from fastapi.exceptions import RequestValidationError
from fastapi.responses import JSONResponse

app = FastAPI()

# 1. Custom Domain Exception Handler
@app.exception_handler(InsufficientFundsError)
async def insufficient_funds_handler(request: Request, exc: InsufficientFundsError):
    return JSONResponse(
        status_code=status.HTTP_409_CONFLICT,
        content={
            "error_code": "INSUFFICIENT_FUNDS",
            "message": str(exc),
            "available": exc.available,
            "requested": exc.requested,
        },
    )

# 2. Standardized Pydantic Validation Error Handler (422)
@app.exception_handler(RequestValidationError)
async def validation_exception_handler(request: Request, exc: RequestValidationError):
    errors = [
        {
            "field": " -> ".join(str(loc) for loc in err["loc"]),
            "message": err["msg"],
            "type": err["type"],
        }
        for err in exc.errors()
    ]
    return JSONResponse(
        status_code=status.HTTP_422_UNPROCESSABLE_ENTITY,
        content={"error_code": "VALIDATION_FAILED", "details": errors},
    )

# 3. Catch-All Unhandled 500 Handler (Prevents internal stack trace leakage)
@app.exception_handler(Exception)
async def generic_unhandled_exception_handler(request: Request, exc: Exception):
    # In production: log full traceback to Sentry / Datadog
    logger.exception("Unhandled server exception on %s", request.url.path)
    return JSONResponse(
        status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
        content={
            "error_code": "INTERNAL_SERVER_ERROR",
            "message": "An unexpected error occurred. Please contact support.",
        },
    )
```
