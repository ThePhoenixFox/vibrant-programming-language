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
#ifndef AST_H
#define AST_H

#include "common.hpp"
#include "token.hpp"

namespace AST {

// ============================================================================
// STRUCT DECLARATION
// ============================================================================

struct ImportStmt;
struct ExportStmt;
struct UseStmt;
struct DefineStmt;
struct UndefineStmt;
struct PreprocessorStmt;
struct StructStmt;
struct ExternStructStmt;
struct EnumStmt;
struct FunStmt;
struct ExternFunStmt;
struct ReturnStmt;
struct VariableStmt;
struct ExprStmt;
struct BreakStmt;
struct ContinueStmt;
struct IfStmt;
struct MatchStmt;
struct ForStmt;
struct WhileStmt;
struct BlockStmt;

// ============================================================================
// SOURCE LOCATION
// ============================================================================

struct SourceSpan {
    size_t lineStart;

    SourceSpan(size_t line) :
        lineStart(line)
    { }

    SourceSpan()
    { }

    operator size_t() {
        return lineStart;
    }
};

// ============================================================================
// TYPE SYSTEM
// ============================================================================

struct GenericResolution {
    std::vector<TypeNode> types;
};

enum class TypeKind {
    I8, I16, I32, I64, I128,
    U8, U16, U32, U64, U128,
    F32, F64, F128,
    D32, D64, D128,
    UNIT, CHAR, BOOL,
    BYTE, WORD, DWORD, QWORD, OWORD,

    ARRAY,
    POINTER,
    REFERENCE,
    FUNCTION,
    USER, // user-defined type name
    SELF // user-defined type alias
};

struct TypeNode {
    SourceSpan loc;

    TypeKind kind;

    // For USER types
    std::optional<std::string> name;
    std::optional<std::variant<
        Generic,
        GenericResolution>> generic;
    bool isMut;
    bool isVolatile; 

    // Semantic children
    std::unique_ptr<TypeNode> subtype;  // array element, pointer target, function return
    std::optional<uint64_t> arrayLength;  // constant length for arrays

    // Optional list of function parameter types
    std::vector<std::unique_ptr<TypeNode>> parameters;

    TypeNode() = default;

    TypeNode(const TypeNode& rhs) :
        loc{rhs.loc},kind{rhs.kind},
        name{rhs.name}, generic{rhs.generic},
        isMut{rhs.isMut},isVolatile{rhs.isVolatile},
        arrayLength{rhs.arrayLength},
        subtype{ rhs.subtype ? std::make_unique<TypeNode>(*rhs.subtype) : nullptr }
    {
        parameters.reserve(rhs.parameters.size());
        for (auto& p : rhs.parameters)
            parameters.push_back(p ? std::make_unique<TypeNode>(*p) : nullptr);
    }

    TypeNode& operator=(const TypeNode& rhs)
    {
        if(this == &rhs) return *this;
        return *this = TypeNode(rhs);
    }

    TypeNode(TypeNode&& rhs) :
        loc{std::move(rhs.loc)},
        kind{std::move(rhs.kind)},
        name{std::move(rhs.name)},
        generic{std::move(rhs.generic)},
        isMut{std::move(rhs.isMut)},
        isVolatile{std::move(rhs.isVolatile)},
        arrayLength{std::move(rhs.arrayLength)},
        subtype{std::move(rhs.subtype)},
        parameters{std::move(rhs.parameters)}
    { }

    TypeNode& operator=(TypeNode&& rhs)
    {
        if (this == &rhs) return *this;
        this->loc = std::move(rhs.loc);
        this->kind = std::move(rhs.kind);
        this->name = std::move(rhs.name);
        this->generic = std::move(rhs.generic);
        this->isMut = std::move(rhs.isMut);
        this->isVolatile = std::move(rhs.isVolatile);
        this->arrayLength = std::move(rhs.arrayLength);
        this->subtype = std::move(rhs.subtype);
        this->parameters = std::move(rhs.parameters);
        return *this;
    }

};

// ============================================================================
// EXPRESSIONS
// ============================================================================

enum class ExprKind {
    Or, And,
    Equal, NotEqual,
    Less, LessEqual, Great, GreatEqual,
    Add, Sub, Mul, Div, Modulo,
    ShiftLeft, ShiftRight,

