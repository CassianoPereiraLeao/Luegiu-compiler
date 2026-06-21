#include "codegen.h"

#define T(s)        codegen_buffer_writez(&codegen->sec_text, (s))
#define D(s)        codegen_buffer_writez(&codegen->sec_data, (s))
#define B(s)        codegen_buffer_writez(&codegen->sec_bss, (s))
#define NL          codegen_buffer_writec(&codegen->sec_text, '\n')
#define DNL         codegen_buffer_writec(&codegen->sec_data, '\n')
#define TAB         codegen_buffer_writez(&codegen->sec_text, "    ")
#define TI(value)   codegen_buffer_writei(&codegen->sec_text, (long long)(value))
#define TU(value)   codegen_buffer_writeu(&codegen->sec_text, (size_t)(value))

#define PROLOG(name_str, local_size) \
    do { \
        T("\n" name_str ":\n"); \
        TAB; T("push rbp"); NL; \
        TAB; T("mov rbp, rsp"); NL; \
        size_t __aligned_local = ((size_t)(local_size) + 15) & ~(size_t)15; \
        if (__aligned_local > 0) { TAB; T("sub rsp, "); TI(__aligned_local); NL; } \
    } while(0)

#define EPILOG \
    do { \
        TAB; T("mov rsp, rbp"); NL; \
        TAB; T("pop rbp"); NL; \
        TAB; T("ret"); NL; \
    } while(0)

#define SAVE_PARAM_RCX(offset) \
    do { TAB; T("mov [rbp-"); TI(offset); T("], rcx"); NL; } while(0)

#define SAVE_PARAM_RDX(offset) \
    do { TAB; T("mov [rbp-"); TI(offset); T("], rdx"); NL; } while(0)

#define SAVE_PARAM_R8(offset) \
    do { TAB; T("mov [rbp-"); TI(offset); T("], r8"); NL; } while(0)

static void codegen_expr(CodeGen *codegen, Node *expr);
static void codegen_stmt(CodeGen *codegen, Node *stmt);
static void codegen_block(CodeGen *codegen, Node *block);
static void emit_prologue(CodeGen *codegen, int local_bytes);
static int bytes_data_value(TokenType type);

static size_t stringlen(const char* str) {
    size_t n = 0;
    while(str[n]) n++;
    return n;
}

static void memcopy(void *dst, const void *src, size_t len) {
    char* d = (char*)dst;
    const char* s = (const char*)src;
    while(len--) *d++ = *s++;
}

static int intato(char* buffer, long long value) {
    if(value == 0) { buffer[0] = '0'; return 1; }
    char temp[22];
    bool negative = (value < 0);
    if(negative) value = -value;
    int i = 0;
    while(value) { temp[i++] = '0' + (int)(value % 10); value /= 10; }
    if(negative) temp[i++] = '-';
    for(int a = 0, b = i-1; a < b; ++a, --b) { char t=temp[a]; temp[a]=temp[b]; temp[b]=t; }
    memcopy(buffer, temp, i);
    return i;
}

static int unsigned_ato(char* buffer, size_t value) {
    return intato(buffer, (long long)value);
}

void codegen_buffer_init(CodeGenBuffer *buffer, Arena *arena, size_t initial_capacity) {
    buffer->arena    = arena;
    buffer->capacity = initial_capacity ? initial_capacity : 4096;
    buffer->len      = 0;
    buffer->data     = (char*)arena_alloc(arena, buffer->capacity);
}

static void buffer_grow(CodeGenBuffer *buffer, size_t extra) {
    if(buffer->len + extra <= buffer->capacity) return;
    size_t new_cap = buffer->capacity * 2;
    while(new_cap < buffer->len + extra) new_cap *= 2;
    char* nd = (char*)arena_alloc(buffer->arena, new_cap);
    memcopy(nd, buffer->data, buffer->len);
    buffer->data     = nd;
    buffer->capacity = new_cap;
}

void codegen_buffer_write(CodeGenBuffer *buffer, const char* src, size_t len) {
    buffer_grow(buffer, len);
    memcopy(buffer->data + buffer->len, src, len);
    buffer->len += len;
}

void codegen_buffer_writez(CodeGenBuffer *buffer, const char* src) {
    codegen_buffer_write(buffer, src, stringlen(src));
}

void codegen_buffer_writec(CodeGenBuffer *buffer, char c) {
    buffer_grow(buffer, 1);
    buffer->data[buffer->len++] = c;
}

void codegen_buffer_writei(CodeGenBuffer *buffer, long long value) {
    char temp[22];
    int len = intato(temp, value);
    codegen_buffer_write(buffer, temp, len);
}

void codegen_buffer_writeu(CodeGenBuffer *buffer, size_t value) {
    char temp[22];
    int len = unsigned_ato(temp, value);
    codegen_buffer_write(buffer, temp, len);
}

static bool view_eq_str(View name, const char* str) {
    size_t slen = stringlen(str);
    if(name.len != slen) return false;
    for(size_t i = 0; i < slen; ++i)
        if(name.start[i] != str[i]) return false;
    return true;
}

static void loop_push(CodeGen *codegen, int label_cond, int label_end) {
    if(codegen->loop_depth >= CODEGEN_MAX_LOOP_DEPTH) return;
    codegen->loop_stack[codegen->loop_depth].label_cond = label_cond;
    codegen->loop_stack[codegen->loop_depth].label_end  = label_end;
    codegen->loop_depth++;
}

static void loop_pop(CodeGen *codegen) {
    if(codegen->loop_depth > 0) codegen->loop_depth--;
}

static void break_push(CodeGen *codegen, BreakCtxType type, int label_end) {
    if(codegen->break_depth >= CODEGEN_MAX_BREAK_DEPTH) return;
    codegen->break_stack[codegen->break_depth].type = type;
    codegen->break_stack[codegen->break_depth].label_end = label_end;
    codegen->break_depth++;
}

static void break_pop(CodeGen *codegen) {
    if(codegen->break_depth > 0) codegen->break_depth--;
}

static void emit_label_ref(CodeGen *codegen, int id) { T("__L"); TI(id); }
static int  new_label(CodeGen *codegen) { return codegen->label_counter++; }

static size_t codegen_struct_size(CodeGen *codegen, View typename) {
    Symbol* sym = lookup_table(codegen->table, typename);
    if(!sym || sym->category != SYMBOL_STRUCT) return 8;

    size_t total = 0;
    for(size_t i = 0; i < sym->fields_count; ++i) {
        size_t field_size = (sym->fields[i].pointer_lvl > 0)
                            ? 8
                            : (size_t)bytes_data_value(sym->fields[i].type);
        total += (field_size + 7) & ~(size_t)7;
    }
    return total ? total : 8;
}

static size_t codegen_union_size(CodeGen *codegen, View typename) {
    Symbol* sym = lookup_table(codegen->table, typename);
    if(!sym || sym->category != SYMBOL_STRUCT) return 8;

    size_t max = 0;
    for(size_t i = 0; i < sym->fields_count; ++i) {
        size_t field_size = (sym->fields[i].pointer_lvl > 0)
                            ? 8
                            : (size_t)bytes_data_value(sym->fields[i].type);
        if(field_size > max) max = field_size;
    }
    return max ? ((max + 7) & ~(size_t)7) : 8;
}

static int codegen_field_offset(CodeGen *codegen, View typename, View fieldname, bool is_union) {
    if(is_union) return 0;
    Symbol* sym = lookup_table(codegen->table, typename);
    if(!sym) return 0;

    int off = 0;
    for(size_t i = 0; i < sym->fields_count; ++i) {
        FieldInfo* info = &sym->fields[i];
        size_t field_size = (info->pointer_lvl > 0)
                            ? 8
                            : (size_t)bytes_data_value(info->type);
        size_t align = (field_size + 7) & ~(size_t)7;

        if(info->name.len == fieldname.len) {
            bool equals = true;
            for(size_t j = 0; j < fieldname.len; ++j) {
                if(info->name.start[j] != fieldname.start[j]) { equals = false; break; }
            }
            if(equals) return off;
        }
        off += (int)align;
    }
    return 0;
}

static size_t codegen_field_size(CodeGen *codegen, View typename, View fieldname) {
    Symbol* sym = lookup_table(codegen->table, typename);
    if(!sym) return 8;
    for(size_t i = 0; i < sym->fields_count; ++i) {
        FieldInfo* info = &sym->fields[i];
        if(info->name.len == fieldname.len) {
            bool equals = true;
            for(size_t j = 0; j < fieldname.len; ++j) {
                if(fieldname.start[j] != info->name.start[j]) { equals = false; break; }
            }
            if(equals)
                return (info->pointer_lvl > 0) ? 8 : (size_t)bytes_data_value(info->type);
        }
    }
    return 8;
}

static CodeGenLocal* local_find(CodeGen *codegen, View name) {
    for(CodeGenLocal* l = codegen->locals; l; l = l->next) {
        if(l->name.len == name.len) {
            bool eq = true;
            for(size_t i = 0; i < name.len; ++i)
                if(l->name.start[i] != name.start[i]) { eq = false; break; }
            if(eq) return l;
        }
    }
    return NULL;
}

static int element_size_for(CodeGen *codegen, Node *base) {
    if(base->type != NODE_VAR_ACCESS) return 8;

    CodeGenLocal* loc = local_find(codegen, base->ast.var_access.name);
    if(loc) {
        if(loc->pointer_lvl > 1) return 8;
        if(loc->pointer_lvl == 1) return bytes_data_value(loc->type);
        return 8;
    }

    Symbol* sym = lookup_table(codegen->table, base->ast.var_access.name);
    if(sym) {
        if(sym->pointer_lvl > 1) return 8;
        if(sym->pointer_lvl == 1) return bytes_data_value(sym->type);
    }
    return 8;
}

