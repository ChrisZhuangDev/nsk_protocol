#include <stdio.h>
#include "lib_protocol.h"


char *test_data[] = {
    "1234567890abcdefaa",
    "@1234567890abcdef*aa",
    "@@1234567890abcdef*aa",
    "@12345*@67890abcdef*aa",
    "@12345@67890abcdef*aa",
    "@12345*67890abcdef*aa",
    "@12345@*67890abcdef*aa",
    "@1234*xx",
    "@abcd*yy",
    "@5678*zz",
    NULL
};

int main(int argc, char *argv[])
{
    protocol_parser_t parser;
    uint8_t i = 0;
    uint8_t *p = test_data;
    protocol_parser_init(&parser);
    for (char **p = test_data; *p != NULL; ++p) {
        printf("---- Testing input: %s ----\n", *p);
        protocol_parser_process(&parser, (uint8_t*)*p, strlen(*p));
    }
}