    BitAnd, BitXor, BitOr,
    PrefixPlus,
    PrefixMinus,
    PostfixPlus,
    PostfixMinus,
    Length,
    UnaryPlus,
    UnaryMinus,
    UnaryNot,
    UnaryBang,
    Index,
    AddressOf,
    Dereference,
    PointerAccess,
    MemberAccess,
    Call,
    Sizeof,
    Alignof,
    Cast,
    Literal,
    Identifier,
    Ternary,

    ArrayLiteral, ArrayIndex, 
    ArrayDesignator, ArrayDesignIndex,
    ListDesignator,  ListIndex,
};

struct ExpressionNode {
    SourceSpan loc;
    ExprKind kind;

    // For binary, unary, ternary
    std::unique_ptr<ExpressionNode> left;
    std::unique_ptr<ExpressionNode> right;

    // For ternary operator
    std::unique_ptr<ExpressionNode> condition;

    // For identifiers holds name
    // For literals holds values (since can be over 64 bits)
    std::optional<std::string> content;
    std::optional<TypeNode> type;
    std::optional<GenericResolution> generic;

    // For function calls/designators
    std::vector<std::unique_ptr<ExpressionNode>> args;

    ExpressionNode() = default;

    ExpressionNode(const ExpressionNode& rhs) :
        loc{rhs.loc},kind{rhs.kind},
        left{ rhs.left ? std::make_unique<ExpressionNode>(*rhs.left) : nullptr },
        right{ rhs.right ? std::make_unique<ExpressionNode>(*rhs.right) : nullptr },
        condition{ rhs.condition ? std::make_unique<ExpressionNode>(*rhs.condition) : nullptr },
        content{rhs.content},
        type{rhs.type},
        generic{rhs.generic}
    { 
        args.reserve(rhs.args.size());
        for(auto& p : rhs.args)
            args.push_back(p ? std::make_unique<ExpressionNode>(*p) : nullptr);
    }

    ExpressionNode& operator=(const ExpressionNode& rhs)
    {
        if(this == &rhs) return *this;
        return *this = ExpressionNode(rhs);
    }

    ExpressionNode(ExpressionNode&& rhs) :
        loc{std::move(rhs.loc)},
        kind{std::move(rhs.kind)},
        left{std::move(rhs.left)},
        right{std::move(rhs.right)},
        condition{std::move(rhs.condition)},
        content{std::move(rhs.content)},
        type{rhs.type},
        generic{rhs.generic},
        args{std::move(rhs.args)}
    { }

    ExpressionNode& operator=(ExpressionNode&& rhs)
    {
        if (this == &rhs) return *this;
        this->loc = std::move(rhs.loc);
        this->kind = std::move(rhs.kind);
        this->left = std::move(rhs.left);
        this->right = std::move(rhs.right);
        this->condition = std::move(rhs.condition);
        this->content = std::move(rhs.content);
        this->args = std::move(rhs.args);
        return *this;
    }
};

// ============================================================================
// STATEMENTS
// ============================================================================

struct TopLevelStatement {
    SourceSpan loc;

    enum class Kind {
        Import,
        Export,

        Use,
        Define,
        Undefine,
        Preprocessor,

        Struct,
        ExternStruct,
        Enum,
        Fun,
        ExternFun,
        Variable
    } kind;
    
    std::variant<
        ImportStmt,
        ExportStmt,
        UseStmt,
        DefineStmt,
        UndefineStmt,
        PreprocessorStmt,
        StructStmt,
        ExternStructStmt,
        EnumStmt,
        FunStmt,
        ExternFunStmt,
        VariableStmt
    > data;
};

struct ScopeLevelStatement {
    enum class Kind {
        Use,
        Define,
        Undefine,
        Preprocessor,

        Struct,
        Enum,
        Fun,
        Return,
        Variable,
        Static,
        Expr,
        If,
        Match,
        For,
        While,
        Block
    } kind;
    
