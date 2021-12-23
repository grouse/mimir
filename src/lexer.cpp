#include "lexer.h"

const char* token_type_str(TokenType type)
{
    static char c[2] = { 0, 0 };

    switch (type) {
    case TOKEN_IDENTIFIER: return "IDENTIFIER";
    case TOKEN_INTEGER: return "INTEGER";
    case TOKEN_NUMBER: return "NUMBER";
    case TOKEN_EOF: return "EOF";
    default:
        c[0] = (char)type;
        return &c[0];
    }
}

Token next_token(Lexer *lexer)
{
    while (lexer->at < lexer->end) {
        if (*lexer->at == ' ' || *lexer->at == '\t') {
            lexer->at++;
        } else if (*lexer->at == '\n' || *lexer->at == '\r') {
            lexer->line_number++;
            if (*lexer->at == '\r') lexer->at++;
            if (*lexer->at == '\n') lexer->at++;
        } else if ((*lexer->at >= '0' && *lexer->at <= '9') ||
                   (lexer->at+1 < lexer->end && *lexer->at == '-' && *(lexer->at+1) >= '0' && *(lexer->at+1) <= '9'))
        {
            Token t;
            t.type = TOKEN_INTEGER;
            t.str.data = lexer->at++;

            while (lexer->at < lexer->end) {
                if (t.type == TOKEN_INTEGER && *lexer->at == '.') {
                    t.type = TOKEN_NUMBER;
                } else if (*lexer->at > '9' || *lexer->at < '0') break;
                lexer->at++;
            }

            t.str.length = (i32)(lexer->at - t.str.data);
            return t;
        } else if ((*lexer->at >= 'a' && *lexer->at <= 'z') ||
                   (*lexer->at >= 'A' && *lexer->at <= 'Z') ||
                   (u8)(*lexer->at) > 127)
        {
            Token t;
            t.type = TOKEN_IDENTIFIER;
            t.str.data = lexer->at++;

            while (lexer->at < lexer->end) {
                if ((*lexer->at < 'a' || *lexer->at > 'z') &&
                    (*lexer->at < 'A' || *lexer->at > 'Z') &&
                    (*lexer->at < '0' || *lexer->at > '9') &&
                    *lexer->at != '_' && (u8)(*lexer->at) <= 127)
                {
                    break;
                }

                lexer->at++;
            }

            t.str.length = (i32)(lexer->at - t.str.data);
            return t;
        } else {
            Token t;
            t.type = (TokenType)*lexer->at;
            t.str = { lexer->at, 1 };
            lexer->at++;
            return t;
        }
    }

    Token t{};
    t.type = TOKEN_EOF;
    return t;
}

Token peek_token(Lexer *lexer)
{
    Lexer copy = *lexer;
    return next_token(&copy);
}

bool require_next_token(Lexer *lexer, TokenType type, Token *t)
{
    *t = next_token(lexer);
    if (t->type != type) {
        PARSE_ERROR(lexer, "unexpected token: expected '%s', got '%s'", token_type_str(type), token_type_str(t->type));
        return false;
    }
    return true;

}

bool require_next_token(Lexer *lexer, char c, Token *t)
{
    *t = next_token(lexer);
    if (t->type != (TokenType)c) {
        PARSE_ERROR(lexer, "unexpected token: expected '%c', got '%s'", c, token_type_str(t->type));
        return false;
    }
    return true;
}