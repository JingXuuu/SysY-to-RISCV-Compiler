#ifndef AST_H
#define AST_H

#include <cassert>
#include <cstdio>
#include <iostream>
#include <memory>
#include <cstring>
#include "tools.h"
using namespace std;


// declaration
class BaseAST;
class CompUnitAST;
class CompUnitWrapAST; //l8

class DeclAST; // l4
class ConstDeclAST;// l4
class ConstDefWrapAST; //l4
class ConstDefAST; //l4
class ConstInitValAST; //l4 
class ConstInitValWrapAST; //l9
class ConstArrayIndexAST; //l9
class VarDeclAST; //l4
class VarDefWrapAST; //l4
class VarDefAST; //l4
class InitValAST; //l4
class InitValWrapAST; //l9


class FuncDefAST;
class FuncFParamWrapAST; //l8
class FuncFParamAST; //l8

class BlockAST; //l4
class BlockItemWrapAST; //l4 
class BlockItemAST; //l4

class StmtAST;
class ExpAST;

class LValAST; // l4
class ArrayIndexAST; // l9
class PrimaryExpAST;
class UnaryExpAST;
class FuncRParamWrapAST; // l8
class MulExpAST;
class AddExpAST;
class RelExpAST;
class EqExpAST;
class LAndExpAST;
class LOrExpAST;
class ConstExpAST; // l4


// 所有 AST 的基类
class BaseAST {
 public:
  virtual ~BaseAST() = default;
  virtual void Dump() const = 0;
};

// CompUnit 是 BaseAST
class CompUnitAST : public BaseAST {
  public:
    // 用智能指针管理对象
    std::unique_ptr<CompUnitWrapAST> compunitwrap;
    void Dump() const override ;
};

class CompUnitWrapAST: public BaseAST{
  public:
    enum TAG{FUNC, DECL, EMPTY};
    TAG tag;

    std::unique_ptr<CompUnitWrapAST> compunitwrap;
    std::unique_ptr<FuncDefAST> funcdef;
    std::unique_ptr<DeclAST> decl;

    void Dump() const override;
};


class DeclAST : public BaseAST{
  public:
    enum TAG{CONST, VAR};
    TAG tag;

    std::unique_ptr<ConstDeclAST> constdecl;
    std::unique_ptr<VarDeclAST> vardecl;

    void Dump() const override;
};


class ConstDeclAST : public BaseAST{
  public:
    std::string type;
    std::unique_ptr<ConstDefWrapAST> constdefwrap;

    void Dump() const override;
};


class ConstDefWrapAST : public BaseAST{
  public:
    enum TAG{ONE, MANY};
    TAG tag;

    std::unique_ptr<ConstDefAST> constdef;
    std::unique_ptr<ConstDefWrapAST> constdefwrap;

    void Dump() const override;
};


class ConstDefAST : public BaseAST{
  public:
    enum TAG{VAR, ARRAY};
    TAG tag;

    std::string ident;
    std::unique_ptr<ConstInitValAST> constinitval;
    std::unique_ptr<ConstArrayIndexAST> constarrayindex;

    void Dump() const override;
};

class ConstArrayIndexAST : public BaseAST{
  public:
    enum TAG{ONE, MANY};
    TAG tag;

    std::unique_ptr<ConstExpAST> constexp;
    std::unique_ptr<ConstArrayIndexAST> constarrayindex;

    void Dump() const override;
};


class ConstInitValAST : public BaseAST{
  public:
    enum TAG{EXP, ARRAY};
    TAG tag;
    std::unique_ptr<ConstExpAST> constexp;
    std::unique_ptr<ConstInitValWrapAST> constinitvalwrap;

    void Dump() const override;
    void Calc() const;
};

class ConstInitValWrapAST : public BaseAST{
  public:
    enum TAG{ONE, MANY, EMPTY};
    TAG tag;

    std::unique_ptr<ConstInitValAST> constinitval;
    std::unique_ptr<ConstInitValWrapAST> constinitvalwrap;

    void Dump() const override;
};


