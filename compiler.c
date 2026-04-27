#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <math.h>

// ─────────────────────────────────────────────────────────────────────────────
// SECTION 1 ── LEXER
// ─────────────────────────────────────────────────────────────────────────────

typedef enum {
    TOK_NUMBER, TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_LPAREN, TOK_RPAREN, TOK_EOF
} TokenKind;

typedef struct {
    TokenKind kind;
    double value; // Only used if kind == TOK_NUMBER
    char op;      // Store the character for operators
    int pos;
} Token;

const char* kind_to_string(TokenKind k) {
    switch(k) {
        case TOK_NUMBER: return "NUMBER"; case TOK_PLUS: return "PLUS";
        case TOK_MINUS: return "MINUS";   case TOK_STAR: return "STAR";
        case TOK_SLASH: return "SLASH";   case TOK_LPAREN: return "LPAREN";
        case TOK_RPAREN: return "RPAREN"; case TOK_EOF: return "EOF";
        default: return "UNKNOWN";
    }
}

// Dynamically allocated token array
Token* lex(const char* source, int* out_count) {
    int capacity = 64;
    Token* tokens = malloc(capacity * sizeof(Token));
    int count = 0;
    int pos = 0;

    while (source[pos] != '\0') {
        if (count >= capacity - 1) {
            capacity *= 2;
            tokens = realloc(tokens, capacity * sizeof(Token));
        }

        char c = source[pos];
        if (isspace(c)) {
            pos++;
            continue;
        }

        Token t;
        t.pos = pos;
        t.value = 0;
        t.op = c;

        if (isdigit(c) || c == '.') {
            char* endptr;
            t.kind = TOK_NUMBER;
            t.value = strtod(&source[pos], &endptr);
            pos = endptr - source;
        } else {
            switch (c) {
                case '+': t.kind = TOK_PLUS; break;
                case '-': t.kind = TOK_MINUS; break;
                case '*': t.kind = TOK_STAR; break;
                case '/': t.kind = TOK_SLASH; break;
                case '(': t.kind = TOK_LPAREN; break;
                case ')': t.kind = TOK_RPAREN; break;
                default:
                    fprintf(stderr, "LexerError: Unexpected character '%c' at position %d\n", c, pos);
                    exit(1);
            }
            pos++;
        }
        tokens[count++] = t;
    }
    
    tokens[count].kind = TOK_EOF;
    tokens[count].pos = pos;
    tokens[count].op = '\0';
    count++;
    
    *out_count = count;
    return tokens;
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION 2 ── AST NODES
// ─────────────────────────────────────────────────────────────────────────────

typedef enum { AST_NUMBER, AST_UNARY, AST_BINARY } ASTKind;

typedef struct ASTNode {
    ASTKind kind;
    union {
        double value; // For AST_NUMBER
        struct {
            char op;
            struct ASTNode* operand;
        } unary;      // For AST_UNARY
        struct {
            char op;
            struct ASTNode* left;
            struct ASTNode* right;
        } binary;     // For AST_BINARY
    } data;
} ASTNode;

ASTNode* make_number_node(double val) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->kind = AST_NUMBER;
    node->data.value = val;
    return node;
}

ASTNode* make_unary_node(char op, ASTNode* operand) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->kind = AST_UNARY;
    node->data.unary.op = op;
    node->data.unary.operand = operand;
    return node;
}

ASTNode* make_binary_node(char op, ASTNode* left, ASTNode* right) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->kind = AST_BINARY;
    node->data.binary.op = op;
    node->data.binary.left = left;
    node->data.binary.right = right;
    return node;
}

void free_ast(ASTNode* node) {
    if (!node) return;
    if (node->kind == AST_UNARY) free_ast(node->data.unary.operand);
    if (node->kind == AST_BINARY) {
        free_ast(node->data.binary.left);
        free_ast(node->data.binary.right);
    }
    free(node);
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION 3 ── PARSER
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
    Token* tokens;
    int pos;
} Parser;

ASTNode* parse_expr(Parser* p);
ASTNode* parse_term(Parser* p);
ASTNode* parse_unary(Parser* p);
ASTNode* parse_primary(Parser* p);

