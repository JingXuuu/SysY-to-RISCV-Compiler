#ifndef TOOLS_H
#define TOOLS_H
#include <vector>
#include <cmath>
#include <algorithm>
#include <stack>
#include <queue>


class KoopaIR
{
public:
    std::string irstr; // the whole string  
    int reg; // always taking the next register that havent be used

    KoopaIR(){
        //initialize
        reg = 0;
        irstr = "";
    }

    //return whole string
    std::string getir(){
        return irstr;
    }

    //register operations
    std::string newreg(){
        std::string retreg = "%" + std::to_string(reg++);
        return retreg;
    }

    std::string oldreg(){
        std::string retreg = "%" + std::to_string(reg - 1);
        return retreg;
    }

    void reset(){
        reg = 0;
    }

    void lib_init(){
        irstr += "decl @getint(): i32\n";
        irstr += "decl @getch(): i32\n";
        irstr += "decl @getarray(*i32): i32\n";
        irstr += "decl @putint(i32)\n";
        irstr += "decl @putch(i32)\n";
        irstr += "decl @putarray(i32, *i32)\n";
        irstr += "decl @starttime()\n";
        irstr += "decl @stoptime()\n";
        irstr += "\n";
    }

    void func_start(std::string name, std::string type, std::string paramstr, std::string allocstr){
        irstr += "fun @" + name + "(" + paramstr + ")";   
        irstr += (type == "int" ? ": i32" : "");
        irstr += " {\n%entry:\n"; //%entry 是这个基本块的名字
        irstr += allocstr;
    }

    void func_end(){
        irstr += "}\n\n";
    }

    void block_init(std::string blockname){
        irstr += "\n" + blockname + ":\n";
    }

    void assign_op(std::string left, std::string op, std::string right1, std::string right2){
        irstr += " " + left + " = " + op + " " + right1 + ", " + right2 + "\n";
    }

    void store_op(std::string left, std::string right){
        irstr += " store " + left + ", " + right + "\n";
    }

    void load_op(std::string left, std::string right){
        irstr += " " + left + " = load " + right + "\n";
    }

    void alloc_op(std::string left, std::string right){
        irstr += " " + left + " = alloc " + right + "\n";
    } 

    void galloc_op(std::string left, std::string right, std::string value){
        irstr += "global " + left + " = alloc " + right + ", " + value + "\n";
    }   

    void ret_op(std::string left){
        irstr += " ret " + left + "\n";
    }

    void br_op(std::string left, std::string branch1, std::string branch2){
        irstr += " br " + left + ", " + branch1 + ", " + branch2 + "\n";
    }

    void jump_op(std::string left){
        irstr += " jump " + left + "\n";
    }

    void call_op(std::string id, std::string left, std::string paramstr){
        irstr += " " + (left==""?"":(left + " = ")) + "call @" + id + "(" + paramstr + ")\n";
    }

    void getelemptr_op(std::string left, std::string right, std::string index){
        irstr += " " + left + " = getelemptr " + right + ", " + index + "\n";
    }

    void getptr_op(std::string left, std::string right, std::string index){
        irstr += " " + left + " = getptr " + right + ", " + index + "\n";
    }

};

class RiscV
{
public:
    std::string rvstr; // the whole string
    std::string retstr; // return string
    int regoffset;
    int totaloffset;
    int raoffset;

    RiscV(){
        rvstr = "";
        regoffset = 0;
    }

    void clear(){
        regoffset = 0;
    }

    //return whole string
    std::string getriscv(){
        return rvstr;
    }

    // register operations
    std::string newreg(){
        std::string retreg = std::to_string(regoffset) + "(sp)";
        regoffset += 4;
        return retreg;
    }

    // allocation for start
    std::string initreg(int size){
        std::string retreg = std::to_string(regoffset);
        regoffset += size;
        return retreg;
    }

