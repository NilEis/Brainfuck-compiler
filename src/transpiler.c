#include "include/transpiler.h"
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

static char *source_c = NULL;

char *transpiler_get_c(const char *source_str)
{
    char ch;
    size_t size = 0;
    FILE *source = fopen(source_str, "r");
    FILE *tmp = tmpfile();
    if (source == NULL)
    {
        return NULL;
    }
    fprintf(tmp,
            "#include <stdio.h>\n \
    \n \
    static char array[30000] = { 0 };\n \
    static char *data_pointer = array;\n \
    \n \
    int main(int argc, char **argv)\n \
    {\n \
    ");
    while ((ch = getc(source)) != EOF)
    {
        switch (ch)
        {
        case '<':
            fprintf(tmp, "\tdata_pointer--;\n");
            break;
        case '>':
            fprintf(tmp, "\tdata_pointer++;\n");
            break;
        case '+':
            fprintf(tmp, "\t(*data_pointer)++;\n");
            break;
        case '-':
            fprintf(tmp, "\t(*data_pointer)--;\n");
            break;
        case '.':
            fprintf(tmp, "\tputchar(*data_pointer);\n");
            break;
        case ',':
            fprintf(tmp, "\t*data_pointer = getchar();\n");
            break;
        case '[':
            fprintf(tmp, "\twhile(*data_pointer)\n\t{\n");
            break;
        case ']':
            fprintf(tmp, "\t}\n");
            break;
        default:
            if (isalnum(ch))
            {
                fprintf(tmp, "\t/* Invalid command: %c */\n", ch);
            }
            else if (ch == '\n' || ch == '\t' || ch == '\r')
            {
                //ignore
                continue;
            }
            else
            {
                fprintf(tmp, "\t/* Invalid command: [%d] */\n", ch);
            }
            continue;
            break;
        }
    }
    fprintf(tmp, "}\n");
    fwrite("\0", sizeof(char), 1, tmp);
    size = ftell(tmp);
    source_c = (char *)malloc(size * sizeof(char));
    rewind(tmp);
    fread(source_c, sizeof(char), size, tmp);
    fclose(source);
    return source_c;
}