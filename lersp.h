#include <stdbool.h>

#define HEAP_SIZE   2048
#define MAX_NAMES   128
#define NAME_LENGTH 8

/* Define built-in symbols. */
#define COND    0
#define DEFINE  1
#define LABEL   2 // Deprecate.
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
#define F       22 // Deprecate
#define T       23
#define MAP     24
#define REDUCE  25

/* setjmp exception return values. */
#define NOT_PARSED      0 // Initial setjmp.
#define NOT_EVALUATED   0 // Initial setjmp.
#define SYNTAX_ERROR    1
#define END_INPUT       2
#define EVAL_ERROR      2


typedef unsigned int    l_symbol;
typedef double          l_number;

enum sexpr_type {
    CONS, NUMBER, SYMBOL, FUNCTION, BUILT_IN_FUNCTION,  WORD /* a real string data type? */
};

typedef struct s_expression sexpr;
typedef sexpr* (* l_builtin)(int, sexpr *[]);

/*
 * S-expressions represent all possible values in Lersp.
 *
 * Functions are just special cons-cells:
 *
 *  ( body . ( var-names . environment ) )
 *
 */
struct s_expression {
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

        /* Builtin function. */
        struct {
            l_builtin func;
            int arity;
        };
    };
};


/**
 * Reads an s-expression from stdin.
 * Calls longjpm
 */
sexpr *l_read(void);

/**
 * { read -> eval -> print } loop
 */
void repl(void);

/**
 * Evaluates an s-expression.
 */
sexpr* eval(sexpr *expr, sexpr *environment);

/**
 * Applies the function func to the given arguments.
 */
sexpr* apply(sexpr *func, sexpr *args);

/**
 * Finds the associated expression in the given environment.
 */
sexpr* assoc(l_symbol symbol, sexpr *environment);

/**
 * Associates symbol (must be an s-expression) with the given value,
 * augmenting the environment.
 */
sexpr* update_environment(sexpr **env, sexpr *symbol, sexpr *value);

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
 * Creates a new cons cell that pairs the given expressions.
 */
sexpr *cons(sexpr*, sexpr*);

/**
 * Space allocated for **all* s-expr in the system.
 */
extern sexpr heap[];