    std::variant<
        DefineStmt,
        UndefineStmt,
        UseStmt,
        PreprocessorStmt,
        StructStmt,
        EnumStmt,
        FunStmt,
        ReturnStmt,
        VariableStmt,
        ExprStmt,
        IfStmt,
        MatchStmt,
        ForStmt,
        WhileStmt,
        BlockStmt
    > data;
};

// ============================================================================
// IMPORT / EXPORT
// ============================================================================

struct ImportStmt {
    SourceSpan loc;
    std::string module;
    std::optional<std::string> name;

    enum class ArgKind { Preprocessor, DefineFlag, DefineFalse, DefineInt };
    struct Arg {
        ArgKind kind;
        std::string name;
        std::optional<uint64_t> value;
    };

    std::vector<Arg> args;
};

struct ExportStmt {
    SourceSpan loc;
    std::vector<std::unique_ptr<TopLevelStatement>> items;

    ExportStmt() = default;

    ExportStmt(const ExportStmt& rhs) :
        loc{rhs.loc}
    { 
        items.reserve(rhs.items.size());
        for(auto& p : rhs.items)
            items.push_back(p ? std::make_unique<TopLevelStatement>(*p) : nullptr);
    }

    ExportStmt(ExportStmt&& rhs) :
        loc{std::move(rhs.loc)},
        items{std::move(rhs.items)}
    { }
};

// ============================================================================
// DEFINES / PREPROCESSOR
// ============================================================================

struct UseStmt {
    SourceSpan loc;
    bool global;
    std::string from;
    std::optional<std::string> to;
};

struct DefineStmt {
    SourceSpan loc;
    bool global;
    std::string name;
    std::optional<int64_t> value;
};

struct UndefineStmt {
    SourceSpan loc;
    bool global;
    std::string name;
};

struct PreExpr {
    SourceSpan loc;
    enum Kind { 
        LogicOr,
        LogicAnd,
        Equal,
        NotEqual,
        Less,
        LessEqual,
        Great,
        GreatEqual,
        BitOr,
        BitXor,
        BitAnd,
        BitNot,
        LogicNot,
        Identifier,
        Integer
    } kind;

    std::optional<std::string> ident;
    std::optional<int64_t> literal;

    std::unique_ptr<PreExpr> left;
    std::unique_ptr<PreExpr> right;

    PreExpr() = default;

    PreExpr(const PreExpr& rhs) :
        loc{rhs.loc},kind{rhs.kind},
        ident{rhs.ident}, literal{rhs.literal},
        left{ rhs.left ? std::make_unique<PreExpr>(*rhs.left) : nullptr },
        right{ rhs.right ? std::make_unique<PreExpr>(*rhs.right) : nullptr }
    { }

    PreExpr& operator=(const PreExpr& rhs)
    {
        if(this == &rhs) return *this;
        return *this = PreExpr(rhs);
    }

    PreExpr(PreExpr&& rhs) :
        loc{std::move(rhs.loc)},
        kind{std::move(rhs.kind)},
        ident{std::move(rhs.ident)}, 
        literal{std::move(rhs.literal)},
        left{std::move(rhs.left)},
        right{std::move(rhs.right)}
    { }

    PreExpr& operator=(PreExpr&& rhs)
    {
        if (this == &rhs) return *this;
        this->loc = std::move(rhs.loc);
        this->kind = std::move(rhs.kind);
        this->ident = std::move(rhs.ident);
        this->literal = std::move(rhs.literal);
        this->left = std::move(rhs.left);
        this->right = std::move(rhs.right);
        return *this;
    }
};

struct PreprocessorStmt {
    SourceSpan loc;
    PreExpr condition;
    bool isGlobalScope;
    std::variant<
        std::vector<std::unique_ptr<TopLevelStatement>>,
        std::vector<std::unique_ptr<ScopeLevelStatement>>
    > body;

    PreprocessorStmt() = default;

    PreprocessorStmt(const PreprocessorStmt& rhs) :
        loc{rhs.loc},condition{rhs.condition},
        isGlobalScope{rhs.isGlobalScope}
    { 
        body = std::visit([&](auto const& vec) -> decltype(body)
        {
            using VecT = std::decay_t<decltype(vec)>;
            using PtrT = typename VecT::value_type;
            using ElemT = typename PtrT::element_type;  

            std::vector<std::unique_ptr<ElemT>> newVec;
            newVec.reserve(vec.size());
            for (auto& p : vec)
                newVec.push_back(p ? std::make_unique<ElemT>(*p) : nullptr);

            return newVec;
        }, rhs.body);
    }
    
