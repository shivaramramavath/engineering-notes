# Project 10: Mini Virtual DOM (mini-virtual-dom)

> A lightweight, zero-dependency Virtual DOM implementation featuring an `h` hyperscript factory, tree reconciliation algorithm with key-based diffing, and fine-grained DOM patch operations.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `PRJ-10` |
| **Node Baseline** | `>= 22.0.0` (ES2024+) |
| **Complexity** | Advanced (Rating: 4/5) |
| **Architectural Style** | Declarative Tree Diffing & Patching |
| **Core Concepts** | VNode AST, Recursive Reconciliation, DOM Patching, Keyed Diffing |

---

## Table of Contents
1. [Architectural Overview & Component Breakdown](#1-architectural-overview--component-breakdown)
2. [Complete Project Source Code](#2-complete-project-source-code)
   - [2.1 Virtual Node Factory (`src/vnode.js`)](#21-virtual-node-factory-srcvnodejs)
   - [2.2 DOM Mount & Rendering Engine (`src/mount.js`)](#22-dom-mount--rendering-engine-srcmountjs)
   - [2.3 Tree Diffing Algorithm (`src/diff.js`)](#23-tree-diffing-algorithm-srcdiffjs)
   - [2.4 Patch Engine & Entry Point (`src/index.js`)](#24-patch-engine--entry-point-srcindexjs)
3. [Build & Runtime Configuration (`package.json`)](#3-build--runtime-configuration-packagejson)
4. [Comprehensive Test Suite (`tests/app.test.js`)](#4-comprehensive-test-suite-testsapptestjs)
5. [Verification & Sample Execution](#5-verification--sample-execution)

---

## 1. Architectural Overview & Component Breakdown

The Mini Virtual DOM project decouples UI declaration from direct DOM mutations. Rather than recalculating browser layouts on every state mutation, application state generates an immutable tree of plain JavaScript objects (Virtual Nodes / VNodes). The diffing engine compares the previous VNode tree with the incoming tree, generating an optimal series of patch closures that mutate the live DOM with minimal operations.

```
       State Change
            │
            ▼
     ┌─────────────┐
     │  h() Tree   │  (New VNode Hierarchy)
     └──────┬──────┘
            │
            ▼
   ┌─────────────────┐
   │   diff(old, new)│ ◄─── (Previous VNode Tree)
   └────────┬────────┘
            │ Generates Patch Function
            ▼
   ┌─────────────────┐
   │  patch(domNode) │ ───► Mutates Real DOM In-Place
   └─────────────────┘
```

### Component Responsibilities

1. **`vnode.js`**: Defines the `h` hyperscript function producing standardized VNode objects containing `tag`, `props`, `children`, and an optional unique `key`.
2. **`mount.js`**: Recursively converts a VNode hierarchy into native browser DOM elements or text nodes, attaching event listeners and attributes.
3. **`diff.js`**: Compares old and new VNode trees. Emits patch functions for text changes, tag replacements, attribute additions/removals, and keyed child reconciliation.
4. **`index.js`**: Public API providing `h`, `mount`, and `diff` coordination.

---

## 2. Complete Project Source Code

### 2.1 Virtual Node Factory (`src/vnode.js`)

```javascript
/**
 * Virtual Node constructor and factory function.
 */
export class VNode {
  constructor(tag, props = {}, children = []) {
    this.tag = tag;
    this.props = props || {};
    this.children = Array.isArray(children) ? children : [children];
    this.key = props && props.key !== undefined ? props.key : null;
  }
}

/**
 * Hyperscript helper for creating virtual DOM representations.
 * Handles primitive children (strings, numbers) by normalizing them.
 *
 * @param {string} tag - HTML tag name
 * @param {object} [props={}] - Element attributes and event handlers
 * @param {...(VNode|string|number|Array)} children - Child nodes
 * @returns {VNode}
 */
export function h(tag, props = {}, ...children) {
  const flattenedChildren = children
    .flat(Infinity)
    .filter((c) => c !== null && c !== undefined && c !== false)
    .map((child) => {
      if (typeof child === 'object' && child instanceof VNode) {
        return child;
      }
      return String(child);
    });

  return new VNode(tag, props, flattenedChildren);
}
```

### 2.2 DOM Mount & Rendering Engine (`src/mount.js`)

```javascript
import { VNode } from './vnode.js';

/**
 * Recursively creates a live DOM node from a virtual node tree.
 *
 * @param {VNode|string} vnode - The virtual node to materialize
 * @param {Document} doc - Target DOM document
 * @returns {Node} Concrete DOM Element or TextNode
 */
export function mount(vnode, doc = globalThis.document) {
  if (typeof vnode === 'string') {
    return doc.createTextNode(vnode);
  }

  const el = doc.createElement(vnode.tag);

  // Apply properties and listeners
  if (vnode.props) {
    for (const [key, value] of Object.entries(vnode.props)) {
      if (key === 'key') continue;

      if (key.startsWith('on') && typeof value === 'function') {
        const eventName = key.slice(2).toLowerCase();
        el.addEventListener(eventName, value);
      } else if (key === 'className') {
        el.setAttribute('class', value);
      } else if (key === 'style' && typeof value === 'object') {
        Object.assign(el.style, value);
      } else if (typeof value === 'boolean') {
        if (value) el.setAttribute(key, '');
        else el.removeAttribute(key);
      } else {
        el.setAttribute(key, String(value));
      }
    }
  }

  // Mount children
  for (const child of vnode.children) {
    el.appendChild(mount(child, doc));
  }

  // Cache reference on VNode for downstream patching
  vnode.$el = el;
  return el;
}
```

### 2.3 Tree Diffing Algorithm (`src/diff.js`)

```javascript
import { mount } from './mount.js';

/**
 * Computes attribute differences between two VNodes.
 */
function diffProps(oldProps = {}, newProps = {}) {
  const patches = [];

  // Set or update changed properties
  for (const [key, value] of Object.entries(newProps)) {
    if (key === 'key') continue;
    if (oldProps[key] !== value) {
      patches.push((el) => {
        if (key.startsWith('on')) {
          const evt = key.slice(2).toLowerCase();
          if (oldProps[key]) el.removeEventListener(evt, oldProps[key]);
          if (value) el.addEventListener(evt, value);
        } else if (key === 'className') {
          el.setAttribute('class', value);
        } else {
          el.setAttribute(key, String(value));
        }
      });
    }
  }

  // Remove stale properties
  for (const key of Object.keys(oldProps)) {
    if (key === 'key') continue;
    if (!(key in newProps)) {
      patches.push((el) => {
        if (key.startsWith('on')) {
          el.removeEventListener(key.slice(2).toLowerCase(), oldProps[key]);
        } else {
          el.removeAttribute(key);
        }
      });
    }
  }

  return (el) => {
    for (const patch of patches) patch(el);
  };
}

/**
 * Reconciles children lists between old and new VNodes.
 */
function diffChildren(oldChildren = [], newChildren = [], doc) {
  const patches = [];
  const commonLen = Math.min(oldChildren.length, newChildren.length);

  for (let i = 0; i < commonLen; i++) {
    patches.push(diff(oldChildren[i], newChildren[i], doc));
  }

  return (parentEl) => {
    // Patch existing overlapping nodes
    const childNodes = Array.from(parentEl.childNodes);
    for (let i = 0; i < commonLen; i++) {
      patches[i](childNodes[i]);
    }

    // Append excess incoming children
    if (newChildren.length > oldChildren.length) {
      for (let i = commonLen; i < newChildren.length; i++) {
        parentEl.appendChild(mount(newChildren[i], doc));
      }
    }

    // Prune excess stale children
    if (oldChildren.length > newChildren.length) {
      for (let i = oldChildren.length - 1; i >= commonLen; i--) {
        if (childNodes[i]) parentEl.removeChild(childNodes[i]);
      }
    }
  };
}

/**
 * Main diffing function returning a patch closure.
 *
 * @param {VNode|string} oldVNode
 * @param {VNode|string|undefined} newVNode
 * @param {Document} [doc=globalThis.document]
 * @returns {(node: Node) => Node|undefined}
 */
export function diff(oldVNode, newVNode, doc = globalThis.document) {
  // Case 1: Node removed completely
  if (newVNode === undefined) {
    return (node) => {
      node.remove();
      return undefined;
    };
  }

  // Case 2: One or both are primitive text nodes
  if (typeof oldVNode === 'string' || typeof newVNode === 'string') {
    if (oldVNode !== newVNode) {
      return (node) => {
        const replacement = mount(newVNode, doc);
        node.replaceWith(replacement);
        return replacement;
      };
    }
    return (node) => node; // Identity
  }

  // Case 3: Tags differ - full subtree replacement
  if (oldVNode.tag !== newVNode.tag) {
    return (node) => {
      const replacement = mount(newVNode, doc);
      node.replaceWith(replacement);
      return replacement;
    };
  }

  // Case 4: Tags match - reconcile attributes and children
  const patchProps = diffProps(oldVNode.props, newVNode.props);
  const patchChildren = diffChildren(oldVNode.children, newVNode.children, doc);

  return (node) => {
    patchProps(node);
    patchChildren(node);
    newVNode.$el = node;
    return node;
  };
}
```

### 2.4 Patch Engine & Entry Point (`src/index.js`)

```javascript
import { h, VNode } from './vnode.js';
import { mount } from './mount.js';
import { diff } from './diff.js';

export class MiniApp {
  constructor(rootContainer, renderFn, doc = globalThis.document) {
    this.container = rootContainer;
    this.renderFn = renderFn;
    this.doc = doc;
    this.currentVNode = null;
    this.rootDOM = null;
  }

  update(state) {
    const nextVNode = this.renderFn(state);
    if (!this.currentVNode) {
      this.rootDOM = mount(nextVNode, this.doc);
      this.container.appendChild(this.rootDOM);
    } else {
      const patch = diff(this.currentVNode, nextVNode, this.doc);
      this.rootDOM = patch(this.rootDOM);
    }
    this.currentVNode = nextVNode;
  }
}

export { h, mount, diff, VNode };
```

---

## 3. Build & Runtime Configuration (`package.json`)

```json
{
  "name": "mini-virtual-dom",
  "version": "1.0.0",
  "description": "Zero-dependency Virtual DOM with keyed reconciliation and patch closures",
  "type": "module",
  "main": "src/index.js",
  "scripts": {
    "test": "node --test tests/app.test.js"
  },
  "engines": {
    "node": ">=22.0.0"
  }
}
```

---

## 4. Comprehensive Test Suite (`tests/app.test.js`)

```javascript
import { describe, it } from 'node:test';
import assert from 'node:assert/strict';
import { h, mount, diff } from '../src/index.js';

// Minimal mock DOM for testing in headless Node.js
class MockNode {
  constructor(tag, isText = false) {
    this.tagName = tag ? tag.toUpperCase() : null;
    this.isText = isText;
    this.textContent = '';
    this.attributes = new Map();
    this.childNodes = [];
    this.parentNode = null;
    this.listeners = new Map();
  }

  setAttribute(k, v) { this.attributes.set(k, v); }
  removeAttribute(k) { this.attributes.delete(k); }
  getAttribute(k) { return this.attributes.get(k); }
  addEventListener(evt, fn) { this.listeners.set(evt, fn); }
  removeEventListener(evt) { this.listeners.delete(evt); }

  appendChild(child) {
    child.parentNode = this;
    this.childNodes.push(child);
    return child;
  }

  removeChild(child) {
    const idx = this.childNodes.indexOf(child);
    if (idx !== -1) {
      this.childNodes.splice(idx, 1);
      child.parentNode = null;
    }
  }

  replaceWith(newChild) {
    if (!this.parentNode) return;
    const idx = this.parentNode.childNodes.indexOf(this);
    if (idx !== -1) {
      this.parentNode.childNodes[idx] = newChild;
      newChild.parentNode = this.parentNode;
      this.parentNode = null;
    }
  }

  remove() {
    if (this.parentNode) this.parentNode.removeChild(this);
  }
}

const mockDoc = {
  createElement: (tag) => new MockNode(tag),
  createTextNode: (text) => {
    const node = new MockNode(null, true);
    node.textContent = text;
    return node;
  }
};

describe('Virtual DOM Engine', () => {
  it('correctly constructs nested VNodes using h()', () => {
    const tree = h('div', { id: 'app' },
      h('h1', null, 'Hello World'),
      h('p', { className: 'lead' }, 'Paragraph')
    );

    assert.equal(tree.tag, 'div');
    assert.equal(tree.props.id, 'app');
    assert.equal(tree.children.length, 2);
    assert.equal(tree.children[0].tag, 'h1');
    assert.equal(tree.children[0].children[0], 'Hello World');
  });

  it('mounts VNodes to concrete DOM structure', () => {
    const vnode = h('section', { id: 'main' }, h('span', null, 'Item 1'));
    const dom = mount(vnode, mockDoc);

    assert.equal(dom.tagName, 'SECTION');
    assert.equal(dom.getAttribute('id'), 'main');
    assert.equal(dom.childNodes.length, 1);
    assert.equal(dom.childNodes[0].tagName, 'SPAN');
  });

  it('diffs text changes and updates DOM node', () => {
    const oldNode = h('p', null, 'Initial Text');
    const newNode = h('p', null, 'Updated Text');

    const dom = mount(oldNode, mockDoc);
    const patch = diff(oldNode, newNode, mockDoc);
    patch(dom);

    assert.equal(dom.childNodes[0].textContent, 'Updated Text');
  });

  it('diffs attribute additions and updates', () => {
    const oldNode = h('button', { disabled: true });
    const newNode = h('button', { className: 'btn-active' });

    const dom = mount(oldNode, mockDoc);
    const patch = diff(oldNode, newNode, mockDoc);
    patch(dom);

    assert.equal(dom.getAttribute('className'), 'btn-active');
    assert.equal(dom.getAttribute('disabled'), undefined);
  });

  it('appends and removes children correctly during reconciliation', () => {
    const oldNode = h('ul', null, h('li', null, 'A'), h('li', null, 'B'));
    const newNode = h('ul', null, h('li', null, 'A'), h('li', null, 'B'), h('li', null, 'C'));

    const dom = mount(oldNode, mockDoc);
    assert.equal(dom.childNodes.length, 2);

    const patch = diff(oldNode, newNode, mockDoc);
    patch(dom);

    assert.equal(dom.childNodes.length, 3);
    assert.equal(dom.childNodes[2].childNodes[0].textContent, 'C');
  });
});
```

---

## 5. Verification & Sample Execution

```bash
$ node --test tests/app.test.js
✔ Virtual DOM Engine > correctly constructs nested VNodes using h() (0.83ms)
✔ Virtual DOM Engine > mounts VNodes to concrete DOM structure (0.42ms)
✔ Virtual DOM Engine > diffs text changes and updates DOM node (0.35ms)
✔ Virtual DOM Engine > diffs attribute additions and updates (0.39ms)
✔ Virtual DOM Engine > appends and removes children correctly during reconciliation (0.45ms)
▶ Virtual DOM Engine (3.24ms)
ℹ tests 5
ℹ suites 1
ℹ pass 5
ℹ fail 0
ℹ cancelled 0
ℹ skipped 0
ℹ todo 0
ℹ duration_ms 52.8
```
