#include "Interpret.h"
#include "Lexer.h"
#include "Parser.h"

using lang::ByteCode;
using lang::LexStatus;
using lang::Node;
using lang::ParseStatus;
using lang::Token;
using lang::unique;

void ShortTest() {
  // Lexing
  const Token expected_tokens[] = {
      Token(lang::TOK_LPAR, "("), Token(lang::TOK_ID, "add"),
      Token(lang::TOK_INT, "2"),  Token(lang::TOK_LPAR, "("),
      Token(lang::TOK_ID, "sub"), Token(lang::TOK_INT, "4"),
      Token(lang::TOK_INT, "2"),  Token(lang::TOK_RPAR, ")"),
      Token(lang::TOK_RPAR, ")"),
  };

  const std::string input = "(add 2 (sub 4 2))";
  std::vector<Token> tokens;
  LexStatus lex_status = ReadTokens(input, tokens);
  assert(lex_status.isSuccessful());
  assert(tokens.size() == sizeof(expected_tokens) / sizeof(Token));

  for (unsigned i = 0; i < tokens.size(); ++i)
    assert(tokens[i] == expected_tokens[i]);

  // Parsing
  std::vector<unique<Node>> sub_args;
  sub_args.push_back(std::make_unique<lang::Int>(4));
  sub_args.push_back(std::make_unique<lang::Int>(2));
  auto sub_call = std::make_unique<lang::Call>(
      std::make_unique<lang::ID>("sub"), std::move(sub_args));

  std::vector<unique<Node>> add_args;
  add_args.push_back(std::make_unique<lang::Int>(2));
  add_args.push_back(std::move(sub_call));
  auto add_call = std::make_unique<lang::Call>(
      std::make_unique<lang::ID>("add"), std::move(add_args));

  std::vector<unique<Node>> nodes;
  nodes.push_back(std::move(add_call));
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
  lang::ByteCodeEvaluator eval(emitter.getConstants());
  eval.Interpret(emitter.getByteCode());

  assert(eval.getEvalStack().size() == 1);
  assert(eval.getEvalStack().back() == 4);
}

void Compile(const std::string &input) {
  std::vector<Token> tokens;
  LexStatus lex_status = ReadTokens(input, tokens);
  assert(lex_status.isSuccessful());

  lang::Module *module_ptr;
  ParseStatus parse_status = lang::ReadModule(tokens, &module_ptr);
  assert(parse_status.isSuccessful());

  unique<lang::Module> module(module_ptr);
  lang::ByteCodeEmitter emitter;
  emitter.ConvertToByteCode(*module);

  lang::ByteCodeEvaluator eval(emitter.getConstants());
  eval.Interpret(emitter.getByteCode());
  assert(eval.getEvalStack().size() == 1);

  std::cout << eval.getEvalStack().back() << std::endl;
}

int main(int argc, char **argv) {
  ShortTest();
  if (argc < 2) return 0;

  std::string input(argv[1]);
  Compile(input);

  return 0;
}
