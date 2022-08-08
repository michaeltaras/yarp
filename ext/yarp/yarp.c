#include "yarp.h"

/******************************************************************************/
/* Basic character checks                                                     */
/******************************************************************************/

static inline bool
is_binary_number_char(const char *c) {
  return *c == '0' || *c == '1';
}

static inline bool
is_octal_number_char(const char *c) {
  return *c >= '0' && *c <= '7';
}

static inline bool
is_decimal_number_char(const char *c) {
  return *c >= '0' && *c <= '9';
}

static inline bool
is_hexadecimal_number_char(const char *c) {
  return (*c >= '0' && *c <= '9') || (*c >= 'a' && *c <= 'f') || (*c >= 'A' && *c <= 'F');
}

static inline bool
is_identifier_start_char(const char *c) {
  return (*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c == '_');
}

static inline bool
is_identifier_char(const char *c) {
  return is_identifier_start_char(c) || is_decimal_number_char(c);
}

static inline bool
is_non_newline_whitespace_char(const char *c) {
  return *c == ' ' || *c == '\t' || *c == '\f' || *c == '\r' || *c == '\v';
}

static inline bool
is_whitespace_char(const char *c) {
  return is_non_newline_whitespace_char(c) || *c == '\n';
}

/******************************************************************************/
/* Lexer check helpers                                                        */
/******************************************************************************/

// If the character to be read matches the given value, then returns true and
// advanced the current pointer.
static inline bool
match(yp_parser_t *parser, char value) {
  if (*parser->current.end == value) {
    parser->current.end++;
    return true;
  }
  return false;
}

// Returns the matching character that should be used to terminate a list
// beginning with the given character.
static char
terminator(const char start) {
  switch (start) {
    case '(': return ')';
    case '[': return ']';
    case '{': return '}';
    case '<': return '>';
    default: return start;
  }
}

/******************************************************************************/
/* Lex mode manipulations                                                     */
/******************************************************************************/

// Push a new lex state onto the stack. If we're still within the pre-allocated
// space of the lex state stack, then we'll just use a new slot. Otherwise we'll
// allocate a new pointer and use that.
static void
push_lex_mode(yp_parser_t *parser, yp_lex_mode_t lex_mode) {
  lex_mode.prev = parser->lex_modes.current;
  parser->lex_modes.index++;

  if (parser->lex_modes.index > YP_LEX_STACK_SIZE - 1) {
    parser->lex_modes.current = (yp_lex_mode_t *) malloc(sizeof(yp_lex_mode_t));
  } else {
    parser->lex_modes.stack[parser->lex_modes.index] = lex_mode;
    parser->lex_modes.current = &parser->lex_modes.stack[parser->lex_modes.index];
  }
}

// Pop the current lex state off the stack. If we're within the pre-allocated
// space of the lex state stack, then we'll just decrement the index. Otherwise
// we'll free the current pointer and use the previous pointer.
static void
pop_lex_mode(yp_parser_t *parser) {
  if (parser->lex_modes.index == 0) {
    parser->lex_modes.current->mode = YP_LEX_DEFAULT;
  } else if (parser->lex_modes.index < YP_LEX_STACK_SIZE) {
    parser->lex_modes.index--;
    parser->lex_modes.current = &parser->lex_modes.stack[parser->lex_modes.index];
  } else {
    parser->lex_modes.index--;
    yp_lex_mode_t *prev = parser->lex_modes.current->prev;
    free(parser->lex_modes.current);
    parser->lex_modes.current = prev;
  }
}

/******************************************************************************/
/* Specific token lexers                                                      */
/******************************************************************************/

static yp_token_type_t
lex_optional_float_suffix(yp_parser_t *parser) {
  yp_token_type_t type = YP_TOKEN_INTEGER;

  // Here we're going to attempt to parse the optional decimal portion of a
  // float. If it's not there, then it's okay and we'll just continue on.
  if (*parser->current.end == '.') {
    if ((parser->current.end + 1 < parser->end) && is_decimal_number_char(parser->current.end + 1)) {
      parser->current.end += 2;
      while (is_decimal_number_char(parser->current.end)) {
        parser->current.end++;
        match(parser, '_');
      }

      type = YP_TOKEN_FLOAT;
    } else {
      // If we had a . and then something else, then it's not a float suffix on
      // a number it's a method call or something else.
      return type;
    }
  }

  // Here we're going to attempt to parse the optional exponent portion of a
  // float. If it's not there, it's okay and we'll just continue on.
  if (match(parser, 'e') || match(parser, 'E')) {
    (void) (match(parser, '+') || match(parser, '-'));

    if (is_decimal_number_char(parser->current.end)) {
      parser->current.end++;
      while (is_decimal_number_char(parser->current.end)) {
        parser->current.end++;
        match(parser, '_');
      }

      type = YP_TOKEN_FLOAT;
    } else {
      return YP_TOKEN_INVALID;
    }
  }

  return type;
}

