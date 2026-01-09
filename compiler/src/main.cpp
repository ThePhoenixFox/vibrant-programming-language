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
#include "common.hpp"
#include "passes.hpp"
#include "error.hpp"

Passes::Target parseCommandLine(int, char*[], Error::ErrorResult&);
Passes::CodeModule compileBottomUp(Passes::Target, Error::ErrorResult&);

int main(int argc, char* argv[])
{
    if(argc < 2)
    {
        printf("Incorrect Usage. Check usage at https://github.com/ThePhoenixFox/vibrant-programming-language.");
    }

    Error::ErrorResult error;
    error.add(Error::ErrorOutput{
        Error::ErrorOrigin::CommandLine,
        Error::ErrorType::Info,
        "The vibrant compiler has started!"s
    });
    Passes::Target compileTarget = parseCommandLine(argc, argv, error);

    if(!error.canContinue())
    {
        error.print();
        exit(-1);
    }

    compileBottomUp(compileTarget,error);
}

Passes::Target parseCommandLine(int argc, char* argv[], Error::ErrorResult& error) 
{

}

Passes::CodeModule compileBottomUp(Passes::Target target, Error::ErrorResult& error)
{
    if(!error.canContinue())
    {
        error.print();
        exit(-1);
    }

    Passes::TokenStream stream = targetLexer(target,error);
    if(target.dependencies.size() != 0)
        for(Passes::Target dependency : target.dependencies)
            compileBottomUp(dependency, error);

}