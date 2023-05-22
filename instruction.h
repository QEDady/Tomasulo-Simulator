#ifndef INSTRUCTION_H
#define INSTRUCTION_H
#include <bits/stdc++.h>
using namespace std;

enum Inst_Categ {LOAD, STORE, BNE, JUMP, ADDITION, NEG, NAND, SLL};
enum Inst_Op {JAL, RET, ADD, ADDI};

string lower(string a){
    for (auto &x:a)
        x = tolower(x);
    
    return a;
}

class Instruction {
public:
    int issue, exec_st, exec_end, wb;
    int rd, rs, rt, imm;
    Inst_Categ categ;
    Inst_Op op;
    int index;
    // int pc;

    Instruction(string _inst, int _index) {
        string inst = _inst;
        issue = exec_st = exec_end = wb = 0;
        rd = rs = rt = imm = 0;
        index = _index;
        // pc = _index;
        stringstream ss(_inst);
        string given_op, str;
        ss >> given_op;

        if (lower(given_op) == "load") {
            categ = LOAD;
            ss >> str;    rd = str[1] - '0';
            ss >> str;    getline(ss, str, '(');
            imm = stoi(str);
            ss >> str;    rs = str[str.size() - 2] - '0';
        }
        else if (lower(given_op) == "store") {
            categ = STORE;
            ss >> str;    rt = str[1] - '0';
            ss >> str;    getline(ss, str, '(');
            imm = stoi(str);
            ss >> str;    rs = str[str.size() - 2] - '0';
        }
        else if (lower(given_op) == "bne") {
            categ = BNE;
            ss >> str;    rs = str[1] - '0';
            ss >> str;    rt = str[1] - '0';
            ss >> str;    imm = stoi(str);
        }
        else if (lower(given_op) == "jal") {
            categ = JUMP;
            op = JAL;
            rd = 1;
            ss >> str;    imm = stoi(str);
        }
        else if (lower(given_op) == "ret") {
            categ = JUMP;
            op = RET;
        }
        else if (lower(given_op) == "add") {
            categ = ADDITION;
            op = ADD;
            ss >> str;    rd = str[1] - '0';
            ss >> str;    rs = str[1] - '0';
            ss >> str;    rt = str[1] - '0';
        }
        else if (lower(given_op) == "addi") {
            categ = ADDITION;
            op = ADDI;
            ss >> str;    rd = str[1] - '0';
            ss >> str;    rs = str[1] - '0';
            ss >> str;    imm = stoi(str);
        }
        else if (lower(given_op) == "neg") {
            categ = NEG;
            ss >> str;    rd = str[1] - '0';
            ss >> str;    rs = str[1] - '0';
        }
        else if (lower(given_op) == "nand") {
            categ = NAND;
            ss >> str;    rd = str[1] - '0';
            ss >> str;    rs = str[1] - '0';
            ss >> str;    rt = str[1] - '0';
        }
        else if (lower(given_op) == "sll") {
            categ = SLL;
            ss >> str;    rd = str[1] - '0';
            ss >> str;    rs = str[1] - '0';
            ss >> str;    rt = str[1] - '0';
        }
        else {
            cout << "The program will terminate due to unsupported instructions";
            exit(1);
        }

        if (imm < -64 || imm > 36) {
            cout << "The program will terminate due to invalid input";
            exit(1);
        }
    }

    void flush() {
        issue = exec_st = exec_end = wb = 0;
    }
};


#endif
