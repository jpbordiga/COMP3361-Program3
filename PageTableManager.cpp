#include "PageTableManager.h"

#include <stdexcept>
#include <vector>

using mem::Addr;
using mem::PageTable;
using mem::PageTableEntry;
using mem::PMCB;
using std::vector;

PageTableManager::PageTableManager(mem::MMU &memory_, MemoryAllocator &allocator_) 
: memory(memory_), allocator(allocator_) {
  // Allocate page for kernel page table
  vector<Addr> allocated;
  if (!allocator.AllocatePageFrames(1, allocated) || allocated.size() != 1) {
    throw std::runtime_error("Allocation of kernel page table failed");
  }
  
  kernel_page_table = allocated[0];
  kernel_pmcb.page_table_base = kernel_page_table;
  
  // Initialize the kernel page table
  PageTable page_table; // local copy of page table to build, initialized to 0
  Addr num_pages = memory.get_frame_count(); // size of physical memory

  // Build page table entries (map to end of existing memory)
  for (Addr i = 0; i < num_pages; ++i) {
    page_table[i] = (i << mem::kPageSizeBits) 
            | mem::kPTE_PresentMask | mem::kPTE_WritableMask;
  }

  // Write page table to MMU memory
  memory.put_bytes(kernel_page_table, mem::kPageTableSizeBytes, &page_table);
  
  // Set page table and enter virtual mode
  memory.set_PMCB(kernel_pmcb);
  memory.enter_virtual_mode();
}

Addr PageTableManager::CreateProcessPageTable(void) {
  PMCB prev_pmcb = SwitchToKernelPageTable();
  vector<Addr> allocated;
  if (!allocator.AllocatePageFrames(1, allocated) || allocated.size() != 1) {
    throw std::runtime_error("Allocation of process page table failed");
  }
  
  // MemoryAllocator guarantees that pages are all 0, which is what we need
  // for an empty page table, so no further processing is necessary.
  memory.set_PMCB(prev_pmcb);  // restore PMCB
  return allocated[0];
}

void PageTableManager::MapProcessPages(Addr vaddr, size_t count) {
  // Check for valid virtual address
  if ((vaddr & mem::kPageOffsetMask) != 0) {
    throw std::runtime_error("vaddr not a multiple of page size");
  }
  
  // Need to use kernel page table
  PMCB proc_pmcb = SwitchToKernelPageTable();
  
  // Allocate pages
  vector<Addr> allocated;
  if (!allocator.AllocatePageFrames(count, allocated) 
          || allocated.size() != count) {
    throw std::runtime_error("Allocation of process pages failed");
  }
  
  // Map the allocated pages
  vector<Addr> already_mapped;  // any pages that are already mapped
  Addr next_vaddr = vaddr;
  while (!allocated.empty()) {
    Addr pt_index = next_vaddr >> mem::kPageSizeBits;
    if (pt_index >= mem::kPageTableEntries) {
      throw std::runtime_error("Virtual address too large");
    }
    
    // Read existing page table entry
    PageTableEntry pt_entry;
    Addr pte_addr = proc_pmcb.page_table_base + pt_index*sizeof(PageTableEntry);
    memory.get_bytes(&pt_entry, pte_addr, sizeof(PageTableEntry));
    
    // If page not mapped, create and write page table entry
    if ((pt_entry & mem::kPTE_PresentMask) == 0) {
      pt_entry = 
              allocated.back() | mem::kPTE_PresentMask | mem::kPTE_WritableMask;
      allocated.pop_back();
      memory.put_bytes(pte_addr, sizeof(PageTableEntry), &pt_entry);
    } else {
      // Duplicate - free the page frame after we're done
      already_mapped.push_back(allocated.back());
      allocated.pop_back();
    }
    
    next_vaddr += mem::kPageSize;
  }
  
  // Release any duplicate pages
  if (!already_mapped.empty()) {
    allocator.FreePageFrames(already_mapped.size(), already_mapped);
  }
  
  memory.set_PMCB(proc_pmcb);  // restore process PMCB
}

void PageTableManager::SetPageWritePermission(mem::Addr vaddr, 
                                              size_t count, 
                                              uint32_t writable) {
  // Check for valid virtual address
  if ((vaddr & mem::kPageOffsetMask) != 0) {
    throw std::runtime_error("vaddr not a multiple of page size");
  }
  
  // Need to use kernel page table
  PMCB proc_pmcb = SwitchToKernelPageTable();
  
  // Modify each page which is present
  Addr pt_index = vaddr >> mem::kPageSizeBits;
  for (Addr i = 0; i < count; ++i) {    
    if ((pt_index+i) >= mem::kPageTableEntries) {
      throw std::runtime_error("Virtual address too large");
    }

    // Read existing page table entry
    PageTableEntry pt_entry;
    Addr pte_addr = proc_pmcb.page_table_base + (pt_index+i)*sizeof(PageTableEntry);
    memory.get_bytes(&pt_entry, pte_addr, sizeof(PageTableEntry));
    
    // If entry is present, modify the writable bit
    if ((pt_entry & mem::kPTE_PresentMask) != 0) {
      PageTableEntry new_pt_entry = (pt_entry & ~mem::kPTE_WritableMask)
              | (writable ? mem::kPTE_WritableMask : 0);
      if (pt_entry != new_pt_entry) { // if changed
        memory.put_bytes(pte_addr, sizeof(PageTableEntry), &new_pt_entry);
      }
    }
  }

  memory.set_PMCB(proc_pmcb);  // restore process PMCB
}

PMCB PageTableManager::SwitchToKernelPageTable(void) {
  PMCB prev_PMCB;
  memory.get_PMCB(prev_PMCB);    // get the current PMCB
  memory.set_PMCB(kernel_pmcb);  // switch to kernel PMCB
  return prev_PMCB;
}

