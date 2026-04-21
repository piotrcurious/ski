#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum {
    NODE_S = 0,
    NODE_K = 1,
    NODE_I = 2,
    NODE_APP = 3
} NodeKind;

typedef struct Node {
    NodeKind kind;
    struct Node *left;
    struct Node *right;
} Node;

static void die(const char *msg) {
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(EXIT_FAILURE);
}

static void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) die("out of memory");
    return p;
}

static Node *new_leaf(NodeKind kind) {
    Node *n = xmalloc(sizeof(*n));
    n->kind = kind;
    n->left = NULL;
    n->right = NULL;
    return n;
}

static Node *new_app(Node *left, Node *right) {
    Node *n = xmalloc(sizeof(*n));
    n->kind = NODE_APP;
    n->left = left;
    n->right = right;
    return n;
}

static Node *clone_tree(const Node *n) {
    if (!n) return NULL;

    Node *copy = xmalloc(sizeof(*copy));
    copy->kind = n->kind;
    copy->left = clone_tree(n->left);
    copy->right = clone_tree(n->right);
    return copy;
}

static void free_tree(Node *n) {
    if (!n) return;
    free_tree(n->left);
    free_tree(n->right);
    free(n);
}

static int next_significant_char(void) {
    int c;
    do {
        c = getchar();
    } while (c != EOF && isspace((unsigned char)c));
    return c;
}

static Node *parse_expr(void) {
    int c = next_significant_char();
    if (c == EOF) {
        die("unexpected EOF while parsing");
    }

    switch (c) {
        case 'S': return new_leaf(NODE_S);
        case 'K': return new_leaf(NODE_K);
        case 'I': return new_leaf(NODE_I);

        case '(': {
            Node *left = parse_expr();
            Node *right = parse_expr();

            int close = next_significant_char();
            if (close != ')') {
                free_tree(left);
                free_tree(right);
                die("expected ')'");
            }

            return new_app(left, right);
        }

        default:
            die("unexpected character while parsing");
    }
}

static void print_tree(const Node *n) {
    if (!n) return;

    if (n->kind == NODE_APP) {
        putchar('(');
        print_tree(n->left);
        print_tree(n->right);
        putchar(')');
    } else {
        static const char leaf_chars[] = "SKI";
        putchar(leaf_chars[n->kind]);
    }
}

static bool try_reduce_here(Node **slot) {
    Node *n = *slot;
    if (!n || n->kind != NODE_APP) return false;

    Node *l = n->left;
    Node *r = n->right;

    /* I x -> x */
    if (l && l->kind == NODE_I) {
        *slot = r;
        free(l);
        free(n);
        return true;
    }

    /*
        (K x) y -> x
        n = ((K x) y)
    */
    if (l &&
        l->kind == NODE_APP &&
        l->left &&
        l->left->kind == NODE_K) {
        Node *x = l->right;

        *slot = x;
        free(l->left);
        free(l);
        free(n);
        return true;
    }

    /*
        ((S f) g) x -> (f x) (g x)
        To avoid aliasing and double-free problems, clone the duplicated
        argument subtrees.
    */
    if (l &&
        l->kind == NODE_APP &&
        l->left &&
        l->left->kind == NODE_APP &&
        l->left->left &&
        l->left->left->kind == NODE_S) {
        Node *f = l->left->right;
        Node *g = l->right;
        Node *x = r;

        Node *fx = new_app(clone_tree(f), clone_tree(x));
        Node *gx = new_app(clone_tree(g), clone_tree(x));
        Node *res = new_app(fx, gx);

        free_tree(n);
        *slot = res;
        return true;
    }

    return false;
}

static bool reduce_once(Node **slot) {
    if (!slot || !*slot) return false;

    if (try_reduce_here(slot)) {
        return true;
    }

    Node *n = *slot;
    if (n->kind == NODE_APP) {
        if (reduce_once(&n->left)) return true;
        if (reduce_once(&n->right)) return true;
    }

    return false;
}

int main(void) {
    Node *root = parse_expr();

    int trailing = next_significant_char();
    if (trailing != EOF) {
        free_tree(root);
        die("trailing input after expression");
    }

    while (reduce_once(&root)) {
        /* keep reducing until normal form */
    }

    print_tree(root);
    putchar('\n');

    free_tree(root);
    return 0;
}
