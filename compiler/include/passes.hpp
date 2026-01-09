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
#ifndef PASSES_H
#define PASSES_H
#include "common.hpp"
#include "token.hpp"
#include "AST.hpp"
#include "error.hpp"

namespace Passes {

struct Dependency;
struct Option;

struct PreprocessorDirective {
    std::string identifier;
    std::variant <
        bool,
        uint64_t
    > data; 
};

struct Script {
    std::string sourceFile;
    std::vector<std::string> commands;
    std::vector<PreprocessorDirective> preprocessor;
};

struct Dependency {
    Script buildProperties;
    Target& parent;
    Target& root;
};

struct Target {
    std::variant<
        Script,
        Dependency
    > buildType;
};

struct TokenStream
{
    std::vector<Token::Token> tokens;
};

struct ParsedAST
{
    std::vector<AST::TopLevelStatement> program;
};

struct TypedAST
{

};
struct CodeModule
{

};

TokenStream targetLexer(const Target&, Error::ErrorResult&);
ParsedAST parseTokenStream(const TokenStream&, Error::ErrorResult&);
TypedAST typeCheckAST(const ParsedAST&, Error::ErrorResult&);
CodeModule generateCode(const TypedAST&, Error::ErrorResult&);

}
#endif