static int local_alloc(CodeGen *codegen, View name, View typename,
                        size_t bytes, TokenType type, size_t pointer_lvl) {
    size_t align = (bytes + 7) & ~(size_t)7;
    codegen->stack_top -= (int)align;
    CodeGenLocal* local = (CodeGenLocal*)arena_alloc(codegen->arena, sizeof(CodeGenLocal));
    local->name        = name;
    local->offset      = codegen->stack_top;
    local->pointer_lvl = pointer_lvl;
    local->type        = type;
    local->typename    = typename;
    local->next        = codegen->locals;
    codegen->locals    = local;
    return codegen->stack_top;
}

static size_t string_intern(CodeGen *codegen, View text) {
    for(CodeGenString* s = codegen->strings; s; s = s->next) {
        if(s->text.len == text.len) {
            bool eq = true;
            for(size_t i = 0; i < text.len; ++i)
                if(s->text.start[i] != text.start[i]) { eq = false; break; }
            if(eq) return s->index;
        }
    }
    CodeGenString* s = (CodeGenString*)arena_alloc(codegen->arena, sizeof(CodeGenString));
    s->text  = text;
    s->index = codegen->string_count++;
    s->next  = codegen->strings;
    codegen->strings = s;
    return s->index;
}

static void emit_strings(CodeGen *codegen) {
    for(CodeGenString* s = codegen->strings; s; s = s->next) {
        D("__str"); codegen_buffer_writeu(&codegen->sec_data, s->index); D(": db ");
        const char* content     = s->text.start + 1;
        size_t      content_len = s->text.len - 2;
        bool first = true;
        while(content_len > 0) {
            if(!first) D(", ");
            first = false;
            if(*content == '\\' && content_len >= 2) {
                char esc  = *(content + 1);
                int  byte = 0;
                switch(esc) {
                case 'n':  byte = 10;   break;
                case 't':  byte = 9;    break;
                case 'r':  byte = 13;   break;
                case '0':  byte = 0;    break;
                case '\\': byte = '\\'; break;
                case '"':  byte = '"';  break;
                default:   byte = esc;  break;
                }
                codegen_buffer_writeu(&codegen->sec_data, (size_t)byte);
                content += 2; content_len -= 2;
            } else {
                codegen_buffer_writeu(&codegen->sec_data, (size_t)(unsigned char)*content);
                content++; content_len--;
            }
        }
        D(", 0"); DNL;
        D("__str"); codegen_buffer_writeu(&codegen->sec_data, s->index);
        D("_len equ $ - __str"); codegen_buffer_writeu(&codegen->sec_data, s->index); DNL;
    }
}

static void emit_mov_reg_imm(CodeGen *codegen, const char* reg, long long imm) {
    TAB; T("mov "); T(reg); T(", "); TI(imm); NL;
}

static void emit_mov_reg_reg(CodeGen *codegen, const char* dst, const char* src) {
    TAB; T("mov "); T(dst); T(", "); T(src); NL;
}

static void emit_load_local(CodeGen *codegen, int offset) {
    TAB; T("mov rax, qword [rbp"); TI(offset); T("]"); NL;
}

static void emit_store_local(CodeGen *codegen, int offset) {
    TAB; T("mov qword [rbp"); TI(offset); T("], rax"); NL;
}

static void emit_lea_local(CodeGen *codegen, int offset) {
    TAB; T("lea rax, [rbp"); TI(offset); T("]"); NL;
}

static void emit_builtin_int_to_str(CodeGen *codegen) {
    T("\n__int_to_str:\n");
    TAB; T("push rbp"); NL;
    TAB; T("mov rbp, rsp"); NL;
    TAB; T("sub rsp, 32"); NL;

    TAB; T("mov rax, rcx"); NL;
    TAB; T("lea rdi, [rel __int_buf + 21]"); NL;
    TAB; T("mov byte [rdi], 0"); NL;
    TAB; T("xor r8, r8"); NL;

    TAB; T("test rax, rax"); NL;
    TAB; T("jns __ipos"); NL;
    TAB; T("neg rax"); NL;
    TAB; T("mov r8, 1"); NL;

    T("__ipos:"); NL;
    TAB; T("mov rcx, 10"); NL;

    T("__iloop:"); NL;
    TAB; T("xor rdx, rdx"); NL;
    TAB; T("div rcx"); NL;
    TAB; T("add dl, 48"); NL;
    TAB; T("dec rdi"); NL;
    TAB; T("mov [rdi], dl"); NL;
    TAB; T("test rax, rax"); NL;
    TAB; T("jnz __iloop"); NL;

    TAB; T("test r8, r8"); NL;
    TAB; T("jz __idone"); NL;
    TAB; T("dec rdi"); NL;
    TAB; T("mov byte [rdi], '-'"); NL;

    T("__idone:"); NL;
    TAB; T("lea rax, [rel __int_buf + 21]"); NL;
    TAB; T("sub rax, rdi"); NL;
    TAB; T("mov rdx, rax"); NL;
    TAB; T("mov rax, rdi"); NL;

    TAB; T("mov rsp, rbp"); NL;
    TAB; T("pop rbp"); NL;
    TAB; T("ret"); NL;
}

static void emit_bss_builtins(CodeGen *codegen) {
    codegen_buffer_writez(&codegen->sec_bss, "__int_buf: resb 22\n");
}

static void emit_prologue(CodeGen *codegen, int local_bytes) {
    TAB; T("push rbp"); NL;
    TAB; T("mov rbp, rsp"); NL;
    if(local_bytes > 0) {
        int aligned = (local_bytes + 15) & ~15;
        TAB; T("sub rsp, "); TI(aligned); NL;
    }

    TAB; T("sub rsp, "); TI(WIN64_SHADOW_SPACE); NL;
}

static void emit_epilogue(CodeGen *codegen) {
    TAB; T("mov rsp, rbp"); NL;
    TAB; T("pop rbp"); NL;
    TAB; T("ret"); NL;
}

static int bytes_data_value(TokenType type) {
    switch(type) {
    case TOKEN_KEYWORD_SMALL:     return 1;
    case TOKEN_KEYWORD_BOOLEAN:   return 1;
    case TOKEN_KEYWORD_INT:       return 4;
    case TOKEN_KEYWORD_BIG:       return 8;
    case TOKEN_KEYWORD_CHAR:      return 1;
    case TOKEN_KEYWORD_UTF8CHAR:  return 1;
    case TOKEN_KEYWORD_UTF16CHAR: return 2;
    case TOKEN_KEYWORD_INT8:
    case TOKEN_KEYWORD_UINT8:     return 1;
    case TOKEN_KEYWORD_INT16:
    case TOKEN_KEYWORD_UINT16:    return 2;
    case TOKEN_KEYWORD_INT32:
    case TOKEN_KEYWORD_UINT32:    return 4;
    case TOKEN_KEYWORD_INT64:
    case TOKEN_KEYWORD_UINT64:    return 8;
    case TOKEN_KEYWORD_FLOAT:     return 4;
    case TOKEN_KEYWORD_DOUBLE:    return 8;
    case TOKEN_KEYWORD_LINK:      return 8;
    default:                      return 8;
    }
}

static int collect_frame_size(CodeGen *codegen, Node *block) {
    if(!block) return 0;

    if(block->type != NODE_BLOCK && block->type != NODE_PROGRAM)
        return 0;

    int      total = 0;
    NodeList* stmts = block->ast.program.declarations;
    if(!stmts) return 0;

    for(size_t i = 0; i < stmts->count; ++i) {
        Node* s = stmts->items[i];
        if(!s) continue;
        switch(s->type) {
        case NODE_VAR_DECL: {
            size_t bytes = 0;
            if(s->ast.var_decl.pointer_lvl > 0) {
                bytes = 8;
            } else if(s->ast.var_decl.data_type == TOKEN_KEYWORD_STRUCT) {
                bytes = codegen_struct_size(codegen, s->ast.var_decl.typename);
            } else if(s->ast.var_decl.data_type == TOKEN_KEYWORD_UNION) {
                bytes = codegen_union_size(codegen, s->ast.var_decl.typename);
            } else {
                bytes = (size_t)bytes_data_value(s->ast.var_decl.data_type);
            }
            total += (int)((bytes + 7) & ~(size_t)7);
            break;
        }
        case NODE_BLOCK:
            total += collect_frame_size(codegen, s);
            break;
        case NODE_IF_STMT:
            total += collect_frame_size(codegen, s->ast.if_stmt.then);
            total += collect_frame_size(codegen, s->ast.if_stmt.otherwise);
            break;
        case NODE_WHILE_STMT:
        case NODE_DO_WHILE_STMT:
            total += collect_frame_size(codegen, s->ast.while_stmt.body);
            break;
        case NODE_FOR_STMT:
            if(s->ast.for_stmt.init &&
                s->ast.for_stmt.init->type == NODE_VAR_DECL) {
                total += 8;
            }
            total += collect_frame_size(codegen, s->ast.for_stmt.body);
            break;
        case NODE_SWITCH_STMT:
            for(size_t c = 0; c < s->ast.switch_stmt.cases->count; ++c) {
                Node* clause = s->ast.switch_stmt.cases->items[c];
                total += collect_frame_size(codegen, clause->ast.case_clause.body);
            }
            break;
        default:
            break;
        }
    }
    return total;
}

static long long switch_const_value(CodeGen *codegen, Node *case_expr) {
    if(case_expr->type == NODE_INT_LIT || case_expr->type == NODE_HEXA_LIT) {
        return case_expr->ast.numeric_literal.value;
    }

    if(case_expr->type == NODE_CHAR_LIT) {
        return decode_char_literal(case_expr->ast.string_literal.text);
    }

    if(case_expr->type == NODE_VAR_ACCESS) {
        Symbol* sym = lookup_table(codegen->table, case_expr->ast.var_access.name);
        if(sym && sym->has_const_value) {
            return sym->const_value;
        }
    }

    return 0;
}

