/**
 * Lersp.
 *
 * 2014 (c) eddieantonio.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <assert.h>

#include <stdarg.h>
#include <ctype.h>

#include <setjmp.h> // Oh... Oh nooooooo.


#include "lersp.h"

static char INTRO_BANNER[] =
    "; Lersp\n"
    "; 2014 (c) eddieantonio.\n"
    "; We may never know why.\n";

/* Points to the next free cell in the free list. */
static sexpr *next_free_cell;
sexpr heap[HEAP_SIZE];

/* Points to the cell where the universe starts.  */
static sexpr *global = NULL; // execution context?

/* List of (identifier . name) pairs. */
static sexpr *name_list = NULL;
/* Amount of symbos left to use. */
static int next_symbol_id = 0;

/* setjmp read/eval exception buffer. */
sigjmp_buf top_level_exception;


/*
 * A lisp interpreter, I guess.
 */
int main(int argc, char *argv[]) {
    init();

    if (argc < 2) {
        puts(INTRO_BANNER);
        repl();
    }

    print(name_list);

    /* TODO: I guess, interpret a file or something. */

    return 0;
}



void repl(void) {
    int parse_status;
    int eval_status;
    sexpr *input;
    sexpr *evaluation;

    while (1) {
        printf("#=> ");

        parse_status = setjmp(top_level_exception);
        if (parse_status == NOT_PARSED) {
            input = l_read();
        } else if (parse_status == END_INPUT) {
            break;
        } else if (parse_status == SYNTAX_ERROR) {
            /* Most useful error message ever. */
            fprintf(stderr, "Syntax error.\n");
            continue;
        }

        eval_status = setjmp(top_level_exception);
        if (eval_status == NOT_EVALUATED) {
            evaluation = eval(input, &global);
            print(evaluation);
        } else {
            /* Wow. There is no way to be any more vague. */
            fprintf(stderr, "Evaluation error.\n");
        }
    }

}



void display_list(sexpr *head) {
    sexpr *current = head;
    assert(head->type == CONS);

    printf("(");
    do {
        display(current->car);
        if (current->cdr != NULL) {
            printf(" ");
        }

        current = current->cdr;
    } while ((current != NULL) && (current->type == CONS));

    if (current != NULL) {
        /* This must be the end of an improper list. */
        printf(". ");
        display(current);
    }

    printf(")");
}

void display(sexpr* expr) {
    if (expr == NULL) {
        printf("NIL");
        return;
    }

    switch (expr->type) {
        case NUMBER:
            printf("%g", expr->number);
            break;

        case SYMBOL:
            printf("%s", lookup(expr->symbol));
            break;

        case FUNCTION:
            /* A function is just a cons-cell. */
            /* FALL THROUGH! */
        case CONS:
            display_list(expr);
            break;
        case WORD:
            /* DEBUG! `word` type is internal and not representable in program
             * text. */
            printf("\033[1;33;44m#<WORD %s>\033[0m", expr->word);
            break;
        case BUILT_IN_FUNCTION:
            printf("\033[1;33;44m#<BIF %p>\033[0m", expr->func);
            break;
        default:
            assert(0);
    }
}



void print(sexpr *expr) {
    printf(";=> ");
    display(expr);
    puts("");
}

/**
 * Returns the (identifier . word) pair.
 * In itself, not very useful.
 *
 * See lookup() and slookup().
 */
sexpr *lookup_pair(l_symbol symbol) {
    sexpr *entry = name_list;
    sexpr *name_pair, *identifier, *name;

    assert(name_list != NULL);

    do {
        name_pair = entry->car;
        assert(name_pair->type == CONS);

        identifier = name_pair->car;
        name = name_pair->cdr;

        assert(identifier->type == SYMBOL);

        if (identifier->symbol == symbol) {
            assert(name->type == WORD);
            return name_pair;
        }

        entry = entry->cdr;
    } while (entry != NULL);

    printf("Could not find symbol: %d\n", symbol);
    assert(0);

    return NULL; /* For type-checking's sake. */
}

/**
 * Returns the s-expression representing the given symbol.
 */
sexpr *slookup(l_symbol symbol) {
    sexpr *ssymbol = lookup_pair(symbol);
    return (ssymbol == NULL) ? NULL : ssymbol->car;
}

