#ifndef AST_CPP
#define AST_CPP

#include "ast.h"
#include <queue>
#include <map>
#include <stack>

// #define DEBUG

// #ifdef DEBUG

KoopaIR ki;


struct VAR{
    std::string value;
    enum {CONST, VARIABLE, PARAM} tag;
};

struct FUNC{
    std::string rettype;
    int paramnum;
};

struct ARR{
    std::string type;
    vector<int> size; //front is the innermost
    vector<int> offset; //front is the innermost
    int dim;
    int totalsize;
    enum {VARIABLE, PARAM} tag;
    vector<std::string> value;
};

struct PARAM{
    std::string name;
    enum {VARIABLE, ARRAY} tag;
};


std::stack<std::string> dumpstack; // queue to store variables/registers, main for number
std::stack<int> calcstack; // stack to calculate the value of the expression
std::stack<PARAM> paramstack; // stack to store parameters of the function

std::map<int, std::map<std::string, VAR> > varmap;// map to store variables and registers of different nest level
std::map<int, std::map<std::string, ARR> > arrmap; // map to store array name and its type and size
std::map<std::string, FUNC> funcmap; // map to store function name and its return type


//process array
std::string curarr; // current array name
bool isArr = false;
int arrCurNest = 0;
int arrPt = 0; //current pointer and determine whether need array to be pointer
std::vector<std::string> fillarr;


int nextNest = 0; //0 is the outermost ifnest
struct STATE{
    int curNest;
    bool isIf;
    bool isWhile;
    bool isReturn; //return, continue, break use the same bit
    int lastWhileNest;
    bool isBlock;
    std::string curFunc;

    STATE(bool isif, bool iswhile, int lastwhilenest = -1, std::string curfunc = "", bool isblock = false){
        curNest = nextNest++;
        isIf = isif;
        isWhile = iswhile;
        if(!isblock && iswhile){
            lastWhileNest = curNest;
        }
        else{
            lastWhileNest = lastwhilenest;
        }
        isReturn = false;
        curFunc = curfunc;
        isBlock = isblock;
    }
};
std::stack<STATE> statestack;
STATE state = STATE(false, false); //initiate for global





///////////////////////////// HELPER FUNCTIONS  /////////////////////////////
//dumpstack: get and pop
std::string dumpstacktop(){
    std::string ret = dumpstack.top();
    dumpstack.pop();
    return ret;
}

//nest
std::string nestedID(std::string ident, int nest){
    return ident + "_" + std::to_string(nest);
}

int latestNest(std::string ident){
    int n = state.curNest;
    while(varmap[n].find(ident) == varmap[n].end() && arrmap[n].find(ident) == arrmap[n].end()){
        n--;
    }
    return n;
}

//condition
void startNewState(bool isif, bool iswhile, std::string curfunc = state.curFunc, bool isblock = false){
    statestack.push(state);
    state = STATE(isif, iswhile, state.lastWhileNest, curfunc, isblock);
}

void endState(){
    varmap[state.curNest].clear();
    arrmap[state.curNest].clear();
    STATE tempstate = state;
    state = statestack.top();

    //if and while will get the isReturn
    if(tempstate.isBlock){
        state.isReturn = tempstate.isReturn;
    }
    statestack.pop();
}

//array
int initArray(std::string ident){
    // get the dimension and size of array
    // store every information into arrmap
    int arrdim = 0;
    vector<int> arrsize;
    vector<int> arroffset;
    std::string arrtype = "i32";
    int arrtotalsize = 1;

    while(!calcstack.empty()){
        arrsize.push_back(calcstack.top());
        arrtype = "[" + arrtype + ", " + std::to_string(calcstack.top()) + "]";
        arrtotalsize *= calcstack.top();
        arroffset.push_back(arrtotalsize);
        calcstack.pop();
        arrdim++;
    }

    arrmap[state.curNest][ident] = {arrtype, arrsize, arroffset, arrdim, arrtotalsize, ARR::VARIABLE};
    return arrtotalsize;
}

void storeArray(int totalsize){ 
    //get element pointer, and store the value at the destination
    for(int i = 0; i < totalsize; ++i){
        int t = i;
        //get pointer from the outermost to innermost
        std::string ptr = "@" + nestedID(curarr, state.curNest);
        for(int j = arrmap[state.curNest][curarr].dim - 1; j >=0  ;--j){
            std::string newreg = ki.newreg();
            if(j){
                ki.getelemptr_op(newreg, ptr, std::to_string(t / arrmap[state.curNest][curarr].offset[j - 1]));
                t %= arrmap[state.curNest][curarr].offset[j-1];
            }
            else{
                ki.getelemptr_op(newreg, ptr, std::to_string(t));
            }
            ptr = newreg;
        }
        ki.store_op(fillarr[i], ptr);
    }
}

