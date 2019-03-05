#include <unordered_map>

#include "Interpret.h"

namespace lang {

TypeKind IntType::Kind = TYPE_INT;
TypeKind StrType::Kind = TYPE_STR;
TypeKind FuncType::Kind = TYPE_FUNC;

namespace {

unique<Type> MakeBinaryIntFuncType() {
  std::vector<unique<Type>> arg_types;
  arg_types.push_back(std::make_unique<IntType>());
  arg_types.push_back(std::make_unique<IntType>());
  return std::make_unique<FuncType>(std::make_unique<IntType>(),
                                    std::move(arg_types));
}

}  // namespace

Evaluatable &&Evaluatable::GetInt(int32_t val) {
  Evaluatable value(std::make_unique<IntType>());
  value.val_.int_val = val;
  return std::move(value);
}

Evaluatable &&Evaluatable::GetStr(const std::string &val) {
  Evaluatable value(std::make_unique<StrType>());
  value.val_.str_val = {MakeChars(val.c_str(), val.size()), val.size()};
  return std::move(value);
}

Evaluatable &&Evaluatable::GetFunc(unique<Type> type,
                                   unique<FunctionValue> val) {
  Evaluatable value(std::move(type));
  value.val_.func_val = val.release();
  return std::move(value);
}

Evaluatable::Evaluatable(const Evaluatable &other) {
  type_ = other.type_->UniqueCopy();
  switch (type_->getKind()) {
    case TYPE_STR:
      val_.str_val = {
          MakeChars(other.val_.str_val.chars, other.val_.str_val.num_chars),
          other.val_.str_val.num_chars};
      break;
    case TYPE_FUNC:
      val_.func_val = other.val_.func_val->Copy();
      break;
    case TYPE_INT:
      val_.int_val = other.val_.int_val;
      break;
  }
}

Evaluatable::~Evaluatable() {
  switch (type_->getKind()) {
    case TYPE_STR:
      delete[] val_.str_val.chars;
      break;
    case TYPE_FUNC:
      delete val_.func_val;
      break;
    case TYPE_INT:
      // No custom destruction necessary.
      break;
  }
}

FunctionValue &Evaluatable::getFunc() const {
  assert(isFuncType() &&
         "Cannot get a function from one that is not a function type");
  return *(val_.func_val);
}

FunctionValue *FunctionValue::Copy() const {
  // TODO: Find a way to store functions in an Evaluatable.
  // return SafeNew<FunctionValue>(type_->UniqueCopy());
  return nullptr;
}

void FunctionValue::CheckArgs(const std::vector<Evaluatable> &args) const {
  const FuncType &type = getType();
  assert(args.size() >= type.getNumArgs() &&
         "Not enough arguments provided for this function.");

  for (unsigned i = type.getNumArgs(); i != 0; --i) {
    const Type &expected_arg_type = *(type.getArgTypes()[i - 1]);
    const Type &found_arg_type = args[args.size() - i].getType();
    assert(expected_arg_type == found_arg_type && "Argument type mismatch.");
  }
}

void ASTVisitor::Visit(const Node &node) {
  switch (node.getKind()) {
    case NODE_MODULE:
      return VisitModule(*node.getAs<Module>());
    case NODE_INT:
      return VisitInt(*node.getAs<Int>());
    case NODE_STR:
      return VisitStr(*node.getAs<Str>());
    case NODE_ID:
      return VisitID(*node.getAs<ID>());
    case NODE_CALL:
      return VisitCall(*node.getAs<Call>());
    case NODE_ASSIGN:
      return VisitAssign(*node.getAs<Assign>());
  }
}

void ByteCodeEmitter::ConvertToByteCode(const Node &node) { Visit(node); }

void ByteCodeEmitter::VisitModule(const Module &module) {
  for (const auto &node_ptr : module.getNodes()) Visit(*node_ptr);
}

void ByteCodeEmitter::VisitInt(const Int &node) {
  PushBackInstr(INSTR_PUSH);
  PushBackValue(node.getVal());
}

uint64_t ByteCodeEmitter::getUniqueConstantID(const std::string &str) {
  uint64_t str_id = constants_.size();
  constants_.push_back(Evaluatable::GetStr(str));
  return str_id;
}

uint64_t ByteCodeEmitter::getUniqueSymbolID(const std::string &name) const {
  assert(uniqueSymbolExists(name) &&
         "Attempting to get a symbol that does not yet exist");
  return symbols_.at(name);
}

bool ByteCodeEmitter::uniqueSymbolExists(const std::string &name) const {
  return symbols_.find(name) != symbols_.end();
}

void ByteCodeEmitter::makeUniqueSymbolID(const std::string &name) {
  assert(!uniqueSymbolExists(name) &&
         "Cannot remake a symbol that already exists.");
  uint64_t new_id = symbols_.size();
  symbols_[name] = new_id;
}

void ByteCodeEmitter::VisitStr(const Str &node) {
  PushBackInstr(INSTR_PUSH);

  uint64_t str_id = constants_.size();
  constants_.push_back(Evaluatable::GetStr(node.getVal()));
  PushBackValue(str_id);
}

void ByteCodeEmitter::VisitID(const ID &node) {
  // Check builtin operations.
  Instruction instr;
  if (node.getName() == "add") {
    instr = INSTR_ADD_OP;
  } else if (node.getName() == "sub") {
    instr = INSTR_SUB_OP;
  } else {
    // Load from the symbol table
    uint64_t symbol = getUniqueSymbolID(node.getName());
    PushBackInstr(INSTR_PUSH);
    PushBackValue(symbol);
    return;
  }

  // Call to custom function.
  PushBackInstr(instr);
}

void ByteCodeEmitter::VisitAssign(const Assign &node) {
  // First declaration of this variable. May need to add it to the set of known
  // symbols.
  if (const auto *id_node = node.getDst().getAs<ID>()) {
    const std::string &name = id_node->getName();
    if (!uniqueSymbolExists(name)) makeUniqueSymbolID(name);
  }
  Visit(node.getDst());

  Visit(node.getSrc());

  PushBackInstr(INSTR_STORE);
}

void ByteCodeEmitter::VisitCall(const Call &node) {
  VisitNodeSequence(node.getArgs());
  Visit(node.getFunc());
}

void ByteCodeEvaluator::Interpret(const std::vector<ByteCode> &codes) {
  int64_t i = 0;
  while (i < codes.size()) {
    const ByteCode &code = codes[i];

    // This is always an instruction
    switch (code.instr) {
      case INSTR_PUSH:
        assert(i <= codes.size() - 1 && "Expected at least one more code");
        eval_stack_.push_back(codes[i + 1].value);

        SafeSignedInplaceAdd(i, 2);
        break;
      case INSTR_ADD_OP: {
        assert(eval_stack_.size() >= 2 &&
               "Expected at least 2 values on the eval stack.");
        int64_t rhs = eval_stack_.back();
        eval_stack_.pop_back();
        int64_t lhs = eval_stack_.back();
        eval_stack_.pop_back();
        eval_stack_.push_back(lhs + rhs);

        SafeSignedInc(i);
        break;
      }
      case INSTR_SUB_OP: {
        assert(eval_stack_.size() >= 2 &&
               "Expected at least 2 values on the eval stack.");
        int64_t rhs = eval_stack_.back();
        eval_stack_.pop_back();
        int64_t lhs = eval_stack_.back();
        eval_stack_.pop_back();
        eval_stack_.push_back(lhs - rhs);

        SafeSignedInc(i);
        break;
      }
      case INSTR_CALL:
        assert(0 && "Calls not yet supported");
        break;
      case INSTR_STORE:
        assert(eval_stack_.size() >= 2 &&
               "Expected at least 2 values on the eval stack.");
        int64_t val = eval_stack_.back();
        eval_stack_.pop_back();

        uint64_t dst_id = eval_stack_.back();
        eval_stack_.pop_back();

        assert(symbol_table_.find(dst_id) != symbol_table_.end() &&
               "Found unknown symbol ID");
        symbol_table_[dst_id] = val;

        SafeSignedInc(i);
        break;
    }
  }
}

}  // namespace lang
