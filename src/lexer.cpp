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


bool parse_version_decl(Lexer *lexer, i32 *version_out, i32 max_version, Token *t)
{
    if (!require_next_token(lexer, '#', t)) return false;
    if (!require_next_token(lexer, TOKEN_IDENTIFIER, t)) return false;

    if (t->str != "version") {
        PARSE_ERROR(lexer, "expected identifier 'version', got '%.*s'", STRFMT(t->str));
        return false;
    }

    Token version_token;
    if (!require_next_token(lexer, TOKEN_INTEGER, &version_token)) return false;
    i32 version = i32_from_string(version_token.str);

    if (version < 0) {
        PARSE_ERROR(lexer, "version cannot be a negative number");
        return false;
    }

    if (version > max_version) {
        PARSE_ERROR(lexer, "version (%d) is higher than currently supported (%d)", version, max_version);
        return false;
    }

    *version_out = version;
    return true;
}

bool parse_vector2(Lexer *lexer, Token *t, Vector2 *position)
{
    Token x_tok, y_tok;
    if (!require_next_token(lexer, TOKEN_NUMBER, &x_tok)) return false;
    if (!require_next_token(lexer, TOKEN_NUMBER, &y_tok)) return false;
    if (!require_next_token(lexer, ';', t)) return false;
    if (!f32_from_string(x_tok.str, &position->x)) return false;
    if (!f32_from_string(y_tok.str, &position->y)) return false;
    return true;
}