#include "MemoryAllocator.h"
#include "PageTableManager.h"
//#include "Process.h"
#include "ProcessScheduler.h"
#include <MMU.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <list>

using mem::MMU;

using std::cerr;
using std::cout;
using std::string;
using std::vector;

int main(int argc, char* argv[]) {
  
    // Use command line argument as file name
    if (argc < 3) { //
      std::cerr << "usage: Program3 input_file\n";
      exit(1);
    }

    int quantum = atoi(argv[1]);
    mem::MMU memory(0x80);  // fixed memory size of 128 pages
    MemoryAllocator allocator(memory);
    PageTableManager ptm(memory, allocator);
    std::list<Process> processList;

    // create list of p's (std::list),
    // go through list of file names, construct new p
  
    for(int fileNum = 2; fileNum < argc; ++fileNum){ //

        std::cout << argv[fileNum] << "\n";
        processList.emplace_back(argv[fileNum], fileNum, memory, ptm, allocator);

    }
    
    ProcessScheduler pS(processList, quantum);  
    
}

