# Express 5.0 Migration Guide & Internal Architecture

> Complete guide to migrating from Express 4 to Express 5.0 (GA October 2024): automatic async error handling, `path-to-regexp` v8, removed methods, and performance upgrades.

| Metadata | Details |
| :--- | :--- |
| **Framework Baseline** | Express v5.0.0+ |
| **Node.js Baseline** | Node.js 18+ (Node 22 LTS recommended) |
| **Breaking Changes** | Async error forwarding, `path-to-regexp` v8, `app.del()` removed |

---

## 1. Automatic Promise & Async Error Handling

In Express 4, unhandled promise rejections inside `async` route handlers or middleware would hang the request or crash the process unless explicitly caught in `try/catch` and passed to `next(err)`.

**In Express 5, all rejected promises and thrown errors in async functions are automatically caught and forwarded to error middleware**:

```javascript
// Express 5: No try/catch or express-async-errors wrapper needed!
app.get('/users/:id', async (req, res) => {
  const user = await db.findUser(req.params.id); // If this rejects...
  if (!user) throw new NotFoundError('User not found'); // ...or this throws
  res.json(user);
});

// Automatically forwards to:
app.use((err, req, res, next) => {
  res.status(err.statusCode || 500).json({ error: err.message });
});
```

---

## 2. Breaking Changes & Removed Methods

### A. Removed Methods
- `app.del()` is **removed** $\rightarrow$ use `app.delete()`.
- `req.param(name)` is **removed** $\rightarrow$ access directly via `req.params[name]`, `req.query[name]`, or `req.body[name]`.
- `res.send(body, status)` and `res.json(obj, status)` signatures are **removed** $\rightarrow$ chain status: `res.status(status).json(obj)`.
- `res.redirect('back')` is **removed** $\rightarrow$ use explicit URL or `req.get('Referrer')`.

### B. Routing & Path Matching (`path-to-regexp` v8)
- **Raw Regular Expressions Removed**: `app.get(/^\/users/)` is no longer supported to prevent Regular Expression Denial of Service (ReDoS).
- **Optional Parameters**: Must be enclosed in braces: `app.get('/users/{:id}', ...)` instead of `/users/:id?`.
- **Wildcard Splats**: Wildcards must be explicitly named: `app.get('/files/*filepath', ...)` instead of `app.get('/files/*', ...)`.

### C. Changed Defaults
- `express.urlencoded({ extended: false })` is now the default (simple querystring parser rather than `qs`).
- Dotfiles in `express.static` are ignored by default.
- `req.query` is now a read-only getter.
