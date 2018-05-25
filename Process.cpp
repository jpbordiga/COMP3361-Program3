#include "Process.h"
#include "PageTableManager.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <sstream>

using mem::Addr;
using mem::kPageSize;
using std::cin;
using std::cout;
using std::cerr;
using std::getline;
using std::istringstream;
using std::string;
using std::vector;

Process::Process(const string &file_name_, int processNumber, mem::MMU &memory_, PageTableManager &ptm_, MemoryAllocator &allocator_) 
: file_name(file_name_), process_number(processNumber), line_number(0), current_num_pages(0), memory(memory_), ptm(ptm_), allocator(allocator_) {
  // Open the trace file.  Abort program if can't open.
  trace.open(file_name, std::ios_base::in);
  if (!trace.is_open()) {
    cerr << "ERROR: failed to open trace file: " << file_name << "\n";
    exit(2);
  }
  
  // Set up empty process page table and load process PMCB
  proc_pmcb.page_table_base = ptm.CreateProcessPageTable();
  memory.set_PMCB(proc_pmcb);
}

Process::~Process() {
  trace.close();
}

bool Process::Run(int n) { // mod 
    
    // switch to current p's pt
    
    memory.set_PMCB(proc_pmcb);
    
    // Read and process commands
    string line;                // text line read
    string cmd;                 // command from line
    vector<uint32_t> cmdArgs;   // arguments from line

    // Select the command to execute
    for (int i = 0; i < n; ++i){
        
        if(ParseCommand(line, cmd, cmdArgs)){
        
            if (cmd == "pagelimit" ) {
              CmdPageLimit(line, cmd, cmdArgs);         // allocate and map pages
            } else if (cmd == "diff") {
              CmdDiff(line, cmd, cmdArgs);        // get and compare multiple bytes
            } else if (cmd == "store") {
              CmdStore(line, cmd, cmdArgs);       // put bytes
            } else if (cmd == "replicate") {
              CmdRepl(line, cmd, cmdArgs);        // fill bytes with value
            } else if (cmd == "duplicate") {
              CmdDupl(line, cmd, cmdArgs);        // copy bytes to dest from source
            } else if (cmd == "print") {
              CmdPrint(line, cmd, cmdArgs);       // dump byte values to output
            } else if (cmd == "permission") {
              CmdPermission(line, cmd, cmdArgs);  // change page permissions
            } else if (cmd != "#") {
              cout << "ERROR: invalid command\n";
              exit(2);
            }
      
        } else {
            
            // free up pages (change to kernel and free up pages, free up p pt)
            
            ptm.SwitchToKernelPageTable();
            ptm.FreePageFrames(current_num_pages); //
            
            memory.set_PMCB(proc_pmcb);
            ptm.FreePageFrames(current_num_pages);
            
            std::cout << line_number << ":" << process_number << ":TERMINATED, free page frames = " << allocator.get_page_frames_free() << "\n";
            
            return false;
            
        }
            
    }
    
}

bool Process::ParseCommand(
    string &line, string &cmd, vector<uint32_t> &cmdArgs) {
  cmdArgs.clear();
  line.clear();
  
  // Read next line
  if (std::getline(trace, line)) {
    ++line_number;
    cout << std::dec << line_number << ":" << line << "\n";
    
    // No further processing if comment or empty line
    if (line.size() == 0 || line[0] == '#') {
      cmd = "#";
      return true;
    }
    
    // Make a string stream from command line
    istringstream lineStream(line);
    
    // Get command
    if (!(lineStream >> cmd)) {
      // Blank line, treat as comment
      cmd = "#";
      return true;
    }
    
    // Get arguments
    uint32_t arg;
    while (lineStream >> std::hex >> arg) {
      cmdArgs.push_back(arg);
    }
    return true;
  } else if (trace.eof()) {
      return false;
  } else {
    cerr << "ERROR: getline failed on trace file: " << file_name 
            << "at line " << line_number << "\n";
    exit(2);
  }
}

void Process::CmdPageLimit(const string &line, 
                         const string &cmd, 
                         vector<uint32_t> &cmdArgs) {
    
    max_pages = cmdArgs.at(0);
    std::cout << max_pages << " !!!!\n";
    
}

