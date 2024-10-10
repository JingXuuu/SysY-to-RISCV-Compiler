#ifndef VISIT_CPP
#define VISIT_CPP

#include <queue>
#include <stack>
#include <map>
#include "visit.h"

RiscV rv;

// 函数声明略
// ...

stack<std::string> visitstack;
map<koopa_raw_value_t, std::string> addrmap;
stack<bool> ifvisitstack; //if ifvisitstack is empty, then is outermost if


size_t getTypeSize(koopa_raw_type_t ty){
    switch(ty->tag){
        case KOOPA_RTT_INT32:
            return 4;
        case KOOPA_RTT_UNIT:
            return 0;
        case KOOPA_RTT_ARRAY:
            return ty->data.array.len * getTypeSize(ty->data.array.base);
        case KOOPA_RTT_POINTER:
            return 4;
        case KOOPA_RTT_FUNCTION:
            return 0;
    }
    return 0;
}


// 访问 raw program
void Visit(const koopa_raw_program_t &program) {
    // typedef struct {
    //   /// Global values (global allocations only).
    //   koopa_raw_slice_t values;
    //   /// Function definitions.
    //   koopa_raw_slice_t funcs;
    // } koopa_raw_program_t;

    // 执行一些其他的必要操作
    // ...
    assert(program.funcs.kind == KOOPA_RSIK_FUNCTION);

    // 访问所有全局变量
    Visit(program.values);
    // 访问所有函数
    Visit(program.funcs);
}

// 访问 raw slice (每个都会先到这边分类看要被转去哪个函数)
void Visit(const koopa_raw_slice_t &slice) {
    // typedef struct {
    //   /// Buffer of slice items.
    //   const void **buffer;
    //   /// Length of slice.
    //   uint32_t len;
    //   /// Kind of slice items.
    //   koopa_raw_slice_item_kind_t kind;
    // } koopa_raw_slice_t;

    for (size_t i = 0; i < slice.len; ++i) {
        auto ptr = slice.buffer[i];
        // 根据 slice 的 kind 决定将 ptr 视作何种元素
        switch (slice.kind) {
        case KOOPA_RSIK_FUNCTION:
            // 访问函数
            Visit(reinterpret_cast<koopa_raw_function_t>(ptr));
            break;

        case KOOPA_RSIK_BASIC_BLOCK:
            // 访问基本块

            Visit(reinterpret_cast<koopa_raw_basic_block_t>(ptr));
            break;

        case KOOPA_RSIK_VALUE:
            // 访问指令
            Visit(reinterpret_cast<koopa_raw_value_t>(ptr));
            break;

        default:
            // 我们暂时不会遇到其他内容, 于是不对其做任何处理
            assert(false);
        }
    }
}

// 访问函数
void Visit(const koopa_raw_function_t &func) {
    // typedef struct {
    //   /// Type of function.
    //   koopa_raw_type_t ty;
    //   /// Name of function.
    //   const char *name;
    //   /// Parameters.
    //   koopa_raw_slice_t params;
    //   /// Basic blocks, empty if is a function declaration.
    //   koopa_raw_slice_t bbs;
    // } koopa_raw_function_data_t;


    // 执行一些其他的必要操作
    // ignore declare function
    if(func->bbs.len == 0){
        return;
    }

    addrmap.clear();
    rv.clear();

    // dfs to get all basic blocks, calculate stack size
    // S : alloc
    // R : 4 is got call, else 0, for saving ra
    // A : max(max(call_i.args.len) - 8, 0) * 4, for saving parameters to call
    // S' = ceil((S + R + A)/16) * 16
    int S = 0, R = 0, A = 0, EXTRA = 0;
    for(int i = 0; i < func->bbs.len; i++){
        auto bb = reinterpret_cast<koopa_raw_basic_block_t>(func->bbs.buffer[i]);
        auto insts = bb->insts;
        for(int j = 0; j < insts.len; j++){
            auto inst = reinterpret_cast<koopa_raw_value_t>(insts.buffer[j]);

            if(inst->kind.tag == KOOPA_RVT_CALL){
                R = 4;
                if(inst->kind.data.call.args.len > 8){
                    A = max(A, (int)inst->kind.data.call.args.len - 8) * 4;
                }
            }
            else if(inst->kind.tag == KOOPA_RVT_ALLOC){
                size_t size = getTypeSize(inst->ty->data.pointer.base);
                S += size;
                addrmap[inst] = rv.initreg(size);
                continue; 
            }
            // else if(inst->kind.tag == KOOPA_RVT_CALL || inst->kind.tag == KOOPA_RVT_BINARY || inst->kind.tag == KOOPA_RVT_GET_ELEM_PTR || inst->kind.tag == KOOPA_RVT_GET_PTR){
            //     // for extra space
            //     printf("extra\n");
                
            //     S += 4;
            //     addrmap[inst] = rv.initreg(4);
            // }

            //yinggai correct le bahh
            int size = getTypeSize(inst->ty);
            if(size != 0){
                S += size;
                addrmap[inst] = rv.initreg(size);
            }
        }
    }
    
    //change every addrmap with sp
    for(auto it = addrmap.begin(); it != addrmap.end(); it++){
        it->second = std::to_string(A + stoi(it->second)) + "(sp)";
    }

    int S_ = ceil( (float)(S + R + A + EXTRA) / 16) * 16;
    rv.func_start(func->name + 1, S_, R);
    Visit(func->bbs);
    rv.func_end();
}