    int getOffset(std::string spval){
        int offset = 0;
        if(spval.find("(") != std::string::npos){
            offset = stoi(spval.substr(0, spval.find("(")));
        }
        return offset;
    }
    ////////////////////////  function //////////////////////////

    void func_start(std::string funcname, int space, int savera){ //save ra
        totaloffset = space;
        raoffset = 0;

        rvstr += " .text\n";
        rvstr += " .globl " + funcname + "\n" ;
        rvstr +=  funcname + ":\n";
        if(totaloffset > 2047){
            loadi_op("t6", "-" + std::to_string(totaloffset));
            bin2_op("add", "sp", "sp", "t6");

            if(savera){
                raoffset = space-4;
                loadi_op("t6", std::to_string(raoffset));
                bin2_op("add", "t6", "sp", "t6");
                storew_op("ra", "0(t6)");
            }
        }
        else{
            if(space != 0){
                rvstr += " addi sp, sp, -" + std::to_string(totaloffset) + "\n";
            }
            if(savera){
                rvstr += " sw ra, " + std::to_string(space-4) +   "(sp)\n";
                raoffset = space - 4;
            }
        }
    }

    void func_end(){
        rvstr += "\n";
    }

    void glob_alloc(std::string varname){
        rvstr += " .data\n";
        rvstr += " .globl " + varname + "\n";
        rvstr += varname + ":\n";
    }

    void glob_init(std::string val){
        rvstr += " .word " + val + "\n";
    }

    void zero_init(int size){
        rvstr += " .zero " + std::to_string(size) + "\n";
    }

    void glob_end(){
        rvstr += "\n";
    }

    void ret_op(){
        if(totaloffset > 2047){
            if(raoffset){
                loadi_op("t6", std::to_string(raoffset));
                bin2_op("add", "t6", "sp", "t6");
                loadw_op("ra", "0(t6)");
            }
            if(totaloffset){
                loadi_op("t6", std::to_string(totaloffset));
                bin2_op("add", "sp", "sp", "t6");
            }            
        }
        else{
            if(raoffset){
                rvstr += " lw ra, " + std::to_string(raoffset) + "(sp)\n";
            }
            if(totaloffset){
                rvstr += " addi sp, sp, " + std::to_string(totaloffset) + "\n";
            }  
        }
  
        rvstr += " ret\n";
    }

    void label_op(std::string labelname){
        if(labelname == "entry"){
            return; 
        }
        rvstr += labelname + ":\n";
    }

    ////////////////////////  load and store //////////////////////////

    void loadi_op(std::string dst, std::string value){
        rvstr += " li " + dst + ", " + value + "\n";
    }

    void loadw_op(std::string dst, std::string value){
        if(getOffset(value) > 2047){
            loadi_op("t6", std::to_string(getOffset(value)));
            bin2_op("add", "t6", "sp", "t6");
            loadw_op(dst, "0(t6)");
        }
        else{
            rvstr += " lw " + dst + ", " + value + "\n";
        }
    }

    void loada_op(std::string dst, std::string value){
        rvstr += " la " + dst + ", " + value + "\n";
    }

    void storew_op(std::string src, std::string dst){
        if(getOffset(dst) > 2047){
            loadi_op("t6", std::to_string(getOffset(dst)));
            bin2_op("add", "t6", "sp", "t6");
            storew_op(src, "0(t6)");
        }
        else{
            rvstr += " sw " + src + ", " + dst + "\n";
        }
    }

    ////////////////////////  arithmetic //////////////////////////

    void bin0_op(std::string op, std::string dst){
        rvstr += " " + op + " " + dst + "\n";
    }

    void bin1_op(std::string op, std::string dst, std::string src){
        rvstr += " " + op + " " + dst + ", " + src + "\n";
    }

    void bin2_op(std::string op, std::string dst, std::string src1, std::string src2){
        rvstr += " " + op + " " + dst + ", " + src1 + ", " + src2 + "\n";
    }

    

    

};
#endif