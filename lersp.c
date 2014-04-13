/**
 * Lersp.
 *
 * 2014 (c) eddieantonio.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
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
            /* Wow. There is not way to be anymore vague. */
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
            printf("\033[1;7;43;34m%s\033[0m", expr->word);
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

char *lookup(l_symbol symbol) {
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
            return name->word;
        }

        entry = entry->cdr;
    } while (entry != NULL);

    printf("Could not find symbol: %d\n", symbol);
    assert(0);

    return NULL; /* For type-checking's sake. */
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
    for (int i = HEAP_SIZE; i >= 0; i--) {
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

static void prepare_execution_context(void) {
    /* IT BEGINS! */
    insert_initial_symbols();

    /* This is the initial binding list. */
    /* There's nothing in it... */
    global = NULL;
}


static int mark_cells(sexpr *cell) {
    if (cell == NULL) {
        return 0;
    }

    cell->reached = true;

    if (cell->type == CONS) {
        return mark_cells(cell->car) + mark_cells(cell->cdr);
    } else {
        return 1;
    }
}

/* TODO: Rewrite this! */
static int mark_all_reachable_cells(void) {
    int count;
    assert(global != NULL);

    count = mark_cells(global);

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
            /* Return the cell to the free list. */
            cell->cdr = next_free_cell;
            next_free_cell = cell->cdr;
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


sexpr *cons(sexpr* car, sexpr* cdr) {
    sexpr *cell = new_cell();
    cell->type = CONS;

    cell->car = car;
    cell->cdr = cdr;

    return cell;
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



static sexpr* eval_form(l_symbol symbol, sexpr *args, sexpr **env);
static sexpr* update_environment(sexpr **env, sexpr* symbol, sexpr *value);

sexpr* eval(sexpr *expr, sexpr **env) {
    sexpr* evaluation;

    if (expr == NULL) {
        return expr;
    }

    if (expr->type == SYMBOL) {
        return assoc(expr->symbol, *env);
    } else if ((expr->type == CONS) && (expr->car->type == SYMBOL)) {
        /* Try to evaluate a special form: */
        return eval_form(expr->car->symbol, expr->cdr, env);
    }
    /* TODO: evlist, and, in fact, everything else. */

    /* Otherwise, it just evaluates to itself. */
    return expr;
}


/* Evaluates a cons cell. */
static sexpr* eval_form(l_symbol symbol, sexpr *args, sexpr **env) {
    sexpr *evaluation;

    /*
     * Evaluate all special forms and built-in functions.
     */
    switch (symbol) {
        case QUOTE:
            evaluation = args->car;
            break;
        case LAMBDA:
            /* Make a lambda... */
            break;
        case LABEL:
            /* I feel like this should be... void. */
            if ((args == NULL)
                    || (args->car == NULL)
                    || (args->cdr == NULL)
                    || (args->cdr->type != CONS)
                    || (args->car->type != SYMBOL)) {
                fprintf(stderr, "Invalid LABEL statement\n");
                longjmp(top_level_exception, EVAL_ERROR);
                return NULL;
            }

            evaluation = eval(args->cdr->car, env);
            update_environment(env, args->car, evaluation);

            break;
        case S_CONS:
            /* TODO: some sort of evaluation for stuff. */
            return cons(
                    eval(args->car, env),
                    eval(args->cdr, env));
            
        case COND:
            //return evcond(args, env);
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

static sexpr* update_environment(sexpr **env, sexpr* symbol, sexpr *value) {
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


/* Returns the first expression that is associated with the symbol in the
 * given environment. */
sexpr* assoc(l_symbol symbol, sexpr *environment) {
    sexpr *current, *pair;

    if (environment == NULL) {
        environment = global;
    }

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

