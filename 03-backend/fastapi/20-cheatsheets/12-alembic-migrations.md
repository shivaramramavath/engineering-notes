# Quick Reference: Alembic Async Migrations Cheatsheet

## Setup Async Alembic

```bash
# Initialize alembic with the async template
alembic init -t async migrations
```

---

## `migrations/env.py` Configuration for Async SQLAlchemy

```python
import asyncio
from logging.config import fileConfig
from alembic import context
from sqlalchemy.ext.asyncio import create_async_engine

# Import application DeclarativeBase and Models
from src.config import get_settings
from src.domain.models import Base

config = context.config
if config.config_file_name is not None:
    fileConfig(config.config_file_name)

target_metadata = Base.metadata
settings = get_settings()

def run_migrations_offline() -> None:
    context.configure(
        url=settings.DATABASE_URL,
        target_metadata=target_metadata,
        literal_binds=True,
        dialect_opts={"paramstyle": "named"},
    )
    with context.begin_transaction():
        context.run_migrations()

def do_run_migrations(connection):
    context.configure(connection=connection, target_metadata=target_metadata)
    with context.begin_transaction():
        context.run_migrations()

async def run_async_migrations() -> None:
    connectable = create_async_engine(settings.DATABASE_URL)
    async with connectable.connect() as connection:
        await connection.run_sync(do_run_migrations)
    await connectable.dispose()

def run_migrations_online() -> None:
    asyncio.run(run_async_migrations())

if context.is_offline_mode():
    run_migrations_offline()
else:
    run_migrations_online()
```

---

## Essential CLI Commands

```bash
# Generate auto-detected migration from DeclarativeBase changes
alembic revision --autogenerate -m "add_users_table"

# Apply all pending migrations to the latest revision
alembic upgrade head

# Rollback the single most recent migration
alembic downgrade -1

# Rollback to baseline (empty DB)
alembic downgrade base

# View current database revision
alembic current

# View complete migration history
alembic history --verbose
```
