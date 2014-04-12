#include <stdbool.h>

#define OKAY 0
#define HALT 1

#define HEAP_SIZE   2048
#define MAX_NAMES   256
#define NAME_LENGTH 8

typedef unsigned int    l_symbol;
typedef double          l_number;

enum sexpr_type {
    NUMBER, SYMBOL, FUNCTION, CONS, WORD /* a real string data type? */
};


/*
 * S-expressions represent all possible values in Lersp.
 * 
 * Functions are just special cons-cells:
 *
 *  ( body . ( var-names . environment ) )
 *
 */
typedef struct s_expression {
    enum sexpr_type type;
    bool reached; // for garbage collection
    union {
        l_number number;
        l_symbol symbol;
        // Cons cell.
        struct {
            struct s_expression *car;
            struct s_expression *cdr;
        };

        // A very small 'string', for internal use (identifiers, environments).
        char word[NAME_LENGTH];
    };
} sexpr;

/**
 * Generic tuple.
 */
typedef struct {
    int status;
    sexpr *expr;
} result; // TODO better name for this...

/**
 * Reads an s-expression from stdin.
 */
result l_read(void);

/**
 * { read -> eval -> print } loop
 */
void repl(void);

/**
 * Evaluates an s-expression.
 */
result eval(sexpr *);

/**
 * Print an s-expression on stdout.
 */
void display(sexpr *);

/**
 * Print an s-expression on stdout, but all pretty-like.
 */
void print(sexpr *);

/**
 * Initialize the interpreter state.
 *
 * This initializes the free list, adds initial symbols to the symbol table
 * and... that's it.
 */
void init(void);

/**
 * Looks up the **name** for the given symbol.
 */
char *lookup(l_symbol symbol);

/**
 * Get the next free cell. This should really only be called by cons()...
 */
sexpr *new_cell(void);

/**
 * 
 */
sexpr *cons(sexpr*, sexpr*);

/**
 * Space allocated for **all* s-expr in the system.
 */
extern sexpr heap[];