static void codegen_switch_jumptable(CodeGen *codegen, NodeList *cases,
                                        long long min_val, long long max_val,
                                        int label_end, int *case_labels) {
    size_t range = (size_t)(max_val - min_val + 1);

    TAB; T("mov rbx, rax"); NL;
    TAB; T("sub rbx, "); TI(min_val); NL;
    TAB; T("cmp rbx, "); TI((long long)range); NL;
    TAB; T("jae "); emit_label_ref(codegen, label_end); NL;

    int label_table = new_label(codegen);

    TAB; T("lea rax, [rel __L"); TI(label_table); T("]"); NL;
    TAB; T("mov rax, [rax + rbx*8]"); NL;
    TAB; T("jmp rax"); NL;

    D("__L"); codegen_buffer_writeu(&codegen->sec_data, (size_t)label_table); D(":\n");

    for(size_t i = 0; i < range; ++i) {
        long long want = min_val + (long long)i;
        int target = label_end;

        for(size_t c = 0; c < cases->count; ++c) {
            Node* clause = cases->items[c];
            if(switch_const_value(codegen, clause->ast.case_clause.expr) == want) {
                target = case_labels[c];
                break;
            }
        }

        D("    dq __L"); codegen_buffer_writeu(&codegen->sec_data, (size_t)target); DNL;
    }
}

static void codegen_switch_chain(CodeGen *codegen, NodeList *cases,
                                  int label_end, int *case_labels) {
    TAB; T("mov rbx, rax"); NL;

    for(size_t c = 0; c < cases->count; ++c) {
        Node* clause = cases->items[c];
        long long cv = switch_const_value(codegen, clause->ast.case_clause.expr);
        TAB; T("cmp rbx, "); TI(cv); NL;
        TAB; T("je "); emit_label_ref(codegen, case_labels[c]); NL;
    }

    TAB; T("jmp "); emit_label_ref(codegen, label_end); NL;
}

static void codegen_switch(CodeGen *codegen, Node *node) {
    NodeList* cases = node->ast.switch_stmt.cases;
    if(!cases || cases->count == 0) return;

    int label_end = new_label(codegen);

    int* case_labels = (int*)arena_alloc(codegen->arena, sizeof(int) * cases->count);
    for(size_t i = 0; i < cases->count; ++i)
        case_labels[i] = new_label(codegen);

    break_push(codegen, BREAK_CTX_SWITCH, label_end);

    codegen_expr(codegen, node->ast.switch_stmt.subject);

    long long min_val = switch_const_value(codegen, ((Node*)cases->items[0])->ast.case_clause.expr);
    long long max_val = min_val;
    for(size_t i = 1; i < cases->count; ++i) {
        long long v = switch_const_value(codegen, ((Node*)cases->items[i])->ast.case_clause.expr);
        if(v < min_val) min_val = v;
        if(v > max_val) max_val = v;
    }

    long long range = max_val - min_val + 1;
    bool use_jumptable = (range > 0) && (range <= (long long)cases->count * 2) && (range <= 4096);

    if(use_jumptable)
        codegen_switch_jumptable(codegen, cases, min_val, max_val, label_end, case_labels);
    else
        codegen_switch_chain(codegen, cases, label_end, case_labels);

    for(size_t i = 0; i < cases->count; ++i) {
        Node* clause = cases->items[i];
        emit_label_ref(codegen, case_labels[i]); T(":"); NL;
        codegen_stmt(codegen, clause->ast.case_clause.body);
    }

    emit_label_ref(codegen, label_end); T(":"); NL;

    break_pop(codegen);
}

static bool emit_lvalue_addr(CodeGen *codegen, Node *expr) {
    if(!expr) return false;

    if(expr->type == NODE_VAR_ACCESS) {
        CodeGenLocal* local = local_find(codegen, expr->ast.var_access.name);
        if(local) { emit_lea_local(codegen, local->offset); return true; }
        TAB; T("lea rax, [rel __g_");
        codegen_buffer_write(&codegen->sec_text,
            expr->ast.var_access.name.start, expr->ast.var_access.name.len);
        T("]"); NL;
        return true;
    }

    if(expr->type == NODE_UNARY_OP && expr->ast.unary_op.op == TOKEN_OP_STAR) {
        codegen_expr(codegen, expr->ast.unary_op.operand);
        return true;
    }

    if(expr->type == NODE_BINARY_OP &&
        (expr->ast.binary_op.op == TOKEN_DOT ||
        expr->ast.binary_op.op == TOKEN_OP_ARROW)) {
        Node* lhs        = expr->ast.binary_op.left;
        Node* rhs        = expr->ast.binary_op.right;
        View  field_name = rhs->ast.var_access.name;

        View typename        = {0};
        bool is_union_access = false;

        if(lhs->type == NODE_VAR_ACCESS) {
            CodeGenLocal* local = local_find(codegen, lhs->ast.var_access.name);
            if(local) {
                typename = local->typename;
            } else {
                Symbol* sym = lookup_table(codegen->table, lhs->ast.var_access.name);
                if(sym) typename = sym->typename;
            }
        }

        Symbol* sym = lookup_table(codegen->table, lhs->ast.var_access.name);
        if(sym) is_union_access = (sym->type == TOKEN_KEYWORD_UNION);

        int field_off = codegen_field_offset(codegen, typename, field_name, is_union_access);

        if(expr->ast.binary_op.op == TOKEN_DOT) {
            if(lhs->type == NODE_VAR_ACCESS) {
                CodeGenLocal* local = local_find(codegen, lhs->ast.var_access.name);
                if(local) {
                    emit_lea_local(codegen, local->offset);
                } else {
                    codegen_expr(codegen, lhs);
                }
            }
        } else {
            codegen_expr(codegen, lhs);
        }

        if(field_off != 0) {
            TAB; T("add rax, "); TI(field_off); NL;
        }
        return true;
    }

    if(expr->type == NODE_ARRAY) {
        Node* base         = expr->ast.binary_op.left;
        int   element_size = element_size_for(codegen, base);

        codegen_expr(codegen, expr->ast.binary_op.right);
        if(element_size != 1) {
            TAB; T("imul rax, "); TI(element_size); NL;
        }
        emit_mov_reg_reg(codegen, "rbx", "rax");
        codegen_expr(codegen, expr->ast.binary_op.left);
        TAB; T("add rax, rbx"); NL;
        return true;
    }

    return false;
}

