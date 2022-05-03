#ifndef COMPILER_H
#define COMPILER_H

int compiler_compile_gcc(const char *name, const char *input);

int compiler_compile(const char *bf_source, const char *output_name, int LOG_C, int LOG_ASM, int LOG_S);

#endif // COMPILER_H