/**
 * Returns the string associated with the given symbol.
 */
char *lookup(l_symbol symbol) {
    sexpr *ssymbol = lookup_pair(symbol);
    return (ssymbol == NULL) ? NULL : ssymbol->cdr->word;
}




static void prepare_free_list(void);
static void prepare_execution_context(void);

void init(void) {
    prepare_free_list();
    prepare_execution_context();
}

static void prepare_free_list(void) {
    sexpr *last_value = NULL;

    /* Link every cell to the next in the free list. */
    for (int i = HEAP_SIZE - 1; i >= 0; i--) {
        sexpr *cell = heap + i;

        cell->type = CONS;
        cell->car = NULL;
        cell->cdr = last_value;
        cell->reached = false;

        last_value = cell;
    }

    next_free_cell = last_value;
}

/* Returns the symbol ID if it exists, else returns -1. */
static l_symbol find_symbol_by_name(char *name) {
    sexpr *current, *pair;

    current = name_list;

    while (current != NULL) {
        pair = current->car;

        if (strcmp(name, pair->cdr->word) == 0) {
            return pair->car->symbol;
        }

        current = current->cdr;
    }

    return -1;
}

static l_symbol insert_symbol(char *name) {
    sexpr *pair, *identifier, *word;

    assert(strlen(name) < NAME_LENGTH);

    int id = find_symbol_by_name(name);
    if (id != -1) {
        return id;
    }

    identifier = new_cell();
    identifier->type = SYMBOL;
    identifier->symbol = next_symbol_id++;

    if (next_symbol_id >= MAX_NAMES) {
        fprintf(stderr, "Ran out of symbol memory. (Sorry %s) :C\n", name);
        fprintf(stderr, "Symbol dump: ");
        print(name_list);
        exit(2);
    }

    word = new_cell();
    word->type = WORD;
    strncpy(word->word, name, NAME_LENGTH);

    pair = cons(identifier, word);

    name_list = cons(pair, name_list);

    return identifier->symbol;
}


/* TODO: Make this less gross.
 *
 * It's important to keep this table in sync with the definitions in the
 * header file! */
static char* INITIAL_SYMBOLS[] = {
    /* Special forms. */
    "COND", "DEFINE", "LABEL", "LAMBDA", "QUOTE",

    "EVAL", "APPLY",
    "CONS", "CAR", "CDR",

    "EQ", "ATOM", "NULL", "NOT", "AND", "OR",
    "+", "-", "/", "*", "<", ">",

    "F", "T",
    "MAP", "REDUCE"
};


static void insert_initial_symbols(void) {
    int i;
    for (i = 0; i < sizeof(INITIAL_SYMBOLS)/sizeof(char**); i++) {
        insert_symbol(INITIAL_SYMBOLS[i]);
    }
}

static void insert_initial_environment(void);

static void prepare_execution_context(void) {
    /* IT BEGINS! */
    insert_initial_symbols();
    /* This will also have the side-effect of setting up the global
     * environment for us. */
    insert_initial_environment();

    /* This is the initial binding list. */
    /* There's nothing in it... */
}


static int mark_cells(sexpr *cell) {
    int count = 0;

    /* Handles non-null atoms. */
    if (cell->type != CONS) {
#if GC_DEBUG
        printf("Reached: ");
        display(cell);
        puts("");
#endif
        cell->reached = true;

        return 1;
    }

    /* Handles cons cells. */
    while ((cell != NULL) && (cell->type == CONS)) {
        cell->reached = true;
        count++;

#if GC_DEBUG
        printf("Reached: (");
        display(cell->car);
        puts(" . ...)");
#endif

        count += mark_cells(cell->car);
        cell = cell->cdr;
    }

    /* Final check for improper lists. */
    if (cell != NULL) {
#if GC_DEBUG
        printf("Reached (improper list): ");
        display(cell);
        puts("");
#endif
        cell->reached = true;
        count++;
    }

    return count;
}

/* TODO: Rewrite this! */
static int mark_all_reachable_cells(void) {
    int count;
    assert(global != NULL);

    count = mark_cells(global);
    count += mark_cells(name_list);

#if GC_DEBUG
    printf("Reached %d cells (%d total)\n", count, HEAP_SIZE);
#endif

    return count;
}


