# Troubleshooting & Debugging Commands

| Issue | Cause | Fix |
| :--- | :--- | :--- |
| **Nest can't resolve dependencies (?)** | Missing in providers or imports | Add missing module to `imports` or export provider |
| **Circular dependency undefined** | Circular file imports | Use `forwardRef(() => Module)` |
| **Port EADDRINUSE** | Existing process on port 3000 | Kill process with `taskkill /PID <PID> /F` |
| **Wildcard route crash (Express 5)** | `@Get('*')` syntax deprecated | Change to `@Get('*path')` |
