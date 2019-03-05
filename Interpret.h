#ifndef INTERPRET_H
#define INTERPRET_H

#include <cstring>
#include <unordered_map>

#include "Parser.h"

namespace lang {

class ASTVisitor {
 public:
  virtual ~ASTVisitor() {}
  void Visit(const Node &node);

 protected:
  void VisitNodeSequence(const std::vector<unique<Node>> &nodes) {
    for (const auto &node_ptr : nodes) Visit(*node_ptr);
  }

  virtual void VisitModule(const Module &node) {
    VisitNodeSequence(node.getNodes());
  }
  virtual void VisitInt(const Int &) {}
  virtual void VisitStr(const Str &) {}
  virtual void VisitID(const ID &) {}
  virtual void VisitCall(const Call &node) {
    Visit(node.getFunc());
    VisitNodeSequence(node.getArgs());
  }
  virtual void VisitAssign(const Assign &node) {
    Visit(node.getSrc());
    Visit(node.getDst());
  }
  virtual void VisitBinOp(const BinOp &node) {
    Visit(node.getLHS());
    Visit(node.getRHS());
  }
};

enum TypeKind {
  TYPE_INT,
  TYPE_STR,
  TYPE_FUNC,
};

class Type {
 public:
  virtual ~Type() {}

  TypeKind getKind() const { return kind_; }

  template <typename T>
  T *getAs() {
    if (kind_ != T::Kind) return nullptr;
    return static_cast<T *>(this);
  }

  template <typename T>
  const T *getAs() const {
    if (kind_ != T::Kind) return nullptr;
    return static_cast<const T *>(this);
  }

  // Perform a deep copy of this type.
  virtual Type *Copy() const = 0;
  unique<Type> UniqueCopy() const {
    unique<Type> type(Copy());
    return std::move(type);
  }

  bool operator==(const Type &other) const { return equals(other); }
  bool operator!=(const Type &other) const { return !equals(other); }

  virtual bool equals(const Type &other) const = 0;

 protected:
  // We do not want to create a raw type.
  Type(TypeKind kind) : kind_(kind) {}

 private:
  TypeKind kind_;
};

class IntType : public Type {
 public:
  static TypeKind Kind;

  IntType() : Type(Kind) {}

  Type *Copy() const override { return SafeNew<IntType>(); }

  bool equals(const Type &other) const override {
    return other.getKind() == Kind;
  }
};

class StrType : public Type {
 public:
  static TypeKind Kind;

  StrType() : Type(Kind) {}

  Type *Copy() const override { return SafeNew<StrType>(); }

  bool equals(const Type &other) const override {
    return other.getKind() == Kind;
  }
};

class FuncType : public Type {
 public:
  static TypeKind Kind;

  FuncType(unique<Type> ret_type, std::vector<unique<Type>> arg_types)
      : Type(Kind),
        ret_type_(std::move(ret_type)),
        arg_types_(std::move(arg_types)) {
    CheckNonNull(ret_type_.get());
    CheckNonNullVector(arg_types_);
  }

  Type *Copy() const override {
    std::vector<unique<Type>> arg_types;
    for (const auto &arg_ptr : arg_types_) {
      unique<Type> arg_type(arg_ptr->Copy());
      arg_types.push_back(std::move(arg_type));
    }
    return SafeNew<FuncType>(ret_type_->UniqueCopy(), std::move(arg_types));
  }

  bool equals(const Type &other) const override {
    const auto *other_func = other.getAs<FuncType>();
    if (!other_func) return false;

    if (other_func->getNumArgs() != getNumArgs()) return false;

    for (unsigned i = 0; i < getNumArgs(); ++i) {
      if (*(arg_types_[i]) != *(other_func->getArgTypes()[i])) return false;
    }

    return true;
  }

  const Type &getReturnType() const { return *ret_type_; }
  const std::vector<unique<Type>> &getArgTypes() const { return arg_types_; }
  unsigned getNumArgs() const { return arg_types_.size(); }

 private:
  unique<Type> ret_type_;
  std::vector<unique<Type>> arg_types_;
};

class FunctionValue;

/**
 * Represents a value that we can determine during evaluation. This is pretty
 * much the same as a variant.
 */
class Evaluatable {
 public:
  static Evaluatable &&GetInt(int32_t val);
  static Evaluatable &&GetStr(const std::string &val);
  static Evaluatable &&GetFunc(unique<Type> type, unique<FunctionValue> func);

  Evaluatable(const Evaluatable &other);
  Evaluatable operator=(const Evaluatable &other) const {
    Evaluatable new_eval(other);
    return new_eval;
  }
  ~Evaluatable();

  const Type &getType() const { return *type_; }

  bool isIntType() const { return type_->getKind() == TYPE_INT; }
  int32_t getIntVal() const {
    assert(isIntType() &&
           "Cannot get an int value from one that is not an int type");
    return val_.int_val;
  }

  bool isFuncType() const { return type_->getKind() == TYPE_FUNC; }
  FunctionValue &getFunc() const;

  bool isStrType() const { return type_->getKind() == TYPE_STR; }
  const char *getStrID() const {
    assert(isStrType() &&
           "Cannot get a str value from one that is not a str type");
    return val_.str_val.chars;
  }
  size_t getStrLen() const {
    assert(isStrType() &&
           "Cannot get a str length from one that is not a str type");
    return val_.str_val.num_chars;
  }

 private:
  Evaluatable(unique<Type> type) : type_(std::move(type)) {
    CheckNonNull(type_.get());
  }

