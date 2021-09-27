/**
 * @file ObjectAllocator.cpp
 * @author Giam Jian Kai (jiankai.g\@digipen.edu)
 * @par email: jiankai.g\@digipen.edu
 * @par Digipend Login: jiankai.g
 * @par Course: CS280
 * @par Assignment #1
 * @brief 
 *    This file contains the implementation of the following functions for
 *    the ObjectAllocator class:
 *    
 *    Public functions include:
 *      Constructor
 *      Destructor
 *      Allocate
 *      Free
 *      DumpMemoryInUse
 *      ValidatePages
 *      FreeEmptyPages
 *      SetDebugState
 *      GetFreeList
 *      GetPageList
 *      GetConfig
 *      GetStats
 * 
 *    Private methods include:
 *      checkObjectForPattern
 * 		ComputeAlignment
 *      CreateAPage
 *      FindPageByObject
 *      isPadCorrupted
 * 		isPageEmpty
 *    Hours spent on this assignment: 60
 *    Specific portions that gave you the most trouble: optimisation of free.
 * @version 0.1 20 tests run/16 tests passed
 * @version 0.2 20 tests run/19 tests passed
 * @version 0.3 20 tests run/20 tests passed
 * @date 2021-09-26
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#include "ObjectAllocator.h"

/**
 * @fn ObjectAllocator::ObjectAllocator(size_t ObjectSize, const OAConfig& config)
		: config_(config), stats_(), OAException_()
 * 
 * @brief 
 * 		Construct a new Object Allocator:: Object Allocator object
 *		Creates the ObjectManager per the specified values
 *		Throws an exception if the construction fails. (Memory allocation problem)
 * 
 * @param ObjectSize 
 * 		Size of Object
 * 
 * @param config 
 * 		an custom struct OAConfig that contains configuration of Memory Manager
 */
ObjectAllocator::ObjectAllocator(size_t ObjectSize, const OAConfig& config)
: config_(config), stats_(), OAException_()
{
	AllocNum = 0;
	FreeList_ = NULL;
	PageList_ = NULL;

	stats_.ObjectSize_ = ObjectSize;
	sizeof_LeftmostBlock = sizeof(void*) + config_.PadBytes_ + config_.HBlockInfo_.size_;
	sizeof_InnerBlock = stats_.ObjectSize_ + 2*config_.PadBytes_ + config_.HBlockInfo_.size_;
	ComputeAlignment();
	
	stats_.PageSize_ = sizeof(void*) + static_cast<size_t>(config.ObjectsPerPage_) * ObjectSize 
		+ config_.PadBytes_ * 2 * config_.ObjectsPerPage_
		+ config_.HBlockInfo_.size_ * (config_.ObjectsPerPage_)
		+ config_.LeftAlignSize_ + (config_.ObjectsPerPage_ - 1) * config_.InterAlignSize_;
	OAException_ = new OAException(OAException::E_BAD_BOUNDARY, "A message returned by the what method.");
	
	sizeof_LeftmostBlock = sizeof(void*) + config_.PadBytes_ + config_.HBlockInfo_.size_ + config_.LeftAlignSize_;
	sizeof_InnerBlock = stats_.ObjectSize_ + 2*config_.PadBytes_ + config_.HBlockInfo_.size_ + config_.InterAlignSize_;

	if(!config_.UseCPPMemManager_){
		// create if not using standard new/delete
		lowerBoundary = CreateAPage();
		upperBoundary = lowerBoundary + stats_.PageSize_;
	}

}

// Destroys the ObjectManager (never throws)
/**
 * @fn ObjectAllocator::~ObjectAllocator()
 * 
 * @brief Destroy the Object Allocator:: Object Allocator object
 */
ObjectAllocator::~ObjectAllocator(){
	if(config_.UseCPPMemManager_)
		return;
	GenericObject* ptrToPage = PageList_;
	while(ptrToPage){
		PageList_ = ptrToPage;
		ptrToPage = PageList_->Next;
		delete [] PageList_;
	}
	
}	