static void codegen_expr(CodeGen *codegen, Node *expr) {
    if(!expr) return;

    switch(expr->type) {

    case NODE_INT_LIT:
        emit_mov_reg_imm(codegen, "rax", expr->ast.numeric_literal.value);
        break;

    case NODE_BOOL_LIT:
        emit_mov_reg_imm(codegen, "rax", expr->ast.numeric_literal.value);
        break;

    case NODE_HEXA_LIT:
        emit_mov_reg_imm(codegen, "rax", expr->ast.numeric_literal.value);
        break;

    case NODE_DOUBLE_LIT:
        emit_mov_reg_imm(codegen, "rax", (long long)expr->ast.numeric_literal.value);
        break;

    case NODE_CHAR_LIT: {
        long long byte = decode_char_literal(expr->ast.string_literal.text);
        emit_mov_reg_imm(codegen, "rax", byte);
        break;
    }

    case NODE_STRING_LIT: {
        size_t idx = string_intern(codegen, expr->ast.string_literal.text);
        TAB; T("lea rax, [rel __str"); TU(idx); T("]"); NL;
        break;
    }

    case NODE_VAR_ACCESS: {
        CodeGenLocal* local = local_find(codegen, expr->ast.var_access.name);
        if(local) { emit_load_local(codegen, local->offset); break; }

        Symbol* sym = lookup_table(codegen->table, expr->ast.var_access.name);
        if(sym && sym->has_const_value) {
            emit_mov_reg_imm(codegen, "rax", sym->const_value);
            break;
        }

        TAB; T("mov rax, [rel __g_");
        codegen_buffer_write(&codegen->sec_text,
            expr->ast.var_access.name.start, expr->ast.var_access.name.len);
        T("]"); NL;
        break;
    }

    case NODE_UNARY_OP: {
        TokenType op = expr->ast.unary_op.op;

        if(op == TOKEN_BIT_AND) {
            if(!emit_lvalue_addr(codegen, expr->ast.unary_op.operand))
                T("; TODO: address-of de expressao nao suportada\n");
            break;
        }

        if(op == TOKEN_OP_STAR) {
            codegen_expr(codegen, expr->ast.unary_op.operand);
            TAB; T("mov rax, [rax]"); NL;
            break;
        }

        codegen_expr(codegen, expr->ast.unary_op.operand);
        switch(op) {
        case TOKEN_OP_MINUS:
            TAB; T("neg rax"); NL; break;
        case TOKEN_OP_BANG:
            TAB; T("test rax, rax"); NL;
            TAB; T("sete al"); NL;
            TAB; T("movzx rax, al"); NL;
            break;
        case TOKEN_OP_DESC:
            TAB; T("not rax"); NL; break;
        case TOKEN_OP_PLUS_PLUS: {
            Node* operand = expr->ast.unary_op.operand;
            if(operand->type == NODE_VAR_ACCESS) {
                CodeGenLocal* loc = local_find(codegen, operand->ast.var_access.name);
                if(loc) {
                    TAB; T("inc qword [rbp"); TI(loc->offset); T("]"); NL;
                    emit_load_local(codegen, loc->offset);
                }
            }
            break;
        }
        case TOKEN_OP_MINUS_MINUS: {
            Node* operand = expr->ast.unary_op.operand;
            if(operand->type == NODE_VAR_ACCESS) {
                CodeGenLocal* loc = local_find(codegen, operand->ast.var_access.name);
                if(loc) {
                    TAB; T("dec qword [rbp"); TI(loc->offset); T("]"); NL;
                    emit_load_local(codegen, loc->offset);
                }
            }
            break;
        }
        default: break;
        }
        break;
    }

    case NODE_POSTFIX_OP: {
        Node* operand = expr->ast.unary_op.operand;
        if(operand->type == NODE_VAR_ACCESS) {
            CodeGenLocal* loc = local_find(codegen, operand->ast.var_access.name);
            if(loc) {
                emit_load_local(codegen, loc->offset);
                TAB; T("push rax"); NL;
                if(expr->ast.unary_op.op == TOKEN_OP_PLUS_PLUS)
                    { TAB; T("inc qword [rbp"); TI(loc->offset); T("]"); NL; }
                else
                    { TAB; T("dec qword [rbp"); TI(loc->offset); T("]"); NL; }
                TAB; T("pop rax"); NL;
            }
        }
        break;
    }

    case NODE_BINARY_OP: {
        TokenType op = expr->ast.binary_op.op;

        if(op == TOKEN_OP_ASSIGN) {
            codegen_expr(codegen, expr->ast.binary_op.right);
            Node* lhs = expr->ast.binary_op.left;

            if(lhs->type == NODE_UNARY_OP && lhs->ast.unary_op.op == TOKEN_OP_STAR) {
                TAB; T("push rax"); NL;
                codegen_expr(codegen, lhs->ast.unary_op.operand);
                emit_mov_reg_reg(codegen, "rbx", "rax");
                TAB; T("pop rax"); NL;
                TAB; T("mov [rbx], rax"); NL;
                break;
            }

            if(lhs->type == NODE_VAR_ACCESS) {
                CodeGenLocal* loc = local_find(codegen, lhs->ast.var_access.name);
                if(loc) { emit_store_local(codegen, loc->offset); break; }

                TAB; T("mov [rel __g_");
                codegen_buffer_write(&codegen->sec_text,
                    lhs->ast.var_access.name.start, lhs->ast.var_access.name.len);
                T("], rax"); NL;
                break;
            }

            if(lhs->type == NODE_ARRAY) {
                TAB; T("push rax"); NL;
                emit_lvalue_addr(codegen, lhs);
                emit_mov_reg_reg(codegen, "rbx", "rax");
                TAB; T("pop rax"); NL;
                TAB; T("mov [rbx], rax"); NL;
                break;
            }

            if(lhs->type == NODE_BINARY_OP &&
                (lhs->ast.binary_op.op == TOKEN_DOT ||
                lhs->ast.binary_op.op == TOKEN_OP_ARROW)) {
                TAB; T("push rax"); NL;
                emit_lvalue_addr(codegen, lhs);
                emit_mov_reg_reg(codegen, "rbx", "rax");
                TAB; T("pop rax"); NL;
                TAB; T("mov [rbx], rax"); NL;
            }
            break;
        }

        if(op == TOKEN_DOT || op == TOKEN_OP_ARROW) {
            Node* lhs        = expr->ast.binary_op.left;
            Node* rhs        = expr->ast.binary_op.right;
            View  field_name = rhs->ast.var_access.name;

            View typename        = {0};
            bool is_union_access = false;

            if(lhs->type == NODE_VAR_ACCESS) {
                CodeGenLocal* loc = local_find(codegen, lhs->ast.var_access.name);
                if(loc) {
                    typename = loc->typename;
                } else {
                    Symbol* sym = lookup_table(codegen->table, lhs->ast.var_access.name);
                    if(sym) typename = sym->typename;
                }
            }

            Symbol* sym = lookup_table(codegen->table, typename);
            if(sym) is_union_access = (sym->type == TOKEN_KEYWORD_UNION);

            int    field_off  = codegen_field_offset(codegen, typename, field_name, is_union_access);
            size_t field_size = codegen_field_size(codegen, typename, field_name);

            if(op == TOKEN_DOT) {
                if(lhs->type == NODE_VAR_ACCESS) {
                    CodeGenLocal* loc = local_find(codegen, lhs->ast.var_access.name);
                    if(loc) {
                        emit_lea_local(codegen, loc->offset);
                    } else {
                        TAB; T("lea rax, [rel __g_");
                        codegen_buffer_write(&codegen->sec_text,
                            lhs->ast.var_access.name.start,
                            lhs->ast.var_access.name.len);
                        T("]"); NL;
                    }
                } else {
                    codegen_expr(codegen, lhs);
                }
            } else {
                codegen_expr(codegen, lhs);
            }

            if(field_off != 0) {
                TAB; T("add rax, "); TI(field_off); NL;
            }

            switch(field_size) {
            case 1: TAB; T("movzx rax, byte [rax]");    NL; break;
            case 2: TAB; T("movzx rax, word [rax]");    NL; break;
            case 4: TAB; T("movsxd rax, dword [rax]");  NL; break;
            default: TAB; T("mov rax, [rax]");           NL; break;
            }
            break;
        }

        if(op == TOKEN_OP_PLUS_EQ  || op == TOKEN_OP_MINUS_EQ ||
            op == TOKEN_OP_MOD_EQ   || op == TOKEN_OP_STAR_EQ  ||
            op == TOKEN_OP_SLASH_EQ) {
            Node* lhs = expr->ast.binary_op.left;
            CodeGenLocal* loc = (lhs->type == NODE_VAR_ACCESS)
                                ? local_find(codegen, lhs->ast.var_access.name)
                                : NULL;
            if(loc) {
                codegen_expr(codegen, expr->ast.binary_op.right);
                emit_mov_reg_reg(codegen, "rbx", "rax");
                emit_load_local(codegen, loc->offset);
                switch(op) {
                case TOKEN_OP_PLUS_EQ:
                    TAB; T("add rax, rbx"); NL; break;
                case TOKEN_OP_MINUS_EQ:
                    TAB; T("sub rax, rbx"); NL; break;
                case TOKEN_OP_STAR_EQ:
                    TAB; T("imul rax, rbx"); NL; break;
                case TOKEN_OP_SLASH_EQ:
                    TAB; T("cqo"); NL; TAB; T("idiv rbx"); NL; break;
                case TOKEN_OP_MOD_EQ:
                    TAB; T("cqo"); NL; TAB; T("idiv rbx"); NL;
                    emit_mov_reg_reg(codegen, "rax", "rdx"); break;
                default: break;
                }
                emit_store_local(codegen, loc->offset);
            }
            break;
        }

        codegen_expr(codegen, expr->ast.binary_op.left);
        TAB; T("push rax"); NL;
        codegen_expr(codegen, expr->ast.binary_op.right);
        emit_mov_reg_reg(codegen, "rbx", "rax");
        TAB; T("pop rax"); NL;

        switch(op) {
        case TOKEN_OP_PLUS:    TAB; T("add rax, rbx"); NL; break;
        case TOKEN_OP_MINUS:   TAB; T("sub rax, rbx"); NL; break;
        case TOKEN_OP_STAR:    TAB; T("imul rax, rbx"); NL; break;
        case TOKEN_BIT_AND:    TAB; T("and rax, rbx"); NL; break;
        case TOKEN_BIT_OR:     TAB; T("or rax, rbx"); NL; break;
        case TOKEN_OP_XOR:     TAB; T("xor rax, rbx"); NL; break;
        case TOKEN_OP_SLASH:
            TAB; T("cqo"); NL; TAB; T("idiv rbx"); NL; break;
        case TOKEN_OP_MOD:
            TAB; T("cqo"); NL; TAB; T("idiv rbx"); NL;
            emit_mov_reg_reg(codegen, "rax", "rdx"); break;
        case TOKEN_BIT_LSHIFT:
            TAB; T("mov rcx, rbx"); NL; TAB; T("sal rax, cl"); NL; break;
        case TOKEN_BIT_RSHIFT:
            TAB; T("mov rcx, rbx"); NL; TAB; T("sar rax, cl"); NL; break;
        case TOKEN_OP_EQ_EQ:
            TAB; T("cmp rax, rbx"); NL; TAB; T("sete al"); NL;
            TAB; T("movzx rax, al"); NL; break;
        case TOKEN_OP_BANG_EQ:
            TAB; T("cmp rax, rbx"); NL; TAB; T("setne al"); NL;
            TAB; T("movzx rax, al"); NL; break;
        case TOKEN_OP_LT:
            TAB; T("cmp rax, rbx"); NL; TAB; T("setl al"); NL;
            TAB; T("movzx rax, al"); NL; break;
        case TOKEN_OP_LT_EQ:
            TAB; T("cmp rax, rbx"); NL; TAB; T("setle al"); NL;
            TAB; T("movzx rax, al"); NL; break;
        case TOKEN_OP_GT:
            TAB; T("cmp rax, rbx"); NL; TAB; T("setg al"); NL;
            TAB; T("movzx rax, al"); NL; break;
        case TOKEN_OP_GT_EQ:
            TAB; T("cmp rax, rbx"); NL; TAB; T("setge al"); NL;
            TAB; T("movzx rax, al"); NL; break;
        case TOKEN_OP_AND:
            TAB; T("and rax, rbx"); NL;
            TAB; T("setne al"); NL;
            TAB; T("movzx rax, al"); NL; break;
        case TOKEN_OP_OR:
            TAB; T("or rax, rbx"); NL;
            TAB; T("setne al"); NL;
            TAB; T("movzx rax, al"); NL; break;
        default: break;
        }
        break;
    }

    case NODE_FUNC_CALL: {
        if (view_eq_str(expr->ast.func_call.name, "__va_start")) {
            if (!codegen->has_va_reg_save) {
                break;
            }

            TAB; T("mov rax, rsp"); NL;
            TAB; T("and rsp, -16"); NL;
            TAB; T("push rax"); NL;

            TAB; T("lea rcx, [rbp-"); TI(codegen->va_reg_save_offset); T("]"); NL;

            TAB; T("sub rsp, 32"); NL;
            TAB; T("call __va_start"); NL;
            TAB; T("add rsp, 32"); NL;

            TAB; T("mov rbx, rax"); NL;
            TAB; T("pop rax"); NL;
            TAB; T("mov rsp, rax"); NL;
            TAB; T("mov rax, rbx"); NL;
            break;
        }

        NodeList* args = expr->ast.func_call.args;
        size_t    argc = args ? args->count : 0;

        TAB; T("mov rax, rsp"); NL;
        TAB; T("and rsp, -16"); NL;
        TAB; T("push rax"); NL;

        if(argc > WIN64_INT_PARAMS) {
            for(size_t i = argc - 1; i >= WIN64_INT_PARAMS; --i) {
                codegen_expr(codegen, args->items[i]);
                TAB; T("push rax"); NL;
            }
        }

        size_t reg_args = (argc < WIN64_INT_PARAMS) ? argc : WIN64_INT_PARAMS;
        for(size_t i = 0; i < reg_args; ++i) {
            codegen_expr(codegen, args->items[i]);
            TAB; T("push rax"); NL;
        }

        for(int i = (int)reg_args - 1; i >= 0; --i) {
            TAB; T("pop "); T(WIN64_INT_REGISTERS[i]); NL;
        }

        TAB; T("sub rsp, 32"); NL;
        TAB; T("call ");
        codegen_buffer_write(&codegen->sec_text,
            expr->ast.func_call.name.start, expr->ast.func_call.name.len);
        NL;
        TAB; T("add rsp, 32"); NL;

        if(argc > WIN64_INT_PARAMS) {
            size_t extra = argc - WIN64_INT_PARAMS;
            TAB; T("add rsp, "); TU(extra * 8); NL;
        }

        TAB; T("mov rbx, rax"); NL;
        TAB; T("pop rax"); NL;
        TAB; T("mov rsp, rax"); NL;
        TAB; T("mov rax, rbx"); NL;
        break;
    }

    case NODE_ARRAY: {
        Node* base         = expr->ast.binary_op.left;
        int   element_size = element_size_for(codegen, base);

        codegen_expr(codegen, expr->ast.binary_op.right);
        if(element_size != 1) {
            TAB; T("imul rax, "); TI(element_size); NL;
        }
        emit_mov_reg_reg(codegen, "rbx", "rax");
        codegen_expr(codegen, expr->ast.binary_op.left);
        TAB; T("add rax, rbx"); NL;

        switch(element_size) {
        case 1:  TAB; T("movzx rax, byte [rax]");   NL; break;
        case 2:  TAB; T("movzx rax, word [rax]");   NL; break;
        case 4:  TAB; T("movsxd rax, dword [rax]"); NL; break;
        default: TAB; T("mov rax, [rax]");           NL; break;
        }
        break;
    }

    default:
        T("; TODO: expr "); TI((int)expr->type); NL;
        break;
    }
}

