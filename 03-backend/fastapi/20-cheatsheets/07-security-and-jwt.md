# Quick Reference: Security, JWT & Password Hashing Cheatsheet

## Argon2 / Bcrypt Password Hashing

```python
from passlib.context import CryptContext

pwd_context = CryptContext(schemes=["argon2", "bcrypt"], deprecated="auto")

def hash_password(plain_password: str) -> str:
    return pwd_context.hash(plain_password)

def verify_password(plain_password: str, hashed_password: str) -> bool:
    return pwd_context.verify(plain_password, hashed_password)
```

---

## JWT Creation & Decoding (PyJWT)

```python
from datetime import datetime, timedelta, timezone
import jwt

SECRET_KEY = "your-secure-random-256-bit-key-min-32-chars"
ALGORITHM = "HS256"

def create_access_token(user_id: str, expires_delta: timedelta = timedelta(minutes=15)) -> str:
    now = datetime.now(timezone.utc)
    payload = {
        "sub": user_id,
        "iat": now,
        "nbf": now,
        "exp": now + expires_delta,
        "type": "access",
    }
    return jwt.encode(payload, SECRET_KEY, algorithm=ALGORITHM)

def decode_token(token: str) -> dict:
    try:
        return jwt.decode(token, SECRET_KEY, algorithms=[ALGORITHM])
    except jwt.ExpiredSignatureError:
        raise HTTPException(status_code=401, detail="Token expired")
    except jwt.PyJWTError:
        raise HTTPException(status_code=401, detail="Invalid token")
```

---

## OAuth2 Bearer Dependency

```python
from typing import Annotated
from fastapi import Depends, HTTPException, Security, status
from fastapi.security import HTTPAuthorizationCredentials, HTTPBearer

security = HTTPBearer()

async def get_current_user_id(
    credentials: Annotated[HTTPAuthorizationCredentials, Security(security)],
) -> str:
    token = credentials.credentials
    payload = decode_token(token)
    user_id = payload.get("sub")
    if not user_id:
        raise HTTPException(status_code=401, detail="Missing user identity")
    return user_id

@app.get("/me")
async def read_current_user(user_id: Annotated[str, Depends(get_current_user_id)]):
    return {"user_id": user_id}
```
