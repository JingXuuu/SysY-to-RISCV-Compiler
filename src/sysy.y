%code requires {
  #include <memory>
  #include <string>
  #include "ast.h"
}

%{

#include <iostream>
#include <memory>
#include <string>
#include "ast.h"

/******************* 声明 lexer 函数和错误处理函数 *******************/
int yylex();
void yyerror(std::unique_ptr<BaseAST> &ast, const char *s);

using namespace std;

%}

/******************* 定义 parser 函数和错误处理函数的附加参数 *******************/
// 把附加参数定义成字符串的智能指针
// 解析完成后, 我们要手动修改这个参数, 把它设置成解析得到的字符串
%parse-param { std::unique_ptr<BaseAST> &ast }


/******************* yylval 的定义, 我们把它定义成了一个联合体 (union) *******************/
%union {
  std::string *str_val;
  int int_val;
  char char_val;
  BaseAST *ast_val;
}

/******************* token 设置 *******************/
// lexer 返回的所有 token 种类的声明
// 注意 IDENT 和 INT_CONST 会返回 token 的值, 分别对应 str_val 和 int_val
// token 必须在sysy.l进行解释
%token RETURN LANDOP LOROP CONST IF ELSE WHILE BREAK CONTINUE
%token <str_val> IDENT RELOP EQOP TYPE
%token <int_val> INT_CONST

/******************* 非终结符的类型定义 *******************/
%type <ast_val> CompUnit CompUnitWrap FuncDef Block Stmt Exp PrimaryExp UnaryExp MulExp AddExp RelExp EqExp LAndExp LOrExp Decl ConstDecl ConstDefWrap ConstDef ConstInitVal VarDecl VarDefWrap VarDef InitVal ConstExp LVal BlockItem BlockItemWrap
%type <ast_val> FuncFParamWrap FuncFParam FuncRParamWrap
%type <ast_val> ConstInitValWrap InitValWrap ConstArrayIndex ArrayIndex
%type <int_val> Number
%type <char_val> UnaryOp

/******************* if else 优先级设置 *******************/
%precedence IFX
%precedence ELSE



%%
/******************* 开始 *******************/


// CompUnit ::= FuncDef, 大括号后声明了解析完成后 parser 要做的事情
// 之前我们定义了 FuncDef 会返回一个 str_val, 也就是字符串指针
// 而 parser 一旦解析完 CompUnit, 就说明所有的 token 都被解析了, 即解析结束了
// 此时我们应该把 FuncDef 返回的结果收集起来, 作为 AST 传给调用 parser 的函数
// $1 指代规则里第一个符号的返回值, 也就是 FuncDef 的返回值
CompUnit
  : CompUnitWrap{
    auto compunit = make_unique<CompUnitAST>();
    compunit->compunitwrap = unique_ptr<CompUnitWrapAST>((CompUnitWrapAST *)$1);
    ast = move(compunit);
  }
  ;


CompUnitWrap
  : FuncDef CompUnitWrap{
    auto ast = new CompUnitWrapAST();
    ast->compunitwrap = unique_ptr<CompUnitWrapAST>((CompUnitWrapAST *)$2);
    ast->funcdef = unique_ptr<FuncDefAST>((FuncDefAST *)$1);
    ast->tag = CompUnitWrapAST::FUNC;
    $$ = ast;
  }
  | Decl CompUnitWrap{
    auto ast = new CompUnitWrapAST();
    ast->compunitwrap = unique_ptr<CompUnitWrapAST>((CompUnitWrapAST *)$2);
    ast->decl = unique_ptr<DeclAST>((DeclAST *)$1);
    ast->tag = CompUnitWrapAST::DECL;
    $$ = ast;
  }
  | {
    auto ast = new CompUnitWrapAST();
    ast->tag = CompUnitWrapAST::EMPTY;
    $$ = ast;
  }
  ;


