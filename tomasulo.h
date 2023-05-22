#ifndef TOMASULO_H
#define TOMASULO_H
#include "instruction.h"

#define NStationTypes 8
#define NRegisters 8

struct ReservationStation {
    int id; //Starting from 1
    bool busy;
    Inst_Op op;
    int vj, vk;
    int qj, qk;
    int addr;

    int cycles_per_exec;
    int cycles_per_addr;
    int rem_cycles_exec;
    int rem_cycles_addr;

    int inst_index; //instruction index in the current queue
    int result;

    ReservationStation(int id, int cycles_per_exec, int cycles_per_addr) {
        this->id = id;
        busy = 0;
        vj = vk = addr = 0;
        qj = qk = 0;
        this->cycles_per_exec = cycles_per_exec;
        this->cycles_per_addr = cycles_per_addr;
    }
};

struct SystemState{
    int issue;
    int register_stat[8] = {};
    SystemState(int issue, int* register_stat){
        this->issue = issue;
        for(int i=0; i< NRegisters; i++){
            this->register_stat[i] = register_stat[i];
        }   
    }
};


vector<string> split_line(string line, char delimiter = ' ') {
    vector<string> words;
    string cur_word = "";
    for (auto &u : line) {
        if (u == delimiter) {
            if (cur_word.length()) words.push_back(cur_word);
        } else cur_word += u;
    }
    if (cur_word.length()) words.push_back(cur_word);
    return words;
}

class Tomasulo {
private:
    short int mem[1 << 16]; // word addressable with word size of 16 bits
    short int regs[NRegisters];
    vector<Instruction> program;
  ///  deque<Instruction> running_instructions;
    vector<ReservationStation> stations[NStationTypes]; // array of vectors of the default NStationTypes reseavation station types
    map<int, pair<int, int>> id_to_rs; // absolute station id to its location in the 2d array stations 
    int register_stat[NRegisters]; // register to it's waiting station  
    deque<int> load_store_queue; // indices of load_store instructions issue
    queue<SystemState> BNE_states; //Saving system states before knowing BNE
    int pc = 0; 
    int cycle = 1;

    void issue() {
        for (auto &s: stations[program[pc].categ]) {
            if (!s.busy) {
                program[pc].issue = cycle;
                s.busy = 1;
                s.addr = program[pc].imm;
                s.op = program[pc].op;
                s.rem_cycles_addr = s.cycles_per_addr;
                s.rem_cycles_exec = s.cycles_per_exec;
                //s.inst_index = running_instructions.size();
                s.inst_index = pc;
                if (register_stat[program[pc].rs] != 0) 
                    s.qj = register_stat[program[pc].rs];
                else {
                    s.vj = regs[program[pc].rs];
                    s.qj = 0;
                }

                if (register_stat[program[pc].rt] != 0) 
                    s.qk = register_stat[program[pc].rt];
                else {
                    s.vk = regs[program[pc].rt];
                    s.qk = 0;
                }

                if (program[pc].rd != 0) {
                    if(BNE_states.empty())
                        register_stat[program[pc].rd] = s.id;
                    else
                        BNE_states.back().register_stat[program[pc].rd] = s.id;
                }
                if (program[pc].categ == LOAD || program[pc].categ == STORE) 
                    load_store_queue.push_back(pc);
                
                if(program[pc].categ == BNE){
                    BNE_states.push(SystemState(cycle, register_stat));
                }
               // running_instructions.push_back(&program[pc]);
                pc += 1;
                break;
            }
        }
    }

    void execution_logic(int category, ReservationStation& station){
        if (category == LOAD){
            station.result = mem[station.addr];
        }
        else if (category == BNE){
            station.result = (station.vj != station.vk);
        }
        else if (category == JUMP){
            if (station.op == JAL){
                station.result = station.inst_index + 1;
            }       
        }
        else if(category == ADDITION){
            if (station.op == ADD){
                station.result = station.vj + station.vk;
            }
            else{
                  station.result = station.vj + station.addr;
            }
        }
        else if(category == NEG){
            station.result = -1 * station.vj;
        }
        else if(category == NAND){
            station.result = ~(station.vj & station.vk);
        } 
        else if (category == SLL) {
            station.result = station.vj << station.vk;
        }
    }