// 访问基本块
void Visit(const koopa_raw_basic_block_t &bb) {
    // typedef struct {
    //   /// Name of basic block, null if no name.
    //   const char *name;
    //   /// Parameters.
    //   koopa_raw_slice_t params;
    //   /// Values that this basic block is used by.
    //   koopa_raw_slice_t used_by;
    //   /// Instructions in this basic block.
    //   koopa_raw_slice_t insts;
    // } koopa_raw_basic_block_data_t;

    // 执行一些其他的必要操作
    // ...
    // 访问所有指令
    if(bb->name != nullptr){
        rv.label_op(bb->name + 1);
    }
    Visit(bb->insts);
}

// 访问指令
void Visit(const koopa_raw_value_t &value) {
    // struct koopa_raw_value_data {
    //   /// Type of value.
    //   koopa_raw_type_t ty;
    //   /// Name of value, null if no name.
    //   const char *name;
    //   /// Values that this value is used by.
    //   koopa_raw_slice_t used_by;
    //   /// Kind of value.
    //   koopa_raw_value_kind_t kind;
    // };
    

    // 根据指令类型判断后续需要如何访问
    //   typedef struct {
    //   koopa_raw_value_tag_t tag;
    //   union {
    //     koopa_raw_integer_t integer;
    //     koopa_raw_aggregate_t aggregate;
    //     koopa_raw_func_arg_ref_t func_arg_ref;
    //     koopa_raw_block_arg_ref_t block_arg_ref;
    //     koopa_raw_global_alloc_t global_alloc;
    //     koopa_raw_load_t load;
    //     koopa_raw_store_t store;
    //     koopa_raw_get_ptr_t get_ptr;
    //     koopa_raw_get_elem_ptr_t get_elem_ptr;
    //     koopa_raw_binary_t binary;
    //     koopa_raw_branch_t branch;
    //     koopa_raw_jump_t jump;
    //     koopa_raw_call_t call;
    //     koopa_raw_return_t ret;
    //   } data;
    // } koopa_raw_value_kind_t;

    // typedef enum {
    // /// Integer constant.
    // KOOPA_RVT_ZERO_INIT,
    // /// Undefined value.
    // KOOPA_RVT_UNDEF,
    // /// Basic block argument reference.
    // KOOPA_RVT_BLOCK_ARG_REF,
    // } koopa_raw_value_tag_t;


    const auto &kind = value->kind;
    switch (kind.tag) {
        case KOOPA_RVT_RETURN:
            // 访问 return 指令
            Visit(kind.data.ret);
            break;

        case KOOPA_RVT_INTEGER:
            // 访问 integer 指令
            Visit(kind.data.integer);
            break;

        case KOOPA_RVT_BINARY:
            //访问二元运算
            Visit(kind.data.binary, value);
            break;

        case KOOPA_RVT_LOAD:
            //访问load
            Visit(kind.data.load, value);
            break;

        case KOOPA_RVT_STORE:
            //访问store
            Visit(kind.data.store);
            break;

        case KOOPA_RVT_ALLOC:
            //访问alloc，只需要分配好位置
            // addrmap[value] = rv.newreg();
            break;
        case KOOPA_RVT_BRANCH:
            //访问branch
            Visit(kind.data.branch);
            break;
        case KOOPA_RVT_JUMP:
            //访问jump
            Visit(kind.data.jump);
            break;
        case KOOPA_RVT_CALL:
            //访问call
            Visit(kind.data.call, value);
            break;
        case KOOPA_RVT_FUNC_ARG_REF:
            //访问func_arg_ref
            Visit(kind.data.func_arg_ref, value);
            break;

        case KOOPA_RVT_GLOBAL_ALLOC:
            //访问global_alloc
            Visit(kind.data.global_alloc, value);
            break;

        // case KOOPA_RVT_AGGREGATE:
        //     break;
        
        case KOOPA_RVT_GET_ELEM_PTR:
            Visit(kind.data.get_elem_ptr, value);
            break;
        
        case KOOPA_RVT_GET_PTR:
            Visit(kind.data.get_ptr, value);
            break;

        default:
        // 其他类型暂时遇不到
            //assert(false);
            printf("default: %d\n", kind.tag);
            break;
    }
}


