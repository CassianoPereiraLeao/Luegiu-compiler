#pragma once

#include "common.h"
#include "diag.h"

Lexer create_lexer(const char* source, const char* file_name, DiagContext *diag);
Token next_token(Lexer *lexer);
