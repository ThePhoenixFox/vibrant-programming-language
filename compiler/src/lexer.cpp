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
#include "passes.hpp"
#include "token.hpp"
#include <bit>

namespace Passes {

static thread_local const char* start;
static thread_local const char* current;
static thread_local size_t line;
static thread_local TokenStream tokenStream;
static thread_local Error::ErrorResult* errors;

static bool isAlpha(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool isBinaryDigit(char c) 
{
    return c == '0' || c == '1';
}

static bool isOctoDigit(char c)
{
    return isBinaryDigit(c) || (c >= '2' && c <= '7');
}

static bool isDigit(char c)
{
    return isOctoDigit(c) || c == '8' || c == '9';
}

static bool isHexDigit(char c) 
{
    return isDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static bool isAlphanumeric(char c)
{
    return isAlpha(c) || isDigit(c);
}
    
static std::string curToken()
{
    return std::string(start,current - start);
}

static void makeCustomToken(Token::TokenType type, std::string val)
{
    tokenStream.tokens.emplace_back(Token::Token{
        type,
        val,
        line
    });
}

static void makeToken(Token::TokenType type)
{
    makeCustomToken(type,curToken());
}

static void beginNewToken()
{
    start = current;
}

static char advance()
{
    return *current++;
}

static char peek()
{
    return *current;
}
    
static bool reachedEnd()
{
    return *current == '\0';
}

static char peekNext()
{
    if(reachedEnd())
        return '\0';

    return current[1];
}
    
static bool match(char expected)
{
    if(reachedEnd())
        return false;
    
    if(*current != expected)
        return false;

    current++;
    return true;
}


static void skipToWhitespace()
{
    while(!reachedEnd())
    {
        char c = peek();
        switch(c) {
            case '\n':
            case ' ':
            case '\t':
                return;
            case '/':
                if(peekNext() == '/' || peekNext() == '*')
                    return;
            default:
            advance();
        }
    }
}

static void skipWhitespace()
{
    while(!reachedEnd())
    {
        char c = peek();
        switch (c) {
            case '\n':
                line++;
                advance();
                break;
            case ' ':
            case '\r':
            case '\t':
                advance();
                break;
            case '/':
                if (peekNext() == '/') {
                    while (peek() != '\n' && !reachedEnd())
                        advance();
                } else if (peekNext() == '*') {
                    while((peek() != '*' && peekNext() != '/') && !reachedEnd())
                        if(advance() == '\n')
                            line++;
                    
                    if(!reachedEnd()) {
                        advance();
                        advance();
                    }

                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

static void skipAlphanumeric()
{
    while(isAlphanumeric(peek()) && !reachedEnd())
        advance();
}
    
static Token::TokenType checkKeyword(std::string keyword,Token::TokenType type)
{
    size_t length = keyword.size();
    size_t identifierLength = current - start;
    if (identifierLength == length && std::memcmp(start, keyword.c_str(), length) == 0)
        return type;

    return Token::TokenType::IDENTIFIER;
}
    
static Token::TokenType identifierType()
{
    size_t len = static_cast<size_t>(current - start);
    switch (*start)
    {
    case 'a':
        return checkKeyword("alignof"s,Token::TokenType::ALIGNOF);
    case 't':
        if(len == 8)
            return checkKeyword("thiscall"s,Token::TokenType::THISCALL);
        return checkKeyword("true"s,Token::TokenType::TRUE);
    case 'i':
        // i8 i16 i32 i64 i128 import
        if (len >= 2) {
            switch (start[1]) {
                case '8':   
                return checkKeyword("i8"s,   Token::TokenType::I8);
                case '1':   
                if((len == 3) && start[2] == '6')
                    return checkKeyword("i16"s,  Token::TokenType::I16);
                return checkKeyword("i128"s, Token::TokenType::I128);
                case '3':
                return checkKeyword("i32"s,  Token::TokenType::I32);
                case '6':
                return checkKeyword("i64"s,  Token::TokenType::I64); 
                default:
            }
        }
        return checkKeyword("import"s, Token::TokenType::IMPORT);

    case 'u':
        // undef unit u8 u16 u32 u64 u128
        if (len >= 2) {
            switch (start[1]) {
                case '8':
                return checkKeyword("u8"s,   Token::TokenType::U8);
                case '1':
                if((len == 3) && start[2] == '6')
                    return checkKeyword("u16"s,  Token::TokenType::U16);
                return checkKeyword("u128"s, Token::TokenType::U128);
                case '3':   
                return checkKeyword("u32"s,  Token::TokenType::U32);
                case '6':   
                return checkKeyword("u64"s,  Token::TokenType::U64);
                default:
            }
        }
        if ((len == 5) && start[2] == 'd')
            return checkKeyword("undef"s, Token::TokenType::UNDEFINE);
        if(len == 4)
            return checkKeyword("unit"s, Token::TokenType::UNIT);
        return checkKeyword("use"s,Token::TokenType::USE);

    case 'f':
        // f32 f64 f128 fun for
        if (len >= 2) {
            switch (start[1]) {
                case '3': 
                return checkKeyword("f32"s, Token::TokenType::F32);
                case '6': 
                return checkKeyword("f64"s, Token::TokenType::F64);
                case '1': 
                return checkKeyword("f128"s, Token::TokenType::F128);
                default:
            }
        }
        if (len == 3) {
            if(start[1] == 'u')
                return checkKeyword("fun"s, Token::TokenType::FUN);
            return checkKeyword("for"s, Token::TokenType::FOR);
        }
        return checkKeyword("false"s,Token::TokenType::FALSE);

    case 'd':
        // def d32 d64 d128 dword
        if ((len == 3) && start[1] == 'e')
            return checkKeyword("def"s, Token::TokenType::DEFINE);

        if (len >= 2) {
            switch (start[1]) {
                case '3': 
                return checkKeyword("d32"s, Token::TokenType::D32);
                case '6': 
                return checkKeyword("d64"s, Token::TokenType::D64);
                case '1': 
                return checkKeyword("d128"s, Token::TokenType::D128);
                default:
            }
        }
        return checkKeyword("dword"s, Token::TokenType::DWORD);

    case 'c':
        // char
        if(len == 5)
            return checkKeyword("cdecl"s,Token::TokenType::CDECL);
        if(len == 4)
            return checkKeyword("char"s, Token::TokenType::CHAR);
        return checkKeyword("continue"s,Token::TokenType::CONTINUE);

    case 's':
        if(len == 6) {
            switch(start[3])
            {
                case 't':
                    return checkKeyword("static"s,Token::TokenType::STATIC);
                case 'u':
                    return checkKeyword("struct"s, Token::TokenType::STRUCT);
                case 'z':
                    return checkKeyword("sizeof"s, Token::TokenType::SIZEOF);
                default:
            }
        }
        return checkKeyword("stdcall"s,Token::TokenType::STDCALL);

    case 'S':
        return checkKeyword("Self"s, Token::TokenType::SELF);

    case 'm':
        // mut mtd

        if(len == 3)
        {
            if(start[1] == 'u')
                return checkKeyword("mut"s, Token::TokenType::MUT);
            return checkKeyword("mtd"s, Token::TokenType::MTD);
        }
        return checkKeyword("match"s,Token::TokenType::MATCH);        

    case 'v':
        // volatile
        return checkKeyword("volatile"s, Token::TokenType::VOLATILE);

    case 'b':
        // bool byte
        if(len == 4) {
            if(start[1] == 'o')
                return checkKeyword("bool"s, Token::TokenType::BOOL);
            return checkKeyword("byte"s, Token::TokenType::BYTE);
        }

        return checkKeyword("break"s,Token::TokenType::BREAK);
    case 'w':
        // word while
        if ((len == 4) && start[1] == 'o')
            return checkKeyword("word"s, Token::TokenType::WORD);
        return checkKeyword("while"s, Token::TokenType::WHILE);

    case 'q':
        // qword
        return checkKeyword("qword"s, Token::TokenType::QWORD);

    case 'o':
        // oword
        return checkKeyword("oword"s, Token::TokenType::OWORD);

    case 'e':
        // elif else export
        if (len == 4) {
            if (start[2] == 'i')
                return checkKeyword("elif"s, Token::TokenType::ELIF);
            return checkKeyword("else"s, Token::TokenType::ELSE);
        }
        if(len == 6) 
        {
            if(start[2] == 't')
                return checkKeyword("extern"s, Token::TokenType::EXTERN);
            return checkKeyword("export"s, Token::TokenType::EXPORT);
        }
        return checkKeyword("enum",Token::TokenType::ENUM);

    case 'g':
        return checkKeyword("global", Token::TokenType::GLOBAL);

    case 'l':
        // let
        return checkKeyword("let"s, Token::TokenType::LET);

    case 'r':
        // return
        return checkKeyword("return"s, Token::TokenType::RETURN);

    default:
        return Token::TokenType::IDENTIFIER;
    }
}

    
static void identifier() {
    const char* backupStart = start;
    while (isAlphanumeric(peek()))
        advance();

    if(!(peek() == ':' && peekNext() == ':'))
    {
        makeToken(identifierType());
        return;
    }

    const char* backup = current;

    while(peek() == ':' && peekNext() == ':')
    {
        if(identifierType() != Token::TokenType::IDENTIFIER)
        {
            errors->add(Error::ErrorOutput{
                Error::ErrorOrigin::Lexer,
                Error::ErrorType::Error,
                "Expected an identifier before '::' not a keyword"s,
                line
            });
            skipToWhitespace();
            return;
        }
        advance();
        advance();
        beginNewToken();
        while (isAlphanumeric(peek()))
            advance();
    }
    current = backup;
    makeToken(Token::TokenType::SCOPED_IDENTIFER);
}

static constexpr unsigned int MAX_BIN_DIGITS = 128;    // 128 bits
static constexpr unsigned int MAX_OCT_DIGITS = 43;    // ceil(128 / 3)
static constexpr unsigned int MAX_DEC_DIGITS = 39;    // ceil(log10(2^128))
static constexpr unsigned int MAX_HEX_DIGITS = 32;    // 128 bits

static void integer(bool (*numChecker)(char) = isDigit, unsigned int base = 10) {
    unsigned int max;
    switch (base)
    {
    case 16:
    default:
        max = MAX_HEX_DIGITS;
        break;
    case 10:
        max = MAX_DEC_DIGITS;
        break;
    case 8:
        max = MAX_OCT_DIGITS;
        break;
    case 2:
        max = MAX_BIN_DIGITS;
        break;
    }
    
    for(;numChecker(peek()) && (max >= 1); --max)
        advance();

    if(max == 0) {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Lexer,
            Error::ErrorType::Error,
            "Too many digits in numeric literal"s,
            line
        });
        skipToWhitespace();
        return;
    }

    if(isAlpha(peek())) {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Lexer,
            Error::ErrorType::Error,
            "Unexpected character '"s + peek() + "' after numeric literal"s,
            line
        });
        skipToWhitespace();
        return;
    };

    makeToken(Token::TokenType::INTEGER);
};

static constexpr unsigned int MAX_DEC_BEFORE_DOT = 35;
static constexpr unsigned int MAX_DEC_AFTER_DOT = 35;
static constexpr unsigned int MAX_DEC_AFTER_E = 4;

static void floating() {
    unsigned int max;
    
    for(max = MAX_DEC_BEFORE_DOT;isDigit(peek()) && (max >= 1); --max)
        advance();

    if(max == 0) {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Lexer,
            Error::ErrorType::Error,
            "Too many digits in numeric literal"s,
            line
        });
        skipToWhitespace();
        return;
    }

    if (peek() == '.' && isDigit(peekNext())) {
        advance();
        for(max = MAX_DEC_AFTER_DOT;isDigit(peek()) && (max >= 1); --max)
            advance();

        if(max == 0) {
            errors->add(Error::ErrorOutput{
                Error::ErrorOrigin::Lexer,
                Error::ErrorType::Error,
                "Too many digits in float numeric literal"s,
                line
            });
            skipToWhitespace();
            return;
        }
        
        if(peek() == 'e' || peek() == 'E')
        {
            advance();
            if(!(peek() == '+' || peek() == '-'))
            {
                errors->add(Error::ErrorOutput{
                    Error::ErrorOrigin::Lexer,
                    Error::ErrorType::Error,
                    "Unexpected character '"s + peek() + "' after 'e' in floating numeric literal, expected '+' or '-'"s,
                    line
                });
                return;
            }
            advance();
            if(!isDigit(peek())) {
                errors->add(Error::ErrorOutput{
                    Error::ErrorOrigin::Lexer,
                    Error::ErrorType::Error,
                    "Unexpected character '"s + peek() + "' after 'e+' or 'e-' in floating numeric literal, expected digit"s,
                    line
                });
                return;
            }
            for(max = MAX_DEC_AFTER_E;isDigit(peek()) && (max >= 1); --max)
                advance();

            if(max == 0) {
                errors->add(Error::ErrorOutput{
                    Error::ErrorOrigin::Lexer,
                    Error::ErrorType::Error,
                    "Too many digits in exponent"s,
                    line
                });
                skipToWhitespace();
                return;
            }
        }
        if(isAlpha(peek())) {
            errors->add(Error::ErrorOutput{
                Error::ErrorOrigin::Lexer,
                Error::ErrorType::Error,
                "Unexpected character '"s + peek() + "' after floating numeric literal"s,
                line
            });
        };
        makeToken(Token::TokenType::FLOAT);
    } else {
        if(isAlpha(peek()) ) {
            errors->add(Error::ErrorOutput{
                Error::ErrorOrigin::Lexer,
                Error::ErrorType::Error,
                "Unexpected character '"s + peek() + "' after numeric literal"s,
                line
            });
        };
        makeToken(Token::TokenType::INTEGER);
    }

}

static void numberBase(char c) {
    auto ensureMatchDigitBase = [&](bool (*numChecker)(char)) -> bool {
        if(!numChecker(peek()))
        {
            errors->add(Error::ErrorOutput{
                Error::ErrorOrigin::Lexer,
                Error::ErrorType::Error,
                "Unexpect character '"s + peek() + "', expect digit to match base"s,
                line
            });
            skipToWhitespace();
            return true;
        }
        return false;
    };

    if (c == '0') {
        if(isDigit(peek())) {
            errors->add(Error::ErrorOutput{
                Error::ErrorOrigin::Lexer,
                Error::ErrorType::Error,
                "Numeric literals can not lead with 0 unless defining base or floating values"s,
                line
            });
            skipToWhitespace();
        } else if(isHexDigit(peekNext())) {
            switch(peek()) {
                case 'B':
                case 'b':
                advance();
                if(ensureMatchDigitBase(isBinaryDigit))
                    break;
                integer(isBinaryDigit, 2);
                break;
                case 'O':
                case 'o':
                advance();
                if(ensureMatchDigitBase(isOctoDigit))
                    break;
                integer(isOctoDigit, 8);
                break;
                case 'D':
                case 'd':
                advance();
                if(ensureMatchDigitBase(isDigit))
                    break;
                beginNewToken();
                integer();
                break;
                case 'X':
                case 'x':
                advance();
                integer(isHexDigit, 16);
                break;
                case '.':
                floating();
                break;
                default:
                if(isAlpha(peek()))
                {
                    errors->add(Error::ErrorOutput{
                        Error::ErrorOrigin::Lexer,
                        Error::ErrorType::Error,
                        "Unexpected character '"s + peek() + "' after '0', expected base or '.'"s,
                        line
                    });
                } else {
                    integer();
                }
            }
        }
    } else if(isDigit(c)) {
        floating();
    }
}

static constexpr unsigned int MAX_BIN_DIGIT_CHAR = 8;
static constexpr unsigned int MAX_OCT_DIGIT_CHAR = 3;
static constexpr unsigned int MAX_DEC_DIGIT_CHAR = 3;
static constexpr unsigned int MAX_HEX_DIGIT_CHAR = 2;

static bool escape(std::string& output, size_t tempLine) 
{
    unsigned int max = MAX_BIN_DIGIT_CHAR;
    switch(peek())
    {
        case '\'':
        output.push_back('\'');
        return false;
        case '\"':
        output.push_back('\"');
        return false;
        case '\\':
        output.push_back('\\');
        return false;
        case 'a':
        output.push_back('\a');
        return false;
        case 'b':  
        output.push_back('\b');
        return false;
        case 'n': 
        output.push_back('\n');
        return false;
        case 'r':
        output.push_back('\r');
        return false;
        case 't':
        output.push_back('\t');
        return false;
        case 'v':
        output.push_back('\v');
        return false;
        case 'u':
        if(!match('{'))
        {
            errors->add(Error::ErrorOutput{
                Error::ErrorOrigin::Lexer,
                Error::ErrorType::Fatal,
                "Unexpected character '"s + peek() + "' after '\\u', expected '{'"s,
                tempLine
            });
            return true;
        }
        if(match('0'))
        {
            unsigned int base = 10;
            switch (peek())
            {
            case '}':
                output.push_back('\0');
                return false;
            case 'b':
            case 'B':
                beginNewToken();
                max = MAX_BIN_DIGIT_CHAR;
                base = 2;
                if(!isBinaryDigit(peek()))
                {
                    errors->add(Error::ErrorOutput{
                        Error::ErrorOrigin::Lexer,
                        Error::ErrorType::Fatal,
                        "Unexpected character '"s + peek() + "' after '\\u{0b', expected binary digit"s,
                        tempLine
                    });
                }
                while (isBinaryDigit(peek()) && (max-- >= 1))
                    advance();
                break;
            case 'o':
            case 'O':
                beginNewToken();
                max = MAX_OCT_DIGIT_CHAR;
                base = 8;
                if(!isOctoDigit(peek()))
                {
                    errors->add(Error::ErrorOutput{
                        Error::ErrorOrigin::Lexer,
                        Error::ErrorType::Fatal,
                        "Unexpected character '"s + peek() + "' after '\\u{0o', expected oct digit"s,
                        tempLine
                    });
                }
                while (isOctoDigit(peek()) && (max-- >= 1))
                    advance();
                break;
            case 'd':
            case 'D':
                beginNewToken();
                max = MAX_DEC_DIGIT_CHAR;
                base = 10;
                if(!isDigit(peek()))
                {
                    errors->add(Error::ErrorOutput{
                        Error::ErrorOrigin::Lexer,
                        Error::ErrorType::Fatal,
                        "Unexpected character '"s + peek() + "' after '\\u{0d', expected decimal digit"s,
                        tempLine
                    });
                }
                while (isDigit(peek()) && (max-- >= 1))
                    advance();
                break;
            case 'x':
            case 'X':
                beginNewToken();
                max = MAX_HEX_DIGIT_CHAR;
                base = 16;
                if(!isHexDigit(peek()))
                {
                    errors->add(Error::ErrorOutput{
                        Error::ErrorOrigin::Lexer,
                        Error::ErrorType::Fatal,
                        "Unexpected character '"s + peek() + "' after '\\u{0x', expected hex digit"s,
                        tempLine
                    });
                }
                while (isHexDigit(peek()) && (max-- >= 1))
                    advance();
                break;
            default:
                errors->add(Error::ErrorOutput{
                    Error::ErrorOrigin::Lexer,
                    Error::ErrorType::Fatal,
                    "Unexpected character '"s + peek() + "' after '\\u{0', expected '}' or base"s,
                    tempLine
                });
                return true;
            }
            if(max == 0) {
                errors->add(Error::ErrorOutput{
                    Error::ErrorOrigin::Lexer,
                    Error::ErrorType::Error,
                    "Too many digits in numeric literal in escape sequence"s,
                    line
                });
                return true;
            }
            if(peek() != '}') {
                errors->add(Error::ErrorOutput{
                    Error::ErrorOrigin::Lexer,
                    Error::ErrorType::Fatal,
                    "Unexpected character '"s + peek() + "' after numeric literal in escape sequence"s,
                    tempLine
                });
                return true;
            }
            unsigned int code = std::stoi(curToken(),NULL,base);
            if(code > 255) {
                errors->add(Error::ErrorOutput{
                    Error::ErrorOrigin::Lexer,
                    Error::ErrorType::Fatal,
                    "Expected range in escape sequence to be between 0 and 255"s,
                    tempLine
                });
                return true;
            }
            advance();
            output.push_back(std::bit_cast<char,uint8_t>(static_cast<uint8_t>(code)));
        } else if(isDigit(peek())) {
            beginNewToken();
            max = MAX_DEC_DIGIT_CHAR;
            while (isDigit(peek()) && (max-- >= 1))
                advance();
            if(max == 0) {
                errors->add(Error::ErrorOutput{
                    Error::ErrorOrigin::Lexer,
                    Error::ErrorType::Error,
                    "Too many digits in numeric literal in escape sequence"s,
                    line
                });
                return true;
            }
            if(peek() != '}') {
                errors->add(Error::ErrorOutput{
                    Error::ErrorOrigin::Lexer,
                    Error::ErrorType::Fatal,
                    "Unexpected character '"s + peek() + "' after numeric literal in escape sequence"s,
                    tempLine
                });
                return true;
            }
            unsigned int code = std::stoi(curToken(),NULL,10);
            if(code > 255) {
                errors->add(Error::ErrorOutput{
                    Error::ErrorOrigin::Lexer,
                    Error::ErrorType::Fatal,
                    "Expected range in escape sequence to be between 0 and 255"s,
                    tempLine
                });
                return true;
            }
            advance();
            output.push_back(std::bit_cast<char,uint8_t>(static_cast<uint8_t>(code)));
        } else {
            errors->add(Error::ErrorOutput{
                Error::ErrorOrigin::Lexer,
                Error::ErrorType::Fatal,
                "Unexpected character '"s + peek() + "' after '\\u{', expected digit"s,
                tempLine
            });
            return true;
        }
        break;
        default:
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Lexer,
            Error::ErrorType::Fatal,
            "Unexpected character '"s + peek() + "' after '\\', expected escape sequence"s,
            tempLine
        });
        return true;
    }
    return false;
}

