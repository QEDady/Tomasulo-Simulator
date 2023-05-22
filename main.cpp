#include "tomasulo.h"

int main() {
     string instruction_file, memory_file, hardware_file;
     char mode, mem_init, cust_hardware; 
     int initial_pc = 0;
     cout << "-------------------- Welcome to our Tomasulo simulator --------------------\n"
          << "First, please enter the path of the instructions file: ";
     cin >> instruction_file;
     cout << "Please precify if you want to enter the tutorial mode (program state after each instruction) or normal mode\n"
          << "T: Tutorial Mode\n" 
          << "N: Normal Mode\n";
     cin >> mode;
     while (tolower(mode) != 't' && tolower(mode) != 'n') {
          cout << "Sorry, this is not a valid selection of a mode, please try again\n"
               << "T: Tutorial Mode\n" 
               << "N: Normal Mode\n";
          cin >> mode;
     }

     cout << "Please select your starting address for the instructions (initial pc): ";
     cin >> initial_pc;

     cout << "Now, please select if you want a custom hardware for you program (Y, N) ";
     cin >> cust_hardware;
     while (tolower(cust_hardware) != 'y' && tolower(cust_hardware) != 'n') {
          cout << "Sorry, this is not a valid selection, please try again\n"
               << "Choose Y or N\n";
          cin >> cust_hardware;
     }

     if (tolower(cust_hardware) == 'y') {
          cout << "Then, please enter the path to the hardware description file: ";
          cin >> hardware_file;
     }

     Tomasulo tomasulo(instruction_file, (tolower(cust_hardware) == 'n'), hardware_file, (tolower(mode) == 't'), initial_pc);
     
     cout << "Now, do you want to initize the memory for you program (Y, N) ";
     cin >> mem_init;
     while (tolower(mem_init) != 'y' && tolower(mem_init) != 'n') {
          cout << "Sorry, this is not a valid selection, please try again\n"
               << "Choose Y or N\n";
          cin >> mem_init;
     }

     if (tolower(mem_init) == 'y') {
          cout << "Then, please enter the path to the memory initialization file: ";
          cin >> memory_file;

          tomasulo.read_mem(memory_file);
     }
     
     tomasulo.initiate_running();
}