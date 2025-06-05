#include <stdint.h>
#include "tree_sitter/parser.h"
#include "tree_sitter/array.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stddef.h>

size_t not_found = SIZE_MAX;
uint32_t max_unsized = -1;

enum TokenType {
  LINE_START,
  LINE_END,        // Token type for line_end
  EMPHASIS_STAR_START,  // Token type for emphasis_start
  EMPHASIS_STAR_END,     // Token type for emphasis_end
  EMPHASIS_UNDER_START,
  EMPHASIS_UNDER_END,
  STRONG_STAR_START,
  STRONG_STAR_END,
  STRONG_UNDER_START,
  STRONG_UNDER_END,
  NO_PARSE,
  ERROR, //General Emphasis
};

enum ParseToken {
    NONE,
    DO_NOT_PARSE,
    EMPHASIS_STAR,
    EMPHASIS_UNDER,
    STRONG_STAR,
    STRONG_UNDER,
};



// this struct is for emphasis
// strong, and strong_emphasis
// sections. I dont predict that
// they will need the full size
// of a uint32, but i suppose it
// isn't imposible?
typedef struct {
  bool within;
  uint32_t row;
  uint32_t col;
} WithinRange;

typedef struct Pos {
    uint32_t row;
    uint32_t col;
} Pos;

typedef struct Range {
    Pos start;
    Pos end;
} Range;

enum RangeType {
    DISJOINT_LESS,
    OVERLAP,
    CHILD,
    PARENT,
    DISJOINT_GREATER
};

typedef struct LexWrap {
    TSLexer *lexer;
    Pos init_pos;
    Pos curr_pos;
    uint32_t pos;
    uint32_t line;
    Array(int32_t) buffer;
    Array(uint32_t) line_width;
    Array(uint32_t) new_line_loc;
} LexWrap;


static void print_letter(int32_t letter) {
    if (letter == '\n') {
        // fprintf(stderr, "'\\n'");
    } else {
        // fprintf(stderr, "'%c'", letter);
    }
}

static LexWrap new_lexer(TSLexer *lexer, Pos init_pos) {
    LexWrap obj;
    obj.lexer = lexer;
    obj.init_pos = init_pos;
    obj.curr_pos = init_pos;
    obj.pos = 0;
    obj.line = max_unsized;
    array_init(&obj.buffer);
    array_init(&obj.line_width);
    array_init(&obj.new_line_loc);
    return obj;
}

static int32_t lex_lookahead(LexWrap* wrapper) {

    if (wrapper->pos == wrapper->buffer.size) {
        return wrapper->lexer->lookahead;
    } else {
        return *array_get(&wrapper->buffer, wrapper->pos);
    }
}

static void lex_advance(LexWrap* wrapper, bool skip) {
    int32_t lookahead = lex_lookahead(wrapper);
    if (wrapper->pos == wrapper->buffer.size) {
        if (lookahead == '\n') {
            // fprintf(stderr, "found new line, documenting position at %i\n", wrapper->pos + 1);
            array_push(&wrapper->new_line_loc, wrapper->pos + 1);
        }
        array_push(&wrapper->buffer, lookahead);
        wrapper->lexer->advance(wrapper->lexer, skip);
    }
    if (lookahead != '\n') {
        wrapper->curr_pos.col++;
    } else {
        wrapper->curr_pos.row++;
        wrapper->line++;
        if (wrapper->line == wrapper->line_width.size) {
            array_push(&wrapper->line_width, wrapper->curr_pos.col);
        }
        wrapper->curr_pos.col = 0;
    }
    wrapper->pos++;

    // fprintf(stderr, " * consuming: ");
    print_letter(lookahead);
    // fprintf(stderr, "\n");
}

static void lex_backtrack_n(LexWrap* wrapper, uint32_t n) {
    // fprintf(stderr, "n: %i -- wrapper->pos: %i\n", n, wrapper->pos);
    assert(n <= wrapper->pos);
    int32_t *letter;
    for(uint32_t i = 0; i < n; i++) {
        wrapper->pos--;
        letter = &wrapper->buffer.contents[wrapper->pos];
        if (*letter != '\n') {
            wrapper->curr_pos.col--;
        } else {
            wrapper->curr_pos.row--;
            assert(wrapper->line < wrapper->line_width.size);
            wrapper->curr_pos.col = wrapper->line_width.contents[wrapper->line];
            wrapper->line--;
        }
    }
}

static void lex_set_position(LexWrap *wrapper, uint32_t pos) {
    if (pos >= wrapper->pos) {
        uint32_t diff = pos - wrapper->pos;
        for (uint32_t i = 0; i < diff; i++) {
            lex_advance(wrapper, false);
        }
    } else {
        uint32_t diff = wrapper->pos - pos;
        lex_backtrack_n(wrapper, diff);
    }
}


static Pos new_position(uint32_t row, uint32_t col) {
    Pos obj;
    obj.row = row;
    obj.col = col;
    return obj;
}

static bool pos_eq(Pos *x, Pos *y) {
    return (x->row == y->row) && (x->col == y->col);
}

static bool pos_ne(Pos *x, Pos *y) {
    return (x->row != y->row) || (x->col != y->col);
}

/// x < y
static bool pos_lt(Pos *x, Pos *y) {
    return (x->row < y->row ) || (x->row == y->row && x->col < y->col);
}

/// x <= y
static bool pos_le(Pos *x, Pos *y) {
    return x->row <= y->row && x->col <= y->col;
}

static bool pos_gt(Pos *x, Pos *y) {
    return (x->row > y->row) || (x->row == y->row && x->col > y ->col);
}

static bool pos_ge(Pos *x, Pos *y) {
    return x->row >= y->row && x->col >= y->col;
}

static void print_pos(const Pos *pos) {
    // fprintf(stderr, "Pos { row: %u, col: %u }", pos->row, pos->col);
}

static void debug_pos(const Pos *pos) {
    // fprintf(stderr, "[%u, %u]", pos->row, pos->col);
}

static Pos lex_current_position(LexWrap *wrapper) {
    Pos range = new_position(wrapper->init_pos.row, wrapper->init_pos.col + wrapper->pos);
    // fprintf(stderr, "current position: ");
    debug_pos(&range);
    if (wrapper->new_line_loc.size > 0) {
        uint32_t diff;
        uint32_t last_index = 0;
        uint32_t line_index = 0;
        // *array_get(&wrapper->new_line_loc, i);
        for (uint32_t i = 0; i < wrapper->new_line_loc.size; i++) {
            line_index = wrapper->new_line_loc.contents[i];
            if (line_index > wrapper->pos) {
                break;
            }
            range.row++;
            diff = line_index - last_index;
            last_index = line_index;
            range.col -= diff;
            // fprintf(stderr, " ");
            debug_pos(&range);
        }
        // fprintf(stderr, "\n");
    }
    return range;
}

typedef struct ParseResult {
    bool success;
    uint32_t length;
    Range range;
    enum ParseToken token;
} ParseResult;

static Range new_range(Pos start, Pos end) {
    Range obj;
    obj.start = start;
    obj.end = end;
    return obj;
}

static ParseResult new_parse_result() {
    ParseResult obj;
    obj.success = false;
    obj.length = 0;
    obj.range = new_range(new_position(0, 0), new_position(0, 0));
    obj.token = NONE;
    return obj;
}

typedef Array(ParseResult) ParseResultArray;
typedef Array(uint32_t) IndexArray;

