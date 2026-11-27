# Legacy Code & Characterisation Tests

| Field | Value |
| :--- | :--- |
| **Category** | Refactoring (Legacy Safety) |
| **Origin** | Michael Feathers (*Working Effectively with Legacy Code*, 2004) |

> **TL;DR**: *"Legacy code is simply code without tests."* Before refactoring untested legacy code, write **Characterisation Tests** (pinning what the code *actually does today*, quirks and all) and introduce **Seams** so you can break hard dependencies safely.

## Characterisation Test Workflow
1. Call the legacy function with representative inputs in a test.
2. Assert a dummy value (`assert result == "???"`) to see what it actually outputs.
3. Lock the actual output (**Golden Master / Approval Testing**) into the test.
4. Now refactor safely—any accidental behavior change fails the Characterization Test immediately!

## Key Seams for Untested Code
- **Extract and Override Call**: Subclass the legacy class in a test to stub out a static `Database.save()` call.
- **Parameterize Constructor / Method**: Pass `clock` or `repo` with a default value preserving existing callers.
