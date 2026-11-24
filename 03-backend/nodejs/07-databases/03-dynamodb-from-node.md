# Amazon DynamoDB with Node.js & AWS SDK v3

> Single-digit millisecond NoSQL key-value and document persistence using `@aws-sdk/client-dynamodb` and `@aws-sdk/lib-dynamodb`.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **SDK** | AWS SDK for JavaScript v3 (`@aws-sdk/lib-dynamodb`) |
| **Database** | Amazon DynamoDB |

---

## 1. Using the DynamoDB Document Client

The low-level `@aws-sdk/client-dynamodb` requires verbose attribute typing (`{ S: "user_123" }`, `{ N: "42" }`).
The `@aws-sdk/lib-dynamodb` **Document Client** automatically marshals and unmarshals standard JavaScript objects:

```javascript
import { DynamoDBClient } from '@aws-sdk/client-dynamodb';
import { DynamoDBDocumentClient, GetCommand, PutCommand } from '@aws-sdk/lib-dynamodb';

const baseClient = new DynamoDBClient({ region: 'us-east-1' });
export const docClient = DynamoDBDocumentClient.from(baseClient, {
  marshallOptions: { removeUndefinedValues: true },
});

// Put item
export async function createSession(userId, token) {
  const command = new PutCommand({
    TableName: 'Sessions',
    Item: {
      userId,
      token,
      ttl: Math.floor(Date.now() / 1000) + (7 * 24 * 60 * 60), // 7 days TTL
    },
  });
  await docClient.send(command);
}

// Get item
export async function getSession(userId) {
  const command = new GetCommand({
    TableName: 'Sessions',
    Key: { userId },
  });
  const response = await docClient.send(command);
  return response.Item || null;
}
```
