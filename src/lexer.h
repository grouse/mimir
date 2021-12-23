#ifndef LEXER_H
#define LEXER_H

#include "platform.h"
#include "string.h"
#include "core.h"

#define PARSE_ERROR(lexer, fmt, ...) LOG_ERROR("parse error: %.*s:%d: " fmt, STRFMT((lexer)->debug_name), (lexer)->line_number, ##__VA_ARGS__)

struct Lexer {
    char *at;
    char *end;
    String debug_name;
    i32 line_number = 1;
};

enum TokenType : u8 {
    TOKEN_START = 127, // NOTE(jesper): 0-127 reserved for ascii token values
    TOKEN_IDENTIFIER,
    TOKEN_INTEGER,
    TOKEN_NUMBER,
    TOKEN_EOF,
};

struct Token {
    TokenType type;
    String str;
};


#endif // LEXER_H