Token current(Parser* p) { return p->tokens[p->pos]; }
int match(Parser* p, TokenKind kind) { return current(p).kind == kind; }
Token eat(Parser* p, TokenKind kind) {
    if (current(p).kind != kind) {
        fprintf(stderr, "ParseError: Expected %s, got %s at pos %d\n", 
                kind_to_string(kind), kind_to_string(current(p).kind), current(p).pos);
        exit(1);
    }
    return p->tokens[p->pos++];
}

ASTNode* parse(Token* tokens) {
    Parser p = {tokens, 0};
    ASTNode* node = parse_expr(&p);
    eat(&p, TOK_EOF);
    return node;
}

ASTNode* parse_expr(Parser* p) {
    ASTNode* node = parse_term(p);
    while (match(p, TOK_PLUS) || match(p, TOK_MINUS)) {
        char op = current(p).op;
        p->pos++;
        node = make_binary_node(op, node, parse_term(p));
    }
    return node;
}

ASTNode* parse_term(Parser* p) {
    ASTNode* node = parse_unary(p);
    while (match(p, TOK_STAR) || match(p, TOK_SLASH)) {
        char op = current(p).op;
        p->pos++;
        node = make_binary_node(op, node, parse_unary(p));
    }
    return node;
}

ASTNode* parse_unary(Parser* p) {
    if (match(p, TOK_MINUS)) {
        p->pos++;
        return make_unary_node('-', parse_unary(p));
    }
    return parse_primary(p);
}