Decl         
  : ConstDecl{
    auto ast = new DeclAST();
    ast->constdecl = unique_ptr<ConstDeclAST>((ConstDeclAST *)$1);
    ast->tag = DeclAST::CONST;
    $$ = ast;
  }
  | VarDecl{
    auto ast = new DeclAST();
    ast->vardecl = unique_ptr<VarDeclAST>((VarDeclAST *)$1);
    ast->tag = DeclAST::VAR;
    $$ = ast;
  }
  ;


ConstDecl     
  : CONST TYPE ConstDefWrap ';'{
    auto ast = new ConstDeclAST();
    ast->type = *unique_ptr<string>($2);
    ast->constdefwrap = unique_ptr<ConstDefWrapAST>((ConstDefWrapAST *)$3);    
    $$ = ast;
  }
  ;


ConstDefWrap
  : ConstDef ',' ConstDefWrap{
    auto ast = new ConstDefWrapAST();
    ast->constdef = unique_ptr<ConstDefAST>((ConstDefAST *)$1);
    ast->constdefwrap = unique_ptr<ConstDefWrapAST>((ConstDefWrapAST *)$3);
    ast->tag = ConstDefWrapAST::MANY;
    $$ = ast;
  }
  | ConstDef{
    auto ast = new ConstDefWrapAST();
    ast->constdef = unique_ptr<ConstDefAST>((ConstDefAST *)$1);
    ast->tag = ConstDefWrapAST::ONE;
    $$ = ast;
  }


ConstDef      
  : IDENT '=' ConstInitVal{
    auto ast = new ConstDefAST();
    ast->tag = ConstDefAST::VAR;
    ast->ident = *unique_ptr<string>($1);
    ast->constinitval = unique_ptr<ConstInitValAST>((ConstInitValAST *)$3);
    $$ = ast;
  }
  | IDENT ConstArrayIndex '=' ConstInitVal{
    auto ast = new ConstDefAST();
    ast->tag = ConstDefAST::ARRAY;
    ast->ident = *unique_ptr<string>($1);
    ast->constarrayindex = unique_ptr<ConstArrayIndexAST>((ConstArrayIndexAST *)$2);
    ast->constinitval = unique_ptr<ConstInitValAST>((ConstInitValAST *)$4);
    $$ = ast;
  }
  ;

ConstArrayIndex
  : '[' ConstExp ']' ConstArrayIndex{
    auto ast = new ConstArrayIndexAST();
    ast->constexp = unique_ptr<ConstExpAST>((ConstExpAST *)$2);
    ast->constarrayindex = unique_ptr<ConstArrayIndexAST>((ConstArrayIndexAST *)$4);
    ast->tag = ConstArrayIndexAST::MANY;
    $$ = ast;
  }
  | '[' ConstExp ']'{
    auto ast = new ConstArrayIndexAST();
    ast->constexp = unique_ptr<ConstExpAST>((ConstExpAST *)$2);
    ast->tag = ConstArrayIndexAST::ONE;
    $$ = ast;
  }
  ;

ConstInitVal  
  : ConstExp{
    auto ast = new ConstInitValAST();
    ast->tag = ConstInitValAST::EXP;
    ast->constexp = unique_ptr<ConstExpAST>((ConstExpAST *)$1);
    $$ = ast;
  }
  | '{' ConstInitValWrap '}'{
    auto ast = new ConstInitValAST();
    ast->tag = ConstInitValAST::ARRAY;
    ast->constinitvalwrap = unique_ptr<ConstInitValWrapAST>((ConstInitValWrapAST *)$2);
    $$ = ast;
  }
  ;


ConstInitValWrap
  : ConstInitVal ',' ConstInitValWrap{
    auto ast = new ConstInitValWrapAST();
    ast->constinitval = unique_ptr<ConstInitValAST>((ConstInitValAST *)$1);
    ast->constinitvalwrap = unique_ptr<ConstInitValWrapAST>((ConstInitValWrapAST *)$3);
    ast->tag = ConstInitValWrapAST::MANY;
    $$ = ast;
  }
  | ConstInitVal{
    auto ast = new ConstInitValWrapAST();
    ast->constinitval = unique_ptr<ConstInitValAST>((ConstInitValAST *)$1);
    ast->tag = ConstInitValWrapAST::ONE;
    $$ = ast;
  }
  | {
    auto ast = new ConstInitValWrapAST();
    ast->tag = ConstInitValWrapAST::EMPTY;
    $$ = ast;
  }
  ;


