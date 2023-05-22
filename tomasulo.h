#ifndef TOMASULO_H
#define TOMASULO_H
#include "instruction.h"

#define NStationTypes 8
#define NRegisters 8

struct ReservationStation {
    int id; //Starting from 1
    string name;
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

    ReservationStation(string name, int id, int cycles_per_exec, int cycles_per_addr) : name(name), id(id) {
        // this->id = id;
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
            if (cur_word.length()){ 
                words.push_back(cur_word);
                cur_word = "";
            }
        } else cur_word += u;
    }
    if (cur_word.length()) words.push_back(cur_word);
    return words;
}

class Tomasulo {
private:
    short int mem[1 << 16] = {}; // word addressable with word size of 16 bits
    short int regs[NRegisters];
    vector<Instruction> program;
  ///  deque<Instruction> running_instructions;
    vector<ReservationStation> stations[NStationTypes]; // array of vectors of the default NStationTypes reseavation station types
    map<int, pair<int, int>> id_to_rs; // absolute station id to its location in the 2d array stations 
    int register_stat[NRegisters]; // register to it's waiting station  
    deque<int> load_store_queue; // indices of load_store instructions issue
    queue<SystemState> system_states; //Saving system states before knowing BNE or Jumping
    int pc = 0; 
    int cycle;
    bool tutorial_mode;

