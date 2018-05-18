/* MemoryAllocator - allocate pages in memory.
 * 
 * Note that all methods expect physical memory mode or the kernel page table 
 * to be enabled.
 * 
 * File:   MemoryAllocator.h
 * Author: Mike Goss <mikegoss@cs.du.edu>
 */

#ifndef MEMORYALLOCATOR_H
#define MEMORYALLOCATOR_H

#include <MMU.h>

#include <cstdint>
#include <string>
#include <vector>

class MemoryAllocator {
public:
  /**
   * Constructor
   * 
   * Allocates the specified number of page frames, and builds free list of
   * all page frames.
   * 
   * @param page_frame_count
   */
  MemoryAllocator(mem::MMU &memory_);
  
  virtual ~MemoryAllocator() {}  // empty destructor
  
  // Disallow copy/move
  MemoryAllocator(const MemoryAllocator &other) = delete;
  MemoryAllocator(MemoryAllocator &&other) = delete;
  MemoryAllocator &operator=(const MemoryAllocator &other) = delete;
  MemoryAllocator &operator=(MemoryAllocator &&other) = delete;
  
  /**
   * Allocate - allocate page frames from the free list.
   * 
   * @param count number of page frames to allocate
   * @param page_frames page frame addresses allocated are pushed on back
   * @return true if success, false if insufficient page frames (no frames allocated)
   */
  bool AllocatePageFrames(uint32_t count, std::vector<mem::Addr> &page_frames);
  
  /**
   * FreePageFrames - return page frames to free list
   * 
   * @param count number of page frames to free
   * @param page_frames contains page frame addresses to deallocate; addresses
   *   are popped from back of vector
   * @return true if success, false if insufficient page frames in vector
   */
  bool FreePageFrames(uint32_t count, std::vector<mem::Addr> &page_frames);
  
  // Functions to return list info
  uint32_t get_page_frames_free(void) const;
  uint32_t get_page_frames_total(void) const;
  
  /**
   * FreeListToString - get string representation of free list
   * 
   * @return hex numbers of all free pages
   */
  std::string FreeListToString(void) const;

private:
  // Vector to hold memory to be allocated
  mem::MMU &memory;
  
  // Address of number of first free page frame
  static const size_t kFreeListHead = 0;
  
  // Total number of page frames
  static const size_t kPageFramesTotal = kFreeListHead + sizeof(mem::Addr);
  
  // Current number of free page frames
  static const size_t kPageFramesFree = kPageFramesTotal + sizeof(mem::Addr);
  
  // End of list marker
  static const mem::Addr kEndList = 0xFFFFFFFF;
  
  // Private getters and setters
  mem::Addr get_free_list_head(void) const;
  void set_free_list_head(mem::Addr free_list_head);
  void set_page_frames_total(mem::Addr page_frames_total);
  void set_page_frames_free(mem::Addr page_frames_free);
};

#endif /* MEMORYALLOCATOR_H */