VarDecl       
  : TYPE VarDefWrap ';'{
    auto ast = new VarDeclAST();
    ast->type = *unique_ptr<string>($1);
    ast->vardefwrap = unique_ptr<VarDefWrapAST>((VarDefWrapAST *)$2);
    $$ = ast;
  }
  ;


VarDefWrap
  : VarDef ',' VarDefWrap{
    auto ast = new VarDefWrapAST();
    ast->vardef = unique_ptr<VarDefAST>((VarDefAST *)$1);
    ast->vardefwrap = unique_ptr<VarDefWrapAST>((VarDefWrapAST *)$3);
    ast->tag = VarDefWrapAST::MANY;
    $$ = ast;
  }
  | VarDef{
    auto ast = new VarDefWrapAST();
    ast->vardef = unique_ptr<VarDefAST>((VarDefAST *)$1);
    ast->tag = VarDefWrapAST::ONE;
    $$ = ast;
  }
  ;


VarDef
  : IDENT{
    auto ast = new VarDefAST();
    ast->ident = *unique_ptr<string>($1);
    ast->tag = VarDefAST::IDENT;
    $$ = ast;
  }
  | IDENT '=' InitVal{
    auto ast = new VarDefAST();
    ast->ident = *unique_ptr<string>($1);
    ast->initval = unique_ptr<InitValAST>((InitValAST *)$3);
    ast->tag = VarDefAST::INIT;
    $$ = ast;
  }
  | IDENT ConstArrayIndex{
    auto ast = new VarDefAST();
    ast->ident = *unique_ptr<string>($1);
    ast->constarrayindex = unique_ptr<ConstArrayIndexAST>((ConstArrayIndexAST *)$2);
    ast->tag = VarDefAST::ARRAY;
    $$ = ast;
  }
  | IDENT ConstArrayIndex '=' InitVal {
    auto ast = new VarDefAST();
    ast->ident = *unique_ptr<string>($1);
    ast->constarrayindex = unique_ptr<ConstArrayIndexAST>((ConstArrayIndexAST *)$2);
    ast->initval = unique_ptr<InitValAST>((InitValAST *)$4);
    ast->tag = VarDefAST::ARRAY_INIT;
    $$ = ast;
  }
  ;


InitVal
  : Exp{
    auto ast = new InitValAST();
    ast->tag = InitValAST::EXP;
    ast->exp = unique_ptr<ExpAST>((ExpAST *)$1);
    $$ = ast;
  }
  | '{' InitValWrap '}' {
    auto ast = new InitValAST();
    ast->tag = InitValAST::ARRAY;
    ast->initvalwrap = unique_ptr<InitValWrapAST>((InitValWrapAST *)$2);
    $$ = ast;
  }
  ;


InitValWrap
  : InitVal ',' InitValWrap{
    auto ast = new InitValWrapAST();
    ast->initval = unique_ptr<InitValAST>((InitValAST *)$1);
    ast->initvalwrap = unique_ptr<InitValWrapAST>((InitValWrapAST *)$3);
    ast->tag = InitValWrapAST::MANY;
    $$ = ast;
  }
  | InitVal{
    auto ast = new InitValWrapAST();
    ast->initval = unique_ptr<InitValAST>((InitValAST *)$1);
    ast->tag = InitValWrapAST::ONE;
    $$ = ast;
  }
  | {
    auto ast = new InitValWrapAST();
    ast->tag = InitValWrapAST::EMPTY;
    $$ = ast;
  }
  ;


