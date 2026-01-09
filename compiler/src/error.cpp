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
#include "error.hpp"
#include <numeric>
#include <stdio.h>

namespace Error {
    

void ErrorResult::add(ErrorOutput error)
{
    errors.emplace_back(error);
}
void ErrorResult::clear(ErrorOrigin type)
{
    std::erase_if(errors,[type](ErrorOutput output){
        return type == output.type;
    });
}

static const char* originToString(ErrorOrigin origin)
{
    switch(origin)
    {
        case ErrorOrigin::CommandLine:
        return "Command Line";
        case ErrorOrigin::FileReader:
        return "File Reader";
        case ErrorOrigin::Lexer:
        return "Lexer";
        case ErrorOrigin::Parser:
        return "Parser";
        case ErrorOrigin::TypeChecker:
        return "Type Checker";
        case ErrorOrigin::ExpressionChecker:
        return "Semantic Category Checker";
        case ErrorOrigin::CodeGen:
        return "Code Generation";
    }
}

void ErrorResult::print()
{
    for(const ErrorOutput& error : errors)
    {
        switch(error.type)
        {
            case ErrorType::Fatal:
            printf("[FATAL]: the operation %s has failed, \"%s\" Line: %uz \n", originToString(error.origin),error.contents.c_str(), error.line);
            break;
            case ErrorType::Error:
            printf("[ERROR]: a operation in %s has failed, \"%s\" Line: %uz \n", originToString(error.origin),error.contents.c_str(), error.line);
            break;
            case ErrorType::Warning:
            printf("[WARNING]: the operation %s has output warnings, \"%s\" Line: %uz \n", originToString(error.origin),error.contents.c_str(), error.line);
            break;
            case ErrorType::Info:
            printf("[INFO]: the operation %s has output information, \"%s\" Line: %uz \n", originToString(error.origin),error.contents.c_str(), error.line);
        }
    }
}
bool ErrorResult::canContinue()
{
    return std::accumulate(errors.begin(),errors.end(),true,[](bool canContinue, ErrorOutput output){
        return canContinue && ((output.type != ErrorType::Fatal) && (output.type != ErrorType::Error));
    });
}
}