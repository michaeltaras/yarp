# frozen_string_literal: true

require_relative "yarp/yarp"
require_relative "yarp/version"

module YARP
  # This lexes with the Ripper lex. It drops any space events and normalizes all
  # ignored newlines into regular newlines.
  def self.lex_ripper(filepath)
    Ripper.lex(File.read(filepath)).each_with_object([]) do |token, tokens|
      case token[1]
      when :on_ignored_nl
        tokens << [token[0], :on_nl, token[2], token[3]]
      when :on_sp
        # skip
      else
        tokens << token
      end
    end
  end

  # Returns an array of tokens that closely resembles that of the Ripper lexer.
  # The only difference is that since we don't keep track of lexer state in the
  # same way, it's going to always return the NONE state.
  def self.lex_compat(filepath)
    offsets = [0]
    File.foreach(filepath) { |line| offsets << offsets.last + line.bytesize }

    lexer_state = Ripper::Lexer::State.new(0)
    tokens = []

    lex_file(filepath) do |((start_char, _), type, value)|
      line_number, line_offset =
        offsets.each_with_index.detect do |(offset, line)|
          break [line, offsets[line - 1]] if start_char < offset
        end

      line_number ||= offsets.length + 1
      line_offset ||= offsets.last

      line_byte = start_char - line_offset
      event = RIPPER.fetch(type)

      unescaped =
        if %i[on_comment on_tstring_content].include?(event) && value.include?("\\")
          # Ripper unescapes string content and comments, so we need to do the
          # same here.
          value.force_encoding("UTF-8").unicode_normalize
        else
          value
        end

      tokens << [[line_number, line_byte], event, unescaped, lexer_state]
    end

    tokens
  end

  RIPPER = {
    AMPERSAND: :on_op,
    AMPERSAND_AMPERSAND: :on_op,
    AMPERSAND_AMPERSAND_EQUAL: :on_op,
    AMPERSAND_EQUAL: :on_op,
    BACK_REFERENCE: :on_backref,
    BACKTICK: :on_backtick,
    BANG: :on_op,
    BANG_AT: :on_op,
    BANG_EQUAL: :on_op,
    BANG_TILDE: :on_op,
    BRACE_LEFT: :on_lbrace,
    BRACE_RIGHT: :on_rbrace,
    BRACKET_LEFT: :on_lbracket,
    BRACKET_LEFT_RIGHT: :on_op,
    BRACKET_RIGHT: :on_rbracket,
    CARET: :on_op,
    CARET_EQUAL: :on_op,
    CHARACTER_LITERAL: :on_CHAR,
    CLASS_VARIABLE: :on_cvar,
    COLON: :on_op,
    COLON_COLON: :on_op,
    COMMA: :on_comma,
    COMMENT: :on_comment,
    CONSTANT: :on_const,
    DOT: :on_period,
    DOT_DOT: :on_op,
    DOT_DOT_DOT: :on_op,
    EMBDOC_BEGIN: :on_embdoc_beg,
    EMBDOC_END: :on_embdoc_end,
    EMBDOC_LINE: :on_embdoc,
    EMBEXPR_BEGIN: :on_embexpr_beg,
    EMBEXPR_END: :on_embexpr_end,
    EQUAL: :on_op,
    EQUAL_EQUAL: :on_op,
    EQUAL_EQUAL_EQUAL: :on_op,
    EQUAL_GREATER: :on_op,
    EQUAL_TILDE: :on_op,
    FLOAT: :on_float,
    GREATER: :on_op,
    GREATER_EQUAL: :on_op,
    GREATER_GREATER: :on_op,
    GREATER_GREATER_EQUAL: :on_op,
    GLOBAL_VARIABLE: :on_gvar,
    IDENTIFIER: :on_ident,
    IMAGINARY_NUMBER: :on_imaginary,
    INTEGER: :on_int,
    INSTANCE_VARIABLE: :on_ivar,
    INVALID: :INVALID,
    KEYWORD___ENCODING__: :on_kw,
    KEYWORD___LINE__: :on_kw,
    KEYWORD___FILE__: :on_kw,
    KEYWORD_ALIAS: :on_kw,
    KEYWORD_AND: :on_kw,
    KEYWORD_BEGIN: :on_kw,
    KEYWORD_BEGIN_UPCASE: :on_kw,
    KEYWORD_BREAK: :on_kw,
    KEYWORD_CASE: :on_kw,
    KEYWORD_CLASS: :on_kw,
    KEYWORD_DEF: :on_kw,
    KEYWORD_DEFINED: :on_kw,
    KEYWORD_DO: :on_kw,
    KEYWORD_ELSE: :on_kw,
    KEYWORD_ELSIF: :on_kw,
    KEYWORD_END: :on_kw,
    KEYWORD_END_UPCASE: :on_kw,
    KEYWORD_ENSURE: :on_kw,
    KEYWORD_FALSE: :on_kw,
    KEYWORD_FOR: :on_kw,
    KEYWORD_IF: :on_kw,
    KEYWORD_IN: :on_kw,
    KEYWORD_MODULE: :on_kw,
    KEYWORD_NEXT: :on_kw,
    KEYWORD_NIL: :on_kw,
    KEYWORD_NOT: :on_kw,
    KEYWORD_OR: :on_kw,
    KEYWORD_REDO: :on_kw,
    KEYWORD_RESCUE: :on_kw,
    KEYWORD_RETRY: :on_kw,
    KEYWORD_RETURN: :on_kw,
    KEYWORD_SELF: :on_kw,
    KEYWORD_SUPER: :on_kw,
    KEYWORD_THEN: :on_kw,
    KEYWORD_TRUE: :on_kw,
    KEYWORD_UNDEF: :on_kw,
    KEYWORD_UNLESS: :on_kw,
    KEYWORD_UNTIL: :on_kw,
    KEYWORD_WHEN: :on_kw,
    KEYWORD_WHILE: :on_kw,
    KEYWORD_YIELD: :on_kw,
    LABEL: :on_label,
    LAMBDA_BEGIN: :on_tlambeg,
    LESS: :on_op,
    LESS_EQUAL: :on_op,
    LESS_EQUAL_GREATER: :on_op,
    LESS_LESS: :on_op,
    LESS_LESS_EQUAL: :on_op,
    MINUS: :on_op,
    MINUS_AT: :on_op,
    MINUS_EQUAL: :on_op,
    MINUS_GREATER: :on_tlambda,
    NEWLINE: :on_nl,
    NTH_REFERENCE: :on_backref,
    PARENTHESIS_LEFT: :on_lparen,
    PARENTHESIS_RIGHT: :on_rparen,
    PERCENT: :on_op,
    PERCENT_EQUAL: :on_op,
    PERCENT_LOWER_I: :on_qsymbols_beg,
    PERCENT_LOWER_W: :on_qwords_beg,
    PERCENT_LOWER_X: :on_backtick,
    PERCENT_UPPER_I: :on_symbols_beg,
    PERCENT_UPPER_W: :on_words_beg,
    PIPE: :on_op,
    PIPE_EQUAL: :on_op,
    PIPE_PIPE: :on_op,
    PIPE_PIPE_EQUAL: :on_op,
    PLUS: :on_op,
    PLUS_AT: :on_op,
    PLUS_EQUAL: :on_op,
    QUESTION_MARK: :on_op,
    RATIONAL_NUMBER: :on_rational,
    REGEXP_BEGIN: :on_regexp_beg,
    REGEXP_END: :on_regexp_end,
    SEMICOLON: :on_semicolon,
    SLASH: :on_op,
    SLASH_EQUAL: :on_op,
    STAR: :on_op,
    STAR_EQUAL: :on_op,
    STAR_STAR: :on_op,
    STAR_STAR_EQUAL: :on_op,
    STRING_BEGIN: :on_tstring_beg,
    STRING_CONTENT: :on_tstring_content,
    STRING_END: :on_tstring_end,
    SYMBOL_BEGIN: :on_symbeg,
    TILDE: :on_op,
    TILDE_AT: :on_op,
    WORDS_SEP: :on_words_sep,
  }.freeze

  private_constant :RIPPER
end