static void codegen_block(CodeGen *codegen, Node *block) {
    if(!block) return;
    NodeList* stmts = block->ast.program.declarations;
    if(!stmts) return;
    for(size_t i = 0; i < stmts->count; ++i)
        codegen_stmt(codegen, stmts->items[i]);
}

static void codegen_stmt(CodeGen *codegen, Node *stmt) {
    if(!stmt) return;

    Node* current = stmt;
    while(current) {
        switch(current->type) {

        case NODE_VAR_DECL: {
            size_t var_size = 8;
            if(current->ast.var_decl.pointer_lvl == 0) {
                TokenType data = current->ast.var_decl.data_type;
                if(data == TOKEN_KEYWORD_STRUCT || data == TOKEN_KEYWORD_UNION) {
                    Symbol* sym = lookup_table(codegen->table, current->ast.var_decl.typename);
                    if(sym && sym->category == SYMBOL_STRUCT) {
                        var_size = (data == TOKEN_KEYWORD_UNION)
                                    ? codegen_union_size(codegen, current->ast.var_decl.typename)
                                    : codegen_struct_size(codegen, current->ast.var_decl.typename);
                    }
                } else {
                    var_size = (size_t)bytes_data_value(data);
                }
            }

            int offset = local_alloc(codegen,
                current->ast.var_decl.name,
                current->ast.var_decl.typename,
                var_size,
                current->ast.var_decl.data_type,
                current->ast.var_decl.pointer_lvl);

            T("; var ");
            codegen_buffer_write(&codegen->sec_text,
                current->ast.var_decl.name.start, current->ast.var_decl.name.len);
            T(" @ rbp"); TI(offset); NL;

            if(current->ast.var_decl.init) {
                codegen_expr(codegen, current->ast.var_decl.init);
                emit_store_local(codegen, offset);
            } else {
                TAB; T("mov qword [rbp"); TI(offset); T("], 0"); NL;
            }
            break;
        }

        case NODE_SWITCH_STMT:
            codegen_switch(codegen, current);
            break;

        case NODE_RETURN_STMT: {
            if(current->ast.return_stmt.value)
                codegen_expr(codegen, current->ast.return_stmt.value);
            else
                emit_mov_reg_imm(codegen, "rax", 0);
            emit_epilogue(codegen);
            break;
        }

        case NODE_BLOCK: {
            CodeGenLocal* saved_local     = codegen->locals;
            int           saved_stack_top = codegen->stack_top;
            codegen_block(codegen, current);
            codegen->locals    = saved_local;
            codegen->stack_top = saved_stack_top;
            break;
        }

        case NODE_IF_STMT: {
            int label_else = new_label(codegen);
            int label_end  = new_label(codegen);

            codegen_expr(codegen, current->ast.if_stmt.condition);
            TAB; T("test rax, rax"); NL;
            TAB; T("jz "); emit_label_ref(codegen, label_else); NL;

            codegen_stmt(codegen, current->ast.if_stmt.then);
            if(current->ast.if_stmt.otherwise) {
                TAB; T("jmp "); emit_label_ref(codegen, label_end); NL;
            }

            emit_label_ref(codegen, label_else); T(":"); NL;

            if(current->ast.if_stmt.otherwise) {
                codegen_stmt(codegen, current->ast.if_stmt.otherwise);
                emit_label_ref(codegen, label_end); T(":"); NL;
            }
            break;
        }

        case NODE_WHILE_STMT: {
            int label_cond = new_label(codegen);
            int label_end  = new_label(codegen);

            loop_push(codegen, label_cond, label_end);
            break_push(codegen, BREAK_CTX_LOOP, label_end);

            CodeGenLocal* saved_local     = codegen->locals;
            int           saved_stack_top = codegen->stack_top;

            emit_label_ref(codegen, label_cond); T(":"); NL;
            codegen_expr(codegen, current->ast.while_stmt.condition);
            TAB; T("test rax, rax"); NL;
            TAB; T("jz "); emit_label_ref(codegen, label_end); NL;
            codegen_stmt(codegen, current->ast.while_stmt.body);
            TAB; T("jmp "); emit_label_ref(codegen, label_cond); NL;
            emit_label_ref(codegen, label_end); T(":"); NL;

            codegen->locals    = saved_local;
            codegen->stack_top = saved_stack_top;

            loop_pop(codegen);
            break_pop(codegen);
            break;
        }

        case NODE_DO_WHILE_STMT: {
            int label_body = new_label(codegen);
            int label_cond = new_label(codegen);
            int label_end  = new_label(codegen);

            loop_push(codegen, label_cond, label_end);
            break_push(codegen, BREAK_CTX_LOOP, label_end);

            emit_label_ref(codegen, label_body); T(":"); NL;
            codegen_stmt(codegen, current->ast.while_stmt.body);

            emit_label_ref(codegen, label_cond); T(":"); NL;
            codegen_expr(codegen, current->ast.while_stmt.condition);
            TAB; T("test rax, rax"); NL;
            TAB; T("jnz "); emit_label_ref(codegen, label_body); NL;
            emit_label_ref(codegen, label_end); T(":"); NL;

            loop_pop(codegen);
            break_pop(codegen);
            break;
        }

        case NODE_FOR_STMT: {
            int label_cond = new_label(codegen);
            int label_incr = new_label(codegen);
            int label_end  = new_label(codegen);

            loop_push(codegen, label_cond, label_end);
            break_push(codegen, BREAK_CTX_LOOP, label_end);

            CodeGenLocal* saved_locals    = codegen->locals;
            int           saved_stack_top = codegen->stack_top;

            if(current->ast.for_stmt.init)
                codegen_stmt(codegen, current->ast.for_stmt.init);

            emit_label_ref(codegen, label_cond); T(":"); NL;

            if(current->ast.for_stmt.condition) {
                codegen_expr(codegen, current->ast.for_stmt.condition);
                TAB; T("test rax, rax"); NL;
                TAB; T("jz "); emit_label_ref(codegen, label_end); NL;
            }

            codegen_stmt(codegen, current->ast.for_stmt.body);

            emit_label_ref(codegen, label_incr); T(":"); NL;
            if(current->ast.for_stmt.increment)
                codegen_expr(codegen, current->ast.for_stmt.increment);

            TAB; T("jmp "); emit_label_ref(codegen, label_cond); NL;
            emit_label_ref(codegen, label_end); T(":"); NL;

            codegen->locals    = saved_locals;
            codegen->stack_top = saved_stack_top;

            loop_pop(codegen);
            break_pop(codegen);
            break;
        }

        case NODE_BREAK_STMT:
            if(codegen->break_depth > 0) {
                int lbl = codegen->break_stack[codegen->break_depth - 1].label_end;
                TAB; T("jmp "); emit_label_ref(codegen, lbl); NL;
            } else {
                T("; break fora de loop/switch\n");
            }
            break;

        case NODE_CONTINUE_STMT:
            if(codegen->loop_depth > 0) {
                int lbl = codegen->loop_stack[codegen->loop_depth - 1].label_cond;
                TAB; T("jmp "); emit_label_ref(codegen, lbl); NL;
            } else {
                T("; continue fora de loop\n");
            }
            break;

        case NODE_BINARY_OP:
        case NODE_UNARY_OP:
        case NODE_POSTFIX_OP:
        case NODE_FUNC_CALL:
            codegen_expr(codegen, current);
            break;

        case NODE_STRUCT:
        case NODE_UNION:
        case NODE_ENUM:
        case NODE_TYPEDEF_DECL:
            break;

        default:
            T("; TODO: stmt "); TI((int)current->type); NL;
            break;
        }

        current = current->next;
    }
}

