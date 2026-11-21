#include <stdio.h>
#include <stdlib.h>

typedef struct Node {
    int value;
    struct Node *next; /* Self-referential pointer */
} Node;

static Node* node_create(int val) {
    Node *n = malloc(sizeof *n);
    if (!n) return NULL;
    n->value = val;
    n->next = NULL;
    return n;
}

static void list_push_front(Node **head_ref, int val) {
    Node *n = node_create(val);
    if (!n) return;
    n->next = *head_ref;
    *head_ref = n;
}

static void list_free(Node **head_ref) {
    if (!head_ref) return;
    Node *curr = *head_ref;
    while (curr) {
        Node *next = curr->next;
        free(curr);
        curr = next;
    }
    *head_ref = NULL;
}

int main(void) {
    printf("=== C Linked Structures with Pointers Demo ===\n");

    Node *head = NULL;
    list_push_front(&head, 30);
    list_push_front(&head, 20);
    list_push_front(&head, 10);

    printf("Traversing linked list: ");
    for (Node *curr = head; curr != NULL; curr = curr->next) {
        printf("%d -> ", curr->value);
    }
    printf("NULL\n");

    list_free(&head);
    printf("List cleanly freed, head is NULL: %s\n", head == NULL ? "YES" : "NO");

    return 0;
}
