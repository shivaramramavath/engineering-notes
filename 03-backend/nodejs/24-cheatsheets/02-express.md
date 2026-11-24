# Cheatsheet 02: Express 5.0 Reference Card

> Production syntax reference for Express 5.0, routing patterns, built-in middleware, async route handling, and error middleware.

---

### 1. Minimal Express 5 Server
```javascript
import express from 'express';

const app = express();

// Built-in Body Parsers
app.use(express.json({ limit: '1mb' }));
app.use(express.urlencoded({ extended: true }));

app.get('/health', (_req, res) => {
  res.status(200).json({ status: 'ok' });
});

app.listen(3000, () => console.log('Listening on port 3000'));
```

---

### 2. Express 5 Native Async Routing
In Express 5, unhandled promise rejections are automatically forwarded to error middleware:
```javascript
app.get('/users/:id', async (req, res) => {
  const user = await db.findUser(req.params.id);
  if (!user) {
    return res.status(404).json({ error: 'User not found' });
  }
  res.json(user);
});
```

---

### 3. Route Parameter Wildcards (`path-to-regexp` v8)
```javascript
// Express 5 named wildcards:
app.get('/files/*name', (req, res) => {
  res.send(`File requested: ${req.params.name}`);
});

// Express 5 optional parameters:
app.get('/users{/:id}', (req, res) => {
  res.send(`User: ${req.params.id || 'all'}`);
});
```

---

### 4. Router Composition
```javascript
// src/routes/user.routes.js
import { Router } from 'express';

export const userRouter = Router();

userRouter.route('/')
  .get(listUsers)
  .post(createUser);

userRouter.route('/:id')
  .get(getUser)
  .delete(deleteUser);

// In app.js:
app.use('/api/v1/users', userRouter);
```

---

### 5. Centralized Error Middleware (4 Arguments)
Must be placed **after all routes**:
```javascript
app.use((err, req, res, next) => {
  console.error('[Error Pipeline]', err);

  const status = err.statusCode || 500;
  res.status(status).json({
    type: `https://api.example.com/errors/${status}`,
    title: err.name || 'Internal Server Error',
    status,
    detail: err.message,
  });
});
```

---

### 6. Common Request & Response Methods

| Method | Description |
| :--- | :--- |
| `req.params` | Named URL route parameters (e.g. `req.params.id`). |
| `req.query` | Parsed URL query string parameters (e.g. `req.query.page`). |
| `req.body` | Parsed request payload (populated by `express.json()`). |
| `req.headers` | Inbound request headers dictionary. |
| `res.status(code)` | Set HTTP status code (chainable). |
| `res.json(obj)` | Serialize object to JSON and send with `application/json`. |
| `res.sendStatus(204)` | Set status and send empty response with default status text. |
| `res.set(header, val)`| Set outbound HTTP header. |
