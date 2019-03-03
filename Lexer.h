#ifndef LEXER_H
#define LEXER_H

#include <cassert>
#include <string>
#include <vector>

namespace lang {

/**
 * A container for representing the original source of some piece of information
 * in the original source.
 */
struct SourceLocation {
  SourceLocation(unsigned row, unsigned col);
  SourceLocation() = default;

  // Both the row and column are zero-indexed.
  int32_t row = -1;
  int32_t col = -1;

  // We can use a SourceLocation as optional data for some other structures. In
  // these cases, the SourceLocation does not need to be valid.
  bool isValid() const { return row >= 0 && col >= 0; }
};

enum TokenKind {
  // This token does not represent anything and is invalid.
  TOK_NONE,

  // Containment characters
  TOK_LPAR,
  TOK_RPAR,

  // Atoms
  TOK_INT,
  TOK_STR,
  TOK_ID,
};

/**
 * A generic container for any kind of token and data relevant to it.
 */
struct Token {
  Token(TokenKind kind, SourceLocation loc, const std::string &chars);

  // Made with an invalid SourceLocation.
  Token(TokenKind kind, const std::string &chars);

  Token() { kind = TOK_NONE; }

  TokenKind kind;
  SourceLocation loc;

  // If the token is a string, it will not be saved surrounded with the quotes.
  std::string chars;

  // We do not care about the SourceLocation.
  bool operator==(const Token &other) const {
    return kind == other.kind && chars == other.chars;
  }

  bool isValid() const { return kind == TOK_NONE; }
};

enum LexStatusKind {
  LEX_SUCCESS,
  LEX_FAIL,
};

class LexStatus {
 public:
  LexStatusKind getKind() const { return kind_; }
  bool isSuccessful() const { return kind_ == LEX_SUCCESS; }
  bool getFailingCharacter() const {
    assert(!isSuccessful() &&
           "Cannot get the character we failed on if we did not fail");
    return failing_char_;
  }
  SourceLocation getFailingLocation() const {
    assert(!isSuccessful() &&
           "Cannot get the location we failed on if we did not fail");
    return loc_;
  }

  static LexStatus GetSuccess();
  static LexStatus GetFailure(SourceLocation loc, char c);

 private:
  // Does nothing, but we do not want to accidentally create a new LexStatus
  // without any of the static getters.
  LexStatus() {}

  LexStatusKind kind_;

  // These values may not be relevant depending on the status kind. These are
  // left unitialized in these cases.
  SourceLocation loc_;
  char failing_char_;
};

LexStatus ReadTokens(const std::string &input, std::vector<Token> &result);

}  // namespace lang

#endif
