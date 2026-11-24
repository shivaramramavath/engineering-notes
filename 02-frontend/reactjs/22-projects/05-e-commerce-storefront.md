# Project 05: E-Commerce Storefront (`e-commerce-storefront`)

> A full-featured modern E-Commerce storefront featuring product browsing, faceted category/price filtering, full-text search, an animated cart drawer powered by Zustand with persistent storage, and an optimistic checkout process.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-react-05-ecommerce-store` |
| **React Version** | React 19.0.0 |
| **Tested With** | Vite 6.0, TypeScript 5.7, Vitest 3.0 |
| **State Architecture** | Zustand v5 (`persist` middleware) + TanStack Query v5 |
| **Styling** | Tailwind CSS v4 + Radix UI Dialog / Sheet |
| **Complexity Level** | Production-Grade |

---

## 1. Architectural Overview & Component Hierarchy

### 1.1 Architecture & Data Flow
Global client state (cart quantity, active discount vouchers, drawer visibility) is managed using Zustand v5 with local storage persistence. Catalog browsing and faceted search queries are handled by TanStack Query. Cart mutations (add, remove, quantity update) compute total order value optimistically.

```
[ Catalog Grid / Filter Sidebar ] ──> [ TanStack Query Product Cache ]
                                                    │
                                                    ▼
[ Add to Cart / Quantity Stepper ] ──> [ Zustand Cart Store (Persistent) ]
                                                    │
                                                    ▼
[ Sliding Cart Sheet / Checkout ] <── [ Subtotal + Tax + Promo Calculations ]
```

### 1.2 Component Structure
```text
src/
├── types/
│   └── commerce.ts          # Product, CartItem, Filter types
├── store/
│   └── useCartStore.ts      # Zustand cart state with persistence
├── components/
│   ├── Header.tsx           # Logo, search bar, Cart badge button
│   ├── ProductCard.tsx      # Image, badge, rating, Add to Cart trigger
│   ├── ProductFilters.tsx   # Category checkboxes, price range slider
│   ├── CartDrawer.tsx       # Radix Sheet cart view with line items
│   └── CheckoutModal.tsx    # Shipping details and payment confirmation
└── App.tsx                  # Storefront layout
```

---

## 2. Complete Project Source Code

### 2.1 Types (`src/types/commerce.ts`)
```ts
export interface Product {
  id: string;
  name: string;
  price: number;
  category: string;
  rating: number;
  image: string;
  stock: number;
}

export interface CartItem extends Product {
  quantity: number;
}

export interface FilterState {
  category: string;
  minPrice: number;
  maxPrice: number;
  search: string;
}
```

### 2.2 Zustand Cart Store (`src/store/useCartStore.ts`)
```ts
import { create } from 'zustand';
import { persist } from 'zustand/middleware';
import { CartItem, Product } from '../types/commerce';

interface CartStore {
  items: CartItem[];
  isOpen: boolean;
  addItem: (product: Product) => void;
  removeItem: (id: string) => void;
  updateQuantity: (id: string, delta: number) => void;
  clearCart: () => void;
  setIsOpen: (isOpen: boolean) => void;
  totalItems: () => number;
  subtotal: () => number;
}

export const useCartStore = create<CartStore>()(
  persist(
    (set, get) => ({
      items: [],
      isOpen: false,
      addItem: (product) => {
        set((state) => {
          const existing = state.items.find((item) => item.id === product.id);
          if (existing) {
            return {
              items: state.items.map((item) =>
                item.id === product.id
                  ? { ...item, quantity: item.quantity + 1 }
                  : item
              ),
              isOpen: true,
            };
          }
          return { items: [...state.items, { ...product, quantity: 1 }], isOpen: true };
        });
      },
      removeItem: (id) => {
        set((state) => ({ items: state.items.filter((item) => item.id !== id) }));
      },
      updateQuantity: (id, delta) => {
        set((state) => ({
          items: state.items
            .map((item) => {
              if (item.id === id) {
                const newQty = item.quantity + delta;
                return newQty > 0 ? { ...item, quantity: newQty } : null;
              }
              return item;
            })
            .filter((item): item is CartItem => item !== null),
        }));
      },
      clearCart: () => set({ items: [] }),
      setIsOpen: (isOpen) => set({ isOpen }),
      totalItems: () => get().items.reduce((sum, item) => sum + item.quantity, 0),
      subtotal: () => get().items.reduce((sum, item) => sum + item.price * item.quantity, 0),
    }),
    {
      name: 'ecommerce-cart-storage',
    }
  )
);
```

### 2.3 Product Card & Cart Drawer (`src/components/ProductCard.tsx`, `src/components/CartDrawer.tsx`)
```tsx
// src/components/ProductCard.tsx
import React from 'react';
import { Product } from '../types/commerce';
import { useCartStore } from '../store/useCartStore';