std::string fillArray(int dim, int offset){
    if(!arrPt){ //no need fill anything
        return "zeroinit";
    }

    std::string ret = "{";
    for(int i = 0; i < arrmap[state.curNest][curarr].size[dim]; ++i){
        if(dim == 0){
            ret += fillarr[offset + i];
        }
        else{
            ret += fillArray(dim - 1, offset + i * arrmap[state.curNest][curarr].offset[dim - 1]);
        }

        if(i != arrmap[state.curNest][curarr].size[dim] - 1){
            ret += ", ";
        }
        else{
            ret += "}";
        }
    }
    return ret;
}

void startArrayInit(std::string ident){
    curarr = ident;
    isArr = true;
    arrCurNest = 0;
    arrPt = 0;

    //fill the array, init with 0
    int arrtotalsize = initArray(ident);
    for(int i = 0; i < arrtotalsize; ++i){
        fillarr.push_back("0");
    }
}

void endArrayInit(bool isInit, std::string ident){
    arrmap[state.curNest][ident].value = fillarr;
    if(state.curNest){ //local
        ki.alloc_op("@" + nestedID(ident, state.curNest), arrmap[state.curNest][ident].type);
        if(isInit){
            storeArray(arrmap[state.curNest][ident].totalsize);
        }
    }
    else{ //global
        std::string arrval = fillArray(arrmap[state.curNest][ident].dim - 1, 0); //fill from outermost to innermost
        ki.galloc_op("@" + nestedID(ident, state.curNest), arrmap[state.curNest][ident].type, arrval);
    }

    //reset array settings
    fillarr.clear();
    curarr = "";
    isArr = false;
    arrPt = 0; 
    arrCurNest = 0;
}

void updateArrPt(){
    //algorithm to calculate next place of array pointer
    int dimsize = 1;
    int dimdiff = arrmap[state.curNest][curarr].dim - arrCurNest;
    if(dimdiff > 0){
        for(int i = 0; i < dimdiff; ++i){
            dimsize *= arrmap[state.curNest][curarr].size[i];
        }
        arrPt = ceil((float)arrPt / (float)dimsize) * dimsize;
    }

}







/////////////////////////////  AST  /////////////////////////////

void CompUnitAST::Dump() const{ //DONE
    //declare function for sysy lib
    ki.lib_init();
    funcmap["getint"] = {"int", 0};
    funcmap["getch"] = {"int", 0};
    funcmap["getarray"] = {"int", 1};
    funcmap["putint"] = {"void", 1};
    funcmap["putch"] = {"void", 1};
    funcmap["putarray"] = {"void", 2};
    funcmap["starttime"] = {"void", 0};
    funcmap["stoptime"] = {"void", 0};

    compunitwrap->Dump();
}

void CompUnitWrapAST::Dump() const{ //DONE
    if(tag == FUNC){
        funcdef->Dump();
        compunitwrap->Dump();
    }
    if(tag == DECL){
        decl->Dump();
        compunitwrap->Dump();
    }
    else if(tag == EMPTY){
        // do nothing
    }
}

/////////////////////////////  DECLARE  /////////////////////////////

void DeclAST::Dump() const{ //DONE
    if(tag == CONST){
        constdecl->Dump();
    }
    else if(tag == VAR){
        vardecl->Dump();
    }
}

void ConstDeclAST::Dump() const{ //DONE
    constdefwrap->Dump();
}

void ConstDefWrapAST::Dump() const{ //DONE
    if(tag == ONE){
        constdef->Dump(); // will jump to calc and store the value into varmap
    }
    else if(tag == MANY){
        constdef->Dump(); 
        constdefwrap->Dump();
    }
}

void ConstDefAST::Dump() const{
    if(tag == VAR){
        //calculate value
        constinitval->Calc();
        varmap[state.curNest][ident] = {std::to_string(calcstack.top()), VAR::CONST};
        calcstack.pop();
    }
    else if(tag == ARRAY){
        
        //const int arr[2] = {1, 2};
        // @arr = alloc [i32, 2], {1, 2} 
        constarrayindex->Dump(); //get all dimensions into calcstack

        startArrayInit(ident);
        constinitval->Dump();
        endArrayInit(1, ident);      
    }
}

void ConstArrayIndexAST::Dump() const{
    // result store in calcstack
    if(tag == ONE){
        constexp->Calc();
    }
    else if(tag == MANY){
        constexp->Calc();
        constarrayindex->Dump();
    }
}

void ConstInitValAST::Dump() const{
    //only array will come in
    if(tag == EXP){
        constexp->Calc();
        //calculation of where to put the int
        if(isArr){ 
            fillarr[arrPt++] = std::to_string(calcstack.top());
            calcstack.pop();
        }
    }
    else if(tag == ARRAY){
        // aggregrate when needed
        arrCurNest++;
        constinitvalwrap->Dump();
        arrCurNest--;
        
        //when come out, update arrPtr
        updateArrPt();
    }
}

void ConstInitValWrapAST::Dump() const{ //DONE
    if(tag == ONE){
        constinitval->Dump();
    }
    else if(tag == MANY){
        constinitval->Dump();
        constinitvalwrap->Dump();
    }
    else if(tag == EMPTY){
        // do nothing
    }
}

