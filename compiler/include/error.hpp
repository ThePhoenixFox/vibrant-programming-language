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
#ifndef ERROR_H
#define ERROR_H

#include "common.hpp"

namespace Error {

enum ErrorOrigin {
    CommandLine,
    FileReader,
    Lexer,
    Parser,
    TypeChecker,
    ExpressionChecker,
    CodeGen
};

enum ErrorType {
    Fatal,
    Error,
    Warning,
    Info
};

struct ErrorOutput {
    ErrorOrigin origin;
    ErrorType type;
    std::string contents;
    size_t line;
};

struct ErrorResult {
    std::vector<ErrorOutput> errors;
    
    void add(ErrorOutput);
    void clear(ErrorOrigin);

    void print();
    bool canContinue();
};

}

#endif