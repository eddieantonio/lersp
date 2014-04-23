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

/* Internal nil; having this explict makes the GC algorithm more elegant. */
static sexpr nil = {
    .type = CONS,
    .reached = 0,
    .car = &nil,
    .cdr = &nil,
};

sexpr *const NIL = &nil;

/* Points to the cell where the universe starts.  */
static sexpr *global = &nil; // execution context?

/* List of (identifier . name) pairs. */
static sexpr *name_list = &nil;
/* Amount of symbols left to use. */
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

#if VERBOSE_DEBUG
    print(name_list);
#endif

    /* TODO: I guess, interpret a file or something. */

    return 0;
}



void repl(void) {
    int parse_status;
    int eval_status;
    sexpr *input;
    sexpr *evaluation;

    while (1) {
        printf(";=> ");

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
            evaluation = eval(input, global);
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
        if (current->cdr != NIL) {
            printf(" ");
        }

        current = current->cdr;
    } while ((current != NIL) && (current->type == CONS));

    if (current != NIL) {
        /* This must be the end of an improper list. */
        printf(". ");
        display(current);
    }

    printf(")");
}

void display(sexpr* expr) {
    if (expr == NIL) {
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
            printf("#<LAMBDA ");
            display(expr->car);
            printf(">");
            break;
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

    assert(name_list != NIL);

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
    } while (entry != NIL);

    printf("Could not find symbol: %d\n", symbol);
    assert(0);

    return NIL; /* For type-checking's sake. */
}

/**
 * Returns the s-expression representing the given symbol.
 */
sexpr *slookup(l_symbol symbol) {
    sexpr *ssymbol = lookup_pair(symbol);
    return (ssymbol == NIL) ? NIL : ssymbol->car;
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
    sexpr *last_value = NIL;

    /* Link every cell to the next in the free list. */
    for (int i = HEAP_SIZE - 1; i >= 0; i--) {
        sexpr *cell = heap + i;

        cell->type = CONS;
        cell->car = NIL;
        cell->cdr = last_value;
        cell->reached = 0;

        last_value = cell;
    }

    next_free_cell = last_value;
}

