# Project 04: High-Scale E-Commerce Storefront (`04-ecommerce-backend`)

> High-throughput e-commerce backend featuring TypeORM, Redis caching, pessimistic inventory reservation, and Stripe payment webhook synchronization.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-nest-04-ecommerce-backend` |
| **Database** | PostgreSQL 16 (TypeORM) + Redis (Product Catalog Cache) |
| **Payments** | Stripe Checkout + Webhook Signature Verification |
| **Concurrency** | Pessimistic Locking (`SELECT FOR UPDATE`) |
| **Complexity Level** | Advanced Enterprise Reference |

---

## 1. Pessimistic Inventory Reservation Service

```typescript
// src/orders/orders.service.ts
import { Injectable, BadRequestException } from '@nestjs/common';
import { DataSource } from 'typeorm';
import { InventoryEntity } from '../inventory/entities/inventory.entity';
import { OrderEntity, OrderStatus } from './entities/order.entity';

@Injectable()
export class OrdersService {
  constructor(private readonly dataSource: DataSource) {}

  async placeOrder(userId: string, items: Array<{ productId: string; qty: number }>) {
    return this.dataSource.transaction(async (manager) => {
      // 1. Lock inventory rows using pessimistic write lock
      for (const item of items) {
        const inventory = await manager
          .createQueryBuilder(InventoryEntity, 'inv')
          .setLock('pessimistic_write')
          .where('inv.product_id = :id', { id: item.productId })
          .getOne();

        if (!inventory || inventory.stock < item.qty) {
          throw new BadRequestException(`Insufficient stock for product ${item.productId}`);
        }

        inventory.stock -= item.qty;
        await manager.save(inventory);
      }

      // 2. Create pending order
      const order = manager.create(OrderEntity, {
        userId,
        status: OrderStatus.PENDING_PAYMENT,
      });
      return manager.save(order);
    });
  }
}
```
