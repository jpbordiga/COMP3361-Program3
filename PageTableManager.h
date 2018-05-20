/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   PageTableManager.h
 * Author: jpbordiga
 *
 * Created on May 19, 2018, 4:59 PM
 */

#ifndef PAGETABLEMANAGER_H
#define PAGETABLEMANAGER_H

#include "MemoryAllocator.h"

class PageTableManager {
public:
  /**
   * Constructor - build kernel page table and enter virtual mode
   * 
   * Must be in physical memory mode on entry. Creates kernel page table in MMU, 
   * enters virtual mode.
   * 
   * @param memory_ MMU class object to use for memory
   * @param allocator_ page frame allocator object
   * @throws std::runtime_error if unable to allocate memory for page table
   */
  PageTableManager(mem::MMU &memory_, MemoryAllocator &allocator_);
  virtual ~PageTableManager() {}  // empty destructor
  
  // Disallow copy/move
  PageTableManager(const PageTableManager &other) = delete;
  PageTableManager(PageTableManager &&other) = delete;
  PageTableManager &operator=(const PageTableManager &other) = delete;
  PageTableManager &operator=(PageTableManager &&other) = delete;
  
  /**
   * CreateProcessPageTable - create empty page table for process
   * 
   * All entries in the page table will be 0 (not present).
   * 
   * @return kernel address of new page table
   * @throws std::runtime_error if unable to allocate memory for page table
   */
  mem::Addr CreateProcessPageTable(void);
  
  /**
   * MapProcessPages - map pages into the memory of currently running process
   * 
   * The requested pages are allocated and mapped into the page table of
   * the current process (process whose PMCB is currently loaded). Any pages
   * already mapped are ignored.
   * 
   * @param vaddr starting virtual address
   * @param count number of pages to map
   * @throws std::runtime_error if unable to allocate memory for pages
   */
  void MapProcessPages(mem::Addr vaddr, size_t count);
  
  /**
   * SetPageWritePermission - change writable bit for page(s)
   * 
   * @param vaddr starting virtual address
   * @param count number of pages to change
   * @param writable non-zero to set writable bit, 0 to clear it
   */
  void SetPageWritePermission(mem::Addr vaddr, size_t count, uint32_t writable);
  
  /**
   * SwitchToKernelPageTable - set MMU to use kernel page table
   * 
   * @return PMCB in use at time of call
   */
  mem::PMCB SwitchToKernelPageTable(void);
  
 private:
   // Save references to memory and allocator
   mem::MMU &memory;
   MemoryAllocator &allocator;
   
   mem::Addr kernel_page_table;  // address of kernel page table
   mem::PMCB kernel_pmcb;   // PMCB for kernel mode

};

#endif /* PAGETABLEMANAGER_H */