static bool pos_within_range(Pos *x, Range *y) {
    return pos_le(x, &y->end) && pos_ge(x, &y->start);
}

/// x:   |----|
/// y:  |-------|
static bool range_within(Range *x, Range *y) {
    return pos_gt(&x->end, &y->start) && pos_lt(&x->start, &y->end);
}

/// x: |---|
/// y:       |----|
static bool range_disjoint(Range *x, Range *y) {
    return pos_ge(&y->start, &x->end) ||  pos_ge(&x->start, &y->end);
}

static enum RangeType classify_range(Range *x, Range *y) {
    // |----|
    //        |----|

    if (pos_le(&x->end, &y->start)) {
        return DISJOINT_LESS;
    }
    if (pos_le(&x->end, &y->end)) {
        if (pos_ge(&x->start, &y->start)) {
            return CHILD;
        } else {
            return OVERLAP;
        }
    } else {
        if (pos_lt(&x->start, &y->start)) {
            return PARENT;
        }

        if (pos_lt(&x->start, &y->end)) {
            return OVERLAP;
        } else {
            return DISJOINT_GREATER;
        }
    }

}

static void print_parse_result(const ParseResult *res) {
    // fprintf(stderr, "ParseResult { success: %d, length: %u, range: ", res->success, res->length);
    // fprintf(stderr, "[%i, %i] - ", res->range.start.row, res->range.start.col);
    // fprintf(stderr, "[%i, %i]", res->range.end.row, res->range.end.col);
    // fprintf(stderr, ", token: %d }\n", res->token);
}

static void print_stack(ParseResultArray *stack) {
    for (uint32_t i = 0; i < stack->size; i++) {
        // fprintf(stderr, "\t");
        print_parse_result(&stack->contents[i]);
    }
}

static size_t stack_insert(ParseResultArray* array, ParseResult element) {
    size_t out = not_found;
    if (array->size == 0) {
        array_push(array, element);
        out = 0;
        goto func_end;
    } else {
        for (size_t i = 0; i < array->size; i++) {
            ParseResult *result = &array->contents[i];
            switch (classify_range(&element.range, &result->range)) {
                case OVERLAP: {
                    // fprintf(stderr, "OVERLAP found [%i, %i] - [%i, %i] ... [%i, %i] - [%i, %i] ",
                        // element.range.start.row,
                        // element.range.start.col,
                        // element.range.end.row,
                        // element.range.end.col,
                        // result->range.start.row, result->range.start.col,
                        // result->range.end.row, result->range.end.col);
                    out = not_found;
                    goto func_end;
                }
                case PARENT: {
                    array_insert(array, i, element);
                    out = i;
                    goto func_end;
                }
                case DISJOINT_LESS: {
                    array_insert(array, i, element);
                    out = i;
                    goto func_end;
                }
                case DISJOINT_GREATER: {
                    array_insert(array, i + 1, element);
                    out = i + 1;
                    goto func_end;
                }
                case CHILD: {
                    continue;
                }
            }
        }
    }

    func_end: {
        if (out == not_found) {
            // fprintf(stderr, "attempting to insert: ");
            print_parse_result(&element);
        }
        // fprintf(stderr, "insert was %ssuccessful: \n", out==not_found ? "un" : "");
        print_stack(array);
        return out;
    }


}



static size_t stack_find(ParseResultArray *array, Pos *pos, enum ParseToken token, bool end) {
    ParseResult *element;
    if (end) {
        for (size_t i = 0; i < array->size; i++) {
            element = &array->contents[i];
            if (element->token == token && pos_eq(&element->range.end, pos)) {
                return i;
            }
        }
    } else {
        for (size_t i = 0; i < array->size; i++) {
            element = &array->contents[i];
            if (element->token == token && pos_eq(&element->range.start, pos)) {
                return i;
            }
        }
    }
    return not_found;
}

static size_t stack_find_within(ParseResultArray *array, Pos *pos, enum ParseToken token) {
     ParseResult *element;
    for (size_t i = 0; i < array->size; i++) {
        element = &array->contents[i];
        if (element->token == token && pos_within_range(pos, &element->range)) {
            return i;
        }
    }
    return not_found;
}

static size_t stack_find_exact(ParseResultArray *array,  ParseResult *res) {
    ParseResult *element;
   for (size_t i = 0; i < array->size; i++) {
       element = &array->contents[i];
       if (element->token == res->token &&
           pos_eq(&element->range.start, &res->range.start) &&
           pos_eq(&element->range.end, &res->range.end)) {
           return i;
       }
   }
   return not_found;
}


static bool is_whitespace(int32_t char_) {
    return char_ == ' ' || char_ == '\t' || char_ == '\n';
}
static bool is_whitespace_next(TSLexer *lexer) {
    return is_whitespace(lexer->lookahead);
}

static bool is_inline_synatx(int32_t char_) {
    return char_ == '*' || char_ == '_' ||
     char_ == '^' || char_ == '~' ||
     char_ == '`' || char_ == '@' ||
     char_ == '[' || char_ == ']';
}

// prototypes:

static ParseResult parse_inline(LexWrap *wrapper, ParseResultArray* stack);
static ParseResult parse_star(LexWrap *wrapper, ParseResultArray* stack);
static ParseResult parse_under(LexWrap *wrapper, ParseResultArray* stack);

static ParseResult parse_inline(LexWrap *wrapper, ParseResultArray* stack) {
    // fprintf(stderr, "calling parse_inline()\n");
    // uint32_t stack_start_size = stack->size;
    uint32_t buffer_start_pos = wrapper->pos;
    ParseResult res = new_parse_result();
    res.range.start = wrapper->curr_pos;
    int32_t lookahead = lex_lookahead(wrapper);
    switch (lookahead) {
        case '*': {
            res = parse_star(wrapper, stack);
            break;
        }

        case '_': {
            res = parse_under(wrapper, stack);
            break;
        }

    }
    if (!res.success) {

        res.range.end = wrapper->curr_pos;
        res.token = DO_NOT_PARSE;
        res.length = wrapper->pos - buffer_start_pos;
    }
    // fprintf(stderr, "inline parse results: ");
    print_parse_result(&res);
    return res;
}

/// takes a result object, and inserts appropriate DO_NOT_PARSE
/// tokens into the stack. optionally, it will delete the result
/// if the element exists in the stack
static size_t dont_parse_result(ParseResult *result, ParseResultArray *array, bool remove) {
    switch (result->token) {
        case EMPHASIS_STAR:
        case EMPHASIS_UNDER: {
            ParseResult emph_start = new_parse_result();
            emph_start.token = DO_NOT_PARSE;
            emph_start.range.start = result->range.start;
            ParseResult emph_end = new_parse_result();
            emph_end.token = DO_NOT_PARSE;
            emph_end.range.end = result->range.end;
            emph_start.range.end = emph_start.range.start;
            emph_start.length = 1;
            emph_start.range.end.col += 1;
            emph_end.range.start = emph_end.range.end;
            emph_end.range.start.col -= 1;
            emph_end.length = 1;
            stack_insert(array, emph_start);
            stack_insert(array, emph_end);
            break;
        }
        case STRONG_STAR:
        case STRONG_UNDER: {
            ParseResult strong_start = new_parse_result();
            strong_start.token = DO_NOT_PARSE;
            strong_start.range.start = result->range.start;
            ParseResult strong_end = new_parse_result();
            strong_end.token = DO_NOT_PARSE;
            strong_end.range.end = result->range.end;
            strong_start.range.end = strong_start.range.start;
            strong_start.length = 2;
            strong_start.range.end.col += 2;
            strong_end.range.start = strong_end.range.end;
            strong_end.range.start.col -= 2;
            strong_end.length = 2;
            stack_insert(array, strong_start);
            stack_insert(array, strong_end);
        }

        default: {

        }
    }

    if (remove) {
        size_t index = stack_find_exact(array, result);
        if (index < not_found) {
            array_erase(array, index);
            return index;
        }
    }


    return not_found;
}

