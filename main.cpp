/* COMP3361 Operating Systems Spring 2018
 * Program 2 Sample Solution
 * 
 * File:   main.cpp
 * Author: Mike Goss <mikegoss@cs.du.edu>
 */

#include "MemoryAllocator.h"
#include "PageTableManager.h"
#include "Process.h"

#include <MMU.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

using mem::MMU;

using std::cerr;
using std::cout;
using std::string;
using std::vector;

int main(int argc, char* argv[]) {
  // Use command line argument as file name
  if (argc != 2) {
    std::cerr << "usage: program2 input_file\n";
    exit(1);
  }
  
  // Create allocator and page table manager (these will be shared among all 
  // processes in programming assignment 2)
  mem::MMU memory(64);  // fixed memory size of 64 pages
  MemoryAllocator allocator(memory);
  PageTableManager ptm(memory, allocator);
  
  // Create the process
  Process process(argv[1], memory, ptm);
  
  // Run the commands
  process.Run();
}