    void execute(){

        // Non-Load and Non-Store stations
        for(int i = 2; i < NStationTypes; i++) {
            for (auto &s: stations[i]){
                if(s.busy && program[s.inst_index].issue < cycle){
                    if (!BNE_states.empty() && program[s.inst_index].issue > BNE_states.front().issue) 
                        continue;  // No execution for those after BNE until BNE writes back. 
                    if(s.qj == 0 && s.qk == 0 && program[s.inst_index].issue < cycle) {
                        if(s.rem_cycles_exec == s.cycles_per_exec){
                            program[s.inst_index].exec_st = cycle;
                        }
                        if(s.rem_cycles_exec != 0)
                            s.rem_cycles_exec--;
                    }
                    if(s.rem_cycles_exec == 0){
                        execution_logic(i, s);
                        program[s.inst_index].exec_end = cycle;
                    }
                }
            }
        }
        
        // Load and Store stations
        bool queue_pop = false;
        for (int i = 0; i < 2; i++) {
            for (auto &s: stations[i]) {
                if (s.busy && program[s.inst_index].issue < cycle) {
                    if (!BNE_states.empty() && program[s.inst_index].issue > BNE_states.front().issue) 
                        continue;  // No execution for those after BNE until BNE writes back. 
                    if (s.rem_cycles_addr) {
                        if (s.qj == 0 && load_store_queue.size() && load_store_queue.front() == s.inst_index) {
                            if (s.rem_cycles_addr == s.cycles_per_addr)
                                program[s.inst_index].exec_st = cycle;
                            if (s.rem_cycles_addr != 0) 
                                s.rem_cycles_addr--;
                        }

                        if (s.rem_cycles_addr == 0) {
                            s.result = s.addr + s.vj;
                            queue_pop = true;
                        }
                    } else if (s.rem_cycles_exec){
                        bool AW, WAR; // Handling Memorry Hazards
                        AW = WAR = false;
                        //After Write Hazard
                        for (auto &store_s : stations[1]) 
                            if (store_s.busy && program[store_s.inst_index].wb == 0 
                                && program[store_s.inst_index].issue < program[s.inst_index].issue && store_s.addr == s.addr) {
                                AW = true;
                                break;
                            }

                        if (i == 1) { // WAR hazard 
                            for (auto &load_s : stations[0]) 
                                if (load_s.busy && load_s.rem_cycles_exec != 0 
                                    && program[load_s.inst_index].issue < program[s.inst_index].issue && load_s.addr == s.addr) {
                                    WAR = true;
                                    break;
                                }                        
                            if (!WAR && !AW) s.rem_cycles_exec--;
                        } else {
                            if (!AW) s.rem_cycles_exec--;
                        }

                        if (s.rem_cycles_exec == 0) {
                            program[s.inst_index].exec_end = cycle;
                            if (i == 0) s.result = mem[s.addr];
                        }
                    }
                }
            }
        }
        if (queue_pop) load_store_queue.pop_front();
    }