class VarDeclAST : public BaseAST{
  public:
    std::string type;
    std::unique_ptr<VarDefWrapAST> vardefwrap;

    void Dump() const override;
};

// VarDefWrap ::= ',' VarDef VarDefWrap | ε
class VarDefWrapAST : public BaseAST{
  public:
    enum TAG{MANY, ONE};
    TAG tag;

    std::unique_ptr<VarDefAST> vardef;
    std::unique_ptr<VarDefWrapAST> vardefwrap;

    void Dump() const override;
};


class VarDefAST : public BaseAST{
  public:
    enum TAG{IDENT, INIT, ARRAY, ARRAY_INIT};
    TAG tag;

    std::string ident;
    std::unique_ptr<InitValAST> initval;
    std::unique_ptr<ConstArrayIndexAST> constarrayindex;

    void Dump() const override;
};


class InitValAST : public BaseAST{
  public:
    enum TAG{EXP, ARRAY};
    TAG tag;

    std::unique_ptr<ExpAST> exp;
    std::unique_ptr<InitValWrapAST> initvalwrap;

    void Dump() const override;
    void Calc() const;
};


class InitValWrapAST : public BaseAST{
  public:
    enum TAG{ONE, MANY, EMPTY};
    TAG tag;

    std::unique_ptr<InitValAST> initval;
    std::unique_ptr<InitValWrapAST> initvalwrap;

    void Dump() const override;
};


// FuncDef 也是 BaseAST
class FuncDefAST : public BaseAST {
  public:
    std::string type;
    std::string ident;
    std::unique_ptr<BlockAST> block;
    std::unique_ptr<FuncFParamWrapAST> fparamwrap;

    void Dump() const override;
};

class FuncFParamWrapAST : public BaseAST{
  public:
    enum TAG{ONE, MANY, EMPTY};
    TAG tag;
    std::unique_ptr<FuncFParamAST> fparam;
    std::unique_ptr<FuncFParamWrapAST> fparamwrap;

    void Dump() const override;
};

class FuncFParamAST : public BaseAST{
  public:
    enum TAG{VAR, ARRAY};
    TAG tag;

    std::string type;
    std::string ident;
    std::unique_ptr<ConstArrayIndexAST> constarrayindex;

    void Dump() const override;
};

// Block ::= '{' BlockItemWrap '}'
class BlockAST : public BaseAST{
  public:
    std::unique_ptr<BlockItemWrapAST> blockitemwrap;

    void Dump() const override;
};

// BlockItemWrap ::= BlockItem BlockItemWrap | ε
class BlockItemWrapAST : public BaseAST{
  public:
    enum TAG{ITEM, EMPTY};
    TAG tag;

    std::unique_ptr<BlockItemAST> blockitem;
    std::unique_ptr<BlockItemWrapAST> blockitemwrap;

    void Dump() const override;
};

// BlockItem ::= Decl | Stmt
class BlockItemAST : public BaseAST{
  public:
    enum TAG{DECL, STMT};
    TAG tag;

    std::unique_ptr<DeclAST> decl;
    std::unique_ptr<StmtAST> stmt;

    void Dump() const override;
};

class StmtAST : public BaseAST{
  public:
    enum TAG{ASSIGN, EMPTY, EXP, BLOCK, IF, IFELSE, WHILE, BREAK, CONTINUE, RETURN};
    TAG tag;

    std::unique_ptr<ExpAST> exp;
    std::unique_ptr<LValAST> lval;
    std::unique_ptr<BlockAST> block;
    std::unique_ptr<StmtAST> stmt;
    std::unique_ptr<StmtAST> elsestmt;


    void Dump() const override;
};


class ExpAST : public BaseAST{
  public:
    std::unique_ptr<LOrExpAST> lorexp;

    void Dump() const override;
    void Calc() const;
};


class LValAST : public BaseAST{
  public:
    enum TAG {IDENT, ARRAY};
    TAG tag;

    std::string ident;
    std::unique_ptr<ArrayIndexAST> arrayindex;

    void Dump() const override;
    void Calc() const;
};