static yp_token_type_t
lex_numeric_prefix(yp_parser_t *parser) {
  yp_token_type_t type = YP_TOKEN_INTEGER;

  if (parser->current.end[-1] == '0') {
    switch (*parser->current.end) {
      // 0d1111 is a decimal number
      case 'd': case 'D':
        if (!is_decimal_number_char(++parser->current.end)) return YP_TOKEN_INVALID;
        while (is_decimal_number_char(parser->current.end)) {
          parser->current.end++;
          match(parser, '_');
        }
        break;

      // 0b1111 is a binary number
      case 'b': case 'B':
        if (!is_binary_number_char(++parser->current.end)) return YP_TOKEN_INVALID;
        while (is_binary_number_char(parser->current.end)) {
          parser->current.end++;
          match(parser, '_');
        }
        break;

      // 0o1111 is an octal number
      case 'o': case 'O':
        if (!is_octal_number_char(++parser->current.end)) return YP_TOKEN_INVALID;
        // fall through

      // 01111 is an octal number
      case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7':
        while (is_octal_number_char(parser->current.end)) {
          parser->current.end++;
          match(parser, '_');
        }
        break;

      // 0x1111 is a hexadecimal number
      case 'x': case 'X':
        if (!is_hexadecimal_number_char(++parser->current.end)) return YP_TOKEN_INVALID;
        while (is_hexadecimal_number_char(parser->current.end)) {
          parser->current.end++;
          match(parser, '_');
        }
        break;

      // 0.xxx is a float
      case '.': {
        type = lex_optional_float_suffix(parser);
        break;
      }

      // 0exxx is a float
      case 'e': case 'E': {
        type = lex_optional_float_suffix(parser);
        break;
      }
    }
  } else {
    // If it didn't start with a 0, then we'll lex as far as we can into a
    // decimal number.
    while (is_decimal_number_char(parser->current.end)) {
      parser->current.end++;
      match(parser, '_');
    }

    // Afterward, we'll lex as far as we can into an optional float suffix.
    type = lex_optional_float_suffix(parser);
  }

  // If the last character that we consumed was an underscore, then this is
  // actually an invalid integer value, and we should return an invalid token.
  if (parser->current.end[-1] == '_') return YP_TOKEN_INVALID;
  return type;
}

static yp_token_type_t
lex_numeric(yp_parser_t *parser) {
  yp_token_type_t type = lex_numeric_prefix(parser);

  if (type != YP_TOKEN_INVALID) {
    if (match(parser, 'r')) type = YP_TOKEN_RATIONAL_NUMBER;
    if (match(parser, 'i')) type = YP_TOKEN_IMAGINARY_NUMBER;
  }

  return type;
}

static yp_token_type_t
lex_global_variable(yp_parser_t *parser) {
  switch (*parser->current.end) {
    case '~':   // $~: match-data
    case '*':   // $*: argv
    case '$':   // $$: pid
    case '?':   // $?: last status
    case '!':   // $!: error string
    case '@':   // $@: error position
    case '/':   // $/: input record separator
    case '\\':  // $\: output record separator
    case ';':   // $;: field separator
    case ',':   // $,: output field separator
    case '.':   // $.: last read line number
    case '=':   // $=: ignorecase
    case ':':   // $:: load path
    case '<':   // $<: reading filename
    case '>':   // $>: default output handle
    case '\"':  // $": already loaded files
      parser->current.end++;
      return YP_TOKEN_GLOBAL_VARIABLE;

    case '&':   // $&: last match
    case '`':   // $`: string before last match
    case '\'':  // $': string after last match
    case '+':   // $+: string matches last paren.
      parser->current.end++;
      return YP_TOKEN_BACK_REFERENCE;
  
    case '1': case '2': case '3': case '4': case '5':
    case '6': case '7': case '8': case '9':
      do { parser->current.end++; } while (is_decimal_number_char(parser->current.end));
      return YP_TOKEN_NTH_REFERENCE;

    default:
      if (is_identifier_char(parser->current.end)) {
        do { parser->current.end++; } while (is_identifier_char(parser->current.end));
        return YP_TOKEN_GLOBAL_VARIABLE;
      }

      // If we get here, then we have a $ followed by something that isn't
      // recognized as a global variable.
      return YP_TOKEN_INVALID;
  }
}

