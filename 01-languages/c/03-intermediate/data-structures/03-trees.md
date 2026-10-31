# Trees

A tree is a hierarchy of nodes where each node has zero or more children and exactly one parent (except the root). The binary search tree (BST) is the classic example: it stores keys in sorted order and supports search, insert, and delete in O(height).

**Prerequisites:** [Pointers](../../02-core-concepts/03-pointers.md), [linked lists](./01-linked-lists.md), [stacks and queues](./02-stacks-queues.md).

## Binary tree basics

```text
          8                 root
        /   \
       3     10             8's children
      / \      \
     1   6      14          leaves: 1, 6, 14
        / \    /
       4   7  13
```

Terms: **root**, **leaf** (no children), **height** (longest root-to-leaf path), **depth** (distance from root), **subtree**.

```c
typedef struct tree {
    int          key;
    struct tree *left;
    struct tree *right;
} tree_t;
```

A **binary search tree** keeps the invariant: all keys in the left subtree are `<` the node's key, all keys in the right subtree are `>`.

## Insert (iterative)

```c
#include <stdbool.h>
#include <stdlib.h>

bool bst_insert(tree_t **root, int key) {
    tree_t **pp = root;                         /* pointer to the link we will fill */
    while (*pp != NULL) {
        if      (key < (*pp)->key) pp = &(*pp)->left;
        else if (key > (*pp)->key) pp = &(*pp)->right;
        else return true;                       /* duplicate: ignore */
    }
    tree_t *n = malloc(sizeof *n);
    if (n == NULL) return false;
    n->key = key;
    n->left = n->right = NULL;
    *pp = n;
    return true;
}
```

Using a pointer-to-pointer (as in [linked lists](./01-linked-lists.md)) removes the "empty tree" special case.

## Search

```c
tree_t *bst_find(tree_t *t, int key) {
    while (t != NULL) {
        if      (key < t->key) t = t->left;
        else if (key > t->key) t = t->right;
        else return t;
    }
    return NULL;
}
```

## Delete

Three cases: no children (just remove), one child (replace with it), two children (replace the key with the in-order successor, then delete the successor).

```c
tree_t *bst_delete(tree_t *t, int key) {
    if (t == NULL) return NULL;
    if (key < t->key) {
        t->left = bst_delete(t->left, key);
    } else if (key > t->key) {
        t->right = bst_delete(t->right, key);
    } else {
        if (t->left == NULL)  { tree_t *r = t->right; free(t); return r; }
        if (t->right == NULL) { tree_t *l = t->left;  free(t); return l; }
        tree_t *m = t->right;                   /* smallest key in the right subtree */
        while (m->left != NULL) m = m->left;
        t->key = m->key;
        t->right = bst_delete(t->right, m->key);
    }
    return t;
}
```

Call as `root = bst_delete(root, key);`.

## Traversals

```c
void inorder(const tree_t *t, void (*visit)(int)) {      /* left, node, right: sorted order for a BST */
    if (t == NULL) return;
    inorder(t->left, visit);
    visit(t->key);
    inorder(t->right, visit);
}
```

| Order | Visits | Typical use |
|---|---|---|
| In-order | left, node, right | sorted output of a BST |
| Pre-order | node, left, right | copying a tree, serialization |
| Post-order | left, right, node | freeing a tree, evaluating expression trees |
| Level-order | row by row | breadth-first; uses a [queue](./02-stacks-queues.md) |

Free a tree with a post-order traversal (children before the parent):

```c
void tree_free(tree_t *t) {
    if (t == NULL) return;
    tree_free(t->left);
    tree_free(t->right);
    free(t);
}
```

## The balance problem

BST operations take O(height). Inserting sorted keys (1, 2, 3, ...) creates a **degenerate tree**, which is effectively a linked list with O(n) operations, and recursive functions then risk stack overflow.

| Shape | Height | Search |
|---|---|---|
| Balanced | ~log₂ n | O(log n) |
| Degenerate | n | O(n) |

Self-balancing trees keep the height logarithmic:

- **AVL tree:** strictly balanced; more rotations, faster lookups.
- **Red-black tree:** looser balance; fewer rotations; used in many standard libraries and the Linux kernel.
- **B-tree / B+ tree:** wide nodes that match disk blocks; used in databases and file systems.

Writing a balanced tree from scratch is rarely worth it; use a library or consider a [hash table](./04-hash-tables.md) when order doesn't matter.

## Other tree structures

- **Heap:** a complete binary tree stored in an **array** (children of `i` at `2i+1` and `2i+2`), giving a priority queue with O(log n) insert and extract-min.
- **Trie:** a tree keyed by characters; fast prefix search (autocomplete, routing tables).
- **Expression/syntax trees:** the output of a parser; see the [expression interpreter project](../../09-projects/08-expression-interpreter/).
- **N-ary trees:** each node holds an array or a first-child/next-sibling pair of pointers (file systems, DOM).

## Complexity (BST)

| Operation | Average (random keys) | Worst (degenerate) |
|---|---|---|
| Search / insert / delete | O(log n) | O(n) |
| Traversal | O(n) | O(n) |

## Common mistakes

| Mistake | Effect |
|---|---|
| Forgetting that `delete` can change the root (`root = bst_delete(root, k)`) | Dangling or stale root |
| Freeing a node before its children | Leaks the subtrees |
| Recursion on very deep/degenerate trees | Stack overflow; use an explicit stack |
| Not initializing `left`/`right` to `NULL` | Wild pointers |
| Inserting sorted data into an unbalanced BST | O(n) operations |
| Violating the BST invariant by mutating keys in place | Searches silently fail |
| Comparing with `a - b` for ordering ints | Overflow; use `<` / `>` |

## Summary

- A BST gives sorted storage with O(height) search, insert, delete.
- Use pointer-to-pointer for insertion; handle the three delete cases.
- Post-order frees a tree; in-order yields sorted keys.
- Unbalanced input degrades BSTs; use a balanced variant or another structure when it matters.

**Next:** [Hash tables](./04-hash-tables.md)
