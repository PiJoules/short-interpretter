#include "Interpret.h"
#include "Lexer.h"
#include "Parser.h"

using lang::ByteCode;
using lang::LexStatus;
using lang::Node;
using lang::ParseStatus;
using lang::Token;
using lang::unique;

namespace {

class Compiler {
 public:
  ~Compiler() {
    if (module_ptr_) delete module_ptr_;
  }

  const std::vector<Token> &getTokens() const { return tokens_; }

  const lang::Module &getModule() const { return *module_ptr_; }

  const lang::ByteCodeEmitter &getEmitter() const { return emitter_; }

  const lang::ByteCodeEvaluator &getEvaluator() const { return eval_; }

  // Perform a standalone lexing of input into a vector of tokens attainable
  // with getTokens(). Repeated calls to this overrides the previous value of
  // getTokens() instead of appending.
  LexStatus Lex(const std::string &input) {
    ResetTokens();
    return ReadTokens(input, tokens_);
  }

  // Perform a standalone parsing of input into a lang::Module * attainable with
  // getModule(). Repeated calls to this overrides the previous value of
  // getModule().
  ParseStatus Parse() {
    ResetModule();
    return lang::ReadModule(tokens_, &module_ptr_);
  }

  void GenerateByteCode() { emitter_.ConvertToByteCode(*module_ptr_); }

  void EvaluateByteCode() {
    eval_.InitializeConstants(emitter_.getConstants());
    eval_.InitializeSymbolTable(emitter_.getSymbols());
    eval_.Interpret(emitter_.getByteCode());
  }

  int64_t ResetAndCompile(const std::string &input) {
    ResetComponents();
    return Compile(input);
  }

 private:
  int64_t Compile(const std::string &input) {
    Run(input);
    return getOnlyEvalResult();
  }

  int64_t getOnlyEvalResult() const {
    assert(eval_.getEvalStack().size() == 1);
    return eval_.getEvalStack().back();
  }

  void ResetComponents() {
    tokens_.clear();
    delete module_ptr_;
    module_ptr_ = nullptr;
    emitter_.ResetComponents();
    eval_.ResetComponents();
  }

  void Run(const std::string &input) {
    assert(Lex(input).isSuccessful());
    assert(Parse().isSuccessful());
    GenerateByteCode();
    EvaluateByteCode();
  }

  void ResetTokens() { tokens_.clear(); }

  void ResetModule() {
    if (module_ptr_) {
      delete module_ptr_;
      module_ptr_ = nullptr;
    }
  }

  std::vector<Token> tokens_;
  lang::Module *module_ptr_ = nullptr;
  lang::ByteCodeEmitter emitter_;
  lang::ByteCodeEvaluator eval_;
};

template <typename T>
void CompareVectors(const std::vector<T> &expected,
                    const std::vector<T> &found) {
  assert(found.size() == expected.size());
  for (unsigned i = 0; i < found.size(); ++i) assert(found[i] == expected[i]);
}

}  // namespace

void ShortTest() {
  // Lexing
  const std::vector<Token> expected_tokens = {
      Token(lang::TOK_LPAR, "("), Token(lang::TOK_ID, "add"),
      Token(lang::TOK_INT, "2"),  Token(lang::TOK_LPAR, "("),
      Token(lang::TOK_ID, "sub"), Token(lang::TOK_INT, "4"),
      Token(lang::TOK_INT, "2"),  Token(lang::TOK_RPAR, ")"),
      Token(lang::TOK_RPAR, ")"), Token(lang::TOK_SEMICOL, ";"),
  };

  const std::string input = "(add 2 (sub 4 2));";
  std::vector<Token> tokens;
  LexStatus lex_status = ReadTokens(input, tokens);
  assert(lex_status.isSuccessful());
  CompareVectors(expected_tokens, tokens);

  // Parsing
  auto sub_call = std::make_unique<lang::BinOp>(lang::BINOP_SUB,
                                                std::make_unique<lang::Int>(4),
                                                std::make_unique<lang::Int>(2));
  auto add_call = std::make_unique<lang::BinOp>(
      lang::BINOP_ADD, std::make_unique<lang::Int>(2), std::move(sub_call));

  auto stmt = std::make_unique<lang::Stmt>(std::move(add_call));

  std::vector<unique<Node>> nodes;
  nodes.push_back(std::move(stmt));
  auto expected_module = std::make_unique<lang::Module>(std::move(nodes));

  lang::Module *module_ptr;
  ParseStatus parse_status = lang::ReadModule(tokens, &module_ptr);
  assert(parse_status.isSuccessful());

  unique<lang::Module> module(module_ptr);
  assert(*module == *expected_module);

  // Byte code emission
  lang::ByteCodeEmitter emitter;
  emitter.ConvertToByteCode(*module);

  const lang::ByteCode expected_codes[] = {
      ByteCode::GetInstr(lang::INSTR_PUSH),
      ByteCode::GetValue(2),
      ByteCode::GetInstr(lang::INSTR_PUSH),
      ByteCode::GetValue(4),
      ByteCode::GetInstr(lang::INSTR_PUSH),
      ByteCode::GetValue(2),
      ByteCode::GetInstr(lang::INSTR_SUB_OP),
      ByteCode::GetInstr(lang::INSTR_ADD_OP),
  };
  unsigned num_codes = sizeof(expected_codes) / sizeof(lang::ByteCode);

  assert(emitter.getByteCode().size() == num_codes);
  for (unsigned i = 0; i < emitter.getByteCode().size(); ++i) {
    auto expected_code = expected_codes[i];
    auto found_code = emitter.getByteCode()[i];
    assert(expected_code == found_code);
  }

  assert(emitter.getConstants().empty());

  // Byte code evaluation
  lang::ByteCodeEvaluator eval(emitter.getConstants(), emitter.getSymbols());
  eval.Interpret(emitter.getByteCode());

  assert(eval.getEvalStack().size() == 1);
  assert(eval.getEvalStack().back() == 4);
}

