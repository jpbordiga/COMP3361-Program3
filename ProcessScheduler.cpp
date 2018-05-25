#include "ProcessScheduler.h"
#include <list>
#include <iterator>

ProcessScheduler::ProcessScheduler(std::list<Process> &processList, int quantum_) : 
    quantum(quantum_), currunt_process_number(0), comment_lines(0){
    
    while(processList.size() > 0){
    
        auto iter = processList.begin();
        
        while(iter != processList.end()){
            
            // if not terminated, go to next process
            // use return value of erase

            // if true, ++ iterator
            // if false, erase, set iterator to what erase returns

            if(iter->Run(quantum)){ //
                iter++;
            } else {
                iter = processList.erase(iter);
            }      
        
        }
        
    }
    
}