# Coding Challenges — Senior Interview Questions & Deep Dives

> Machine coding rounds and algorithm challenges in React: Building a custom `useVirtualizer`, an accessible `Autocomplete/Combobox`, a `useUndoRedo` history stack, and an `AsyncQueue`.

---

## Challenge 1: Implement an Accessible Autocomplete (Combobox) from Scratch

### Requirements
1. Debounced search input.
2. Arrow Up / Arrow Down keyboard selection with active item highlight.
3. Escape to close, Enter to select.
4. Correct WAI-ARIA `role="combobox"`, `aria-expanded`, and `aria-activedescendant`.

### Solution Implementation
```tsx
import React, { useState, useRef, useEffect } from 'react';

interface ComboboxProps<T> {
  items: T[];
  filterFn: (query: string, item: T) => boolean;
  getItemLabel: (item: T) => string;
  onSelect: (item: T) => void;
}

export function AccessibleCombobox<T>({
  items,
  filterFn,
  getItemLabel,
  onSelect,
}: ComboboxProps<T>) {
  const [query, setQuery] = useState('');
  const [isOpen, setIsOpen] = useState(false);
  const [highlightedIndex, setHighlightedIndex] = useState(-1);

  const filteredItems = items.filter((item) => filterFn(query, item));
  const listboxId = 'combobox-listbox-items';

  const handleKeyDown = (e: React.KeyboardEvent) => {
    if (!isOpen) {
      if (e.key === 'ArrowDown' || e.key === 'ArrowUp') {
        setIsOpen(true);
        setHighlightedIndex(0);
      }
      return;
    }

    switch (e.key) {
      case 'ArrowDown':
        e.preventDefault();
        setHighlightedIndex((prev) => (prev + 1) % filteredItems.length);
        break;
      case 'ArrowUp':
        e.preventDefault();
        setHighlightedIndex((prev) => (prev - 1 + filteredItems.length) % filteredItems.length);
        break;
      case 'Enter':
        e.preventDefault();
        if (highlightedIndex >= 0 && filteredItems[highlightedIndex]) {
          onSelect(filteredItems[highlightedIndex]);
          setIsOpen(false);
        }
        break;
      case 'Escape':
        setIsOpen(false);
        break;
    }
  };

  return (
    <div className="relative w-72">
      <input
        type="text"
        role="combobox"
        aria-expanded={isOpen}
        aria-controls={listboxId}
        aria-autocomplete="list"
        aria-activedescendant={
          highlightedIndex >= 0 ? `item-${highlightedIndex}` : undefined
        }
        value={query}
        onChange={(e) => {
          setQuery(e.target.value);
          setIsOpen(true);
          setHighlightedIndex(0);
        }}
        onFocus={() => setIsOpen(true)}
        onKeyDown={handleKeyDown}
        className="w-full rounded-lg border border-slate-300 p-2 text-sm focus:outline-blue-500"
        placeholder="Type to search..."
      />

      {isOpen && filteredItems.length > 0 && (
        <ul
          id={listboxId}
          role="listbox"
          className="absolute z-10 mt-1 max-h-60 w-full overflow-y-auto rounded-lg border border-slate-200 bg-white py-1 shadow-lg"
        >
          {filteredItems.map((item, idx) => (
            <li
              key={idx}
              id={`item-${idx}`}
              role="option"
              aria-selected={highlightedIndex === idx}
              onClick={() => {
                onSelect(item);
                setIsOpen(false);
              }}
              className={`cursor-pointer px-3 py-2 text-sm ${
                highlightedIndex === idx ? 'bg-blue-600 text-white' : 'text-slate-800 hover:bg-slate-50'
              }`}
            >
              {getItemLabel(item)}
            </li>
          ))}
        </ul>
      )}
    </div>
  );
}
```

---

## Challenge 2: Implement a `useUndoRedo` History Hook

```ts
import { useState, useCallback } from 'react';

export function useUndoRedo<T>(initialPresent: T) {
  const [past, setPast] = useState<T[]>([]);
  const [present, setPresent] = useState<T>(initialPresent);
  const [future, setFuture] = useState<T[]>([]);

  const canUndo = past.length > 0;
  const canRedo = future.length > 0;

  const undo = useCallback(() => {
    if (!canUndo) return;
    const previous = past[past.length - 1];
    const newPast = past.slice(0, past.length - 1);

    setPast(newPast);
    setFuture([present, ...future]);
    setPresent(previous);
  }, [canUndo, past, present, future]);

  const redo = useCallback(() => {
    if (!canRedo) return;
    const next = future[0];
    const newFuture = future.slice(1);

    setPast([...past, present]);
    setPresent(next);
    setFuture(newFuture);
  }, [canRedo, past, present, future]);

  const set = useCallback(
    (newPresent: T) => {
      if (newPresent === present) return;
      setPast((prev) => [...prev, present]);
      setPresent(newPresent);
      setFuture([]); // Clear future on new mutation
    },
    [present]
  );

  return { state: present, set, undo, redo, canUndo, canRedo };
}
```
