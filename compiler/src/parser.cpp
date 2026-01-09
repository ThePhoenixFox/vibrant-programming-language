/*
    Copyright 2025 Carson F. Becker

    Licensed under the Apache License}, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing}, software
    distributed under the License is distributed on an "AS IS" BASIS},
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND}, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/
#include "common.hpp"
#include "AST.hpp"
#include "passes.hpp"
#include <stdexcept>

enum StatementLevel {
    Top,
    Scope
};

static void sync(StatementLevel);
static std::optional<AST::TopLevelStatement> parseStatement();
static std::optional<AST::ScopeLevelStatement> parseScopeStatement();
static std::optional<AST::ImportStmt> parseImport(AST::SourceSpan);
static std::optional<AST::ExportStmt> parseExport(AST::SourceSpan);
static std::optional<AST::UseStmt> parseUse(AST::SourceSpan, bool isGlobal);
static std::optional<AST::DefineStmt> parseDefine(AST::SourceSpan, bool isGlobal);
static std::optional<AST::UndefineStmt> parseUndefine(AST::SourceSpan, bool isGlobal);
static std::optional<AST::StructStmt> parseStruct(AST::SourceSpan);
static std::optional<AST::EnumStmt> parseEnum(AST::SourceSpan);
static std::optional<AST::ExternStructStmt> parseExternStruct(AST::SourceSpan);
static std::optional<AST::ExternFunStmt> parseExternFun(AST::SourceSpan);
static std::optional<AST::VariableStmt> parseVariable(AST::SourceSpan,bool isGlobal);
static std::optional<AST::FunStmt> parseFun(AST::SourceSpan);

static std::optional<uint64_t> parseIntegerToNumber();
static std::optional<uint64_t> stringToInteger(AST::SourceSpan,std::string);
static std::optional<AST::Generic> parseGeneric();
static std::optional<AST::GenericResolution> parseGenericResolution();
static std::optional<AST::TypeNode> parseType(AST::SourceSpan, bool isNonRefType);
static std::optional<AST::Field> parseProperty(AST::SourceSpan loc);
static std::optional<AST::MethodStmt> parseMethod(AST::SourceSpan loc);
static std::optional<std::vector<AST::Param>> parseParam();

template <typename T> 
struct Prefix 
{
    std::function<std::optional<T>(AST::SourceSpan)> prefixFunction;
    uint32_t bindingPower;
};

template <typename T> 
struct Infix 
{
    std::function<std::optional<T>(AST::SourceSpan, T)> infixFunction;
    uint32_t bindingPowerLhs;
    uint32_t bindingPowerRhs;
};

template <typename T> 
struct Postfix
{
    std::function<std::optional<T>(AST::SourceSpan, T)> postfixFunction;
    uint32_t bindingPower;
};

using ExprPrefix = Prefix<AST::ExpressionNode>;
using ExprInfix = Infix<AST::ExpressionNode>;
using ExprPostfix = Postfix<AST::ExpressionNode>;

using PrePrefix = Prefix<AST::PreExpr>;
using PreInfix = Infix<AST::PreExpr>;

static std::optional<AST::PreprocessorStmt> parsePreprocessorStmt(AST::SourceSpan, bool isGlobal);
static std::optional<AST::PreExpr> prattPreExpression(AST::SourceSpan,uint32_t);
static std::optional<AST::PreExpr> parseGroupingPreExpression(AST::SourceSpan);
static std::optional<AST::PreExpr> parseGenericBinaryPreExpression(AST::SourceSpan,AST::PreExpr,AST::PreExpr::Kind,uint32_t);
static std::optional<AST::PreExpr> parseGenericPrefixPreExpression(AST::SourceSpan,AST::PreExpr::Kind,uint32_t);
static std::optional<AST::PreExpr> parseIdentiferPreExpression(AST::SourceSpan);
static std::optional<AST::PreExpr> parseIntegerPreExpression(AST::SourceSpan);

static std::optional<AST::ExpressionNode> parseExpressionStmt(AST::SourceSpan);
static std::optional<AST::ExpressionNode> prattExpression(AST::SourceSpan,uint32_t);
static std::optional<AST::ExpressionNode> parseGenericBinaryExpression(AST::SourceSpan,AST::ExpressionNode,AST::ExprKind,uint32_t);
static std::optional<AST::ExpressionNode> parseGenericPrefixExpression(AST::SourceSpan,AST::ExprKind,uint32_t);
static std::optional<AST::ExpressionNode> parseGenericPrimaryExpression(AST::SourceSpan,AST::ExprKind,uint32_t);
static std::optional<AST::ExpressionNode> parseGenericPostfixExpression(AST::SourceSpan,AST::ExpressionNode,AST::ExprKind,uint32_t);
static std::optional<AST::ExpressionNode> parseCastExpression(AST::SourceSpan,AST::ExpressionNode);
static std::optional<AST::ExpressionNode> parseMemberAccessExpression(AST::SourceSpan,AST::ExpressionNode,bool);
static std::optional<AST::ExpressionNode> parseDesignatorExpression(AST::SourceSpan);
static std::optional<AST::ExpressionNode> parseGroupingExpression(AST::SourceSpan);
static std::optional<AST::ExpressionNode> parseSubscriptExpression(AST::SourceSpan,AST::ExpressionNode);
static std::optional<AST::ExpressionNode> parseTernaryExpression(AST::SourceSpan,AST::ExpressionNode);

struct ExprParseRule {
  ExprPrefix prefix;
  ExprInfix infix;
  ExprPostfix postfix;
};

struct PreParseRule {
    PrePrefix prefix;
    PreInfix infix;
};

static const std::map<Token::TokenType, ExprParseRule> exprBindingPower = {
    // Parentheses
    {Token::TokenType::LEFT_PAREN,  {
        .prefix  = { nullptr, 0 },
        .infix   = { nullptr, 0, 0 },
        .postfix = { nullptr, 0 }
    }},
    {Token::TokenType::RIGHT_PAREN, {
        .prefix  = { nullptr, 0 },
        .infix   = { nullptr, 0, 0 },
        .postfix = { nullptr, 0 }
    }},

    // Braces / Designated Literal
    {Token::TokenType::LEFT_BRACE,  {
        .prefix  = { nullptr, 0 },
        .infix   = { nullptr, 0, 0 },
        .postfix = { nullptr, 0 }
    }},
    {Token::TokenType::RIGHT_BRACE, {
        .prefix  = { nullptr, 0 },
        .infix   = { nullptr, 0, 0 },
        .postfix = { nullptr, 0 }
    }},

    // Brackets / Indexing
    {Token::TokenType::LEFT_BRACKET, {
        .prefix  = { nullptr, 0 },
        .infix   = { nullptr, 0, 0 },
        .postfix = { nullptr, 200 }
    }},
    {Token::TokenType::RIGHT_BRACKET, {
        .prefix  = { nullptr, 0 },
        .infix   = { nullptr, 0, 0 },
        .postfix = { nullptr, 0 }
    }},

    // Question / Ternary
    {Token::TokenType::QUESTION, {
        .prefix  = { nullptr, 0 },
        .infix   = { nullptr, 20, 19 },
        .postfix = { nullptr, 0 }
    }},

    // Dot, Arrow / Member Access
    {Token::TokenType::DOT, {
        .prefix  = { nullptr, 0 },
        .infix   = { nullptr, 0, 0 },
        .postfix = { nullptr, 180 }
    }},
    {Token::TokenType::ARROW, {
        .prefix  = { nullptr, 0 },
        .infix   = { nullptr, 0, 0 },
        .postfix = { nullptr, 180 }
    }},

    // Arithmetic
    {Token::TokenType::PLUS, {
        .prefix  = { nullptr, 120 },
        .infix   = { nullptr, 70, 70 },
        .postfix = { nullptr, 0 }
    }},
    {Token::TokenType::MINUS, {
        .prefix  = { nullptr, 120 },
        .infix   = { nullptr, 70, 70 },
        .postfix = { nullptr, 0 }
    }},
    {Token::TokenType::STAR, {
        .prefix  = { nullptr, 140 },
        .infix   = { nullptr, 90, 90 },
        .postfix = { nullptr, 0 }
    }},
    {Token::TokenType::SLASH, {
        .prefix  = { nullptr, 0 },
        .infix   = { nullptr, 90, 90 },
        .postfix = { nullptr, 0 }
    }},
    {Token::TokenType::MODULO, {
        .prefix  = { nullptr, 0 },
        .infix   = { nullptr, 90, 90 },
        .postfix = { nullptr, 0 }
    }},
    {Token::TokenType::AMPERSAND, {
        .prefix  = { nullptr, 130 },
        .infix   = { nullptr, 110, 110 },
        .postfix = { nullptr, 0 }
    }},
    {Token::TokenType::PIPE, {
        .prefix  = { nullptr, 0 },
        .infix   = { nullptr, 100, 100 },
        .postfix = { nullptr, 0 }
    }},
    {Token::TokenType::DOUBLE_AMPERSAND, {
        .prefix  = { nullptr, 0 },
        .infix   = { nullptr, 40, 40 },
        .postfix = { nullptr, 0 }
    }},
    {Token::TokenType::DOUBLE_PIPE, {
        .prefix  = { nullptr, 0 },
        .infix   = { nullptr, 30, 30 },
        .postfix = { nullptr, 0 }
    }},
    {Token::TokenType::SHIFT_LEFT, {
        .prefix  = { nullptr, 0 },
        .infix   = { nullptr, 80, 80 },
        .postfix = { nullptr, 0 }
    }},
    {Token::TokenType::SHIFT_RIGHT, {
        .prefix  = { nullptr, 0 },
        .infix   = { nullptr, 80, 80 },
        .postfix = { nullptr, 0 }
    }},

    // Comparison
    {Token::TokenType::LESS, {
        .prefix  = { nullptr, 0 },
        .infix   = { nullptr, 60, 60 },
        .postfix = { nullptr, 0 }
    }},
    {Token::TokenType::LESS_EQUAL, {
        .prefix  = { nullptr, 0 },
        .infix   = { nullptr, 60, 60 },
        .postfix = { nullptr, 0 }
    }},
    {Token::TokenType::GREATER, {
        .prefix  = { nullptr, 0 },
        .infix   = { nullptr, 60, 60 },
        .postfix = { nullptr, 0 }
    }},
    {Token::TokenType::GREATER_EQUAL, {
        .prefix  = { nullptr, 0 },
        .infix   = { nullptr, 60, 60 },
        .postfix = { nullptr, 0 }
    }},
    {Token::TokenType::EQUAL_EQUAL, {
        .prefix  = { nullptr, 0 },
        .infix   = { nullptr, 50, 50 },
        .postfix = { nullptr, 0 }
    }},
    {Token::TokenType::BANG_EQUAL, {
        .prefix  = { nullptr, 0 },
        .infix   = { nullptr, 50, 50 },
        .postfix = { nullptr, 0 }
    }},

    // Assignment
    {Token::TokenType::EQUAL, {
        .prefix  = { nullptr, 0 },
        .infix   = { nullptr, 10, 9 },
        .postfix = { nullptr, 0 }
    }},
    {Token::TokenType::PLUS_EQUAL, {
        .prefix  = { nullptr, 0 },
        .infix   = { nullptr, 10, 9 },
        .postfix = { nullptr, 0 }
    }},
    {Token::TokenType::MINUS_EQUAL, {
        .prefix  = { nullptr, 0 },
        .infix   = { nullptr, 10, 9 },
        .postfix = { nullptr, 0 }
    }},
    {Token::TokenType::STAR_EQUAL, {
        .prefix  = { nullptr, 0 },
        .infix   = { nullptr, 10, 9 },
        .postfix = { nullptr, 0 }
    }},
    {Token::TokenType::SLASH_EQUAL, {
        .prefix  = { nullptr, 0 },
        .infix   = { nullptr, 10, 9 },
        .postfix = { nullptr, 0 }
    }},

    // Unary / prefix
    {Token::TokenType::BANG, {
        .prefix  = { nullptr, 125 },
        .infix   = { nullptr, 0, 0 },
        .postfix = { nullptr, 0 }
    }},
    {Token::TokenType::TILDE, {
        .prefix  = { nullptr, 125 },
        .infix   = { nullptr, 0, 0 },
        .postfix = { nullptr, 0 }
    }},
    {Token::TokenType::HASH, {
        .prefix  = { nullptr, 150 },
        .infix   = { nullptr, 0, 0 },
        .postfix = { nullptr, 0 }
    }},
    {Token::TokenType::PLUS_PLUS, {
        .prefix  = { nullptr, 160 },
        .infix   = { nullptr, 0, 0 },
        .postfix = { nullptr, 170 }
    }},
    {Token::TokenType::MINUS_MINUS, {
        .prefix  = { nullptr, 160 },
        .infix   = { nullptr, 0, 0 },
        .postfix = { nullptr, 170 }
    }},

    // Literals / identifiers
    {Token::TokenType::IDENTIFIER, {
        .prefix  = { nullptr, 0 },
        .infix   = { nullptr, 0, 0 },
        .postfix = { nullptr, 0 }
    }},
    {Token::TokenType::STRING, {
        .prefix  = { nullptr, 0 },
        .infix   = { nullptr, 0, 0 },
        .postfix = { nullptr, 0 }
    }},
    {Token::TokenType::CHARACTER, {
        .prefix  = { nullptr, 0 },
        .infix   = { nullptr, 0, 0 },
        .postfix = { nullptr, 0 }
    }},
    {Token::TokenType::INTEGER, {
        .prefix  = { nullptr, 0 },
        .infix   = { nullptr, 0, 0 },
        .postfix = { nullptr, 0 }
    }},
    {Token::TokenType::FLOAT, {
        .prefix  = { nullptr, 0 },
        .infix   = { nullptr, 0, 0 },
        .postfix = { nullptr, 0 }
    }},
    {Token::TokenType::TRUE, {
        .prefix  = { nullptr, 0 },
        .infix   = { nullptr, 0, 0 },
        .postfix = { nullptr, 0 }
    }},
    {Token::TokenType::FALSE, {
        .prefix  = { nullptr, 0 },
        .infix   = { nullptr, 0, 0 },
        .postfix = { nullptr, 0 }
    }},

    // Built-ins
    {Token::TokenType::SIZEOF, {
        .prefix  = { nullptr, 0 },
        .infix   = { nullptr, 0, 0 },
        .postfix = { nullptr, 0 }
    }},
    {Token::TokenType::ALIGNOF, {
        .prefix  = { nullptr, 0 },
        .infix   = { nullptr, 0, 0 },
        .postfix = { nullptr, 0 }
    }},

    // Cast / AS
    {Token::TokenType::AS, {
        .prefix  = { nullptr, 0 },
        .infix   = { nullptr, 0, 0 },
        .postfix = { nullptr, 0 }
    }},
    // Templates
    {Token::TokenType::DOUBLE_COLON, {
        .prefix  = { nullptr, 0 },
        .infix   = { nullptr, 0, 0 },
        .postfix = { nullptr, 0 }
    }},
};

static const std::map<Token::TokenType, PreParseRule> preBindingPower = {
    // Parentheses
    {Token::TokenType::LEFT_PAREN,  {
        .prefix = { parseGroupingPreExpression, 0 },
        .infix   = { nullptr, 0, 0 }
    }},

    // Arithmetic
    {Token::TokenType::AMPERSAND, {
        .prefix = { nullptr, 0 },
        .infix  = { std::bind(parseGenericBinaryPreExpression, std::placeholders::_1,std::placeholders::_2, AST::PreExpr::BitAnd, 71), 
            70, 71 }
    }},
    {Token::TokenType::PIPE, {
        .prefix = { nullptr, 0 },
        .infix  = { std::bind(parseGenericBinaryPreExpression, std::placeholders::_1,std::placeholders::_2, AST::PreExpr::BitOr, 51), 
            50, 51 }
    }},
    {Token::TokenType::CARET, {
        .prefix = { nullptr, 0 },
        .infix  = { std::bind(parseGenericBinaryPreExpression, std::placeholders::_1,std::placeholders::_2, AST::PreExpr::BitXor, 61), 
            60, 61 }
    }},
    {Token::TokenType::DOUBLE_AMPERSAND, {
        .prefix = { nullptr, 0 },
        .infix  = { std::bind(parseGenericBinaryPreExpression, std::placeholders::_1,std::placeholders::_2, AST::PreExpr::LogicAnd, 21),
            20, 21 }
    }},
    {Token::TokenType::DOUBLE_PIPE, {
        .prefix = { nullptr, 0 },
        .infix  = { std::bind(parseGenericBinaryPreExpression, std::placeholders::_1,std::placeholders::_2, AST::PreExpr::LogicOr, 11),
            10, 11 }
    }},

    // Comparison
    {Token::TokenType::LESS, {
        .prefix = { nullptr, 0 },
        .infix  = { std::bind(parseGenericBinaryPreExpression, std::placeholders::_1,std::placeholders::_2, AST::PreExpr::Less, 41),
            40, 41 }
    }},
    {Token::TokenType::LESS_EQUAL, {
        .prefix = { nullptr, 0 },
        .infix  = { std::bind(parseGenericBinaryPreExpression, std::placeholders::_1,std::placeholders::_2, AST::PreExpr::LessEqual, 41),
            40, 41 }
    }},
    {Token::TokenType::GREATER, {
        .prefix = { nullptr, 0 },
        .infix  = { std::bind(parseGenericBinaryPreExpression, std::placeholders::_1,std::placeholders::_2, AST::PreExpr::Great, 41),
            40, 41 }
    }},
    {Token::TokenType::GREATER_EQUAL, {
        .prefix = { nullptr, 0 },
         .infix  = { std::bind(parseGenericBinaryPreExpression, std::placeholders::_1,std::placeholders::_2, AST::PreExpr::GreatEqual, 41),
            40, 41 }
    }},
    {Token::TokenType::EQUAL_EQUAL, {
        .prefix = { nullptr, 0 },
        .infix  = { std::bind(parseGenericBinaryPreExpression, std::placeholders::_1,std::placeholders::_2, AST::PreExpr::Equal, 31),
            30, 31 }
    }},
    {Token::TokenType::BANG_EQUAL, {
        .prefix = { nullptr, 0 },
        .infix = { std::bind(parseGenericBinaryPreExpression, std::placeholders::_1,std::placeholders::_2, AST::PreExpr::NotEqual, 31),
            30, 31 }
    }},

    // Unary / prefix
    {Token::TokenType::BANG, {
        .prefix  = { std::bind(parseGenericPrefixPreExpression, std::placeholders::_1,AST::PreExpr::LogicNot, 80), 80 },
        .infix   = { nullptr, 0, 0 }
    }},
    {Token::TokenType::TILDE, {
        .prefix  = { std::bind(parseGenericPrefixPreExpression, std::placeholders::_1,AST::PreExpr::BitNot, 80), 80 },
        .infix   = { nullptr, 0, 0 }
    }},

    // Literals / identifiers
    {Token::TokenType::PREPROCESSOR, {
        .prefix  = { parseIdentiferPreExpression, 0 },
        .infix   = { nullptr, 0, 0 }
    }},
    {Token::TokenType::INTEGER, {
        .prefix  = { parseIntegerPreExpression, 0 },
        .infix   = { nullptr, 0, 0 }
    }}
};

static thread_local const std::vector<Token::Token>* tokStream;
static thread_local std::vector<Token::Token>::const_iterator current;
static thread_local Passes::ParsedAST astStream;
static thread_local Error::ErrorResult* errors;

static void makeToken(const AST::TopLevelStatement& type)
{
    astStream.program.emplace_back(type);
}

static thread_local uint64_t parenBalance;
static thread_local uint64_t braceBalance;
static thread_local uint64_t bracketBalance;

static Token::Token advance()
{
    switch(current->type)
    {
        case Token::TokenType::LEFT_PAREN:
            ++parenBalance;
            break;
        case Token::TokenType::RIGHT_PAREN:
            --parenBalance;
            break;
        case Token::TokenType::LEFT_BRACE:
            ++braceBalance;
            break;
        case Token::TokenType::RIGHT_BRACE:
            --braceBalance;
            break;
        case Token::TokenType::LEFT_BRACKET:
            ++bracketBalance;
            break;
        case Token::TokenType::RIGHT_BRACKET:
            --bracketBalance;
            break;
    }
    if(current->type == Token::TokenType::END_OF_FILE)
        return *current;
    return *current++;
}

static Token::Token peek()
{
    return *current;
}
    
static bool reachedEnd()
{
    return (current == tokStream->cend()) || (current->type == Token::TokenType::END_OF_FILE);
} 

static Token::Token peekNext()
{
    if(reachedEnd())
        return peek();

    return current[1];
}

static Token::Token prev()
{
    if(current == tokStream->cbegin() || (current->type == Token::TokenType::START_OF_FILE))
    {
        return peek();
    }
    return current[-1];
}

static bool peekMatch(Token::TokenType expected)
{
    if(reachedEnd())
        return false;

    return current->type == expected;
}
    
static bool match(Token::TokenType expected)
{    
    if(!peekMatch(expected))
        return false;

    current++;
    return true;
}

static std::optional<uint64_t> parseIntegerToNumber()
{
    Token::Token integer = advance();
    return stringToInteger(integer.line,integer.input);
}

static std::optional<uint64_t> stringToInteger(AST::SourceSpan loc, std::string value)
{
    int base = 10;
    uint64_t num = 0;

    if(value.starts_with("0b"))
    {
        value = value.substr(2);
        base = 2;
    }
    else if(value.starts_with("0o"))
    {
        value = value.substr(2);
        base = 8;
    }
    else if(value.starts_with("0d"))
    {
        value = value.substr(2);
        base = 10;
    }
    else if(value.starts_with("0x"))
    {
        value = value.substr(2);
        base = 16;
    }

    try {
        num = std::stoul(value,NULL,base);
    }
    catch (const std::invalid_argument& e)
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Error,
            "Integer literal contains invalid characters, BY THE WAY THIS SHOULD NEVER HAPPEN! Bad programming Skills + Ratio + Learn Rust"s,
            loc.lineStart
        });
        return std::nullopt;
    }
    catch (const std::overflow_error& e)
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Integer literal overflows 64 bit limit"s,
            loc.lineStart
        });
        return std::nullopt;
    }
    
    return num;
}

static std::optional<AST::Generic> parseGeneric()
{
    AST::Generic gen;
    AST::SourceSpan loc = peek().line;

    if(!match(Token::TokenType::LESS))
    {
        return std::nullopt;
    }
    do {
        if(!peekMatch(Token::TokenType::IDENTIFIER))
        {
            errors->add(Error::ErrorOutput{
                Error::ErrorOrigin::Parser,
                Error::ErrorType::Fatal,
                "Unexpected token in generic, expected an identifer, got "s + peek().input,
                peek().line
            });
            return std::nullopt;
        }
        gen.typeIdentifer.emplace_back(advance().input);
    } while(!reachedEnd() && match(Token::TokenType::COMMA));

    if(reachedEnd())
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Expected '>', reached end of file instead"s,
            loc
        });
        return std::nullopt;
    }

    return gen;
}

static std::optional<AST::GenericResolution> parseGenericResolution() {
    AST::GenericResolution genResolution;
    AST::SourceSpan loc = peek().line;
    
    do {
        std::optional<AST::TypeNode> childType = parseType(peek().line,true);

        if(!childType)
            return std::nullopt;
        genResolution.types.emplace_back(std::make_unique<AST::TypeNode>(childType.value()));
    } while (!reachedEnd() && match(Token::TokenType::COMMA));
    
    if(reachedEnd())
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Expected '>' in generic resolution, reached end of file instead"s,
            loc
        });
        return std::nullopt;
    }
    if(!match(Token::TokenType::GREATER))
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Unexpected token after generic resolution, expected '>', got "s + peek().input,
            peek().line
        });
        return std::nullopt;
    }
    return genResolution;
}

static std::optional<AST::PreExpr> prattPreExpression(AST::SourceSpan loc, uint32_t minBindingPower)
{
    AST::PreExpr preExpr;
    std::optional<AST::PreExpr> tempOutput;
    Token::Token lhs = advance();
    auto rulesIter = preBindingPower.find(lhs.type);
    
    if(rulesIter == preBindingPower.end())
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Expected unary token, Token does not have exist in preprocessor expression pratt rules table, got "s + lhs.input,
            lhs.line
        });
        return std::nullopt;
    }

    PreParseRule rule = rulesIter->second;

    if(!rule.prefix.prefixFunction)
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Token does not have prefix function, got "s + lhs.input,
            lhs.line
        });
        return std::nullopt;
    }
    tempOutput = rule.prefix.prefixFunction(loc);
    if(!tempOutput)
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Prefix preprocessor function failed, got "s + lhs.input,
            lhs.line
        });
        return std::nullopt;
    }
    preExpr = tempOutput.value();

    while(true)
    {
        Token::Token rhs = peek();
        rulesIter = preBindingPower.find(rhs.type);

        if(rulesIter == preBindingPower.end())
            break;
        
        rule = rulesIter->second;

        if(!rule.infix.infixFunction)
        {
            errors->add(Error::ErrorOutput{
                Error::ErrorOrigin::Parser,
                Error::ErrorType::Fatal,
                "Token does not have infix function, got "s + rhs.input,
                rhs.line
            });
            return std::nullopt;
        }

        if (rule.infix.bindingPowerLhs < minBindingPower)
            break;

        advance();
        tempOutput = rule.infix.infixFunction(rhs.line,preExpr);
        if(!tempOutput)
        {
            errors->add(Error::ErrorOutput{
                Error::ErrorOrigin::Parser,
                Error::ErrorType::Fatal,
                "Infix preprocessor function failed, got "s + rhs.input,
                rhs.line
            });
            return std::nullopt;
        }
        preExpr = tempOutput.value();
    }

    return preExpr;
}

static std::optional<AST::PreExpr> parseGroupingPreExpression(AST::SourceSpan)
{
    std::optional<AST::PreExpr> preExpr = prattPreExpression(peek().line,0);
    if(!match(Token::TokenType::RIGHT_PAREN))
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Expected ')' after '(' in preprocessor expression, got "s + peek().input,
            peek().line
        });
        return std::nullopt;
    }
    return preExpr;
}

static std::optional<AST::PreExpr> parseGenericBinaryPreExpression(
    AST::SourceSpan loc, AST::PreExpr lhs,
    AST::PreExpr::Kind resulting, uint32_t current)
{
    AST::PreExpr preExpr;
    preExpr.loc = loc;
    preExpr.kind = resulting;
    preExpr.left = std::make_unique<AST::PreExpr>(lhs);
    preExpr.ident = std::nullopt;
    preExpr.literal = std::nullopt;

    std::optional<AST::PreExpr> rhs = prattPreExpression(peek().line, current);
    if(!rhs)
        return std::nullopt;
    
    preExpr.right = std::make_unique<AST::PreExpr>(rhs.value());

    return preExpr;
}

static std::optional<AST::PreExpr> parseGenericPrefixPreExpression(
    AST::SourceSpan loc, AST::PreExpr::Kind resulting, uint32_t current)
{
    AST::PreExpr preExpr;
    preExpr.loc = loc;
    preExpr.kind = resulting;
    std::optional<AST::PreExpr> prefix = prattPreExpression(peek().line,current);
    if(!prefix)
        return std::nullopt;
    preExpr.left = std::make_unique<AST::PreExpr>(prefix.value());
    preExpr.right = nullptr;
    preExpr.ident = std::nullopt;
    preExpr.literal = std::nullopt;

    return preExpr;
}

static std::optional<AST::PreExpr> parseIdentiferPreExpression(AST::SourceSpan loc)
{
    AST::PreExpr preExpr;
    preExpr.loc = loc;
    preExpr.kind = AST::PreExpr::Kind::Identifier;
    preExpr.left = nullptr;
    preExpr.right = nullptr;
    preExpr.ident = prev().input;
    preExpr.literal = std::nullopt;

    return preExpr;
}

static std::optional<AST::PreExpr> parseIntegerPreExpression(AST::SourceSpan loc)
{
    AST::PreExpr preExpr;
    preExpr.loc = loc;
    preExpr.kind = AST::PreExpr::Kind::Integer;
    preExpr.left = nullptr;
    preExpr.right = nullptr;
    preExpr.ident = std::nullopt;
    std::optional<uint64_t> val = stringToInteger(prev().line,prev().input);
    if(!val)
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Error,
            "Expected integer literal in the range of 64 bits for preprocessor expressions"s,
            peek().line
        });
        return std::nullopt;
    }
    preExpr.literal = val;

    return preExpr;
}

static std::optional<AST::PreprocessorStmt> parsePreprocessorStmt(AST::SourceSpan loc, bool isGlobal)
{
    AST::PreprocessorStmt stmt;
    stmt.loc = loc;
    stmt.isGlobalScope = isGlobal;

    if(prev().type == Token::TokenType::PREPROCESSOR)
    {
        std::optional<AST::PreExpr> preExpr = parseIdentiferPreExpression(prev().line);
        stmt.condition = std::move(preExpr.value());
    }
    else
    {
        if(!match(Token::TokenType::LEFT_PAREN))
        {
            errors->add(Error::ErrorOutput{
                Error::ErrorOrigin::Parser,
                Error::ErrorType::Fatal,
                "Unexpected token, expected '(' after '@' for preprocessor statement, got"s + peek().input,
                peek().line
            });
            return std::nullopt;
        }

        std::optional<AST::PreExpr> preExpr = prattPreExpression(peek().line,0);
        if(!preExpr)
            return std::nullopt;
        stmt.condition = preExpr.value();

        if(!match(Token::TokenType::RIGHT_PAREN))
        {
            errors->add(Error::ErrorOutput{
                Error::ErrorOrigin::Parser,
                Error::ErrorType::Fatal,
                "Unexpected token, expected ')' after preprocessor expression, got"s + peek().input,
                peek().line
            });
            return std::nullopt;
        }
    }

    if(!match(Token::TokenType::LEFT_BRACE))
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Unexpected token, expected '{' after preprocessor condition, got"s + peek().input,
            peek().line
        });
        return std::nullopt;
    }

    std::vector<std::unique_ptr<AST::TopLevelStatement>> topStmts;
    std::vector<std::unique_ptr<AST::ScopeLevelStatement>> scopeStmts;
    
    while(!match(Token::TokenType::RIGHT_BRACE))
    {
        if(isGlobal)
        {
            std::optional<AST::TopLevelStatement> topStmt = parseStatement();
            if(!topStmt)
                return std::nullopt;
            topStmts.emplace_back(topStmt.value());
        }
        else
        {
            std::optional<AST::ScopeLevelStatement> scopeStmt = parseScopeStatement();
            if(!scopeStmt)
                return std::nullopt;
            scopeStmts.emplace_back(scopeStmt.value());
        }
    }


    if(isGlobal)
        stmt.body = std::move(topStmts);
    else
        stmt.body = std::move(scopeStmts);

    return stmt;
}

static std::optional<AST::ExpressionNode> prattExpression(AST::SourceSpan loc, uint32_t minBindingPower)
{
    AST::ExpressionNode expr;
    std::optional<AST::ExpressionNode> tempOutput;
    Token::Token lhs = advance();
    auto rulesIter = exprBindingPower.find(lhs.type);
    
    if(rulesIter == exprBindingPower.end())
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Expected unary token, Token does not have exist in expression pratt rules table, got "s + lhs.input,
            lhs.line
        });
        return std::nullopt;
    }

    ExprParseRule rule = rulesIter->second;

    if(!rule.prefix.prefixFunction)
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Token does not have prefix function, got "s + lhs.input,
            lhs.line
        });
        return std::nullopt;
    }
    tempOutput = rule.prefix.prefixFunction(loc);
    if(!tempOutput)
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Prefix expression function failed, got "s + lhs.input,
            lhs.line
        });
        return std::nullopt;
    }
    expr = tempOutput.value();

    while(true)
    {
        Token::Token rhs = peek();
        rulesIter = exprBindingPower.find(rhs.type);

        if(rulesIter == exprBindingPower.end())
            break;
        
        rule = rulesIter->second;

        if(rule.postfix.postfixFunction)
        {
            if (rule.postfix.bindingPower < minBindingPower)
                break;
            advance();
            tempOutput = rule.postfix.postfixFunction(rhs.line,expr);
            if(!tempOutput)
            {
                errors->add(Error::ErrorOutput{
                    Error::ErrorOrigin::Parser,
                    Error::ErrorType::Fatal,
                    "Postfix expression function failed, got "s + rhs.input,
                    rhs.line
                });
                return std::nullopt;
            }
            expr = tempOutput.value();
            continue;
        }

        if(!rule.infix.infixFunction)
        {
            errors->add(Error::ErrorOutput{
                Error::ErrorOrigin::Parser,
                Error::ErrorType::Fatal,
                "Token does not have infix function, got "s + rhs.input,
                rhs.line
            });
            return std::nullopt;
        }

        if (rule.infix.bindingPowerLhs < minBindingPower)
            break;

        advance();
        tempOutput = rule.infix.infixFunction(rhs.line,expr);
        if(!tempOutput)
        {
            errors->add(Error::ErrorOutput{
                Error::ErrorOrigin::Parser,
                Error::ErrorType::Fatal,
                "Infix expression function failed, got "s + rhs.input,
                rhs.line
            });
            return std::nullopt;
        }
        expr = tempOutput.value();
    }

    return expr;
}

static std::optional<AST::ExpressionNode> parseExpressionStmt(AST::SourceSpan loc)
{
    std::optional<AST::ExpressionNode> stmt = prattExpression(loc, 0);
    if(!match(Token::TokenType::SEMICOLON))
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Unexpected token, expected ';' after expression statement"s + peek().input,
            loc
        });
        return std::nullopt;
    }
    return stmt;
}


static std::optional<AST::ExpressionNode> parseGenericBinaryExpression(
    AST::SourceSpan loc, AST::ExpressionNode lhs,
    AST::ExprKind resulting, uint32_t current)
{
    AST::ExpressionNode expr;
    expr.loc = loc;
    expr.kind = resulting;
    expr.left = std::make_unique<AST::ExpressionNode>(lhs);
    expr.condition = nullptr;
    expr.content = std::nullopt;
    expr.type = std::nullopt;
    expr.generic = std::nullopt;
    expr.args.clear();

    std::optional<AST::ExpressionNode> rhs = prattExpression(peek().line, current);
    if(!rhs)
        return std::nullopt;

    expr.right = std::make_unique<AST::ExpressionNode>(rhs.value());

    return expr;
}

static std::optional<AST::ExpressionNode> parseGenericPrefixExpression(AST::SourceSpan loc,AST::ExprKind resulting,uint32_t current)
{
    AST::ExpressionNode expr;
    expr.loc = loc;
    expr.kind = resulting;
    std::optional<AST::ExpressionNode> prefix = prattExpression(peek().line,current);
    if(!prefix)
        return std::nullopt;
    expr.left = std::make_unique<AST::ExpressionNode>(prefix.value());
    expr.right = nullptr;
    expr.condition = nullptr;
    expr.content = std::nullopt;
    expr.type = std::nullopt;
    expr.generic = std::nullopt;
    expr.args.clear();

    return expr;  
}

static std::optional<AST::ExpressionNode> parseGenericPrimaryExpression(AST::SourceSpan loc,AST::ExprKind resulting,uint32_t current)
{
    AST::ExpressionNode expr;
    expr.loc = loc;
    expr.kind = resulting;
    std::optional<AST::ExpressionNode> prefix = prattExpression(peek().line,current);
    if(!prefix)
        return std::nullopt;
    expr.left = std::make_unique<AST::ExpressionNode>(prefix.value());
    expr.right = nullptr;
    expr.condition = nullptr;
    expr.content = std::nullopt;
    expr.type = std::nullopt;
    expr.generic = std::nullopt;
    expr.args.clear();

    return expr;
}
static std::optional<AST::ExpressionNode> parseGenericPostfixExpression(
    AST::SourceSpan loc,AST::ExpressionNode lhs,
    AST::ExprKind resulting,uint32_t current)
{
    AST::ExpressionNode expr;
    expr.loc = loc;
    expr.kind = resulting;
    expr.left = std::make_unique<AST::ExpressionNode>(lhs);
    expr.right = nullptr;
    expr.condition = nullptr;
    expr.content = std::nullopt;
    expr.type = std::nullopt;
    expr.generic = std::nullopt;
    expr.args.clear();

    return expr;
}

static std::optional<AST::ExpressionNode> parseCastExpression(AST::SourceSpan loc,AST::ExpressionNode lhs)
{
    AST::ExpressionNode expr;
    expr.loc = loc;
    expr.kind = AST::ExprKind::Cast;
    expr.left = std::make_unique<AST::ExpressionNode>(lhs);
    expr.right = nullptr;
    expr.condition = nullptr;
    expr.content = std::nullopt;
    expr.generic = std::nullopt;
    expr.args.clear();

    std::optional<AST::TypeNode> type = parseType(peek().line,true);
    if(!type)
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Expected type after 'as', got"s + peek().input,
            peek().line
        });
        return std::nullopt;
    }
    expr.type = type;

    return expr;
}

static std::optional<AST::ExpressionNode> parseMemberAccessExpression(AST::SourceSpan loc,AST::ExpressionNode lhs,bool pointerAccess)
{
    AST::ExpressionNode expr;
    expr.loc = loc;
    expr.kind = pointerAccess ? AST::ExprKind::PointerAccess : AST::ExprKind::MemberAccess;
    expr.left = std::make_unique<AST::ExpressionNode>(lhs);
    expr.right = nullptr;
    expr.condition = nullptr;
    expr.type = std::nullopt;
    expr.generic = std::nullopt;
    expr.args.clear();

    if(!peekMatch(Token::IDENTIFIER))
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Expected identifer after '"s + (pointerAccess ? "->"s : "."s) + "', got"s + peek().input,
            peek().line
        });
        return std::nullopt;
    }

    expr.content = advance().input;

    return expr;
}

static std::optional<AST::ExpressionNode> parseDesignatorExpression(AST::SourceSpan)
{
    AST::ExpressionNode expr;
    expr.left = nullptr;
    expr.right = nullptr;
    expr.condition = nullptr;
    expr.generic = std::nullopt;
    expr.args.clear();
    
    if(match(Token::TokenType::RIGHT_BRACE))
    {
        expr.kind = AST::ExprKind::ArrayLiteral;
    }
    else if(peekMatch(Token::TokenType::DOT))
    {
        expr.kind = AST::ExprKind::ListDesignator;
        do {
            AST::SourceSpan subLoc = peek().line;


            if(!match(Token::TokenType::DOT))
            {
                errors->add(Error::ErrorOutput{
                    Error::ErrorOrigin::Parser,
                    Error::ErrorType::Fatal,
                    "Expected '.' in list designator, got "s + peek().input,
                    peek().line
                });
                return std::nullopt;
            }

            AST::ExpressionNode subExpr;
            if(!peekMatch(Token::TokenType::IDENTIFIER))
            {
                errors->add(Error::ErrorOutput{
                    Error::ErrorOrigin::Parser,
                    Error::ErrorType::Fatal,
                    "Unexpect token, expected identifer in list designator, got "s + peek().input,
                    peek().line
                });
                return std::nullopt;
            }
            subExpr.content = advance().input;

            if(!match(Token::TokenType::EQUAL))
            {
                errors->add(Error::ErrorOutput{
                    Error::ErrorOrigin::Parser,
                    Error::ErrorType::Fatal,
                    "Expected '='  in list designator, got "s + peek().input,
                    peek().line
                });
                return std::nullopt;
            }

            std::optional<AST::ExpressionNode> value = prattExpression(peek().line, 0);
            if (!value)
            {
                errors->add(Error::ErrorOutput{
                    Error::ErrorOrigin::Parser,
                    Error::ErrorType::Fatal,
                    "Expected expression in list designator"s,
                    peek().line
                });
                return std::nullopt;
            }

            subExpr.loc = subLoc;
            subExpr.kind = AST::ExprKind::ListIndex;
            subExpr.left = std::make_unique<AST::ExpressionNode>(value.value());
            subExpr.right = nullptr;
            subExpr.condition = nullptr;
            subExpr.type = std::nullopt;
            subExpr.generic = std::nullopt;
            subExpr.args.clear();

            expr.args.emplace_back(std::move(subExpr));

        } while (match(Token::COMMA));

        if(!match(Token::TokenType::RIGHT_BRACE))
        {
            errors->add(Error::ErrorOutput{
                Error::ErrorOrigin::Parser,
                Error::ErrorType::Fatal,
                "Expected '}' after list designator, got "s + peek().input,
                peek().line
            });
            return std::nullopt;
        }
    }
    else if(peekMatch(Token::LEFT_BRACKET))
    {
        expr.kind = AST::ExprKind::ArrayDesignator;

        do {
            AST::SourceSpan subLoc = peek().line;
            if(!match(Token::TokenType::LEFT_BRACKET))
            {
                errors->add(Error::ErrorOutput{
                    Error::ErrorOrigin::Parser,
                    Error::ErrorType::Fatal,
                    "Expected '['  in array designator, got "s + peek().input,
                    peek().line
                });
                return std::nullopt;
            }
            
            std::optional<AST::ExpressionNode> index = prattExpression(peek().line, 0);
            if (!index) 
            {
                errors->add(Error::ErrorOutput{
                    Error::ErrorOrigin::Parser,
                    Error::ErrorType::Fatal,
                    "Expected expression in array designator's index"s,
                    peek().line
                });
                return std::nullopt;
            }

            if(!match(Token::TokenType::RIGHT_BRACKET))
            {
                errors->add(Error::ErrorOutput{
                    Error::ErrorOrigin::Parser,
                    Error::ErrorType::Fatal,
                    "Expected ']'  in array designator, got "s + peek().input,
                    peek().line
                });
                return std::nullopt;
            }

            if(!match(Token::TokenType::EQUAL))
            {
                errors->add(Error::ErrorOutput{
                    Error::ErrorOrigin::Parser,
                    Error::ErrorType::Fatal,
                    "Expected '='  in array designator, got "s + peek().input,
                    peek().line
                });
                return std::nullopt;
            }

            std::optional<AST::ExpressionNode> value = prattExpression(peek().line, 0);
            if (!value)
            {
                errors->add(Error::ErrorOutput{
                    Error::ErrorOrigin::Parser,
                    Error::ErrorType::Fatal,
                    "Expected expression in array designator's value"s,
                    peek().line
                });
                return std::nullopt;
            }

            AST::ExpressionNode subExpr;
            subExpr.loc = subLoc;
            subExpr.kind = AST::ExprKind::ArrayDesignIndex;
            subExpr.left = std::make_unique<AST::ExpressionNode>(index.value());
            subExpr.right = std::make_unique<AST::ExpressionNode>(value.value());
            subExpr.condition = nullptr;
            subExpr.type = std::nullopt;
            subExpr.generic = std::nullopt;
            subExpr.args.clear();

            expr.args.emplace_back(std::move(subExpr));

        } while (match(Token::TokenType::COMMA));

        if(!match(Token::TokenType::RIGHT_BRACE))
        {
            errors->add(Error::ErrorOutput{
                Error::ErrorOrigin::Parser,
                Error::ErrorType::Fatal,
                "Expected '}' after array designator, got "s + peek().input,
                peek().line
            });
            return std::nullopt;
        }
    }
    else
    {
        expr.kind = AST::ExprKind::ArrayLiteral;

        do {
            AST::SourceSpan subLoc = peek().line;
            std::optional<AST::ExpressionNode> element = prattExpression(subLoc, 0);
            if (!element)
            {
                errors->add(Error::ErrorOutput{
                    Error::ErrorOrigin::Parser,
                    Error::ErrorType::Fatal,
                    "Expected expression in array literal"s,
                    peek().line
                });
                return std::nullopt;
            }
            AST::ExpressionNode subExpr;
            subExpr.loc = subLoc;
            subExpr.kind = AST::ExprKind::ArrayIndex;
            subExpr.left = std::make_unique<AST::ExpressionNode>(element.value());
            subExpr.right = nullptr;
            subExpr.condition = nullptr;
            subExpr.content = std::nullopt;
            subExpr.type = std::nullopt;
            subExpr.generic = std::nullopt;
            subExpr.args.clear();

            expr.args.emplace_back(std::move(subExpr));
        } while (match(Token::TokenType::COMMA));

        if (!match(Token::TokenType::RIGHT_BRACE)) 
        {
            errors->add(Error::ErrorOutput{
                Error::ErrorOrigin::Parser,
                Error::ErrorType::Fatal,
                "Expected '}' after array literal, got"s + peek().input,
                peek().line
            });
            return std::nullopt;
        }
    }

    if (!match(Token::TokenType::AS)) 
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Expected 'as' after compound literal, got"s + peek().input,
            peek().line
        });
        return std::nullopt;
    }

    std::optional<AST::TypeNode> type = parseType(peek().line,true);
    if(!type)
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Expected type after 'as' in compound literal, got"s + peek().input,
            peek().line
        });
        return std::nullopt;
    }
    expr.type = type;

    return expr;
}

static std::optional<AST::ExpressionNode> parseGroupingExpression(AST::SourceSpan loc)
{
    std::optional<AST::ExpressionNode> expr = prattExpression(peek().line,0);
    if(!expr)
        return std::nullopt;
    if(!match(Token::TokenType::RIGHT_PAREN))
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Unexpect token, expected ')' after '(', got"s + peek().input,
            peek().line
        });
        return std::nullopt;
    }
    return expr;
}

static std::optional<AST::ExpressionNode> parseSubscriptExpression(AST::SourceSpan loc,AST::ExpressionNode lhs)
{
    AST::ExpressionNode expr;
    expr.loc = loc;
    expr.kind = AST::ExprKind::Index;
    expr.left = std::make_unique<AST::ExpressionNode>(lhs);
    expr.condition = nullptr;
    expr.type = std::nullopt;
    expr.generic = std::nullopt;
    expr.args.clear();

    std::optional<AST::ExpressionNode> subExpr = prattExpression(peek().line,0);
    if(!subExpr)
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Expected expression after '[' in index"s,
            peek().line
        });
        return std::nullopt;
    }
    expr.right = std::make_unique<AST::ExpressionNode>(subExpr.value());

    return expr;
}

static std::optional<AST::ExpressionNode> parseTernaryExpression(AST::SourceSpan,AST::ExpressionNode);

static std::optional<AST::TypeNode> parseType(AST::SourceSpan loc, bool isNonRefType)
{
    AST::TypeNode type{};
    type.loc = loc;
    type.generic = std::nullopt;
    type.isMut = false;

    if(match(Token::TokenType::VOLATILE))
        type.isVolatile = true;
    else
        type.isVolatile = false;

    type.subtype = nullptr;
    type.parameters = {};

    auto typeToken = [&type,loc](AST::TypeKind kind)
    {
        type.kind = kind;
    };
    Token::Token c = advance();

    switch (c.type)
    {
        case Token::TokenType::I8:
            typeToken(AST::TypeKind::I8);
            break;
        case Token::TokenType::I16:
            typeToken(AST::TypeKind::I16);
            break;
        case Token::TokenType::I32:
            typeToken(AST::TypeKind::I32);
            break;
        case Token::TokenType::I64:
            typeToken(AST::TypeKind::I64);
            break;
        case Token::TokenType::I128:
            typeToken(AST::TypeKind::I128);
            break;
        case Token::TokenType::U8:
            typeToken(AST::TypeKind::U8);
            break;
        case Token::TokenType::U16:
            typeToken(AST::TypeKind::U16);
            break;
        case Token::TokenType::U32:
            typeToken(AST::TypeKind::U32);
            break;
        case Token::TokenType::U64:
            typeToken(AST::TypeKind::U64);
            break;
        case Token::TokenType::U128:
            typeToken(AST::TypeKind::U128);
            break;
        case Token::TokenType::F32:
            typeToken(AST::TypeKind::F32);
            break;
        case Token::TokenType::F64:
            typeToken(AST::TypeKind::F64);
            break;
        case Token::TokenType::F128:
            typeToken(AST::TypeKind::F128);
            break;
        case Token::TokenType::D32:
            typeToken(AST::TypeKind::D32);
            break;
        case Token::TokenType::D64:
            typeToken(AST::TypeKind::D64);
            break;
        case Token::TokenType::D128:
            typeToken(AST::TypeKind::D128);
            break;
        case Token::TokenType::UNIT:
            typeToken(AST::TypeKind::UNIT);
            break;
        case Token::TokenType::CHAR:
            typeToken(AST::TypeKind::CHAR);
            break;
        case Token::TokenType::BOOL:
            typeToken(AST::TypeKind::BOOL);
            break;
        case Token::TokenType::BYTE:
            typeToken(AST::TypeKind::BYTE);
            break;
        case Token::TokenType::WORD:
            typeToken(AST::TypeKind::WORD);
            break;
        case Token::TokenType::DWORD:
            typeToken(AST::TypeKind::DWORD);
            break;
        case Token::TokenType::QWORD:
            typeToken(AST::TypeKind::QWORD);
            break;
        case Token::TokenType::OWORD:
            typeToken(AST::TypeKind::OWORD);
            break;

        // DERIVED TYPES
        case Token::TokenType::LEFT_BRACKET:
        {
            std::optional<AST::TypeNode> childType = parseType(loc,true);
            type.kind = AST::TypeKind::ARRAY;

            if(!childType)
                return std::nullopt;
            
            type.subtype = std::make_unique<AST::TypeNode>(childType.value());

            if(match(Token::TokenType::COLON))
            {
                if(!peekMatch(Token::TokenType::INTEGER))
                {
                    errors->add(Error::ErrorOutput{
                        Error::ErrorOrigin::Parser,
                        Error::ErrorType::Fatal,
                        "Expected size parameter after array type, got"s,
                        peek().line
                    });
                    return std::nullopt;
                }

                std::optional<uint64_t> val = parseIntegerToNumber();
                if(!val)
                {
                    errors->add(Error::ErrorOutput{
                        Error::ErrorOrigin::Parser,
                        Error::ErrorType::Error,
                        "Expected integer literal in the range of 64 bits for array size"s,
                        peek().line
                    });
                    return std::nullopt;
                }
                type.arrayLength = val;
            }
            else
                type.arrayLength = std::nullopt;
            break;
        }
        case Token::TokenType::STAR:
        {
            if(match(Token::TokenType::MUT))
                type.isMut = true;
            std::optional<AST::TypeNode> childType = parseType(loc,true);
            type.kind = AST::TypeKind::POINTER;

            if(!childType)
                return std::nullopt;
            
            type.subtype = std::make_unique<AST::TypeNode>(childType.value());
            break;
        }
        case Token::TokenType::AMPERSAND:
        {
            if(isNonRefType)
            {
                errors->add(Error::ErrorOutput{
                    Error::ErrorOrigin::Parser,
                    Error::ErrorType::Fatal,
                    "Unexpected token, this type can not include reference types"s,
                    peek().line
                });
                return std::nullopt;
            }

            if(type.isVolatile)
            {
                errors->add(Error::ErrorOutput{
                    Error::ErrorOrigin::Parser,
                    Error::ErrorType::Fatal,
                    "Unexpected token, this type can not include volatile qualifier"s,
                    peek().line
                });
                return std::nullopt;
            }

            if(match(Token::TokenType::MUT))
                type.isMut = true;
            std::optional<AST::TypeNode> childType = parseType(loc,true);
            type.kind = AST::TypeKind::REFERENCE;

            if(!childType)
                return std::nullopt;
            
            type.subtype = std::make_unique<AST::TypeNode>(childType.value());
            break;
        }
        case Token::TokenType::FUN:
        {
            if(type.isVolatile)
            {
                errors->add(Error::ErrorOutput{
                    Error::ErrorOrigin::Parser,
                    Error::ErrorType::Fatal,
                    "Unexpected token, this type can not include volatile qualifier"s,
                    peek().line
                });
                return std::nullopt;
            }

            type.kind = AST::TypeKind::FUNCTION;

            if(match(Token::TokenType::LESS))
            {
                std::optional<AST::Generic> generic = parseGeneric();
                if(!generic)
                    return std::nullopt;
                type.generic = generic.value();
            }

            if(!match(Token::TokenType::LEFT_PAREN))
            {
                errors->add(Error::ErrorOutput{
                    Error::ErrorOrigin::Parser,
                    Error::ErrorType::Fatal,
                    "Unexpected token after function type, expected '(', got "s + peek().input,
                    peek().line
                });
                return std::nullopt;
            }

            if(!match(Token::TokenType::RIGHT_PAREN))
            {
                do {
                    std::optional<AST::TypeNode> childType = parseType(loc,true);

                    if(!childType)
                        return std::nullopt;
                    type.parameters.emplace_back(std::make_unique<AST::TypeNode>(childType.value()));
                } while (!reachedEnd() && match(Token::TokenType::COMMA));

                if(reachedEnd())
                {
                    errors->add(Error::ErrorOutput{
                        Error::ErrorOrigin::Parser,
                        Error::ErrorType::Fatal,
                        "Expected ')', reached end of file instead"s,
                        loc
                    });
                    return std::nullopt;
                }
                if(!match(Token::TokenType::RIGHT_PAREN))
                {
                    errors->add(Error::ErrorOutput{
                        Error::ErrorOrigin::Parser,
                        Error::ErrorType::Fatal,
                        "Unexpected token after function type, expected ')', got "s + peek().input,
                        peek().line
                    });
                    return std::nullopt;
                }
            }

            if(!match(Token::TokenType::ARROW))
            {
                errors->add(Error::ErrorOutput{
                    Error::ErrorOrigin::Parser,
                    Error::ErrorType::Fatal,
                    "Unexpected token after function type, expected '->', got "s + peek().input,
                    peek().line
                });
                return std::nullopt;
            }

            std::optional<AST::TypeNode> childType = parseType(loc,true);

            if(!childType)
                return std::nullopt;

            type.subtype = std::make_unique<AST::TypeNode>(childType.value());
            break;
        }
        case Token::TokenType::SELF:
            typeToken(AST::TypeKind::SELF);
            break;

        case Token::TokenType::IDENTIFIER:
        case Token::TokenType::SCOPED_IDENTIFER:
        {
            type.name = c.input;

            if(match(Token::TokenType::LESS))
            {
                std::optional<AST::GenericResolution> genRes = parseGenericResolution();
                if(!genRes)
                    return std::nullopt;
                
                type.generic = genRes.value();
            }
            break;
        }

        default:
            return std::nullopt;
    }
    return type;
}

static std::optional<AST::Field> parseProperty(AST::SourceSpan loc)
{
    AST::Field field;
    field.loc = loc;

    if(prev().type != Token::TokenType::IDENTIFIER)
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Unexpected token, expected identifier in property, got"s + peek().input + ", BY THE WAY THIS SHOULD NEVER HAPPEN! Bad programming Skills + Ratio + Learn Rust"s,
            peek().line
        });
        return std::nullopt;
    }
    field.name = prev().input;

    if(!match(Token::TokenType::COLON))
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Unexpected token in property, expected ':', got"s + peek().input,
            peek().line
        });
        return std::nullopt;
    }

    std::optional<AST::TypeNode> type = parseType(peek().line,true);
    if(!type)
        return std::nullopt;
    return field;
}

static std::optional<AST::MethodStmt> parseMethod(AST::SourceSpan loc)
{

}

static std::optional<std::vector<AST::Param>> parseParam(AST::SourceSpan loc)
{
    std::vector<AST::Param> params;

    do {

    if(!peekMatch(Token::TokenType::IDENTIFIER))
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Unexpected token, expected identifier in parameter list, got"s + peek().input,
            peek().line
        });
        return std::nullopt;
    }

    AST::Param param;
    param.name = advance().input;

    if(!match(Token::TokenType::COLON))
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Unexpected token, expected ':' in after parameter identifier, got"s + peek().input,
            peek().line
        });
        return std::nullopt;
    }

    std::optional<AST::TypeNode> type = parseType(loc, false);
    if(type)
        param.type = type.value();
    else
        return std::nullopt;
    
    params.emplace_back(param);
    } while(!reachedEnd() && match(Token::TokenType::COMMA));

    if(reachedEnd())
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Expected parameter, reached end of file instead"s,
            loc
        });
        return std::nullopt;
    }

    return params;
}

static std::optional<AST::ImportStmt> parseImport(AST::SourceSpan loc)
{
    AST::ImportStmt stmt{};
    stmt.loc = loc;
    if(!peekMatch(Token::TokenType::IDENTIFIER))
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Expected identifer after 'import', got"s + peek().input,
            peek().line
        });
        return std::nullopt;
    }
    stmt.module = advance().input;
    if(match(Token::TokenType::LEFT_BRACE))
    {
        do {
            
        Token::Token arg = advance();
        switch (arg.type)
        {
        case Token::TokenType::PREPROCESSOR:
        {
            stmt.args.emplace_back(
                AST::ImportStmt::Arg {
                    AST::ImportStmt::ArgKind::Preprocessor,
                    arg.input,
                    std::nullopt
                }
            );
            break;
        }

        case Token::TokenType::BANG:
        {
            if(!peekMatch(Token::TokenType::IDENTIFIER))
            {
                errors->add(Error::ErrorOutput{
                    Error::ErrorOrigin::Parser,
                    Error::ErrorType::Fatal,
                    "Expected identifer after '!' in import commands, got "s + peek().input,
                    peek().line
                });
                return std::nullopt;
            }
            stmt.args.emplace_back(
                AST::ImportStmt::Arg {
                    AST::ImportStmt::ArgKind::DefineFalse,
                    advance().input,
                    std::nullopt
                }
            );
            break;
        }

        case Token::TokenType::IDENTIFIER:
        {
            if(match(Token::TokenType::EQUAL))
            {
                if(!peekMatch(Token::TokenType::INTEGER))
                {
                    errors->add(Error::ErrorOutput{
                        Error::ErrorOrigin::Parser,
                        Error::ErrorType::Fatal,
                        "Expected integer after '=' in import commands, got"s + peek().input,
                        peek().line
                    });
                    return std::nullopt;
                }
                std::optional<uint64_t> val = parseIntegerToNumber();
                if(!val)
                {
                    errors->add(Error::ErrorOutput{
                        Error::ErrorOrigin::Parser,
                        Error::ErrorType::Error,
                        "Expected integer literal in the range of 64 bits for import command definition"s,
                        peek().line
                    });
                    return std::nullopt;
                }
                stmt.args.emplace_back(
                    AST::ImportStmt::Arg {
                        AST::ImportStmt::ArgKind::DefineInt,
                        arg.input,
                        val
                    }
                );
            }
            else 
            {
                stmt.args.emplace_back(
                    AST::ImportStmt::Arg {
                        AST::ImportStmt::ArgKind::DefineFlag,
                        arg.input,
                        std::nullopt
                    }
                );
            }
            break;
        }
        
        default:
            errors->add(Error::ErrorOutput{
                Error::ErrorOrigin::Parser,
                Error::ErrorType::Fatal,
                "Unexpected token in import commands, expected an import command, got "s + arg.input,
                peek().line
            });
            return std::nullopt;
        }

        } while(!reachedEnd() && match(Token::TokenType::COMMA));

        if(reachedEnd())
        {
            errors->add(Error::ErrorOutput{
                Error::ErrorOrigin::Parser,
                Error::ErrorType::Fatal,
                "Expected '}', reached end of file instead"s,
                loc
            });
            return std::nullopt;
        }

        if(!match(Token::TokenType::RIGHT_BRACE))
        {
            errors->add(Error::ErrorOutput{
                Error::ErrorOrigin::Parser,
                Error::ErrorType::Fatal,
                "Unexpected token in import commands, expected '}', got"s + peek().input,
                peek().line
            });
            return std::nullopt;
        }
    }

    if(!match(Token::TokenType::AS))
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Unexpected token, expected 'as', got"s + peek().input,
            peek().line
        });
        return std::nullopt;
    }

    if(peekMatch(Token::TokenType::IDENTIFIER))
    {
        stmt.name = advance().input;
    }
    else if(match(Token::TokenType::GLOBAL))
    {
        stmt.name = std::nullopt;
    }
    else
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Unexpected token after 'as', expected identifer, got"s + peek().input,
            peek().line
        });
        return std::nullopt;
    }

    if(!match(Token::TokenType::SEMICOLON))
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Unexpected token in import statement, expected ';', got"s + peek().input,
            peek().line
        });
        return std::nullopt;
    }

    return stmt;
}

static std::optional<AST::ExportStmt> parseExport(AST::SourceSpan loc)
{
    AST::ExportStmt stmt{};
    stmt.loc = loc;

    if(!match(Token::TokenType::LEFT_BRACE))
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Unexpected token after export, expected '{', got"s + peek().input,
            peek().line
        });
        return std::nullopt;
    }

    while(!reachedEnd() && !match(Token::TokenType::RIGHT_BRACE)) 
    {
        std::optional<AST::TopLevelStatement> internalStmt = parseStatement();
        if(internalStmt)
            stmt.items.emplace_back(std::make_unique<AST::TopLevelStatement>(internalStmt.value()));
        else
            sync(StatementLevel::Top);
    }

    if(reachedEnd())
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Expected '}', reached end of file instead"s,
            loc
        });
        return std::nullopt;
    }

    return stmt;
}

static std::optional<AST::UseStmt> parseUse(AST::SourceSpan loc, bool global)
{
    AST::UseStmt stmt{};
    stmt.loc = loc;
    if(!match(Token::TokenType::SCOPED_IDENTIFER))
    {
        errors->add(Error::ErrorOutput{
                Error::ErrorOrigin::Parser,
                Error::ErrorType::Fatal,
                "Unexpected token after 'use', expected scoped identifer, got "s + peek().input,
                peek().line
        });
        return std::nullopt;
    }
    stmt.from = advance().input;

    if(!match(Token::TokenType::AS))
    {
        errors->add(Error::ErrorOutput{
                Error::ErrorOrigin::Parser,
                Error::ErrorType::Fatal,
                "Unexpected token, expected 'as', got "s + peek().input,
                peek().line
        });
        return std::nullopt;
    }

    if(global && match(Token::TokenType::GLOBAL))
    {
        stmt.global = true;
        stmt.to = std::nullopt;
    }
    else if(peekMatch(Token::TokenType::IDENTIFIER))
    {
        stmt.global = false;
        stmt.to = advance().input;
    }
    else
    {
        errors->add(Error::ErrorOutput{
                Error::ErrorOrigin::Parser,
                Error::ErrorType::Fatal,
                "Unexpected token after 'as', expected identifer"s + (global ? "or 'global'"s : ""s) + ", got "s + peek().input,
                peek().line
        });
        return std::nullopt;
    }

    if(!match(Token::TokenType::SEMICOLON))
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Unexpected token, expected ';', got"s + peek().input,
            peek().line
        });
        return std::nullopt;
    }

    return stmt;
}

static std::optional<AST::DefineStmt> parseDefine(AST::SourceSpan loc, bool global)
{
    AST::DefineStmt stmt;
    stmt.loc = loc;

    if(global && peekMatch(Token::TokenType::GLOBAL))
    {
        advance();
        stmt.global = true;
    }
    else if(!global && peekMatch(Token::TokenType::GLOBAL))
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Unexpected token, 'global' defines are only aviable on global scope, got "s + peek().input,
            peek().line
        });
        return std::nullopt;
    }

    if(!peekMatch(Token::TokenType::IDENTIFIER))
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Expected identifer after 'define', got "s + peek().input,
            peek().line
        });
        return std::nullopt;
    }
    stmt.name = advance().input;
    if(match(Token::TokenType::EQUAL))
    {
        if(!peekMatch(Token::TokenType::INTEGER))
        {
            errors->add(Error::ErrorOutput{
                Error::ErrorOrigin::Parser,
                Error::ErrorType::Fatal,
                "Expected integer after '=', got "s + peek().input,
                peek().line
            });
            return std::nullopt;
        }

        std::optional<uint64_t> val = parseIntegerToNumber();
        if(!val)
        {
            errors->add(Error::ErrorOutput{
                Error::ErrorOrigin::Parser,
                Error::ErrorType::Error,
                "Expected integer literal in the range of 64 bits for import command definition"s,
                peek().line
            });
            return std::nullopt;
        }

        stmt.value = val.value();
    }
    else
        stmt.value = std::nullopt;

    if(!match(Token::TokenType::SEMICOLON))
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Unexpected token, expected ';', got"s + peek().input,
            peek().line
        });
        return std::nullopt;
    }
    return stmt;
}

static std::optional<AST::UndefineStmt> parseUndefine(AST::SourceSpan loc, bool global)
{
    AST::UndefineStmt stmt;
    stmt.loc = loc;

    if(global && peekMatch(Token::TokenType::GLOBAL))
    {
        advance();
        stmt.global = true;
    }
    else if(!global && peekMatch(Token::TokenType::GLOBAL))
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Unexpected token, 'global' undefines are only aviable on global scope, got "s + peek().input,
            peek().line
        });
        return std::nullopt;
    }

    if(!peekMatch(Token::TokenType::IDENTIFIER))
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Expected identifer after 'undefine', got "s + peek().input,
            peek().line
        });
        return std::nullopt;
    }
    stmt.name = advance().input;

    if(!match(Token::TokenType::SEMICOLON))
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Unexpected token, expected ';', got"s + peek().input,
            peek().line
        });
        return std::nullopt;
    }
    return stmt;
}

static std::optional<AST::StructStmt> parseStruct(AST::SourceSpan loc)
{
    AST::StructStmt stmt;
    stmt.loc = loc;
    
    if(!peekMatch(Token::TokenType::IDENTIFIER))
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Unexpected token, expected identifier, got"s + peek().input,
            peek().line
        });
        return std::nullopt;
    }
    stmt.name = advance().input;

    stmt.generic = parseGeneric();
    
    if(!match(Token::TokenType::LEFT_BRACE))
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Unexpected token, expected '{', got"s + peek().input,
            peek().line
        });
        return std::nullopt;
    }

    if(match(Token::TokenType::RIGHT_BRACE))
        return stmt;

    do {
        Token::Token c = advance();
        switch (c.type)
        {
        case Token::TokenType::MTD:
        {
            std::optional<AST::MethodStmt> internalStmt = parseMethod(c.line);
            if(internalStmt)
                stmt.entries.emplace_back(internalStmt.value());
            else
                return std::nullopt;
            break;
        }
        case Token::TokenType::FUN:
        {
            std::optional<AST::FunStmt> internalStmt = parseFun(c.line);
            if(internalStmt)
                stmt.entries.emplace_back(internalStmt.value());
            else
                return std::nullopt;
            break;
        }
        case Token::TokenType::IDENTIFIER:
        {
            std::optional<AST::Field> internalStmt = parseProperty(c.line);
            if(internalStmt)
                stmt.entries.emplace_back(internalStmt.value());
            else
                return std::nullopt;
            break;
        }
        default:
            errors->add(Error::ErrorOutput{
                Error::ErrorOrigin::Parser,
                Error::ErrorType::Fatal,
                "Unexpected token in struct, expected an struct definition, got "s + c.input,
                peek().line
            });
            return std::nullopt;
        }
    } while(!reachedEnd() && match(Token::TokenType::COMMA));

    if(reachedEnd())
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Expected '}', reached end of file instead"s,
            loc
        });
        return std::nullopt;
    }

    if(!match(Token::TokenType::RIGHT_BRACE))
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Unexpected token after struct, expected '}', got "s + peek().input,
            peek().line
        });
        return std::nullopt;
    }

    return stmt;
}

static std::optional<AST::EnumStmt> parseEnum(AST::SourceSpan loc)
{
    AST::EnumStmt stmt;
    stmt.loc = loc;
    
    if(!peekMatch(Token::TokenType::IDENTIFIER))
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Unexpected token, expected identifier, got"s + peek().input,
            peek().line
        });
        return std::nullopt;
    }
    stmt.name = advance().input;

    stmt.generic = parseGeneric();
    
    if(!match(Token::TokenType::LEFT_BRACE))
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Unexpected token, expected '{', got"s + peek().input,
            peek().line
        });
        return std::nullopt;
    }

    if(match(Token::TokenType::RIGHT_BRACE))
        return stmt;

    do {
        if(!match(Token::TokenType::IDENTIFIER))
        {
            errors->add(Error::ErrorOutput{
                Error::ErrorOrigin::Parser,
                Error::ErrorType::Fatal,
                "Unexpected token, expected identifer for property, got"s + peek().input,
                peek().line
            });
            return std::nullopt;
        }
        std::optional<AST::Field> internalStmt = parseProperty(loc);
        if(!internalStmt)
            return std::nullopt;
        stmt.fields.emplace_back(internalStmt.value());
    } while(!reachedEnd() && match(Token::TokenType::COMMA));

    if(reachedEnd())
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Expected '}', reached end of file instead"s,
            loc
        });
        return std::nullopt;
    }

    if(!match(Token::TokenType::RIGHT_BRACE))
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Unexpected token after enum, expected '}', got "s + peek().input,
            peek().line
        });
        return std::nullopt;
    }

    return stmt;  
}

static std::optional<AST::ExternStructStmt> parseExternStruct(AST::SourceSpan loc)
{
    AST::ExternStructStmt stmt;
    stmt.loc = loc;
    if(!match(Token::TokenType::STRUCT))
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Unexpected token after extern, expected 'struct', got "s + peek().input + ", BY THE WAY THIS SHOULD NEVER HAPPEN! Bad programming Skills + Ratio + Learn Rust"s,
            peek().line
        });
        return std::nullopt;
    }

    if(!peekMatch(Token::TokenType::IDENTIFIER))
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Unexpected token after extern struct, expected identifer, got "s + peek().input,
            peek().line
        });
        return std::nullopt;
    }
    stmt.name = advance().input;

    if(!match(Token::TokenType::SEMICOLON))
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Unexpected token after extern struct, expected ';', got "s + peek().input,
            peek().line
        });
        return std::nullopt;
    }
    return stmt;
}

static std::optional<AST::ExternFunStmt> parseExternFun(AST::SourceSpan loc)
{
    AST::ExternFunStmt stmt;
    stmt.loc = loc;
    switch (advance().type)
    {
    case Token::TokenType::CDECL:
        stmt.conv = AST::ExternFunStmt::CallConvention::cdelc;
        break;
    case Token::TokenType::STDCALL:
        stmt.conv = AST::ExternFunStmt::CallConvention::stdcall;
        break;
    case Token::TokenType::THISCALL:
        stmt.conv = AST::ExternFunStmt::CallConvention::thiscall;
        break;

    default:
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Unexpected token after extern, expected calling convention, got "s + peek().input,
            peek().line
        });
        return std::nullopt;
    }

    if(!match(Token::TokenType::FUN))
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Unexpected token after extern, expected 'fun', got "s + peek().input,
            peek().line
        });
        return std::nullopt;
    }

    if(!peekMatch(Token::TokenType::IDENTIFIER))
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Unexpected token after extern, expected identifier, got "s + peek().input,
            peek().line
        });
        return std::nullopt;
    }



    return stmt;
}

static std::optional<AST::VariableStmt> parseVariable(AST::SourceSpan loc, bool isGlobal)
{
    AST::VariableStmt stmt;
    stmt.loc = loc;
    stmt.isGlobalScope = isGlobal;

    if(prev().type == Token::TokenType::STATIC)
        stmt.isStatic = true;
    else if(!isGlobal && prev().type == Token::TokenType::LET)
        stmt.isStatic = false;
    else if(isGlobal && prev().type == Token::TokenType::LET)
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Unexpected token for variable statement, expected 'static' in global scope, got "s + peek().input,
            peek().line
        });
        return std::nullopt;
    }
    else 
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Expected lifetime for variable statement, got "s + peek().input + ", BY THE WAY THIS SHOULD NEVER HAPPEN! Bad programming Skills + Ratio + Learn Rust"s,
            peek().line
        });
        return std::nullopt;
    }

    if(match(Token::TokenType::MUT))
        stmt.isMut = true;
    else
        stmt.isMut = false;

    if(!peekMatch(Token::TokenType::IDENTIFIER))
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Unexpected token after variable statement, expected identifier, got "s + peek().input,
            peek().line
        });
        return std::nullopt;
    }
    stmt.name = advance().input;

    if(match(Token::TokenType::COLON))
    {
        std::optional<AST::TypeNode> type = parseType(loc,true);
        if(!type)
            return std::nullopt;
        stmt.annotatedType = type;

        if(isGlobal && match(Token::TokenType::SEMICOLON))
            return stmt;
    }
    else
        stmt.annotatedType = std::nullopt;

    if(!match(Token::TokenType::EQUAL))
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Unexpected token after variable statement, expected '=', got "s + peek().input,
            peek().line
        });
        return std::nullopt;
    }

    std::optional<AST::ExpressionNode> expr = prattExpression(loc, 0);
    if(!expr)
        return std::nullopt;
    stmt.expr = expr.value();

    if(!match(Token::TokenType::SEMICOLON))
    {
        errors->add(Error::ErrorOutput{
            Error::ErrorOrigin::Parser,
            Error::ErrorType::Fatal,
            "Unexpected token, expected ';' after expression in variable statement"s + peek().input,
            loc
        });
        return std::nullopt;
    }
    return stmt;
}

static void sync()
{

}

static std::optional<AST::TopLevelStatement> parseStatement() 
{
    Token::Token c = advance();
    AST::SourceSpan loc = c.line;
    switch (c.type)
    {
    case Token::TokenType::IMPORT:
    {
        std::optional<AST::ImportStmt> stmt = parseImport(loc);
        if(stmt)
            return AST::TopLevelStatement{
                loc,
                AST::TopLevelStatement::Kind::Import,
                stmt.value()
            };
        return std::nullopt;
    }
    case Token::TokenType::EXPORT:
    {
        std::optional<AST::ExportStmt> stmt = parseExport(loc);
        if(stmt)
            return AST::TopLevelStatement{
                loc,
                AST::TopLevelStatement::Kind::Export,
                stmt.value()
            };
        return std::nullopt;
    }
    case Token::TokenType::USE:
    {
        std::optional<AST::UseStmt> stmt = parseUse(loc, true);
        if(stmt)
            return AST::TopLevelStatement{
                loc,
                AST::TopLevelStatement::Kind::Use,
                stmt.value()
            };
        return std::nullopt;
    }
    case Token::TokenType::DEFINE:
    {
        std::optional<AST::DefineStmt> stmt = parseDefine(loc, true);
        if(stmt)
            return AST::TopLevelStatement{
                loc,
                AST::TopLevelStatement::Kind::Define,
                stmt.value()
            };
        return std::nullopt;
    }
    case Token::TokenType::UNDEFINE:
    {
        std::optional<AST::UndefineStmt> stmt = parseUndefine(loc, true);
        if(stmt)
            return AST::TopLevelStatement{
                loc,
                AST::TopLevelStatement::Kind::Define,
                stmt.value()
            };
        return std::nullopt;
    }
    case Token::TokenType::STRUCT:
    {
        std::optional<AST::StructStmt> stmt = parseStruct(loc);
        if(stmt)
            return AST::TopLevelStatement{
                loc,
                AST::TopLevelStatement::Kind::Struct,
                stmt.value()
            };
        return std::nullopt;
    }
    case Token::TokenType::ENUM:
    {
        std::optional<AST::EnumStmt> stmt = parseEnum(loc);
        if(stmt)
            return AST::TopLevelStatement{
                loc,
                AST::TopLevelStatement::Kind::Enum,
                stmt.value()
            };
        return std::nullopt;
    }
    case Token::TokenType::EXTERN:
    {
        if(match(Token::TokenType::STRUCT))
        {
            std::optional<AST::ExternStructStmt> stmt = parseExternStruct(loc);
            if(stmt)
                return AST::TopLevelStatement{
                    loc,
                    AST::TopLevelStatement::Kind::ExternStruct,
                    stmt.value()
                };
            return std::nullopt;
        }
        std::optional<AST::ExternFunStmt> stmt = parseExternFun(loc);
        if(stmt)
            return AST::TopLevelStatement{
                loc,
                AST::TopLevelStatement::Kind::ExternFun,
                stmt.value()
            };
        return std::nullopt;
    }
    case Token::TokenType::FUN:
    {
        std::optional<AST::FunStmt> stmt = parseFun(loc);
        if(stmt)
            return AST::TopLevelStatement{
                loc,
                AST::TopLevelStatement::Kind::ExternFun,
                stmt.value()
            };
        return std::nullopt;
    }
    case Token::TokenType::STATIC:
    {
        std::optional<AST::VariableStmt> stmt = parseVariable(loc,true);
        if(stmt)
            return AST::TopLevelStatement{
                loc,
                AST::TopLevelStatement::Kind::Variable,
                stmt.value()
            };
        return std::nullopt;
    }
    case Token::TokenType::AT:
    case Token::TokenType::PREPROCESSOR:
    {
        std::optional<AST::PreprocessorStmt> stmt = parsePreprocessorStmt(loc, true);
        if(stmt)
            return AST::TopLevelStatement{
                loc,
                AST::TopLevelStatement::Kind::Preprocessor,
                stmt.value()
            };
        return std::nullopt;
    }
    
    default:
    }
    return std::nullopt;
}

Passes::ParsedAST parseTokenStream(const Passes::TokenStream& stream, Error::ErrorResult& error) 
{
    if(!error.canContinue())
    {
        error.print();
        exit(-1);
    }

    tokStream = &stream.tokens;
    current = stream.tokens.cbegin();
    astStream.program.clear();
    errors = &error;
    parenBalance = 0;
    braceBalance = 0;
    bracketBalance = 0;

    while (!reachedEnd())
    {
        
    }

    return astStream;
}