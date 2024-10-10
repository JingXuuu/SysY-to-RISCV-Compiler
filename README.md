# 基于 Makefile 的 SysY 编译器项目

## 前言

这是北京大学编译原理（2023秋）的课程大作业，本项目在线评测系统中的分数为：

- 从SysY生成KoopaIR：99/100

- 从KoopaIR生成RISCV-V汇编：95/100

（Debug了很久，找了很多测试样例，都没测出哪里有Bug。下面附上了搜到的一些测试样例。）

Lab文档：https://pku-minic.github.io/online-doc/#/



## 编译器概述

本编译器基本具备如下功能：
1. 利用C++语言构造出AST
2. 利用AST编译成koopair中间代码
3. 将koopair中间代码编译成riscv底层代码
4. 可正确编译四则运算
5. 可正确编译局部和全局变量定义及运算
6. 可正确编译If条件判断，并在特定情形下利用短路提高程序效率
7. 可正确编译while循环程序
8. 可正确编译函数调用
9. 可几乎正确编译一维及多维数组定义及运算
10. 可正确编译拥有嵌套的程序



## 额外测试

从网上搜出许多测试样例，忘了出处，因此把所有找到的测试样例放在以下网盘内。

链接: https://pan.baidu.com/s/16uvNsM6WTsOdKE4MELcV-w?pwd=4s3t 提取码: 4s3t 

