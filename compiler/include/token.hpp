/*
    Copyright 2025 Carson F. Becker

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/
#ifndef TOKEN_H
#define TOKEN_H
#include "common.hpp"

namespace Token {

enum TokenType {
    START_OF_FILE,
    /*
     ( ) { } [ ] , . - + ; / * & | && || ~ % : @ ? ++ -- << >> # ^
    */
    LEFT_PAREN, RIGHT_PAREN, LEFT_BRACE, RIGHT_BRACE, LEFT_BRACKET, RIGHT_BRACKET,
    COMMA, DOT, MINUS, PLUS, SEMICOLON, SLASH, STAR,
    AMPERSAND, PIPE, DOUBLE_AMPERSAND, DOUBLE_PIPE, 
    TILDE, MODULO, COLON, DOUBLE_COLON, AT, QUESTION, PLUS_PLUS, MINUS_MINUS,
    SHIFT_LEFT, SHIFT_RIGHT, HASH, CARET,

    /*
     ! != = == += -= *= /= -> > >= < <=
    */
    BANG, BANG_EQUAL,
    EQUAL, EQUAL_EQUAL,
    PLUS_EQUAL, MINUS_EQUAL, 
    STAR_EQUAL, SLASH_EQUAL,
    ARROW,
    GREATER, GREATER_EQUAL,
    LESS, LESS_EQUAL,

    IDENTIFIER, SCOPED_IDENTIFER, STRING, CHARACTER, INTEGER, FLOAT, TRUE, FALSE, PREPROCESSOR,

    // Type
    I8, I16, I32, I64, I128, U8, U16, U32, U64, U128,
    F32, F64, F128, D32, D64, D128, UNIT, CHAR,
    MUT, VOLATILE, STATIC, BOOL, BYTE, WORD, DWORD, QWORD, OWORD,

    // Keywords.
    STRUCT, ENUM, IF, ELIF, ELSE, FUN, MTD, FOR, WHILE, GLOBAL,
    LET, RETURN, SELF, IMPORT, EXPORT, DEFINE, UNDEFINE,
    MATCH, SIZEOF, ALIGNOF, AS, USE, BREAK, CONTINUE,

    // Extern calls
    EXTERN, CDECL, STDCALL, THISCALL,

    END_OF_FILE
};

struct Token {
    TokenType type;
    std::string input;
    size_t line;
};

}
#endif