/**
 * @fn void* ObjectAllocator::Allocate(const char *label) 
 * @brief 
 * 		To allocate fixed-sized memory blocks for a client.
 * 		Take an object from the free list and give it to the client (simulates new)
 *		Throws an exception if the object can't be allocated. (Memory allocation problem)
 * 
 * @param label 
 * 		for hbExternal blocks stored in struct MemBlockInfo
 * @return void* 
 * 		returns reference to allocated page.
 */
void* ObjectAllocator::Allocate(const char *label) {

	if(config_.UseCPPMemManager_){
		stats_.Allocations_ += 1;
		size_t size = stats_.ObjectSize_;

		if(stats_.Allocations_ > stats_.MostObjects_ )
			stats_.MostObjects_ = stats_.Allocations_;

		return new char[size];
	}

	
	if (stats_.FreeObjects_ == 0 && stats_.PagesInUse_ < config_.MaxPages_){
		//check page limit before adding new page
		char* newPageStart;
		newPageStart = CreateAPage(); // create another page and link to preivous page
		if(newPageStart < lowerBoundary)
			lowerBoundary = newPageStart;

		newPageStart += stats_.PageSize_;
		if(newPageStart > upperBoundary)
			upperBoundary = newPageStart;

	}
	if(stats_.FreeObjects_ > 0){
		
		// Remove first object from free list for client
		NewObject = reinterpret_cast<char*>(FreeList_);
		FreeList_ = FreeList_->Next;

		// Process assignment to header block
		char* ptrToUseCount = NULL;
		char* ptrToAllocNum = NULL;
		AllocNum +=1;
		char* ptrToHeaderBlock = NewObject-config_.PadBytes_ - config_.HBlockInfo_.size_;
		// Check header block type then assigning pointers
		if(config_.HBlockInfo_.type_ == OAConfig::hbExtended){
			// Assign Extended header
			ptrToUseCount = ptrToHeaderBlock + config_.HBlockInfo_.additional_;
			ptrToAllocNum = ptrToHeaderBlock + config_.HBlockInfo_.additional_ + 2;
			*ptrToUseCount = static_cast<char>(*ptrToUseCount + 1); // increment use count

			memset(ptrToAllocNum, static_cast<int>(AllocNum), 1); // set memory signature
			memset(ptrToAllocNum+4, 0x1, 1); // Flag assignment
		}
		else if(config_.HBlockInfo_.type_ == OAConfig::hbExternal){
			// allocate External header
			MemBlockInfo* ptrHbExternal = NULL;
			try{
				ptrHbExternal = new MemBlockInfo();  // should free this??
			}
			catch(std::bad_alloc &){
				throw OAException(OAException::E_NO_MEMORY, 
									std::string("No Memory, allocation failed for external header"));
			}
			ptrHbExternal->in_use = true;
			ptrHbExternal->label = const_cast<char *>(label);
			ptrHbExternal->alloc_num = static_cast<unsigned int>(AllocNum);
			*(reinterpret_cast<MemBlockInfo **>(ptrToHeaderBlock)) = ptrHbExternal;
		}
		else if(config_.HBlockInfo_.type_ == OAConfig::hbBasic){
			// Assign Basic header
			ptrToAllocNum = ptrToHeaderBlock;
			memset(ptrToAllocNum, static_cast<int>(AllocNum), 1); // set memory signature
			memset(ptrToAllocNum+4, 0x1, 1); // Flag assignment
		}
		
		//process byte pattern for Object space as "allocated"
		memset(NewObject, ALLOCATED_PATTERN, stats_.ObjectSize_);
		//update acounting info
		stats_.ObjectsInUse_ += 1;
		stats_.FreeObjects_ -= 1;
		stats_.Allocations_ += 1;
		if(stats_.Allocations_ > stats_.MostObjects_ )
			stats_.MostObjects_ = stats_.Allocations_;

	}
	else{
		throw OAException(OAException::E_NO_MEMORY, "allocate_new_page: No system memory available.");
	}
	return reinterpret_cast<void*>(NewObject);
}

