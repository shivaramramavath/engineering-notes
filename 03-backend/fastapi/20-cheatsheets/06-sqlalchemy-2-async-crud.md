# Quick Reference: SQLAlchemy 2.0 Async CRUD & Query Recipes

## Async Engine & Session Setup

```python
from sqlalchemy.ext.asyncio import (
    AsyncEngine,
    AsyncSession,
    async_sessionmaker,
    create_async_engine,
)

engine: AsyncEngine = create_async_engine(
    "postgresql+asyncpg://postgres:pass@localhost:5432/mydb",
    pool_size=20,
    max_overflow=10,
    pool_pre_ping=True,
)

async_session = async_sessionmaker(bind=engine, class_=AsyncSession, expire_on_commit=False)
```

---

## Complete Modern Async CRUD Operations

```python
from sqlalchemy import delete, func, select, update
from sqlalchemy.orm import selectinload

# 1. CREATE
async def create_user(session: AsyncSession, name: str, email: str) -> User:
    user = User(name=name, email=email)
    session.add(user)
    await session.commit()
    await session.refresh(user)
    return user

# 2. READ (Single with Eager Loading)
async def get_user_with_orders(session: AsyncSession, user_id: uuid.UUID) -> User | None:
    stmt = (
        select(User)
        .options(selectinload(User.orders))  # Eliminates N+1 query loops
        .where(User.id == user_id)
    )
    result = await session.execute(stmt)
    return result.scalars().first()

# 3. READ (Paginated List)
async def list_users(session: AsyncSession, limit: int = 20, offset: int = 0) -> list[User]:
    stmt = select(User).order_by(User.created_at.desc()).limit(limit).offset(offset)
    result = await session.execute(stmt)
    return list(result.scalars().all())

# 4. UPDATE (Atomic In-DB Update)
async def update_user_status(session: AsyncSession, user_id: uuid.UUID, is_active: bool) -> None:
    stmt = update(User).where(User.id == user_id).values(is_active=is_active)
    await session.execute(stmt)
    await session.commit()

# 5. PESSIMISTIC LOCK (SELECT FOR UPDATE)
async def lock_and_deduct_inventory(session: AsyncSession, sku: str, qty: int) -> Product:
    stmt = select(Product).where(Product.sku == sku).with_for_update()
    result = await session.execute(stmt)
    product = result.scalar_one()
    product.stock -= qty
    await session.commit()
    return product

# 6. DELETE
async def delete_user(session: AsyncSession, user_id: uuid.UUID) -> None:
    stmt = delete(User).where(User.id == user_id)
    await session.execute(stmt)
    await session.commit()
```
