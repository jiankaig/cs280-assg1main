#include "ObjectAllocator.h"
#include <iostream>//self-added
void ObjectAllocator::CreateAPage(){
	try{
		//1. Request memory for first page
		NewPage = new char[stats_->PageSize_];
		GenericObject* oldPage = PageList_;
		//2. Casting the page to a GenericObject* and adjusting pointers.
		PageList_ = reinterpret_cast<GenericObject* >(NewPage);
		PageList_->Next = oldPage;
		oldPage = NULL;
		stats_->PagesInUse_ += 1;//update acounting info
		stats_->FreeObjects_ += config_.ObjectsPerPage_;//update acounting info

		//3. Casting the 1st 16-byte block to GenericObject* and putting on free list
		GenericObject* currentObj = NULL;
		currentObj = FreeList_;
		NewObject = NewPage + sizeof(size_t) + config_.PadBytes_ + config_.HBlockInfo_.size_; // point after page's Next
		memset(NewObject-config_.PadBytes_-config_.HBlockInfo_.size_, 0x00, config_.HBlockInfo_.size_);
		memset(NewObject-config_.PadBytes_, PAD_PATTERN, config_.PadBytes_);
		memset(NewObject, UNALLOCATED_PATTERN, stats_->ObjectSize_);
		memset(NewObject+stats_->ObjectSize_, PAD_PATTERN, config_.PadBytes_);
		FreeList_ = reinterpret_cast<GenericObject*>(NewObject);
		FreeList_->Next = NULL;
		currentObj = NULL;
		NewObject = NULL;
		// printf("FreeList_: %p, FreeList_->Next: %p (in CreateAPage)\n", 
		// 	reinterpret_cast<void*>(FreeList_), reinterpret_cast<void*>(FreeList_->Next));

		//4. Casting the 2nd 16-byte block to GenericObject* and putting on free list

		currentObj = FreeList_;
		NewObject = reinterpret_cast<char* >(FreeList_); 
		NewObject = NewObject + stats_->ObjectSize_ + config_.PadBytes_*2 + config_.HBlockInfo_.size_;
		memset(NewObject-config_.PadBytes_-config_.HBlockInfo_.size_, 0x00, config_.HBlockInfo_.size_);
		memset(NewObject-config_.PadBytes_, PAD_PATTERN, config_.PadBytes_);
		memset(NewObject, UNALLOCATED_PATTERN, stats_->ObjectSize_);
		memset(NewObject+stats_->ObjectSize_, PAD_PATTERN, config_.PadBytes_);
		FreeList_ = reinterpret_cast<GenericObject* >(NewObject);
		FreeList_->Next = currentObj;
		currentObj = NULL;
		NewObject = NULL;
		// printf("FreeList_: %p, FreeList_->Next: %p (in CreateAPage)\n", 
		// 	reinterpret_cast<void*>(FreeList_), reinterpret_cast<void*>(FreeList_->Next));


		//5. Do the same for the 3rd and 4th blocks
		currentObj = FreeList_;
		NewObject = reinterpret_cast<char* >(FreeList_);
		NewObject = NewObject + stats_->ObjectSize_ + config_.PadBytes_*2 + config_.HBlockInfo_.size_; 
		memset(NewObject-config_.PadBytes_-config_.HBlockInfo_.size_, 0x00, config_.HBlockInfo_.size_);
		memset(NewObject-config_.PadBytes_, PAD_PATTERN, config_.PadBytes_);
		memset(NewObject, UNALLOCATED_PATTERN, stats_->ObjectSize_);
		memset(NewObject+stats_->ObjectSize_, PAD_PATTERN, config_.PadBytes_);
		FreeList_ = reinterpret_cast<GenericObject* >(NewObject);
		FreeList_->Next = currentObj;
		currentObj = NULL;
		NewObject = NULL;
		// printf("FreeList_: %p, FreeList_->Next: %p (in CreateAPage)\n", 
		// 	reinterpret_cast<void*>(FreeList_), reinterpret_cast<void*>(FreeList_->Next));


		currentObj = FreeList_;
		NewObject = reinterpret_cast<char* >(FreeList_);
		NewObject = NewObject + stats_->ObjectSize_ + config_.PadBytes_*2 + config_.HBlockInfo_.size_;
		memset(NewObject-config_.PadBytes_-config_.HBlockInfo_.size_, 0x00, config_.HBlockInfo_.size_);
		memset(NewObject-config_.PadBytes_, PAD_PATTERN, config_.PadBytes_);
		memset(NewObject, UNALLOCATED_PATTERN, stats_->ObjectSize_);
		memset(NewObject+stats_->ObjectSize_, PAD_PATTERN, config_.PadBytes_);
		FreeList_ = reinterpret_cast<GenericObject* >(NewObject);
		FreeList_->Next = currentObj;
		ObjectList = reinterpret_cast<GenericObject*>(NewObject);//Save Latest Block for easier access later
		currentObj = NULL;
		NewObject = NULL;
		// printf("FreeList_: %p, FreeList_->Next: %p (in CreateAPage)\n", 
		// 	reinterpret_cast<void*>(FreeList_), reinterpret_cast<void*>(FreeList_->Next));

	}
	catch(std::bad_alloc &){
		throw OAException(OAException::E_NO_MEMORY, "allocate_new_page: No system memory available.");
	}
}