static void codegen_function(CodeGen *codegen, Node *node) {
    View name     = node->ast.func_decl.name;
    bool is_entry = view_eq_str(name, codegen->entry_name);
    bool is_void  = (node->ast.func_decl.return_type == TOKEN_KEYWORD_VOID);

    codegen->locals    = NULL;
    codegen->stack_top = 0;

    NodeList* params = node->ast.func_decl.params;

    int frame = collect_frame_size(codegen, node->ast.func_decl.body);
    frame += (params ? (int)params->count * 8 : 0);

    int reg_save_base = 0;
    if (node->ast.func_decl.is_variadic) {
        frame += WIN64_SHADOW_SPACE;
        reg_save_base = frame;
    }

    NL;
    codegen_buffer_write(&codegen->sec_text, name.start, name.len);
    T(":"); NL;
    emit_prologue(codegen, frame);

    if(params) {
        for(size_t i = 0; i < params->count && i < (size_t)WIN64_INT_PARAMS; ++i) {
            Node* param = params->items[i];
            int   off   = local_alloc(codegen,
                            param->ast.var_decl.name,
                            param->ast.var_decl.typename,
                            8,
                            param->ast.var_decl.data_type,
                            param->ast.var_decl.pointer_lvl);
            TAB; T("mov qword [rbp"); TI(off); T("], ");
            T(WIN64_INT_REGISTERS[i]); NL;
        }
    }

    if(node->ast.func_decl.is_variadic) {
        codegen->has_va_reg_save    = true;
        codegen->va_reg_save_offset = reg_save_base;

        TAB; T("mov [rbp-"); TI(reg_save_base);      T("], rcx"); NL;
        TAB; T("mov [rbp-"); TI(reg_save_base - 8);  T("], rdx"); NL;
        TAB; T("mov [rbp-"); TI(reg_save_base - 16); T("], r8");  NL;
        TAB; T("mov [rbp-"); TI(reg_save_base - 24); T("], r9");  NL;
    } else {
        codegen->has_va_reg_save = false;
    }

    codegen_block(codegen, node->ast.func_decl.body);

    if(is_entry && is_void)
        emit_mov_reg_imm(codegen, "rax", 0);

    emit_epilogue(codegen);
    codegen->has_va_reg_save = false;
}

static const char* bss_directive(int bytes) {
    switch (bytes)
    {
    case 1: return "resb";
    case 2: return "resw";
    case 3: return "resd";
    default: return "resq";
    }
}

static void codegen_global_var(CodeGen *codegen, Node *node) {
    int var_size = 0;
    if(node->ast.var_decl.pointer_lvl == 0) {
        var_size = bytes_data_value(node->ast.var_decl.data_type);
    }

    codegen_buffer_writez(&codegen->sec_bss, "__g_");
    codegen_buffer_write(&codegen->sec_bss,
        node->ast.var_decl.name.start, node->ast.var_decl.name.len);
    codegen_buffer_writez(&codegen->sec_bss, ": ");
    codegen_buffer_writez(&codegen->sec_bss, bss_directive(var_size));
    codegen_buffer_writez(&codegen->sec_bss, " 1\n");

    if(node->ast.var_decl.init) {
        #define TI_INIT(value) codegen_buffer_writei(&codegen->sec_init, (long long)(value))
        #define T_INIT(s) codegen_buffer_writez(&codegen->sec_init, (s))
        #define NL_INIT codegen_buffer_writec(&codegen->sec_init, '\n')

        T_INIT("; init global ");
        codegen_buffer_write(&codegen->sec_init,
            node->ast.var_decl.name.start, node->ast.var_decl.name.len);
        NL_INIT;
        
        codegen->in_func = true;
        codegen_expr(codegen, node->ast.var_decl.init);
        codegen->in_func = false;

        switch (var_size)
        {
        case 1: {
            T_INIT("    mov byte [rel __g_");
            codegen_buffer_write(&codegen->sec_init, node->ast.var_decl.name.start, node->ast.var_decl.name.len);
            T_INIT("], al\n");
            break;
        }
        case 2: {
            T_INIT("    mov word [rel __g_");
            codegen_buffer_write(&codegen->sec_init, node->ast.var_decl.name.start, node->ast.var_decl.name.len);
            T_INIT("], ax\n");
            break;
        }
        case 4: {
            T_INIT("    mov dword [rel __g_");
            codegen_buffer_write(&codegen->sec_init, node->ast.var_decl.name.start, node->ast.var_decl.name.len);
            T_INIT("], eax\n");
            break;
        }
        default:
            T_INIT("    mov qword [rel __g_");
            codegen_buffer_write(&codegen->sec_init, node->ast.var_decl.name.start, node->ast.var_decl.name.len);
            T_INIT("], rax\n");
            break;
        }

        #undef TI_INIT
        #undef T_INIT
        #undef NL_INIT
    }
}

static void emit_builtin_write_stdout(CodeGen *codegen) {
    PROLOG("__write_stdout", 64);

    SAVE_PARAM_RCX(8);
    SAVE_PARAM_RDX(16);

    TAB; T("mov rcx, 0xFFFFFFFFFFFFFFF5"); NL;
    TAB; T("call [rel __imp_GetStdHandle]"); NL;

    TAB; T("mov qword [rbp-24], 0"); NL;
    TAB; T("lea r9, [rbp-24]"); NL;

    TAB; T("mov rcx, rax"); NL;
    TAB; T("mov rdx, [rbp-8]"); NL;
    TAB; T("mov r8d, dword [rbp-16]"); NL;
    TAB; T("mov qword [rsp+32], 0"); NL;

    TAB; T("call [rel __imp_WriteFile]"); NL;

    EPILOG;
}

static void emit_builtin_exit(CodeGen *codegen) {
    PROLOG("__exit", 32);

    TAB; T("call [rel __imp_ExitProcess]"); NL;
    TAB; T("hlt"); NL;
}

static void emit_builtin_write_int(CodeGen *codegen) {
    PROLOG("__write_int", 48);

    SAVE_PARAM_RCX(8);

    TAB; T("mov rcx, [rbp-8]"); NL;
    TAB; T("call __int_to_str"); NL;

    TAB; T("mov rcx, rax"); NL;
    TAB; T("call __write_stdout"); NL;

    EPILOG;
}

static void emit_builtin_malloc(CodeGen *codegen) {
    PROLOG("__malloc", 32);

    TAB; T("mov rdx, rcx"); NL;
    TAB; T("xor rcx, rcx"); NL;
    TAB; T("mov r8, 0x3000"); NL;
    TAB; T("mov r9, 0x04"); NL;

    TAB; T("sub rsp, 32"); NL;
    TAB; T("call [rel __imp_VirtualAlloc]"); NL;
    TAB; T("add rsp, 32"); NL;

    EPILOG;
}

static void emit_builtin_free(CodeGen *codegen) {
    PROLOG("__free", 32);

    TAB; T("mov rdx, 0"); NL;
    TAB; T("mov r8, 0x8000"); NL;

    TAB; T("sub rsp, 32"); NL;
    TAB; T("call [rel __imp_VirtualFree]"); NL;
    TAB; T("add rsp, 32"); NL;

    EPILOG;
}

static void emit_builtin_parse_args(CodeGen *codegen) {
    PROLOG("__parse_args", 48);

    SAVE_PARAM_RCX(8);

    TAB; T("sub rsp, 32"); NL;
    TAB; T("call [rel __imp_GetCommandLineA]"); NL;
    TAB; T("add rsp, 32"); NL;

    TAB; T("mov rsi, rax"); NL;
    TAB; T("mov rdi, rax"); NL;

    T("__pargs_strlen:\n");
    TAB; T("cmp byte [rdi], 0"); NL;
    TAB; T("je __pargs_strlen_done"); NL;
    TAB; T("inc rdi"); NL;
    TAB; T("jmp __pargs_strlen"); NL;

    T("__pargs_strlen_done:\n");
    TAB; T("sub rdi, rsi"); NL;
    TAB; T("inc rdi"); NL;

    TAB; T("xor rcx, rcx"); NL;
    TAB; T("mov rdx, rdi"); NL;
    TAB; T("mov r8, 0x3000"); NL;
    TAB; T("mov r9, 0x04"); NL;
    TAB; T("sub rsp, 32"); NL;
    TAB; T("call [rel __imp_VirtualAlloc]"); NL;
    TAB; T("add rsp, 32"); NL;
    TAB; T("mov [rbp-16], rax"); NL;

    TAB; T("mov rdi, rax"); NL;

    T("__pargs_copy:\n");
    TAB; T("mov al, [rsi]"); NL;
    TAB; T("mov [rdi], al"); NL;
    TAB; T("test al, al"); NL;
    TAB; T("jz __pargs_copy_done"); NL;
    TAB; T("inc rsi"); NL;
    TAB; T("inc rdi"); NL;
    TAB; T("jmp __pargs_copy"); NL;

    T("__pargs_copy_done:\n");

    TAB; T("mov rsi, [rbp-16]"); NL;
    TAB; T("mov rdi, [rbp-8]"); NL;
    TAB; T("xor rcx, rcx"); NL;

    T("__pargs_skip_space:\n");
    TAB; T("movzx rax, byte [rsi]"); NL;
    TAB; T("test al, al"); NL;
    TAB; T("jz __pargs_done"); NL;
    TAB; T("cmp al, ' '"); NL;
    TAB; T("je __pargs_next_space"); NL;

    TAB; T("mov [rdi + rcx*8], rsi"); NL;
    TAB; T("inc rcx"); NL;

    T("__pargs_skip_token:\n");
    TAB; T("movzx rax, byte [rsi]"); NL;
    TAB; T("test al, al"); NL;
    TAB; T("jz __pargs_done"); NL;
    TAB; T("cmp al, ' '"); NL;
    TAB; T("je __pargs_null_term"); NL;
    TAB; T("inc rsi"); NL;
    TAB; T("jmp __pargs_skip_token"); NL;

    T("__pargs_null_term:\n");
    TAB; T("mov byte [rsi], 0"); NL;
    TAB; T("inc rsi"); NL;
    TAB; T("jmp __pargs_skip_space"); NL;

    T("__pargs_next_space:\n");
    TAB; T("inc rsi"); NL;
    TAB; T("jmp __pargs_skip_space"); NL;

    T("__pargs_done:\n");
    TAB; T("mov rax, rcx"); NL;

    EPILOG;
}

