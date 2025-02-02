#include <cassert>
#include <cstdio>
#include <iostream>
#include <memory>
#include <cstring>
#include "ast.h"
#include "visit.h"
#include "tools.h"


using namespace std;



// 声明 lexer 的输入, 以及 parser 函数
// 为什么不引用 sysy.tab.hpp 呢? 因为首先里面没有 yyin 的定义
// 其次, 因为这个文件不是我们自己写的, 而是被 Bison 生成出来的
// 你的代码编辑器/IDE 很可能找不到这个文件, 然后会给你报错 (虽然编译不会出错)
// 看起来会很烦人, 于是干脆采用这种看起来 dirty 但实际很有效的手段
extern FILE *yyin, *yyout;
extern int yyparse(unique_ptr<BaseAST> &ast);
extern KoopaIR ki;
extern RiscV rv;

int main(int argc, const char *argv[]) {
  // 解析命令行参数. 测试脚本/评测平台要求你的编译器能接收如下参数:
  // compiler 模式 输入文件 -o 输出文件
  assert(argc == 5);
  auto mode = argv[1];
  auto input = argv[2];
  auto output = argv[4];

  // 打开输入文件, 并且指定 lexer 在解析的时候读取这个文件
  yyin = fopen(input, "r");
  assert(yyin);


  // 调用 parser 函数, parser 函数会进一步调用 lexer 解析输入文件的
  unique_ptr<BaseAST> ast;
  auto rett = yyparse(ast);
  assert(!rett);

  // 输出解析得到的 AST, 其实就是个字符串
  ast->Dump();
  std::string str = ki.getir(); 

  printf("AST-----------------------------------\n");
  printf("%s", str.c_str());
  printf("--------------------------------------\n");
  
  if(std::strcmp(mode, "-koopa") == 0){
    //do nothing as dump is already in koopa form
  }
  else if(std::strcmp(mode, "-riscv") == 0){
    // 解析字符串 str, 得到 Koopa IR 程序
    koopa_program_t program;
    koopa_error_code_t ret = koopa_parse_from_string(str.c_str(), &program);
    assert(ret == KOOPA_EC_SUCCESS);  // 确保解析时没有出错

    // 创建一个 raw program builder, 用来构建 raw program
    koopa_raw_program_builder_t builder = koopa_new_raw_program_builder();
    // 将 Koopa IR 程序转换为 raw program
    koopa_raw_program_t raw = koopa_build_raw_program(builder, program);
    // 释放 Koopa IR 程序占用的内存
    koopa_delete_program(program);

    // main process of raw
    Visit(raw);
    str = rv.getriscv();
    printf("RISCV-----------------------------------\n");
    printf("%s",str.c_str());
    printf("----------------------------------------\n");

    // 处理完成, 释放 raw program builder 占用的内存
    // 注意, raw program 中所有的指针指向的内存均为 raw program builder 的内存
    // 所以不要在 raw program 处理完毕之前释放 builder
    koopa_delete_raw_program_builder(builder);
  }


  //打开输出到文件
  yyout = fopen(output, "w");
  fprintf(yyout, "%s", str.c_str());



  //关闭文件
  fclose(yyin);
  fclose(yyout);
  return 0;
}

