# Cheatsheet 06: MongoDB & Mongoose Reference Card

> Production syntax reference for MongoDB and Mongoose in Node.js 22 LTS, schema design, compound indexing, aggregation pipelines, and atomic transactions.

---

### 1. Connection with Connection Pooling
```javascript
import mongoose from 'mongoose';

await mongoose.connect(process.env.MONGODB_URI, {
  maxPoolSize: 50,              // Maintain up to 50 socket connections
  minPoolSize: 10,
  serverSelectionTimeoutMS: 5000,// Timeout if primary election takes > 5s
  socketTimeoutMS: 45000,
});
```

---

### 2. Schema Definition & Indexing
```javascript
import { Schema, model } from 'mongoose';

const userSchema = new Schema({
  email: { type: String, required: true, unique: true, lowercase: true, trim: true },
  name: { type: String, required: true },
  role: { type: String, enum: ['USER', 'ADMIN'], default: 'USER' },
  metadata: { type: Map, of: String },
  createdAt: { type: Date, default: Date.now },
}, {
  timestamps: true, // Automatically manages createdAt and updatedAt
  optimisticConcurrency: true, // Enables __v version checking to prevent overwrites
});

// Indexes
userSchema.index({ email: 1 });
userSchema.index({ role: 1, createdAt: -1 }); // Compound index
userSchema.index({ createdAt: 1 }, { expireAfterSeconds: 86400 * 30 }); // TTL index (auto-delete after 30 days)

export const User = model('User', userSchema);
```

---

### 3. Essential CRUD Operations
```javascript
// Create
const user = await User.create({ email: 'ada@example.com', name: 'Ada' });

// Read (with Lean for 3x faster read performance when Mongoose methods unneeded)
const user = await User.findOne({ email }).lean();
const users = await User.find({ role: 'ADMIN' }).select('name email').limit(20).lean();

// Atomic Update
const updated = await User.findOneAndUpdate(
  { _id: userId, role: 'USER' },
  { $set: { role: 'ADMIN' }, $inc: { loginAttempts: 1 } },
  { new: true, runValidators: true }
);

// Delete
await User.deleteOne({ _id: userId });
```

---

### 4. Aggregation Pipeline
```javascript
// Calculate total revenue per category
const results = await Order.aggregate([
  { $match: { status: 'PAID' } },
  { $unwind: '$items' },
  {
    $group: {
      _id: '$items.category',
      totalRevenue: { $sum: { $multiply: ['$items.price', '$items.quantity'] } },
      orderCount: { $sum: 1 },
    },
  },
  { $sort: { totalRevenue: -1 } },
  { $limit: 10 },
]);
```

---

### 5. Multi-Document ACID Transactions
```javascript
const session = await mongoose.startSession();
session.startTransaction();

try {
  await Account.updateOne({ _id: fromId }, { $inc: { balance: -amount } }, { session });
  await Account.updateOne({ _id: toId }, { $inc: { balance: amount } }, { session });

  await session.commitTransaction();
} catch (error) {
  await session.abortTransaction();
  throw error;
} finally {
  session.endSession();
}
```
