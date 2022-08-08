#ifndef YARP_H
#define YARP_H

#include <ruby.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef enum {
  YP_TOKEN_EOF = 0,                   // final token in the file
  YP_TOKEN_INVALID,                   // an invalid token
  YP_TOKEN_AMPERSAND,                 // &
  YP_TOKEN_AMPERSAND_AMPERSAND,       // &&
  YP_TOKEN_AMPERSAND_AMPERSAND_EQUAL, // &&=
  YP_TOKEN_AMPERSAND_EQUAL,           // &=
  YP_TOKEN_BACK_REFERENCE,            // a back reference
  YP_TOKEN_BACKTICK,                  // `
  YP_TOKEN_BANG,                      // !
  YP_TOKEN_BANG_AT,                   // !@
  YP_TOKEN_BANG_EQUAL,                // !=
  YP_TOKEN_BANG_TILDE,                // !~
  YP_TOKEN_BRACE_LEFT,                // {
  YP_TOKEN_BRACE_RIGHT,               // }
  YP_TOKEN_BRACKET_LEFT,              // [
  YP_TOKEN_BRACKET_LEFT_RIGHT,        // []
  YP_TOKEN_BRACKET_RIGHT,             // ]
  YP_TOKEN_CARET,                     // ^
  YP_TOKEN_CARET_EQUAL,               // ^=
  YP_TOKEN_CHARACTER_LITERAL,         // a character literal
  YP_TOKEN_CLASS_VARIABLE,            // a class variable
  YP_TOKEN_COLON,                     // :
  YP_TOKEN_COLON_COLON,               // ::
  YP_TOKEN_COMMA,                     // ,
  YP_TOKEN_COMMENT,                   // a comment
  YP_TOKEN_CONSTANT,                  // a constant
  YP_TOKEN_DOT,                       // .
  YP_TOKEN_DOT_DOT,                   // ..
  YP_TOKEN_DOT_DOT_DOT,               // ...
  YP_TOKEN_EMBDOC_BEGIN,              // =begin
  YP_TOKEN_EMBDOC_END,                // =end
  YP_TOKEN_EMBDOC_LINE,               // a line inside of embedded documentation
  YP_TOKEN_EMBEXPR_BEGIN,             // #{
  YP_TOKEN_EMBEXPR_END,               // }
  YP_TOKEN_EQUAL,                     // =
  YP_TOKEN_EQUAL_EQUAL,               // ==
  YP_TOKEN_EQUAL_EQUAL_EQUAL,         // ===
  YP_TOKEN_EQUAL_GREATER,             // =>
  YP_TOKEN_EQUAL_TILDE,               // =~
  YP_TOKEN_FLOAT,                     // a floating point number
  YP_TOKEN_GREATER,                   // >
  YP_TOKEN_GREATER_EQUAL,             // >=
  YP_TOKEN_GREATER_GREATER,           // >>
  YP_TOKEN_GREATER_GREATER_EQUAL,     // >>=
  YP_TOKEN_GLOBAL_VARIABLE,           // a global variable
  YP_TOKEN_IDENTIFIER,                // an identifier
  YP_TOKEN_IMAGINARY_NUMBER,          // an imaginary number literal
  YP_TOKEN_INSTANCE_VARIABLE,         // an instance variable
  YP_TOKEN_INTEGER,                   // an integer (any base)
  YP_TOKEN_KEYWORD___ENCODING__,      // __ENCODING__
  YP_TOKEN_KEYWORD___LINE__,          // __LINE__
  YP_TOKEN_KEYWORD___FILE__,          // __FILE__
  YP_TOKEN_KEYWORD_ALIAS,             // alias
  YP_TOKEN_KEYWORD_AND,               // and
  YP_TOKEN_KEYWORD_BEGIN,             // begin
  YP_TOKEN_KEYWORD_BEGIN_UPCASE,      // BEGIN
  YP_TOKEN_KEYWORD_BREAK,             // break
  YP_TOKEN_KEYWORD_CASE,              // case
  YP_TOKEN_KEYWORD_CLASS,             // class
  YP_TOKEN_KEYWORD_DEF,               // def
  YP_TOKEN_KEYWORD_DEFINED,           // defined?
  YP_TOKEN_KEYWORD_DO,                // do
  YP_TOKEN_KEYWORD_ELSE,              // else
  YP_TOKEN_KEYWORD_ELSIF,             // elsif
  YP_TOKEN_KEYWORD_END,               // end
  YP_TOKEN_KEYWORD_END_UPCASE,        // END
  YP_TOKEN_KEYWORD_ENSURE,            // ensure
  YP_TOKEN_KEYWORD_FALSE,             // false
  YP_TOKEN_KEYWORD_FOR,               // for
  YP_TOKEN_KEYWORD_IF,                // if
  YP_TOKEN_KEYWORD_IN,                // in
  YP_TOKEN_KEYWORD_MODULE,            // module
  YP_TOKEN_KEYWORD_NEXT,              // next
  YP_TOKEN_KEYWORD_NIL,               // nil
  YP_TOKEN_KEYWORD_NOT,               // not
  YP_TOKEN_KEYWORD_OR,                // or
  YP_TOKEN_KEYWORD_REDO,              // redo
  YP_TOKEN_KEYWORD_RESCUE,            // rescue
  YP_TOKEN_KEYWORD_RETRY,             // retry
  YP_TOKEN_KEYWORD_RETURN,            // return
  YP_TOKEN_KEYWORD_SELF,              // self
  YP_TOKEN_KEYWORD_SUPER,             // super
  YP_TOKEN_KEYWORD_THEN,              // then
  YP_TOKEN_KEYWORD_TRUE,              // true
  YP_TOKEN_KEYWORD_UNDEF,             // undef
  YP_TOKEN_KEYWORD_UNLESS,            // unless
  YP_TOKEN_KEYWORD_UNTIL,             // until
  YP_TOKEN_KEYWORD_WHEN,              // when
  YP_TOKEN_KEYWORD_WHILE,             // while
  YP_TOKEN_KEYWORD_YIELD,             // yield
  YP_TOKEN_LABEL,                     // a label
  YP_TOKEN_LAMBDA_BEGIN,              // {
  YP_TOKEN_LESS,                      // <
  YP_TOKEN_LESS_EQUAL,                // <=
  YP_TOKEN_LESS_EQUAL_GREATER,        // <=>
  YP_TOKEN_LESS_LESS,                 // <<
  YP_TOKEN_LESS_LESS_EQUAL,           // <<=
  YP_TOKEN_MINUS,                     // -
  YP_TOKEN_MINUS_AT,                  // -@
  YP_TOKEN_MINUS_EQUAL,               // -=
  YP_TOKEN_MINUS_GREATER,             // ->
  YP_TOKEN_NEWLINE,                   // a newline character outside of other tokens
  YP_TOKEN_NTH_REFERENCE,             // an nth global variable reference
  YP_TOKEN_PARENTHESIS_LEFT,          // (
  YP_TOKEN_PARENTHESIS_RIGHT,         // )
  YP_TOKEN_PERCENT,                   // %
  YP_TOKEN_PERCENT_EQUAL,             // %=
  YP_TOKEN_PERCENT_LOWER_I,           // %i
  YP_TOKEN_PERCENT_LOWER_W,           // %w
  YP_TOKEN_PERCENT_LOWER_X,           // %x
  YP_TOKEN_PERCENT_UPPER_I,           // %I
  YP_TOKEN_PERCENT_UPPER_W,           // %W
  YP_TOKEN_PIPE,                      // |
  YP_TOKEN_PIPE_EQUAL,                // |=
  YP_TOKEN_PIPE_PIPE,                 // ||
  YP_TOKEN_PIPE_PIPE_EQUAL,           // ||=
  YP_TOKEN_PLUS,                      // +
  YP_TOKEN_PLUS_AT,                   // +@
  YP_TOKEN_PLUS_EQUAL,                // +=
  YP_TOKEN_QUESTION_MARK,             // ?
  YP_TOKEN_RATIONAL_NUMBER,           // a rational number literal
  YP_TOKEN_REGEXP_BEGIN,              // the beginning of a regular expression
  YP_TOKEN_REGEXP_END,                // the end of a regular expression
  YP_TOKEN_SEMICOLON,                 // ;
  YP_TOKEN_SLASH,                     // /
  YP_TOKEN_SLASH_EQUAL,               // /=
  YP_TOKEN_STAR,                      // *
  YP_TOKEN_STAR_EQUAL,                // *=
  YP_TOKEN_STAR_STAR,                 // **
  YP_TOKEN_STAR_STAR_EQUAL,           // **=
  YP_TOKEN_STRING_BEGIN,              // the beginning of a string
  YP_TOKEN_STRING_CONTENT,            // the contents of a string
  YP_TOKEN_STRING_END,                // the end of a string
  YP_TOKEN_SYMBOL_BEGIN,              // the beginning of a symbol
  YP_TOKEN_TILDE,                     // ~
  YP_TOKEN_TILDE_AT,                  // ~@
  YP_TOKEN_WORDS_SEP,                 // a separator between words in a list
} yp_token_type_t;