static void dont_parse_next_n(LexWrap *wrapper, ParseResultArray *stack, uint32_t n) {
    if (n > 0) {
        ParseResult result = new_parse_result();
        result.range.start = wrapper->curr_pos;
        for (uint32_t i = 0; i < n; i++) {
            lex_advance(wrapper, false);
        }
        result.range.end = wrapper->curr_pos;
        result.length = n;
        result.success = true;
        result.token = DO_NOT_PARSE;
        stack_insert(stack, result);
    }

}

static ParseResult parse_star(LexWrap *wrapper, ParseResultArray* stack) {
    // fprintf(stderr, "calling - parse_star()\n");
    // uint32_t stack_start_size = stack->size;
    uint32_t buffer_start_pos = wrapper->pos;
    ParseResult res = new_parse_result();
    res.range.start = wrapper->curr_pos;

    /// for this parse to be valid one of
    /// if we detect 1 --> expecting emphasis
    /// if we detect 2 --> expecting strong
    /// if we detect 3 --> expecting combo of emphasis or strong
    ///                    with the possibility of either ending
    ///                    early.
    /// > 3 --> return false
    uint8_t char_count = 0;
    while (lex_lookahead(wrapper) == '*') {
        lex_advance(wrapper, false);
        char_count++;
    }
    switch (char_count) {
        case 1: {
            res.token = EMPHASIS_STAR;
            break;
        }
        case 2: {
            res.token = STRONG_STAR;
            break;
        }
        default: {}
    }
    if (char_count > 3) {
        // as a special feature, we insert this into
        // the stack to signal that it should not match
        // any symbols

        res.success = true;
        res.range.end = wrapper->curr_pos;
        res.length = char_count;
        res.token = DO_NOT_PARSE;
        // wrapper->lexer->mark_end(wrapper->lexer);
        // fprintf(stderr, "returning a NO_PARSE result:");
        print_parse_result(&res);
        // fprintf(stderr, "\n");
        stack_insert(stack, res);
        return res;
    }
    int32_t lookahead = lex_lookahead(wrapper);
    if (is_whitespace(lookahead)) {
        // cannot parse star as any type of valid
        // emphasis or strong.
        return res;
    }
    uint32_t last_lex_pos = 0;
    uint8_t end_char_count = 0;
    uint8_t new_line_count = 0;
    while(lookahead != '\0' && char_count > 0) {
        switch (lookahead) {
            case '*': {
                // see how many we can consume
                end_char_count = 0;
                while (lex_lookahead(wrapper) == '*') {
                    lex_advance(wrapper, false);
                    end_char_count++;
                }
                switch (char_count) {
                    case 1: {
                        // we only have one left to match...
                        // no matter the size of end_char_count
                        lex_backtrack_n(wrapper, end_char_count - 1);
                        last_lex_pos = wrapper->pos;
                        res.range.end = wrapper->curr_pos;
                        res.token = EMPHASIS_STAR;

                        switch (end_char_count) {
                            case 2: {
                                lex_backtrack_n(wrapper, 1);
                                ParseResult attempt = parse_star(wrapper, stack);
                                if (attempt.success) {
                                    lookahead = lex_lookahead(wrapper);
                                    continue;
                                } else {
                                    lex_set_position(wrapper, last_lex_pos - 1);
                                    dont_parse_next_n(wrapper, stack, 2);
                                }
                                break;
                            }
                            default: {
                                res.length = last_lex_pos - buffer_start_pos;
                                res.success = true;
                                break;
                            }
                        }

                        goto return_res;
                        break;
                    }
                    case 2: {
                        last_lex_pos = wrapper->pos;
                        res.token = STRONG_STAR;
                        switch (end_char_count) {
                            case 1: {
                                lex_backtrack_n(wrapper, 1);
                                ParseResult attempt = parse_star(wrapper, stack);
                                if (attempt.success) {
                                    lookahead = lex_lookahead(wrapper);
                                    continue;
                                } else {
                                    lex_set_position(wrapper, last_lex_pos - 1);
                                    dont_parse_next_n(wrapper, stack, 1);
                                }
                                break;
                            }
                            default: {
                                // no matter how many match here. we have
                                // reached our target.
                                lex_backtrack_n(wrapper, end_char_count - 2);
                                res.range.end = wrapper->curr_pos;
                                res.success = true;
                                res.length = wrapper->pos - buffer_start_pos;
                                break;
                            }
                        }
                        goto return_res;
                        break;
                    }
                    case 3: {
                        switch (end_char_count){
                            case 1: {
                                // inner syntax is an emph and outer is
                                // likely a strong.
                                // create new result to insert
                                ParseResult inner = new_parse_result();
                                inner.range.end = wrapper->curr_pos;
                                inner.range.start = res.range.start;
                                inner.range.start.col += 2;
                                inner.success = true;
                                inner.token = EMPHASIS_STAR;
                                inner.length = wrapper->pos - buffer_start_pos - 2;
                                if (stack_insert(stack, inner) < not_found) {
                                    char_count--;
                                }
                                lookahead = lex_lookahead(wrapper);
                                continue;
                            }
                            case 2: {
                                // inner syntax is an strong and outer is
                                // likely a emph.
                                // create new result to insert
                                ParseResult inner = new_parse_result();
                                inner.range.end = wrapper->curr_pos;
                                inner.range.start = res.range.start;
                                inner.range.start.col += 1;
                                inner.success = true;
                                inner.token = STRONG_STAR;
                                inner.length = wrapper->pos - buffer_start_pos - 1;
                                if (stack_insert(stack, inner) < not_found) {
                                    char_count -= 2;
                                }
                                lookahead = lex_lookahead(wrapper);
                                continue;
                            }
                            default: {
                                // no matter how many times we detected
                                // a '*'
                                // we have matched our stack!
                                lex_backtrack_n(wrapper, end_char_count - 3);
                                res.range.end = wrapper->curr_pos;
                                res.success = true;
                                res.token = STRONG_STAR;
                                res.length = wrapper->pos - buffer_start_pos;
                                // inner will be an emphasis
                                ParseResult inner = new_parse_result();
                                inner.range.end = wrapper->curr_pos;
                                inner.range.end.col -= 2;
                                inner.range.start = res.range.start;
                                inner.range.start.col += 2;
                                inner.success = true;
                                inner.token = EMPHASIS_STAR;
                                inner.length = wrapper->pos - buffer_start_pos - 2;
                                size_t index = stack_insert(stack, inner);
                                if (index == not_found) {
                                    res.success = false;
                                }
                                break;
                            }
                        }
                        goto return_res;
                        break;
                    }
                }
                break;
            }
            case '\n': {
                new_line_count++;
                if (new_line_count > 1) {
                    return res;
                }
                break;
            }
            case '\\': {
                // treat next character as literal - do not
                // parse it
                lex_advance(wrapper, false);
                break;
            }
            default: {
                // check if inline symbol
                new_line_count = 0;
                if (is_inline_synatx(lookahead)) {
                    ParseResult attempt = parse_inline(wrapper, stack);
                    if (!attempt.success) {
                        return res;
                    }
                    lookahead = lex_lookahead(wrapper);
                    continue;
                }
            }
                break;
        }
        lex_advance(wrapper, false);
        lookahead = lex_lookahead(wrapper);
    }

    goto return_res;
    return_res: {

        if (res.success) {
            stack_insert(stack, res);
        } else {
            // fprintf(stderr, "failed parsing: ");
            print_parse_result(&res);
            // we do not know if result ranges are correct...
            ParseResult start = new_parse_result();
            start.token = DO_NOT_PARSE;
            start.range.start = res.range.start;
            start.range.end = res.range.start;
            start.success = true;
            switch (res.token) {
                case EMPHASIS_STAR: {
                    start.range.end.col++;
                    start.length = 1;
                    break;
                }
                case STRONG_STAR: {
                    start.range.end.col += 2;
                    start.length = 2;
                    break;
                }
                default: {}
            }
            if (start.length > 0) {
                stack_insert(stack, start);
            }
        }
        // fprintf(stderr, "parser is at position: ");
        debug_pos(&wrapper->curr_pos);
        // fprintf(stderr, "\n");
        return res;
    }

}