static void character()
{
    size_t tempLine = line;
    std::string output = ""s;

    if(match('\\'))
        escape(output,line);
    else
        output.push_back(advance());
    
    if(!match('\''))
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Lexer,
            Error::ErrorType::Error,
            "Unexpected character '"s + peek() + "' in char, expected ' "s,
            line
        });
        skipToWhitespace();
        return;
    }
    
    makeCustomToken(Token::TokenType::CHARACTER, output);
}

static void string()
{
    size_t tempLine = line;
    std::string output = ""s;
    bool malformed = false;
    while (peek() != '"' && !reachedEnd()) {
        if (match('\\')) {
            const char* backup = start;
            malformed |= escape(output,line);
            start = backup;
        } else {
            if (peek() == '\n')
                line++;
            output.push_back(advance());
        }
    }

    if (reachedEnd()) {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Lexer,
            Error::ErrorType::Fatal,
            "Unterminated string"s,
            tempLine
        });
        return;
    }

    // The closing quote
    advance();
    if(!malformed)
        makeCustomToken(Token::TokenType::STRING,output);
}

static void preprocessor()
{
    while (isAlphanumeric(peek())) 
        advance();
    
    makeToken(Token::TokenType::PREPROCESSOR);
}


Passes::TokenStream targetLexer(const Passes::Target& target, Error::ErrorResult& error)
{
    if(!error.canContinue())
    {
        error.print();
        exit(-1);
    }
    
    start = NULL;
    current = NULL;
    line = 1;
    tokenStream.tokens.clear();
    errors = &error;

    std::string sourceFile = std::visit(overloaded{
        [](const Passes::Script& s)
        {
            return s.sourceFile;
        },
        [](const Passes::Dependency& d)
        {
            return d.buildProperties.sourceFile;
        }
    }, target.buildType);


    FILE* fileReader = fopen(sourceFile.c_str(),"rb");
    if(!fileReader)
    {
        error.add(Error::ErrorOutput{
            Error::ErrorOrigin::FileReader,
            Error::ErrorType::Error,
            "Unable to open"s + sourceFile,
            0
        });
        return tokenStream;
    }

    fseek(fileReader,0,SEEK_END);
    size_t fileSize = ftell(fileReader);
    fseek(fileReader,0,SEEK_SET);

    char* buffer = static_cast<char*>(malloc(fileSize + 1));
    size_t read = fread(buffer,sizeof(char),fileSize,fileReader);
    if(read < fileSize)
    {
        error.add(Error::ErrorOutput{
            Error::ErrorOrigin::FileReader,
            Error::ErrorType::Error,
            "Unable to read"s + sourceFile,
            0
        });
        free(buffer);
        fclose(fileReader);
        return tokenStream;
    }

    buffer[read] = '\0';
    start = buffer;
    current = buffer;

    fclose(fileReader);
    
    makeCustomToken(Token::TokenType::START_OF_FILE, "SOF"s);

    while(!reachedEnd()){
        skipWhitespace();
        beginNewToken();
        if (reachedEnd()) {
            break;
        }

        char c = advance();
        if (isAlpha(c)) {
            identifier();
            continue;
        }
        if (isDigit(c)) {
            numberBase(c);
            continue;
        }
        if(c == '@')
        {
            if(isAlpha(peek()))
                preprocessor();
            else if(peek() == '(')
                makeToken(Token::TokenType::AT);
            else {
                errors->add(Error::ErrorOutput{
                    Error::ErrorOrigin::Lexer,
                    Error::ErrorType::Error,
                    "Unexpected character '"s + c + "' after preprocessor expression"s,
                    line
                });
                skipToWhitespace();
            }
            continue;
        }

        switch (c) {
            break;
            case '(':
            makeToken(Token::TokenType::LEFT_PAREN);
            break;
            case ')':
            makeToken(Token::TokenType::RIGHT_PAREN);
            break;
            case '{':
            makeToken(Token::TokenType::LEFT_BRACE);
            break;
            case '}':
            makeToken(Token::TokenType::RIGHT_BRACE);
            break;
            case '[':
            makeToken(Token::TokenType::LEFT_BRACKET);
            break;
            case ']':
            makeToken(Token::TokenType::RIGHT_BRACKET);
            break;
            case ';':
            makeToken(Token::TokenType::SEMICOLON);
            break;
            case ',':
            makeToken(Token::TokenType::COMMA);
            break;
            case '%':
            makeToken(Token::TokenType::MODULO);
            break;
            case '.':
            makeToken(Token::TokenType::DOT);
            break;
            case '?':
            makeToken(Token::TokenType::QUESTION);
            break;
            case '#':
            makeToken(Token::TokenType::HASH);
            break;
            case '^':
            makeToken(Token::TokenType::CARET);
            break;
            case ':':
            makeToken(match(':') ? Token::TokenType::DOUBLE_COLON : Token::TokenType::COLON);
            break;
            case '&':
            makeToken(match('&') ? Token::TokenType::DOUBLE_AMPERSAND : Token::TokenType::AMPERSAND);
            break;
            case '|':
            makeToken(match('|') ? Token::TokenType::DOUBLE_PIPE : Token::TokenType::PIPE);
            break;
            case '-':
            if(match('>'))
            {
                makeToken(Token::TokenType::ARROW);
                break;
            } else if(match('-')) {
                makeToken(Token::TokenType::MINUS_MINUS);
                break;
            }
            makeToken(match('=') ? Token::TokenType::MINUS_EQUAL :Token::TokenType::MINUS);
            break;
            case '+':
            if(match('+')) {
                makeToken(Token::TokenType::PLUS_PLUS);
                break;
            }
            makeToken(match('=') ? Token::TokenType::PLUS_EQUAL : Token::TokenType::PLUS);
            break;
            case '/':
            makeToken(match('=') ? Token::TokenType::SLASH_EQUAL : Token::TokenType::SLASH);
            break;
            case '*':
            makeToken(match('=') ? Token::TokenType::STAR_EQUAL : Token::TokenType::STAR);
            break;
            case '!':
            makeToken(match('=') ? Token::TokenType::BANG_EQUAL : Token::TokenType::BANG);
            break;
            case '=':
            makeToken(match('=') ? Token::TokenType::EQUAL_EQUAL : Token::TokenType::EQUAL);
            break;
            case '<':
            if(match('<')) {
                makeToken(Token::TokenType::SHIFT_LEFT);
                break;
            }
            makeToken(match('=') ? Token::TokenType::LESS_EQUAL : Token::TokenType::LESS);
            break;
            case '>':
            if(match('>')) {
                makeToken(Token::TokenType::SHIFT_RIGHT);
                break;
            }
            makeToken(match('=') ? Token::TokenType::GREATER_EQUAL : Token::TokenType::GREATER);
            break;
            case '"':
            string();
            break;
            case '\'':
            character();
            break;
            default:
            errors->add(Error::ErrorOutput{
                Error::ErrorOrigin::Lexer,
                Error::ErrorType::Error,
                "Unexpected character '"s + c + "'"s,
                line
            });
            skipToWhitespace();
            break;
        }
    }

    makeCustomToken(Token::TokenType::END_OF_FILE, "EOF"s);

    free(buffer);
    return tokenStream;
}

}