// Returns an object to the free list for the client (simulates delete)
// Throws an exception if the the object can't be freed. (Invalid object)
/**
 * @fn void ObjectAllocator::Free(void *Object)
 * @brief 
 * 		deallocates Object stored in PageList for client, by freeing the block it is in.
 * 
 * @param Object 
 * 		Pointer to object to be freed and addded to FreeList_
 * 
 */
void ObjectAllocator::Free(void *Object){
	
	if(config_.UseCPPMemManager_){
		stats_.Deallocations_ += 1;
		delete [] reinterpret_cast<char*>(Object);
		return;
	}

	//check for "double free"
	char* ptrToObject = reinterpret_cast<char*>(Object);
	bool bNoDoubleFree = !checkObjectForPattern(ptrToObject, FREED_PATTERN);

	//check if page can be found
	GenericObject* page = PageList_;
	int ret = FindPageByObject(page, Object);
	if(ret == -1)
		throw OAException(OAException::E_BAD_BOUNDARY, "bad boundary. page not found ");
	
	//check if object is aligned and base case
	size_t ObjectPosition = (size_t)Object - (size_t)page;
	bool isAligned = ((ObjectPosition - sizeof_LeftmostBlock) % sizeof_InnerBlock)  == 0;
	bool baseCase = ObjectPosition % stats_.PageSize_ == 0; //special case
	if(!isAligned || baseCase){
		throw OAException(OAException::E_BAD_BOUNDARY, "bad boundary. mis-alignment ");
	}

	//check for out-of-bounds
	if(Object < lowerBoundary || Object > upperBoundary)
	{
		throw OAException(OAException::E_BAD_BOUNDARY, "bad boundary. ");
	}


	//check left and right pad bytes for pad pattern
	if(config_.PadBytes_ != 0){
		PaddState ps = isPadCorrupted(Object);
		if(ps == CORRUPT_LEFT)
			throw OAException(OAException::E_CORRUPTED_BLOCK,"corruption on left");
		if(ps == CORRUPT_RIGHT)
			throw OAException(OAException::E_CORRUPTED_BLOCK,"corruption on right");
		
	}
	


	//Free Object if no issues
	if(bNoDoubleFree){
		GenericObject* temp = FreeList_;
		FreeList_ = reinterpret_cast<GenericObject* >(Object);
		FreeList_->Next = temp;
		temp = NULL;

		char* ptrToHeaderBlock = reinterpret_cast<char* >(Object)-config_.PadBytes_ - config_.HBlockInfo_.size_;
		char* ptrToAllocNum = NULL;
		size_t BytesInBasicBlock;
		if(config_.HBlockInfo_.type_ == OAConfig::hbExtended){
			ptrToAllocNum = ptrToHeaderBlock + config_.HBlockInfo_.additional_ + 2;
			BytesInBasicBlock = config_.HBlockInfo_.size_-config_.HBlockInfo_.additional_-2;
		}
		else{
			ptrToAllocNum = ptrToHeaderBlock;
			BytesInBasicBlock = config_.HBlockInfo_.size_;
		}
		memset(ptrToAllocNum, ZERO_PATTERN, BytesInBasicBlock);
		char* ptrToFreedPatternArea = reinterpret_cast<char*>(FreeList_) + sizeof(void*);
		memset(ptrToFreedPatternArea, FREED_PATTERN, stats_.ObjectSize_-sizeof(void*));
		

		//update acounting info
		stats_.ObjectsInUse_ -= 1;
		stats_.FreeObjects_ += 1;
		stats_.Deallocations_ += 1;
	}
	else if(!bNoDoubleFree){
		throw OAException(OAException::E_MULTIPLE_FREE, "double free. ");
	}
	
}


/**
 * @fn unsigned ObjectAllocator::DumpMemoryInUse(DUMPCALLBACK fn) const
 * @brief 
 * 		Calls the callback fn for each block still in use
 * 
 * 		This function will display memory region that were currently allocated.
 * 
 * @param fn 
 * 		Pointer to a function that print out the allocated memory
 * 
 * @return unsigned 
 * 		Return the number of objects in use
 */