void VarDeclAST::Dump() const{ //DONE
    vardefwrap->Dump();
}

void VarDefWrapAST::Dump() const{ //DONE
    if(tag == ONE){
        vardef->Dump();
    }
    else if(tag == MANY){
        vardef->Dump();
        vardefwrap->Dump();
    }
}

void VarDefAST::Dump() const{
    if(tag == IDENT){
        // map ident to 0, initialise ident
        varmap[state.curNest][ident] = {"0", VAR::VARIABLE};
    }
    else if(tag == INIT){
        initval->Dump();
        std::string initvalstr = dumpstacktop();

        //map ident to the value
        varmap[state.curNest][ident] = {initvalstr, VAR::VARIABLE};
    }
    else if(tag == ARRAY){
        constarrayindex->Dump(); //get all dimensions into calcstack

        startArrayInit(ident);
        //nothing to dump
        endArrayInit(0, ident);
    }
    else if(tag == ARRAY_INIT){
        constarrayindex->Dump(); //get all dimensions into calcstack

        startArrayInit(ident);
        initval->Dump();
        endArrayInit(1, ident);
    }


    //alloc and store in koopaIR
    if(tag == IDENT || tag == INIT){
        if(state.curNest){
            ki.alloc_op("@" + nestedID(ident, state.curNest), "i32");//only i32 is processed in l4
            ki.store_op(varmap[state.curNest][ident].value , "@" + nestedID(ident, state.curNest));
        }
        else{//global
            ki.galloc_op("@" + nestedID(ident, state.curNest), "i32", varmap[state.curNest][ident].value);
        }
    }
}

void InitValAST::Dump() const{
    if(tag == EXP){
        //calculation of where to put the int
        if(!isArr){
            exp->Dump();
        }
        else{ 
            if(!state.curNest){
                //global
                exp->Calc();
                fillarr[arrPt++] = std::to_string(calcstack.top());
                calcstack.pop();
            }
            else{
                int lastArrPt = arrPt;
                arrPt = 0;
                exp->Dump();
                arrPt = lastArrPt;
                fillarr[arrPt++] = dumpstacktop();
            }        
        }
    }
    else if(tag == ARRAY){
        // aggregrate when needed
        arrCurNest++;
        initvalwrap->Dump();
        arrCurNest--;
        //whenever come out, update array pointer
        updateArrPt();
    }
}

void InitValWrapAST::Dump() const{ //DONE
    if(tag == ONE){
        initval->Dump();
    }
    else if(tag == MANY){
        initval->Dump();
        initvalwrap->Dump();
    }
    else if(tag == EMPTY){
        // do nothing
    }
}




/////////////////////////////  FUNC  /////////////////////////////

void FuncDefAST::Dump() const{
    // Koopa IR 里的规定, Function, BasicBlock, Value 的名字必须以 @ 或者 % 开头
    // increase nest level
    startNewState(false, false, ident);
    ki.reset();
    int paramnum = 0;
    std::string paramstr = "", allocstr = "";

    //all parameter, store into paramstack
    fparamwrap->Dump(); 
    while(!paramstack.empty()){
        std::string dst = "%" + nestedID(paramstack.top().name, state.curNest);
        std::string src = "@" + paramstack.top().name;
        std::string right = (paramstack.top().tag == PARAM::VARIABLE?"i32": arrmap[state.curNest][paramstack.top().name].type);
        paramstack.pop();
        paramstr += src + ": " + right + (paramstack.empty()?"":", ");
        allocstr += " " + dst + "= alloc " + right + "\n";
        allocstr += " store " + src + ", " + dst + "\n";
        paramnum++;
    }
    funcmap[ident] = {type, paramnum};
    state.curFunc = ident;
    ki.func_start(ident, type, paramstr, allocstr);

    // function main block start
    block->Dump();
    // function main block end

    if(!state.isReturn){ //check is function returned?
        if(type == "void"){
            ki.ret_op("");
        }
        else{
            ki.ret_op("0");
        }
    }

    ki.func_end();
    endState();
    //nextNest = 1;
}

void FuncFParamWrapAST::Dump() const{
    if(tag == ONE){
        //last
        fparam->Dump();
    }
    else if(tag == MANY){
        fparamwrap->Dump();
        fparam->Dump();
    }
    else if(tag == EMPTY){
        // do nothing
    }
}

void FuncFParamAST::Dump() const{
    PARAM newparam = {ident, (tag == VAR? PARAM::VARIABLE: PARAM::ARRAY)};  
    paramstack.push(newparam);

    if(tag == VAR){
        varmap[state.curNest][ident] = {ident, VAR::PARAM};
    }
    else if(tag == ARRAY){
        int arrdim = 1; //one dim is used as
        vector<int> arrsize;
        vector<int> arroffset;
        std::string arrtype = "i32";
        int arrtotalsize = 1;
        if(constarrayindex){ //one dimension dont have constarrayindex
            constarrayindex->Dump(); 
        }

        while(!calcstack.empty()){
            arrsize.push_back(calcstack.top());
            arrtype = "[" + arrtype + ", " + std::to_string(calcstack.top()) + "]";
            arrtotalsize *= calcstack.top();
            arroffset.push_back(arrtotalsize);
            calcstack.pop();
            arrdim++;
        }
        arrtype = "*" + arrtype;

        //for outermost dimension, store as 0
        arrsize.push_back(0);
        arroffset.push_back(0);
        
        //map the array
        arrmap[state.curNest][ident] = {arrtype, arrsize, arroffset, arrdim, arrtotalsize, ARR::PARAM};
    }
    
}