void Visit(const koopa_raw_integer_t &integer) {
    visitstack.push(std::to_string(integer.value));
}

// visit return
void Visit(const koopa_raw_return_t &ret) {
    if(ret.value == nullptr){
        
    }
    else if(ret.value->kind.tag == KOOPA_RVT_INTEGER){
        Visit(ret.value);
        rv.loadi_op("a0", visitstack.top());
        visitstack.pop();
    }
    else{
        rv.loadw_op("a0", addrmap[ret.value]);
    }
    rv.ret_op();
}

void Visit(const koopa_raw_load_t &load, const koopa_raw_value_t &value){
    // typedef struct {
    // /// Source.
    // koopa_raw_value_t src;
    // } koopa_raw_load_t;
    //load source
    if(load.src->kind.tag == KOOPA_RVT_GLOBAL_ALLOC){
        rv.loada_op("t5", load.src->name + 1); //get pointer
        //rv.loadw_op("t0", "0(t0)"); //get value
        addrmap[value] = "0(t5)";
    }
    else if(load.src->kind.tag == KOOPA_RVT_ALLOC){
        //correct
        addrmap[value] = addrmap[load.src];
    }
    else{
        // correct
        rv.loadw_op("t0", addrmap[load.src]); //load address
        rv.loadw_op("t0", "0(t0)"); //get value
        rv.storew_op("t0", addrmap[load.src]); //store back
        addrmap[value] = addrmap[load.src];
    }
    
}

void Visit(const koopa_raw_store_t &store){
    // typedef struct {
    // /// Value.
    // koopa_raw_value_t value;
    // /// Destination.
    // koopa_raw_value_t dest;
    // } koopa_raw_store_t;

    // src
    if(store.value->kind.tag == KOOPA_RVT_INTEGER){
        Visit(store.value);
        rv.loadi_op("t0", visitstack.top());
        visitstack.pop();
    }
    else if(store.value->kind.tag == KOOPA_RVT_FUNC_ARG_REF){
        Visit(store.value);
        int idx = store.value->kind.data.func_arg_ref.index;
        if(idx < 8){
            rv.storew_op("a" + std::to_string(idx), addrmap[store.dest]);
            return ;
        }
        else{
            // find value from last function's stack
            rv.loadw_op("t0", std::to_string((idx - 8) * 4 + rv.totaloffset) + "(sp)"); 
        }
    }
    else{
        rv.loadw_op("t0", addrmap[store.value]);
    }

    //destination
    if(store.dest->kind.tag == KOOPA_RVT_GLOBAL_ALLOC){
        rv.loada_op("t1", store.dest->name + 1);
        rv.storew_op("t0", "0(t1)");
    }
    else if(store.dest->kind.tag == KOOPA_RVT_ALLOC){
        rv.storew_op("t0", addrmap[store.dest]);
    }
    else{
        rv.loadw_op("t1", addrmap[store.dest]);
        rv.storew_op("t0", "0(t1)");
    }

}