static ParseResult parse_under(LexWrap *wrapper, ParseResultArray* stack) {
    // fprintf(stderr, "calling - parse_under()\n");
    // uint32_t stack_start_size = stack->size;
    uint32_t buffer_start_pos = wrapper->pos;
    uint32_t last_lex_pos = wrapper->pos;
    ParseResult res = new_parse_result();
    res.range.start = wrapper->curr_pos;

    /// for this parse to be valid one of
    /// if we detect 1 --> expecting emphasis
    /// if we detect 2 --> expecting strong
    /// if we detect 3 --> expecting combo of emphasis or strong
    ///                    with the possibility of either ending
    ///                    early.
    /// > 3 --> return false
    uint8_t char_count = 0;
    while (lex_lookahead(wrapper) == '_') {
        lex_advance(wrapper, false);
        char_count++;
    }
    switch (char_count) {
        case 1: {
            res.token = EMPHASIS_UNDER;
            break;
        }
        case 2: {
            res.token = STRONG_UNDER;
            break;
        }
        default: {}
    }
    if (char_count > 3) {
        // as a special feature, we insert this into
        // the stack to signal that it should not match
        // any symbols

        res.success = true;
        res.range.end = wrapper->curr_pos;
        res.length = char_count;
        // wrapper->lexer->mark_end(wrapper->lexer);
        // fprintf(stderr, "returning a NONE result:");
        print_parse_result(&res);
        // fprintf(stderr, "\n");
        stack_insert(stack, res);
        return res;
    }
    int32_t lookahead = lex_lookahead(wrapper);
    if (is_whitespace(lookahead)) {
        // cannot parse star as any type of valid
        // emphasis or strong.
        return res;
    }
    uint32_t last_char = ' ';
    uint8_t end_char_count = 0;
    uint8_t new_line_count = 0;
    while(lookahead != '\0' && char_count > 0) {
        // fprintf(stderr, "lookahead - %c\n", lookahead);
        switch (lookahead) {
            case '_': {
                // see how many we can consume
                end_char_count = 0;
                while (lex_lookahead(wrapper) == '_') {
                    lex_advance(wrapper, false);
                    end_char_count++;
                }
                int32_t next_char = lex_lookahead(wrapper);
                switch (char_count) {
                    case 1: {
                        // we only have one left to match...
                        // no matter the size of end_char_count
                        lex_backtrack_n(wrapper, end_char_count - 1);
                        last_lex_pos = wrapper->pos;
                        res.range.end = wrapper->curr_pos;
                        res.token = EMPHASIS_UNDER;
                        res.length = wrapper->pos - buffer_start_pos;
                        // we do not know if it is valid yet...

                        switch (end_char_count) {
                            case 1: {
                                // do we need to check that the next character
                                // is syntax???
                                if (!isalpha(next_char)) {
                                    // if the next character is NOT alphabet
                                    // then we can complete this case
                                    res.success = true;
                                } else if (!isalpha(last_char)) {
                                    // we know the next character IS alphabet,
                                    // which automatically invalidates the current
                                    // scope
                                    // do not modify beginning, leave that for the return_res section
                                    // however if the last character is NOT alphabet
                                    // then it is possible to parse the next _.
                                    lex_backtrack_n(wrapper, 1);
                                    ParseResult attempt = parse_under(wrapper, stack);
                                    if (!attempt.success) {
                                        lex_set_position(wrapper, last_lex_pos);
                                    }
                                } else {
                                    // special case where we treat it as literal
                                    lex_backtrack_n(wrapper, 1);
                                    dont_parse_next_n(wrapper, stack, 1);
                                    last_char = '_';
                                    lookahead = next_char;
                                    continue;
                                }

                                break;

                            }
                            case 2: {
                                // interestingly, if we can parse this
                                // token, it takes precendence
                                lex_backtrack_n(wrapper, 1);
                                Pos pos = wrapper->curr_pos;
                                // fprintf(stderr, "about to call parse_under: ");
                                debug_pos(&pos);
                                // fprintf(stderr, "\n");
                                ParseResult attempt = parse_under(wrapper, stack);
                                // fprintf(stderr, "returned with: ");
                                print_parse_result(&attempt);
                                // fprintf(stderr, " and at position: ");
                                pos = wrapper->curr_pos;
                                debug_pos(&pos);
                                // fprintf(stderr, "\n");
                                if (!attempt.success) {
                                    lex_set_position(wrapper, last_lex_pos + 1);
                                    break;
                                }

                                // if it was successful, our lexer should be at the end
                                // of the lexed token.
                                Pos last_pos = wrapper->curr_pos;
                                print_pos(&last_pos);
                                size_t index = stack_find(stack, &last_pos, DO_NOT_PARSE, true);
                                print_stack(stack);
                                if (index < not_found) {
                                    // fprintf(stderr, "index at %zu\n", index);
                                    array_erase(stack, index);
                                    last_pos = wrapper->curr_pos;
                                    print_pos(&last_pos);
                                    lex_backtrack_n(wrapper, 1);
                                }
                                // if it was successful, there is a chance to
                                // finish this parse.
                                last_char = '_';
                                lookahead = lex_lookahead(wrapper);
                                continue;
                            }
                            case 3: {
                                if (!isalpha(next_char)) {
                                    res.success = true;
                                    dont_parse_next_n(wrapper, stack, 1);
                                } else {
                                    lex_backtrack_n(wrapper, 1);
                                    // dont_parse_result(&res, stack, false);
                                    dont_parse_next_n(wrapper, stack, 2);
                                }
                                break;
                            }
                            default: {
                                res.success = true;
                                Pos pos = wrapper->curr_pos;
                                print_pos(&pos);
                                dont_parse_next_n(wrapper, stack, 1);
                                print_stack(stack);
                                break;
                            }
                        }
                        // fprintf(stderr, "returning from case 1: ");
                        goto return_res;
                        break;
                    }
                    case 2: {
                        // we do not know if it is valid yet...
                        res.token = STRONG_UNDER;
                        switch (end_char_count) {
                            case 1: {
                                // a single token cannot satisfy this condition
                                // this is either parsible
                                // or ignore this token
                                // or invalidates the entire thing
                                lex_backtrack_n(wrapper, 1);
                                last_lex_pos = wrapper->pos;
                                res.range.end = wrapper->curr_pos;
                                res.length = last_lex_pos - buffer_start_pos;

                                if (!isalpha(last_char) && isalpha(next_char)) {

                                    ParseResult attempt = parse_under(wrapper, stack);
                                    if (!attempt.success) {
                                        lex_set_position(wrapper, last_lex_pos + 1);
                                        break;
                                    }
                                    // check if last position was ignored
                                    Pos last_pos = wrapper->curr_pos;
                                    print_pos(&last_pos);
                                    size_t index = stack_find(stack, &last_pos, DO_NOT_PARSE, true);
                                    print_stack(stack);
                                    if (index < not_found) {
                                        // fprintf(stderr, "index at %zu\n", index);
                                        array_erase(stack, index);
                                        last_pos = wrapper->curr_pos;
                                        print_pos(&last_pos);
                                        lex_backtrack_n(wrapper, 1);
                                    }
                                    lookahead = lex_lookahead(wrapper);

                                } else {
                                    // if we cannot parse it, we skip it
                                    dont_parse_next_n(wrapper, stack, 1);
                                    lookahead = next_char;
                                }
                                continue;
                            }
                            case 2:  {
                                last_lex_pos = wrapper->pos;
                                res.length = last_lex_pos - buffer_start_pos;
                                res.range.end = wrapper->curr_pos;
                                // do we need to check that the next character
                                // is syntax???
                                if (!isalpha(next_char)) {
                                    // if the next character is NOT alphabet
                                    // then we can complete this case
                                    res.success = true;
                                    break;
                                } else if (!isalpha(last_char)) {
                                    // we know the next character IS alphabet,
                                    // which automatically invalidates the current
                                    // scope
                                    // however if the last character is NOT alphabet
                                    // then it is possible to parse the next _.
                                    lex_backtrack_n(wrapper, 2);
                                    ParseResult attempt = parse_under(wrapper, stack);
                                    if (!attempt.success) {
                                        lex_set_position(wrapper, last_lex_pos);
                                        // dont_parse_next_n(wrapper, stack, 2);
                                    }
                                }

                                break;
                            }
                            default: {
                                lex_backtrack_n(wrapper, end_char_count - 2);
                                last_lex_pos = wrapper->pos;
                                res.success = true;
                                res.range.end = wrapper->curr_pos;
                                res.length = last_lex_pos - buffer_start_pos;
                                print_pos(&res.range.end);
                                dont_parse_next_n(wrapper, stack, 1);
                            }
                        }
                        // fprintf(stderr, "returning from case 2: ");
                        goto return_res;
                        break;
                    }
                    case 3: {
                        switch (end_char_count){
                            case 1: {
                                last_lex_pos = wrapper->pos;
                                // if the next character is not an alphabet
                                // then we know that the inner set is an
                                // emphasis.
                                if (!isalpha(next_char)) {
                                    ParseResult inner = new_parse_result();
                                    inner.range.end = wrapper->curr_pos;
                                    inner.range.start = res.range.start;
                                    inner.range.start.col += 2;
                                    inner.success = true;
                                    inner.token = EMPHASIS_UNDER;
                                    inner.length = last_lex_pos - buffer_start_pos - 2;
                                    if (stack_insert(stack, inner) < not_found) {
                                        res.token = STRONG_UNDER;
                                        char_count--;
                                    }
                                    lookahead = next_char;
                                } else if (!isalpha(last_char)) {
                                    // we know the next character IS alphabet,
                                    // unlike where we have 1 leading _,
                                    // this may not be invalidated immediately
                                    // however if the last character is NOT alphabet
                                    // then it is possible to parse the next _.
                                    lex_backtrack_n(wrapper, 1);
                                    ParseResult res = parse_under(wrapper, stack);
                                    if (!res.success) {
                                        lex_set_position(wrapper, last_lex_pos);
                                        // dont_parse_next_n(wrapper, stack, 1);
                                    }
                                    lookahead = lex_lookahead(wrapper);
                                } else {
                                    lookahead = next_char;
                                }
                                // at the end of this case the lexer should
                                // be ready to continue
                                last_char = '_';
                                continue;
                            }
                            case 2: {
                                // inner syntax is an strong and outer is
                                // likely a emph.
                                // create new result to insert
                                if (!isalpha(next_char)) {
                                    ParseResult inner = new_parse_result();
                                    inner.range.end = wrapper->curr_pos;
                                    inner.range.start = res.range.start;
                                    inner.range.start.col += 1;
                                    inner.success = true;
                                    inner.token = STRONG_UNDER;
                                    inner.length = wrapper->pos - buffer_start_pos - 1;
                                    if (stack_insert(stack, inner) < not_found) {
                                        res.token = EMPHASIS_UNDER;
                                        char_count -= 2;
                                    }
                                } else {
                                    // if the next character IS an alphabet,
                                    // an odd thing occurs...
                                    // the inner becomes an emphasis and the second
                                    // _ is a literal.
                                    lex_backtrack_n(wrapper, 1);
                                    ParseResult inner = new_parse_result();
                                    inner.range.end = wrapper->curr_pos;
                                    inner.range.start = res.range.start;
                                    inner.range.start.col += 2;
                                    inner.success = true;
                                    inner.token = EMPHASIS_UNDER;
                                    inner.length = wrapper->pos - buffer_start_pos - 2;
                                    if (stack_insert(stack, inner) < not_found) {
                                        res.token = STRONG_UNDER;
                                        char_count--;
                                    }
                                    dont_parse_next_n(wrapper, stack, 1);
                                }
                                lookahead = next_char;
                                continue;
                            }
                            case 3: {
                                if (!isalpha(next_char)) {
                                    // complete match
                                    lex_backtrack_n(wrapper, end_char_count - 3);
                                    res.token = STRONG_UNDER;
                                    res.success = true;
                                    res.range.end = wrapper->curr_pos;
                                    res.length = wrapper->pos - buffer_start_pos;
                                    ParseResult inner = new_parse_result();
                                    inner.range.end = wrapper->curr_pos;
                                    inner.range.end.col -= 2;
                                    inner.range.start = res.range.start;
                                    inner.range.start.col += 2;
                                    inner.success = true;
                                    inner.token = EMPHASIS_UNDER;
                                    inner.length = wrapper->pos - buffer_start_pos - 4;
                                    size_t index = stack_insert(stack, inner);
                                    if (index == not_found) {
                                        res.success = false;
                                    }
                                } else {
                                    ParseResult inner = new_parse_result();
                                    lex_backtrack_n(wrapper, 1);
                                    inner.range.end = wrapper->curr_pos;
                                    inner.range.start = res.range.start;
                                    inner.range.start.col++;
                                    inner.success = true;
                                    inner.token = STRONG_UNDER;
                                    inner.length = wrapper->pos - buffer_start_pos - 1;
                                    size_t index = stack_insert(stack, inner);
                                    if (index == not_found) {
                                        res.success = false;
                                        return res;
                                    }
                                    dont_parse_next_n(wrapper, stack, 1);
                                    lookahead = next_char;
                                    char_count -= 2;
                                    continue;
                                }
                                break;
                            }
                            default: {
                                // no matter how many times we detected
                                // a '_'

                                lex_backtrack_n(wrapper, end_char_count - 3);
                                last_lex_pos = wrapper->pos;
                                res.token = STRONG_UNDER;
                                res.success = true;
                                res.range.end = wrapper->curr_pos;
                                res.length = last_lex_pos - buffer_start_pos;
                                ParseResult inner = new_parse_result();
                                inner.range.end = wrapper->curr_pos;
                                inner.range.end.col -= 2;
                                inner.range.start = res.range.start;
                                inner.range.start.col += 2;
                                inner.success = true;
                                inner.token = EMPHASIS_UNDER;
                                inner.length = res.length - 4;
                                size_t index = stack_insert(stack, inner);
                                if (index == not_found) {
                                    res.success = false;
                                }
                                dont_parse_next_n(wrapper, stack, 1);
                                break;
                            }
                        }
                        // fprintf(stderr, "returning from case 3:");
                        goto return_res;
                        break;
                    }
                }
                break;
            }
            case '\n': {
                new_line_count++;
                if (new_line_count > 1) {
                    // fprintf(stderr, "found too many '\\n' characters. returning...\n");
                    goto return_res;
                }
                break;
            }
            case '\\': {
                // treat next character as literal - do not
                // parse it
                lex_advance(wrapper, false);
                break;
            }
            default: {
                // check if inline symbol
                new_line_count = 0;
                if (is_inline_synatx(lookahead)) {
                    ParseResult attempt = parse_inline(wrapper, stack);
                    // should probabbly decide how to handle inline parse failures
                    // maybe they should just be considered literal for this purpose
                    // or maybe just ignored for later?
                    if(!attempt.success) {
                        // do something here?
                    }
                    lookahead = lex_lookahead(wrapper);
                    continue;
                }
                break;
            }
        }
        // fprintf(stderr, "made it to the end of the loop - last_char %c, lookahead %c \n", last_char, lookahead);
        last_char = lookahead;
        lex_advance(wrapper, false);
        lookahead = lex_lookahead(wrapper);
    }
    // fprintf(stderr, "reached end of file\n");
    goto return_res;
    return_res: {

        if (res.success) {
            stack_insert(stack, res);
        } else {
            // fprintf(stderr, "failed parsing: ");
            print_parse_result(&res);
            // we do not know if result ranges are correct...
            ParseResult start = new_parse_result();
            start.token = DO_NOT_PARSE;
            start.range.start = res.range.start;
            start.range.end = res.range.start;
            start.success = true;
            switch (res.token) {
                case EMPHASIS_UNDER: {
                    start.range.end.col++;
                    start.length = 1;
                    break;
                }
                case STRONG_UNDER: {
                    start.range.end.col += 2;
                    start.length = 2;
                    break;
                }
                default: {}
            }
            if (start.length > 0) {
                stack_insert(stack, start);
            }
        }
        // fprintf(stderr, "parser is at position: ");
        debug_pos(&wrapper->curr_pos);
        // fprintf(stderr, "\n");
        return res;
    }


}



