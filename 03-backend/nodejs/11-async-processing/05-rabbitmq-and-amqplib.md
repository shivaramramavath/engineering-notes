# RabbitMQ & AMQP with Node.js (`amqplib`)

> Production message queuing with RabbitMQ: Exchanges (Direct, Topic, Fanout), durable queues, publisher confirms, and consumer acknowledgments.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Client** | `amqplib` (Channel-based API) |
| **Broker** | RabbitMQ 3.13+ |

---

## 1. AMQP Concepts: Exchanges, Queues & Bindings

Unlike Redis queues where producers push directly to a queue name:
- **Producers**: Publish messages to an **Exchange**, accompanied by a **Routing Key**.
- **Exchange Types**:
  - `direct`: Routes to queues whose binding key matches the routing key exactly.
  - `topic`: Pattern matching with wildcards (`order.*`, `order.#`).
  - `fanout`: Broadcasts messages to all bound queues simultaneously.
- **Queues**: Buffer messages until consumed by worker processes.

---

## 2. Producer with Publisher Confirms (`producer.js`)

```javascript
import amqp from 'amqplib';

export async function createProducer() {
  const connection = await amqp.connect(process.env.RABBITMQ_URL || 'amqp://localhost');
  // Create a confirm channel to guarantee message arrival
  const channel = await connection.createConfirmChannel();

  const exchangeName = 'order_events';
  await channel.assertExchange(exchangeName, 'topic', { durable: true });

  return {
    publishOrderEvent: async (routingKey, eventData) => {
      const payload = Buffer.from(JSON.stringify(eventData));

      return new Promise((resolve, reject) => {
        channel.publish(
          exchangeName,
          routingKey,
          payload,
          { persistent: true }, // Persist to disk
          (err) => {
            if (err) reject(err);
            else resolve(true);
          }
        );
      });
    },
  };
}
```

---

## 3. Consumer with Manual Acknowledgments (`consumer.js`)

```javascript
import amqp from 'amqplib';

export async function startConsumer() {
  const connection = await amqp.connect(process.env.RABBITMQ_URL || 'amqp://localhost');
  const channel = await connection.createChannel();

  // Prefetch: process max 10 messages concurrently per worker
  await channel.prefetch(10);

  const queueName = 'order_processing_queue';
  await channel.assertQueue(queueName, { durable: true });
  await channel.bindQueue(queueName, 'order_events', 'order.created');

  channel.consume(queueName, async (msg) => {
    if (!msg) return;

    try {
      const content = JSON.parse(msg.content.toString());
      console.log('Processing message:', content);

      // Acknowledge message upon successful processing
      channel.ack(msg);
    } catch (err) {
      console.error('Job failed:', err);
      // Reject and requeue (or route to Dead Letter Exchange)
      channel.nack(msg, false, false); // Do not requeue; send to DLQ
    }
  });
}
```
