/*
 * Handmade Script Parser - Lexer and recursive descent parser
 * 
 * Hand-written, no yacc/bison
 * Clear error messages with line tracking
 * Optimized for cache-friendly parsing
 */

#include "handmade_script.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

// Token types
typedef enum {
    // Literals
    TOKEN_NIL,
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_NUMBER,
    TOKEN_STRING,
    TOKEN_IDENTIFIER,
    
    // Keywords
    TOKEN_LET,
    TOKEN_FN,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_WHILE,
    TOKEN_FOR,
    TOKEN_BREAK,
    TOKEN_CONTINUE,
    TOKEN_RETURN,
    TOKEN_YIELD,
    
    // Operators
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_PERCENT,
    TOKEN_CARET,     // Power
    
    TOKEN_EQ_EQ,     // ==
    TOKEN_BANG_EQ,   // !=
    TOKEN_LT,
    TOKEN_LT_EQ,
    TOKEN_GT,
    TOKEN_GT_EQ,
    
    TOKEN_AMP_AMP,   // &&
    TOKEN_PIPE_PIPE, // ||
    TOKEN_BANG,      // !
    
    TOKEN_EQ,        // =
    TOKEN_PLUS_EQ,   // +=
    TOKEN_MINUS_EQ,  // -=
    TOKEN_STAR_EQ,   // *=
    TOKEN_SLASH_EQ,  // /=
    
    // Punctuation
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_LBRACKET,
    TOKEN_RBRACKET,
    TOKEN_COMMA,
    TOKEN_DOT,
    TOKEN_COLON,
    TOKEN_SEMICOLON,
    
    // Special
    TOKEN_EOF,
    TOKEN_ERROR
} Token_Type;

// Token structure
typedef struct {
    Token_Type type;
    const char* start;
    uint32_t length;
    uint32_t line;
    uint32_t column;
    union {
        double number;
        struct {
            const char* data;
            uint32_t length;
        } string;
    } value;
} Token;

// Lexer state
typedef struct {
    const char* source;
    const char* current;
    const char* line_start;
    uint32_t line;
    uint32_t column;
    Token current_token;
    Token peek_token;
    bool has_peek;
    char error_message[256];
} Lexer;

// Parser state
typedef struct Parser {
    Lexer lexer;
    Script_VM* vm;
    
    // Error handling
    bool had_error;
    bool panic_mode;
    char error_message[256];
    
    // Current compilation unit
    struct Compiler* compiler;
    
    // Memory pools
    void* ast_pool;
    size_t ast_pool_size;
    size_t ast_pool_used;
} Parser;

// AST node types
typedef enum {
    AST_LITERAL,
    AST_IDENTIFIER,
    AST_BINARY_OP,
    AST_UNARY_OP,
    AST_ASSIGNMENT,
    AST_CALL,
    AST_INDEX,
    AST_FIELD,
    AST_TABLE,
    AST_FUNCTION,
    AST_IF,
    AST_WHILE,
    AST_FOR,
    AST_BREAK,
    AST_CONTINUE,
    AST_RETURN,
    AST_YIELD,
    AST_BLOCK,
    AST_VAR_DECL,
    AST_EXPRESSION_STMT
} AST_Node_Type;

// Forward declare AST node
typedef struct AST_Node AST_Node;

// AST node structure
struct AST_Node {
    AST_Node_Type type;
    uint32_t line;
    uint32_t column;
    
    union {
        // Literal
        struct {
            Script_Value value;
        } literal;
        
        // Identifier
        struct {
            const char* name;
            uint32_t length;
        } identifier;
        
        // Binary operation
        struct {
            Token_Type op;
            AST_Node* left;
            AST_Node* right;
        } binary;
        
        // Unary operation
        struct {
            Token_Type op;
            AST_Node* operand;
        } unary;
        
        // Assignment
        struct {
            AST_Node* target;
            AST_Node* value;
        } assignment;
        
        // Function call
        struct {
            AST_Node* function;
            AST_Node** arguments;
            uint32_t arg_count;
        } call;
        
        // Index access
        struct {
            AST_Node* object;
            AST_Node* index;
        } index;
        
        // Field access
        struct {
            AST_Node* object;
            const char* field;
            uint32_t field_length;
        } field;
        
        // Table literal
        struct {
            AST_Node** keys;
            AST_Node** values;
            uint32_t entry_count;
        } table;
        
        // Function definition
        struct {
            const char* name;
            uint32_t name_length;
            const char** params;
            uint32_t* param_lengths;
            uint32_t param_count;
            AST_Node* body;
        } function;
        
        // If statement
        struct {
            AST_Node* condition;
            AST_Node* then_branch;
            AST_Node* else_branch;
        } if_stmt;
        
        // While loop
        struct {
            AST_Node* condition;
            AST_Node* body;
        } while_loop;
        
        // For loop
        struct {
            AST_Node* init;
            AST_Node* condition;
            AST_Node* increment;
            AST_Node* body;
        } for_loop;
        
        // Return statement
        struct {
            AST_Node* value;
        } return_stmt;
        
        // Yield expression
        struct {
            AST_Node* value;
        } yield_expr;
        
        // Block statement
        struct {
            AST_Node** statements;
            uint32_t statement_count;
        } block;
        