// FuncDef ::= FuncType IDENT '(' ')' Block;
// 解析完成后, 把这些符号的结果收集起来, 然后拼成一个新的字符串, 作为结果返回
// $$ 表示非终结符的返回值, 我们可以通过给这个符号赋值的方法来返回结果
FuncDef
  : TYPE IDENT '(' FuncFParamWrap ')' Block {
    auto ast = new FuncDefAST();
    ast->type = *unique_ptr<string>($1);
    ast->ident = *unique_ptr<string>($2);
    ast->fparamwrap = unique_ptr<FuncFParamWrapAST>((FuncFParamWrapAST *)$4);
    ast->block = unique_ptr<BlockAST>((BlockAST *)$6);
    $$ = ast;
  }
  ;


FuncFParamWrap
  : FuncFParam ',' FuncFParamWrap{
    auto ast = new FuncFParamWrapAST();
    ast->fparam = unique_ptr<FuncFParamAST>((FuncFParamAST *)$1);
    ast->fparamwrap = unique_ptr<FuncFParamWrapAST>((FuncFParamWrapAST *)$3);
    ast->tag = FuncFParamWrapAST::MANY;
    $$ = ast;
  }
  | FuncFParam {
    auto ast = new FuncFParamWrapAST();
    ast->fparam = unique_ptr<FuncFParamAST>((FuncFParamAST *)$1);
    ast->tag = FuncFParamWrapAST::ONE;
    $$ = ast;
  }
  | {
    auto ast = new FuncFParamWrapAST();
    ast->tag = FuncFParamWrapAST::EMPTY;
    $$ = ast;
  }
  ;


FuncFParam
  : TYPE IDENT{
    auto ast = new FuncFParamAST();
    ast->type = *unique_ptr<string>($1);
    ast->ident = *unique_ptr<string>($2);
    ast->tag = FuncFParamAST::VAR;
    $$ = ast;
  }
  | TYPE IDENT '[' ']'{
    auto ast = new FuncFParamAST();
    ast->type = *unique_ptr<string>($1);
    ast->ident = *unique_ptr<string>($2);
    ast->tag = FuncFParamAST::ARRAY;
    $$ = ast;
  }
  | TYPE IDENT '[' ']' ConstArrayIndex {
    auto ast = new FuncFParamAST();
    ast->type = *unique_ptr<string>($1);
    ast->ident = *unique_ptr<string>($2);
    ast->constarrayindex = unique_ptr<ConstArrayIndexAST>((ConstArrayIndexAST *)$5);
    ast->tag = FuncFParamAST::ARRAY;
    $$ = ast;
  }
  ;


Block
  : '{' BlockItemWrap '}' {
    auto ast = new BlockAST();
    ast->blockitemwrap = unique_ptr<BlockItemWrapAST>((BlockItemWrapAST *)$2);
    $$ = ast;
  }
  ;

//BlockItemWrap can be ntg or repeat many times
BlockItemWrap
  : BlockItem BlockItemWrap{
    auto ast = new BlockItemWrapAST();
    ast->blockitem = unique_ptr<BlockItemAST>((BlockItemAST *)$1);
    ast->blockitemwrap = unique_ptr<BlockItemWrapAST>((BlockItemWrapAST *)$2);
    ast->tag = BlockItemWrapAST::ITEM;
    $$ = ast;
  }
  | {
    auto ast = new BlockItemWrapAST();
    ast->tag = BlockItemWrapAST::EMPTY;
    $$ = ast;
  }
  ;

BlockItem
  : Decl{
    auto ast = new BlockItemAST();
    ast->decl = unique_ptr<DeclAST>((DeclAST *)$1);
    ast->tag = BlockItemAST::DECL;
    $$ = ast;
  } 
  | Stmt{
    auto ast = new BlockItemAST();
    ast->stmt = unique_ptr<StmtAST>((StmtAST *)$1);
    ast->tag = BlockItemAST::STMT;
    $$ = ast;
  }
  ;