static int garbage_collect(void) {
    int freed;

#if GC_DEBUG
    puts("Garbage collecting...");
#endif

    mark_all_reachable_cells();

    /* For reached cells, unmark 'em. For unreached cells, return 'em to the
     * free list. */
    for (int i = 0; i < HEAP_SIZE; i++) {
        sexpr *cell = heap + i;

        if (!cell->reached) {
#if GC_DEBUG
            printf("Did not reach ");
            display(cell);
            puts("");

#endif
            /* Return the cell to the free list. */
            cell->cdr = next_free_cell;
            next_free_cell = cell;
            freed++;
        } else {
            cell->reached = false;
        }
    }

#if GC_DEBUG
    printf("Freed %d cells\n", freed);
#endif
    return freed;
}


sexpr *new_cell(void) {
    static unsigned int calls_to_new = 0;

    sexpr* cell = next_free_cell;

    if (cell == NULL) {
        garbage_collect();
        cell = next_free_cell;
        if (cell == NULL)  {
            fprintf(stderr, "Ran out of cells in free list.\n");
            exit(-1);
        }
    }

    printf("Cell %u: %p -> %p\n", calls_to_new++, cell, cell->cdr);

    next_free_cell = cell->cdr;

    return cell;
}



enum token {
    NONE,
    LBRACKET, RBRACKET,
    T_SYMBOL,
    T_NUMBER,
    /* String? */
};

union token_data {
    char name[NAME_LENGTH];
    l_number number;
};

/* Reads characters to make a symbol; any extra characters are truncated. */
static void tokenize_symbol(char *);

static enum token next_token(union token_data *state) {
    int c;

    while ((c = fgetc(stdin)) != EOF) {
        if (isspace(c))
            continue;

        /* Parse out comments. */
        if (c == ';') {
            do {
                c = fgetc(stdin);
            } while ((c != '\n') && (c != EOF));
            ungetc(c, stdin);

            continue;
        }

        if (c == '(') {
            return LBRACKET;
        }

        if (c == ')') {
            return RBRACKET;
        }

        /* The following two rely on the read characters to be back on the
         * stream. */

        ungetc(c, stdin);
        if (isdigit(c)) {
            scanf("%lf", &state->number);
            return T_NUMBER;
        }

        /* If we got here, it's a symbol. */
        tokenize_symbol(state->name);
        return T_SYMBOL;
    }

    return NONE;
}

static bool is_symbol_char(char c) {
    return !((c == EOF) || isspace(c) || (c == '(') || (c == ')'));
}

static void tokenize_symbol(char *buffer) {
    int i, c;

    for (i = 0; i < NAME_LENGTH - 1; i++) {
        c = fgetc(stdin);

        if (!is_symbol_char(c)) {
            /* Finalize the buffer. */
            ungetc(c, stdin);
            buffer[i] = '\0';
            return;
        }

        /* Normalize to uppercase. */
        if (isalpha(c)) {
            c &= 0x5f;
        }

        buffer[i] = c;
    }

    /* Finalize the buffer. */
    buffer[i] = '\0';

    do {
        /* loop until non-symbol character. */;
        c = fgetc(stdin);
    } while (is_symbol_char(c));

    ungetc(c, stdin);
}

static sexpr* parse_list(void);

sexpr* l_read(void) {
    static int depth = 0;

    sexpr *expr = NULL;

    enum token token;
    union token_data token_data;

    token = next_token(&token_data);

    switch (token) {
        case T_NUMBER:
            expr = new_cell();
            expr->type = NUMBER;
            expr->number = token_data.number;
            break;

        case T_SYMBOL:
            expr = new_cell();
            expr->type = SYMBOL;
            expr->symbol = insert_symbol(token_data.name);
            break;

        case LBRACKET:
            depth++;
            return parse_list();

        case RBRACKET:
            if (depth < 1) {
                longjmp(top_level_exception, SYNTAX_ERROR);
            }
            depth--;
            expr = NULL;
            break;

        case NONE:
            longjmp(top_level_exception, END_INPUT);
            expr = NULL;
            /* Apparently this is a syntax error? */
            break;
    }

    return expr;
}