        // Variable declaration
        struct {
            const char* name;
            uint32_t name_length;
            AST_Node* initializer;
        } var_decl;
        
        // Expression statement
        struct {
            AST_Node* expression;
        } expr_stmt;
    } as;
};

// Keyword table for fast lookup
static const struct {
    const char* text;
    Token_Type type;
} keywords[] = {
    {"nil", TOKEN_NIL},
    {"true", TOKEN_TRUE},
    {"false", TOKEN_FALSE},
    {"let", TOKEN_LET},
    {"fn", TOKEN_FN},
    {"if", TOKEN_IF},
    {"else", TOKEN_ELSE},
    {"while", TOKEN_WHILE},
    {"for", TOKEN_FOR},
    {"break", TOKEN_BREAK},
    {"continue", TOKEN_CONTINUE},
    {"return", TOKEN_RETURN},
    {"yield", TOKEN_YIELD},
    {NULL, TOKEN_EOF}
};

// Lexer functions

static void lexer_init(Lexer* lexer, const char* source) {
    lexer->source = source;
    lexer->current = source;
    lexer->line_start = source;
    lexer->line = 1;
    lexer->column = 1;
    lexer->has_peek = false;
}

static bool is_at_end(Lexer* lexer) {
    return *lexer->current == '\0';
}

static char advance(Lexer* lexer) {
    char c = *lexer->current++;
    lexer->column++;
    if (c == '\n') {
        lexer->line++;
        lexer->column = 1;
        lexer->line_start = lexer->current;
    }
    return c;
}

static char peek(Lexer* lexer) {
    return *lexer->current;
}

static char peek_next(Lexer* lexer) {
    if (is_at_end(lexer)) return '\0';
    return lexer->current[1];
}

static bool match_char(Lexer* lexer, char expected) {
    if (is_at_end(lexer)) return false;
    if (*lexer->current != expected) return false;
    advance(lexer);
    return true;
}