void ShortTestAssign() {
  const std::string &input = "def x 2;";

  // Lexing
  Compiler compiler;
  LexStatus lex_status = compiler.Lex(input);
  assert(lex_status.isSuccessful());

  const std::vector<Token> expected = {
      Token(lang::TOK_DEF, "def"),
      Token(lang::TOK_ID, "x"),
      Token(lang::TOK_INT, "2"),
      Token(lang::TOK_SEMICOL, ";"),
  };
  CompareVectors(expected, compiler.getTokens());

  // Parsing
  auto dst = std::make_unique<lang::ID>("x");
  auto src = std::make_unique<lang::Int>(2);
  auto assign = std::make_unique<lang::Assign>(std::move(dst), std::move(src));
  auto stmt = std::make_unique<lang::Stmt>(std::move(assign));

  std::vector<unique<Node>> nodes;
  nodes.push_back(std::move(stmt));
  auto expected_module = std::make_unique<lang::Module>(std::move(nodes));

  ParseStatus parse_status = compiler.Parse();
  assert(parse_status.isSuccessful());
  assert(compiler.getModule() == *expected_module);

  // Byte code emission
  compiler.GenerateByteCode();
  assert(compiler.getEmitter().getSymbols().size() == 1);
  uint64_t symbol = compiler.getEmitter().getSymbolID("x");

  const std::vector<lang::ByteCode> expected_bytecode = {
      ByteCode::GetInstr(lang::INSTR_PUSH),  ByteCode::GetValue(symbol),
      ByteCode::GetInstr(lang::INSTR_PUSH),  ByteCode::GetValue(2),
      ByteCode::GetInstr(lang::INSTR_STORE),
  };
  CompareVectors(expected_bytecode, compiler.getEmitter().getByteCode());

  // Byte code evaluation
  compiler.EvaluateByteCode();
  assert(compiler.getEvaluator().getEvalStack().empty());
}

void ShortTestExample() {
  // This is literally just the example in the README.
  const std::string &input = "(add (sub 4 3) 2);";
  assert(Compiler().ResetAndCompile(input) == 3);
}

void ShortTestCompileBackToBack() {
  const std::string &input = "def x 2; (add x 5);";
  Compiler compiler;

  // Lexing
  const std::vector<Token> expected_toks = {
      Token(lang::TOK_DEF, "def"), Token(lang::TOK_ID, "x"),
      Token(lang::TOK_INT, "2"),   Token(lang::TOK_SEMICOL, ";"),

      Token(lang::TOK_LPAR, "("),  Token(lang::TOK_ID, "add"),
      Token(lang::TOK_ID, "x"),    Token(lang::TOK_INT, "5"),
      Token(lang::TOK_RPAR, ")"),  Token(lang::TOK_SEMICOL, ";"),
  };
  int64_t result = compiler.ResetAndCompile(input);
  CompareVectors(expected_toks, compiler.getTokens());

  // Bytecode emission
  assert(compiler.getEmitter().getSymbols().size() == 1);
  uint64_t symbol = compiler.getEmitter().getSymbolID("x");

  const std::vector<lang::ByteCode> expected_bytecode = {
      ByteCode::GetInstr(lang::INSTR_PUSH),   ByteCode::GetValue(symbol),
      ByteCode::GetInstr(lang::INSTR_PUSH),   ByteCode::GetValue(2),
      ByteCode::GetInstr(lang::INSTR_STORE),

      ByteCode::GetInstr(lang::INSTR_LOAD),   ByteCode::GetValue(symbol),
      ByteCode::GetInstr(lang::INSTR_PUSH),   ByteCode::GetValue(5),
      ByteCode::GetInstr(lang::INSTR_ADD_OP),
  };
  CompareVectors(expected_bytecode, compiler.getEmitter().getByteCode());

  assert(result == 7);
}

int main(int argc, char **argv) {
  ShortTest();
  ShortTestExample();
  ShortTestAssign();
  ShortTestCompileBackToBack();
  if (argc < 2) return 0;

  std::string input(argv[1]);
  std::cout << Compiler().ResetAndCompile(input) << std::endl;

  return 0;
}