    void write_back(){
            //s.rem_cycles_exec == 0 && exec_end!= cycle
        int write_s_id = -1;
        int min_issue_time = INT_MAX;
        for(int i = 0; i < NStationTypes; i++){
            for(auto& s: stations[i]){
                if(s.busy && s.rem_cycles_exec == 0 && program[s.inst_index].exec_end > cycle){
                    if (i == STORE && s.qk != 0)
                        continue;
                    if(program[s.inst_index].issue < min_issue_time){
                        min_issue_time = program[s.inst_index].issue;
                        write_s_id = s.id;
                    }
                }
            }
        }
        
        if(write_s_id != -1){
            ReservationStation& write_s = stations[id_to_rs[write_s_id].first][id_to_rs[write_s_id].second];
            write_s.busy = false;
            int type = id_to_rs[write_s_id].first; 
            if (type == STORE){
                mem[write_s.addr] = write_s.vk;
            }
            else if (type == JUMP){
                if(write_s.op == JAL)
                    pc = write_s.addr + program[write_s.inst_index].index + 1;
                else
                    pc = regs[1]; ///Not sure. Should we save this value in execution?

                for (int i=0; i< NStationTypes;i++){ // flushing unneeded issued instructions
                    for(auto& s: stations[i]){
                        if(s.busy && program[s.inst_index].issue > program[write_s.inst_index].issue){
                            program[s.inst_index].flush();
                            s.busy = 0;
                            for (int j = 0; j < NRegisters; j++){
                                if(register_stat[j] == s.id)
                                    register_stat[j] = 0;
                            }
                        }
                    }
                }

                //Flush load_store_queue queue
                while (!load_store_queue.empty() && 
                        program[load_store_queue.back()].issue > program[write_s.inst_index].issue)
                {
                    load_store_queue.pop_back();
                }
            }
            else if (type == BNE){
                if(write_s.result){ // The prediction is wrong, and we need to take the branch
                    pc = program[write_s.inst_index].index + 1 + write_s.addr; // Update PC
                    
                    while(!BNE_states.empty())
                        BNE_states.pop(); // empty all the states since the first BNE is taken

                    for (int i=0; i< NStationTypes;i++){ // flushing unneeded issued instructions
                        for(auto& s: stations[i]){
                            if(s.busy && program[s.inst_index].issue > program[write_s.inst_index].issue){
                                program[s.inst_index].flush();
                                s.busy = 0;
                                for (int j = 0; j < NRegisters; j++){
                                    if(register_stat[j] == s.id)
                                        register_stat[j] = 0;
                                }
                            }
                        }
                    }
                    
                    //Flush load_store_queue queue
                    while (!load_store_queue.empty() && 
                            program[load_store_queue.back()].issue > program[write_s.inst_index].issue)
                    {
                        load_store_queue.pop_back();
                    }   
                }
                else{// Prediction is right, we update the actual system state with the buffered one.
                    for (int i=0; i < 8 ;i++){
                        register_stat[i] = BNE_states.front().register_stat[i];
                    }
                    BNE_states.pop();
                }
            }

            if (type != STORE && type!= BNE){
                for (int i=0; i < NRegisters; i++){
                    if( i !=0 && register_stat[i] == write_s_id){ // make sure we don't change reg0. It is not needed but just a safety.
                        regs[i] = write_s.result;
                        register_stat[i] = 0;   
                    }
                }
                
                for (int i = 0; i< NStationTypes; i++){
                    for (auto& s: stations[i]){
                        if(s.qj = write_s_id){
                            s.vj = write_s.result;
                            s.qj = 0;
                        }
                        if(s.qk == write_s_id){
                            s.vk = write_s.result;
                            s.qk = 0;
                        }
                    }
                }
            }
        }
        

    }
    void next_cycle(){

        if (pc < program.size()) 
            issue();
        execute();
        write_back();
        cycle++;
    }

public:

    Tomasulo() {
        for (auto &reg: regs) reg = 0;
    }

    void read_hardware(string hardware_file) {
        fstream f_hardware(hardware_file);
        string line;

        int rs_id = 0;
        for (int i = 0; i < NStationTypes; i++) {
            int n_units, exec_cycles, addr_cycles = 0;
            getline(f_hardware, line);
            vector<string> line_data = split_line(line);

            n_units = stoi(line_data[0]);
            exec_cycles = stoi(line_data[1]);
            if (i < 2) {
                addr_cycles = stoi(line_data[2]);
            }

            for (int j = 0; j < n_units; j++) {
                stations[i].push_back(ReservationStation(++rs_id, exec_cycles, addr_cycles));
                id_to_rs[rs_id] = {i, stations[i].size() - 1};
            }
        }
    }

    void read_mem(string mem_file) {
        fstream f_mem(mem_file);
        string line;
        while (!f_mem.eof()) {
            int address, value;
            getline(f_mem, line);
            vector<string> line_data = split_line(line); 
            address = stoi(line_data[0]);
            value = stoi(line_data[1]);
            mem[address] = value;
        }
    }

    void read_inst_file(string inst_file) {
        fstream f_inst(inst_file);
        string line;
        while (!f_inst.eof()) {
            getline(f_inst, line);
            Instruction inst(line, program.size());
            program.push_back(inst);
        }
    }
};


#endif