// This struct represents a token in the Ruby source. We use it to track both
// type and location information.
typedef struct {
  yp_token_type_t type;
  const char *start;
  const char *end;
} yp_token_t;

// When lexing Ruby source, the lexer has a small amount of state to tell which
// kind of token it is currently lexing. For example, when we find the start of
// a string, the first token that we return is a TOKEN_STRING_BEGIN token. After
// that the lexer is now in the YP_LEX_STRING mode, and will return tokens that
// are found as part of a string.
typedef struct yp_lex_mode {
  enum {
    // This state is used when any given token is being lexed.
    YP_LEX_DEFAULT,

    // This state is used when we're lexing an embdoc (a =begin..=end comment).
    YP_LEX_EMBDOC,

    // This state is used when we're lexing as normal but inside an embedded
    // expression of a string.
    YP_LEX_EMBEXPR,

    // This state is used when we are lexing a list of tokens, as in a %w word
    // list literal or a %i symbol list literal.
    YP_LEX_LIST,

    // This state is used when a regular expression has been begun and we are
    // looking for the terminator.
    YP_LEX_REGEXP,

    // This state is used when we are lexing a string or a string-like token, as
    // in string content with either quote or an xstring.
    YP_LEX_STRING,

    // This state is used when a symbol has already been begun (e.g., by a
    // colon) and we still need to lex the rest of the symbol.
    YP_LEX_SYMBOL,
  } mode;

  // This is the terminator of the current state. It is used when lexing a
  // string (either single or double quoted) and an xstring.
  char term;

  // Whether or not interpolation is allowed in this lex state. This corresponds
  // to some LEX_LIST states (e.g., %W) and LEX_STRING states (e.g., double
  // quotes).
  bool interp;

  // The previous lex state so that it knows how to pop.
  struct yp_lex_mode *prev;
} yp_lex_mode_t;

