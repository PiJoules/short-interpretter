#include <unordered_map>

#include "Common.h"
#include "Interpret.h"
#include "Parser.h"

namespace lang {

NodeKind Module::Kind = NODE_MODULE;
NodeKind Stmt::Kind = NODE_STMT;
NodeKind Int::Kind = NODE_INT;
NodeKind Str::Kind = NODE_STR;
NodeKind ID::Kind = NODE_ID;
NodeKind Assign::Kind = NODE_ASSIGN;
NodeKind BinOp::Kind = NODE_BINOP;
NodeKind Call::Kind = NODE_CALL;

namespace {

ParseStatus ReadNode(const std::vector<Token> &input, int64_t &current,
                     Node **result);

ParseStatus ReadBinOpOperands(const std::vector<Token> &input, int64_t &current,
                              Node **result, BinOpKind kind) {
  SourceLocation start_loc = input[current].loc;

  Node *lhs, *rhs;
  ParseStatus status = ReadNode(input, current, &lhs);
  if (!status) return status;
  unique<Node> lhs_node(lhs);

  status = ReadNode(input, current, &rhs);
  if (!status) return status;
  unique<Node> rhs_node(rhs);

  // We reached the end of the input without finding an appropriate RPAR.
  const Token &tok = input[current];
  if (tok.kind != TOK_RPAR)
    return ParseStatus::GetFailure(PARSE_FAIL_TOO_MANY_BINOP_OPERANDS, tok);

  // Consume the RPAR
  SafeSignedInc(current);

  *result =
      SafeNew<BinOp>(start_loc, kind, std::move(lhs_node), std::move(rhs_node));
  return ParseStatus::GetSuccess();
}

ParseStatus ReadCall(const std::vector<Token> &input, int64_t &current,
                     Node **result) {
  SourceLocation start_loc = input[current].loc;

  // Consume the (
  SafeSignedInc(current);

  Node *func;
  ParseStatus status = ReadNode(input, current, &func);
  if (!status) return status;
  unique<Node> unique_func(func);

  // Handle builtins here for now.
  // TODO: This should be replaced with it's own ADD/SUB token.
  if (const auto *id_func = func->getAs<ID>()) {
    if (id_func->getName() == "add") {
      return ReadBinOpOperands(input, current, result, BINOP_ADD);
    } else if (id_func->getName() == "sub") {
      return ReadBinOpOperands(input, current, result, BINOP_SUB);
    }
  }

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

ParseStatus ReadDefine(const std::vector<Token> &input, int64_t &current,
                       Node **result) {
  SourceLocation start_loc = input[current].loc;

  // Consume the 'def'
  SafeSignedInc(current);

  // Store destination
  Node *dst;
  ParseStatus status = ReadNode(input, current, &dst);
  if (!status) return status;
  unique<Node> dst_node(dst);

  // Load value
  Node *src;
  status = ReadNode(input, current, &src);
  if (!status) return status;
  unique<Node> src_node(src);

  *result =
      SafeNew<Assign>(start_loc, std::move(dst_node), std::move(src_node));
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
      case TOK_DEF:
        return ReadDefine(input, current, result);
      case TOK_NONE:
        lang_unreachable(
            "Ran into a TOK_NONE. Logically, this should not be handled "
            "here. This means an invalid token was created somewhere.");
        break;
      case TOK_SEMICOL:
        lang_unreachable(
            "Ran into a TOK_SEMICOL. This implies we have not finished parsing "
            "a statement, which should be handled by ReadStmt().");
        break;
    }
  }

  return ParseStatus::GetFailure(PARSE_FAIL, input.back());
}

ParseStatus ReadStmt(const std::vector<Token> &input, int64_t &current,
                     Node **result) {
  SourceLocation start_loc = input[current].loc;

  Node *inner;
  ParseStatus status = ReadNode(input, current, &inner);
  if (!status) return status;
  unique<Node> inner_node(inner);

  // Consume remaining ';'
  if (current >= input.size())
    return ParseStatus::GetFailure(PARSE_FAIL_MISSING_SEMICOL, input.back());

  const Token &tok = input[current];
  if (tok.kind != TOK_SEMICOL)
    return ParseStatus::GetFailure(PARSE_FAIL_MISSING_SEMICOL, tok);

  SafeSignedInc(current);

  *result = SafeNew<Stmt>(start_loc, std::move(inner_node));
  return ParseStatus::GetSuccess();
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
    ParseStatus status = ReadStmt(input, current, &result);
    if (!status) return status;

    unique<Node> node(result);
    nodes.push_back(std::move(node));
  }
  *module = SafeNew<Module>(input.front().loc, std::move(nodes));
  return ParseStatus::GetSuccess();
}

}  // namespace lang
