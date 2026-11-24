# Microservices Transporters Cheat Sheet

```typescript
// Bootstrap Microservice
const app = await NestFactory.createMicroservice<MicroserviceOptions>(AppModule, {
  transport: Transport.RMQ,
  options: {
    urls: ['amqp://localhost:5672'],
    queue: 'orders_queue',
  },
});
await app.listen();

// Handler
@MessagePattern('order.created')
handleOrderCreated(@Payload() data: any) {
  return { status: 'ACK' };
}
```
