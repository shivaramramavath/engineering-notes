# Project 11: Distributed Microservices System (`11-microservices-system`)

> Polyglot distributed microservices system: API Gateway communicating with Users, Orders, and Payments services via RabbitMQ message brokers and high-performance gRPC.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-nest-11-microservices-system` |
| **Transporters** | RabbitMQ (Asynchronous Events) + gRPC (Synchronous RPC) |
| **Services** | API Gateway, Users Service, Orders Service, Payments Service |
| **Complexity Level** | Enterprise Distributed Reference |

---

## 1. System Topology & gRPC Protobuf

```protobuf
// proto/orders.proto
syntax = "proto3";

package orders;

service OrdersService {
  rpc CreateOrder (CreateOrderRequest) returns (OrderResponse);
  rpc GetOrderStatus (GetOrderStatusRequest) returns (OrderStatusResponse);
}

message CreateOrderRequest {
  string customerId = 1;
  double amount = 2;
}

message OrderResponse {
  string orderId = 1;
  string status = 2;
}

message GetOrderStatusRequest {
  string orderId = 1;
}

message OrderStatusResponse {
  string status = 1;
}
```
