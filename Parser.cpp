#include <unordered_map>

#include "Common.h"
#include "Interpret.h"
#include "Parser.h"

namespace lang {

NodeKind Module::Kind = NODE_MODULE;
NodeKind Int::Kind = NODE_INT;
NodeKind Str::Kind = NODE_STR;
NodeKind ID::Kind = NODE_ID;
NodeKind Call::Kind = NODE_CALL;

namespace {

ParseStatus ReadNode(const std::vector<Token> &input, int64_t &current,
                     Node **result);

ParseStatus ReadCall(const std::vector<Token> &input, int64_t &current,
                     Node **result) {
  SourceLocation start_loc = input[current].loc;

  // Consume the (
  SafeSignedInc(current);

  Node *func;
  ParseStatus status = ReadNode(input, current, &func);
  if (!status) return status;

  bool found_rpar = false;
  std::vector<unique<Node>> args;
  while (current < input.size()) {
    const Token &tok = input[current];
    if (tok.kind == TOK_RPAR) {
      found_rpar = true;
      break;
    }

    Node *arg;
    status = ReadNode(input, current, &arg);
    if (!status) return status;

    unique<Node> node(arg);
    args.push_back(std::move(node));
  }

  // We reached the end of the input without finding an appropriate RPAR.
  if (!found_rpar)
    return ParseStatus::GetFailure(PARSE_FAIL_NO_RPAR, input.back());

  // Consume the RPAR
  SafeSignedInc(current);

  unique<Node> unique_func(func);
  *result = SafeNew<Call>(start_loc, std::move(unique_func), std::move(args));
  return ParseStatus::GetSuccess();
}

ParseStatus ReadInt(const std::vector<Token> &input, int64_t &current,
                    Node **result) {
  const Token &tok = input[current];
  int32_t val = std::stol(tok.chars);
  *result = SafeNew<Int>(tok.loc, val);
  SafeSignedInc(current);
  return ParseStatus::GetSuccess();
}

ParseStatus ReadStr(const std::vector<Token> &input, int64_t &current,
                    Node **result) {
  const Token &tok = input[current];
  std::string val = tok.chars;
  *result = SafeNew<Str>(tok.loc, val);
  SafeSignedInc(current);
  return ParseStatus::GetSuccess();
}

ParseStatus ReadID(const std::vector<Token> &input, int64_t &current,
                   Node **result) {
  const Token &tok = input[current];
  std::string val = tok.chars;
  *result = SafeNew<ID>(tok.loc, val);
  SafeSignedInc(current);
  return ParseStatus::GetSuccess();
}

ParseStatus ReadNode(const std::vector<Token> &input, int64_t &current,
                     Node **result) {
  while (current < input.size()) {
    const Token &tok = input[current];
    switch (tok.kind) {
      case TOK_LPAR:
        return ReadCall(input, current, result);
      case TOK_INT:
        return ReadInt(input, current, result);
      case TOK_STR:
        return ReadStr(input, current, result);
      case TOK_ID:
        return ReadID(input, current, result);
      case TOK_RPAR:
        return ParseStatus::GetFailure(PARSE_FAIL_NO_LPAR, tok);
      case TOK_NONE:
        assert(0 &&
               "Ran into a TOK_NONE. Logically, this should not be handled "
               "here. This means an invalid token was created somewhere.");
        break;
    }
  }

  return ParseStatus::GetFailure(PARSE_FAIL, input.back());
}

}  // namespace

ParseStatus ParseStatus::GetSuccess() {
  ParseStatus status;
  status.kind_ = PARSE_SUCCESS;
  return status;
}

ParseStatus ParseStatus::GetFailure(ParseStatusKind kind, SourceLocation loc,
                                    Token tok) {
  ParseStatus status;
  status.kind_ = kind;
  status.loc_ = loc;
  status.tok_ = tok;
  return status;
}

ParseStatus ReadModule(const std::vector<Token> &input, Module **module) {
  int64_t current = 0;
  std::vector<unique<Node>> nodes;
  while (current < input.size()) {
    Node *result;
    ParseStatus status = ReadNode(input, current, &result);
    if (!status) return status;

    unique<Node> node(result);
    nodes.push_back(std::move(node));
  }
  *module = SafeNew<Module>(input.front().loc, std::move(nodes));
  return ParseStatus::GetSuccess();
}

}  // namespace lang