/////////////////////////////  BLOCK  /////////////////////////////

void BlockAST::Dump() const{ //DONE
    blockitemwrap->Dump();
}

void BlockItemWrapAST::Dump() const{ //DONE
    if(tag == ITEM){
        blockitem->Dump();
        blockitemwrap->Dump();
    }
    else if(tag == EMPTY){
        // do nothing
    }
}

void BlockItemAST::Dump() const{ //DONE
    if(state.isReturn){
        return ;
    }

    if(tag == DECL){
        decl->Dump();
    }
    else if(tag == STMT){
        stmt->Dump();
    }
}



/////////////////////////////  STMT and EXP  /////////////////////////////

void StmtAST::Dump() const{
    if(state.isReturn){
        return ;
    }

    if(tag == ASSIGN){
        if(lval->tag == LValAST::ARRAY){
            arrPt = 1; //need to give pointer
            lval->Dump();
            std::string arraddr = dumpstacktop();
            arrPt = 0; //need value after
            
            exp->Dump();
            std::string expstr = dumpstacktop();

            //store the value into the array
            ki.store_op(expstr, arraddr);
        }
        else if(lval->tag == LValAST::IDENT){
            int ln = latestNest(lval->ident);
            exp->Dump();
            
            varmap[ln][lval->ident].value = dumpstacktop();
            
            if(varmap[ln][lval->ident].tag == VAR::PARAM){
                ki.store_op(varmap[ln][lval->ident].value , "%" + nestedID(lval->ident, ln));
            }
            else{
                ki.store_op(varmap[ln][lval->ident].value , "@" + nestedID(lval->ident, ln));
            }
        }
    }
    else if(tag == EMPTY){
        //do nothing
    }
    else if(tag == EXP){
        exp->Dump();
    }
    else if(tag == BLOCK){
        startNewState(state.isIf, state.isWhile, state.curFunc, true);
        block->Dump();
        endState();
    }
    else if(tag == IF){
        startNewState(true, false);
        std::string thenlabel = "%then_" + std::to_string(state.curNest);
        std::string endlabel = "%end_" + std::to_string(state.curNest);

        //exp
        exp->Dump();
        std::string expstr = dumpstacktop();
        ki.br_op(expstr, thenlabel, endlabel);

        //then stmt
        ki.block_init(thenlabel);
        stmt->Dump();
        if(!state.isReturn){ //no return, so need jump to end
            ki.jump_op(endlabel);
        }
        
        endState();

        //end
        ki.block_init(endlabel);
    }
    else if (tag == IFELSE){
        startNewState(true, false);
        std::string thenlabel = "%then_" + std::to_string(state.curNest);
        std::string elselabel = "%else_" + std::to_string(state.curNest);
        std::string endlabel = "%end_" + std::to_string(state.curNest);

        //exp inside if
        exp->Dump();
        std::string expstr = dumpstacktop();
        ki.br_op(expstr, thenlabel, elselabel);

        //then stmt
        ki.block_init(thenlabel);
        stmt->Dump();
        if(!state.isReturn){ //no return, so jump to end
            ki.jump_op(endlabel);
        }
        endState();

        //else stmt
        startNewState(true, false);
        ki.block_init(elselabel);
        elsestmt->Dump();
        if(!state.isReturn){
            ki.jump_op(endlabel);
        }
        endState();


        //end 
        ki.block_init(endlabel);
    }
    else if (tag == WHILE){
        startNewState(false, true);
        std::string entrylabel = "%while_entry_" + std::to_string(state.curNest);
        std::string bodylabel = "%while_body_" + std::to_string(state.curNest);
        std::string endlabel = "%end_" + std::to_string(state.curNest);
        
        //exp 
        ki.jump_op(entrylabel);
        ki.block_init(entrylabel);
        exp->Dump();
        std::string expstr = dumpstacktop();
        ki.br_op(expstr, bodylabel, endlabel);

        //then 
        ki.block_init(bodylabel);
        stmt->Dump();
        if(!state.isReturn){ //no return, so need jump to end
            ki.jump_op(entrylabel);
        }
        endState();

        //end
        ki.block_init(endlabel);
    }
    else if (tag == BREAK){
        std::string lastendlabel = "%end_" + std::to_string(state.lastWhileNest);
        if(state.isWhile || state.isIf){
            ki.jump_op(lastendlabel);
        }
        state.isReturn = true;
    }
    else if(tag == CONTINUE){
        std::string lastentrylabel = "%while_entry_" + std::to_string(state.lastWhileNest);
        if(state.isWhile || state.isIf){
            ki.jump_op(lastentrylabel);
        }
        state.isReturn = true;
        
    }
    else if(tag == RETURN){
        if(exp){ //check if is void function return or normal return
            exp->Dump();
            ki.ret_op(dumpstacktop());
        }
        else{
            ki.ret_op("");
        }
        state.isReturn = true;
    }
}

