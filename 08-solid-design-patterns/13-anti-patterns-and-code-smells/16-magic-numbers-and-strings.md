# Magic Numbers & Magic Strings

| Field | Value |
| :--- | :--- |
| **Category** | Code Smells (Readability & Safety) |
| **Fix** | Named Constants, Enums, Sealed Types |

> **TL;DR**: Unexplained literals (`if (status == 3)`, `sleep(86400)`, `if (role == "adm")`) hide intent and invite typos. Replace them with named constants (`SECONDS_PER_DAY`) or typed `enum` / union types (`OrderStatus.PAID`).

## Before vs After
```python
# ❌ Magic literals
if user.type == 2 and elapsed > 86400: ...
# ✅ Named enum + constant
SECONDS_PER_DAY = 24 * 60 * 60
if user.tier == Tier.PREMIUM and elapsed > SECONDS_PER_DAY: ...
```
