//
//  kern_mach.hpp
//  AppleALC
//
//  Certain parts of code are the subject of
//   copyright © 2011, 2012, 2013, 2014 fG!, reverser@put.as - http://reverse.put.as
//  Copyright © 2016 vit9696. All rights reserved.
//

#ifndef kern_mach_hpp
#define kern_mach_hpp

#include "kern_util.hpp"

#include <sys/time.h>
#include <sys/types.h>
#include <sys/vnode.h>
#include <mach-o/loader.h>
#include <mach/vm_param.h>

class MachInfo {
	mach_vm_address_t running_text_addr {0}; // the address of running __TEXT segment
	mach_vm_address_t disk_text_addr {0};    // the same address at from a file
	mach_vm_address_t kaslr_slide {0};       // the kernel aslr slide, computed as the difference between above's addresses
	uint8_t *file_buf {nullptr};             // read file data if decompression was used
	uint8_t *linkedit_buf {nullptr};         // pointer to __LINKEDIT buffer containing symbols to solve
	uint64_t linkedit_fileoff {0};           // __LINKEDIT file offset so we can read
	uint64_t linkedit_size {0};
	uint32_t symboltable_fileoff {0};        // file offset to symbol table - used to position inside the __LINKEDIT buffer
	uint32_t symboltable_nr_symbols {0};
	uint32_t stringtable_fileoff {0};        // file offset to string table
	mach_header_64 *running_mh {nullptr};    // pointer to mach-o header of running kernel item
	off_t fat_offset {0};                    // additional fat offset
	size_t memory_size {HeaderSize};         // memory size
	bool kaslr_slide_set {false};            // kaslr can be null, used for disambiguation
	
	/**
	 *  16 byte IDT descriptor, used for 32 and 64 bits kernels (64 bit capable cpus!)
	 */
	struct descriptor_idt {
		uint16_t offset_low;
		uint16_t seg_selector;
		uint8_t reserved;
		uint8_t flag;
		uint16_t offset_middle;
		uint32_t offset_high;
		uint32_t reserved2;
	};
	
	/**
	 *  retrieve the address of the IDT
	 *
	 *  @return always returns the IDT address
	 */
	mach_vm_address_t getIDTAddress();
	
	/**
	 *  calculate the address of the kernel int80 handler
	 *
	 *  @return always returns the int80 handler address
	 */
	mach_vm_address_t calculateInt80Address();
	
	/**
	 *  retrieve LC_UUID command value from a mach header
	 *
	 *  @param header mach header pointer
	 *
	 *  @return UUID or nullptr
	 */
	uint64_t *getUUID(void *header);
	
	/**
	 *  enable/disable the Write Protection bit in CR0 register
	 *
	 *  @param enable the desired value
	 *
	 *  @return KERN_SUCCESS if succeeded
	 */
	kern_return_t setWPBit(bool enable);
	
	/**
	 *  retrieve the first pages of a binary at disk into a buffer
	 *  version that uses KPI VFS functions and a ripped uio_createwithbuffer() from XNU
	 *
	 *  @param buffer allocated buffer sized no less than HeaderSize
	 *  @param vnode  file node
	 *  @param ctxt   filesystem context
	 *  @param off    fat offset or 0
	 *
	 *  @return KERN_SUCCESS if the read data contains 64-bit mach header
	 */
	kern_return_t readMachHeader(uint8_t *buffer, vnode_t vnode, vfs_context_t ctxt, off_t off=0);

	/**
	 *  retrieve the whole linkedit segment into target buffer from kernel binary at disk
	 *
	 *  @param vnode file node
	 *  @param ctxt  filesystem context
	 *
	 *  @return KERN_SUCCESS on success
	 */
	kern_return_t readLinkedit(vnode_t vnode, vfs_context_t ctxt);
	
	/**
	 *  retrieve necessary mach-o header information from the mach header
	 *
	 *  @param header read header sized no less than HeaderSize
	 */
	void processMachHeader(void *header);
	
	MachInfo(bool asKernel=false) : isKernel(asKernel) {
		DBGLOG("mach @ MachInfo asKernel %d object constructed", asKernel);
	}
	MachInfo(const MachInfo &) = delete;
	MachInfo &operator =(const MachInfo &) = delete;
	
public:

	/**
	 *  Each header is assumed to fit two pages
	 */
	static constexpr size_t HeaderSize {PAGE_SIZE_64*2};
	
	/**
	 *  Representation mode (kernel/kext)
	 */
	const bool isKernel;

	/**
	 *  MachInfo object generator
	 *
	 *  @param asKernel this MachInfo represents a kernel
	 *
	 *  @return MachInfo object or nullptr
	 */
	static MachInfo *create(bool asKernel=false) { return new MachInfo(asKernel); }
	static void deleter(MachInfo *i) { delete i; }

	/**
	 *  Resolve mach data in the kernel
	 *
	 *  @param enable filesystem paths for lookup
	 *  @param num    the number of paths passed
	 *
	 *  @return KERN_SUCCESS if loaded
	 */
	kern_return_t init(const char * const paths[], size_t num = 1);
	
	/**
	 *  Release the allocated memory, must be called regardless of the init error
	 */
	void deinit();

	/**
	 *  retrieve the mach header and __TEXT addresses
	 *
	 *  @param slide load slide if calculating for kexts
	 *  @param memory size
	 *
	 *  @return KERN_SUCCESS on success
	 */
	kern_return_t getRunningAddresses(mach_vm_address_t slide=0, size_t size=0);

	/**
	 *  retrieve running mach positions
	 *
	 *  @param header pointer to header
	 *  @param size   file size
	 */
	void getRunningPosition(uint8_t * &header, size_t &size);

	/**
	 *  solve a mach symbol (running addresses must be calculated)
	 *
	 *  @param symbol symbol to solve
	 *
	 *  @return running symbol address or 0
	 */
	mach_vm_address_t solveSymbol(const char *symbol);

	/**
	 *  Read file data from a vnode
	 *
	 *  @param buffer output buffer
	 *  @param off    file offset
	 *  @param sz     bytes to read
	 *  @param vnode  file node
	 *  @param ctxt   filesystem context
	 *
	 *  @return 0 on success
	 */
	int readFileData(void *buffer, off_t off, size_t sz, vnode_t vnode, vfs_context_t ctxt);
	
	/**
	 *  Read file size from a vnode
	 *
	 *  @param vnode file node
	 *  @param ctxt  filesystem context
	 *
	 *  @return file size or 0
	 */
	size_t readFileSize(vnode_t vnode, vfs_context_t ctxt);

	/**
	 *  find the kernel base address (mach-o header)
	 *  by searching backwards using the int80 handler as starting point
	 *
	 *  @return kernel base address or 0
	 */
	mach_vm_address_t findKernelBase();

	/**
	 *  enable/disable kernel memory write protection
	 *
	 *  @param enable the desired value
	 *
	 *  @return KERN_SUCCESS if succeeded
	 */
	kern_return_t setKernelWriting(bool enable);
	
	/**
	 *  Compare the loaded kernel with the passed kernel header
	 *
	 *  @param kernel_header 64-bit mach header of at least HeaderSize size
	 *
	 *  @return true if the kernel uuids match
	 */
	bool isCurrentKernel(void *kernelHeader);
};

#endif /* kern_mach_hpp */