Stmt
  : LVal '=' Exp ';' {
    auto ast = new StmtAST();
    ast->lval = unique_ptr<LValAST>((LValAST *)$1);
    ast->exp = unique_ptr<ExpAST>((ExpAST *)$3);
    ast->tag = StmtAST::ASSIGN;
    $$ = ast;
  }
  | ';'{
    auto ast = new StmtAST();
    ast->tag = StmtAST::EMPTY;
    $$ = ast;
  }
  | Exp ';' {
    auto ast = new StmtAST();
    ast->exp = unique_ptr<ExpAST>((ExpAST *)$1);
    ast->tag = StmtAST::EXP;
    $$ = ast;
  }
  | Block {
    auto ast = new StmtAST();
    ast->block = unique_ptr<BlockAST>((BlockAST *)$1);
    ast->tag = StmtAST::BLOCK;
    $$ = ast;
  }
  | IF '(' Exp ')' Stmt %prec IFX{
    auto ast = new StmtAST();
    ast->exp = unique_ptr<ExpAST>((ExpAST *)$3);
    ast->stmt = unique_ptr<StmtAST>((StmtAST *)$5);
    ast->tag = StmtAST::IF;
    $$ = ast;
  }
  | IF '(' Exp ')' Stmt ELSE Stmt{
    auto ast = new StmtAST();
    ast->exp = unique_ptr<ExpAST>((ExpAST *)$3);
    ast->stmt = unique_ptr<StmtAST>((StmtAST *)$5);
    ast->elsestmt = unique_ptr<StmtAST>((StmtAST *)$7);
    ast->tag = StmtAST::IFELSE;
    $$ = ast;
  }
  | WHILE '(' Exp ')' Stmt{
    auto ast = new StmtAST();
    ast->exp = unique_ptr<ExpAST>((ExpAST *)$3);
    ast->stmt = unique_ptr<StmtAST>((StmtAST *)$5);
    ast->tag = StmtAST::WHILE;
    $$ = ast;
  }
  | BREAK ';'{
    auto ast = new StmtAST();
    ast->tag = StmtAST::BREAK;
    $$ = ast;

  }
  | CONTINUE ';'{
    auto ast = new StmtAST();
    ast->tag = StmtAST::CONTINUE;
    $$ = ast;

  }
  | RETURN Exp ';' {
    auto ast = new StmtAST();
    ast->exp = unique_ptr<ExpAST>((ExpAST *)$2);
    ast->tag = StmtAST::RETURN;
    $$ = ast;
  }
  | RETURN ';'{
    auto ast = new StmtAST();
    ast->tag = StmtAST::RETURN;
    $$ = ast;
  }
  ;




Exp         
  : LOrExp {
    auto ast = new ExpAST();
    ast->lorexp = unique_ptr<LOrExpAST>((LOrExpAST *)$1);
    $$ = ast;
  }
  ;



LVal         
  : IDENT{
    auto ast = new LValAST();
    ast->tag = LValAST::IDENT;
    ast->ident = *unique_ptr<string>($1);
    $$ = ast;
  }
  | IDENT ArrayIndex{
    auto ast = new LValAST();
    ast->tag = LValAST::ARRAY;
    ast->ident = *unique_ptr<string>($1);
    ast->arrayindex = unique_ptr<ArrayIndexAST>((ArrayIndexAST *)$2);
    $$ = ast;
  }
  ;

ArrayIndex
  : '[' Exp ']' ArrayIndex{
    auto ast = new ArrayIndexAST();
    ast->exp = unique_ptr<ExpAST>((ExpAST *)$2);
    ast->arrayindex = unique_ptr<ArrayIndexAST>((ArrayIndexAST *)$4);
    ast->tag = ArrayIndexAST::MANY;
    $$ = ast;

  }
  | '[' Exp ']'{
    auto ast = new ArrayIndexAST();
    ast->exp = unique_ptr<ExpAST>((ExpAST *)$2);
    ast->tag = ArrayIndexAST::ONE;
    $$ = ast;
  }
  ;


