#include "ObjectAllocator.h"
#include <iostream>//self-added
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
		NewObject = NewPage + sizeof(size_t) + config_.PadBytes_ + config_.HBlockInfo_.size_; // point after page's Next
		memset(NewObject-config_.PadBytes_-config_.HBlockInfo_.size_, 0x00, config_.HBlockInfo_.size_);
		memset(NewObject-config_.PadBytes_, PAD_PATTERN, config_.PadBytes_);
		memset(NewObject, UNALLOCATED_PATTERN, stats_.ObjectSize_);
		memset(NewObject+stats_.ObjectSize_, PAD_PATTERN, config_.PadBytes_);
		FreeList_ = reinterpret_cast<GenericObject*>(NewObject);
		FreeList_->Next = NULL;
		currentObj = NULL;
		NewObject = NULL;

		// cast remaining blocks
		unsigned int countBlocks = config_.ObjectsPerPage_ - 1;
		while(countBlocks){
			currentObj = FreeList_;
			NewObject = reinterpret_cast<char* >(FreeList_); 
			NewObject = NewObject + stats_.ObjectSize_ + config_.PadBytes_*2 + config_.HBlockInfo_.size_;
			memset(NewObject-config_.PadBytes_-config_.HBlockInfo_.size_, 0x00, config_.HBlockInfo_.size_);
			memset(NewObject-config_.PadBytes_, PAD_PATTERN, config_.PadBytes_);
			memset(NewObject, UNALLOCATED_PATTERN, stats_.ObjectSize_);
			memset(NewObject+stats_.ObjectSize_, PAD_PATTERN, config_.PadBytes_);
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


// Creates the ObjectManager per the specified values
// Throws an exception if the construction fails. (Memory allocation problem)
ObjectAllocator::ObjectAllocator(size_t ObjectSize, const OAConfig& config)
: config_(config), stats_(), OAException_()
{
	FreeList_ = NULL;
	PageList_ = NULL;
	
	stats_.ObjectSize_ = ObjectSize;
	stats_.PageSize_ = sizeof(void*) + static_cast<size_t>(config.ObjectsPerPage_) * ObjectSize 
		+ config_.PadBytes_ * 2 * config_.ObjectsPerPage_
		+ config_.HBlockInfo_.size_ * (config_.ObjectsPerPage_);
	OAException_ = new OAException(OAException::E_BAD_BOUNDARY, "A message returned by the what method.");
	
	if(!config_.UseCPPMemManager_){
		// create if not using standard new/delete
		lowerBoundary = CreateAPage();
		//printf("first page start: %p\n", (void*)lowerBoundary);
		upperBoundary = lowerBoundary + stats_.PageSize_;
	}

}

// Destroys the ObjectManager (never throws)
ObjectAllocator::~ObjectAllocator(){
	
}	

// Take an object from the free list and give it to the client (simulates new)
// Throws an exception if the object can't be allocated. (Memory allocation problem)
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
		AllocNum +=1;
		ptrToHeaderBlock = NewObject-config_.PadBytes_ - config_.HBlockInfo_.size_;
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
				ptrHbExternal = new MemBlockInfo();
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
	// printf("Object allocated at: %p\n",reinterpret_cast<void*>(NewObject));
	return reinterpret_cast<void*>(NewObject);
}