void Visit(const koopa_raw_binary_t &bin, const koopa_raw_value_t &value){
    //   typedef struct {
    //   /// Operator.
    //   koopa_raw_binary_op_t op; -> typedef uint32_t
    //   /// Left-hand side value.
    //   koopa_raw_value_t lhs;
    //   /// Right-hand side value.
    //   koopa_raw_value_t rhs;
    // } koopa_raw_binary_t;


    std::string left;
    std::string right;


    //left save to t0
    if(bin.lhs->kind.tag == KOOPA_RVT_INTEGER){
        Visit(bin.lhs);
        rv.loadi_op("t0", visitstack.top());
        visitstack.pop();
    }
    else{
        rv.loadw_op("t0", addrmap[bin.lhs]);   
    }

    //right save to t1
    if(bin.rhs->kind.tag == KOOPA_RVT_INTEGER){
        Visit(bin.rhs);
        rv.loadi_op("t1", visitstack.top());
        visitstack.pop();
    }
    else{
        rv.loadw_op("t1", addrmap[bin.rhs]); 
    }


    //根据指令op switch
    //   enum koopa_raw_binary_op {
    //   /// Not equal to.
    //   KOOPA_RBO_NOT_EQ,
    //   /// Equal to.
    //   KOOPA_RBO_EQ,
    //   /// Greater than.
    //   KOOPA_RBO_GT,
    //   /// Less than.
    //   KOOPA_RBO_LT,
    //   /// Greater than or equal to.
    //   KOOPA_RBO_GE,
    //   /// Less than or equal to.
    //   KOOPA_RBO_LE,
    //   /// Addition.
    //   KOOPA_RBO_ADD,
    //   /// Subtraction.
    //   KOOPA_RBO_SUB,
    //   /// Multiplication.
    //   KOOPA_RBO_MUL,
    //   /// Division.
    //   KOOPA_RBO_DIV,
    //   /// Modulo.
    //   KOOPA_RBO_MOD,
    //   /// Bitwise AND.
    //   KOOPA_RBO_AND,
    //   /// Bitwise OR.
    //   KOOPA_RBO_OR,
    //   /// Bitwise XOR.
    //   KOOPA_RBO_XOR,
    //   /// Shift left logical.
    //   KOOPA_RBO_SHL,
    //   /// Shift right logical.
    //   KOOPA_RBO_SHR,
    //   /// Shift right arithmetic.
    //   KOOPA_RBO_SAR,
    // };

    switch(bin.op){
        case KOOPA_RBO_NOT_EQ:
            rv.bin2_op("xor", "t0", "t0", "t1");
            rv.bin1_op("snez", "t0", "t0");
            break;
        case KOOPA_RBO_EQ:
            rv.bin2_op("xor", "t0", "t0", "t1");
            rv.bin1_op("seqz", "t0", "t0");
            break;
        case KOOPA_RBO_GT:
            rv.bin2_op("slt", "t0", "t1", "t0"); // swap left and right
            break;
        case KOOPA_RBO_LT:
            rv.bin2_op("slt", "t0", "t0", "t1");
            break;
        case KOOPA_RBO_GE:
            rv.bin2_op("slt", "t0", "t0", "t1"); 
            rv.bin1_op("seqz", "t0", "t0");
            break;
        case KOOPA_RBO_LE:
            rv.bin2_op("sgt", "t0", "t0", "t1");
            rv.bin1_op("seqz", "t0", "t0");
            break;
        case KOOPA_RBO_ADD:
            rv.bin2_op("add", "t0", "t0", "t1");
            break;
        case KOOPA_RBO_SUB:
            rv.bin2_op("sub", "t0", "t0", "t1");
            break;
        case KOOPA_RBO_MUL:
            rv.bin2_op("mul", "t0", "t0", "t1");
            break;
        case KOOPA_RBO_DIV:
            rv.bin2_op("div", "t0", "t0", "t1");
            break;
        case KOOPA_RBO_MOD:
            rv.bin2_op("rem", "t0", "t0", "t1");
            break;
        case KOOPA_RBO_AND:
            rv.bin2_op("and", "t0", "t0", "t1");
            break;
        case KOOPA_RBO_OR:
            rv.bin2_op("or", "t0", "t0", "t1");
            break;
        default:
            assert(false);
    }


    //store it into address map
    //addrmap[value] = rv.newreg();
    rv.storew_op("t0", addrmap[value]);
}