    PreprocessorStmt(PreprocessorStmt&& rhs) :
        loc{std::move(rhs.loc)},condition{std::move(rhs.condition)},
        isGlobalScope{std::move(rhs.isGlobalScope)},
        body{std::move(rhs.body)}
    { }
};

// ============================================================================
// STRUCTS
// ============================================================================

struct Generic {
    std::vector<std::string> typeIdentifer;
};

struct Field {
    SourceSpan loc;
    std::string name;
    TypeNode type;
};

struct Param {
    std::string name;
    TypeNode type;
};

struct MethodStmt {
    SourceSpan loc;
    std::string name;

    std::vector<Param> params;
    TypeNode return_type;
    BlockStmt body;
};

struct StructStmt {
    SourceSpan loc;
    std::string name;
    std::optional<Generic> generic;

    std::vector<std::variant<
        Field,
        MethodStmt,
        FunStmt
    >> entries;
};

struct ExternStructStmt {
    SourceSpan loc;
    std::string name;
};

// ============================================================================
// ENUM
// ============================================================================

struct EnumStmt {
    SourceSpan loc;
    std::string name;
    std::optional<Generic> generic;
    std::vector<Field> fields;
};

// ============================================================================
// FUNCTIONS
// ============================================================================

struct FunStmt {
    SourceSpan loc;
    std::string name;
    std::optional<Generic> generic;
    std::vector<Param> params;

    TypeNode return_type;
    BlockStmt body;
};

struct ExternFunStmt {
    SourceSpan loc;
    std::string name;
    enum CallConvention {
        cdelc,
        stdcall,
        thiscall
    };
    CallConvention conv;

    std::vector<Param> params;
    TypeNode return_type;
};

// ============================================================================
// BASIC STATEMENTS
// ============================================================================

struct ReturnStmt {
    SourceSpan loc;
    ExpressionNode value;
};

struct VariableStmt {
    SourceSpan loc;
    bool isGlobalScope;

    std::string name;
    bool isMut;
    bool isStatic;

    std::optional<TypeNode> annotatedType;
    std::optional<ExpressionNode> expr;
};

struct ExprStmt {
    SourceSpan loc;
    ExpressionNode expr;
};

struct BreakStmt
{
    SourceSpan loc;
};

struct ContinueStmt
{
    SourceSpan loc;
};

// ============================================================================
// CONTROL FLOW
// ============================================================================

struct IfBranch {
    ExpressionNode condition;
    BlockStmt block;
};

struct IfStmt {
    SourceSpan loc;
    IfBranch ifBranch;
    std::vector<IfBranch> elifBranches;
    std::optional<BlockStmt> elseBranch;
};

struct MatchCase {
    TypeNode type;
    BlockStmt block;
};

struct MatchStmt {
    SourceSpan loc;
    ExpressionNode expr;
    std::vector<MatchCase> cases;
};

struct ForStmt {
    SourceSpan loc;
    ExpressionNode init;
    ExpressionNode cond;
    ExpressionNode step;
    BlockStmt block;
};

struct WhileStmt {
    SourceSpan loc;
    ExpressionNode cond;
    BlockStmt block;
};

// ============================================================================
// BLOCK
// ============================================================================

struct BlockStmt {
    SourceSpan loc;
    std::vector<std::unique_ptr<ScopeLevelStatement>> statements;

    BlockStmt() = default;

    BlockStmt(const BlockStmt& rhs) :
        loc{rhs.loc}
    {
        statements.reserve(rhs.statements.size());
        for(auto& p : rhs.statements)
            statements.push_back(p ? std::make_unique<ScopeLevelStatement>(*p) : nullptr);
    }

    BlockStmt(BlockStmt&& rhs) :
        loc{std::move(rhs.loc)},
        statements{std::move(rhs.statements)}
    { }
};

}

#endif // AST_H