static yp_token_type_t
lex_identifier(yp_parser_t *parser) {
  // Lex as far as we can into the current identifier.
  while (is_identifier_char(parser->current.end)) {
    parser->current.end++;
  }

  off_t width = parser->current.end - parser->current.start;

#define KEYWORD(value, size, token) if (width == size && strncmp(parser->current.start, value, size) == 0) return YP_TOKEN_KEYWORD_##token;

  if ((parser->current.end + 1 < parser->end) && (parser->current.end[1] != '=') && (match(parser, '!') || match(parser, '?'))) {
    width++;
    if (parser->previous.type != YP_TOKEN_DOT) {
      KEYWORD("defined?", 8, DEFINED)
    }
    return YP_TOKEN_IDENTIFIER;
  }

  if (parser->previous.type != YP_TOKEN_DOT) {
    KEYWORD("__ENCODING__", 12, __ENCODING__)
    KEYWORD("__LINE__", 8, __LINE__)
    KEYWORD("__FILE__", 8, __FILE__)
    KEYWORD("alias", 5, ALIAS)
    KEYWORD("and", 3, AND)
    KEYWORD("begin", 5, BEGIN)
    KEYWORD("BEGIN", 5, BEGIN_UPCASE)
    KEYWORD("break", 5, BREAK)
    KEYWORD("case", 4, CASE)
    KEYWORD("class", 5, CLASS)
    KEYWORD("def", 3, DEF)
    KEYWORD("do", 2, DO)
    KEYWORD("else", 4, ELSE)
    KEYWORD("elsif", 5, ELSIF)
    KEYWORD("end", 3, END)
    KEYWORD("END", 3, END_UPCASE)
    KEYWORD("ensure", 6, ENSURE)
    KEYWORD("false", 5, FALSE)
    KEYWORD("for", 3, FOR)
    KEYWORD("if", 2, IF)
    KEYWORD("in", 2, IN)
    KEYWORD("module", 6, MODULE)
    KEYWORD("next", 4, NEXT)
    KEYWORD("nil", 3, NIL)
    KEYWORD("not", 3, NOT)
    KEYWORD("or", 2, OR)
    KEYWORD("redo", 4, REDO)
    KEYWORD("rescue", 6, RESCUE)
    KEYWORD("retry", 5, RETRY)
    KEYWORD("return", 6, RETURN)
    KEYWORD("self", 4, SELF)
    KEYWORD("super", 5, SUPER)
    KEYWORD("then", 4, THEN)
    KEYWORD("true", 4, TRUE)
    KEYWORD("undef", 5, UNDEF)
    KEYWORD("unless", 6, UNLESS)
    KEYWORD("until", 5, UNTIL)
    KEYWORD("when", 4, WHEN)
    KEYWORD("while", 5, WHILE)
    KEYWORD("yield", 5, YIELD)
  }

#undef KEYWORD

  char start = parser->current.start[0];
  return start >= 'A' && start <= 'Z' ? YP_TOKEN_CONSTANT : YP_TOKEN_IDENTIFIER;
}

