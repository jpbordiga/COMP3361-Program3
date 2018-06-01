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
    bool pageLimitExceeded = false;

    // Select the command to execute
    for (int i = 0; i < n; ++i){
        
        if((ParseCommand(line, cmd, cmdArgs) == true) && (pageLimitExceeded == false)){
        
            if (cmd == "pagelimit") {
                CmdPageLimit(line, cmd, cmdArgs);         // set page limit
            } else if (cmd == "diff") {
                CmdDiff(line, cmd, cmdArgs);        // get and compare multiple bytes
            } else if (cmd == "store") {
                pageLimitExceeded = CmdStore(line, cmd, cmdArgs);       // put bytes
            } else if (cmd == "replicate") {
                pageLimitExceeded = CmdRepl(line, cmd, cmdArgs);        // fill bytes with value
            } else if (cmd == "duplicate") {
                pageLimitExceeded = CmdDupl(line, cmd, cmdArgs);        // copy bytes to dest from source
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
           
            // incorrect?
            ptm.SwitchToKernelPageTable();
            ptm.FreePageFrames(current_num_pages); //
            
            memory.set_PMCB(proc_pmcb);
            ptm.FreePageFrames(current_num_pages);
            //
            
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
  } catch(mem::PageFaultException e) {
      
    memory.get_PMCB(proc_pmcb);
    cout << "PageFaultException at address " << std::hex 
            <<  proc_pmcb.next_vaddress << ": " << e.what() << "\n";
    proc_pmcb.operation_state = mem::PMCB::NONE;  // clear fault
    memory.set_PMCB(proc_pmcb);
    
  }
}

bool Process::CmdStore(const string &line,
                       const string &cmd,
                       vector<uint32_t> &cmdArgs) {
    
    bool pageLimitExceeded = false;
    
    // Store multiple bytes starting at specified address
    Addr addr = cmdArgs.back();
    cmdArgs.pop_back();

    // Build buffer
    uint8_t buffer[cmdArgs.size()];
    for (size_t i = 0; i < cmdArgs.size(); ++i) {
        buffer[i] = cmdArgs[i];
    }

    pageLimitExceeded = MMUWrite(addr, cmdArgs.size(), buffer);
    
    return pageLimitExceeded;
    
}

bool Process::CmdDupl(const string &line,
                      const string &cmd,
                      vector<uint32_t> &cmdArgs) {
    
    // Duplicate specified number of bytes from source to destination
    Addr dst = cmdArgs.at(2);
    Addr src = cmdArgs.at(1);
    uint32_t count = cmdArgs.at(0);
    uint8_t buffer[count];
    uint8_t b;
    bool pageLimitExceeded = false;
    
    // Build buffer
    for (size_t i = count; count > i; --i) { //
        memory.get_byte(&b, src++); 
        memory.put_byte(buffer[i], &b);
    }
    
    pageLimitExceeded = MMUWrite(src, count, buffer);
    
    return pageLimitExceeded;
    
}

bool Process::CmdRepl(const string &line,
                      const string &cmd,
                      vector<uint32_t> &cmdArgs) {
    
    // Replicate specified value in destination range
    uint8_t value = cmdArgs.at(0);
    uint32_t count = cmdArgs.at(1);
    uint32_t addr = cmdArgs.at(2);
    bool bytesRemaining = true;
    bool putBytesCalled = false;
    bool pageLimitExceeded = false;
    
    // Build buffer
    uint8_t buffer[count];
    for (size_t i = 0; i < count; ++i) {
        buffer[i] = value; // Replicate
    }
    
    pageLimitExceeded = MMUWrite(addr, count, buffer);
    
    return pageLimitExceeded;
    
}

bool Process::MMUWrite(Addr addr_, uint32_t count_, uint8_t buffer[]) {

    uint32_t addr = addr_;
    uint32_t byte_count = count_;
    bool first_write_attempt = true;
    bool pageLimitExceeded = false;
    
    std::cout << "0 ****\n";
    
    while(byte_count != 0){
        std::cout << "1 ****\n";
        try {
            std::cout << "2 ****\n";
            if(first_write_attempt){
                std::cout << "3 ****\n";
                first_write_attempt = false;
                std::cout << "4 ****\n";
                memory.put_bytes(addr, byte_count, buffer); // entire buffer
                std::cout << "5 ****\n";
                byte_count = 0;
                std::cout << "6 ****\n";
            } else {
                std::cout << "7 ****\n";
                memory.set_PMCB(proc_pmcb); //
                std::cout << "8 ****\n";
            }
                
        } catch(mem::PageFaultException e) {
            
            std::cout << "9 ****\n";
            memory.get_PMCB(proc_pmcb);
            std::cout << "10 ****\n";
            ptm.MapProcessPages(proc_pmcb.next_vaddress & mem::kPageNumberMask, 1);
            std::cout << "11 ****\n";
            byte_count = proc_pmcb.remaining_count;
            std::cout << "12 ****\n";
            current_num_pages++; // other places to increment?
                
            if(current_num_pages > max_pages){ //
                std::cout << " ERROR: page count exceeded maximum number of pages";
                pageLimitExceeded = true;
                return pageLimitExceeded;
            }
            std::cout << "13 ****\n";
        } catch(mem::WritePermissionFaultException e) {

            memory.get_PMCB(proc_pmcb);
            cout << "WritePermissionFaultException at address " << std::hex 
                    <<  proc_pmcb.next_vaddress << ": " << e.what() << "\n";
            proc_pmcb.operation_state = mem::PMCB::NONE;  // clear fault
            memory.set_PMCB(proc_pmcb);

        }
        std::cout << "14 ****\n";
        memory.get_PMCB(proc_pmcb);
        std::cout << "15 ****\n";
    }
 std::cout << "16 ****\n";

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

