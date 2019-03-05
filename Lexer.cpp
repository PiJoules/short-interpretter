#include "Lexer.h"
#include "Common.h"

namespace lang {

SourceLocation::SourceLocation(unsigned row, unsigned col)
    : row(row), col(col) {}

Token::Token(TokenKind kind, SourceLocation loc, const std::string &chars)
    : kind(kind), loc(loc), chars(chars) {}

Token::Token(TokenKind kind, const std::string &chars)
    : kind(kind), chars(chars) {}

LexStatus LexStatus::GetSuccess() {
  LexStatus status;
  status.kind_ = LEX_SUCCESS;
  return status;
}

LexStatus LexStatus::GetFailure(SourceLocation loc, char c) {
  LexStatus status;
  status.kind_ = LEX_FAIL;
  status.loc_ = loc;
  status.failing_char_ = c;
  return status;
}

LexStatus ReadTokens(const std::string &input, std::vector<Token> &result) {
  // Signed so we check for overflow
  int64_t current = 0;
  int64_t col = 0;
  int64_t row = 0;

  while (current < input.size()) {
    char c = input[current];
    SourceLocation loc(row, col);

    if (c == '(') {
      Token tok(TOK_LPAR, loc, "(");
      result.push_back(tok);
      SafeSignedInc(current);
      SafeSignedInc(col);
      continue;
    } else if (c == ')') {
      Token tok(TOK_RPAR, loc, ")");
      result.push_back(tok);
      SafeSignedInc(current);
      SafeSignedInc(col);
      continue;
    } else if (c == '"') {
      std::string str;

      // Consume opening "
      SafeSignedInc(current);
      c = input[current];

      while (c != '"') {
        str.push_back(c);
        SafeSignedInc(current);
        c = input[current];
      }

      // Consume ending "
      SafeSignedInc(current);

      Token tok(TOK_STR, loc, str);
      result.push_back(tok);
      SafeSignedInplaceAdd(col, str.size() + 2);  // +2 for quotes
      continue;
    } else if (isspace(c)) {
      SafeSignedInc(current);
      if (c == '\n') {
        SafeSignedInc(row);
        current = 0;
        continue;
      }
      SafeSignedInc(col);
      continue;
    } else if (isdigit(c)) {
      std::string str;
      while (isdigit(c)) {
        str.push_back(c);
        SafeSignedInc(current);
        c = input[current];
      }

      Token tok(TOK_INT, loc, str);
      result.push_back(tok);
      SafeSignedInplaceAdd(col, str.size());
      continue;
    } else if (isalpha(c)) {
      // IDs are composed only of alphabetic characters for now.
      std::string str;
      while (isalpha(c)) {
        str.push_back(c);
        SafeSignedInc(current);
        c = input[current];
      }

      TokenKind kind;
      if (str == "def")
        kind = TOK_DEF;
      else
        kind = TOK_ID;

      Token tok(kind, loc, str);
      result.push_back(tok);
      SafeSignedInplaceAdd(col, str.size());
      continue;
    } else {
      return LexStatus::GetFailure(loc, c);
    }
  }

  return LexStatus::GetSuccess();
}

}  // namespace lang