/* Parses the inside of a list. */
static sexpr* parse_list(void) {
    sexpr *head, *last, *current, *inner;

    /* Read the first s-expr. */
    inner = l_read();

    /* Don't bother allocating anything if it's NULL (such as if an RBRACKET
     * was returned. */
    if (inner == NULL) {
        return inner;
    }

    last = head = new_cell();
    head->type = CONS;
    head->car = inner;

    inner = l_read();
    /* Build up the list in order. */
    while (inner != NULL) {
        current = new_cell();
        current->type = CONS;
        current->car = inner;

        last->cdr = current;
        last = current;

        inner = l_read();
    }

    last->cdr = NULL;

    return head;
}



/**************************** Built-in functions ****************************/

/* Raise an evaluation error with the given message. */
#define raise_eval_error(msg) \
        fprintf(stderr, msg "\n"); \
        longjmp(top_level_exception, EVAL_ERROR); \
        return NULL // Semicolon omited; should be provided in program text.


sexpr *cons(sexpr* car, sexpr* cdr) {
    sexpr *cell = new_cell();
    cell->type = CONS;

    cell->car = car;
    cell->cdr = cdr;

    return cell;
}

sexpr *car(sexpr *cons_cell) {
    if (cons_cell == NULL) {
        raise_eval_error("car called on nil");
    } else if (cons_cell->type != CONS) {
        raise_eval_error("car called on an atom");
    }

    return cons_cell->car;
}

sexpr *cdr(sexpr *cons_cell) {
    if (cons_cell == NULL) {
        raise_eval_error("cdr called on NIL");
    } else if (cons_cell->type != CONS) {
        raise_eval_error("cdr called on an atom");
    }

    return cons_cell->cdr;
}

sexpr *to_lisp_boolean(bool result) {
    if (result) {
        /* True is really weird, you guys... */
        return slookup(T);
    }
    /* NULL is false... I guess. */
    return NULL;
}

bool c_atom(sexpr *expr) {
    if ((expr == NULL) || (expr->type != CONS)) {
        return true;
    } else {
        return false;
    }
}

bool c_eq(sexpr *a, sexpr *b) {
    if ((a->type != CONS) && (a->type == b->type)) {
        switch (a->type) {
            case NUMBER:
                return a->number == b->number;
            case SYMBOL:
                return a->symbol == b->symbol;
            case LAMBDA:
                return a->car == b->car;
            default:
                return false;
        }
    }
    return false;
}

sexpr *atom(sexpr *expr) {
    return to_lisp_boolean(c_atom(expr));
}

sexpr *eq(sexpr *a, sexpr *b) {
    return  to_lisp_boolean(c_eq(a, b));
}



static sexpr* eval_atom(sexpr *atom, sexpr *env);
static sexpr* eval_form(l_symbol symbol, sexpr *args, sexpr **env);
static sexpr* create_lambda(sexpr *formal_args, sexpr *body, sexpr *env);

sexpr* eval(sexpr *expr, sexpr **env) {
    sexpr* evaluation;

    if (env == NULL) {
        env = &global;
    }

    if (c_atom(expr)) {
        return eval_atom(expr, *env);
    }

    if (expr->car->type == SYMBOL) {
        /* Try to evaluate a special form. */
        return eval_form(expr->car->symbol, expr->cdr, env);
    }

    raise_eval_error("function application not implemented");
    return NULL;

}

static sexpr *eval_atom(sexpr *expr, sexpr *env) {
    if (expr == NULL) {
        return NULL;
    }

    if (expr->type == SYMBOL) {
        return assoc(expr->symbol, env);
    } else {
        /* Every other atom evaluates to itself. */
        return expr;
    }
}


/* Evaluates a cons cell. */
static sexpr* eval_form(l_symbol symbol, sexpr *args, sexpr **env) {
    sexpr *evaluation;

    /*
     * Evaluate all special forms and built-in functions.
     */
    switch (symbol) {
        case COND:
            raise_eval_error("COND not implemented");
            break;

        /* TODO: Should this even be a thing?  Or should it be combined with
         * LABEL? */
        case DEFINE:
            raise_eval_error("DEFINE not implemented");
            break;

        case LABEL:
            evaluation = eval(car(cdr(args)), env);
            update_environment(env, car(args), evaluation);

            break;

        case LAMBDA:
            evaluation = create_lambda(car(args), car(cdr(args)), *env);
            break;

        case QUOTE:
            evaluation = car(args);
            break;

        default:
            /* By the way, this makes no sense: */
            fprintf(stderr, "eval not defined for ");
            fflush(stderr);
            lookup(symbol);
            fprintf(stderr, "\n");
            longjmp(top_level_exception, EVAL_ERROR);
            return NULL;
    }

    return evaluation;
}