void ExpAST::Dump() const{
    lorexp->Dump();
}
  
void LValAST::Dump() const{
    if(tag == IDENT){
        // load out ident into register
        int ln = latestNest(ident);
        //check is arr pointer or variable
        int isArray = (arrmap[ln].find(ident) != arrmap[ln].end()); //type 0 for variable, 1 for array

        if(isArray){
            //array, func(arr)
            std::string newreg = ki.newreg();
            if(arrmap[ln][ident].tag == ARR::PARAM){
                //param
                ki.load_op(newreg, "%" + nestedID(ident, ln));
            }
            else if(arrmap[ln][ident].tag == ARR::VARIABLE){
                //variable
                // ki.load_op(newreg, "@" + nestedID(ident, ln));
                ki.getelemptr_op(newreg, "@" + nestedID(ident, ln), "0");
            }
            dumpstack.push(newreg);
        }
        else{
            //variable
            if(varmap[ln][ident].tag == VAR::CONST){
                // const
                dumpstack.push(varmap[ln][ident].value);
            }
            else if(varmap[ln][ident].tag == VAR::VARIABLE){
                // var
                // the value
                std::string newreg = ki.newreg();
                ki.load_op(newreg, "@" + nestedID(ident, ln));
                dumpstack.push(newreg);
            }
            else if(varmap[ln][ident].tag == VAR::PARAM){
                // param
                std::string newreg = ki.newreg();
                ki.load_op(newreg, "%" + nestedID(ident, ln));
                dumpstack.push(newreg);
            }
        }
    }
    else if(tag == ARRAY){
        std::string lastcurarr = curarr;
        int lastArrNest = arrCurNest;

        
        curarr = ident;
        arrCurNest = 0;
        arrayindex->Dump();
        arrCurNest = lastArrNest;
        curarr = lastcurarr;
    }
}

void ArrayIndexAST::Dump() const{
    int ln = latestNest(curarr);
    int lastArrPt = arrPt;
    int lastArrNest = arrCurNest;
    if(tag == ONE){
        arrPt = 0; //want value
        arrCurNest = 0;
        exp->Dump(); //get the index
        arrCurNest = lastArrNest;
        arrPt = lastArrPt;
        std::string index = dumpstacktop();


        // @arr = alloc *[i32, 3]        // @arr 的类型是 **[i32, 3]
        // %ptr1 = load @arr             // %ptr1 的类型是 *[i32, 3]
        // %ptr2 = getptr %ptr1, 1       // %ptr2 的类型是 *[i32, 3]
        // %ptr3 = getelemptr %ptr2, 2   // %ptr3 的类型是 *i32
        // %value = load %ptr3           // %value 的类型是 i32
        // // 这是一段类型和功能都正确的 Koopa IR 代码

        std::string newreg = ki.newreg();
        //check array dimension with the nest level
        if(arrCurNest == 0){ //only one dimension
            if(arrmap[ln][curarr].tag == ARR::PARAM){
                std::string oldreg = newreg;
                ki.load_op(newreg, "%" + nestedID(curarr, ln));
                newreg = ki.newreg();
                ki.getptr_op(newreg, oldreg, index);
            }
            else if(arrmap[ln][curarr].tag == ARR::VARIABLE){
                ki.getelemptr_op(newreg, "@" + nestedID(curarr, ln), index);
            }
        }
        else{
            ki.getelemptr_op(newreg, dumpstacktop(), index);
        }


        if(arrCurNest < arrmap[ln][curarr].dim - 1){
            //if the array is not the innermost, then the array should be a pointer
            std::string oldreg = newreg;
            newreg = ki.newreg();
            ki.getelemptr_op(newreg, oldreg, "0");
        }
        else if(arrCurNest == arrmap[ln][curarr].dim - 1 && !arrPt){ //value
            std::string oldreg = newreg;
            newreg = ki.newreg();
            ki.load_op(newreg, oldreg);
        }
        dumpstack.push(newreg);
    }
    else if(tag == MANY){
        arrPt = 0;
        arrCurNest = 0;
        exp->Dump();
        arrCurNest = lastArrNest;
        arrPt = lastArrPt;

        std::string index = dumpstacktop();
        
        std::string newreg = ki.newreg();


        if(arrCurNest == 0){ //not one dimension, but first time come in
            if(arrmap[ln][curarr].tag == ARR::PARAM){
                std::string oldreg = newreg;
                ki.load_op(newreg, "%" + nestedID(curarr, ln)); //get *[i32, 3]
                newreg = ki.newreg();
                ki.getptr_op(newreg, oldreg, index);
            }
            else if(arrmap[ln][curarr].tag == ARR::VARIABLE){
                ki.getelemptr_op(newreg, "@" + nestedID(curarr, ln), index);
            }
        }
        else{
            ki.getelemptr_op(newreg, dumpstacktop(), index);
            
        }
        dumpstack.push(newreg);

        arrCurNest++;
        arrayindex->Dump();
        arrCurNest--;
    }
}

