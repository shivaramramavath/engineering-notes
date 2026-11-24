# Enterprise Node.js Project Bootstrapping Checklist

> Production-ready initialization checklist for enterprise Node.js 22 LTS / 24+ backend services.

---

## 1. Project Initialization Checklist

### Phase 1: Environment & Module Configuration
- [ ] Initialize repository with `.gitignore` (ignoring `node_modules/`, `.env*`, `dist/`, `coverage/`).
- [ ] Create `.node-version` pinning the Active LTS line (e.g. `22.18.0`).
- [ ] Configure `package.json` with `"type": "module"` and strict engines:
  ```json
  {
    "name": "my-service",
    "version": "1.0.0",
    "type": "module",
    "engines": {
      "node": ">=22.0.0"
    }
  }
  ```
- [ ] Create `.npmrc` enforcing `engine-strict=true` and `save-exact=true`.

### Phase 2: Configuration & Validation Layer
- [ ] Create `.env.example` documenting all required environment variables without real secrets.
- [ ] Implement runtime environment validation using Zod:
  ```typescript
  import { z } from 'zod';

  const envSchema = z.object({
    PORT: z.coerce.number().default(3000),
    DATABASE_URL: z.string().url(),
    NODE_ENV: z.enum(['development', 'production', 'test']).default('development'),
  });

  export const env = envSchema.parse(process.env);
  ```

### Phase 3: Code Quality & Testing
- [ ] Configure ESLint 9 Flat Config (`eslint.config.js`).
- [ ] Set up Prettier or Biome for code formatting.
- [ ] Configure `node:test` or Vitest for testing (`npm test`).
- [ ] Add git pre-commit hooks via Husky or simple shell scripts (`npm test`, `npm run lint`).

### Phase 4: Containerization & CI/CD
- [ ] Create multi-stage production Dockerfile (`FROM node:22-alpine`).
- [ ] Create `docker-compose.yml` for local dependencies (PostgreSQL, Redis).
- [ ] Implement `/health` and `/ready` HTTP probe endpoints.
- [ ] Add graceful shutdown listeners (`SIGTERM`, `SIGINT`).