/* Returns the symbol ID if it exists, else returns -1. */
static l_symbol find_symbol_by_name(char *name) {
    sexpr *current, *pair;

    current = name_list;

    while (current != NIL) {
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
    "MAP", "REDUCE", "GC"
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
}

#if GC_DEBUG
#define GC_DEBUG_PRINT(msg, cell) \
    do {    \
        printf(msg);    \
        display(cell);  \
        puts("");   \
    while (0)
#else
#define GC_DEBUG_PRINT() ((void) 0)
#endif

#define NOT_VISITED     0
#define FULLY_VISITED   3

/*
 * Implementation inspired by:
 * [Gries06] D. Gries. Schorr-Waite Graph Marking Algorithm – Developed
 *           With Style. (http://www.cs.cornell.edu/courses/cs312/2007fa/lectures/lec21-schorr-waite.pdf)
 *           2006.
 */
static int mark_cells(sexpr *cell) {
    int count = 0;
    sexpr *previous, *current, *vroot;

    /* The sentinel node; this needs to be a valid node initialized in its
     * second visit stage. Note the values of car and cdr are */
    sexpr sentinel = {
        .type = CONS,
        .reached = 2,
        .car = NULL,
        .cdr = cell
    };

    current = cell;
    /* Note: vroot is the sentinel node that indicates termination. */
    previous = vroot = &sentinel;

    /* Abort the algorithm early if this node was already visited. */
    if (current->reached == FULLY_VISITED) {
        return 0;
    }

    while (current != vroot) {
        assert(current->reached != FULLY_VISITED);

        /* Visited current node once. */
        current->reached++;

#if GC_DEBUG
        if (current->reached == FULLY_VISITED) {
            count++;
        }
#endif

        /* Unconditionally mark the child if it is an atom.
         * Note that, even though the code suggests only the left child is
         * marked, the right child is marked due to the algorithm rotating its
         * pointer into the car position. See [Gries06] for full case
         * analysis. */
        if (is_atom(current->car) && (current->car->reached != FULLY_VISITED)) {
            assert(current->car->reached == NOT_VISITED);
            current->car->reached = FULLY_VISITED;
            count++;
        }

        /* See [Gries06] for full case analysis. */
        if ((current->reached == FULLY_VISITED)
                || (current->car->reached == NOT_VISITED)) {
            /* Traverse into whatever is in the "left" subgraph. */
            /* Rotate values like so:
            current, current->car, current->cdr, previous =
                current->car, current->cdr, previous, current;
            */
            /* A bunch of temp pointers -- careful graph colouring can come up
             * with a more elegant way to swap all this info. */
            sexpr *_current = current,
                  *_previous = previous,
                  *_left = current->car,
                  *_right = current->cdr;

            current = _left;
            _current->car = _right;
            _current->cdr = _previous;
            previous = _current;

        } else {
            /* Do not traverse further; simply rotate pointers. */
            /* Rotate values like so:
            current->car, current->cdr, previous =
                current->cdr, previous, current->car;
            */
            sexpr *_right = current->cdr,
                  *_previous = previous,
                  *_left = current->car;

            current->car = _right;
            current->cdr = _previous;
            previous = _left;
        }
    }

    return count;
}

/* TODO: Rewrite this! */
static int mark_all_reachable_cells(void) {
    int count;
    assert(global != NIL);

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

        if (cell->reached != FULLY_VISITED) {
            /* Return the cell to the free list. */
            cell->cdr = next_free_cell;
            next_free_cell = cell;
            freed++;
        } else {
            cell->reached = NOT_VISITED;
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

    if (cell == NIL) {
        garbage_collect();
        cell = next_free_cell;
        if (cell == NIL)  {
            fprintf(stderr, "Ran out of cells in free list.\n");
            exit(-1);
        }
    }

#if VERBOSE_DEBUG
    printf("Cell %u: %p -> %p\n", calls_to_new++, cell, cell->cdr);
#endif

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

    sexpr *expr = NIL;

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
                depth = 0;
                longjmp(top_level_exception, SYNTAX_ERROR);
            }
            depth--;
            expr = NIL;
            break;

        case NONE:
            longjmp(top_level_exception, END_INPUT);
            expr = NIL;
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

    /* Don't bother allocating anything if it's NIL (such as if an RBRACKET
     * was returned. */
    if (inner == NIL) {
        return inner;
    }

    last = head = new_cell();
    head->type = CONS;
    head->car = inner;

    inner = l_read();
    /* Build up the list in order. */
    while (inner != NIL) {
        current = new_cell();
        current->type = CONS;
        current->car = inner;

        last->cdr = current;
        last = current;

        inner = l_read();
    }

    last->cdr = NIL;

    return head;
}



/**************************** Built-in functions ****************************/

/* Raise an evaluation error with the given message. */
#define raise_eval_error(msg) \
        fprintf(stderr, msg "\n"); \
        longjmp(top_level_exception, EVAL_ERROR); \
        return NIL // Semicolon omitted; should be provided in program text.


sexpr *cons(sexpr* car, sexpr* cdr) {
    sexpr *cell = new_cell();
    cell->type = CONS;

    cell->car = car;
    cell->cdr = cdr;

    return cell;
}

sexpr *car(sexpr *cons_cell) {
    if (cons_cell == NIL) {
        raise_eval_error("car called on nil");
    } else if (cons_cell->type != CONS) {
        raise_eval_error("car called on an atom");
    }

    return cons_cell->car;
}

sexpr *cdr(sexpr *cons_cell) {
    if (cons_cell == NIL) {
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
    /* NIL is false... I guess. */
    return NIL;
}

bool c_atom(sexpr *expr) {
    if ((expr == NIL) || (expr->type != CONS)) {
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
                return a == b;
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
static sexpr* eval_cond(sexpr *conditions, sexpr *env);
static sexpr* eval_form(l_symbol symbol, sexpr *args, sexpr *env);
static sexpr* create_lambda(sexpr *formal_args, sexpr *body, sexpr *env);
/* Equivalent to (map eval args). */
static sexpr* eval_list(sexpr *args, sexpr *env);

sexpr* eval(sexpr *expr, sexpr *env) {
    sexpr* evaluation;

    if (env == NIL) {
        env = global;
    }

    if (c_atom(expr)) {
        return eval_atom(expr, env);
    }

    if (expr->car->type == SYMBOL) {
        /* Try to evaluate a special form. */
        return eval_form(expr->car->symbol, expr->cdr, env);
    } else {
        return apply(eval(car(expr), env), cdr(expr));
    }

}

static sexpr* call_builtin(l_builtin func, sexpr *args);
static sexpr* apply_lambda(sexpr *func, sexpr *args);

sexpr *apply(sexpr *func, sexpr *args) {
#if VERBOSE_DEBUG
    printf("Applying: ");
    display(func);
    puts("");
#endif

    if (func == NIL) {
        raise_eval_error("Cannot apply NIL");
    }

    if (func->type == FUNCTION) {
        return apply_lambda(func, args);
    } else if (func->type == BUILT_IN_FUNCTION) {
        return call_builtin(func->func, args);
    }

    raise_eval_error("First argument to apply is not callable");
    return NIL;
}



static sexpr *eval_atom(sexpr *expr, sexpr *env) {
    if (expr == NIL) {
        return NIL;
    }

    if (expr->type == SYMBOL) {
        return assoc(expr->symbol, env);
    } else {
        /* Every other atom evaluates to itself. */
        return expr;
    }
}


/* Evaluates a cons cell. */
static sexpr* eval_form(l_symbol symbol, sexpr *args, sexpr *env) {
    sexpr *evaluation, *temp_env;

    /*
     * Evaluate all special forms and built-in functions.
     */
    switch (symbol) {
        case COND:
            return eval_cond(args, env);
            break;

        /* TODO: Should this even be a thing?  Or should it be combined with
         * LABEL? */
        case DEFINE:
            raise_eval_error("DEFINE not implemented");
            break;

        case LABEL:
            /* This one is weird. */
            /* Update enivorment first... */
            temp_env = update_environment(&global, car(args), NIL);
            evaluation = eval(car(cdr(args)), temp_env);

            /* Then MUTATE the environment. */
            temp_env->car->cdr = evaluation;

            return evaluation;

        case LAMBDA:
            return create_lambda(car(args), car(cdr(args)), env);
            break;

        case QUOTE:
            return car(args);
            break;

        default:
            /* Not a special form -- delegate to apply. */
            return apply(assoc(symbol, env), eval_list(args, env));
    }

}

/*
 * Evaluates all elements of the list.
 * (map (lambda (x) (eval x env) args)).
 */
sexpr *eval_list(sexpr *args, sexpr *env) {
    sexpr *head, *unevaluated, *current;

    if (args == NIL) {
        return NIL;
    }

    current = head = cons(eval(car(args), env), NIL);
    unevaluated = cdr(args);

    while (unevaluated != NIL) {
        current->cdr = cons(eval(car(unevaluated), env), NIL);
        /* Advance positions in both lists. */
        unevaluated = unevaluated->cdr;
        current = current->cdr;
    }

#if VERBOSE_DEBUG
    printf("Evaluated list: ");
    display(head);
    puts("");
#endif

    return head;
}

/* Returns the truthiness of the given expression. */
bool is_truthy(sexpr *value) {
    return (value != NIL)
        && (value->type == SYMBOL)
        && (value->symbol == T);
}

/* Evaluates the true condition list. */
sexpr *eval_cond(sexpr *conditions, sexpr *env) {
    sexpr *current, *cond_pair, *result;

    for (current = conditions; current != NIL; current = cdr(current)) {
        cond_pair = car(current);

        result = eval(car(cond_pair), env);

        if (is_truthy(result)) {
            return eval(car(cdr(cond_pair)), env);
        }
    }

    /* Scheme does this... but I think this is unambiguously a major error. */
    return NIL;
}

/* Length of an s-expression. This might... be a built-in. */
int slength(sexpr *list) {
    if (list == NIL) {
        return 0;
    } else {
        return 1 + slength(cdr(list));
    }
}

sexpr *call_builtin(l_builtin func, sexpr *args) {
    int i, argc = slength(args);
    sexpr *argv[argc];
    sexpr *current;

    for (i = 0, current = args; i < argc; i++) {
        argv[i] = car(current);
        current = cdr(current);

#if VERBOSE_DEBUG
        printf("arg %d: ", i);
        display(argv[i]);
        puts("");
#endif
    }

    return func(argc, argv);
}

/* Returns a new environment where old environment is augmented with the
 * values. */
sexpr *bind_args(sexpr *free_vars, sexpr* values, sexpr *old_env) {
    sexpr *symbol_pair, *value_pair, *env;

    env = old_env;
    symbol_pair = free_vars;
    value_pair = values;

    while (symbol_pair != NIL) {
        if (value_pair == NIL) {
            raise_eval_error("Not enough arguments for function.");
        }

        update_environment(&env, car(symbol_pair), car(value_pair));

        value_pair = cdr(value_pair);
        symbol_pair = cdr(symbol_pair);
    }

    if (symbol_pair != NIL) {
        fprintf(stderr, "Warning: Too many aruguments for function.");
    }

    return env;
}

sexpr *apply_lambda(sexpr *lambda, sexpr *args) {
    assert((lambda != NIL) && (lambda->type == FUNCTION));

    sexpr *body = lambda->car;
    sexpr *free_vars = lambda->cdr->car;
    sexpr *env = bind_args(free_vars, args, lambda->cdr->cdr);

#if VERBOSE_DEBUG
    printf("Calling enviroment for func is: ");
    display(env);
    puts("");
#endif

    /* 0-arity function. Apply with existing environment. */
    return eval(body, env);

}

sexpr *update_environment(sexpr **env, sexpr* symbol, sexpr *value) {
    sexpr *binding = new_cell();
    binding->type = CONS;
    binding->car = symbol;
    binding->cdr = value;

#if VERBOSE_DEBUG
    printf("Updating enviroment: ");
    display(*env);
    puts("");
#endif

    *env = cons(binding, *env);

#if VERBOSE_DEBUG
    printf("Environment is now: ");
    display(*env);
    puts("");
#endif

    return *env;
}

/**
 * Lambdas are actually just cons cells.
 *
 * (body . (formal-arguments . lexical-environment))
 *
 * For example, the inner lambda in this expression:
 *      (lambda (a b) (lambda (m) (m a b)))
 *
 * Is:
 *      ((m a b) . ((m) . ((a . BOUND-A) (b . BOUND-B) . ENV)))
 */
static sexpr* create_lambda(sexpr *formal_args, sexpr *body, sexpr *env) {
    sexpr *lambda = cons(body, cons(formal_args, env));
    lambda->type = FUNCTION;

    return lambda;
}


/* Returns the first expression that is associated with the symbol in the
 * given environment. */
sexpr* assoc(l_symbol symbol, sexpr *environment) {
    sexpr *current, *pair;

    if (environment == NIL) {
        environment = global;
    }

    current = environment;

    /* Find the first symbol. */
    while (current != NIL) {
        pair = current->car;

        if (pair->car->symbol == symbol) {
            /* Symbol was found! */
            return pair->cdr;
        }

        current = current->cdr;
    }

    fprintf(stderr, "Undefined symbol: %s\n", lookup(symbol));
    longjmp(top_level_exception, EVAL_ERROR);

    return NIL;
}



/* Wrapped built-ins. */

sexpr *wrapped_eval(int n, sexpr *args[]) {
    if (n != 1) {
        raise_eval_error("eval takes exactly one argument.");
    }
    return eval(args[0], global);
}

sexpr* null(int n, sexpr *argv[]) {
    if (n != 1) {
        raise_eval_error("null takes exactly one argument.");
    }
    return to_lisp_boolean(argv[0] == NIL);
}

sexpr* not(int n, sexpr *argv[]) {
    if (n != 1) {
        raise_eval_error("null takes exactly one argument.");
    }
    sexpr *value = argv[0];
    return to_lisp_boolean(!is_truthy(value));
}

sexpr* new_number(l_number num) {
    sexpr* result;
    result = new_cell();
    result->type = NUMBER;
    result->number = num;
    return result;
}

sexpr* plus(int argc, sexpr *argv[]) {
    int i;
    l_number total = 0;
    sexpr *value;

    for (i = 0; i < argc; i++) {
        value = argv[i];

        if ((value == NIL) || (value->type != NUMBER)) {
            raise_eval_error("+ given non-numeric arguments.");
        }
        total += argv[i]->number;
    }

    return new_number(total);
}

sexpr* mul(int argc, sexpr *argv[]) {
    int i;
    l_number product = 1;
    sexpr *value;

    for (i = 0; i < argc; i++) {
        value = argv[i];

        if ((value == NIL) || (value->type != NUMBER)) {
            raise_eval_error("* given non-numeric arguments.");
        }
        product *= argv[i]->number;
    }

    return new_number(product);
}

sexpr *neg(sexpr *arg) {
    return new_number(-arg->number);
}

sexpr *var_sub(int argc, sexpr *argv[]) {
    int i;
    assert((argc > 1) && (argv[0] != NIL) && (argv[0]->type == NUMBER));

    /* We can assume that argv[0] is a number. */
    l_number total = argv[0]->number;
    sexpr *value;

    for (i = 1; i < argc; i++) {
        value = argv[i];

        if ((value == NIL) || (value->type != NUMBER)) {
            raise_eval_error("* given non-numeric arguments.");
        }

        total -= argv[i]->number;
    }

    return new_number(total);
}

sexpr *sub(int argc, sexpr *argv[]) {
    if ((argc < 1) || (argv[0] == NIL) || (argv[0]->type != NUMBER)) {
        raise_eval_error("- takes at least 1 numeric argument.");
    }

    if (argc == 1) {
        return neg(argv[0]);
    } else {
        return var_sub(argc, argv);
    }

    return new_number(argv[0]->number - argv[1]->number);
}

sexpr *var_div(int argc, sexpr *argv[]) {
    int i;
    l_number divisor, result = 1;
    sexpr *value;

    for (i = 0; i < argc; i++) {
        value = argv[i];

        if ((value == NIL) || (value->type != NUMBER)) {
            raise_eval_error("* given non-numeric arguments.");
        }
        divisor = argv[i]->number;

        if (divisor == 0) {
            raise_eval_error("divide by zero.");
        }

        result /= divisor;
    }

    return new_number(result);
}



struct builtin_func_def {
    l_symbol identifier;
    l_builtin func;
    int arity;
};

#define VARIABLE_ARITY -1

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


/* Make wrappers for all of these dang functions. */
SIMPLE_WRAPPER_2(apply)

SIMPLE_WRAPPER_2(cons)
SIMPLE_WRAPPER_1(car)
SIMPLE_WRAPPER_1(cdr)

SIMPLE_WRAPPER_2(eq)
SIMPLE_WRAPPER_1(atom)

/* Force a garbage collection. */
sexpr *gc(int n, sexpr *args[]) {
    garbage_collect();
    return NIL;
}


static struct builtin_func_def BUILT_INS[] = {
    { EVAL, wrapped_eval, 2 },
    { APPLY, wrapped_apply, 2 },

    { S_CONS, wrapped_cons, 2 },
    { CAR, wrapped_car, 1 },
    { CDR, wrapped_cdr, 1 },

    { EQ, wrapped_eq, 1 },
    { S_NULL, null, 1 },
    { ATOM, wrapped_atom, 1 },
    { NOT, not, 1 },

    { PLUS, plus, VARIABLE_ARITY },
    { MUL, mul, VARIABLE_ARITY },
    { NEG, sub, VARIABLE_ARITY },
    { DIV, var_div, VARIABLE_ARITY },
    { GC, gc, 0 },
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

    /* And also, T, which evaluates to itself. */
    update_environment(&global, slookup(T), slookup(T));

}

