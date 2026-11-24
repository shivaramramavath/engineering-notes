# Module 02: Pydantic v2 Deep Dive

Welcome to **Module 02: Pydantic v2 Deep Dive**. This module details the modern data validation and serialization engine powering FastAPI, from the Rust core to computed fields and environment configuration.

---

## Module Overview

| File | Title | Description |
| :--- | :--- | :--- |
| [01-models-and-fields.md](./01-models-and-fields.md) | **Models & Fields** | BaseModel, Field configuration, constraints, and defaults. |
| [02-validation-and-coercion-modes.md](./02-validation-and-coercion-modes.md) | **Validation Modes** | Strict vs Lax validation, type coercion rules in v2. |
| [03-validators-and-model-validators.md](./03-validators-and-model-validators.md) | **Field & Model Validators** | `@field_validator` and `@model_validator(mode='before'|'after')`. |
| [04-computed-fields-and-serializers.md](./04-computed-fields-and-serializers.md) | **Computed Fields & Serializers** | `@computed_field`, `@field_serializer`, and `@model_serializer`. |
| [05-field-and-model-config.md](./05-field-and-model-config.md) | **ConfigDict & Model Config** | Modern `model_config = ConfigDict(...)` options. |
| [06-aliases-and-naming-conventions.md](./06-aliases-and-naming-conventions.md) | **Aliases & CamelCase** | `validation_alias`, `serialization_alias`, and camelCase generators. |
| [07-nested-models-and-generics.md](./07-nested-models-and-generics.md) | **Nested Models & Generics** | Reusable generic response wrappers (`ResponseEnvelope[T]`). |
| [08-discriminated-unions.md](./08-discriminated-unions.md) | **Discriminated Unions** | Polymorphic payload parsing using discriminator keys. |
| [09-custom-types-and-annotated.md](./09-custom-types-and-annotated.md) | **Custom Types & Validators** | Reusable custom types using `typing.Annotated` and metadata. |
| [10-json-schema-generation.md](./10-json-schema-generation.md) | **JSON Schema Generation** | Customizing JSON Schema generation for OpenAPI documentation. |
| [11-pydantic-settings-and-env-config.md](./11-pydantic-settings-and-env-config.md) | **Settings & Environment** | Centralized application configuration with `pydantic-settings`. |
| [12-performance-and-the-rust-core.md](./12-performance-and-the-rust-core.md) | **Rust Core & Performance** | Memory profiling, benchmark gains, and `pydantic-core`. |
| [13-migrating-from-pydantic-v1.md](./13-migrating-from-pydantic-v1.md) | **Migrating from v1 to v2** | Comprehensive upgrade guide, automated bump2version scripts. |
| [14-typeadapter-and-validating-non-models.md](./14-typeadapter-and-validating-non-models.md) | **TypeAdapter & Non-Models** | Validating raw primitives, lists, and dicts without BaseModels. |
| [15-dataclasses-and-typeddict.md](./15-dataclasses-and-typeddict.md) | **Dataclasses & TypedDict** | Integrating standard library dataclasses with Pydantic validation. |