static void emit_builtin_parse_args_w(CodeGen *codegen) {
    PROLOG("__parse_args_w", 64);

    SAVE_PARAM_RCX(8);

    TAB; T("sub rsp, 32"); NL;
    TAB; T("call [rel __imp_GetCommandLineW]"); NL;
    TAB; T("add rsp, 32"); NL;
    TAB; T("mov rcx, rax"); NL;

    TAB; T("lea rdx, [rbp-16]"); NL;
    TAB; T("sub rsp, 32"); NL;
    TAB; T("call [rel __imp_CommandLineToArgvW]"); NL;
    TAB; T("add rsp, 32"); NL;
    TAB; T("mov [rbp-24], rax"); NL;

    TAB; T("mov eax, dword [rbp-16]"); NL;
    TAB; T("mov [rbp-32], rcx"); NL;

    TAB; T("xor rcx, rcx"); NL;
    TAB; T("mov rdx, 4096"); NL;
    TAB; T("mov r8, 0x3000"); NL;
    TAB; T("mov r9, 0x04"); NL;
    TAB; T("sub rsp, 32"); NL;
    TAB; T("call [rel __imp_VirtualAlloc]"); NL;
    TAB; T("add rsp, 32"); NL;
    TAB; T("mov [rbp-40], rax"); NL;
    TAB; T("mov [rbp-48], rax"); NL;

    TAB; T("xor rcx, rcx"); NL;

    T("__pargsw_loop:\n");
    TAB; T("cmp rcx, [rbp-32]"); NL;
    TAB; T("jge __pargsw_done"); NL;

    TAB; T("mov rax, [rbp-24]"); NL;
    TAB; T("mov rsi, [rax + rcx*8]"); NL;
    TAB; T("mov rdi, [rbp-48]"); NL;

    TAB; T("mov rax, [rbp-8]"); NL;
    TAB; T("mov [rax + rcx*8], rdi"); NL;

    T("__pargsw_copy_char:\n");
    TAB; T("mov ax, [rsi]"); NL;
    TAB; T("mov [rdi], ax"); NL;
    TAB; T("add rsi, 2"); NL;
    TAB; T("add rdi, 2"); NL;
    TAB; T("test ax, ax"); NL;
    TAB; T("jnz __pargsw_copy_char"); NL;

    TAB; T("mov [rbp-48], rdi"); NL;

    TAB; T("inc rcx"); NL;
    TAB; T("jmp __pargsw_loop"); NL;

    T("__pargsw_done:\n");
    TAB; T("mov rcx, [rbp-24]"); NL;
    TAB; T("sub rsp, 32"); NL;
    TAB; T("call [rel __imp_LocalFree]"); NL;
    TAB; T("add rsp, 32"); NL;

    TAB; T("mov rax, [rbp-32]"); NL;

    EPILOG;
}

static void emit_builtin_write_stdout_w(CodeGen *codegen) {
    PROLOG("__write_stdout_w", 80);

    SAVE_PARAM_RCX(8);
    SAVE_PARAM_RDX(16);

    TAB; T("mov rcx, 0xFFFFFFFFFFFFFFF5"); NL;
    TAB; T("call [rel __imp_GetStdHandle]"); NL;
    TAB; T("mov [rbp-24], rax"); NL;

    TAB; T("mov rcx, rax"); NL;
    TAB; T("call [rel __imp_GetFileType]"); NL;

    TAB; T("cmp eax, 0x0002"); NL;
    TAB; T("je __wsw_console"); NL;

    T("__wsw_file:\n");
    TAB; T("mov qword [rbp-32], 0"); NL;
    TAB; T("lea r9, [rbp-32]"); NL;
    TAB; T("mov rcx, [rbp-24]"); NL;
    TAB; T("mov rdx, [rbp-8]"); NL;
    TAB; T("mov r8d, dword [rbp-16]"); NL;
    TAB; T("mov qword [rsp+32], 0"); NL;

    TAB; T("call [rel __imp_WriteFile]"); NL;
    TAB; T("jmp __wsw_done"); NL;

    T("__wsw_console:\n");
    TAB; T("mov rax, [rbp-16]"); NL;
    TAB; T("shr rax, 1"); NL;
    TAB; T("mov r8, rax"); NL;
    TAB; T("mov qword [rbp-40], 0"); NL;
    TAB; T("lea r9, [rbp-40]"); NL;
    TAB; T("mov rcx, [rbp-24]"); NL;
    TAB; T("mov rdx, [rbp-8]"); NL;
    TAB; T("mov qword [rsp+32], 0"); NL;

    TAB; T("call [rel __imp_WriteConsoleW]"); NL;

    T("__wsw_done:\n");
    EPILOG;
}

static void emit_builtin_entry(CodeGen *codegen) {
    NL;
    T("__entry:\n");
    TAB; T("sub rsp, 8"); NL;
    TAB; T("push rbp"); NL;
    TAB; T("mov rbp, rsp"); NL;
    TAB; T("sub rsp, 48"); NL;

    TAB; T("sub rsp, 32"); NL;
    TAB; T("call __global_init"); NL;
    TAB; T("add rsp, 32"); NL;

    TAB; T("xor rcx, rcx"); NL;
    TAB; T("mov rdx, 2048"); NL;
    TAB; T("mov r8, 0x3000"); NL;
    TAB; T("mov r9, 0x04"); NL;
    TAB; T("sub rsp, 32"); NL;
    TAB; T("call [rel __imp_VirtualAlloc]"); NL;
    TAB; T("add rsp, 32"); NL;
    TAB; T("mov [rbp-8], rax"); NL;

    TAB; T("mov rcx, [rbp-8]"); NL;
    TAB; T("sub rsp, 32"); NL;
    TAB; T("call __parse_args"); NL;
    TAB; T("add rsp, 32"); NL;
    TAB; T("mov [rbp-16], rax"); NL;

    TAB; T("mov rcx, [rbp-16]"); NL;
    TAB; T("mov rdx, [rbp-8]"); NL;
    TAB; T("sub rsp, 32"); NL;
    TAB; T("call start"); NL;
    TAB; T("add rsp, 32"); NL;

    TAB; T("mov rcx, rax"); NL;
    TAB; T("sub rsp, 32"); NL;
    TAB; T("call [rel __imp_ExitProcess]"); NL;
    TAB; T("hlt"); NL;
}

static void emit_header(CodeGen *codegen) {
    T("bits 64\n");
    T("default rel\n\n");

    T("extern __imp_ExitProcess\n");
    T("extern __imp_WriteFile\n");
    T("extern __imp_ReadFile\n");
    T("extern __imp_GetStdHandle\n");
    T("extern __imp_CreateFileA\n");
    T("extern __imp_CreateFileW\n");
    T("extern __imp_CloseHandle\n");
    T("extern __imp_VirtualAlloc\n");
    T("extern __imp_VirtualFree\n");
    T("extern __imp_GetCommandLineA\n");
    T("extern __imp_GetCommandLineW\n");
    T("extern __imp_CommandLineToArgvW\n");
    T("extern __imp_WriteConsoleW\n");
    T("extern __imp_GetFileType\n");
    T("extern __imp_LocalFree\n");
    T("\n");

    T("global __entry\n\n");
}

static void emit_builtin_open_file(CodeGen *codegen) {
    PROLOG("__open_file", 32);

    SAVE_PARAM_RCX(8);
    SAVE_PARAM_RDX(16);

    TAB; T("mov rax, [rbp-16]"); NL;
    TAB; T("cmp rax, 0"); NL;
    TAB; T("je __of_mode_r"); NL;
    TAB; T("cmp rax, 1"); NL;
    TAB; T("je __of_mode_w"); NL;
    TAB; T("cmp rax, 2"); NL;
    TAB; T("je __of_mode_a"); NL;
    TAB; T("jmp __of_mode_create"); NL;

    T("__of_mode_r:\n");
    TAB; T("mov dword [rbp-24], 0x80000000"); NL;
    TAB; T("mov dword [rbp-28], 3"); NL;
    TAB; T("jmp __of_dispatch"); NL;

    T("__of_mode_w:\n");
    TAB; T("mov dword [rbp-24], 0x40000000"); NL;
    TAB; T("mov dword [rbp-28], 5"); NL;
    TAB; T("jmp __of_dispatch"); NL;

    T("__of_mode_a:\n");
    TAB; T("mov dword [rbp-24], 0x40000000"); NL;
    TAB; T("mov dword [rbp-28], 4"); NL;
    TAB; T("jmp __of_dispatch"); NL;

    T("__of_mode_create:\n");
    TAB; T("mov dword [rbp-24], 0x40000000"); NL;
    TAB; T("mov dword [rbp-28], 2"); NL;

    T("__of_dispatch:\n");
    TAB; T("sub rsp, 56"); NL;
    TAB; T("mov rcx, [rbp-8]"); NL;
    TAB; T("mov edx, dword [rbp-24]"); NL;
    TAB; T("xor r8, r8"); NL;
    TAB; T("xor r9, r9"); NL;
    TAB; T("mov eax, dword [rbp-28]"); NL;
    TAB; T("mov dword [rsp+32], eax"); NL;
    TAB; T("mov dword [rsp+40], 0x80"); NL;
    TAB; T("mov dword [rsp+48], 0"); NL;

    TAB; T("call [rel __imp_CreateFileA]"); NL;
    TAB; T("add rsp, 56"); NL;

    TAB; T("cmp rax, -1"); NL;
    TAB; T("jne __of_ok"); NL;
    TAB; T("xor rax, rax"); NL;

    T("__of_ok:\n");
    EPILOG;
}