void Process::CmdDiff(const string &line,
                              const string &cmd,
                              vector<uint32_t> &cmdArgs) {
  Addr addr = cmdArgs.back();

  try {
    // Compare specified byte values
    int count = cmdArgs.size() - 1;
    for (int i = 0; i < count; ++i) {
      uint8_t b;
      memory.get_byte(&b, addr);
      if(b != cmdArgs.at(i)) {
        cout << "diff error at address " << std::hex << addr
                << ", expected " << static_cast<uint32_t>(cmdArgs.at(i))
                << ", actual is " << static_cast<uint32_t>(b) << "\n";
      }
      ++addr;
    }
  }
  catch(mem::PageFaultException e) {
    memory.get_PMCB(proc_pmcb);
    cout << "PageFaultException at address " << std::hex 
            <<  proc_pmcb.next_vaddress << ": " << e.what() << "\n";
    proc_pmcb.operation_state = mem::PMCB::NONE;  // clear fault
    memory.set_PMCB(proc_pmcb);
  }
}

void Process::CmdStore(const string &line,
                       const string &cmd,
                       vector<uint32_t> &cmdArgs) {
    
    // Store multiple bytes starting at specified address
    Addr addr = cmdArgs.back();
    cmdArgs.pop_back();

    // Build buffer
    uint8_t buffer[cmdArgs.size()];
    for (size_t i = 0; i < cmdArgs.size(); ++i) {
      buffer[i] = cmdArgs[i];
    }

    // Write to MMU
    try {
        memory.put_bytes(addr, cmdArgs.size(), buffer);
    } catch(mem::PageFaultException e) {
        
        memory.get_PMCB(proc_pmcb);
        cout << "PageFaultException at address " << std::hex 
                <<  proc_pmcb.next_vaddress << ": " << e.what() << "\n";
        proc_pmcb.operation_state = mem::PMCB::NONE;  // clear fault
        memory.set_PMCB(proc_pmcb);
        
        // allocate another to accommodate page fault
        
        ptm.MapProcessPages(proc_pmcb.next_vaddress, 1);
        current_num_pages++; // other places to increment?
        
        if(current_num_pages > max_pages){
            std::cout << "ERROR: number of pages exceeded maximum page number limit";
            std::exit(3);
        }
        
         // reload PMCB's previous state and resume
        
        proc_pmcb.operation_state = mem::PMCB::WRITE_OP;
        memory.set_PMCB(proc_pmcb);
        
      
    } catch(mem::WritePermissionFaultException e) {
        
        memory.get_PMCB(proc_pmcb);
        cout << "WritePermissionFaultException at address " << std::hex 
                <<  proc_pmcb.next_vaddress << ": " << e.what() << "\n";
        proc_pmcb.operation_state = mem::PMCB::NONE;  // clear fault
        memory.set_PMCB(proc_pmcb);
      
    }
    
}

void Process::CmdDupl(const string &line,
                      const string &cmd,
                      vector<uint32_t> &cmdArgs) {
    
    // Duplicate specified number of bytes to destination from source
    Addr dst = cmdArgs.at(2);
    Addr src = cmdArgs.at(1);
    uint32_t count = cmdArgs.at(0);
    uint8_t b;
    
    try {
        
      while (count-- > 0) {
        memory.get_byte(&b, src++);
        memory.put_byte(dst++, &b);
      }
      
    } catch(mem::PageFaultException e) {
        
        // reload PMCB in try/catch block
        
        memory.get_PMCB(proc_pmcb);
        cout << "PageFaultException at address " << std::hex 
                <<  proc_pmcb.next_vaddress << ": " << e.what() << "\n";
        proc_pmcb.operation_state = mem::PMCB::NONE;  // clear fault
        memory.set_PMCB(proc_pmcb);
        
        // allocate another to accommodate page fault
        
        ptm.MapProcessPages(proc_pmcb.next_vaddress, 1);
        current_num_pages++; // other places to increment?
        
        if(current_num_pages > max_pages){
            std::cout << "ERROR: number of pages exceeded maximum page number limit";
            std::exit(3);
        }
        
         // reload PMCB's previous state and resume
        
        proc_pmcb.operation_state = mem::PMCB::WRITE_OP;
        memory.set_PMCB(proc_pmcb);
        
    } catch(mem::WritePermissionFaultException e) {
        
        memory.get_PMCB(proc_pmcb);
        cout << "WritePermissionFaultException at address " << std::hex 
                <<  proc_pmcb.next_vaddress << ": " << e.what() << "\n";
        proc_pmcb.operation_state = mem::PMCB::NONE;  // clear fault
        memory.set_PMCB(proc_pmcb);
      
    }
}