// Creates the ObjectManager per the specified values
// Throws an exception if the construction fails. (Memory allocation problem)
ObjectAllocator::ObjectAllocator(size_t ObjectSize, const OAConfig& config)
: config_(config), stats_(nullptr), OAException_(nullptr)
{
	FreeList_ = NULL;
	PageList_ = NULL;
	
	stats_ = new OAStats();
	stats_->ObjectSize_ = ObjectSize;
	stats_->PageSize_ = sizeof(void*) + static_cast<size_t>(config.ObjectsPerPage_) * ObjectSize 
		+ config_.PadBytes_ * 2 * config_.ObjectsPerPage_
		+ config_.HBlockInfo_.size_ * (config_.ObjectsPerPage_);
	OAException_ = new OAException(OAException::E_BAD_BOUNDARY, "A message returned by the what method.");
	
	if(!config_.UseCPPMemManager_){
		// create if not using standard new/delete
		CreateAPage();
	}

}

// Destroys the ObjectManager (never throws)
ObjectAllocator::~ObjectAllocator(){
	
}	

// Take an object from the free list and give it to the client (simulates new)
// Throws an exception if the object can't be allocated. (Memory allocation problem)
void* ObjectAllocator::Allocate(const char *label) {
	if(config_.UseCPPMemManager_){
		stats_->Allocations_ += 1;
		size_t size = stats_->ObjectSize_;

		if(stats_->Allocations_ > stats_->MostObjects_ )
			stats_->MostObjects_ = stats_->Allocations_;

		return new char[size];
	}

	if(stats_->FreeObjects_ > 0){
		
		// Remove first object from free list for client
		NewObject = reinterpret_cast<char*>(FreeList_);
	// std::cout<<"BREAKPT1\n";
		// printf("FreeList_: %p, FreeList_->Next: %p (in Allocate)\n", 
		// 	reinterpret_cast<void*>(FreeList_), reinterpret_cast<void*>(FreeList_->Next));
		FreeList_ = FreeList_->Next;
		// printf("FreeList_: %p, FreeList_->Next: %p (in Allocate after)\n", 
		// 	reinterpret_cast<void*>(FreeList_), reinterpret_cast<void*>(FreeList_->Next));
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
		memset(NewObject, ALLOCATED_PATTERN, stats_->ObjectSize_);

		//update acounting info
		stats_->ObjectsInUse_ += 1;
		stats_->FreeObjects_ -= 1;
		stats_->Allocations_ += 1;
		if(stats_->Allocations_ > stats_->MostObjects_ )
			stats_->MostObjects_ = stats_->Allocations_;

	}
	else if (stats_->FreeObjects_ == 0 && stats_->PagesInUse_ < config_.MaxPages_){
		//check page limit before adding new page
		CreateAPage(); // create another page and link to preivous page

		// Remove first object from free list for client
		NewObject = reinterpret_cast<char*>(FreeList_);
		label = NewObject;
		FreeList_ = FreeList_->Next;

		// Process assignment to header block
		AllocNum +=1;
		ptrToHeaderBlock = NewObject-config_.PadBytes_ - config_.HBlockInfo_.size_;
		// Check header block type then assigning pointers
		if(config_.HBlockInfo_.type_ == OAConfig::hbExtended){
			ptrToUseCount = ptrToHeaderBlock + config_.HBlockInfo_.additional_;
			ptrToAllocNum = ptrToHeaderBlock + config_.HBlockInfo_.additional_ + 2;
			*ptrToUseCount = static_cast<char>(*ptrToUseCount + 1); // increment use count

			memset(ptrToAllocNum, static_cast<int>(AllocNum), 1); // set memory signature
			memset(ptrToAllocNum+4, 0x1, 1); // Flag assignment
		}
		else if(config_.HBlockInfo_.type_ == OAConfig::hbExternal){
			// allocate external header
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
			ptrToAllocNum = ptrToHeaderBlock;
			memset(ptrToAllocNum, static_cast<int>(AllocNum), 1); // set memory signature
			memset(ptrToAllocNum+4, 0x1, 1); // Flag assignment
		}

		//process byte pattern for Object space as "allocated"
		memset(NewObject, ALLOCATED_PATTERN, stats_->ObjectSize_);

		//update acounting info
		stats_->ObjectsInUse_ += 1;
		stats_->FreeObjects_ -= 1;
		stats_->Allocations_ += 1;
		if(stats_->Allocations_ > stats_->MostObjects_ )
			stats_->MostObjects_ = stats_->Allocations_;
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
		stats_->Deallocations_ += 1;
		delete [] reinterpret_cast<char*>(Object);
		return;
	}
	// printf("Trying to Free Object at: %p\n", Object);
	// printf("FreeList_: %p, FreeList_->Next: %p (in Free)\n", 
		// reinterpret_cast<void*>(FreeList_), reinterpret_cast<void*>(FreeList_->Next));

	//check for "double free"
	bool bNoDoubleFree = true;
	GenericObject* ptrFreeList = nullptr;
	ptrFreeList = FreeList_;
	// unsigned int countFree = stats_->FreeObjects_;
	while(ptrFreeList){
		// printf("in while looop\n");
		// printf("ptrFreeList: %p, Object: %p\n", reinterpret_cast<void*>(ptrFreeList), Object);
		if(ptrFreeList == reinterpret_cast<GenericObject* >(Object)){
			bNoDoubleFree = false; // repeated object freed
			break;
		}
	//  std::cout<<"BREAKP222\n";
	//  countFree--;
		ptrFreeList = ptrFreeList->Next;
	}
	ptrFreeList = nullptr;
	//  std::cout<<"BREAKPT3\n";

	//check for out-of-bounds
	char* lowerBoundary = nullptr;
	char* upperBoundary = nullptr;
	char* rightPage = nullptr;
	size_t ObjLowBou;
	size_t ObjUppBou;
	//char* Page2Head = nullptr;
	unsigned int count = 1;
	GenericObject* ptrPageList = PageList_;
	while(ptrPageList){
		if(count == 1){
			//store latest page's last byte address
			upperBoundary = reinterpret_cast<char*>(ptrPageList);
			upperBoundary = upperBoundary + stats_->PageSize_ - 1;
			//Page2Head = reinterpret_cast<char*>(ptrPageList);
		}
		if(count == stats_->PagesInUse_){
			//store oldest page's head addreass
			lowerBoundary = reinterpret_cast<char*>(ptrPageList);
		}
		
		// find right page..
		ObjLowBou = reinterpret_cast<size_t>(Object) - reinterpret_cast<size_t>(ptrPageList);
		ObjUppBou = reinterpret_cast<size_t>(ptrPageList) + stats_->PageSize_ - reinterpret_cast<size_t>(Object);
		// printf("Object: %lu ptrPageList: %lu\n", (size_t)Object, (size_t)ptrPageList);
		// printf("low: %lu Upp: %lu sum: %lu\n", ObjLowBou, ObjUppBou, ObjLowBou + ObjUppBou );
		if(ObjLowBou + ObjUppBou == stats_->PageSize_ && ObjLowBou < stats_->PageSize_){
			// found right page
			// printf("found page!\n");
			rightPage = reinterpret_cast<char*>(ptrPageList);
			break;
		}
		//printf("%i: %p - %p = %lu\n",count, (void*)ptrPageList, (void*)ptrPageList->Next, (size_t)ptrPageList - (size_t)ptrPageList->Next);
		// flip to next page
		count++;
		ptrPageList = ptrPageList->Next;
	}
	ptrPageList = NULL;
	if(Object < lowerBoundary || Object > upperBoundary)
	{
		//std::cout<<"outof boudns\n";
		throw OAException(OAException::E_BAD_BOUNDARY, "bad boundary. ");
	}

	//check if object is aligned and base case
	size_t ObjectPosition = (size_t)Object - (size_t)rightPage;
	bool isAligned = ((ObjectPosition - 8 - config_.PadBytes_ - config_.HBlockInfo_.size_)  
					% (stats_->ObjectSize_ + 2*config_.PadBytes_ + config_.HBlockInfo_.size_))  == 0;
	bool baseCase = ObjectPosition % stats_->PageSize_ == 0; //special case
	//printf("L:%p | U:%p | O:%p\n",lowerBoundary, upperBoundary, Object);
	// printf("objectpos: %lu\n", ObjectPosition);
	if(!isAligned || baseCase){
		//std::cout<<"MIS ALIGN!!!!!!!!!\n";
		throw OAException(OAException::E_BAD_BOUNDARY, "bad boundary. mis-alignment ");
	}

	//check left and right pad bytes for pad pattern
	if(config_.PadBytes_ != 0){
		char* leftPadStart = reinterpret_cast<char*>(Object) - config_.PadBytes_;
		char* rightPadStart = reinterpret_cast<char*>(Object) + stats_->ObjectSize_;
		
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
		//char* block = reinterpret_cast<char*>(Object);
		//delete [] block;
		//std::cout<<"freeing obejct\n";
		GenericObject* temp = FreeList_;
		FreeList_ = reinterpret_cast<GenericObject* >(Object);
		FreeList_->Next = temp;
		temp = NULL;
		// printf("FreeList_: %p, FreeList_->Next: %p (in Free after)\n", 
		// reinterpret_cast<void*>(FreeList_), reinterpret_cast<void*>(FreeList_->Next));

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
		memset(ptrToFreedPatternArea, FREED_PATTERN, stats_->ObjectSize_-sizeof(void*));
		

		//update acounting info
		stats_->ObjectsInUse_ -= 1;
		stats_->FreeObjects_ += 1;
		stats_->Deallocations_ += 1;
	}
	else if(!bNoDoubleFree){
		throw OAException(OAException::E_MULTIPLE_FREE, "double free. ");
	}
	
}


// Calls the callback fn for each block still in use
unsigned ObjectAllocator::DumpMemoryInUse(DUMPCALLBACK fn) const{
	fn(nullptr, 16);
	return (unsigned)1;
}

// Calls the callback fn for each block that is potentially corrupted
unsigned ObjectAllocator::ValidatePages(VALIDATECALLBACK fn) const{
	fn(nullptr, 16);
	return 1;
}

// Frees all empty page
unsigned ObjectAllocator::FreeEmptyPages(){
	return 1;
}

// Testing/Debugging/Statistic methods
// true=enable, false=disable
void ObjectAllocator::SetDebugState(bool State){
	if(State){

	}
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
	return *stats_;
}         