unsigned ObjectAllocator::DumpMemoryInUse(DUMPCALLBACK fn) const{
	unsigned numOfLeaks = 0;
	GenericObject* page = PageList_;//loop through each page in use
	while(page){
		//loop through each block in page
		// char* ptrToBlock = reinterpret_cast<char*>(page) + sizeof(size_t) + config_.PadBytes_ + config_.HBlockInfo_.size_; //firstblock
		char* ptrToBlock = reinterpret_cast<char*>(page) + sizeof(size_t) + OAConfig::BASIC_HEADER_SIZE + config_.PadBytes_ + config_.LeftAlignSize_; //firstblock
		char* ptrToHeaderBlock;
		bool in_use=false;
		for(int i=0;i<DEFAULT_OBJECTS_PER_PAGE;i++){
			//check if block is in use
			char* ptrToFlag; // find flag
			in_use = false;
			ptrToHeaderBlock = ptrToBlock - config_.PadBytes_ - config_.HBlockInfo_.size_;
			if(config_.HBlockInfo_.type_ == OAConfig::hbExtended){
				ptrToFlag = ptrToHeaderBlock + 6 + config_.HBlockInfo_.additional_;
				if(*ptrToFlag)
					in_use = true;
			}
			else if(config_.HBlockInfo_.type_ == OAConfig::hbExternal){
				in_use = reinterpret_cast<MemBlockInfo*>(ptrToHeaderBlock)->in_use;
			}
			else if(config_.HBlockInfo_.type_ == OAConfig::hbBasic){
				ptrToFlag = ptrToHeaderBlock + 4;
				if(*ptrToFlag)
					in_use = true;
			}
			else{
				//default case
				ptrToFlag = (ptrToBlock - 1);
				if(*ptrToFlag)
					in_use = true;
				ptrToFlag = NULL;
			}
			if(in_use) //if in use, callback fn
			{
				fn(ptrToBlock, stats_.ObjectSize_);
				numOfLeaks++;
				in_use = false;
			}
			// go to next block
			ptrToBlock = ptrToBlock + stats_.ObjectSize_ + config_.PadBytes_*2 + config_.HBlockInfo_.size_;
			
		}
		page = page->Next;
	}
	return numOfLeaks;
}


/**
 * @fn unsigned ObjectAllocator::ValidatePages(VALIDATECALLBACK fn) const
 * @brief 
 * 		Calls the callback fn for each block that is potentially corrupted
 * 
 * 		This function checks for memory corruption of each block of memory.
 * @param fn 
 * 		Pointer to a function that display the memory being corrupted.
 * @return unsigned 
 * 		return the number of corrupted memory region.
 */
unsigned ObjectAllocator::ValidatePages(VALIDATECALLBACK fn) const{
	unsigned numOfCorrupt = 0;
	GenericObject* page = PageList_;//loop through each page in use
	while(page){
		//loop through each block in page
		char* ptrToBlock = reinterpret_cast<char*>(page) + sizeof(size_t) + config_.PadBytes_ + config_.HBlockInfo_.size_; //firstblock
		for(int i=0;i<DEFAULT_OBJECTS_PER_PAGE;i++){
			//check if block is corrupted
			PaddState padState = isPadCorrupted(reinterpret_cast<void*>(ptrToBlock));
			if(padState == CORRUPT_LEFT || padState == CORRUPT_RIGHT) //if in use, callback fn
			{
				fn(ptrToBlock, stats_.ObjectSize_);
				numOfCorrupt++;
			}
			// go to next block
			ptrToBlock = ptrToBlock + stats_.ObjectSize_ + config_.PadBytes_*2 + config_.HBlockInfo_.size_;
			
		}
		page = page->Next;
	}
	return numOfCorrupt;
}