// This is the overall lexer function. It is responsible for advancing both
// parser->current.start and parser->current.end such that they point to the
// beginning and end of the next token. It should return the type of token that
// was found.
static yp_token_type_t
lex_token_type(yp_parser_t *parser) {
  switch (parser->lex_modes.current->mode) {
    case YP_LEX_DEFAULT:
    case YP_LEX_EMBEXPR: {
      // First, we're going to skip past any whitespace at the front of the next
      // token.
      while (is_non_newline_whitespace_char(parser->current.end)) {
        parser->current.end++;
      }

      // Next, we'll set to start of this token to be the current end.
      parser->current.start = parser->current.end;

      // Finally, we'll check the current character to determine the next token.
      switch (*parser->current.end++) {
        case '\0': // NUL or end of script
        case '\004': // ^D
        case '\032': // ^Z
          return YP_TOKEN_EOF;

        case '#': // comments
          while (*parser->current.end != '\n' && *parser->current.end != '\0') {
            parser->current.end++;
          }
          (void) match(parser, '\n');
          return YP_TOKEN_COMMENT;

        case '\n': {
          parser->lineno++;
          return YP_TOKEN_NEWLINE;
        }

        // , ( ) ;
        case ',': return YP_TOKEN_COMMA;
        case '(': return YP_TOKEN_PARENTHESIS_LEFT;
        case ')': return YP_TOKEN_PARENTHESIS_RIGHT;
        case ';': return YP_TOKEN_SEMICOLON;

        // [ []
        case '[':
          if (parser->previous.type == YP_TOKEN_DOT && match(parser, ']')) {
            return YP_TOKEN_BRACKET_LEFT_RIGHT;
          }
          return YP_TOKEN_BRACKET_LEFT;

        // ]
        case ']': return YP_TOKEN_BRACKET_RIGHT;

        // {
        case '{':
          if (parser->previous.type == YP_TOKEN_MINUS_GREATER) return YP_TOKEN_LAMBDA_BEGIN;
          return YP_TOKEN_BRACE_LEFT;

        // }
        case '}':
          if (parser->lex_modes.current->mode == YP_LEX_EMBEXPR) {
            pop_lex_mode(parser);
            return YP_TOKEN_EMBEXPR_END;
          }
          return YP_TOKEN_BRACE_RIGHT;

        // * ** **= *=
        case '*':
          if (match(parser, '*')) return match(parser, '=') ? YP_TOKEN_STAR_STAR_EQUAL : YP_TOKEN_STAR_STAR;
          return match(parser, '=') ? YP_TOKEN_STAR_EQUAL : YP_TOKEN_STAR;

        // ! != !~ !@
        case '!':
          if (match(parser, '=')) return YP_TOKEN_BANG_EQUAL;
          if (match(parser, '~')) return YP_TOKEN_BANG_TILDE;
          if ((parser->previous.type == YP_TOKEN_KEYWORD_DEF || parser->previous.type == YP_TOKEN_DOT) && match(parser, '@')) return YP_TOKEN_BANG_AT;
          return YP_TOKEN_BANG;

        // = => =~ == === =begin
        case '=':
          if (parser->current.end[-2] == '\n' && (strncmp(parser->current.end, "begin\n", 6) == 0)) {
            parser->current.end += 6;
            push_lex_mode(parser, (yp_lex_mode_t) { .mode = YP_LEX_EMBDOC, .term = '\0', .interp = false });
            return YP_TOKEN_EMBDOC_BEGIN;
          }

          if (match(parser, '>')) return YP_TOKEN_EQUAL_GREATER;
          if (match(parser, '~')) return YP_TOKEN_EQUAL_TILDE;
          if (match(parser, '=')) return match(parser, '=') ? YP_TOKEN_EQUAL_EQUAL_EQUAL : YP_TOKEN_EQUAL_EQUAL;
          return YP_TOKEN_EQUAL;

        // < << <<= <= <=>
        case '<':
          if (match(parser, '<')) {
            if (match(parser, '=')) return YP_TOKEN_LESS_LESS_EQUAL;

            // We don't yet handle heredocs.
            if (match(parser, '-') || match(parser, '~')) return YP_TOKEN_EOF;

            return YP_TOKEN_LESS_LESS;
          }
          if (match(parser, '=')) return match(parser, '>') ? YP_TOKEN_LESS_EQUAL_GREATER : YP_TOKEN_LESS_EQUAL;
          return YP_TOKEN_LESS;

        // > >> >>= >=
        case '>':
          if (match(parser, '>')) return match(parser, '=') ? YP_TOKEN_GREATER_GREATER_EQUAL : YP_TOKEN_GREATER_GREATER;
          return match(parser, '=') ? YP_TOKEN_GREATER_EQUAL : YP_TOKEN_GREATER;

        // double-quoted string literal
        case '"':
          push_lex_mode(parser, (yp_lex_mode_t) { .mode = YP_LEX_STRING, .term = '"', .interp = true });
          return YP_TOKEN_STRING_BEGIN;

        // xstring literal
        case '`':
          push_lex_mode(parser, (yp_lex_mode_t) { .mode = YP_LEX_STRING, .term = '`', .interp = true });
          return YP_TOKEN_BACKTICK;

        // single-quoted string literal
        case '\'':
          push_lex_mode(parser, (yp_lex_mode_t) { .mode = YP_LEX_STRING, .term = '\'', .interp = false });
          return YP_TOKEN_STRING_BEGIN;

        // ? character literal
        case '?':
          if (is_identifier_char(parser->current.end)) {
            parser->current.end++;
            return YP_TOKEN_CHARACTER_LITERAL;
          }
          return YP_TOKEN_QUESTION_MARK;

        // & && &&= &=
        case '&':
          if (match(parser, '&')) return match(parser, '=') ? YP_TOKEN_AMPERSAND_AMPERSAND_EQUAL : YP_TOKEN_AMPERSAND_AMPERSAND;
          return match(parser, '=') ? YP_TOKEN_AMPERSAND_EQUAL : YP_TOKEN_AMPERSAND;

        // | || ||= |=
        case '|':
          if (match(parser, '|')) return match(parser, '=') ? YP_TOKEN_PIPE_PIPE_EQUAL : YP_TOKEN_PIPE_PIPE;
          return match(parser, '=') ? YP_TOKEN_PIPE_EQUAL : YP_TOKEN_PIPE;

        // + += +@
        case '+':
          if (match(parser, '=')) return YP_TOKEN_PLUS_EQUAL;
          if ((parser->previous.type == YP_TOKEN_KEYWORD_DEF || parser->previous.type == YP_TOKEN_DOT) && match(parser, '@')) return YP_TOKEN_PLUS_AT;
          return YP_TOKEN_PLUS;

        // - -= -@
        case '-':
          if (match(parser, '>')) return YP_TOKEN_MINUS_GREATER;
          if (match(parser, '=')) return YP_TOKEN_MINUS_EQUAL;
          if ((parser->previous.type == YP_TOKEN_KEYWORD_DEF || parser->previous.type == YP_TOKEN_DOT) && match(parser, '@')) return YP_TOKEN_MINUS_AT;
          return YP_TOKEN_MINUS;

        // . .. ...
        case '.':
          if (!match(parser, '.')) return YP_TOKEN_DOT;
          return match(parser, '.') ? YP_TOKEN_DOT_DOT_DOT : YP_TOKEN_DOT_DOT;

        // integer
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
          return lex_numeric(parser);

        // :: symbol
        case ':':
          if (match(parser, ':')) return YP_TOKEN_COLON_COLON;
          if (is_identifier_char(parser->current.end)) {
            push_lex_mode(parser, (yp_lex_mode_t) { .mode = YP_LEX_SYMBOL, .term = '\0' });
            return YP_TOKEN_SYMBOL_BEGIN;
          }
          return YP_TOKEN_COLON;

        // / /=
        case '/':
          if (match(parser, '=')) return YP_TOKEN_SLASH_EQUAL;
          if (*parser->current.end == ' ') return YP_TOKEN_SLASH;

          push_lex_mode(parser, (yp_lex_mode_t) { .mode = YP_LEX_REGEXP, .term = '/' });
          return YP_TOKEN_REGEXP_BEGIN;

        // ^ ^=
        case '^': return match(parser, '=') ? YP_TOKEN_CARET_EQUAL : YP_TOKEN_CARET;

        // ~ ~@
        case '~':
          if ((parser->previous.type == YP_TOKEN_KEYWORD_DEF || parser->previous.type == YP_TOKEN_DOT) && match(parser, '@')) return YP_TOKEN_TILDE_AT;
          return YP_TOKEN_TILDE;

        // TODO
        case '\\':
          return YP_TOKEN_INVALID;

        // % %= %i %I %q %Q %w %W
        case '%':
          switch (*parser->current.end) {
            case '=':
              parser->current.end++;
              return YP_TOKEN_PERCENT_EQUAL;
            case 'i':
              parser->current.end++;
              push_lex_mode(parser, (yp_lex_mode_t) { .mode = YP_LEX_LIST, .term = terminator(*parser->current.end++), .interp = false });
              return YP_TOKEN_PERCENT_LOWER_I;
            case 'I':
              parser->current.end++;
              push_lex_mode(parser, (yp_lex_mode_t) { .mode = YP_LEX_LIST, .term = terminator(*parser->current.end++), .interp = true });
              return YP_TOKEN_PERCENT_UPPER_I;
            case 'r':
              parser->current.end++;
              push_lex_mode(parser, (yp_lex_mode_t) { .mode = YP_LEX_REGEXP, .term = terminator(*parser->current.end++), .interp = true });
              return YP_TOKEN_REGEXP_BEGIN;
            case 'q':
              parser->current.end++;
              push_lex_mode(parser, (yp_lex_mode_t) { .mode = YP_LEX_STRING, .term = terminator(*parser->current.end++), .interp = false });
              return YP_TOKEN_STRING_BEGIN;
            case 'Q':
              parser->current.end++;
              push_lex_mode(parser, (yp_lex_mode_t) { .mode = YP_LEX_STRING, .term = terminator(*parser->current.end++), .interp = true });
              return YP_TOKEN_STRING_BEGIN;
            case 'w':
              parser->current.end++;
              push_lex_mode(parser, (yp_lex_mode_t) { .mode = YP_LEX_LIST, .term = terminator(*parser->current.end++), .interp = false });
              return YP_TOKEN_PERCENT_LOWER_W;
            case 'W':
              parser->current.end++;
              push_lex_mode(parser, (yp_lex_mode_t) { .mode = YP_LEX_LIST, .term = terminator(*parser->current.end++), .interp = true });
              return YP_TOKEN_PERCENT_UPPER_W;
            case 'x':
              parser->current.end++;
              push_lex_mode(parser, (yp_lex_mode_t) { .mode = YP_LEX_STRING, .term = terminator(*parser->current.end++), .interp = true });
              return YP_TOKEN_PERCENT_LOWER_X;
            default:
              return YP_TOKEN_PERCENT;
          }

        // global variable
        case '$': return lex_global_variable(parser);

        // instance variable, class variable
        case '@': {
          yp_token_type_t type = match(parser, '@') ? YP_TOKEN_CLASS_VARIABLE : YP_TOKEN_INSTANCE_VARIABLE;

          if (is_identifier_start_char(parser->current.end)) {
            do { parser->current.end++; } while (is_identifier_char(parser->current.end));
            return type;
          }

          return YP_TOKEN_INVALID;
        }

        default: {
          // If this isn't the beginning of an identifier, then it's an invalid
          // token as we've exhausted all of the other options.
          if (!is_identifier_start_char(parser->current.start)) {
            return YP_TOKEN_INVALID;
          }

          yp_token_type_t type = lex_identifier(parser);

          // If we're lexing in a place that allows labels and we've hit a
          // colon, then we can return a label token.
          if ((parser->current.end[0] == ':') && (parser->current.end[1] != ':')) {
            parser->current.end++;
            return YP_TOKEN_LABEL;
          }

          return type;
        }
      }
    }
    case YP_LEX_EMBDOC: {
      parser->current.start = parser->current.end;

      // If we've hit the end of the embedded documentation then we'll return that token here.
      if (strncmp(parser->current.end, "=end\n", 5) == 0) {
        parser->current.end += 5;
        pop_lex_mode(parser);
        return YP_TOKEN_EMBDOC_END;
      }

      // Otherwise, we'll parse until the end of the line and return a line of
      // embedded documentation.
      while ((parser->current.end < parser->end) && (*parser->current.end++ != '\n'));

      // If we've still got content, then we'll return a line of embedded
      // documentation.
      if (parser->current.end < parser->end) {
        parser->lineno++;
        return YP_TOKEN_EMBDOC_LINE;
      }

      // Otherwise, fall back to error recovery.
      return parser->error_handler->unterminated_embdoc(parser);
    }
    case YP_LEX_LIST: {
      // If there's any whitespace at the start of the list, then we're going to
      // trim it off the beginning and create a new token.
      if (is_whitespace_char(parser->current.end)) {
        parser->current.start = parser->current.end;

        do {
          if (*parser->current.end == '\n') parser->lineno++;
          parser->current.end++;
        } while (is_whitespace_char(parser->current.end));

        return YP_TOKEN_WORDS_SEP;
      }

      // Next, we'll set to start of this token to be the current end.
      parser->current.start = parser->current.end;

      // Lex as far as we can into the word.
      while (parser->current.end < parser->end) {
        // If we've hit whitespace, then we must have received content by now,
        // so we can return an element of the list.
        if (is_whitespace_char(parser->current.end)) {
          return YP_TOKEN_STRING_CONTENT;
        }

        if (*parser->current.end == parser->lex_modes.current->term) {
          // If we've hit the terminator and we've already skipped past content,
          // then we can return a list node.
          if (parser->current.start < parser->current.end) {
            return YP_TOKEN_STRING_CONTENT;
          }

          // Otherwise, switch back to the default state and return the end of
          // the list.
          parser->current.end++;
          pop_lex_mode(parser);
          return YP_TOKEN_STRING_END;
        }

        // Otherwise, just skip past the content as it's part of an element of
        // the list.
        parser->current.end++;
      }

      // Otherwise, fall back to error recovery.
      return parser->error_handler->unterminated_list(parser);
    }
    case YP_LEX_REGEXP: {
      // First, we'll set to start of this token to be the current end.
      parser->current.start = parser->current.end;

      // If we've hit the end of the string, then we can return to the default
      // state of the lexer and return a string ending token.
      if (match(parser, parser->lex_modes.current->term)) {
        // Since we've hit the terminator of the regular expression, we now need
        // to parse the options.
        bool options = true;
        while (options) {
          switch (*parser->current.end) {
            case 'e': case 'i': case 'm': case 'n': case 's': case 'u': case 'x':
              parser->current.end++;
              break;
            default:
              options = false;
              break;
          }
        }

        pop_lex_mode(parser);
        return YP_TOKEN_REGEXP_END;
      }

      // Otherwise, we'll lex as far as we can into the regular expression. If
      // we hit the end of the regular expression, then we'll return everything
      // up to that point.
      while (parser->current.end < parser->end) {
        // If we hit the terminator, then return this element of the string.
        if (*parser->current.end == parser->lex_modes.current->term) {
          return YP_TOKEN_STRING_CONTENT;
        }

        // If we hit a newline, make sure to do the required bookkeeping.
        if (*parser->current.end == '\n') parser->lineno++;

        // If we've hit a #, then check if it's used as the beginning of either
        // an embedded variable or an embedded expression.
        if (*parser->current.end == '#') {
          switch (parser->current.end[1]) {
            case '{':
              // In this case it's the start of an embedded expression.

              // If we have already consumed content, then we need to return
              // that content as string content first.
              if (parser->current.end > parser->current.start) {
                return YP_TOKEN_STRING_CONTENT;
              }

              parser->current.end += 2;
              push_lex_mode(parser, (yp_lex_mode_t) { .mode = YP_LEX_EMBEXPR });
              return YP_TOKEN_EMBEXPR_BEGIN;
          }
        }

        parser->current.end++;
      }

      // Otherwise, fall back to error recovery.
      return parser->error_handler->unterminated_regexp(parser);
    }
    case YP_LEX_STRING: {
      // First, we'll set to start of this token to be the current end.
      parser->current.start = parser->current.end;

      // If we've hit the end of the string, then we can return to the default
      // state of the lexer and return a string ending token.
      if (match(parser, parser->lex_modes.current->term)) {
        pop_lex_mode(parser);
        return YP_TOKEN_STRING_END;
      }

      // Otherwise, we'll lex as far as we can into the string. If we hit the
      // end of the string, then we'll return everything up to that point.
      while (parser->current.end < parser->end) {
        // If we hit the terminator, then return this element of the string.
        if (*parser->current.end == parser->lex_modes.current->term) {
          return YP_TOKEN_STRING_CONTENT;
        }

        // If we hit a newline, make sure to do the required bookkeeping.
        if (*parser->current.end == '\n') parser->lineno++;

        // If our current lex state allows interpolation and we've hit a #, then
        // check if it's used as the beginning of either an embedded variable or
        // an embedded expression.
        if (parser->lex_modes.current->interp && *parser->current.end == '#') {
          switch (parser->current.end[1]) {
            case '@':
              // In this case it could be an embedded instance or class
              // variable.
              break;
            case '$':
              // In this case it could be an embedded global variable.
              break;
            case '{':
              // In this case it's the start of an embedded expression.

              // If we have already consumed content, then we need to return
              // that content as string content first.
              if (parser->current.end > parser->current.start) {
                return YP_TOKEN_STRING_CONTENT;
              }

              parser->current.end += 2;
              push_lex_mode(parser, (yp_lex_mode_t) { .mode = YP_LEX_EMBEXPR });
              return YP_TOKEN_EMBEXPR_BEGIN;
          }
        }

        parser->current.end++;
      }

      // Otherwise, fall back to error recovery.
      return parser->error_handler->unterminated_string(parser);
    }
    case YP_LEX_SYMBOL: {
      // First, we'll set to start of this token to be the current end.
      parser->current.start = parser->current.end;

      // Lex as far as we can into the symbol.
      if (parser->current.end < parser->end && is_identifier_start_char(parser->current.end++)) {
        pop_lex_mode(parser);

        yp_token_type_t type = lex_identifier(parser);
        return match(parser, '=') ? YP_TOKEN_IDENTIFIER : type;
      }

      // If we get here then we have the start of a symbol with no content. In
      // that case return an invalid token.
      return YP_TOKEN_INVALID;
    }
  }

  // We shouldn't be able to get here at all, but some compilers can't figure
  // that out, so just returning a value here to make them happy.
  return YP_TOKEN_INVALID;
}

