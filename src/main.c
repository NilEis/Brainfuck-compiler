#include <stdio.h>
#include <unistd.h>
#include "include/transpiler.h"
#include "include/compiler.h"

int main(int argc, char *const *argv)
{
	char *bf_source = NULL;
	char *output = "bf_to_exec.exe";
	int generate_c = 0;
	int use_gcc = 0;
	int only_transpile = 0;
	int log = 0;
	int c = 0;
	while ((c = getopt(argc, argv, "-o:cgthl")) != -1)
	{
		switch (c)
		{
		case '\1':
			bf_source = optarg;
			break;
		case 'o':
			output = optarg;
			break;
		case 'c':
			generate_c = 1;
			break;
		case 'g':
			use_gcc = 1;
			break;
		case 'l':
			log++;
			break;
		case 't':
			generate_c = 1;
			only_transpile = 1;
			break;
		case 'h':
			printf("file2c [-h] [-v] [-q] [-o <file>] <input>\n-h: Display this information\n-v: Provide additional details\n-q: Deactivates all warnings/errors\n-o <file>: Place the output into <file>");
			return 0;
			break;
		}
	}
	if (bf_source == NULL)
	{
		printf("No input\n");
	}
	char *source = transpiler_get_c(bf_source);
	if (source == NULL)
	{
		exit(-1);
	}
	FILE *c_source = fopen("c_source", "w");
	fprintf(c_source, source);
	fclose(c_source);
	if (!only_transpile)
	{
		if (use_gcc)
		{
			compiler_compile_gcc(output, "c_source");
		}
		else
		{
			compiler_compile(bf_source, output, log != 0, log > 1, log > 2);
		}
	}
	if (!generate_c)
	{
		remove("c_source");
	}
	return 0;
}