// We pre-allocate a certain number of lex states in order to avoid having to
// call malloc too many times while parsing. You really shouldn't need more than
// this because you only really nest deeply when doing string interpolation.
#define YP_LEX_STACK_SIZE 4

// A forward declaration since our error handler struct accepts a parser for
// each of its function calls.
typedef struct yp_parser yp_parser_t;

// This struct is for handling error recovery. We're going to provide our own
// implementation for default, but this is an extension point if folks want to
// provide their own.
//
// Each function is going to be provided with a pointer to the struct itself, at
// which point it is expected to set the parsers state to whatever it should be
// in order to recover from the error. If it can't recover, it should return
// TOKEN_INVALID.
typedef struct {
  yp_token_type_t (*unterminated_embdoc)(yp_parser_t *parser);
  yp_token_type_t (*unterminated_list)(yp_parser_t *parser);
  yp_token_type_t (*unterminated_regexp)(yp_parser_t *parser);
  yp_token_type_t (*unterminated_string)(yp_parser_t *parser);
} yp_error_handler_t;

// This struct represents the overall parser. It contains a reference to the
// source file, as well as pointers that indicate where in the source it's
// currently parsing. It also contains the most recent and current token that
// it's considering.
struct yp_parser {
  struct {
    yp_lex_mode_t *current; // the current state of the lexer
    yp_lex_mode_t stack[YP_LEX_STACK_SIZE]; // the stack of lexer states
    size_t index;            // the current index into the lexer state stack
  } lex_modes;

  const char *start;         // the pointer to the start of the source
  const char *end;           // the pointer to the end of the source
  yp_token_t previous;       // the previous token we were considering
  yp_token_t current;        // the current token we're considering
  int lineno;                // the current line number we're looking at

  yp_error_handler_t *error_handler; // the error handler
};

// Initialize a parser with the given start and end pointers.
void
yp_parser_init(yp_parser_t *parser, const char *source, off_t size, yp_error_handler_t *error_handler);

// Get the next token type and set its value on the current pointer.
void
yp_lex_token(yp_parser_t *parser);

#endif