void PrimaryExpAST::Dump() const{ //DONE
    if(tag == BRAC){
        exp->Dump();
    }
    else if(tag == NUM){
        dumpstack.push(std::to_string(number));
    }
    else if(tag == LVAL){
        lval->Dump();
    }
}

void UnaryExpAST::Dump() const{
    if(tag == EXP){
        priexp->Dump();
    }
    else if(tag == OPEXP){
        unaexp->Dump();
        std::string oldreg, newreg;
       
        if(op == '+'){
            // do nothing
            return ;
        }
        else if(op == '-'){
            //let the top of the stack will be reg or num
            oldreg = dumpstacktop();
            newreg = ki.newreg();
            ki.assign_op(newreg, "sub", "0", oldreg);
        }
        else if(op == '!'){
            // newreg = eq oldreg, 0
            oldreg = dumpstacktop();
            newreg = ki.newreg();
            ki.assign_op(newreg, "eq", "0", oldreg);
        }
        dumpstack.push(newreg);
    }
    else if(tag == CALL){
        //when call, param push into rparamstack
        //isFuncParam = true;

        //get param str
        std::string paramstr = "";
        rparamwrap->Dump();
        for(int i = 0; i < funcmap[ident].paramnum; ++i){
            paramstr += dumpstacktop() + (i == funcmap[ident].paramnum - 1?"":", ");
        }

        if(funcmap[ident].rettype == "void"){
            // void function
            ki.call_op(ident,"", paramstr);
        }
        else{
            // int function
            std::string newreg = ki.newreg(); //register to store the return value
            ki.call_op(ident, newreg, paramstr);
            dumpstack.push(newreg);
        }
        //isFuncParam = false;
    }
}

void FuncRParamWrapAST::Dump() const{ //DONE
    if(tag == ONE){
        //last param
        exp->Dump();
    }
    else if(tag == MANY){
        rparamwrap->Dump();
        exp->Dump();
    }
    else if(tag == EMPTY){
        // do nothing
    }
}

/////////////////////////////  BASIC CALC EXP  /////////////////////////////

void MulExpAST::Dump() const{
    if(tag == EXP){
        unaexp->Dump();
    }
    else if(tag == OP){
        // MUL OP UNARY
        std::string left;
        std::string right;
        std::string newreg;

        mulexp->Dump();
        left = dumpstacktop();

        unaexp->Dump();
        right = dumpstacktop();
        

        newreg = ki.newreg();

        if(op == '*'){
            ki.assign_op(newreg, "mul", left, right);
        }
        else if(op == '/'){
            ki.assign_op(newreg, "div", left, right);
        }
        else if(op == '%'){
            ki.assign_op(newreg, "mod", left, right);
        }

        dumpstack.push(newreg);
    }
}

void AddExpAST::Dump() const{
    if(tag == EXP){
        mulexp->Dump();
    }
    else if(tag == OP){  
        // ADD OP MUL
        std::string left;
        std::string right;
        std::string newreg;

        addexp->Dump();
        left = dumpstacktop();

        mulexp->Dump();
        right = dumpstacktop();
    
        newreg = ki.newreg();
        if(op == '+'){
            ki.assign_op(newreg, "add", left, right);
        }
        else if(op == '-'){
            ki.assign_op(newreg, "sub", left, right);
        }
        dumpstack.push(newreg);
    }
}

void RelExpAST::Dump()const{
    if(tag == EXP){
        addexp->Dump();
    }
    else if(tag == OP){
        // REL OP ADD
        std::string left;
        std::string right;
        std::string newreg;

        relexp->Dump();
        left = dumpstacktop();

        addexp->Dump();
        right = dumpstacktop();
        
        //count newreg
        newreg = ki.newreg();
        if(op == "<"){
            ki.assign_op(newreg, "lt", left, right);
        }
        else if(op == ">"){
            ki.assign_op(newreg, "gt", left, right);
        }
        else if(op == "<="){
            ki.assign_op(newreg, "le", left, right);
        }
        else if(op == ">="){
            ki.assign_op(newreg, "ge", left, right);
        }
        dumpstack.push(newreg);
    }
}

void EqExpAST::Dump()const{
    if(tag == EXP){
        relexp->Dump();
    }
    else if(tag == OP){
        //EQ OP REL
        std::string left;
        std::string right;
        std::string newreg;

        //eqexp
        eqexp->Dump();
        left = dumpstacktop();

        relexp->Dump();
        right = dumpstacktop();
    
        //count newreg
        newreg = ki.newreg();
        if(op == "=="){
            ki.assign_op(newreg, "eq", left, right);
        }
        else if(op == "!="){
            ki.assign_op(newreg, "ne", left, right);
        }

        dumpstack.push(newreg);
    }
}