void Visit(const koopa_raw_branch_t &branch){
    // typedef struct {
    //     /// Condition.
    //     koopa_raw_value_t cond;
    //     /// Target if condition is `true`.
    //     koopa_raw_basic_block_t true_bb;
    //     /// Target if condition is `false`.
    //     koopa_raw_basic_block_t false_bb;
    //     /// Arguments of `true` target..
    //     koopa_raw_slice_t true_args;
    //     /// Arguments of `false` target..
    //     koopa_raw_slice_t false_args;
    // } koopa_raw_branch_t;

    if(branch.cond->kind.tag == KOOPA_RVT_INTEGER){
        Visit(branch.cond);
        rv.loadi_op("t0", visitstack.top());
        visitstack.pop();
    }
    else{
        rv.loadw_op("t0", addrmap[branch.cond]);
    }
    rv.bin1_op("bnez", "t0", branch.true_bb->name + 1);
    rv.bin0_op("j", branch.false_bb->name + 1);
}

void Visit(const koopa_raw_jump_t &jump){
    // typedef struct {
    //     /// Target.
    //     koopa_raw_basic_block_t target;
    //     /// Arguments of target..
    //     koopa_raw_slice_t args;
    // } koopa_raw_jump_t;

    rv.bin0_op("j", jump.target->name + 1);
}

void Visit(const koopa_raw_call_t &call, const koopa_raw_value_t &caller){
    // typedef struct {
    //     /// Callee.
    //     koopa_raw_function_t callee;
    //     /// Arguments.
    //     koopa_raw_slice_t args;
    // } koopa_raw_call_t;


    //save args (parameters)
    for(int i = 0; i < call.args.len; i++){
        auto arg = reinterpret_cast<koopa_raw_value_t> (call.args.buffer[i]);
        if(arg->kind.tag == KOOPA_RVT_INTEGER){
            Visit(arg);
            if(i < 8){ //first eight
                rv.loadi_op("a" + std::to_string(i), visitstack.top());
            }
            else{
                rv.loadi_op("t0", visitstack.top());
                rv.storew_op("t0", std::to_string((i - 8) * 4) + "(sp)");
            }
            visitstack.pop();
        }
        else{
            if(i < 8){ //first eight
                rv.loadw_op("a" + std::to_string(i), addrmap[arg]);
            }
            else{
                rv.loadw_op("t0", addrmap[arg]);
                rv.storew_op("t0", std::to_string((i - 8) * 4) + "(sp)");
            }
        }
    }

    //call function
    rv.bin0_op("call", call.callee->name + 1);

    //save return value
    if(call.callee->ty->data.function.ret->tag != KOOPA_RTT_UNIT){
        //addrmap[caller] = rv.newreg();
        rv.storew_op("a0", addrmap[caller]);
    }
}

void Visit(const koopa_raw_func_arg_ref_t &func_arg_ref, const koopa_raw_value_t &value){
    // typedef struct {
    //     /// Index.
    //     size_t index;
    // } koopa_raw_func_arg_ref_t;
    addrmap[value] = "a" + std::to_string(value->kind.data.func_arg_ref.index);
}

void initGlobalArray(koopa_raw_value_t init){
    if(init->kind.tag == KOOPA_RVT_INTEGER){
        Visit(init->kind.data.integer);
        rv.glob_init(visitstack.top());
    } else {
        // KOOPA_RVT_AGGREGATE
        auto elems = init->kind.data.aggregate.elems;
        for(int i = 0; i < elems.len; ++i){
            initGlobalArray(reinterpret_cast<koopa_raw_value_t>(elems.buffer[i]));
        }
    }
}

void Visit(const koopa_raw_global_alloc_t &global_alloc, const koopa_raw_value_t &value){
    // typedef struct {
    // /// Initializer.
    // koopa_raw_value_t init;
    // } koopa_raw_global_alloc_t;
    printf("global alloc\n");
    rv.glob_alloc(value->name + 1);
    koopa_raw_value_t init = global_alloc.init;
    auto ty = value->ty->data.pointer.base;
    if(ty->tag == KOOPA_RTT_INT32){
        if(init->kind.tag == KOOPA_RVT_ZERO_INIT){
            rv.zero_init(4);
        } 
        else {
            Visit(init->kind.data.integer);
            rv.glob_init(visitstack.top());
            visitstack.pop();
        }
    }
    else { // array
        if(init->kind.tag == KOOPA_RVT_AGGREGATE){
            printf("aggregate\n");
            // aggregate
            initGlobalArray(init);
        }
        else {
            // KOOPA_RVT_ZERO_INIT
            int size = getTypeSize(ty);
            rv.zero_init(size);
        }
    }
    rv.glob_end();
    return ;
}

