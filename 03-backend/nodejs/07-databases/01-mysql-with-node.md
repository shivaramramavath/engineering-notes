# MySQL & MariaDB with Node.js (`mysql2`)

> Connecting Node.js to MySQL 8+ and MariaDB using `mysql2/promise`, connection pooling, and prepared statements.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Driver** | `mysql2` (Promise API) |
| **Database** | MySQL 8.x / MariaDB |

---

## 1. Setting up Connection Pools with `mysql2`

`mysql2` is the modern, high-performance MySQL driver for Node.js with built-in promise support:

```javascript
import mysql from 'mysql2/promise';

export const pool = mysql.createPool({
  host: process.env.MYSQL_HOST || 'localhost',
  user: process.env.MYSQL_USER || 'app',
  password: process.env.MYSQL_PASSWORD,
  database: process.env.MYSQL_DATABASE || 'production',
  waitForConnections: true,
  connectionLimit: 15,
  queueLimit: 0,
  enableKeepAlive: true,
  keepAliveInitialDelay: 0,
});

// Prepared query execution
export async function findUserById(id) {
  // Uses '?' parameter syntax
  const [rows] = await pool.execute('SELECT * FROM users WHERE id = ?', [id]);
  return rows[0] || null;
}
```

---

## 2. Managing Transactions in MySQL

```javascript
export async function executeOrderTransaction(orderData) {
  const connection = await pool.getConnection();
  try {
    await connection.beginTransaction();

    const [orderResult] = await connection.execute(
      'INSERT INTO orders (user_id, total) VALUES (?, ?)',
      [orderData.userId, orderData.total]
    );

    await connection.execute(
      'UPDATE inventory SET stock = stock - ? WHERE item_id = ?',
      [orderData.quantity, orderData.itemId]
    );

    await connection.commit();
    return { orderId: orderResult.insertId };
  } catch (err) {
    await connection.rollback();
    throw err;
  } finally {
    connection.release();
  }
}
```