typedef struct {
  Pos pos;
  ParseResultArray results; // State to track if we're inside an emphasis block
} ScannerState;

static void print_scanner_state(const ScannerState *state) {
    // fprintf(stderr, "ScannerState {\n  pos: ");
    print_pos(&state->pos);
    // fprintf(stderr, "\n  results (size: %u):\n", state->results.size);
    for (uint32_t i = 0; i < state->results.size; i++) {
        // fprintf(stderr, "\t");
        print_parse_result(&state->results.contents[i]);
    }
    // fprintf(stderr, "}\n");
}

static void print_lexwrap(const LexWrap *wrap) {
    // fprintf(stderr, "LexWrap {\n  init_pos: [%u, %u]\n  pos: %u\n", wrap->init_pos.row,
        // wrap->init_pos.col, wrap->pos);
    // fprintf(stderr, "  buffer (size: %u)\n", wrap->buffer.size);
    // fprintf(stderr, " new_line_loc (size: %u): [", wrap->new_line_loc.size);
    // for (uint32_t i = 0; i < wrap->new_line_loc.size; i++) {
        // fprintf(stderr, "%u", wrap->new_line_loc.contents[i]);
        // if (i + 1 < wrap->new_line_loc.size) fprintf(stderr, ", ");
    // }
    // fprintf(stderr, "]\n}\n");
}