void Visit(const koopa_raw_get_elem_ptr_t &get_elem_ptr, const koopa_raw_value_t &value){
    // typedef struct {
    // /// Source.
    // koopa_raw_value_t src;
    // /// Index.
    // koopa_raw_value_t index;
    // } koopa_raw_get_elem_ptr_t;



    //src
    if(get_elem_ptr.src->kind.tag == KOOPA_RVT_GLOBAL_ALLOC){
        rv.loada_op("t0", get_elem_ptr.src->name + 1);
    }
    else if(get_elem_ptr.src->kind.tag == KOOPA_RVT_ALLOC){
        std::string spval= addrmap[get_elem_ptr.src];
        int offset = stoi(spval.substr(0, spval.find("(")));
        if(offset > 2047){
            rv.loadi_op("t6", std::to_string(offset));
            rv.bin2_op("add", "t0", "sp", "t6");
        }
        else{
            rv.bin2_op("addi", "t0", "sp", std::to_string(offset));
        }

    }
    else{
        rv.loadw_op("t0", addrmap[get_elem_ptr.src]);
    }

    //index
    if(get_elem_ptr.index->kind.tag == KOOPA_RVT_INTEGER){
        Visit(get_elem_ptr.index);
        rv.loadi_op("t1", visitstack.top());
        visitstack.pop();
    }
    else{
        rv.loadw_op("t1", addrmap[get_elem_ptr.index]);
    }

    //store size
    rv.loadi_op("t2", std::to_string(getTypeSize(get_elem_ptr.src->ty->data.pointer.base->data.array.base)));

    //calculate
    rv.bin2_op("mul", "t1", "t1", "t2");
    rv.bin2_op("add", "t0", "t0", "t1");


    //store back into the original stack frame
    //addrmap[value] = rv.newreg();
    rv.storew_op("t0", addrmap[value]);
}

void Visit(const koopa_raw_get_ptr_t &get_ptr, const koopa_raw_value_t &value){
    // typedef struct {
    // /// Source.
    // koopa_raw_value_t src;
    // /// Index.
    // koopa_raw_value_t index;
    // } koopa_raw_get_ptr_t;

    //src
    if(get_ptr.src->kind.tag == KOOPA_RVT_GLOBAL_ALLOC){
        rv.loada_op("t0", get_ptr.src->name + 1);
    }
    else if(get_ptr.src->kind.tag == KOOPA_RVT_ALLOC){
        std::string spval= addrmap[get_ptr.src];
        int offset = stoi(spval.substr(0, spval.find("(")));
        if(offset > 2047){
            rv.loadi_op("t6", std::to_string(offset));
            rv.bin2_op("add", "t0", "sp", "t6");
        }
        else{
            rv.bin2_op("addi", "t0", "sp", std::to_string(offset));
        }
    }
    else{
        rv.loadw_op("t0", addrmap[get_ptr.src]);
    }

    //index
    if(get_ptr.index->kind.tag == KOOPA_RVT_INTEGER){
        Visit(get_ptr.index);
        rv.loadi_op("t1", visitstack.top());
        visitstack.pop();
    }
    else{
        rv.loadw_op("t1", addrmap[get_ptr.index]);
    }

    //store size
    rv.loadi_op("t2", std::to_string(getTypeSize(get_ptr.src->ty->data.pointer.base)));

    //calculate
    rv.bin2_op("mul", "t1", "t1", "t2");
    rv.bin2_op("add", "t0", "t0", "t1");

    //store back into the original stack frame
    //addrmap[value] = rv.newreg();
    rv.storew_op("t0", addrmap[value]);
}



// void Visit(const koopa_raw_aggregate_t &aggregate){
//     // typedef struct {
//     // /// Elements.
//     // koopa_raw_slice_t elems;
//     // } koopa_raw_aggregate_t;

//     // aggregate
//     initGlobalArray(aggregate);
// }



// 访问对应类型指令的函数定义略
// 视需求自行实现
// ...


#endif