class ArrayIndexAST : public BaseAST{
  public:
    enum TAG{ONE, MANY};
    TAG tag;

    std::unique_ptr<ExpAST> exp;
    std::unique_ptr<ArrayIndexAST> arrayindex;

    void Dump() const override;
    void Calc(std::string ident, int dim, int accumulate) const;
};


class PrimaryExpAST : public BaseAST{
  public:
    enum TAG{BRAC, NUM, LVAL};
    TAG tag;

    // BRAC
    std::unique_ptr<ExpAST> exp;
    //NUM
    int number;
    //LVAL
    std::unique_ptr<LValAST> lval;


    void Dump() const override;
    void Calc() const;

};

//UnaryExp::= PrimaryExp | UnaryOp UnaryExp
class UnaryExpAST : public BaseAST{
  public:
    enum TAG{EXP, OPEXP, CALL};
    TAG tag;

    //EXP
    std::unique_ptr<PrimaryExpAST> priexp;
    //OPEXP
    char op;
    std::unique_ptr<UnaryExpAST> unaexp;
    //CALL
    std::string ident;
    std::unique_ptr<FuncRParamWrapAST> rparamwrap;
    

    void Dump() const override;
    void Calc() const;
};

class FuncRParamWrapAST : public BaseAST{
  public:
    enum TAG{ONE, MANY, EMPTY};
    TAG tag;

    std::unique_ptr<ExpAST> exp;
    std::unique_ptr<FuncRParamWrapAST> rparamwrap;

    void Dump() const override;
};


//MulExp ::= UnaryExp | MulExp ("*" | "/" | "%") UnaryExp;
class MulExpAST : public BaseAST{
  public:
    enum TAG{EXP, OP};
    TAG tag;

    //EXP, MULEXP
    std::unique_ptr<UnaryExpAST> unaexp; // EXP will only has this
    std::unique_ptr<MulExpAST> mulexp;
    char op;

    void Dump() const override;
    void Calc() const;
};

//AddExp::= MulExp | AddExp ("+" | "-") MulExp;
class AddExpAST : public BaseAST{
  public:
    enum TAG{EXP, OP};
    TAG tag;

    //EXP, ADDEXP
    std::unique_ptr<MulExpAST> mulexp; // EXP will only has this
    std::unique_ptr<AddExpAST> addexp;
    char op;

    void Dump() const override;
    void Calc() const;
};

//RelExp::= AddExp | RelExp ("<" | ">" | "<=" | ">=") AddExp;
class RelExpAST : public BaseAST{
  public:
    enum TAG{EXP, OP};
    TAG tag;

    std::unique_ptr<AddExpAST> addexp;
    std::unique_ptr<RelExpAST> relexp;
    std::string op;

    void Dump() const override;
    void Calc() const;
};

//EqExp::= RelExp | EqExp ("==" | "!=") RelExp;
class EqExpAST : public BaseAST{
  public:
    enum TAG{EXP, OP};
    TAG tag;

    std::unique_ptr<RelExpAST> relexp;
    std::unique_ptr<EqExpAST> eqexp;
    std::string op;

    void Dump() const override;
    void Calc() const;
};


//LAndExp::= EqExp | LAndExp "&&" EqExp;
class LAndExpAST : public BaseAST{
  public:
    enum TAG{EXP, OP};
    TAG tag;

    std::unique_ptr<EqExpAST> eqexp;
    std::unique_ptr<LAndExpAST> landexp;
    std::string op;

    void Dump() const override;
    void Calc() const;
};

//LOrExp::= LAndExp | LOrExp "||" LAndExp;
class LOrExpAST : public BaseAST{
  public:
    enum TAG{EXP, OP};
    TAG tag;

    std::unique_ptr<LAndExpAST> landexp;
    std::unique_ptr<LOrExpAST> lorexp;
    std::string op;

    void Dump() const override;
    void Calc() const;
};

//ConstExp::= Exp
class ConstExpAST : public BaseAST{
  public:
    std::unique_ptr<ExpAST> exp;

    void Dump() const override;
    void Calc() const;
};


#endif