/**
 * @fn unsigned ObjectAllocator::FreeEmptyPages()
 * @brief 
 * 		This function frees and deallocate all empty pages
 * 
 * @return unsigned 
 * 		Return number of free pages
 */
unsigned ObjectAllocator::FreeEmptyPages(){
	unsigned freedEmptyPages = 0; //number of empty pages freed
	GenericObject* ptrToPage = PageList_;
	GenericObject* ptrToPreviousPage = ptrToPage;
	GenericObject* PageToDelete = NULL;
	size_t count = 0;

	while(ptrToPage){
		count++; // general purpose indexing
		
		if(isPageEmpty(ptrToPage)){
			ptrToPreviousPage->Next = ptrToPage->Next; //link previous page to following page
			freedEmptyPages++;
			stats_.FreeObjects_ -= config_.ObjectsPerPage_;
			stats_.PagesInUse_--;
			PageToDelete = ptrToPage;
		}

		ptrToPreviousPage = ptrToPage;
		ptrToPage = ptrToPage->Next; // flip to next page
		if((count == 1) && (PageToDelete)){
			//base case, re-point PageList if first page is to be deleted
			PageList_ = ptrToPage;
		}
		if(!PageToDelete) //delete page if NULL
			delete [] PageToDelete;
		PageToDelete = NULL;
	}

	return freedEmptyPages;
}

/**
 * @fn void ObjectAllocator::SetDebugState(bool State)
 * @brief 
 * 		This function returns the freelist pointer from the OA.
 * 
 * @param State 
 * 		For Testing/Debugging/Statistic methods
 * 		true=enable, false=disable
 */
void ObjectAllocator::SetDebugState(bool State){
	config_.DebugOn_ = State;
}

/**
 * @fn const void* ObjectAllocator::GetFreeList()
 * @brief 
 * 		returns a pointer to the internal free list
 * 
 * @return const void* 
 */
const void* ObjectAllocator::GetFreeList() const{
	return FreeList_;
}

/**
 * @fn const void* ObjectAllocator::GetPageList() const
 * @brief 
 * 		returns a pointer to the internal page list
 * 
 * @return const void* 
 */
const void* ObjectAllocator::GetPageList() const{
	return PageList_;
}
	
// returns the configuration parameters
/**
 * @fn OAConfig ObjectAllocator::GetConfig() const{
 * @brief 
 * 		returns the configuration parameters
 * @return OAConfig 
 * 		contains configuration properties of memory manager
 */		
OAConfig ObjectAllocator::GetConfig() const{
	return config_;
}  
 
/**
 * @fn OAStats ObjectAllocator::GetStats() const
 * @brief 
 * 		returns the statistics for the allocator 
 * @return OAStats 
 * 		contains statics info of Objet allocator
 */
OAStats ObjectAllocator::GetStats() const{
	return stats_;
}         

/**
 * @fn bool ObjectAllocator::checkObjectForPattern(void* ptrToBlock) const
 * @brief 
 * 		helper function that checks if object's for memory signatures
 * @param ptrToBlock 
 * 		pointer to object to be checked with
 * @param pattern 
 * 		memory signature to be checked for
 * @return true 
 * 		if object contains pattern at every byte in data
 * @return false 
 * 		if object doesnt contain parttern
 */
bool ObjectAllocator::checkObjectForPattern(void* ptrToBlock, const unsigned char pattern) const{
	//check object for freed pattern
	char* ptrToData = reinterpret_cast<char*>(ptrToBlock) + sizeof(void*); 
	
	size_t count = stats_.ObjectSize_ - sizeof(void*);
	size_t countToCheck = count;
	size_t countTocount = 0;
	while(count){
		if(static_cast<unsigned char>(*ptrToData) == pattern)
			countTocount++; // count number of times pattern was found
		count--;
	}
	if(countTocount == countToCheck){
		return true; // when pattern is found in every byte checked
	}
	return false; //pattern not completely found
}

/**
 * @fn void ObjectAllocator::ComputeAlignment()
 * @brief 
 * 		This helper function helps to compute the alignment of the memory block.
 */
