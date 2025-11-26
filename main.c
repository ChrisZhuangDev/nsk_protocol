#include <stdio.h>
#include <string.h>
#include "lib_protocol.h"
#include <stdint.h>

char *test_data[] = {
    // "1234567890abcdefaa",
    // "@1234567890abcdef*aa",
    // "@@1234567890abcdef*aa",
    // "@12345*@67890abcdef*aa",
    // "@12345@67890abcdef*aa",
    // "@12345*67890abcdef*aa",
    // "@12345@*67890abcdef*aa",
    // "@1234*xx",
    // "@abcd*yy",
    // "@5678*zz",
    "@0107100FAA31F4*6B",
    // 粘包测试用例
    "@0107100FAA31F4*6B@0208200FBB42F5*7C",  // 两个完整包粘连
    "@01071*AB@02082*CD@03093*EF",           // 三个短包粘连
    NULL
};

int main(int argc, char *argv[])
{
    protocol_parser_t parser;
    protocol_parser_init(&parser);
    for (char **p = test_data; *p != NULL; ++p) {
        printf("---- Testing input: %s ----\n", *p);
        protocol_parser_process(&parser, (uint8_t*)*p, strlen(*p));
    }
}