void Process::CmdRepl(const string &line,
                      const string &cmd,
                      vector<uint32_t> &cmdArgs) {
    
    // Replicate specified value in destination range
    uint8_t value = cmdArgs.at(0);
    uint32_t count = cmdArgs.at(1);
    uint32_t addr = cmdArgs.at(2);
    try {
        
        for (int i = 0; i < count; ++i) {
          memory.put_byte(addr++, &value);
        }
      
    } catch(mem::PageFaultException e) {
        
        memory.get_PMCB(proc_pmcb);
        cout << "PageFaultException at address " << std::hex 
                <<  proc_pmcb.next_vaddress << ": " << e.what() << "\n";
        proc_pmcb.operation_state = mem::PMCB::NONE;  // clear fault
        memory.set_PMCB(proc_pmcb);
        
        // allocate another to accommodate page fault
        
        ptm.MapProcessPages(proc_pmcb.next_vaddress, 1);
        current_num_pages++; // other places to increment?
        
        if(current_num_pages > max_pages){
            std::cout << "ERROR: number of pages exceeded maximum page number limit";
            std::exit(3);
        }
        
         // reload PMCB's previous state and resume
        
        proc_pmcb.operation_state = mem::PMCB::WRITE_OP;
        memory.set_PMCB(proc_pmcb);
        
    } catch(mem::WritePermissionFaultException e) {
        
        memory.get_PMCB(proc_pmcb);
        cout << "WritePermissionFaultException at address " << std::hex 
                <<  proc_pmcb.next_vaddress << ": " << e.what() << "\n";
        proc_pmcb.operation_state = mem::PMCB::NONE;  // clear fault
        memory.set_PMCB(proc_pmcb);
        
    }
}

void Process::CmdPrint(const string &line,
                       const string &cmd,
                       vector<uint32_t> &cmdArgs) {
  uint32_t addr = cmdArgs.at(1);
  uint32_t count = cmdArgs.at(0);

  // Output the address
  cout << std::hex << addr;

  try {
    // Output the specified number of bytes starting at the address
    uint8_t b;
    for(int i = 0; i < count; ++i) {
      if((i % 16) == 0) {  // line break every 16 bytes
        cout << "\n";
      }
      memory.get_byte(&b, addr++);
      cout << " " << std::setfill('0') << std::setw(2)
              << static_cast<uint32_t> (b);
    }
  }
  catch(mem::PageFaultException e) {
    memory.get_PMCB(proc_pmcb);
    cout << "PageFaultException at address " << std::hex 
            <<  proc_pmcb.next_vaddress << ": " << e.what() << "\n";
    proc_pmcb.operation_state = mem::PMCB::NONE;  // clear fault
    memory.set_PMCB(proc_pmcb);
  }
  catch(mem::WritePermissionFaultException e) {
    memory.get_PMCB(proc_pmcb);
    cout << "WritePermissionFaultException at address " << std::hex 
            <<  proc_pmcb.next_vaddress << ": " << e.what() << "\n";
    proc_pmcb.operation_state = mem::PMCB::NONE;  // clear fault
    memory.set_PMCB(proc_pmcb);
  }

  cout << "\n";
}

void Process::CmdPermission(const string &line, 
                            const string &cmd, 
                            vector<uint32_t> &cmdArgs) {
  // Change the permissions of the specified pages
  ptm.SetPageWritePermission(cmdArgs.at(2), cmdArgs.at(0), cmdArgs.at(1));
}

