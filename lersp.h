#include <stdbool.h>

#define OKAY 0
#define HALT 1

#define HEAP_SIZE   2048
#define MAX_NAMES   256
#define NAME_LENGTH 8

/* Define built-in symbols. */
#define COND    0
#define DEFINE  1
#define LABEL   2
#define LAMBDA  3
#define QUOTE   4
#define EVAL    5
#define APPLY   6
#define S_CONS  7
#define CAR     8
#define CDR     9
#define EQ      10
#define ATOM    11
#define S_NULL  12
#define NOT     13
#define AND     14
#define OR      15
#define PLUS    16
#define NEG     17
#define DIV     18
#define MUL     19
#define LT      20
#define GT      21
#define F       22
#define T       23
#define MAP     24
#define REDUCE  25


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
result eval(sexpr *expr, sexpr **environment);

/**
 * Finds the associated expression in the given environment.
 * d
 */
result assoc(l_symbol symbol, sexpr *environment);

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

