#pragma once

#include "common.h"
#include "diag.h"

bool strcompare(View view, const char* string);
bool strlcompare(View view, const char* string, size_t len);
size_t strlength(const char* string);
Lexer create_lexer(const char* src, const char* filename, DiagContext *context);
Token next_token(Lexer *lexer);