void *tree_sitter_quarto_external_scanner_create() {
  fprintf(stderr, "attempting to create scanner... ");
  ScannerState *state = (ScannerState *)malloc(sizeof(ScannerState));
  state->pos = new_position(0, 0);
  array_init(&state->results); // Initialize the state
  fprintf(stderr, "returning scanner\n");
  return state;
}

void tree_sitter_quarto_external_scanner_destroy(void *payload) {
  fprintf(stderr, "attempting to destroy scanner... ");
  ScannerState *state = (ScannerState *)payload;
  array_delete(&state->results); // Free the heap memory used by the array
  free(payload); // Free the allocated state
  fprintf(stderr, "freeing memory and exiting\n");
}

unsigned tree_sitter_quarto_external_scanner_serialize(void *payload, char *buffer) {
  fprintf(stderr, "attempting to serialize scanner... ");
  ScannerState *state = (ScannerState *)payload;
  size_t offset = 0;
  // get the position
  memcpy(buffer + offset, &state->pos.row, sizeof(uint32_t));
  offset += sizeof(uint32_t);
  memcpy(buffer + offset, &state->pos.col, sizeof(uint32_t));
  offset += sizeof(uint32_t);
  // Serialize results array size
  memcpy(buffer + offset, &state->results.size, sizeof(uint32_t));
  offset += sizeof(uint32_t);

  // Serialize each ParseResult
  for (uint32_t i = 0; i < state->results.size; i++) {
      ParseResult *res = &state->results.contents[i];
      memcpy(buffer + offset, res, sizeof(ParseResult));
      offset += sizeof(ParseResult);
  }
  fprintf(stderr, "%zu bytes written... \n", offset);
  return offset;
}