// Returns an object to the free list for the client (simulates delete)
// Throws an exception if the the object can't be freed. (Invalid object)
void ObjectAllocator::Free(void *Object){
	
	if(config_.UseCPPMemManager_){
		stats_.Deallocations_ += 1;
		delete [] reinterpret_cast<char*>(Object);
		return;
	}

	//check for "double free"
	bool bNoDoubleFree = true;
	GenericObject* ptrFreeList = nullptr;
	ptrFreeList = FreeList_;
	
	while(ptrFreeList){
		// printf("in while looop\n");
		// printf("ptrFreeList: %p, Object: %p\n", reinterpret_cast<void*>(ptrFreeList), Object);
		if(ptrFreeList == reinterpret_cast<GenericObject* >(Object)){
			bNoDoubleFree = false; // repeated object freed
			break;
		}
		ptrFreeList = ptrFreeList->Next;
	}
	ptrFreeList = nullptr;

	//printf("Obj: %p       upp: %p    low: %p      ", Object, (void*)upperBoundary, (void*)lowerBoundary);
	//check for out-of-bounds
	if(Object < lowerBoundary || Object > upperBoundary)
	{
		throw OAException(OAException::E_BAD_BOUNDARY, "bad boundary. ");
	}

	//check if page can be found
	GenericObject* page = PageList_;
	int ret = FindPageByObject(page, Object);
	if(ret == -1)
		throw OAException(OAException::E_BAD_BOUNDARY, "bad boundary. page not found ");
	
	//check if object is aligned and base case
	size_t ObjectPosition = (size_t)Object - (size_t)page;
	// printf("OP:%p = Obj: %p - pg: %p\n",(void*)ObjectPosition, Object, (void*)page);
	bool isAligned = ((ObjectPosition - 8 - config_.PadBytes_ - config_.HBlockInfo_.size_)  
					% (stats_.ObjectSize_ + 2*config_.PadBytes_ + config_.HBlockInfo_.size_))  == 0;
	bool baseCase = ObjectPosition % stats_.PageSize_ == 0; //special case
	//printf("L:%p | U:%p | O:%p\n",lowerBoundary, upperBoundary, Object);
	// printf("objectpos: %lu\n", ObjectPosition);
	if(!isAligned || baseCase){
		//std::cout<<"MIS ALIGN!!!!!!!!!\n";
		throw OAException(OAException::E_BAD_BOUNDARY, "bad boundary. mis-alignment ");
	}

	//check left and right pad bytes for pad pattern
	if(config_.PadBytes_ != 0){
		char* leftPadStart = reinterpret_cast<char*>(Object) - config_.PadBytes_;
		char* rightPadStart = reinterpret_cast<char*>(Object) + stats_.ObjectSize_;
		
		int count = config_.PadBytes_;
		while(count!=0){
			if(*leftPadStart%0xFFFFFF00 !=PAD_PATTERN){
				throw OAException(OAException::E_CORRUPTED_BLOCK,"corruption on left");
			}
			if(*rightPadStart%0xFFFFFF00 != PAD_PATTERN){
				throw OAException(OAException::E_CORRUPTED_BLOCK,"corruption on right");
			}
			count--;
		}
		
	}
	


	//Free Object if no issues
	if(bNoDoubleFree){
		GenericObject* temp = FreeList_;
		FreeList_ = reinterpret_cast<GenericObject* >(Object);
		FreeList_->Next = temp;
		temp = NULL;

		ptrToHeaderBlock = reinterpret_cast<char* >(Object)-config_.PadBytes_ - config_.HBlockInfo_.size_;
		size_t BytesInBasicBlock;
		if(config_.HBlockInfo_.type_ == OAConfig::hbExtended){
			ptrToUseCount = ptrToHeaderBlock + config_.HBlockInfo_.additional_;
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


// Calls the callback fn for each block still in use
unsigned ObjectAllocator::DumpMemoryInUse(DUMPCALLBACK fn) const{
	unsigned numOfLeaks = 0;
	GenericObject* page = PageList_;//loop through each page in use
	while(page){
		//loop through each block in page
		// char* ptrToBlock = reinterpret_cast<char*>(page) + sizeof(size_t) + config_.PadBytes_ + config_.HBlockInfo_.size_; //firstblock
		char* ptrToBlock = reinterpret_cast<char*>(page) + sizeof(size_t) + OAConfig::BASIC_HEADER_SIZE; //firstblock
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

// Calls the callback fn for each block that is potentially corrupted
unsigned ObjectAllocator::ValidatePages(VALIDATECALLBACK fn) const{
	unsigned numOfCorrupt = 0;
	GenericObject* page = PageList_;//loop through each page in use
	while(page){
		//loop through each block in page
		char* ptrToBlock = reinterpret_cast<char*>(page) + sizeof(size_t) + config_.PadBytes_ + config_.HBlockInfo_.size_; //firstblock
		for(int i=0;i<DEFAULT_OBJECTS_PER_PAGE;i++){
			//check if block is corrupted
			if(isPadCorrupted(reinterpret_cast<void*>(ptrToBlock))) //if in use, callback fn
			{
				//  std::cout<<"callack\n";
				fn(ptrToBlock, stats_.ObjectSize_);
				numOfCorrupt++;
			}
			// go to next block
			ptrToBlock = ptrToBlock + stats_.ObjectSize_ + config_.PadBytes_*2 + config_.HBlockInfo_.size_;
			
		}
		//printf("page: %p, %p",(void*)page, (void*)page->Next);
		page = page->Next;
	}
	return numOfCorrupt;
}

// Frees all empty page
unsigned ObjectAllocator::FreeEmptyPages(){
	return 0;
}

// Testing/Debugging/Statistic methods
// true=enable, false=disable
void ObjectAllocator::SetDebugState(bool State){
	config_.DebugOn_ = State;
}

// returns a pointer to the internal free list
const void* ObjectAllocator::GetFreeList() const{
	return FreeList_;
}

// returns a pointer to the internal page list
const void* ObjectAllocator::GetPageList() const{
	return PageList_;
}
	
// returns the configuration parameters
OAConfig ObjectAllocator::GetConfig() const{
	return config_;
}  
 
// returns the statistics for the allocator 
OAStats ObjectAllocator::GetStats() const{
	return stats_;
}         

//Private methods

bool ObjectAllocator::isPadCorrupted(void* ptrToBlock) const{
	//check left and right pad bytes for pad pattern
	if(config_.PadBytes_ != 0){
		char* leftPadStart = reinterpret_cast<char*>(ptrToBlock) - config_.PadBytes_;
		char* rightPadStart = reinterpret_cast<char*>(ptrToBlock) + stats_.ObjectSize_;
		
		int count = config_.PadBytes_; // check each byte indiviually
		while(count!=0){
			//printf("Left PADD: %X != %X\n",(unsigned char)*leftPadStart, PAD_PATTERN);
			//printf("Right PADD: %X != %X\n",(unsigned char)*rightPadStart, PAD_PATTERN);
			if(static_cast<unsigned char>(*leftPadStart)!=PAD_PATTERN){
				return true; //corruptioon found
			}
			if(static_cast<unsigned char>(*rightPadStart)!= PAD_PATTERN){
				return true; //corruptioon found
			}
			ptrToBlock = reinterpret_cast<char*>(ptrToBlock) +1;
			count--;
		}
		
	}
	return false; // no corruption
}

int ObjectAllocator::FindPageByObject(GenericObject* &p, void* Object) const{
	GenericObject* page = reinterpret_cast<GenericObject*>(p);
	size_t ObjLowBou;
	size_t ObjUppBou;
	while(page){
		// find right page..
		// printf("Page123: %p\n", (void*)page);
		ObjLowBou = reinterpret_cast<size_t>(Object) - reinterpret_cast<size_t>(page);
		ObjUppBou = reinterpret_cast<size_t>(page) + stats_.PageSize_ - reinterpret_cast<size_t>(Object);
		// printf("ObjLowBou: %lu   ObjUppBou: %lu\n", ObjLowBou, ObjUppBou);
		if(ObjLowBou + ObjUppBou == stats_.PageSize_ && ObjLowBou < stats_.PageSize_){
			// found right page
			// printf("found page! at %p\n", (void*)(page));
			p = page;
			//FoundPage = reinterpret_cast<char*>(page);
			return 1;
		}

		// flip to next page
		page = page->Next;
	}
	return -1; // page not found
}
