/* 
 * File:   ProcessScheduler.h
 * Author: jpbordiga
 *
 * Created on May 22, 2018, 4:00 PM
 */

// Manages processes in Round Robin fashion

#ifndef PROCESSSCHEDULER_H
#define PROCESSSCHEDULER_H

#include "Process.h"
#include <list>
#include <vector>

class ProcessScheduler {

public:
/**
   * Constructor - open trace file, initialize processing
   * 
   * @param file_name_ source of trace commands
   * @param memory MMU class object to use for memory
   * @param ptm page table manager
   */
  ProcessScheduler(std::list<Process> &processList, int quantum_);
  
  /**
   * Destructor - close trace file, clean up processing
   */
  virtual ~ProcessScheduler() {}; // empty destructor

  // Other constructors, assignment
  ProcessScheduler(const ProcessScheduler &other) = delete;
  ProcessScheduler(ProcessScheduler &&other) = delete;
  ProcessScheduler operator=(const ProcessScheduler &other) = delete;
  ProcessScheduler operator=(ProcessScheduler &&other) = delete;
  
  /**
   * Run_Sch - run next n non-comment lines
   * 
   * @param int n for number of non-comment lines to run
   * 
   * return bool
   */
  
  
private:
    
    int quantum;
    int currunt_process_number;
    int comment_lines;
    
    
};

#endif /* PROCESSSCHEDULER_H */

