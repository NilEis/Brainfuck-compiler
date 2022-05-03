#include "include/compiler.h"
#include <libgccjit.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MAX_OPEN_PARENS 500

typedef struct bf_compiler
{
    gcc_jit_context *ctxt;

    gcc_jit_type *void_t;
    gcc_jit_type *int_t;
    gcc_jit_type *byte_t;
    gcc_jit_type *array_t;

    gcc_jit_function *getchar;
    gcc_jit_function *putchar;

    gcc_jit_function *func;
    gcc_jit_block *curblock;

    gcc_jit_rvalue *int_zero;
    gcc_jit_rvalue *int_one;
    gcc_jit_rvalue *byte_zero;
    gcc_jit_rvalue *byte_one;
    gcc_jit_lvalue *cells;
    gcc_jit_lvalue *idx;

    int num_open_parens;
    gcc_jit_block *paren_test[MAX_OPEN_PARENS];
    gcc_jit_block *paren_body[MAX_OPEN_PARENS];
    gcc_jit_block *paren_after[MAX_OPEN_PARENS];

} bf_compiler;

static gcc_jit_lvalue *get_data(bf_compiler bfc);

int compiler_compile_gcc(const char *name, const char *input)
{
    char *cmd = (char *)calloc(strlen(input) + strlen(name) + 15, sizeof(char));
    sprintf(cmd, "gcc -static -o bf_%s -x c %s -s\n", name, input);
    return system(cmd);
}