static void emit_builtin_open_file_w(CodeGen *codegen) {
    PROLOG("__open_file_w", 32);

    SAVE_PARAM_RCX(8);
    SAVE_PARAM_RDX(16);

    TAB; T("mov rax, [rbp-16]"); NL;
    TAB; T("cmp rax, 0"); NL;
    TAB; T("je __ofw_mode_r"); NL;
    TAB; T("cmp rax, 1"); NL;
    TAB; T("je __ofw_mode_w"); NL;
    TAB; T("cmp rax, 2"); NL;
    TAB; T("je __ofw_mode_a"); NL;
    TAB; T("jmp __ofw_mode_create"); NL;

    T("__ofw_mode_r:\n");
    TAB; T("mov dword [rbp-24], 0x80000000"); NL;
    TAB; T("mov dword [rbp-28], 3"); NL;
    TAB; T("jmp __ofw_dispatch"); NL;

    T("__ofw_mode_w:\n");
    TAB; T("mov dword [rbp-24], 0x40000000"); NL;
    TAB; T("mov dword [rbp-28], 5"); NL;
    TAB; T("jmp __ofw_dispatch"); NL;

    T("__ofw_mode_a:\n");
    TAB; T("mov dword [rbp-24], 0x40000000"); NL;
    TAB; T("mov dword [rbp-28], 4"); NL;
    TAB; T("jmp __ofw_dispatch"); NL;

    T("__ofw_mode_create:\n");
    TAB; T("mov dword [rbp-24], 0x40000000"); NL;
    TAB; T("mov dword [rbp-28], 2"); NL;

    T("__ofw_dispatch:\n");
    TAB; T("sub rsp, 56"); NL;
    TAB; T("mov rcx, [rbp-8]"); NL;
    TAB; T("mov edx, dword [rbp-24]"); NL;
    TAB; T("xor r8, r8"); NL;
    TAB; T("xor r9, r9"); NL;
    TAB; T("mov eax, dword [rbp-28]"); NL;
    TAB; T("mov dword [rsp+32], eax"); NL;
    TAB; T("mov dword [rsp+40], 0x80"); NL;
    TAB; T("mov dword [rsp+48], 0"); NL;

    TAB; T("call [rel __imp_CreateFileW]"); NL;
    TAB; T("add rsp, 56"); NL;

    TAB; T("cmp rax, -1"); NL;
    TAB; T("jne __ofw_ok"); NL;
    TAB; T("xor rax, rax"); NL;

    T("__ofw_ok:\n");
    EPILOG;
}

static void emit_builtin_read_file(CodeGen *codegen) {
    PROLOG("__read_file", 32);

    SAVE_PARAM_RCX(8);
    SAVE_PARAM_RDX(16);
    SAVE_PARAM_R8(24);

    TAB; T("mov qword [rbp-32], 0"); NL;
    TAB; T("lea r9, [rbp-32]"); NL;

    TAB; T("mov rcx, [rbp-8]"); NL;
    TAB; T("mov rdx, [rbp-16]"); NL;
    TAB; T("mov r8d, dword [rbp-24]"); NL;
    TAB; T("mov qword [rsp+32], 0"); NL;

    TAB; T("call [rel __imp_ReadFile]"); NL;
    TAB; T("mov rax, [rbp-32]"); NL;

    EPILOG;
}

static void emit_builtin_global_init(CodeGen *codegen, Node *program) {
    PROLOG("__global_init", 16);

    NodeList* decls = program->ast.program.declarations;
    if(decls) {
        for(size_t i = 0; i < decls->count; ++i) {
            Node* item = decls->items[i];
            if(!item || item->type != NODE_VAR_DECL) continue;
            if(!item->ast.var_decl.init) continue;

            int var_size = (item->ast.var_decl.pointer_lvl == 0) ? bytes_data_value(item->ast.var_decl.data_type) : 8;

            codegen->in_func = true;
            codegen_expr(codegen, item->ast.var_decl.init);
            codegen->in_func = false;

            switch (var_size)
            {
            case 1: TAB; T("mov byte [rel __g_"); break;
            case 2: TAB; T("mov word [rel __g_"); break;
            case 4: TAB; T("mov dword [rel __g_"); break;
            default: TAB; T("mov qword [rel __g_"); break;
            }

            codegen_buffer_write(&codegen->sec_text, item->ast.var_decl.name.start, item->ast.var_decl.name.len);

            switch (var_size)
            {
            case 1: T("], al"); break;
            case 2: T("], ax"); break;
            case 4: T("], eax"); break;
            default: T("], rax"); break;
            }
            NL;
        }
    }

    EPILOG;
}

static void emit_builtin_write_file(CodeGen *codegen) {
    PROLOG("__write_file", 32);

    SAVE_PARAM_RCX(8);
    SAVE_PARAM_RDX(16);
    SAVE_PARAM_R8(24);

    TAB; T("mov qword [rbp-32], 0"); NL;
    TAB; T("lea r9, [rbp-32]"); NL;

    TAB; T("mov rcx, [rbp-8]"); NL;
    TAB; T("mov rdx, [rbp-16]"); NL;
    TAB; T("mov r8d, dword [rbp-24]"); NL;
    TAB; T("mov qword [rsp+32], 0"); NL;

    TAB; T("call [rel __imp_WriteFile]"); NL;
    TAB; T("mov rax, [rbp-32]"); NL;

    EPILOG;
}

static void emit_builtin_close_file(CodeGen *codegen) {
    PROLOG("__close_file", 16);

    SAVE_PARAM_RCX(8);

    TAB; T("call [rel __imp_CloseHandle]"); NL;

    EPILOG;
}

static void emit_builtin_va_next(CodeGen *codegen) {
    PROLOG("__va_next", 16);

    SAVE_PARAM_RCX(8);

    TAB; T("mov rax, [rbp-8]"); NL;
    TAB; T("mov rcx, [rax]");   NL;
    TAB; T("mov rdx, [rcx]");   NL;

    TAB; T("add rcx, 8");       NL;
    TAB; T("mov [rax], rcx");   NL;

    TAB; T("mov rax, rdx");     NL;

    EPILOG;
}

static void emit_builtin_va_start(CodeGen *codegen) {
    PROLOG("__va_start", 16);

    SAVE_PARAM_RCX(8);
    TAB; T("mov rax, [rbp-8]"); NL;

    EPILOG;
}

void codegen_init(CodeGen *codegen, Arena *arena, SymbolTable *table) {
    codegen->arena         = arena;
    codegen->table         = table;
    codegen->entry_name    = "start";
    codegen->strings       = NULL;
    codegen->string_count  = 0;
    codegen->locals        = NULL;
    codegen->stack_top     = 0;
    codegen->label_counter = 0;
    codegen->loop_depth    = 0;
    codegen->in_func       = false;
    codegen->entry_emmited = false;

    codegen_buffer_init(&codegen->sec_text, arena, 16384);
    codegen_buffer_init(&codegen->sec_data, arena, 4096);
    codegen_buffer_init(&codegen->sec_bss,  arena, 1024);
    codegen_buffer_init(&codegen->sec_init, arena, 1024);
}

void codegen_program(CodeGen *codegen, Node *program) {
    if(!program) return;

    emit_header(codegen);
    T("section .text\n\n");

    emit_builtin_write_stdout(codegen);
    emit_builtin_write_file(codegen);
    emit_builtin_read_file(codegen);
    emit_builtin_close_file(codegen);
    emit_builtin_write_int(codegen);
    emit_builtin_write_stdout_w(codegen);
    emit_builtin_malloc(codegen);
    emit_builtin_free(codegen);
    emit_builtin_va_start(codegen);
    emit_builtin_va_next(codegen);
    emit_builtin_parse_args(codegen);
    emit_builtin_parse_args_w(codegen);
    emit_builtin_open_file(codegen);
    emit_builtin_open_file_w(codegen);
    emit_builtin_exit(codegen);
    emit_builtin_int_to_str(codegen);

    NodeList* decls = program->ast.program.declarations;
    if(!decls) return;

    for(size_t i = 0; i < decls->count; ++i) {
        Node* item = decls->items[i];
        if(!item) continue;
        switch(item->type) {
        case NODE_FUNC_DECL:
            if(item->ast.func_decl.body) {
                codegen->in_func = true;
                codegen_function(codegen, item);
                codegen->in_func = false;
            }
            break;
        case NODE_VAR_DECL:
            codegen_global_var(codegen, item);
            break;
        case NODE_STRUCT:
        case NODE_UNION:
        case NODE_ENUM:
        case NODE_TYPEDEF_DECL:
            break;
        default:
            T("; TODO: top-level "); TI((int)item->type); NL;
            break;
        }
    }

    emit_builtin_global_init(codegen, program);
    emit_builtin_entry(codegen);
    emit_bss_builtins(codegen);
}

CodeGenBuffer codegen_finalize(CodeGen *codegen) {
    emit_strings(codegen);

    if(codegen->sec_data.len > 0) {
        codegen_buffer_writez(&codegen->sec_text, "\nsection .data\n");
        codegen_buffer_write(&codegen->sec_text,
            codegen->sec_data.data, codegen->sec_data.len);
    }

    if(codegen->sec_bss.len > 0) {
        codegen_buffer_writez(&codegen->sec_text, "\nsection .bss\n");
        codegen_buffer_write(&codegen->sec_text,
            codegen->sec_bss.data, codegen->sec_bss.len);
    }

    return codegen->sec_text;
}