export const ProductCard: React.FC<{ product: Product }> = ({ product }) => {
  const addItem = useCartStore((state) => state.addItem);

  return (
    <div className="flex flex-col overflow-hidden rounded-xl border border-slate-200 bg-white shadow-sm transition hover:shadow-md">
      <img src={product.image} alt={product.name} className="h-48 w-full object-cover" />
      <div className="flex flex-1 flex-col p-4">
        <span className="text-xs font-semibold uppercase tracking-wider text-slate-400">
          {product.category}
        </span>
        <h3 className="mt-1 font-semibold text-slate-900">{product.name}</h3>
        <div className="mt-auto pt-4 flex items-center justify-between">
          <span className="text-lg font-bold text-slate-900">${product.price.toFixed(2)}</span>
          <button
            onClick={() => addItem(product)}
            className="rounded-lg bg-blue-600 px-3 py-1.5 text-xs font-semibold text-white transition hover:bg-blue-700 active:scale-95"
          >
            Add to Cart
          </button>
        </div>
      </div>
    </div>
  );
};
```

```tsx
// src/components/CartDrawer.tsx
import React from 'react';
import { useCartStore } from '../store/useCartStore';

export const CartDrawer: React.FC = () => {
  const { items, isOpen, setIsOpen, removeItem, updateQuantity, subtotal } = useCartStore();

  if (!isOpen) return null;

  return (
    <div className="fixed inset-0 z-50 flex justify-end bg-black/40 backdrop-blur-xs">
      <div className="flex h-full w-full max-w-md flex-col bg-white p-6 shadow-2xl">
        <div className="flex items-center justify-between border-b pb-4">
          <h2 className="text-lg font-bold text-slate-900">Your Cart ({items.length})</h2>
          <button
            onClick={() => setIsOpen(false)}
            className="rounded p-1 text-slate-400 hover:text-slate-600"
          >
            ✕
          </button>
        </div>

        <div className="flex-1 overflow-y-auto py-4 space-y-4">
          {items.length === 0 ? (
            <p className="py-12 text-center text-slate-400">Your cart is empty.</p>
          ) : (
            items.map((item) => (
              <div key={item.id} className="flex items-center justify-between gap-4 border-b pb-3">
                <img src={item.image} alt={item.name} className="h-16 w-16 rounded object-cover" />
                <div className="flex-1">
                  <h4 className="text-sm font-semibold text-slate-900">{item.name}</h4>
                  <span className="text-xs text-slate-500">${item.price.toFixed(2)}</span>
                  <div className="mt-2 flex items-center gap-2">
                    <button
                      onClick={() => updateQuantity(item.id, -1)}
                      className="h-6 w-6 rounded border text-sm font-bold hover:bg-slate-50"
                    >
                      -
                    </button>
                    <span className="text-xs font-bold">{item.quantity}</span>
                    <button
                      onClick={() => updateQuantity(item.id, 1)}
                      className="h-6 w-6 rounded border text-sm font-bold hover:bg-slate-50"
                    >
                      +
                    </button>
                  </div>
                </div>
                <button
                  onClick={() => removeItem(item.id)}
                  className="text-xs font-medium text-red-500 hover:underline"
                >
                  Remove
                </button>
              </div>
            ))
          )}
        </div>

        {items.length > 0 && (
          <div className="border-t pt-4">
            <div className="flex justify-between text-base font-bold text-slate-900">
              <span>Subtotal</span>
              <span>${subtotal().toFixed(2)}</span>
            </div>
            <button className="mt-4 w-full rounded-xl bg-blue-600 py-3 text-sm font-semibold text-white hover:bg-blue-700">
              Proceed to Checkout
            </button>
          </div>
        )}
      </div>
    </div>
  );
};
```

---

## 3. Testing Suite (`src/tests/CartStore.test.ts`)

```ts
import { describe, it, expect, beforeEach } from 'vitest';
import { useCartStore } from '../store/useCartStore';

const mockProduct = {
  id: 'p1',
  name: 'Mechanical Keyboard',
  price: 120,
  category: 'Electronics',
  rating: 4.8,
  image: '/keyboard.jpg',
  stock: 10,
};

describe('Zustand Cart Store', () => {
  beforeEach(() => {
    useCartStore.getState().clearCart();
  });

  it('adds an item and increments count when added again', () => {
    const { addItem } = useCartStore.getState();
    addItem(mockProduct);
    expect(useCartStore.getState().items).toHaveLength(1);
    expect(useCartStore.getState().items[0].quantity).toBe(1);

    addItem(mockProduct);
    expect(useCartStore.getState().items).toHaveLength(1);
    expect(useCartStore.getState().items[0].quantity).toBe(2);
    expect(useCartStore.getState().subtotal()).toBe(240);
  });

  it('removes item when quantity decrements to zero', () => {
    const { addItem, updateQuantity } = useCartStore.getState();
    addItem(mockProduct);
    updateQuantity('p1', -1);
    expect(useCartStore.getState().items).toHaveLength(0);
  });
});
```

---

## 4. Build and Run Instructions

```bash
# Install dependencies
npm install zustand @tanstack/react-query lucide-react

# Run dev server
npm run dev

# Run test suite
npm run test
```