void LAndExpAST::Dump()const{
    if(tag == EXP){
        eqexp->Dump();
    }
    else if(tag == OP){
        // AND OP EQ
        // shortcircuit
        // int result = 0;
        // if (lhs != 0) {
        // result = rhs != 0;
        // }
        // // 表达式的结果即是 result
        std::string left;
        std::string right;
        std::string newreg;

        // landexp->Dump();
        // left = dumpstacktop();

        // eqexp->Dump();
        // right = dumpstacktop();
        
        // std::string r1, r2;
        // //count newreg
        // newreg = ki.newreg();
        // ki.assign_op(newreg, "ne", left, "0");
        // r1 = newreg;

        // newreg = ki.newreg();
        // ki.assign_op(newreg, "ne", right, "0");
        // r2 = newreg;

        // newreg = ki.newreg();
        // ki.assign_op(newreg, "and", r1, r2);
        
        // dumpstack.push(newreg);

        //short circuit
        startNewState(true, false);
        //alloc for result variable
        ki.alloc_op(nestedID("@andresult", state.curNest), "i32");
        ki.store_op("0",  nestedID("@andresult", state.curNest));

        std::string thenlabel = "%then_" + std::to_string(state.curNest);
        std::string endlabel = "%end_" + std::to_string(state.curNest);

        landexp->Dump();
        left = dumpstacktop();
        newreg = ki.newreg();
        ki.assign_op(newreg, "ne", left, "0");
        ki.br_op(newreg, thenlabel, endlabel);

        //then stmt, need a new nest
        ki.block_init(thenlabel);
        eqexp->Dump();
        right = dumpstacktop();
        newreg = ki.newreg();
        ki.assign_op(newreg, "ne", right, "0");
        ki.store_op(newreg, nestedID("@andresult", state.curNest));
        if(!state.isReturn){ //no return, so need jump to end
            ki.jump_op(endlabel);
        }

        //end
        ki.block_init(endlabel);
        newreg = ki.newreg();
        ki.load_op(newreg, nestedID("@andresult", state.curNest));
        dumpstack.push(newreg);
        endState();
    }

}

void LOrExpAST::Dump()const{
    if(tag == EXP){
        landexp->Dump();
    }
    else if(tag == OP){
        // LOR OP LAND
        // shortcircuit
        // int result = 1;
        // if (lhs == 0) {
        // result = rhs != 0;
        // }
        // // 表达式的结果即是 result

        std::string left;
        std::string right;
        std::string newreg;

        // lorexp->Dump();
        // left = dumpstacktop();

        // landexp->Dump();
        // right = dumpstacktop();
        

        // std::string r1, r2;
        // //count newreg
        // newreg = ki.newreg();
        // ki.assign_op(newreg, "ne", left, "0");
        // r1 = newreg;

        // newreg = ki.newreg();
        // ki.assign_op(newreg, "ne", right, "0");
        // r2 = newreg;

        // newreg = ki.newreg();
        // ki.assign_op(newreg, "or", r1, r2);
        
        // dumpstack.push(newreg);

        //short circuit
        startNewState(true, false);
        //alloc for result variable
        ki.alloc_op(nestedID("@orresult", state.curNest), "i32");
        ki.store_op("1",  nestedID("@orresult", state.curNest));

        std::string thenlabel = "%then_" + std::to_string(state.curNest);
        std::string endlabel = "%end_" + std::to_string(state.curNest);

        lorexp->Dump();
        left = dumpstacktop();
        newreg = ki.newreg();
        ki.assign_op(newreg, "eq", left, "0");
        ki.br_op(newreg, thenlabel, endlabel);

        //then stmt, need a new nest
        ki.block_init(thenlabel);
        landexp->Dump();
        right = dumpstacktop();
        newreg = ki.newreg();
        ki.assign_op(newreg, "ne", right, "0");
        ki.store_op(newreg, nestedID("@orresult", state.curNest));
        if(!state.isReturn){ //no return, so need jump to end
            ki.jump_op(endlabel);
        }
        
        //end
        ki.block_init(endlabel);
        newreg = ki.newreg();
        ki.load_op(newreg, nestedID("@orresult", state.curNest));
        dumpstack.push(newreg);
        endState();
    }
}

void ConstExpAST::Dump()const{
    //do nothing
}





/////////////////////////////  CALCULATIONS  /////////////////////////////

//Calc
void ConstInitValAST::Calc()const{
    constexp->Calc();
}

void ConstExpAST::Calc()const{
    exp->Calc();
}

void InitValAST::Calc()const{
    exp->Calc();
}

void ExpAST::Calc()const{
    lorexp->Calc();
}