/******************************************************************************/
/* External functions                                                         */
/******************************************************************************/

// Initialize a parser with the given start and end pointers.
void
yp_parser_init(yp_parser_t *parser, const char *source, off_t size, yp_error_handler_t *error_handler) {
  *parser = (yp_parser_t) {
    .lex_modes = {
      .index = 0,
      .stack = {{ .mode = YP_LEX_DEFAULT }},
      .current = &parser->lex_modes.stack[0]
    },
    .start = source,
    .end = source + size,
    .current = { .start = source, .end = source },
    .lineno = 1,
    .error_handler = error_handler
  };
}

// Get the next token type and set its value on the current pointer.
void
yp_lex_token(yp_parser_t *parser) {
  parser->previous = parser->current;
  parser->current.type = lex_token_type(parser);
}

/******************************************************************************/
/* C-extension functions                                                      */
/******************************************************************************/

// By default, the lexer won't attempt to recover from lexer errors at all. This
// function provides that implementation.
static yp_token_type_t
unrecoverable(yp_parser_t *parser) {
  return YP_TOKEN_EOF;
}

static VALUE
token_inspect(yp_parser_t *parser) {
  yp_token_t token = parser->current;
  VALUE parts = rb_ary_new();

  // First, we're going to push on the location information.
  VALUE location = rb_ary_new();
  rb_ary_push(location, LONG2FIX(token.start - parser->start));
  rb_ary_push(location, LONG2FIX(token.end - parser->start));
  rb_ary_push(parts, location);

  // Next, we're going to push on a symbol that represents the type of token.
  switch (token.type) {
    // We're going to special-case the invalid token here since that doesn't
    // actually exist in Ripper. This is going to give us a little more
    // information when our tests fail.
    case YP_TOKEN_INVALID:
      rb_ary_push(parts, ID2SYM(rb_intern("INVALID")));
      // fprintf(stderr, "Invalid token: %.*s\n", (int) (token.end - token.start), token.start);
      break;

#define CASE(type) case YP_TOKEN_##type: rb_ary_push(parts, ID2SYM(rb_intern(#type))); break;

    CASE(AMPERSAND)
    CASE(AMPERSAND_AMPERSAND)
    CASE(AMPERSAND_AMPERSAND_EQUAL)
    CASE(AMPERSAND_EQUAL)
    CASE(BACK_REFERENCE)
    CASE(BACKTICK)
    CASE(BANG)
    CASE(BANG_AT)
    CASE(BANG_EQUAL)
    CASE(BANG_TILDE)
    CASE(BRACE_LEFT)
    CASE(BRACE_RIGHT)
    CASE(BRACKET_LEFT)
    CASE(BRACKET_LEFT_RIGHT)
    CASE(BRACKET_RIGHT)
    CASE(CARET)
    CASE(CARET_EQUAL)
    CASE(CHARACTER_LITERAL)
    CASE(CLASS_VARIABLE)
    CASE(COLON)
    CASE(COLON_COLON)
    CASE(COMMA)
    CASE(COMMENT)
    CASE(CONSTANT)
    CASE(DOT)
    CASE(DOT_DOT)
    CASE(DOT_DOT_DOT)
    CASE(EMBDOC_BEGIN)
    CASE(EMBDOC_END)
    CASE(EMBDOC_LINE)
    CASE(EMBEXPR_BEGIN)
    CASE(EMBEXPR_END)
    CASE(EQUAL)
    CASE(EQUAL_EQUAL)
    CASE(EQUAL_EQUAL_EQUAL)
    CASE(EQUAL_GREATER)
    CASE(EQUAL_TILDE)
    CASE(FLOAT)
    CASE(GREATER)
    CASE(GREATER_EQUAL)
    CASE(GREATER_GREATER)
    CASE(GREATER_GREATER_EQUAL)
    CASE(GLOBAL_VARIABLE)
    CASE(IDENTIFIER)
    CASE(IMAGINARY_NUMBER)
    CASE(INTEGER)
    CASE(INSTANCE_VARIABLE)
    CASE(KEYWORD___ENCODING__)
    CASE(KEYWORD___LINE__)
    CASE(KEYWORD___FILE__)
    CASE(KEYWORD_ALIAS)
    CASE(KEYWORD_AND)
    CASE(KEYWORD_BEGIN)
    CASE(KEYWORD_BEGIN_UPCASE)
    CASE(KEYWORD_BREAK)
    CASE(KEYWORD_CASE)
    CASE(KEYWORD_CLASS)
    CASE(KEYWORD_DEF)
    CASE(KEYWORD_DEFINED)
    CASE(KEYWORD_DO)
    CASE(KEYWORD_ELSE)
    CASE(KEYWORD_ELSIF)
    CASE(KEYWORD_END)
    CASE(KEYWORD_END_UPCASE)
    CASE(KEYWORD_ENSURE)
    CASE(KEYWORD_FALSE)
    CASE(KEYWORD_FOR)
    CASE(KEYWORD_IF)
    CASE(KEYWORD_IN)
    CASE(KEYWORD_MODULE)
    CASE(KEYWORD_NEXT)
    CASE(KEYWORD_NIL)
    CASE(KEYWORD_NOT)
    CASE(KEYWORD_OR)
    CASE(KEYWORD_REDO)
    CASE(KEYWORD_RESCUE)
    CASE(KEYWORD_RETRY)
    CASE(KEYWORD_RETURN)
    CASE(KEYWORD_SELF)
    CASE(KEYWORD_SUPER)
    CASE(KEYWORD_THEN)
    CASE(KEYWORD_TRUE)
    CASE(KEYWORD_UNDEF)
    CASE(KEYWORD_UNLESS)
    CASE(KEYWORD_UNTIL)
    CASE(KEYWORD_WHEN)
    CASE(KEYWORD_WHILE)
    CASE(KEYWORD_YIELD)
    CASE(LABEL)
    CASE(LAMBDA_BEGIN)
    CASE(LESS)
    CASE(LESS_EQUAL)
    CASE(LESS_EQUAL_GREATER)
    CASE(LESS_LESS)
    CASE(LESS_LESS_EQUAL)
    CASE(MINUS)
    CASE(MINUS_AT)
    CASE(MINUS_EQUAL)
    CASE(MINUS_GREATER)
    CASE(NEWLINE)
    CASE(NTH_REFERENCE)
    CASE(PARENTHESIS_LEFT)
    CASE(PARENTHESIS_RIGHT)
    CASE(PERCENT)
    CASE(PERCENT_EQUAL)
    CASE(PERCENT_LOWER_I)
    CASE(PERCENT_LOWER_W)
    CASE(PERCENT_LOWER_X)
    CASE(PERCENT_UPPER_I)
    CASE(PERCENT_UPPER_W)
    CASE(PIPE)
    CASE(PIPE_EQUAL)
    CASE(PIPE_PIPE)
    CASE(PIPE_PIPE_EQUAL)
    CASE(PLUS)
    CASE(PLUS_AT)
    CASE(PLUS_EQUAL)
    CASE(QUESTION_MARK)
    CASE(RATIONAL_NUMBER)
    CASE(REGEXP_BEGIN)
    CASE(REGEXP_END)
    CASE(SEMICOLON)
    CASE(SLASH)
    CASE(SLASH_EQUAL)
    CASE(STAR)
    CASE(STAR_EQUAL)
    CASE(STAR_STAR)
    CASE(STAR_STAR_EQUAL)
    CASE(STRING_BEGIN)
    CASE(STRING_CONTENT)
    CASE(STRING_END)
    CASE(SYMBOL_BEGIN)
    CASE(TILDE)
    CASE(TILDE_AT)
    CASE(WORDS_SEP)

#undef CASE

    default:
      rb_bug("Unknown token type: %d", token.type);
  }

  rb_ary_push(parts, rb_str_new(token.start, token.end - token.start));
  return parts;
}

