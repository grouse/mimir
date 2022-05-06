
"#define" @preproc
"#elif" @preproc
"#else" @preproc
"#endif" @preproc
"#if" @preproc
"#ifdef" @preproc
"#ifndef" @preproc
"#include" @preproc
(preproc_directive) @preproc

(nullptr) @constant
(number_literal) @constant
(char_literal) @constant

(raw_string_literal) @string
(string_literal) @string
(system_lib_string) @string

"--" @operator
"-=" @operator
"->" @operator
"=" @operator
"!=" @operator
"*" @operator
"&&" @operator
"++" @operator
"+=" @operator
"==" @operator
"||" @operator
"<<" @operator
">>" @operator

"&" @operator
"-" @operator
"+" @operator
">" @operator
"<" @operator


"break" @keyword
"case" @keyword
"const" @keyword
"continue" @keyword
"default" @keyword
"do" @keyword
"else" @keyword
"enum" @keyword
"extern" @keyword
"for" @keyword
"if" @keyword
"inline" @keyword
"return" @keyword
"sizeof" @keyword
"static" @keyword
"struct" @keyword
"switch" @keyword
"typedef" @keyword
"union" @keyword
"volatile" @keyword
"while" @keyword
(this) @keyword
(auto) @keyword

(comment) @comment