int compiler_compile(const char *bf_source, const char *output_name, int LOG_C, int LOG_ASM, int LOG_S)
{
    FILE *source = fopen(bf_source, "r");
    char ch = 0;
    bf_compiler bfc;
    memset(&bfc, 0, sizeof(bf_compiler));
    bfc.ctxt = gcc_jit_context_acquire();
    gcc_jit_context_set_int_option(bfc.ctxt, GCC_JIT_INT_OPTION_OPTIMIZATION_LEVEL, 3);
    gcc_jit_context_set_bool_option(bfc.ctxt, GCC_JIT_BOOL_OPTION_DUMP_GENERATED_CODE, LOG_ASM);
    gcc_jit_context_set_bool_option(bfc.ctxt, GCC_JIT_BOOL_OPTION_DUMP_INITIAL_GIMPLE, LOG_C);
    gcc_jit_context_set_bool_option(bfc.ctxt, GCC_JIT_BOOL_OPTION_DUMP_INITIAL_TREE, 0);
    
    gcc_jit_context_set_bool_option(bfc.ctxt, GCC_JIT_BOOL_OPTION_DUMP_SUMMARY, LOG_S);
    gcc_jit_context_set_bool_option(bfc.ctxt, GCC_JIT_BOOL_OPTION_DEBUGINFO, 0);
    gcc_jit_context_set_bool_option(bfc.ctxt, GCC_JIT_BOOL_OPTION_KEEP_INTERMEDIATES, 0);

    //Init types
    bfc.void_t = gcc_jit_context_get_type(bfc.ctxt, GCC_JIT_TYPE_VOID);
    bfc.int_t = gcc_jit_context_get_type(bfc.ctxt, GCC_JIT_TYPE_INT);
    bfc.byte_t = gcc_jit_context_get_type(bfc.ctxt, GCC_JIT_TYPE_UNSIGNED_CHAR);
    bfc.array_t = gcc_jit_context_new_array_type(bfc.ctxt, NULL, bfc.byte_t, 30000);

    //Init functions
    bfc.getchar = gcc_jit_context_new_function(bfc.ctxt, NULL, GCC_JIT_FUNCTION_IMPORTED, bfc.int_t, "getchar", 0, NULL, 0);
    gcc_jit_param *param_c = gcc_jit_context_new_param(bfc.ctxt, NULL, bfc.int_t, "c");
    bfc.putchar = gcc_jit_context_new_function(bfc.ctxt, NULL, GCC_JIT_FUNCTION_IMPORTED, bfc.void_t, "putchar", 1, &param_c, 0);

    //init main
    gcc_jit_param *param_argc = gcc_jit_context_new_param(bfc.ctxt, NULL, bfc.int_t, "argc");
    gcc_jit_type *char_ptr_ptr_type = gcc_jit_type_get_pointer(gcc_jit_type_get_pointer(gcc_jit_context_get_type(bfc.ctxt, GCC_JIT_TYPE_CHAR)));
    gcc_jit_param *param_argv = gcc_jit_context_new_param(bfc.ctxt, NULL, char_ptr_ptr_type, "argv");
    gcc_jit_param *params[2] = {param_argc, param_argv};
    bfc.func = gcc_jit_context_new_function(bfc.ctxt, NULL, GCC_JIT_FUNCTION_EXPORTED, bfc.int_t, "main", 2, params, 0);

    bfc.curblock = gcc_jit_function_new_block(bfc.func, "initial");

    bfc.int_zero = gcc_jit_context_zero(bfc.ctxt, bfc.int_t);
    bfc.int_one = gcc_jit_context_one(bfc.ctxt, bfc.int_t);

    bfc.byte_zero = gcc_jit_context_zero(bfc.ctxt, bfc.byte_t);
    bfc.byte_one = gcc_jit_context_one(bfc.ctxt, bfc.byte_t);

    bfc.cells = gcc_jit_context_new_global(bfc.ctxt, NULL, GCC_JIT_GLOBAL_INTERNAL, bfc.array_t, "cells");

    bfc.idx = gcc_jit_function_new_local(bfc.func, NULL, bfc.int_t, "idx");
    gcc_jit_block_add_assignment(bfc.curblock, NULL, bfc.idx, bfc.int_zero);
    while ((ch = getc(source)) != EOF)
    {
        switch (ch)
        {
        case '>':
            gcc_jit_block_add_assignment_op(bfc.curblock, NULL, bfc.idx, GCC_JIT_BINARY_OP_PLUS, bfc.int_one);
            break;
        case '<':
            gcc_jit_block_add_assignment_op(bfc.curblock, NULL, bfc.idx, GCC_JIT_BINARY_OP_MINUS, bfc.int_one);
            break;
        case '+':
            gcc_jit_block_add_assignment_op(bfc.curblock, NULL, get_data(bfc), GCC_JIT_BINARY_OP_PLUS, bfc.byte_one);
            break;
        case '-':
            gcc_jit_block_add_assignment_op(bfc.curblock, NULL, get_data(bfc), GCC_JIT_BINARY_OP_MINUS, bfc.byte_one);
            break;
        case '.':
        {
            gcc_jit_rvalue *arg = gcc_jit_context_new_cast(bfc.ctxt, NULL, gcc_jit_lvalue_as_rvalue(get_data(bfc)), bfc.int_t);
            gcc_jit_rvalue *call = gcc_jit_context_new_call(bfc.ctxt, NULL, bfc.putchar, 1, &arg);
            gcc_jit_block_add_eval(bfc.curblock, NULL, call);
        }
        break;
        case ',':
        {
            gcc_jit_rvalue *call = gcc_jit_context_new_call(bfc.ctxt, NULL, bfc.getchar, 0, NULL);
            gcc_jit_block_add_assignment(bfc.curblock, NULL, get_data(bfc), gcc_jit_context_new_cast(bfc.ctxt, NULL, call, bfc.byte_t));
        }
        break;
        case '[':
        {
            gcc_jit_block *loop_test = gcc_jit_function_new_block(bfc.func, NULL);
            gcc_jit_block *on_zero = gcc_jit_function_new_block(bfc.func, NULL);
            gcc_jit_block *on_non_zero = gcc_jit_function_new_block(bfc.func, NULL);

            if (bfc.num_open_parens == MAX_OPEN_PARENS)
            {
                abort();
            }

            gcc_jit_block_end_with_jump(bfc.curblock, NULL, loop_test);
            gcc_jit_block_end_with_conditional(loop_test, NULL, gcc_jit_context_new_comparison(bfc.ctxt, NULL, GCC_JIT_COMPARISON_EQ, gcc_jit_lvalue_as_rvalue(get_data(bfc)), bfc.byte_zero), on_zero, on_non_zero);
            bfc.paren_test[bfc.num_open_parens] = loop_test;
            bfc.paren_body[bfc.num_open_parens] = on_non_zero;
            bfc.paren_after[bfc.num_open_parens] = on_zero;
            bfc.num_open_parens += 1;
            bfc.curblock = on_non_zero;
        }
        break;
        case ']':
        {
            if (bfc.num_open_parens == 0)
            {
                abort();
            }
            bfc.num_open_parens -= 1;
            gcc_jit_block_end_with_jump(bfc.curblock, NULL, bfc.paren_test[bfc.num_open_parens]);
            bfc.curblock = bfc.paren_after[bfc.num_open_parens];
        }
        break;
        default:
            continue;
            break;
        }
    }
    gcc_jit_block_end_with_return(bfc.curblock, NULL, bfc.int_zero);
    gcc_jit_context_compile_to_file(bfc.ctxt, GCC_JIT_OUTPUT_KIND_EXECUTABLE, output_name);
    fclose(source);
    gcc_jit_context_release(bfc.ctxt);
    return 0;
}

static gcc_jit_lvalue *get_data(bf_compiler bfc)
{
    return gcc_jit_context_new_array_access(bfc.ctxt, NULL, gcc_jit_lvalue_as_rvalue(bfc.cells), gcc_jit_lvalue_as_rvalue(bfc.idx));
}