sexpr* update_environment(sexpr **env, sexpr* symbol, sexpr *value) {
    sexpr *binding = new_cell();
    binding->type = CONS;
    binding->car = symbol;
    binding->cdr = value;

    printf("Updating enviroment: ");
    display(*env);
    puts("");

    *env = cons(binding, *env);

    printf("Environment is now: ");
    display(*env);
    puts("");

    return *env;
}

static sexpr* evcon(sexpr *expr, sexpr *environment) {
    sexpr* evaluation;

    /* TODO */
    return evaluation;
}

static sexpr* create_lambda(sexpr *formal_args, sexpr *body, sexpr *env) {
    sexpr *lambda = cons(body, cons(formal_args, env));
    lambda->type = FUNCTION;

    return lambda;
}


/* Returns the first expression that is associated with the symbol in the
 * given environment. */
sexpr* assoc(l_symbol symbol, sexpr *environment) {
    sexpr *current, *pair;

    if (environment == NULL) {
        environment = global;
    }

    current = environment;

    /* Find the first symbol. */
    while (current != NULL) {
        pair = current->car;

        if (pair->car->symbol == symbol) {
            /* Symbol was found! */
            return pair->cdr;
        }

        current = current->cdr;
    }

    fprintf(stderr, "Undefined symbol: %s\n", lookup(symbol));
    longjmp(top_level_exception, EVAL_ERROR);

    return NULL;
}




#define SIMPLE_WRAPPER_1(name) \
    sexpr * wrapped_ ## name (int n, sexpr *args[]) { \
        if (n != 1) { \
            fprintf(stderr, "Invalid arguments for " #name "\n"); \
            longjmp(top_level_exception, EVAL_ERROR); \
        } \
        return name(args[0]);  \
    }


#define SIMPLE_WRAPPER_2(name) \
    sexpr * wrapped_ ## name (int n, sexpr *args[]) { \
        if (n != 2) { \
            fprintf(stderr, "Invalid arguments for " #name "\n"); \
            longjmp(top_level_exception, EVAL_ERROR); \
        } \
        return name(args[0], args[1]);  \
    }

sexpr *wrapped_eval(int n, sexpr *args[]) {
    if (n != 1) {
        raise_eval_error("eval takes exactly one argument.");
    }
    return eval(args[0], &global);
}



SIMPLE_WRAPPER_2(cons)
SIMPLE_WRAPPER_1(car)
SIMPLE_WRAPPER_1(cdr)

struct builtin_func_def {
    l_symbol identifier;
    l_builtin func;
    int arity;
};

#define VARIABLE_ARITY -1

static struct builtin_func_def BUILT_INS[] = {
    { EVAL, wrapped_eval, 2 },
    /*
    { APPLY, wrapped_apply, 2 },
    { CONS, wrapped_cons, 2 },

    { CAR, wrapped_car, 1 },
    { CDR, wrapped_cdr, 1 },

    { EQ, atom, 1 },
    { ATOM, null, 1 },
    { NOT, not, 1 },

    { AND, and, VARIABLE_ARITY },
    { OR, or, VARIABLE_ARITY },
    { PLUS, plus, VARIABLE_ARITY },
    { TIMES, plus, VARIABLE_ARITY },
    { TIMES, plus, VARIABLE_ARITY },
    */
};


static void insert_initial_environment(void) {
    int i;
    sexpr *func;

    int builtin_count = sizeof(BUILT_INS)/sizeof(struct builtin_func_def);

    for (i = 0; i < builtin_count; i++) {
        func = new_cell();
        func->type = BUILT_IN_FUNCTION;
        func->arity = BUILT_INS[i].arity;
        func->func = BUILT_INS[i].func;;

        update_environment(&global, slookup(BUILT_INS[i].identifier), func);
    }

}