void ObjectAllocator::ComputeAlignment() {
	// return if there is no alignment
	if (config_.Alignment_ == 0) return;

	// compute left alignment
	if(static_cast<unsigned int>(sizeof_LeftmostBlock)% config_.Alignment_ != 0) 
		config_.LeftAlignSize_ = config_.Alignment_ - (static_cast<unsigned int>(sizeof_LeftmostBlock) % config_.Alignment_);

	// compute inter alignment 
	if(static_cast<unsigned int>(sizeof_InnerBlock) % config_.Alignment_ != 0)
		config_.InterAlignSize_ = config_.Alignment_ - (static_cast<unsigned int>(sizeof_InnerBlock) % config_.Alignment_);
	else
		config_.InterAlignSize_ = 0;
}

/**
 * @fn char* ObjectAllocator::CreateAPage()
 * @brief 
 * 		helper function for creating a page based on configuration found
 * 		in OAConfig
 * @return char* 
 * 		returns pointer to Page Allocated
 */
char* ObjectAllocator::CreateAPage(){
	try{
		//1. Request memory for first page
		NewPage = new char[stats_.PageSize_];
		GenericObject* oldPage = PageList_;
		//2. Casting the page to a GenericObject* and adjusting pointers.
		PageList_ = reinterpret_cast<GenericObject* >(NewPage);
		PageList_->Next = oldPage;
		oldPage = NULL;
		stats_.PagesInUse_ += 1;//update acounting info
		stats_.FreeObjects_ += config_.ObjectsPerPage_;//update acounting info


		//3. Casting the 1st 16-byte block to GenericObject* and putting on free list
		GenericObject* currentObj = NULL;
		currentObj = FreeList_;
		char* NewObject = NewPage + sizeof(void*) + config_.PadBytes_ 
							+ config_.HBlockInfo_.size_ + config_.LeftAlignSize_; // point after page's Next
		memset(NewObject-config_.PadBytes_-config_.HBlockInfo_.size_, 0x00, config_.HBlockInfo_.size_);
		memset(NewObject-config_.PadBytes_, PAD_PATTERN, config_.PadBytes_);
		memset(NewObject, UNALLOCATED_PATTERN, stats_.ObjectSize_);
		memset(NewObject+stats_.ObjectSize_, PAD_PATTERN, config_.PadBytes_);
		memset(NewObject-config_.PadBytes_-config_.HBlockInfo_.size_-config_.LeftAlignSize_, ALIGN_PATTERN, config_.LeftAlignSize_);
		FreeList_ = reinterpret_cast<GenericObject*>(NewObject);
		FreeList_->Next = NULL;
		currentObj = NULL;
		NewObject = NULL;

		// cast remaining blocks
		unsigned int countBlocks = config_.ObjectsPerPage_ - 1;
		while(countBlocks){
			currentObj = FreeList_;
			NewObject = reinterpret_cast<char* >(FreeList_); 
			NewObject = NewObject + stats_.ObjectSize_ + config_.PadBytes_*2 + config_.HBlockInfo_.size_ + config_.InterAlignSize_;
			memset(NewObject-config_.PadBytes_-config_.HBlockInfo_.size_, 0x00, config_.HBlockInfo_.size_);
			memset(NewObject-config_.PadBytes_, PAD_PATTERN, config_.PadBytes_);
			memset(NewObject , UNALLOCATED_PATTERN, stats_.ObjectSize_ );
			memset(NewObject+stats_.ObjectSize_, PAD_PATTERN, config_.PadBytes_);
			memset(NewObject-config_.PadBytes_-config_.HBlockInfo_.size_-config_.InterAlignSize_, ALIGN_PATTERN, config_.InterAlignSize_);
			FreeList_ = reinterpret_cast<GenericObject* >(NewObject);
			FreeList_->Next = currentObj;
			currentObj = NULL;
			NewObject = NULL;

			countBlocks--;
		}

	}
	catch(std::bad_alloc &){
		throw OAException(OAException::E_NO_MEMORY, "allocate_new_page: No system memory available.");
	}
	return NewPage;
}

