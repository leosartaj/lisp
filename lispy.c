#include <stdio.h>
#include <stdlib.h>

// Helps in making REPL
#include <editline/readline.h>
#include <editline/history.h>

// Including MPC lib
#include "mpc.h"

struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

//making a function pointer
typedef lval*(*lbuiltin)(lenv*, lval*);

struct lval {

    int type;
    long number;

    char* err;
    char* sym;

    lbuiltin fun;
    lenv* env;
    lval* formals;
    lval* body;

    int count;
    struct lval** cell;

};

struct lenv {

    int count;

    lenv* par;

    char** syms;
    lval** vals;
};

enum {LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_FUN, LVAL_SEXPR, LVAL_QEXPR};

lval* lval_num(long x);
lval* lval_err(char* s, ...);
lval* lval_sym(char* s);
lval* lval_fun(lbuiltin fun);
lval* lval_sexpr(void);
lval* lval_qexpr(void);
lval* lval_copy(lval* v);
void lval_del(lval* v);
lenv* lenv_new(void);
lenv* lenv_copy(lenv* v);
void lenv_def(lenv* e, lval* k, lval* v);
void lenv_del(lenv* v);
lval* lenv_get(lenv* e, lval* v);
void lenv_put(lenv* e, lval* k, lval* v);
lval* builtin_add(lenv* e, lval* a);
lval* builtin_mul(lenv* e, lval* a);
lval* builtin_min(lenv* e, lval* a);
lval* builtin_div(lenv* e, lval* a);
void lenv_add_builtin(lenv* e, char* name, lbuiltin fun);
void lenv_add_builtins(lenv* e);
lval* lval_add(lval* v, lval* x);
lval* lval_read_num(mpc_ast_t* t);
lval* lval_read(mpc_ast_t* t);
void lval_print_expr(lval* v, char open, char close);
void lval_println(lval* v);
void lval_print(lval* v);
lval* lval_eval_sexpr(lenv* e, lval* v);
lval* lval_eval(lenv* e, lval* v);
lval* lval_pop(lval* v, int i);
lval* lval_take(lval* v, int i);
lval* builtin_op(lenv* e, lval* v, char* op);
lval* builtin_head(lenv* e, lval* a);
lval* builtin_tail(lenv* e, lval* a);
lval* builtin_list(lenv* e, lval* a);
lval* builtin_eval(lenv* e, lval* a);
lval* builtin_join(lenv* e, lval* a);
lval* lval_join(lval* x, lval* y);
lval* lval_lambda(lval* formals, lval* body);
lval* builtin_var(lenv* e, lval* a, char* func);
lval* builtin_def(lenv* e, lval* a);
lval* builtin_put(lenv* e, lval* a);

//defining a macro for error handling
#define ERR_CHECK(arg, cond, s, ...) \
    if(!(cond)) { \
        lval* err = lval_err(s, ##__VA_ARGS__); \
        lval_del(arg); \
        return err; \
    }; 


int main(int argc, char** argv) {

    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Symbol = mpc_new("symbol");
    mpc_parser_t* Sexpr = mpc_new("sexpr");
    mpc_parser_t* Qexpr = mpc_new("qexpr");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lispy = mpc_new("lispy");

    mpca_lang(MPCA_LANG_DEFAULT, 
            "                                                                       \
            number   : /-?[0-9]+/ ;                                                 \
            symbol   : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ;                           \
            sexpr    : '(' <expr>* ')' ;                                            \
            qexpr    : '{' <expr>* '}' ;                                            \
            expr     : <number> | <symbol> | <sexpr> | <qexpr> ;                    \
            lispy    : /^/ <expr>+ /$/ ;                                            \
            ",         
            Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

    puts("Lispy Version 0.0.1\n");
    puts("Press Ctrl+c to exit\n");

    lenv* e = lenv_new();
    lenv_add_builtins(e);

    while(1) {

        char* input = readline("MyLisp>> ");

        add_history(input);

        mpc_result_t r;

        if(mpc_parse("<stdin>", input, Lispy, &r)) {
            /* On Success Print the Result */
            lval* result = lval_eval(e, lval_read(r.output));
            lval_println(result);
            lval_del(result);

        } else {
            /* Otherwise Print the Error */
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);

    }

    lenv_del(e);

    mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

    return 0;
}

lval* lval_num(long x) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_NUM;
    v->number = x;
    return v;
}

lval* lval_err(char* s, ...) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_ERR;

    va_list list;
    va_start(list, s);

    v->err = malloc(512);

    vsnprintf(v->err, 511, s, list);

    v->err = realloc(v->err, strlen(v->err) + 1);

    va_end(list);

    return v;
}