PrimaryExp  
  : '(' Exp ')'{
    auto ast = new PrimaryExpAST();
    ast->tag = PrimaryExpAST::BRAC;
    ast->exp = unique_ptr<ExpAST>((ExpAST *)$2);
    $$ = ast;

  }
  | LVal{
    auto ast = new PrimaryExpAST();
    ast->tag = PrimaryExpAST::LVAL;
    ast->lval = unique_ptr<LValAST>((LValAST *)$1);
    $$ = ast;

  }
  | Number{
    auto ast = new PrimaryExpAST();
    ast->tag = PrimaryExpAST::NUM;
    ast->number = $1;
    $$ = ast;
  }
  ;

UnaryExp    
  : PrimaryExp {
    auto ast = new UnaryExpAST();
    ast->tag = UnaryExpAST::EXP;
    ast->priexp = unique_ptr<PrimaryExpAST>((PrimaryExpAST *)$1);
    $$ = ast;
  }
  | IDENT '(' FuncRParamWrap ')'{
    auto ast = new UnaryExpAST();
    ast->tag = UnaryExpAST::CALL;
    ast->ident = *unique_ptr<string>($1);
    ast->rparamwrap = unique_ptr<FuncRParamWrapAST>((FuncRParamWrapAST *)$3);
    $$ = ast;
  }
  | UnaryOp UnaryExp{
    auto ast = new UnaryExpAST();
    ast->tag = UnaryExpAST::OPEXP;
    ast->op = $1;
    ast->unaexp = unique_ptr<UnaryExpAST>((UnaryExpAST *)$2);
    $$ = ast;
  }
  ;

FuncRParamWrap
  : Exp ',' FuncRParamWrap{
    auto ast = new FuncRParamWrapAST();
    ast->exp = unique_ptr<ExpAST>((ExpAST *)$1);
    ast->rparamwrap = unique_ptr<FuncRParamWrapAST>((FuncRParamWrapAST *)$3);
    ast->tag = FuncRParamWrapAST::MANY;
    $$ = ast;
  }
  | Exp{
    auto ast = new FuncRParamWrapAST();
    ast->exp = unique_ptr<ExpAST>((ExpAST *)$1);
    ast->tag = FuncRParamWrapAST::ONE;
    $$ = ast;
    
  }
  | {
    auto ast = new FuncRParamWrapAST();
    ast->tag = FuncRParamWrapAST::EMPTY;
    $$ = ast;
  }
  ;

UnaryOp     
  : '+' {
    $$ = '+';
  }
  | '-' {
    $$ = '-';
  }
  | '!' {
    $$ = '!';
  }
  ;

Number
  : INT_CONST {
    $$ = $1;
  }
  ;

MulExp      
  : UnaryExp{
    auto ast = new MulExpAST();
    ast->tag = MulExpAST::EXP;
    ast->unaexp = unique_ptr<UnaryExpAST>((UnaryExpAST *)$1);
    $$ = ast;
  } 
  | MulExp '*' UnaryExp{
    auto ast = new MulExpAST();
    ast->tag = MulExpAST::OP;
    ast->mulexp = unique_ptr<MulExpAST>((MulExpAST *)$1);
    ast->op = '*';
    ast->unaexp = unique_ptr<UnaryExpAST>((UnaryExpAST *)$3);
    $$ = ast;
  }
  | MulExp '/' UnaryExp{
    auto ast = new MulExpAST();
    ast->tag = MulExpAST::OP;
    ast->mulexp = unique_ptr<MulExpAST>((MulExpAST *)$1);
    ast->op = '/';
    ast->unaexp = unique_ptr<UnaryExpAST>((UnaryExpAST *)$3);
    $$ = ast;
  }
  | MulExp '%' UnaryExp{
    auto ast = new MulExpAST();
    ast->tag = MulExpAST::OP;
    ast->mulexp = unique_ptr<MulExpAST>((MulExpAST *)$1);
    ast->op = '%';
    ast->unaexp = unique_ptr<UnaryExpAST>((UnaryExpAST *)$3);
    $$ = ast;
  }
  ;