ASTNode* parse_primary(Parser* p) {
    Token tok = current(p);
    if (match(p, TOK_NUMBER)) {
        p->pos++;
        return make_number_node(tok.value);
    }
    if (match(p, TOK_LPAREN)) {
        p->pos++;
        ASTNode* node = parse_expr(p);
        eat(p, TOK_RPAREN);
        return node;
    }
    fprintf(stderr, "ParseError: Expected number or '(' at pos %d\n", tok.pos);
    exit(1);
    return NULL;
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION 4 ── CODE GENERATOR (stack-based IR)
// ─────────────────────────────────────────────────────────────────────────────

typedef enum { OP_PUSH, OP_NEG, OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_RET } Opcode;

typedef struct {
    Opcode op;
    double operand; // Used only for OP_PUSH
} Instruction;

typedef struct {
    Instruction* code;
    int count;
    int capacity;
} IRStream;

void emit(IRStream* stream, Opcode op, double operand) {
    if (stream->count >= stream->capacity) {
        stream->capacity = stream->capacity == 0 ? 64 : stream->capacity * 2;
        stream->code = realloc(stream->code, stream->capacity * sizeof(Instruction));
    }
    stream->code[stream->count].op = op;
    stream->code[stream->count].operand = operand;
    stream->count++;
}

void codegen_node(ASTNode* node, IRStream* stream) {
    if (node->kind == AST_NUMBER) {
        emit(stream, OP_PUSH, node->data.value);
    } else if (node->kind == AST_UNARY) {
        codegen_node(node->data.unary.operand, stream);
        emit(stream, OP_NEG, 0);
    } else if (node->kind == AST_BINARY) {
        codegen_node(node->data.binary.left, stream);
        codegen_node(node->data.binary.right, stream);
        switch (node->data.binary.op) {
            case '+': emit(stream, OP_ADD, 0); break;
            case '-': emit(stream, OP_SUB, 0); break;
            case '*': emit(stream, OP_MUL, 0); break;
            case '/': emit(stream, OP_DIV, 0); break;
        }
    }
}

IRStream codegen(ASTNode* ast) {
    IRStream stream = {NULL, 0, 0};
    codegen_node(ast, &stream);
    emit(&stream, OP_RET, 0);
    return stream;
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION 5 ── VIRTUAL MACHINE
// ─────────────────────────────────────────────────────────────────────────────

double execute(IRStream* stream) {
    double stack[256];
    int sp = 0;

    for (int i = 0; i < stream->count; i++) {
        Instruction instr = stream->code[i];
        if (instr.op == OP_PUSH) {
            stack[sp++] = instr.operand;
        } else if (instr.op == OP_NEG) {
            stack[sp - 1] = -stack[sp - 1];
        } else if (instr.op >= OP_ADD && instr.op <= OP_DIV) {
            if (sp < 2) { fprintf(stderr, "VMError: Stack underflow\n"); exit(1); }
            double b = stack[--sp];
            double a = stack[--sp];
            if (instr.op == OP_ADD) stack[sp++] = a + b;
            else if (instr.op == OP_SUB) stack[sp++] = a - b;
            else if (instr.op == OP_MUL) stack[sp++] = a * b;
            else if (instr.op == OP_DIV) {
                if (b == 0) { fprintf(stderr, "VMError: Division by zero\n"); exit(1); }
                stack[sp++] = a / b;
            }
        } else if (instr.op == OP_RET) {
            break;
        }
    }

    if (sp != 1) {
        fprintf(stderr, "VMError: Stack should have 1 value, has %d\n", sp);
        exit(1);
    }
    return stack[0];
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION 6 ── PRETTY-PRINTER
// ─────────────────────────────────────────────────────────────────────────────

void print_tree(ASTNode* node, char* prefix, int is_last) {
    const char* connector = is_last ? "└── " : "├── ";
    printf("%s%s", prefix, connector);

    char next_prefix[256];
    snprintf(next_prefix, sizeof(next_prefix), "%s%s", prefix, is_last ? "    " : "│   ");

    if (node->kind == AST_NUMBER) {
        if (node->data.value == (int)node->data.value) printf("Number(%d)\n", (int)node->data.value);
        else printf("Number(%.2f)\n", node->data.value);
    } else if (node->kind == AST_UNARY) {
        printf("Unary(neg)\n");
        print_tree(node->data.unary.operand, next_prefix, 1);
    } else if (node->kind == AST_BINARY) {
        printf("Binary('%c')\n", node->data.binary.op);
        print_tree(node->data.binary.left, next_prefix, 0);
        print_tree(node->data.binary.right, next_prefix, 1);
    }
}

void print_ast(ASTNode* node) {
    printf("AST\n");
    print_tree(node, "", 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION 7 ── DRIVER
// ─────────────────────────────────────────────────────────────────────────────

double compile_and_run(const char* source, int verbose) {
    if (verbose) {
        printf("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
        printf("  SOURCE:  \"%s\"\n", source);
        printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    }

    // 1. Lex
    int token_count;
    Token* tokens = lex(source, &token_count);
    if (verbose) {
        printf("\n[1] TOKENS\n────────────────────────────────────────────────────────────\n");
        for (int i = 0; i < token_count; i++) {
            if (tokens[i].kind == TOK_NUMBER)
                printf("    Token(%s, %g, @%d)\n", kind_to_string(tokens[i].kind), tokens[i].value, tokens[i].pos);
            else if (tokens[i].kind == TOK_EOF)
                printf("    Token(%s, '', @%d)\n", kind_to_string(tokens[i].kind), tokens[i].pos);
            else
                printf("    Token(%s, '%c', @%d)\n", kind_to_string(tokens[i].kind), tokens[i].op, tokens[i].pos);
        }
    }

    // 2. Parse
    ASTNode* ast = parse(tokens);
    if (verbose) {
        printf("\n[2] AST\n────────────────────────────────────────────────────────────\n");
        print_ast(ast);
    }

    // 3. Code-gen
    IRStream ir = codegen(ast);
    if (verbose) {
        printf("\n[3] STACK-MACHINE IR\n────────────────────────────────────────────────────────────\n");
        for (int i = 0; i < ir.count; i++) {
            printf("  %3d: ", i);
            Opcode op = ir.code[i].op;
            if (op == OP_PUSH) printf("    PUSH  %-10g ← push literal\n", ir.code[i].operand);
            else if (op == OP_NEG) printf("    NEG              ← negate top\n");
            else if (op == OP_ADD) printf("    ADD              ← pop 2, push result\n");
            else if (op == OP_SUB) printf("    SUB              ← pop 2, push result\n");
            else if (op == OP_MUL) printf("    MUL              ← pop 2, push result\n");
            else if (op == OP_DIV) printf("    DIV              ← pop 2, push result\n");
            else if (op == OP_RET) printf("    RET              ← halt; pop result\n");
        }
    }

    // 4. Execute
    double result = execute(&ir);
    if (verbose) {
        printf("\n[4] RESULT\n────────────────────────────────────────────────────────────\n");
        if (result == (int)result) printf("    %s  =  %d\n", source, (int)result);
        else printf("    %s  =  %g\n", source, result);
    } else {
        // Just print the number in interactive mode
        if (result == (int)result) printf("%d\n", (int)result);
        else printf("%g\n", result);
    }

    // Cleanup
    free(tokens);
    free_ast(ast);
    free(ir.code);

    return result;
}

// Read an entire file into memory
char* read_file(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Error: Could not open file '%s'\n", filename);
        exit(1);
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* buffer = malloc(length + 1);
    if (!buffer) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        exit(1);
    }

    fread(buffer, 1, length, file);
    buffer[length] = '\0';
    fclose(file);

    return buffer;
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION 8 ── TESTS
// ─────────────────────────────────────────────────────────────────────────────

void run_tests() {
    struct TestCase { const char* src; double expected; } cases[] = {
        {"42",                  42},
        {"1 + 2",                3},
        {"10 - 3",               7},
        {"3 * 4",               12},
        {"10 / 4",             2.5},
        {"-5",                  -5},
        {"--3",                  3},
        {"2 + 3 * 4",           14},
        {"(2 + 3) * 4",         20},
        {"10 - 2 - 3",           5},
        {"100 / 5 / 4",          5},
        {"-2 * -3",              6},
        {"(1 + 2) * (3 + 4)",   21},
        {"2 * (3 + -(4 - 1))",   0},
        {"1 + 2 + 3 + 4 + 5",   15}
    };
    int num_cases = sizeof(cases) / sizeof(cases[0]);

    printf("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("  TEST SUITE\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

    int passed = 0, failed = 0;
    for (int i = 0; i < num_cases; i++) {
        double got = compile_and_run(cases[i].src, 0);
        int ok = fabs(got - cases[i].expected) < 1e-9;
        
        if (ok) passed++; else failed++;
        
        printf("  %s  %-35s → expected=%g, got=%g\n", 
               ok ? "✓ PASS" : "✗ FAIL", cases[i].src, cases[i].expected, got);
    }
    printf("\n  %d/%d tests passed\n", passed, passed + failed);
}

void run_repl() {
    char line[1024];
    printf("Minimal Arithmetic Compiler REPL\n");
    printf("Type an expression and press Enter. Type 'exit' to quit.\n");
    
    while (1) {
        printf("\n>> ");
        if (!fgets(line, sizeof(line), stdin)) break; // Handle EOF (Ctrl+D)

        // Remove trailing newline
        line[strcspn(line, "\r\n")] = 0;
        
        // Skip empty lines
        if (strlen(line) == 0) continue;
        
        // Exit command
        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) break;

        // Compile and run the line (verbose = 1 to see the pipeline, change to 0 if you just want the answer)
        compile_and_run(line, 1); 
    }
    printf("Goodbye!\n");
}

// ─────────────────────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
    if (argc == 1) {
        // 1. Default behavior (no arguments): Start interactive REPL
        run_repl();
    } 
    else if (argc == 2 && strcmp(argv[1], "--test") == 0) {
        // 2. Test mode: ./minicompiler --test
        run_tests();
    } 
    else if (argc == 2) {
        // 3. File mode: ./minicompiler math.txt (or any other filename)
        const char* filename = argv[1];
        char* source = read_file(filename);
        compile_and_run(source, 1); // 1 = verbose mode on
        free(source);
    } 
    else {
        printf("Usage:\n");
        printf("  ./minicompiler             (Starts interactive REPL)\n");
        printf("  ./minicompiler --test      (Runs unit tests)\n");
        printf("  ./minicompiler <filename>  (Compiles and runs a file)\n");
    }
    
    return 0;
}