static void skip_whitespace(Lexer* lexer) {
    while (1) {
        char c = peek(lexer);
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
            case '\n':
                advance(lexer);
                break;
            case '/':
                if (peek_next(lexer) == '/') {
                    // Comment until end of line
                    while (peek(lexer) != '\n' && !is_at_end(lexer)) {
                        advance(lexer);
                    }
                } else if (peek_next(lexer) == '*') {
                    // Block comment
                    advance(lexer); // /
                    advance(lexer); // *
                    while (!is_at_end(lexer)) {
                        if (peek(lexer) == '*' && peek_next(lexer) == '/') {
                            advance(lexer); // *
                            advance(lexer); // /
                            break;
                        }
                        advance(lexer);
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

static Token make_token(Lexer* lexer, Token_Type type) {
    Token token;
    token.type = type;
    token.start = lexer->source;
    token.length = (uint32_t)(lexer->current - lexer->source);
    token.line = lexer->line;
    token.column = lexer->column - token.length;
    return token;
}

static Token error_token(Lexer* lexer, const char* message) {
    Token token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = (uint32_t)strlen(message);
    token.line = lexer->line;
    token.column = lexer->column;
    return token;
}

static Token scan_string(Lexer* lexer) {
    const char* start = lexer->current;
    uint32_t start_line = lexer->line;
    uint32_t start_column = lexer->column - 1;
    
    while (peek(lexer) != '"' && !is_at_end(lexer)) {
        if (peek(lexer) == '\\') {
            advance(lexer);
            if (!is_at_end(lexer)) advance(lexer);
        } else {
            advance(lexer);
        }
    }
    
    if (is_at_end(lexer)) {
        return error_token(lexer, "Unterminated string");
    }
    
    Token token;
    token.type = TOKEN_STRING;
    token.start = start;
    token.length = (uint32_t)(lexer->current - start);
    token.line = start_line;
    token.column = start_column;
    token.value.string.data = start;
    token.value.string.length = token.length;
    
    advance(lexer); // Closing "
    return token;
}

static Token scan_number(Lexer* lexer) {
    const char* start = lexer->current - 1;
    uint32_t start_column = lexer->column - 1;
    
    while (isdigit(peek(lexer))) {
        advance(lexer);
    }
    
    // Look for decimal part
    if (peek(lexer) == '.' && isdigit(peek_next(lexer))) {
        advance(lexer); // Consume '.'
        while (isdigit(peek(lexer))) {
            advance(lexer);
        }
    }
    
    // Look for exponent
    if (peek(lexer) == 'e' || peek(lexer) == 'E') {
        advance(lexer);
        if (peek(lexer) == '+' || peek(lexer) == '-') {
            advance(lexer);
        }
        while (isdigit(peek(lexer))) {
            advance(lexer);
        }
    }
    
    Token token;
    token.type = TOKEN_NUMBER;
    token.start = start;
    token.length = (uint32_t)(lexer->current - start);
    token.line = lexer->line;
    token.column = start_column;
    
    char* end;
    token.value.number = strtod(start, &end);
    
    return token;
}

static Token scan_identifier(Lexer* lexer) {
    const char* start = lexer->current - 1;
    uint32_t start_column = lexer->column - 1;
    
    while (isalnum(peek(lexer)) || peek(lexer) == '_') {
        advance(lexer);
    }
    
    // Check if it's a keyword
    uint32_t length = (uint32_t)(lexer->current - start);
    for (int i = 0; keywords[i].text != NULL; i++) {
        if (strlen(keywords[i].text) == length &&
            memcmp(start, keywords[i].text, length) == 0) {
            Token token;
            token.type = keywords[i].type;
            token.start = start;
            token.length = length;
            token.line = lexer->line;
            token.column = start_column;
            return token;
        }
    }
    
    Token token;
    token.type = TOKEN_IDENTIFIER;
    token.start = start;
    token.length = length;
    token.line = lexer->line;
    token.column = start_column;
    return token;
}

static Token scan_token(Lexer* lexer) {
    skip_whitespace(lexer);
    
    if (is_at_end(lexer)) {
        return make_token(lexer, TOKEN_EOF);
    }
    
    const char* start = lexer->current;
    char c = advance(lexer);
    
    // Numbers
    if (isdigit(c)) {
        return scan_number(lexer);
    }
    
    // Identifiers and keywords
    if (isalpha(c) || c == '_') {
        return scan_identifier(lexer);
    }
    
    // Single and double character tokens
    switch (c) {
        case '(': return make_token(lexer, TOKEN_LPAREN);
        case ')': return make_token(lexer, TOKEN_RPAREN);
        case '{': return make_token(lexer, TOKEN_LBRACE);
        case '}': return make_token(lexer, TOKEN_RBRACE);
        case '[': return make_token(lexer, TOKEN_LBRACKET);
        case ']': return make_token(lexer, TOKEN_RBRACKET);
        case ',': return make_token(lexer, TOKEN_COMMA);
        case '.': return make_token(lexer, TOKEN_DOT);
        case ':': return make_token(lexer, TOKEN_COLON);
        case ';': return make_token(lexer, TOKEN_SEMICOLON);
        case '^': return make_token(lexer, TOKEN_CARET);
        case '%': return make_token(lexer, TOKEN_PERCENT);
        
        case '+':
            return make_token(lexer, match_char(lexer, '=') ? TOKEN_PLUS_EQ : TOKEN_PLUS);
        case '-':
            return make_token(lexer, match_char(lexer, '=') ? TOKEN_MINUS_EQ : TOKEN_MINUS);
        case '*':
            return make_token(lexer, match_char(lexer, '=') ? TOKEN_STAR_EQ : TOKEN_STAR);
        case '/':
            return make_token(lexer, match_char(lexer, '=') ? TOKEN_SLASH_EQ : TOKEN_SLASH);
        
        case '!':
            return make_token(lexer, match_char(lexer, '=') ? TOKEN_BANG_EQ : TOKEN_BANG);
        case '=':
            return make_token(lexer, match_char(lexer, '=') ? TOKEN_EQ_EQ : TOKEN_EQ);
        case '<':
            return make_token(lexer, match_char(lexer, '=') ? TOKEN_LT_EQ : TOKEN_LT);
        case '>':
            return make_token(lexer, match_char(lexer, '=') ? TOKEN_GT_EQ : TOKEN_GT);
        
        case '&':
            if (match_char(lexer, '&')) return make_token(lexer, TOKEN_AMP_AMP);
            break;
        case '|':
            if (match_char(lexer, '|')) return make_token(lexer, TOKEN_PIPE_PIPE);
            break;
        
        case '"':
            return scan_string(lexer);
    }
    
    return error_token(lexer, "Unexpected character");
}

// Parser functions

static void parser_init(Parser* parser, Script_VM* vm, const char* source) {
    lexer_init(&parser->lexer, source);
    parser->vm = vm;
    parser->had_error = false;
    parser->panic_mode = false;
    parser->compiler = NULL;
    
    // Initialize AST memory pool
    parser->ast_pool_size = 1024 * 1024; // 1MB
    parser->ast_pool = malloc(parser->ast_pool_size);
    parser->ast_pool_used = 0;
}

static void parser_free(Parser* parser) {
    if (parser->ast_pool) {
        free(parser->ast_pool);
        parser->ast_pool = NULL;
    }
}

static void* ast_alloc(Parser* parser, size_t size) {
    // Align to 8 bytes
    size = (size + 7) & ~7;
    
    if (parser->ast_pool_used + size > parser->ast_pool_size) {
        // Expand pool
        size_t new_size = parser->ast_pool_size * 2;
        void* new_pool = realloc(parser->ast_pool, new_size);
        if (!new_pool) {
            parser->had_error = true;
            strcpy(parser->error_message, "Out of memory");
            return NULL;
        }
        parser->ast_pool = new_pool;
        parser->ast_pool_size = new_size;
    }
    
    void* ptr = (char*)parser->ast_pool + parser->ast_pool_used;
    parser->ast_pool_used += size;
    memset(ptr, 0, size);
    return ptr;
}

static Token advance_token(Parser* parser) {
    if (parser->lexer.has_peek) {
        parser->lexer.current_token = parser->lexer.peek_token;
        parser->lexer.has_peek = false;
    } else {
        parser->lexer.current_token = scan_token(&parser->lexer);
    }
    
    if (parser->lexer.current_token.type == TOKEN_ERROR) {
        parser->had_error = true;
        snprintf(parser->error_message, sizeof(parser->error_message),
                "Line %d: %s", parser->lexer.current_token.line,
                parser->lexer.current_token.start);
    }
    
    return parser->lexer.current_token;
}

static Token peek_token(Parser* parser) {
    if (!parser->lexer.has_peek) {
        parser->lexer.peek_token = scan_token(&parser->lexer);
        parser->lexer.has_peek = true;
    }
    return parser->lexer.peek_token;
}

static Token current_token(Parser* parser) {
    return parser->lexer.current_token;
}

static bool check(Parser* parser, Token_Type type) {
    return peek_token(parser).type == type;
}

static bool match(Parser* parser, Token_Type type) {
    if (!check(parser, type)) return false;
    advance_token(parser);
    return true;
}

static void expect(Parser* parser, Token_Type type, const char* message) {
    if (!match(parser, type)) {
        parser->had_error = true;
        Token token = peek_token(parser);
        snprintf(parser->error_message, sizeof(parser->error_message),
                "Line %d, Column %d: Expected %s", 
                token.line, token.column, message);
    }
}

// Forward declarations for recursive parsing
static AST_Node* parse_expression(Parser* parser);
static AST_Node* parse_statement(Parser* parser);
static AST_Node* parse_block(Parser* parser);

// Parse primary expressions
static AST_Node* parse_primary(Parser* parser) {
    Token token = advance_token(parser);
    
    AST_Node* node = ast_alloc(parser, sizeof(AST_Node));
    if (!node) return NULL;
    
    node->line = token.line;
    node->column = token.column;
    
    switch (token.type) {
        case TOKEN_NIL:
            node->type = AST_LITERAL;
            node->as.literal.value = script_nil();
            return node;
            
        case TOKEN_TRUE:
            node->type = AST_LITERAL;
            node->as.literal.value = script_bool(true);
            return node;
            
        case TOKEN_FALSE:
            node->type = AST_LITERAL;
            node->as.literal.value = script_bool(false);
            return node;
            
        case TOKEN_NUMBER:
            node->type = AST_LITERAL;
            node->as.literal.value = script_number(token.value.number);
            return node;
            
        case TOKEN_STRING: {
            node->type = AST_LITERAL;
            // Process escape sequences
            char* processed = ast_alloc(parser, token.value.string.length + 1);
            if (!processed) return NULL;
            
            const char* src = token.value.string.data;
            char* dst = processed;
            for (uint32_t i = 0; i < token.value.string.length; i++) {
                if (src[i] == '\\' && i + 1 < token.value.string.length) {
                    i++;
                    switch (src[i]) {
                        case 'n': *dst++ = '\n'; break;
                        case 't': *dst++ = '\t'; break;
                        case 'r': *dst++ = '\r'; break;
                        case '\\': *dst++ = '\\'; break;
                        case '"': *dst++ = '"'; break;
                        default: *dst++ = src[i]; break;
                    }
                } else {
                    *dst++ = src[i];
                }
            }
            *dst = '\0';
            
            node->as.literal.value = script_string(parser->vm, processed, dst - processed);
            return node;
        }
            
        case TOKEN_IDENTIFIER:
            node->type = AST_IDENTIFIER;
            node->as.identifier.name = token.start;
            node->as.identifier.length = token.length;
            return node;
            
        case TOKEN_LPAREN: {
            AST_Node* expr = parse_expression(parser);
            expect(parser, TOKEN_RPAREN, ")");
            return expr;
        }
            
        case TOKEN_LBRACE: {
            // Table literal
            node->type = AST_TABLE;
            
            // Count entries first
            uint32_t capacity = 8;
            node->as.table.keys = ast_alloc(parser, capacity * sizeof(AST_Node*));
            node->as.table.values = ast_alloc(parser, capacity * sizeof(AST_Node*));
            node->as.table.entry_count = 0;
            
            while (!check(parser, TOKEN_RBRACE) && !check(parser, TOKEN_EOF)) {
                if (node->as.table.entry_count >= capacity) {
                    // Expand arrays
                    capacity *= 2;
                    AST_Node** new_keys = ast_alloc(parser, capacity * sizeof(AST_Node*));
                    AST_Node** new_values = ast_alloc(parser, capacity * sizeof(AST_Node*));
                    memcpy(new_keys, node->as.table.keys, 
                           node->as.table.entry_count * sizeof(AST_Node*));
                    memcpy(new_values, node->as.table.values,
                           node->as.table.entry_count * sizeof(AST_Node*));
                    node->as.table.keys = new_keys;
                    node->as.table.values = new_values;
                }
                
                // Parse key
                if (check(parser, TOKEN_IDENTIFIER)) {
                    Token id = advance_token(parser);
                    AST_Node* key = ast_alloc(parser, sizeof(AST_Node));
                    key->type = AST_LITERAL;
                    key->as.literal.value = script_string(parser->vm, id.start, id.length);
                    node->as.table.keys[node->as.table.entry_count] = key;
                } else if (check(parser, TOKEN_LBRACKET)) {
                    advance_token(parser);
                    node->as.table.keys[node->as.table.entry_count] = parse_expression(parser);
                    expect(parser, TOKEN_RBRACKET, "]");
                } else {
                    parser->had_error = true;
                    strcpy(parser->error_message, "Expected table key");
                    return NULL;
                }
                
                expect(parser, TOKEN_COLON, ":");
                node->as.table.values[node->as.table.entry_count] = parse_expression(parser);
                node->as.table.entry_count++;
                
                if (!match(parser, TOKEN_COMMA)) break;
            }
            
            expect(parser, TOKEN_RBRACE, "}");
            return node;
        }
            
        case TOKEN_FN: {
            // Function literal
            node->type = AST_FUNCTION;
            
            // Optional function name
            if (check(parser, TOKEN_IDENTIFIER)) {
                Token name = advance_token(parser);
                node->as.function.name = name.start;
                node->as.function.name_length = name.length;
            } else {
                node->as.function.name = NULL;
                node->as.function.name_length = 0;
            }
            
            expect(parser, TOKEN_LPAREN, "(");
            
            // Parse parameters
            uint32_t param_capacity = 8;
            node->as.function.params = ast_alloc(parser, param_capacity * sizeof(const char*));
            node->as.function.param_lengths = ast_alloc(parser, param_capacity * sizeof(uint32_t));
            node->as.function.param_count = 0;
            
            while (!check(parser, TOKEN_RPAREN) && !check(parser, TOKEN_EOF)) {
                if (node->as.function.param_count >= param_capacity) {
                    param_capacity *= 2;
                    const char** new_params = ast_alloc(parser, param_capacity * sizeof(const char*));
                    uint32_t* new_lengths = ast_alloc(parser, param_capacity * sizeof(uint32_t));
                    memcpy(new_params, node->as.function.params,
                           node->as.function.param_count * sizeof(const char*));
                    memcpy(new_lengths, node->as.function.param_lengths,
                           node->as.function.param_count * sizeof(uint32_t));
                    node->as.function.params = new_params;
                    node->as.function.param_lengths = new_lengths;
                }
                
                expect(parser, TOKEN_IDENTIFIER, "parameter name");
                Token param = current_token(parser);
                node->as.function.params[node->as.function.param_count] = param.start;
                node->as.function.param_lengths[node->as.function.param_count] = param.length;
                node->as.function.param_count++;
                
                if (!match(parser, TOKEN_COMMA)) break;
            }
            
            expect(parser, TOKEN_RPAREN, ")");
            
            // Parse body
            node->as.function.body = parse_block(parser);
            
            return node;
        }
            
        default:
            parser->had_error = true;
            snprintf(parser->error_message, sizeof(parser->error_message),
                    "Unexpected token at line %d", token.line);
            return NULL;
    }
}

// Parse postfix expressions (calls, index, field access)
static AST_Node* parse_postfix(Parser* parser) {
    AST_Node* node = parse_primary(parser);
    
    while (true) {
        if (match(parser, TOKEN_LPAREN)) {
            // Function call
            AST_Node* call = ast_alloc(parser, sizeof(AST_Node));
            call->type = AST_CALL;
            call->line = current_token(parser).line;
            call->column = current_token(parser).column;
            call->as.call.function = node;
            
            // Parse arguments
            uint32_t arg_capacity = 8;
            call->as.call.arguments = ast_alloc(parser, arg_capacity * sizeof(AST_Node*));
            call->as.call.arg_count = 0;
            
            while (!check(parser, TOKEN_RPAREN) && !check(parser, TOKEN_EOF)) {
                if (call->as.call.arg_count >= arg_capacity) {
                    arg_capacity *= 2;
                    AST_Node** new_args = ast_alloc(parser, arg_capacity * sizeof(AST_Node*));
                    memcpy(new_args, call->as.call.arguments,
                           call->as.call.arg_count * sizeof(AST_Node*));
                    call->as.call.arguments = new_args;
                }
                
                call->as.call.arguments[call->as.call.arg_count++] = parse_expression(parser);
                
                if (!match(parser, TOKEN_COMMA)) break;
            }
            
            expect(parser, TOKEN_RPAREN, ")");
            node = call;
            
        } else if (match(parser, TOKEN_LBRACKET)) {
            // Index access
            AST_Node* index = ast_alloc(parser, sizeof(AST_Node));
            index->type = AST_INDEX;
            index->line = current_token(parser).line;
            index->column = current_token(parser).column;
            index->as.index.object = node;
            index->as.index.index = parse_expression(parser);
            expect(parser, TOKEN_RBRACKET, "]");
            node = index;
            
        } else if (match(parser, TOKEN_DOT)) {
            // Field access
            expect(parser, TOKEN_IDENTIFIER, "field name");
            Token field = current_token(parser);
            
            AST_Node* field_access = ast_alloc(parser, sizeof(AST_Node));
            field_access->type = AST_FIELD;
            field_access->line = field.line;
            field_access->column = field.column;
            field_access->as.field.object = node;
            field_access->as.field.field = field.start;
            field_access->as.field.field_length = field.length;
            node = field_access;
            
        } else {
            break;
        }
    }
    
    return node;
}

// Parse unary expressions
static AST_Node* parse_unary(Parser* parser) {
    if (match(parser, TOKEN_BANG) || match(parser, TOKEN_MINUS)) {
        Token op = current_token(parser);
        AST_Node* node = ast_alloc(parser, sizeof(AST_Node));
        node->type = AST_UNARY_OP;
        node->line = op.line;
        node->column = op.column;
        node->as.unary.op = op.type;
        node->as.unary.operand = parse_unary(parser);
        return node;
    }
    
    return parse_postfix(parser);
}

// Parse power expressions (right associative)
static AST_Node* parse_power(Parser* parser) {
    AST_Node* node = parse_unary(parser);
    
    if (match(parser, TOKEN_CARET)) {
        Token op = current_token(parser);
        AST_Node* binary = ast_alloc(parser, sizeof(AST_Node));
        binary->type = AST_BINARY_OP;
        binary->line = op.line;
        binary->column = op.column;
        binary->as.binary.op = op.type;
        binary->as.binary.left = node;
        binary->as.binary.right = parse_power(parser); // Right associative
        return binary;
    }
    
    return node;
}

// Parse multiplicative expressions
static AST_Node* parse_multiplicative(Parser* parser) {
    AST_Node* node = parse_power(parser);
    
    while (match(parser, TOKEN_STAR) || match(parser, TOKEN_SLASH) || match(parser, TOKEN_PERCENT)) {
        Token op = current_token(parser);
        AST_Node* binary = ast_alloc(parser, sizeof(AST_Node));
        binary->type = AST_BINARY_OP;
        binary->line = op.line;
        binary->column = op.column;
        binary->as.binary.op = op.type;
        binary->as.binary.left = node;
        binary->as.binary.right = parse_power(parser);
        node = binary;
    }
    
    return node;
}

// Parse additive expressions
static AST_Node* parse_additive(Parser* parser) {
    AST_Node* node = parse_multiplicative(parser);
    
    while (match(parser, TOKEN_PLUS) || match(parser, TOKEN_MINUS)) {
        Token op = current_token(parser);
        AST_Node* binary = ast_alloc(parser, sizeof(AST_Node));
        binary->type = AST_BINARY_OP;
        binary->line = op.line;
        binary->column = op.column;
        binary->as.binary.op = op.type;
        binary->as.binary.left = node;
        binary->as.binary.right = parse_multiplicative(parser);
        node = binary;
    }
    
    return node;
}

// Parse comparison expressions
static AST_Node* parse_comparison(Parser* parser) {
    AST_Node* node = parse_additive(parser);
    
    while (match(parser, TOKEN_LT) || match(parser, TOKEN_LT_EQ) ||
           match(parser, TOKEN_GT) || match(parser, TOKEN_GT_EQ)) {
        Token op = current_token(parser);
        AST_Node* binary = ast_alloc(parser, sizeof(AST_Node));
        binary->type = AST_BINARY_OP;
        binary->line = op.line;
        binary->column = op.column;
        binary->as.binary.op = op.type;
        binary->as.binary.left = node;
        binary->as.binary.right = parse_additive(parser);
        node = binary;
    }
    
    return node;
}

// Parse equality expressions
static AST_Node* parse_equality(Parser* parser) {
    AST_Node* node = parse_comparison(parser);
    
    while (match(parser, TOKEN_EQ_EQ) || match(parser, TOKEN_BANG_EQ)) {
        Token op = current_token(parser);
        AST_Node* binary = ast_alloc(parser, sizeof(AST_Node));
        binary->type = AST_BINARY_OP;
        binary->line = op.line;
        binary->column = op.column;
        binary->as.binary.op = op.type;
        binary->as.binary.left = node;
        binary->as.binary.right = parse_comparison(parser);
        node = binary;
    }
    
    return node;
}

// Parse logical AND expressions
static AST_Node* parse_logical_and(Parser* parser) {
    AST_Node* node = parse_equality(parser);
    
    while (match(parser, TOKEN_AMP_AMP)) {
        Token op = current_token(parser);
        AST_Node* binary = ast_alloc(parser, sizeof(AST_Node));
        binary->type = AST_BINARY_OP;
        binary->line = op.line;
        binary->column = op.column;
        binary->as.binary.op = op.type;
        binary->as.binary.left = node;
        binary->as.binary.right = parse_equality(parser);
        node = binary;
    }
    
    return node;
}

// Parse logical OR expressions
static AST_Node* parse_logical_or(Parser* parser) {
    AST_Node* node = parse_logical_and(parser);
    
    while (match(parser, TOKEN_PIPE_PIPE)) {
        Token op = current_token(parser);
        AST_Node* binary = ast_alloc(parser, sizeof(AST_Node));
        binary->type = AST_BINARY_OP;
        binary->line = op.line;
        binary->column = op.column;
        binary->as.binary.op = op.type;
        binary->as.binary.left = node;
        binary->as.binary.right = parse_logical_and(parser);
        node = binary;
    }
    
    return node;
}

// Parse assignment expressions
static AST_Node* parse_assignment(Parser* parser) {
    AST_Node* node = parse_logical_or(parser);
    
    if (match(parser, TOKEN_EQ) || match(parser, TOKEN_PLUS_EQ) ||
        match(parser, TOKEN_MINUS_EQ) || match(parser, TOKEN_STAR_EQ) ||
        match(parser, TOKEN_SLASH_EQ)) {
        
        Token op = current_token(parser);
        AST_Node* assignment = ast_alloc(parser, sizeof(AST_Node));
        assignment->type = AST_ASSIGNMENT;
        assignment->line = op.line;
        assignment->column = op.column;
        assignment->as.assignment.target = node;
        
        AST_Node* value = parse_assignment(parser); // Right associative
        
        // Handle compound assignments
        if (op.type != TOKEN_EQ) {
            AST_Node* binary = ast_alloc(parser, sizeof(AST_Node));
            binary->type = AST_BINARY_OP;
            binary->line = op.line;
            binary->column = op.column;
            binary->as.binary.left = node;
            binary->as.binary.right = value;
            
            switch (op.type) {
                case TOKEN_PLUS_EQ: binary->as.binary.op = TOKEN_PLUS; break;
                case TOKEN_MINUS_EQ: binary->as.binary.op = TOKEN_MINUS; break;
                case TOKEN_STAR_EQ: binary->as.binary.op = TOKEN_STAR; break;
                case TOKEN_SLASH_EQ: binary->as.binary.op = TOKEN_SLASH; break;
                default: break;
            }
            
            value = binary;
        }
        
        assignment->as.assignment.value = value;
        return assignment;
    }
    
    return node;
}

// Parse expressions
static AST_Node* parse_expression(Parser* parser) {
    return parse_assignment(parser);
}

// Parse block statement
static AST_Node* parse_block(Parser* parser) {
    expect(parser, TOKEN_LBRACE, "{");
    
    AST_Node* block = ast_alloc(parser, sizeof(AST_Node));
    block->type = AST_BLOCK;
    block->line = current_token(parser).line;
    block->column = current_token(parser).column;
    
    uint32_t stmt_capacity = 16;
    block->as.block.statements = ast_alloc(parser, stmt_capacity * sizeof(AST_Node*));
    block->as.block.statement_count = 0;
    
    while (!check(parser, TOKEN_RBRACE) && !check(parser, TOKEN_EOF)) {
        if (block->as.block.statement_count >= stmt_capacity) {
            stmt_capacity *= 2;
            AST_Node** new_stmts = ast_alloc(parser, stmt_capacity * sizeof(AST_Node*));
            memcpy(new_stmts, block->as.block.statements,
                   block->as.block.statement_count * sizeof(AST_Node*));
            block->as.block.statements = new_stmts;
        }
        
        block->as.block.statements[block->as.block.statement_count++] = parse_statement(parser);
    }
    
    expect(parser, TOKEN_RBRACE, "}");
    return block;
}

// Parse statements
static AST_Node* parse_statement(Parser* parser) {
    // Variable declaration
    if (match(parser, TOKEN_LET)) {
        Token let = current_token(parser);
        expect(parser, TOKEN_IDENTIFIER, "variable name");
        Token name = current_token(parser);
        
        AST_Node* var_decl = ast_alloc(parser, sizeof(AST_Node));
        var_decl->type = AST_VAR_DECL;
        var_decl->line = let.line;
        var_decl->column = let.column;
        var_decl->as.var_decl.name = name.start;
        var_decl->as.var_decl.name_length = name.length;
        
        if (match(parser, TOKEN_EQ)) {
            var_decl->as.var_decl.initializer = parse_expression(parser);
        } else {
            var_decl->as.var_decl.initializer = NULL;
        }
        
        match(parser, TOKEN_SEMICOLON); // Optional semicolon
        return var_decl;
    }
    
    // If statement
    if (match(parser, TOKEN_IF)) {
        Token if_token = current_token(parser);
        
        AST_Node* if_stmt = ast_alloc(parser, sizeof(AST_Node));
        if_stmt->type = AST_IF;
        if_stmt->line = if_token.line;
        if_stmt->column = if_token.column;
        
        expect(parser, TOKEN_LPAREN, "(");
        if_stmt->as.if_stmt.condition = parse_expression(parser);
        expect(parser, TOKEN_RPAREN, ")");
        
        if_stmt->as.if_stmt.then_branch = parse_statement(parser);
        
        if (match(parser, TOKEN_ELSE)) {
            if_stmt->as.if_stmt.else_branch = parse_statement(parser);
        } else {
            if_stmt->as.if_stmt.else_branch = NULL;
        }
        
        return if_stmt;
    }
    
    // While loop
    if (match(parser, TOKEN_WHILE)) {
        Token while_token = current_token(parser);
        
        AST_Node* while_loop = ast_alloc(parser, sizeof(AST_Node));
        while_loop->type = AST_WHILE;
        while_loop->line = while_token.line;
        while_loop->column = while_token.column;
        
        expect(parser, TOKEN_LPAREN, "(");
        while_loop->as.while_loop.condition = parse_expression(parser);
        expect(parser, TOKEN_RPAREN, ")");
        
        while_loop->as.while_loop.body = parse_statement(parser);
        
        return while_loop;
    }
    
    // For loop
    if (match(parser, TOKEN_FOR)) {
        Token for_token = current_token(parser);
        
        AST_Node* for_loop = ast_alloc(parser, sizeof(AST_Node));
        for_loop->type = AST_FOR;
        for_loop->line = for_token.line;
        for_loop->column = for_token.column;
        
        expect(parser, TOKEN_LPAREN, "(");
        
        // Init
        if (match(parser, TOKEN_SEMICOLON)) {
            for_loop->as.for_loop.init = NULL;
        } else {
            for_loop->as.for_loop.init = parse_statement(parser);
        }
        
        // Condition
        if (match(parser, TOKEN_SEMICOLON)) {
            for_loop->as.for_loop.condition = NULL;
        } else {
            for_loop->as.for_loop.condition = parse_expression(parser);
            expect(parser, TOKEN_SEMICOLON, ";");
        }
        
        // Increment
        if (check(parser, TOKEN_RPAREN)) {
            for_loop->as.for_loop.increment = NULL;
        } else {
            for_loop->as.for_loop.increment = parse_expression(parser);
        }
        
        expect(parser, TOKEN_RPAREN, ")");
        
        for_loop->as.for_loop.body = parse_statement(parser);
        
        return for_loop;
    }
    
    // Break statement
    if (match(parser, TOKEN_BREAK)) {
        Token break_token = current_token(parser);
        AST_Node* break_stmt = ast_alloc(parser, sizeof(AST_Node));
        break_stmt->type = AST_BREAK;
        break_stmt->line = break_token.line;
        break_stmt->column = break_token.column;
        match(parser, TOKEN_SEMICOLON);
        return break_stmt;
    }
    
    // Continue statement
    if (match(parser, TOKEN_CONTINUE)) {
        Token continue_token = current_token(parser);
        AST_Node* continue_stmt = ast_alloc(parser, sizeof(AST_Node));
        continue_stmt->type = AST_CONTINUE;
        continue_stmt->line = continue_token.line;
        continue_stmt->column = continue_token.column;
        match(parser, TOKEN_SEMICOLON);
        return continue_stmt;
    }
    
    // Return statement
    if (match(parser, TOKEN_RETURN)) {
        Token return_token = current_token(parser);
        AST_Node* return_stmt = ast_alloc(parser, sizeof(AST_Node));
        return_stmt->type = AST_RETURN;
        return_stmt->line = return_token.line;
        return_stmt->column = return_token.column;
        
        if (!check(parser, TOKEN_SEMICOLON) && !check(parser, TOKEN_RBRACE)) {
            return_stmt->as.return_stmt.value = parse_expression(parser);
        } else {
            return_stmt->as.return_stmt.value = NULL;
        }
        
        match(parser, TOKEN_SEMICOLON);
        return return_stmt;
    }
    
    // Yield expression
    if (match(parser, TOKEN_YIELD)) {
        Token yield_token = current_token(parser);
        AST_Node* yield_expr = ast_alloc(parser, sizeof(AST_Node));
        yield_expr->type = AST_YIELD;
        yield_expr->line = yield_token.line;
        yield_expr->column = yield_token.column;
        
        if (!check(parser, TOKEN_SEMICOLON) && !check(parser, TOKEN_RBRACE)) {
            yield_expr->as.yield_expr.value = parse_expression(parser);
        } else {
            yield_expr->as.yield_expr.value = NULL;
        }
        
        match(parser, TOKEN_SEMICOLON);
        return yield_expr;
    }
    
    // Block statement
    if (check(parser, TOKEN_LBRACE)) {
        return parse_block(parser);
    }
    
    // Expression statement
    AST_Node* expr_stmt = ast_alloc(parser, sizeof(AST_Node));
    expr_stmt->type = AST_EXPRESSION_STMT;
    expr_stmt->line = peek_token(parser).line;
    expr_stmt->column = peek_token(parser).column;
    expr_stmt->as.expr_stmt.expression = parse_expression(parser);
    match(parser, TOKEN_SEMICOLON); // Optional semicolon
    return expr_stmt;
}

// Parse top-level program
AST_Node* parse_program(Parser* parser) {
    // Initialize with first token
    advance_token(parser);
    
    AST_Node* program = ast_alloc(parser, sizeof(AST_Node));
    program->type = AST_BLOCK;
    program->line = 1;
    program->column = 1;
    
    uint32_t stmt_capacity = 32;
    program->as.block.statements = ast_alloc(parser, stmt_capacity * sizeof(AST_Node*));
    program->as.block.statement_count = 0;
    
    while (!check(parser, TOKEN_EOF)) {
        if (program->as.block.statement_count >= stmt_capacity) {
            stmt_capacity *= 2;
            AST_Node** new_stmts = ast_alloc(parser, stmt_capacity * sizeof(AST_Node*));
            memcpy(new_stmts, program->as.block.statements,
                   program->as.block.statement_count * sizeof(AST_Node*));
            program->as.block.statements = new_stmts;
        }
        
        program->as.block.statements[program->as.block.statement_count++] = parse_statement(parser);
        
        if (parser->had_error) break;
    }
    
    return program;
}

// Public API
Script_Compile_Result script_parse(Script_VM* vm, const char* source, const char* name) {
    Script_Compile_Result result = {0};
    
    Parser parser;
    parser_init(&parser, vm, source);
    
    AST_Node* ast = parse_program(&parser);
    
    if (parser.had_error) {
        result.error_message = strdup(parser.error_message);
        result.error_line = parser.lexer.line;
        result.error_column = parser.lexer.column;
        result.function = NULL;
    } else {
        // AST is ready for compilation
        // This will be passed to the compiler
        result.function = (Script_Function*)ast; // Temporary cast
        result.error_message = NULL;
    }
    
    parser_free(&parser);
    return result;
}