AddExp      
  : MulExp {
    auto ast = new AddExpAST();
    ast->tag = AddExpAST::EXP;
    ast->mulexp = unique_ptr<MulExpAST>((MulExpAST *)$1);
    $$ = ast;
  }
  | AddExp '+' MulExp {
    auto ast = new AddExpAST();
    ast->tag = AddExpAST::OP;
    ast->addexp = unique_ptr<AddExpAST>((AddExpAST *)$1);
    ast->op = '+';
    ast->mulexp = unique_ptr<MulExpAST>((MulExpAST *)$3);
    $$ = ast;
  }
  | AddExp '-' MulExp {
    auto ast = new AddExpAST();
    ast->tag = AddExpAST::OP;
    ast->addexp = unique_ptr<AddExpAST>((AddExpAST *)$1);
    ast->op = '-';
    ast->mulexp = unique_ptr<MulExpAST>((MulExpAST *)$3);
    $$ = ast;
  }
  ;

RelExp      
  : AddExp {
    auto ast = new RelExpAST();
    ast->tag = RelExpAST::EXP;
    ast->addexp = unique_ptr<AddExpAST>((AddExpAST *)$1);
    $$ = ast;

  }
  | RelExp RELOP AddExp{
    auto ast = new RelExpAST();
    ast->tag = RelExpAST::OP;
    ast->relexp = unique_ptr<RelExpAST>((RelExpAST *)$1);
    ast->op = *unique_ptr<string>($2);
    ast->addexp = unique_ptr<AddExpAST>((AddExpAST *)$3);
    $$ = ast;

  }
  ;


EqExp       
  : RelExp {
    auto ast = new EqExpAST();
    ast->tag = EqExpAST::EXP;
    ast->relexp = unique_ptr<RelExpAST>((RelExpAST *)$1);
    $$ = ast;

  }
  | EqExp EQOP RelExp{
    auto ast = new EqExpAST();
    ast->tag = EqExpAST::OP;
    ast->eqexp = unique_ptr<EqExpAST>((EqExpAST *)$1);
    ast->op = *unique_ptr<string>($2);
    ast->relexp = unique_ptr<RelExpAST>((RelExpAST *)$3);
    $$ = ast;
  }
  ;


LAndExp     
  : EqExp {
    auto ast = new LAndExpAST();
    ast->tag = LAndExpAST::EXP;
    ast->eqexp = unique_ptr<EqExpAST>((EqExpAST *)$1);
    $$ = ast;

  }
  | LAndExp LANDOP EqExp{
    auto ast = new LAndExpAST();
    ast->tag = LAndExpAST::OP;
    ast->landexp = unique_ptr<LAndExpAST>((LAndExpAST *)$1);
    ast->op = *unique_ptr<string>(new string("&&"));
    ast->eqexp = unique_ptr<EqExpAST>((EqExpAST *)$3);
    $$ = ast;
  }
  ;

LOrExp      
  : LAndExp {
    auto ast = new LOrExpAST();
    ast->tag = LOrExpAST::EXP;
    ast->landexp = unique_ptr<LAndExpAST>((LAndExpAST *)$1);
    $$ = ast;
  }
  | LOrExp LOROP LAndExp{
    auto ast = new LOrExpAST();
    ast->tag = LOrExpAST::OP;
    ast->lorexp = unique_ptr<LOrExpAST>((LOrExpAST *)$1);
    ast->op = *unique_ptr<string>(new string("||"));
    ast->landexp = unique_ptr<LAndExpAST>((LAndExpAST *)$3);
    $$ = ast;
  }
  ;

ConstExp      
  : Exp{
    auto ast = new ConstExpAST();
    ast->exp = unique_ptr<ExpAST>((ExpAST *)$1);
    $$ = ast;
  }
  ;

%%

// 定义错误处理函数, 其中第二个参数是错误信息
// parser 如果发生错误 (例如输入的程序出现了语法错误), 就会调用这个函数
void yyerror(unique_ptr<BaseAST> &ast, const char *s) {
  cerr << "error: " << s << endl;
}