    // final calculations
    int num_bne;
    int misprediction; 
    int wb_insts;

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
                    if(system_states.empty())
                        register_stat[program[pc].rd] = s.id;
                    else
                        system_states.back().register_stat[program[pc].rd] = s.id;
                }
                if (program[pc].categ == LOAD || program[pc].categ == STORE) 
                    load_store_queue.push_back(pc);
                
                if(program[pc].categ == BNE || program[pc].categ == JUMP){
                    if(system_states.empty())
                        system_states.push(SystemState(cycle, register_stat));
                    else
                        system_states.push(SystemState(cycle, system_states.back().register_stat));
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
            else{
                station.result = station.vj;
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
                if(s.busy && program[s.inst_index].issue < cycle) {
                    if (!system_states.empty() && program[s.inst_index].issue > system_states.front().issue) 
                        continue;  // No execution for those after BNE until BNE writes back. 
                    if (s.qj == 0 && s.qk == 0 && program[s.inst_index].issue < cycle && s.rem_cycles_exec) {
                        if (s.rem_cycles_exec == s.cycles_per_exec) {
                            program[s.inst_index].exec_st = cycle;
                        }
                        if (s.rem_cycles_exec != 0)
                            s.rem_cycles_exec--;

                        if (s.rem_cycles_exec == 0) {
                            execution_logic(i, s);
                            program[s.inst_index].exec_end = cycle;
                        }
                    }
                }
            }
        }
        
        // Load and Store stations
        bool queue_pop = false;
        for (int i = 0; i < 2; i++) {
            for (auto &s: stations[i]) {
                if (s.busy && program[s.inst_index].issue < cycle) {
                    if (!system_states.empty() && program[s.inst_index].issue > system_states.front().issue) 
                        continue;  // No execution for those after BNE until BNE writes back. 
                    if (s.rem_cycles_addr) {
                        if (s.qj == 0 && load_store_queue.size() && load_store_queue.front() == s.inst_index) {
                            if (s.rem_cycles_addr == s.cycles_per_addr)
                                program[s.inst_index].exec_st = cycle;
                            if (s.rem_cycles_addr != 0) 
                                s.rem_cycles_addr--;
                             if (s.rem_cycles_addr == 0) {
                                s.result = s.addr + s.vj;
                                queue_pop = true;
                            }
                        }
                       
                    } else if (s.rem_cycles_exec){
                        bool AW, WAR; // Handling Memorry Hazards
                        AW = WAR = false;
                        //After Write Hazard
                        for (auto &store_s : stations[STORE]) 
                            if (store_s.busy && program[store_s.inst_index].wb == 0 
                                && program[store_s.inst_index].issue < program[s.inst_index].issue && store_s.addr == s.addr) {
                                AW = true;
                                break;
                            }

                        if (i == 1) { // WAR hazard 
                            for (auto &load_s : stations[LOAD]) 
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
        int write_store_id = -1;
        int min_issue_time = INT_MAX;
        int min_store_issue_time = INT_MAX;
        for (int i = 0; i < NStationTypes; i++) {
            for (auto& s: stations[i]) {
                if(s.busy && s.rem_cycles_exec == 0 && program[s.inst_index].exec_end < cycle){
                    if (i == STORE && s.qk != 0)
                        continue;
                    if (program[s.inst_index].issue < min_issue_time) {
                        if (i == STORE) {
                            min_store_issue_time = program[s.inst_index].issue;
                            write_store_id = s.id;
                        } else {
                            min_issue_time = program[s.inst_index].issue;
                            write_s_id = s.id;
                        }
                    }
                }
            }
        }

        if (write_store_id != -1) {
            wb_insts++;

            ReservationStation& write_s = stations[id_to_rs[write_store_id].first][id_to_rs[write_store_id].second];
            write_s.busy = false;
            program[write_s.inst_index].wb = cycle;
            mem[write_s.addr] = write_s.vk;
        }
        if(write_s_id != -1){
            wb_insts++;

            ReservationStation& write_s = stations[id_to_rs[write_s_id].first][id_to_rs[write_s_id].second];
            write_s.busy = false;
            program[write_s.inst_index].wb = cycle;
            int type = id_to_rs[write_s_id].first; 
            if (type == JUMP){
                if(write_s.op == JAL)
                    pc = write_s.addr + program[write_s.inst_index].index + 1;
                else
                    pc = write_s.result; // RET
                
                while(!system_states.empty())
                    system_states.pop(); // empty all the states since the jump is done
                    
                for (int i=0; i< NStationTypes;i++){ // flushing unneeded issued instructions
                    for(auto& s: stations[i]){
                        if(s.busy && program[s.inst_index].issue > program[write_s.inst_index].issue){
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
                num_bne++;
                if(write_s.result){ // The prediction is wrong, and we need to take the branch
                    pc = program[write_s.inst_index].index + 1 + write_s.addr; // Update PC
                    
                    misprediction++;

                    while(!system_states.empty())
                        system_states.pop(); // empty all the states since the first BNE is taken

                    for (int i=0; i< NStationTypes;i++){ // flushing unneeded issued instructions
                        for(auto& s: stations[i]){
                            if(s.busy && program[s.inst_index].issue > program[write_s.inst_index].issue){
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
                        register_stat[i] = system_states.front().register_stat[i];
                    }
                    system_states.pop();
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
                        if(s.qj == write_s_id){
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

    void next_cycle() {
        if (pc < program.size()) 
            issue();
        execute();
        write_back();
        cycle++;
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

    void read_hardware(bool is_default, string hardware_file) {
        vector<string> st = {"load", "store", "bne", "jump", "add", "neg", "nand", "sll"};

        if (is_default) {
            vector<vector<int>> def = {{2, 1, 1}, {2, 1, 1}, {1, 1}, {1, 1}, 
                                       {3, 2}, {1, 2}, {1, 1}, {1, 8}}; // default settings from the project description
            
            int rs_id = 0;
            for (int i = 0; i < NStationTypes; i++) {
                int n_units, exec_cycles, addr_cycles = 0;
                n_units = def[i][0];
                exec_cycles = def[i][1];
                if (i < 2) {
                    addr_cycles = def[i][2];
                }

                for (int j = 0; j < n_units; j++) {
                    string name = st[i] + (n_units > 1 ? to_string(j + 1) : "");
                    stations[i].push_back(ReservationStation(name, ++rs_id, exec_cycles, addr_cycles));
                    id_to_rs[rs_id] = {i, stations[i].size() - 1};
                }
            }
        } else {
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
                    string name = st[i] + (n_units > 1 ? to_string(j + 1) : "");
                    stations[i].push_back(ReservationStation(name, ++rs_id, exec_cycles, addr_cycles));
                    id_to_rs[rs_id] = {i, stations[i].size() - 1};
                }
            }
        }
    }

    void print_reservation_stations() {
        cout << "Current cycle: " << cycle - 1 << "\n";

        cout << "Reservation stations\n";
        cout << "Name\t\t" 
             << "Busy\t\t"
             << "Op\t\t"
             << "Vj\t\t"
             << "Vk\t\t"
             << "Qj\t\t"
             << "Qk\t\t"
             << "Addr\t\t"
             << "\n";

        for (int i = 0; i < NStationTypes; i++) {
            for (auto &s: stations[i]) {
                if (s.busy) {
                    cout << s.name << "\t\t"
                        << s.busy << "\t\t"
                        << program[s.inst_index].op_str << "\t\t"
                        << s.vj << "\t\t"
                        << s.vk << "\t\t"
                        << s.qj << "\t\t"
                        << s.qk << "\t\t"
                        << s.addr << "\t\t"
                        << "\n";
                } else {
                    cout << s.name << "\t\t"
                        << s.busy << "\t\t"
                        << "\n";
                }
            }
        }
        cout << "--------------------------------------------------------\n"; 

        cout << "Register State Table\t\t\t";
        cout << "RegFile\n";

        cout << "Register\t"
             << "Stat\t\t\t"
             << "Register\t"
             << "Value\t"
             << "\n";

        for (int i = 0; i < NRegisters; i++) {
            cout << i << "\t\t"
                 << register_stat[i] << "\t\t\t"
                 << i << "\t\t"
                 << regs[i] << "\t\t"
                 << "\n";
        }

        cout << "--------------------------------------------------------\n"; 
    }

    void print_final_instructions_details() {
        cout << "Instruction\t\t" 
             << "Issue\t\t"
             << "Exec_start\t"
             << "Exec_end\t"
             << "Write_back\t\t"
             << "\n";

        for (auto &inst: program) {
            cout << inst.inst << "\t\t" 
                 << inst.issue << "\t\t"
                 << inst.exec_st << "\t\t"
                 << inst.exec_end << "\t\t"
                 << inst.wb << "\t\t"
                 << "\n";
        }   
        cout << "--------------------------------------------------------\n"; 
    }

    void print_calculations() {
        double misprediction_rate = 0;
        double IPC = (double) wb_insts / (cycle - 1);  
        if (num_bne) misprediction_rate = (double) misprediction / num_bne;
        
        cout << "All instructions finished in " << cycle - 1 << " cycles\n"; 
        cout << "Misprediction rate = " << misprediction_rate << "\n";
        cout << "IPC = " << IPC << "\n";
        cout << "--------------------------------------------------------\n"; 
    }

public:

    Tomasulo(string instruction_file, bool is_default_hardware, string hardware_file, bool tutorial_mode, int pc) : pc(pc), tutorial_mode(tutorial_mode) {    
        cycle = 1;
        for (auto &reg: regs) reg = 0;
        for (auto &reg: register_stat) reg = 0;

        // final calculations
        num_bne = 0;
        misprediction = 0;
        wb_insts = 0;
        
        read_inst_file(instruction_file);
        read_hardware(is_default_hardware, hardware_file); 
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

    void initiate_running() {
        bool stations_busy = true;

        while (stations_busy || pc < program.size()) {
            next_cycle();
               
            if (tutorial_mode){
                print_reservation_stations();
            }

            stations_busy = false;
            for (auto &station_type: stations) {
                for (auto &s: station_type) {
                    if (s.busy) {
                        stations_busy = true;
                        break;
                    }
                }
            }
        }

        print_final_instructions_details();
        print_calculations();
    }
};


#endif