/**
 * @fn int ObjectAllocator::FindPageByObject(GenericObject* &p, void* Object) const
 * @brief 
 * 		helper function to find page that object was allocated
 * @param p 
 * 		reference to pointer to page that object was found at. 
 * @param Object 
 * 		pointer to object at which page to be found
 * @return int 
 * 		returns 1 if successful page found, return -1 if page not found
 */
int ObjectAllocator::FindPageByObject(GenericObject* &p, void* Object) const{
	GenericObject* page = reinterpret_cast<GenericObject*>(p);
	size_t ObjLowBou;
	size_t ObjUppBou;
	while(page){
		// find right page..
		ObjLowBou = reinterpret_cast<size_t>(Object) - reinterpret_cast<size_t>(page);
		ObjUppBou = reinterpret_cast<size_t>(page) + stats_.PageSize_ - reinterpret_cast<size_t>(Object);
		
		if(ObjLowBou + ObjUppBou == stats_.PageSize_ && ObjLowBou < stats_.PageSize_){
			// found right page
			p = page;
			return 1;
		}

		// flip to next page
		page = page->Next;
	}
	return -1; // page not found
}

/**
 * @fn ObjectAllocator::PaddState  ObjectAllocator::isPadCorrupted(void* ptrToBlock) const
 * @brief 
 * 		helper function to check if block's pad are corrupted 
 * @param ptrToBlock 
 * 		pointer to block for pad checking
 * @return ObjectAllocator::PaddState 
 * 		returns enum CORRUPT_LEFT or CORRUPT_RIGHT or NO_CORRUPT
 */
ObjectAllocator::PaddState  ObjectAllocator::isPadCorrupted(void* ptrToBlock) const{
	//check left and right pad bytes for pad pattern
	if(config_.PadBytes_ != 0){
		char* leftPadStart = reinterpret_cast<char*>(ptrToBlock) - config_.PadBytes_;
		char* rightPadStart = reinterpret_cast<char*>(ptrToBlock) + stats_.ObjectSize_;
		
		int count = config_.PadBytes_; // check each byte indiviually
		while(count!=0){
			if(static_cast<unsigned char>(*leftPadStart)!=PAD_PATTERN){
				return ObjectAllocator::CORRUPT_LEFT; //corruptioon found
			}
			if(static_cast<unsigned char>(*rightPadStart)!= PAD_PATTERN){
				return ObjectAllocator::CORRUPT_RIGHT; //corruptioon found
			}
			ptrToBlock = reinterpret_cast<char*>(ptrToBlock) +1;
			count--;
		}
		
	}
	return ObjectAllocator::NO_CORRUPT; // no corruption
}

/**
 * @fn bool ObjectAllocator::isPageEmpty(void* ptrToPage) const
 * @brief 
 * 		helper function used for checking if page is empty
 * @param ptrToPage 
 * 		input parameter, the pointer to page to be checked
 * @return true 
 * 		true if page is empty
 * @return false 
 * 		false if page is currently in use
 */
bool ObjectAllocator::isPageEmpty(void* ptrToPage) const{
	
	char* ptrToFirstBlock = reinterpret_cast<char*>(ptrToPage) + sizeof_LeftmostBlock;
	size_t count = config_.ObjectsPerPage_;
	size_t countFreeObjects = 0;
	while(count){
		//check each object in each if they are empty
		if(checkObjectForPattern(ptrToFirstBlock, FREED_PATTERN)){ //check for other patterns
			countFreeObjects++;
		}
		else if(checkObjectForPattern(ptrToFirstBlock, UNALLOCATED_PATTERN)){ //check for other patterns
			countFreeObjects++;
		}
		//when all blocks are empty, then page is empty
		if(countFreeObjects == config_.ObjectsPerPage_){
			return true;
		}
			

		ptrToFirstBlock = ptrToFirstBlock + sizeof_InnerBlock; //go to next block
		count--;
	}
	return false;
}