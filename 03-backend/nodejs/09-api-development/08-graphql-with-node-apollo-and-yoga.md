# GraphQL in Node.js: Apollo Server & GraphQL Yoga

> Building scalable, production GraphQL APIs in Node.js using GraphQL Yoga, schema-first design, code-first Pothos, and N+1 DataLoader optimization.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Servers** | GraphQL Yoga v5+, Apollo Server v4+ |
| **Patterns** | DataLoader batching, Schema vs Code-First (Pothos) |

---

## 1. Setting up GraphQL Yoga (Web Standards Engine)

GraphQL Yoga is built on standard `Request` and `Response` interfaces, running on Express, Fastify, or native Node.js HTTP:

```javascript
import { createServer } from 'node:http';
import { createYoga, createSchema } from 'graphql-yoga';

const yoga = createYoga({
  schema: createSchema({
    typeDefs: /* GraphQL */ `
      type Query {
        user(id: ID!): User
        users: [User!]!
      }
      type User {
        id: ID!
        name: String!
        email: String!
      }
    `,
    resolvers: {
      Query: {
        user: async (_, { id }, ctx) => ctx.db.users.findById(id),
        users: async (_, __, ctx) => ctx.db.users.findAll(),
      },
    },
  }),
  context: ({ request }) => ({
    // Ambient context per request
    authHeader: request.headers.get('authorization'),
  }),
});

const server = createServer(yoga);
server.listen(4000, () => {
  console.log('GraphQL Yoga running at http://localhost:4000/graphql');
});
```

---

## 2. Preventing Query Depth Attacks & Denial of Service

Malicious clients can send recursively nested queries that exhaust server CPU and memory:
```graphql
query {
  user {
    friends {
      friends {
        friends {
          friends { id }
        }
      }
    }
  }
}
```
**Defense**: Configure `graphql-depth-limit` and query complexity validation plugins in Yoga/Apollo to reject queries deeper than 5 levels.
