#ifndef PARSER_H
#define PARSER_H

#include <memory>

#include "Common.h"
#include "Lexer.h"

namespace lang {

enum NodeKind {
  NODE_MODULE,

  NODE_ASSIGN,

  // Expressions
  NODE_INT,
  NODE_STR,
  NODE_ID,
  NODE_CALL,
};

class Node {
 public:
  virtual ~Node() {}
  NodeKind getKind() const { return kind_; }
  SourceLocation getLoc() const { return loc_; }

  template <typename NodeTy>
  bool isa() const {
    return kind_ == NodeTy::Kind;
  }

  template <typename NodeTy>
  NodeTy *getAs() {
    if (kind_ != NodeTy::Kind) return nullptr;
    return static_cast<NodeTy *>(this);
  }

  template <typename NodeTy>
  const NodeTy *getAs() const {
    if (kind_ != NodeTy::Kind) return nullptr;
    return static_cast<const NodeTy *>(this);
  }

  bool operator==(const Node &other) const { return equals(other); }
  bool operator!=(const Node &other) const { return !equals(other); }

  virtual bool equals(const Node &other) const = 0;

 protected:
  // We do not want to create a raw node.
  Node(NodeKind kind, SourceLocation loc) : kind_(kind), loc_(loc) {}

  // Create with an invalid source location.
  Node(NodeKind kind) : kind_(kind) {}

 private:
  NodeKind kind_;
  SourceLocation loc_;
};

class Module : public Node {
 public:
  static NodeKind Kind;

  Module(SourceLocation loc, std::vector<unique<Node>> nodes)
      : Node(Kind, loc), nodes_(std::move(nodes)) {
    CheckNonNullVector(nodes_);
  }
  Module(std::vector<unique<Node>> nodes)
      : Node(Kind), nodes_(std::move(nodes)) {
    CheckNonNullVector(nodes_);
  }

  const std::vector<unique<Node>> &getNodes() const { return nodes_; }

  bool equals(const Node &other) const override {
    const auto *module = other.getAs<Module>();
    if (!module) return false;

    if (nodes_.size() != module->getNodes().size()) return false;

    for (unsigned i = 0; i < nodes_.size(); ++i) {
      if (*(nodes_[i]) != *(module->getNodes()[i])) return false;
    }

    return true;
  }

 private:
  std::vector<unique<Node>> nodes_;
};

class Int : public Node {
 public:
  static NodeKind Kind;

  Int(SourceLocation loc, int32_t val) : Node(Kind, loc), val_(val) {}
  Int(int32_t val) : Int(SourceLocation(), val) {}

  int32_t getVal() const { return val_; }

  bool equals(const Node &other) const override {
    const Int *other_int = other.getAs<Int>();
    if (!other_int) return false;

    return val_ == other_int->getVal();
  }

 private:
  int32_t val_;
};

class Str : public Node {
 public:
  static NodeKind Kind;

  Str(SourceLocation loc, const std::string &val)
      : Node(Kind, loc), val_(val) {}
  Str(const std::string &val) : Node(Kind), val_(val) {}

  const std::string &getVal() const { return val_; }

  bool equals(const Node &other) const override {
    const Str *other_str = other.getAs<Str>();
    if (!other_str) return false;

    return val_ == other_str->getVal();
  }

 private:
  std::string val_;
};

class ID : public Node {
 public:
  static NodeKind Kind;

  ID(SourceLocation loc, const std::string &name)
      : Node(Kind, loc), name_(name) {}
  ID(const std::string &name) : ID(SourceLocation(), name) {}

  const std::string &getName() const { return name_; }

  bool equals(const Node &other) const override {
    const ID *other_id = other.getAs<ID>();
    if (!other_id) return false;

    return name_ == other_id->getName();
  }

 private:
  std::string name_;
};

class Assign : public Node {
 public:
  static NodeKind Kind;

  Assign(SourceLocation loc, unique<Node> dst, unique<Node> src)
      : Node(Kind, loc), dst_(std::move(dst)), src_(std::move(src)) {
    CheckNonNull(src_.get());
    CheckNonNull(dst_.get());
  }
  Assign(unique<Node> dst, unique<Node> src)
      : Assign(SourceLocation(), std::move(dst), std::move(src)) {}

  const Node &getDst() const { return *dst_; }
  const Node &getSrc() const { return *src_; }

  bool equals(const Node &other) const override {
    const Assign *other_assign = other.getAs<Assign>();
    if (!other_assign) return false;

    return (getDst() == other_assign->getDst() &&
            getSrc() == other_assign->getSrc());
  }

 private:
  unique<Node> dst_;
  unique<Node> src_;
};

class Call : public Node {
 public:
  static NodeKind Kind;

  Call(SourceLocation loc, unique<Node> func, std::vector<unique<Node>> args)
      : Node(Kind, loc), func_(std::move(func)), args_(std::move(args)) {
    CheckNonNull(func_.get());
    CheckNonNullVector(args_);
  }
  Call(unique<Node> func, std::vector<unique<Node>> args)
      : Node(Kind), func_(std::move(func)), args_(std::move(args)) {
    CheckNonNull(func_.get());
    CheckNonNullVector(args_);
  }

  const Node &getFunc() const { return *func_; }
  const std::vector<unique<Node>> &getArgs() const { return args_; }

  bool equals(const Node &other) const override {
    const Call *call = other.getAs<Call>();
    if (!call) return false;

    if (*func_ != call->getFunc()) return false;

    if (args_.size() != call->getArgs().size()) return false;

    for (unsigned i = 0; i < args_.size(); ++i) {
      if (*(args_[i]) != *(call->getArgs()[i])) return false;
    }

    return true;
  }

 private:
  unique<Node> func_;
  std::vector<unique<Node>> args_;
};

enum ParseStatusKind {
  PARSE_SUCCESS,

  // Here to act as a dummy failure kind. We know we failed, but do not know the
  // reason.
  PARSE_FAIL,

  // We read an RPAR that did not have a corresponding LPAR.
  PARSE_FAIL_NO_LPAR,

  // We read an LPAR that did not have a corresponding RPAR.
  PARSE_FAIL_NO_RPAR,
};

class ParseStatus {
 public:
  ParseStatusKind getKind() const { return kind_; }
  bool isSuccessful() const { return kind_ == PARSE_SUCCESS; }
  operator bool() const { return isSuccessful(); }

  static ParseStatus GetSuccess();
  static ParseStatus GetFailure(ParseStatusKind kind, SourceLocation loc,
                                Token tok);
  static ParseStatus GetFailure(ParseStatusKind kind, Token tok) {
    return GetFailure(kind, tok.loc, tok);
  }

 private:
  // Does nothing, but we do not want to accidentally create a new ParseStatus
  // without any of the static getters.
  ParseStatus() {}

  ParseStatusKind kind_;

  // These values may not be relevant depending on the status kind. These are
  // left unitialized in these cases.
  SourceLocation loc_;
  Token tok_;
};

ParseStatus ReadModule(const std::vector<Token> &input, Module **module);

}  // namespace lang

#endif