static VALUE
each_token(VALUE self, VALUE rb_filepath) {
  char *filepath = StringValueCStr(rb_filepath);

  // Open the file for reading
  int fd = open(filepath, O_RDONLY);
  if (fd == -1) {
    perror("open");
    return Qnil;
  }

  // Stat the file to get the file size
  struct stat sb;
  if (fstat(fd, &sb) == -1) {
    close(fd);
    perror("fstat");
    return Qnil;
  }

  // mmap the file descriptor to virtually get the contents
  off_t size = sb.st_size;
  const char *source = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);

  close(fd);
  if (source == MAP_FAILED) {
    perror("mmap");
    return Qnil;
  }

  yp_error_handler_t default_error_handler = {
    .unterminated_embdoc = unrecoverable,
    .unterminated_list = unrecoverable,
    .unterminated_regexp = unrecoverable,
    .unterminated_string = unrecoverable
  };

  // Instantiate the parser struct with all of the necessary information
  yp_parser_t parser;
  yp_parser_init(&parser, source, size, &default_error_handler);

  // Create an array and populate it with the tokens from the filepath
  for (yp_lex_token(&parser); parser.current.type != YP_TOKEN_EOF; yp_lex_token(&parser)) {
    rb_yield(token_inspect(&parser));
  }

  // Clean up and free
  munmap((void *) source, size);
  return Qnil;
}

void
Init_yarp(void) {
  VALUE rb_cYARP = rb_define_module("YARP");
  rb_define_singleton_method(rb_cYARP, "each_token", each_token, 1);
}