void LOrExpAST::Calc()const{
    if(tag == EXP){
        landexp->Calc();
    }
    else if(tag == OP){
        lorexp->Calc(); // will store the value into calcstack
        //shortcut
        // if(calcstack.top() != 0){
        //     return ;
        // }
        landexp->Calc(); // will store the value into calcstack

        int right = calcstack.top(); //landexp
        calcstack.pop();
        int left = calcstack.top();//lorexp
        calcstack.pop();
    
        int retint = left || right;
        calcstack.push(retint);
    }
}

void LAndExpAST::Calc()const{
    if(tag == EXP){
        eqexp->Calc();
    }
    else if(tag == OP){
        landexp->Calc(); // will store the value into calcstack
        //and shortcut
        // if(calcstack.top() == 0){
        //     return ;
        // }
        eqexp->Calc(); // will store the value into calcstack

        int right = calcstack.top(); //eqexp
        calcstack.pop();
        int left = calcstack.top(); //landexp
        calcstack.pop();

        int retint = left && right;
        calcstack.push(retint);
    }
}

void EqExpAST::Calc()const{
    if(tag == EXP){
        relexp->Calc();
    }
    else if(tag == OP){
        eqexp->Calc(); // will store the value into calcstack
        relexp->Calc(); // will store the value into calcstack

        int left = calcstack.top(); //relexp
        calcstack.pop();
        int right = calcstack.top(); //eqexp
        calcstack.pop();

        int retint;
        if(op == "=="){
            retint = left == right;
        }
        else if(op == "!="){
            retint = left != right;
        }
        calcstack.push(retint);
    }
}

void RelExpAST::Calc()const{
    if(tag == EXP){
        addexp->Calc();
    }
    else if(tag == OP){
        relexp->Calc(); // will store the value into calcstack
        addexp->Calc(); // will store the value into calcstack

        int right = calcstack.top(); //addexp
        calcstack.pop();
        int left = calcstack.top(); //relexp
        calcstack.pop();
        
        int retint;
        if(op == "<"){
            retint = left < right;
        }
        else if(op == ">"){
            retint = left > right;
        }
        else if(op == "<="){
            retint = left <= right;
        }
        else if(op == ">="){
            retint = left >= right;
        }
        calcstack.push(retint);
    }
}

void AddExpAST::Calc()const{
    if(tag == EXP){
        mulexp->Calc();
    }
    else if(tag == OP){
        // do mul first
        addexp->Calc(); // will store the value into calcstack
        mulexp->Calc(); // will store the value into calcstack
        
        int right = calcstack.top(); //mulexp
        calcstack.pop();
        int left = calcstack.top(); //addexp
        calcstack.pop();

        int retint;
        if(op == '+'){
            retint = left + right;
        }
        else if(op == '-'){
            retint = left - right;
        }

        calcstack.push(retint);
    }
}

void MulExpAST::Calc()const{
    if(tag == EXP){
        unaexp->Calc();
    }
    else if(tag == OP){
        mulexp->Calc(); // will store the value into calcstack
        unaexp->Calc(); // will store the value into calcstack

        int right = calcstack.top();
        calcstack.pop();
        int left = calcstack.top();
        calcstack.pop();

        int retint;
        if(op == '*'){
            retint = left * right;
        }
        else if(op == '/'){
            retint = left / right;
        }
        else if(op == '%'){
            retint = left % right;
        }
        calcstack.push(retint);
    }
}

void UnaryExpAST::Calc()const{
    if(tag == EXP){
        priexp->Calc();
    }
    else if(tag == OPEXP){
        unaexp->Calc(); // will store the value into calcstack

        int left = calcstack.top();
        calcstack.pop();

        int retint;
        if(op == '-'){
            retint = -left;
        }
        else if(op == '!'){
            retint = !left;
        }
        else if(op == '+'){
            retint = left;
        }

        calcstack.push(retint);
    }
}

void PrimaryExpAST::Calc()const{
    if(tag == BRAC){
        exp->Calc();
    }
    else if(tag == NUM){
        calcstack.push(number);
    }
    else if(tag == LVAL){
        lval->Calc();
    }
}

void LValAST::Calc()const{
    printf("inside lval!\n");
    printf("ident: %s\n", ident.c_str());
    //store the value into the calcstack
    int ln = latestNest(ident);
    if(tag == ARRAY){
        arrayindex->Calc(ident, arrmap[ln][ident].dim - 2 , 0);
    }
    else if(tag == IDENT){
        calcstack.push(std::atoi(varmap[ln][ident].value.c_str()));  
    }

}

void ArrayIndexAST::Calc(std::string ident, int dim, int accumulate) const{
    printf("inside array index!\n");
    int ln = latestNest(ident);
    if(tag == ONE){
        printf("dim: %d\n", dim);
        assert(dim == -1);
        //comfirm is last
        exp->Calc(); //get the index
        int index = accumulate + calcstack.top();
        calcstack.pop();
        calcstack.push(std::atoi(arrmap[ln][ident].value[index].c_str()));
    }
    else if(tag == MANY){
        exp->Calc();
        int index = arrmap[ln][ident].offset[dim] * calcstack.top() + accumulate;
        calcstack.pop();

        arrayindex->Calc(ident, dim - 1, index);
    }
}



#endif