void tree_sitter_quarto_external_scanner_deserialize(void *payload, const char *buffer, unsigned length) {
    fprintf(stderr, "attempting to deserialize scanner... \n");
    if (!payload || !buffer) {
        fprintf(stderr, "Null pointer in deserialize!\n");
        return;
    }
    if (length < sizeof(uint32_t)) {
        fprintf(stderr, "Buffer too small in deserialize!\n");
        return;
    }
    ScannerState *state = (ScannerState *)payload;
    size_t offset = 0;

    fprintf(stderr, "writing row bits... ");
    memcpy(&state->pos.row, buffer + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    fprintf(stderr, "writing col bits... ");
    memcpy(&state->pos.col, buffer + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    fprintf(stderr, "writing array size bits... ");
    // Deserialize results array size
    uint32_t arr_size = 0;
    memcpy(&arr_size, buffer + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    fprintf(stderr, "reserving array size... ");
    array_clear(&state->results);
    array_reserve(&state->results, arr_size);
    state->results.size = arr_size;

    fprintf(stderr, "attempting to pull buffer info of %i elements... ", arr_size);
    // Deserialize each ParseResult
    for (uint32_t i = 0; i < arr_size; i++) {
      memcpy(&state->results.contents[i], buffer + offset, sizeof(ParseResult));
      offset += sizeof(ParseResult);
    }
    fprintf(stderr, "exiting from deserializing function... \n");
}




static int32_t other_emphasis(int32_t char_) {
    if (char_=='*') {
        return '_';
    }
    return '*';
}

static void print_valid_symbols(const bool *valid_symbols) {
    // fprintf(stderr, "valid_symbols: [");
    // fprintf(stderr, "LINE_START=%d, ", valid_symbols[LINE_START]);
    // fprintf(stderr, "LINE_END=%d, ", valid_symbols[LINE_END]);
    // fprintf(stderr, "EMPHASIS_STAR_START=%d, ", valid_symbols[EMPHASIS_STAR_START]);
    // fprintf(stderr, "EMPHASIS_STAR_END=%d, ", valid_symbols[EMPHASIS_STAR_END]);
    // fprintf(stderr, "EMPHASIS_UNDER_START=%d, ", valid_symbols[EMPHASIS_UNDER_START]);
    // fprintf(stderr, "EMPHASIS_UNDER_END=%d, ", valid_symbols[EMPHASIS_UNDER_END]);
    // fprintf(stderr, "STRONG_STAR_START=%d, ", valid_symbols[STRONG_STAR_START]);
    // fprintf(stderr, "STRONG_STAR_END=%d, ", valid_symbols[STRONG_STAR_END]);
    // fprintf(stderr, "STRONG_UNDER_START=%d, ", valid_symbols[STRONG_UNDER_START]);
    // fprintf(stderr, "STRONG_UNDER_END=%d, ", valid_symbols[STRONG_UNDER_END]);
    // fprintf(stderr, "NO_PARSE=%d, ", valid_symbols[NO_PARSE]);
    // fprintf(stderr, "ERROR=%d", valid_symbols[ERROR]);
    // fprintf(stderr, "]\n");
}

/// called after a new line is detected and the next symbol is not a new_line
/// This will preparse the next line so that we can accurately identify end position
/// marks when the lexer finially reaches that position.
///
/// This function should continue parsing  until it reaches a new line character.
/// if some internal parse occurs in which we pass a new line, that is fine
///
static void parse_new_line(ScannerState *state, TSLexer *lexer) {
    // fprintf(stderr, "- calling: parse_new_line()\n");
    // the position of the state should ALWAYS be correct when this
    // function is called.
    LexWrap wrapper = new_lexer(lexer, state->pos);
    int32_t lookahead = lex_lookahead(&wrapper);
    // int8_t indent_size = 0;
    Pos pos = new_position(0, 0);
    while(lookahead == ' ' || lookahead == '\t') {
        if (lookahead == ' ') {
            // indent_size++;
        } else {
            // indent_size += 2;
        }
        lex_advance(&wrapper, false);
        lookahead = lex_lookahead(&wrapper);
    }
    if (lookahead=='\n') {
        return;
    }
    // decide what to do with the first symbol
    // mostely for items that could expand into other syntatic elements
    // i.e.
    // - a list item could be a number of characters.
    // - a block quote however is easy to identify
    // - a table may require a bit more parsing
    // - a div :::
    // - some code block
    // - a line block
    switch (lookahead) {
        case '*': {
            // this could be a list item, or
            // just inline syntax
        }
        default: {

        }
    }
    while(lookahead != '\0') {
        switch (lookahead) {
            case '\n': {
                // fprintf(stderr, "new-line is next... ending parse_new_line()\n");
                return;
            }
            case '\\': {
                lex_advance(&wrapper, false);
                if (lex_lookahead(&wrapper) == '\n') {
                    return;
                }
                break;
            }
            default: {

                if (is_inline_synatx(lookahead)) {
                    // fprintf(stderr, "about to parse inline: ");
                    debug_pos(&wrapper.curr_pos);
                    // fprintf(stderr, "\n");
                    ParseResult attempt = parse_inline(&wrapper, &state->results);
                    if (attempt.success) {
                        lex_backtrack_n(&wrapper, 1);
                    }
                }
            }
        }
        lex_advance(&wrapper, false);
        pos = wrapper.curr_pos;
        size_t index = stack_find(&state->results, &pos, DO_NOT_PARSE, false);
        if (index < not_found) {
            ParseResult *no_parse = &state->results.contents[index];
            for (uint32_t i = 0; i < no_parse->length; i++) {
                lex_advance(&wrapper, false);
            }
        }
        lookahead = lex_lookahead(&wrapper);


    }


}

bool tree_sitter_quarto_external_scanner_scan(void *payload, TSLexer *lexer, const bool *valid_symbols) {


  ScannerState *state = (ScannerState *)payload;
  print_scanner_state(state);
  // fprintf(stderr, "scanner invoked before: %c - is alpha: %i\n",
      // lexer->lookahead, isalnum((int)lexer->lookahead));
  print_valid_symbols(valid_symbols);
  if (valid_symbols[ERROR]) {
      // fprintf(stderr, "ERROR is a valid symbol. do not handle\n");
      // lexer->mark_end(lexer);
      // lexer->result_symbol = ERROR;
      return false;
  }


  if (valid_symbols[LINE_START] && state->pos.col == 0 &&
      lexer->lookahead != '\n' && lexer->lookahead != '\0') {
      // fprintf(stderr, "possible line start\n");
      lexer->mark_end(lexer);
      lexer->result_symbol = LINE_START;
      parse_new_line(state, lexer);
      return true;
  }

  // Skip whitespace
  bool skipped_whitespace = false;
  while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
    skipped_whitespace = true;
    lexer->advance(lexer, true);
  }

  // fprintf(stderr, "scanner invoked... next char %c\n", lexer->lookahead);
  // Detect a newline
  if (lexer->lookahead == '\n' && valid_symbols[LINE_END]) {
    state->pos.row++;
    state->pos.col = 0;
    lexer->advance(lexer, false); // Consume the newline
    lexer->result_symbol = LINE_END; // Emit the LINE_END token
    lexer->mark_end(lexer);
    return true;
  }

  // handle NO_PARSE -
  // this symbol can occur anywhere, and if it
  // appears it means that this section was already
  // pre-parsed and willl show up literally.
  if (valid_symbols[NO_PARSE]) {
      state->pos.col = lexer->get_column(lexer);
      size_t index = stack_find(&state->results, &state->pos, DO_NOT_PARSE, false);
      if (index < not_found) {
          ParseResult *res = &state->results.contents[index];
          for (uint32_t i = 0; i < res->length; i++) {
              lexer->advance(lexer, false);
          }
          lexer->mark_end(lexer);
          lexer->result_symbol = NO_PARSE;
          array_erase(&state->results, index);
          return true;
      }

  } else {
      // just check if this is something we should skip
      state->pos.col = lexer->get_column(lexer);
      size_t index = stack_find(&state->results, &state->pos, DO_NOT_PARSE, false);
      if (index < not_found) {
          // ParseResult *res = &state->results.contents[index];
          return false;
      }
  }

  // detect  star
  if (lexer->lookahead == '*' && (
      valid_symbols[EMPHASIS_STAR_START] ||
      valid_symbols[STRONG_STAR_START] ||
      valid_symbols[EMPHASIS_STAR_END] ||
      valid_symbols[STRONG_STAR_END]
  )) {
      // fprintf(stderr, "looking for strong or emph star\n");
      // get current start position
      state->pos.col = lexer->get_column(lexer);
      LexWrap wrapper = new_lexer(lexer, state->pos);
      lex_advance(&wrapper, false);
      // possible end if just an emphasis
      lexer->mark_end(lexer);
      // before we move the lexer forward check
      // if emphasis is valid... The grammar could
      // enable STRONG_STAR_END and EMPH_STAR_END
      // at the same time...
      Pos possible_pos = wrapper.curr_pos;
      // fprintf(stderr, "lex is at: ");
      print_pos(&possible_pos);
      // fprintf(stderr, "\n");
      if (valid_symbols[EMPHASIS_STAR_END]) {
          size_t index = stack_find(&state->results, &possible_pos, EMPHASIS_STAR, true);
          if (index < not_found) {
              lexer->result_symbol = EMPHASIS_STAR_END;
              array_erase(&state->results, index);
              return true;
          }
      }

      if (valid_symbols[EMPHASIS_STAR_START]) {
          // the start position should be one step prior
          possible_pos.col--;
          size_t index = stack_find(&state->results, &possible_pos, EMPHASIS_STAR, false);
          if (index < not_found) {
              lexer->result_symbol = EMPHASIS_STAR_START;
              return true;
          }
          possible_pos.col++;
      }

      // without actually advancing the lexer, check the stack
      if (valid_symbols[STRONG_STAR_START] || valid_symbols[STRONG_STAR_END]) {
          possible_pos.col++;
          if (valid_symbols[STRONG_STAR_END]) {
              size_t index = stack_find(&state->results, &possible_pos, STRONG_STAR, true);
              if (index < not_found) {
                  lex_advance(&wrapper, false);
                  lexer->mark_end(lexer);
                  lexer->result_symbol = STRONG_STAR_END;
                  array_erase(&state->results, index);
                  return true;
              }
          }
          if (valid_symbols[STRONG_STAR_START]) {
              //again, the start will be on the other side
              possible_pos.col -= 2;
              size_t index = stack_find(&state->results, &possible_pos, STRONG_STAR, false);
              if (index < not_found) {
                  lex_advance(&wrapper, false);
                  lexer->mark_end(lexer);
                  lexer->result_symbol = STRONG_STAR_START;
                  return true;
              }
              possible_pos.col += 2;
          }
      }

      // failed to match any pre-parsed info on the stack.
      // Its not the time to advance the lexer if STRONG match is possible.
      if (valid_symbols[EMPHASIS_STAR_START] || valid_symbols[STRONG_STAR_START]) {

          if (lexer->lookahead == '*' && valid_symbols[STRONG_STAR_START]) {
              lex_advance(&wrapper, false);
              lexer->mark_end(lexer);
          }
          // reset wrapper to begining of this scan.
          lex_backtrack_n(&wrapper, wrapper.buffer.size);
          // try and handle this parse...
          ParseResult res = parse_star(&wrapper, &state->results);
          if (res.success) {
              if (res.token == NONE) {
                  lexer->result_symbol = ERROR;
                  return true;
              }
              if (res.token == DO_NOT_PARSE) {
                  size_t index = stack_find_exact(&state->results, &res);
                  if (index < not_found) {
                      array_erase(&state->results, index);
                  }
                  lexer->result_symbol = NO_PARSE;
                  return true;
              }
              if (valid_symbols[EMPHASIS_STAR_START] && res.token == EMPHASIS_STAR) {
                  lexer->result_symbol = EMPHASIS_STAR_START;
                  return true;
              } else if (valid_symbols[STRONG_STAR_START] && res.token == STRONG_STAR){
                  lexer->result_symbol = STRONG_STAR_START;
                  return true;
              }
          }
      }

  }


  // detect  underscore
  if (lexer->lookahead == '_' && (
      valid_symbols[EMPHASIS_UNDER_START] ||
      valid_symbols[STRONG_UNDER_START] ||
      valid_symbols[EMPHASIS_UNDER_END] ||
      valid_symbols[STRONG_UNDER_END]
  )) {
      // fprintf(stderr, "looking for strong or emph under\n");
      // get current start position
      state->pos.col = lexer->get_column(lexer);
      LexWrap wrapper = new_lexer(lexer, state->pos);
      lex_advance(&wrapper, false);
      // possible end if just an emphasis
      lexer->mark_end(lexer);
      // before we move the lexer forward check
      // if emphasis is valid... The grammar could
      // enable STRONG_STAR_END and EMPH_STAR_END
      // at the same time...
      Pos possible_pos = wrapper.curr_pos;
      // fprintf(stderr, "lex is at: ");
      print_pos(&possible_pos);
      // fprintf(stderr, "\n");
      if (valid_symbols[EMPHASIS_UNDER_END]) {
          size_t index = stack_find(&state->results, &possible_pos, EMPHASIS_UNDER, true);
          if (index < not_found) {
              lexer->result_symbol = EMPHASIS_UNDER_END;
              array_erase(&state->results, index);
              return true;
          }
      }

      if (valid_symbols[EMPHASIS_UNDER_START]) {
          // the start position should be one step prior
          possible_pos.col--;
          size_t index = stack_find(&state->results, &possible_pos, EMPHASIS_UNDER, false);
          if (index < not_found) {
              lexer->result_symbol = EMPHASIS_UNDER_START;
              return true;
          }
          possible_pos.col++;
      }

      // without actually advancing the lexer, check the stack
      if (valid_symbols[STRONG_UNDER_START] || valid_symbols[STRONG_UNDER_END]) {
          possible_pos.col++;
          if (valid_symbols[STRONG_UNDER_END]) {
              size_t index = stack_find(&state->results, &possible_pos, STRONG_UNDER, true);
              if (index < not_found) {
                  lex_advance(&wrapper, false);
                  lexer->mark_end(lexer);
                  lexer->result_symbol = STRONG_UNDER_END;
                  array_erase(&state->results, index);
                  return true;
              }
          }
          if (valid_symbols[STRONG_UNDER_START]) {
              //again, the start will be on the other side
              possible_pos.col -= 2;
              size_t index = stack_find(&state->results, &possible_pos, STRONG_UNDER, false);
              if (index < not_found) {
                  lex_advance(&wrapper, false);
                  lexer->mark_end(lexer);
                  lexer->result_symbol = STRONG_UNDER_START;
                  return true;
              }
              possible_pos.col += 2;
          }
      }

      // failed to match any pre-parsed info on the stack.
      // Its not the time to advance the lexer if STRONG match is possible.
      if (valid_symbols[EMPHASIS_UNDER_START] || valid_symbols[STRONG_UNDER_START]) {

          if (lexer->lookahead == '_' && valid_symbols[STRONG_UNDER_START]) {
              lex_advance(&wrapper, false);
              // however, only mark end here if the next symbol is NOT
              // an '_'. This is because a stream of ___ implies the first
              // character is part of an emphasis
              if (lexer->lookahead != '_') {
                  lexer->mark_end(lexer);
              }
          }
          // reset wrapper to begining of this scan.
          lex_backtrack_n(&wrapper, wrapper.buffer.size);
          // try and handle this parse...
          ParseResult res = parse_under(&wrapper, &state->results);
          if (res.success) {
              if (res.token == NONE) {
                  lexer->result_symbol = ERROR;
                  return true;
              }
              if (res.token == DO_NOT_PARSE) {
                  size_t index = stack_find_exact(&state->results, &res);
                  if (index < not_found) {
                      array_erase(&state->results, index);
                  }
                  lexer->result_symbol = NO_PARSE;
                  return true;
              }
              if (valid_symbols[EMPHASIS_UNDER_START] && res.token == EMPHASIS_UNDER) {
                  lexer->result_symbol = EMPHASIS_UNDER_START;
                  return true;
              } else if (valid_symbols[STRONG_UNDER_START] && res.token == STRONG_UNDER){
                  lexer->result_symbol = STRONG_UNDER_START;
                  return true;
              }
          }
      }

  }

  return false; // No token recognized
}