lval* lval_sym(char* s) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    v->sym = malloc(strlen(s) + 1);
    strcpy(v->sym, s);
    return v;
}

lval* lval_fun(lbuiltin fun) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_FUN;
    v->fun = fun;
    return v;
}

lval* lval_sexpr(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

lval* lval_qexpr(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_QEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

lval* lval_copy(lval* v) {

    lval* x = malloc(sizeof(lval));

    x->type = v->type;

    switch(v->type) {
        case LVAL_NUM:
            x->number = v->number;
            break;
        case LVAL_FUN:
            if(v->fun) {
                x->fun = v->fun;
            } else {
                x->fun = NULL;
                x->formals = lval_copy(v->formals);
                x->body = lval_copy(v->body);
                x->env = lenv_copy(v->env);
            }
            break;
        case LVAL_ERR:
            x->err = malloc(strlen(v->err) + 1);
            strcpy(x->err, v->err);
            break;
        case LVAL_SYM:
            x->sym = malloc(strlen(v->sym) + 1);
            strcpy(x->sym, v->sym);
            break;
        case LVAL_QEXPR:
        case LVAL_SEXPR:
            x->count = v->count;
            x->cell = malloc(sizeof(lval*) * x->count);
            for(int i = 0; i < v->count; i++) {
                x->cell[i] = lval_copy(v->cell[i]);
            }
            break;
    }

    return x;
}

void lval_del(lval* v) {

    switch(v->type) {
        case LVAL_NUM:
            break;
        case LVAL_FUN:
            if(!(v->fun)) {
                lenv_del(v->env);
                lval_del(v->formals);
                lval_del(v->body);
            }
            break;
        case LVAL_ERR:
            free(v->err);
            break;
        case LVAL_SYM:
            free(v->sym);
            break;
        case LVAL_QEXPR:
        case LVAL_SEXPR:

            for(int i = 0; i < v->count; i++) {
                lval_del(v->cell[i]);
            }

            free(v->cell);
            break;
    }

    free(v);

}

lenv* lenv_new(void) {
    lenv* x = malloc(sizeof(lenv));
    x->count = 0;
    x->par = NULL;
    x->syms = NULL;
    x->vals = NULL;
    return x;
}

void lenv_del(lenv* v) {
    for(int i = 0; i < v->count; i++) {
        free(v->syms[i]);
        lval_del(v->vals[i]);
    }
    free(v->syms);
    free(v->vals);
    free(v);
}

lenv* lenv_copy(lenv* v) {
    lenv* x = malloc(sizeof(lenv));
    x->par = v->par;
    x->count = v->count;

    for(int i = 0; i < x->count; i++) {
        x->syms[i] = malloc(strlen(v->syms[i]) + 1);
        strcpy(x->syms[i], v->syms[i]);
        x->vals[i] = lval_copy(v->vals[i]);
    }
    return x;

}

void lenv_def(lenv* e, lval* k, lval* v) {

    while(e->par) {
        e = e->par;
    }

    lenv_put(e, k, v);
}

lval* lenv_get(lenv* e, lval* v) {

    for(int i = 0; i < e->count; i++) {
        if(strcmp(e->syms[i], v->sym) == 0) {
            return lval_copy(e->vals[i]);
        }
    }

    if(e->par) {
        return lenv_get(e->par, v);
    } else {
        return lval_err("Unbound Symbol '%s'", v->sym);
    }

}

void lenv_put(lenv* e, lval* k, lval* v) {

    for(int i = 0; i < e->count; i++) {

        if(strcmp(e->syms[i], k->sym) == 0) {
            lval_del(e->vals[i]);
            e->vals[i] = lval_copy(v);
            return;
        }
    }

    e->count++;
    e->vals = realloc(e->vals, sizeof(lval*) * e->count);
    e->syms = realloc(e->syms, sizeof(char*) * e->count);

    e->vals[e->count - 1] = lval_copy(v);
    e->syms[e->count - 1] = malloc(strlen(k->sym) + 1);
    strcpy(e->syms[e->count - 1], k->sym);
}

lval* builtin_add(lenv* e, lval* a) {
    return builtin_op(e, a, "+");
}
lval* builtin_mul(lenv* e, lval* a) {
    return builtin_op(e, a, "*");
}
lval* builtin_min(lenv* e, lval* a) {
    return builtin_op(e, a, "-");
}
lval* builtin_div(lenv* e, lval* a) {
    return builtin_op(e, a, "/");
}

void lenv_add_builtin(lenv* e, char* name, lbuiltin fun) {
    lval* k = lval_sym(name);
    lval* f = lval_fun(fun);
    lenv_put(e, k, f);
    lval_del(k);
    lval_del(f);
}

void lenv_add_builtins(lenv* e) {

    lenv_add_builtin(e, "list", builtin_list);
    lenv_add_builtin(e, "head", builtin_head);
    lenv_add_builtin(e, "tail", builtin_tail);
    lenv_add_builtin(e, "join", builtin_join);
    lenv_add_builtin(e, "eval", builtin_eval);

    lenv_add_builtin(e, "+", builtin_add);
    lenv_add_builtin(e, "-", builtin_min);
    lenv_add_builtin(e, "/", builtin_div);
    lenv_add_builtin(e, "*", builtin_mul);

    lenv_add_builtin(e, "def", builtin_def);
    lenv_add_builtin(e, "=", builtin_put);
}

lval* lval_add(lval* v, lval* x) {
    v->count++;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    v->cell[v->count - 1] = x;
    return v;
}

lval* lval_read_num(mpc_ast_t* t) {
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ? lval_num(x) : lval_err("Invalid Number");
}

lval* lval_read(mpc_ast_t* t) {
    if(strstr(t->tag, "number")) {
        return lval_read_num(t);
    }
    if(strstr(t->tag, "symbol")) {
        return lval_sym(t->contents);
    }

    lval* x = NULL;
    if(strcmp(t->tag, ">") == 0) {
        x = lval_sexpr();
    }
    if(strstr(t->tag, "sexpr")) {
        x = lval_sexpr();
    }
    if(strstr(t->tag, "qexpr")) {
        x = lval_qexpr();
    }

    for(int i = 0; i < t->children_num; i++) {
        if(strcmp(t->children[i]->contents, "(") == 0) {
            continue;
        }
        if(strcmp(t->children[i]->contents, ")") == 0) {
            continue;
        }
        if(strcmp(t->children[i]->contents, "{") == 0) {
            continue;
        }
        if(strcmp(t->children[i]->contents, "}") == 0) {
            continue;
        }
        if(strcmp(t->children[i]->tag, "regex") == 0) {
            continue;
        }
        x = lval_add(x, lval_read(t->children[i]));
    }
    return x;
}

void lval_print_expr(lval* v, char open, char close) {

    putchar (open);

    for(int i = 0; i < v->count; i++) {
        lval_print(v->cell[i]);

        if(i != v->count - 1) {
            putchar(' ');
        }

    }
    putchar(close);

}

void lval_println(lval* v) {
    lval_print(v);
    putchar('\n');
}

void lval_print(lval* v) {

    switch(v->type) {
        case LVAL_NUM:
            printf("%ld", v->number);
            break;
        case LVAL_ERR:
            printf("Error: %s", v->err);
            break;
        case LVAL_SYM:
            printf("%s", v->sym);
            break;
        case LVAL_FUN:
            if(v->fun) {
                printf("<builtin>");
            } else {
                printf("(//");
                putchar(' ');
                lval_print(v->formals);
                putchar(' ');
                lval_print(v->body);
                putchar(')');
            }
            break;
        case LVAL_SEXPR:
            lval_print_expr(v, '(', ')');
            break;
        case LVAL_QEXPR:
            lval_print_expr(v, '{', '}');
            break;
    }
}

lval* lval_eval_sexpr(lenv* e, lval* v) {

    for(int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(e, v->cell[i]);
        if(v->cell[i]->type == LVAL_ERR) {
            return lval_take(v, i);
        }
    }

    if(v->count == 0) {
        return v;
    }

    if(v->count == 1) {
        return lval_take(v, 0);
    }

    lval* x = lval_pop(v, 0);

    if(x->type != LVAL_FUN) {
        lval_del(x);
        lval_del(v);
        return lval_err("first argument is not a function");
    }

    lval* result = x->fun(e, v);
    lval_del(x);
    return result;

}

lval* lval_eval(lenv* e, lval* v) {

    if(v->type == LVAL_SYM) {
        lval* x = lenv_get(e, v);
        lval_del(v);
        return x;
    }
    if(v->type == LVAL_SEXPR) {
        return lval_eval_sexpr(e, v);
    }
    return v;
}

lval* lval_pop(lval* v, int i) {
    lval* x = v->cell[i];

    memmove(&v->cell[i], &v->cell[i + 1], sizeof(lval*) * (v->count - i - 1));

    v->count--;

    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    return x;

}

lval* lval_take(lval* v, int i) {
    lval* x = lval_pop(v, i);
    lval_del(v);
    return x;
}

lval* builtin_op(lenv* e, lval* v, char* op) {

    for(int i = 0; i < v->count; i++) {
        if(v->cell[i]->type != LVAL_NUM) {
            lval_del(v);
            return lval_err("Cannot operate on non-numbers");
        }
    }

    lval* x = lval_pop(v, 0);

    if(strcmp(op, "-") == 0 && v->count == 0) {
        x->number = -x->number;
    }

    while(v->count > 0) {

        lval* y = lval_pop(v, 0);

        if(strcmp(op, "+") == 0) {
            x->number += y->number;
        }
        if(strcmp(op, "-") == 0) {
            x->number -= y->number;
        }
        if(strcmp(op, "*") == 0) {
            x->number *= y->number;
        }
        if(strcmp(op, "/") == 0) {
            if(y->number == 0) {
                lval_del(y);
                lval_del(x);
                lval* x = lval_err("Division with zero");
                break;
            }
            x->number /= y->number;
        }
        lval_del(y);
    }

    lval_del(v);

    return x;
}

lval* builtin_head(lenv* e, lval* a) {

    ERR_CHECK(a, (a->count == 1), "Function head passed  '%d' arguments, expecting '%d'", a->count, 1);
    ERR_CHECK(a, (a->cell[0]->type == LVAL_QEXPR), "Function head passed correct type of argument");
    ERR_CHECK(a, (a->count != 0), "Function head passed {}");

    lval* v = lval_take(a, 0);

    while(v->count > 1) {
        lval_del(lval_pop(v, 1));
    }

    return v;
}

lval* builtin_tail(lenv* e, lval* a) {

    ERR_CHECK(a, (a->count == 1), "Function tail passed '%d' arguments, expecting '%d'", a->count, 1);
    ERR_CHECK(a, (a->cell[0]->type == LVAL_QEXPR), "Function tail not passed correct type of argument");
    ERR_CHECK(a, (a->count != 0), "Function tail passed {}");


    lval*v = lval_take(a, 0);

    lval_del(lval_pop(v, 0));

    return v;

}

lval* builtin_list(lenv* e, lval* a) {
    a->type = LVAL_QEXPR;

    return a;

}

lval* builtin_eval(lenv* e, lval* a) {
    ERR_CHECK(a, (a->count == 1), "Function eval passed '%d' arguments, expecting '%d'", a->count, 1);
    ERR_CHECK(a, (a->cell[0]->type == LVAL_QEXPR), "Function eval passed invaild arguments");

    lval* x = lval_take(a, 0);

    x->type = LVAL_SEXPR;

    return lval_eval(e, x);

}

lval* builtin_join(lenv* e, lval* a) {

    for(int i = 0; i < a->count; i++) {
        ERR_CHECK(a, (a->cell[i]->type == LVAL_QEXPR), "Function join passed incorrect types");
    }

    lval* x = lval_pop(a, 0);

    while(a->count) {

        x = lval_join(x, lval_pop(a, 0));

    }

    lval_del(a);
    return x;

}

lval* lval_join(lval* x, lval* y) {

    while(y->count) {
        lval_add(x, lval_pop(y, 0));
    }

    lval_del(y);
    return x;

}

lval* lval_lambda(lval* formals, lval* body) {

    lval* v =  malloc(sizeof(lval));
    v->type = LVAL_FUN;

    v->fun = NULL;

    v->env = lenv_new();

    v->formals = formals;
    v->body = body;

    return v;
}

lval* builtin_lambda(lenv* e, lval* a) {
    /* Check Two arguments, each of which are Q-Expressions 
       LASSERT_NUM("\\", a, 2);
       LASSERT_TYPE("\\", a, 0, LVAL_QEXPR);
       LASSERT_TYPE("\\", a, 1, LVAL_QEXPR);

       Check first Q-Expression contains only Symbols
       for (int i = 0; i < a->cell[0]->count; i++) {
       ERR_CHECK(a, (a->cell[0]->cell[i]->type == LVAL_SYM),
       "Cannot define non-symbol. Got %s, Expected %s.",
       ltype_name(a->cell[0]->cell[i]->type), ltype_name(LVAL_SYM));
       }
       */

    /* Pop first two arguments and pass them to lval_lambda */
    lval* formals = lval_pop(a, 0);
    lval* body = lval_pop(a, 0);
    lval_del(a);

    return lval_lambda(formals, body);
}

lval* builtin_var(lenv* e, lval* a, char* func) {

    lval* syms = a->cell[0];

    for(int i = 0; i < syms->count; i++) {
        if(strcmp("def", func) == 0) {
            lenv_def(e, syms->cell[i], a->cell[i + 1]);
        }
        if(strcmp("=", func) == 0) {
            lenv_put(e, syms->cell[i], a->cell[i + 1]);
        } 
    }

    lval_del(a);
    return lval_sexpr();

}

lval* builtin_def(lenv* e, lval* a) {
    return builtin_var(e, a, "def");
}

lval* builtin_put(lenv* e, lval* a) {
    return builtin_var(e, a, "=");
}