  static char *MakeChars(const char *src, size_t len) {
    // TODO: Replace this with a safe new that takes into account out of memory
    // errors.
    char *dst = new char[len + 1];

    assert(dst == strncpy(dst, src, len) &&
           "Something wrong with strncpy() since it should return the "
           "destination");
    dst[len] = '\0';
    return dst;
  }

  unique<Type> type_;

  union Value {
    int32_t int_val;

    // Small inline representation of a sequence of characters.
    struct {
      char *chars;
      size_t num_chars;
    } str_val;

    FunctionValue *func_val;  // Every function we find will have a unique id.
  } val_;
};

/**
 * Representation for a function we know at evaluation time.
 */
class FunctionValue {
 public:
  FunctionValue(unique<Type> type) : type_(std::move(type)) {
    CheckNonNull(type_.get());
    assert(type_->getKind() == TYPE_FUNC &&
           "Expected a function type for a function value.");
  }

  virtual ~FunctionValue() {}

  FunctionValue *Copy() const;

  const FuncType &getType() const {
    return *(static_cast<const FuncType *>(type_.get()));
  }

  // The stack we pass can be of any length as long as the values in the last N
  // elements match the types of the arguments this function expects. We are
  // essentially passing the whole value stack by reference here instead of
  // taking a slice which copies the elements.
  //
  // If a function has arguments laid out as:
  //
  //   func `arg1` `arg2` ...
  //
  // then the arguments must be laid out as:
  //
  // ..., `arg1`, `arg2`
  //
  //              ^ stack top
  // ^ stack bottom
  //
  void CheckArgs(const std::vector<Evaluatable> &args) const;
  Evaluatable Evaluate(const std::vector<Evaluatable> &args) const {
    CheckArgs(args);
    return EvaluateImpl(args);
  }

 protected:
  virtual Evaluatable EvaluateImpl(
      const std::vector<Evaluatable> &args) const = 0;

 private:
  unique<Type> type_;
};

/**
 * Bytecode instructions
 */
enum Instruction : int64_t {
  // Add a value onto the evaluation stack.
  INSTR_PUSH,

  // Perform an binary operation on the top 2 elements of the evaluation stack
  // and add push the result to the top of the stack.
  INSTR_ADD_OP,
  INSTR_SUB_OP,

  INSTR_CALL,
  INSTR_STORE,
};

union ByteCode {
  Instruction instr;
  int64_t value;

  static ByteCode GetValue(int64_t val) {
    ByteCode code;
    code.value = val;
    return code;
  }

  static ByteCode GetInstr(Instruction instr) {
    ByteCode code;
    code.instr = instr;
    return code;
  }

  bool operator==(const ByteCode &other) const { return value == other.value; }
  bool operator!=(const ByteCode &other) const { return value != other.value; }
};

class ByteCodeEmitter : public ASTVisitor {
 public:
  void ConvertToByteCode(const Node &node);
  const std::vector<ByteCode> &getByteCode() const { return byte_code_; }
  const std::vector<Evaluatable> &getConstants() const { return constants_; }
  const std::unordered_map<std::string, uint64_t> &getSymbols() const {
    return symbols_;
  }

  uint64_t getSymbolID(const std::string &symbol) const {
    return symbols_.at(symbol);
  }

  void ResetComponents() {
    byte_code_.clear();
    symbols_.clear();
    constants_.clear();
  }

 private:
  void VisitModule(const Module &module) override;
  void VisitInt(const Int &) override;
  void VisitStr(const Str &) override;
  void VisitID(const ID &) override;
  void VisitCall(const Call &) override;
  void VisitAssign(const Assign &) override;
  void VisitBinOp(const BinOp &) override;

  void PushBackInstr(Instruction instr) {
    byte_code_.push_back(ByteCode::GetInstr(instr));
  }

  void PushBackValue(int64_t val) {
    byte_code_.push_back(ByteCode::GetValue(val));
  }

  uint64_t getUniqueConstantID(const std::string &str);

  uint64_t getUniqueSymbolID(const std::string &name) const;
  void makeUniqueSymbolID(const std::string &name);
  bool uniqueSymbolExists(const std::string &name) const;

  std::unordered_map<std::string, uint64_t> symbols_;
  std::vector<ByteCode> byte_code_;
  std::vector<Evaluatable> constants_;
};

class ByteCodeEvaluator {
 public:
  ByteCodeEvaluator(const std::vector<Evaluatable> &constants,
                    const std::unordered_map<std::string, uint64_t> &symbols)
      : constants_(constants) {
    InitializeSymbolTable(symbols);
  }
  ByteCodeEvaluator() {}

  void InitializeConstants(const std::vector<Evaluatable> &constants) {
    constants_ = constants;
  }

  void InitializeSymbolTable(
      const std::unordered_map<std::string, uint64_t> &symbols) {
    for (auto it = symbols.begin(); it != symbols.end(); ++it) {
      symbol_table_[it->second] = 0;
    }
  }

  void ResetComponents() {
    eval_stack_.clear();
    constants_.clear();
    symbol_table_.clear();
  }

  void Interpret(const std::vector<ByteCode> &codes);

  const std::vector<int64_t> &getEvalStack() const { return eval_stack_; }

 private:
  std::vector<int64_t> eval_stack_;
  std::vector<Evaluatable> constants_;
  std::unordered_map<uint64_t, int64_t> symbol_table_;
};

}  // namespace lang

#endif
