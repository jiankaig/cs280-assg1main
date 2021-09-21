#include "ObjectAllocator.h"
#include <iostream>//self-added

// Creates the ObjectManager per the specified values
// Throws an exception if the construction fails. (Memory allocation problem)
ObjectAllocator::ObjectAllocator(size_t ObjectSize, const OAConfig& config)
: config_(config), stats_(nullptr), OAException_(nullptr)
{
	stats_ = new OAStats();
	stats_->ObjectSize_ = ObjectSize;
	stats_->PageSize_ = sizeof(void*) + static_cast<size_t>(config.ObjectsPerPage_) * ObjectSize;
	OAException_ = new OAException(OAException::E_NO_MEMORY, "A message returned by the what method.");
	
	try{
		//1. Request memory for first page
		NewPage = new char[stats_->PageSize_];	
		PageList_ = NULL;
		FreeList_ = NULL;

		//2. Casting the page to a GenericObject* and adjusting pointers.
		PageList_ = reinterpret_cast<GenericObject* >(NewPage);
		PageList_->Next = NULL;
		stats_->PagesInUse_ += 1;//update acounting info
		stats_->FreeObjects_ += config.ObjectsPerPage_;//update acounting info

		//3. Casting the 1st 16-byte block to GenericObject* and putting on free list
		FreeList_ = reinterpret_cast<GenericObject*>(NewPage+sizeof(size_t));
		FreeList_->Next = NULL;

		//4. Casting the 2nd 16-byte block to GenericObject* and putting on free list
		GenericObject* currentObj;
		char* NextObj;
		currentObj = FreeList_;
		NextObj = reinterpret_cast<char* >(FreeList_ + stats_->ObjectSize_); 
		FreeList_ = reinterpret_cast<GenericObject* >(NextObj);
		FreeList_->Next = currentObj;
		currentObj = NULL;
		NextObj = NULL;

		//5. Do the same for the 3rd and 4th blocks
		currentObj = FreeList_;
		NextObj = reinterpret_cast<char* >(FreeList_ + stats_->ObjectSize_); 
		FreeList_ = reinterpret_cast<GenericObject* >(NextObj);
		FreeList_->Next = currentObj;
		currentObj = NULL;
		NextObj = NULL;

		currentObj = FreeList_;
		NextObj = reinterpret_cast<char* >(FreeList_ + stats_->ObjectSize_); 
		FreeList_ = reinterpret_cast<GenericObject* >(NextObj);
		FreeList_->Next = currentObj;
		currentObj = NULL;
		NextObj = NULL;
	}
	catch(std::bad_alloc &){
		throw OAException(OAException::E_NO_MEMORY, "allocate_new_page: No system memory available.");
	}

}

// Destroys the ObjectManager (never throws)
ObjectAllocator::~ObjectAllocator(){
	
}

// Take an object from the free list and give it to the client (simulates new)
// Throws an exception if the object can't be allocated. (Memory allocation problem)
void* ObjectAllocator::Allocate(const char *label) {
	//std::cout<<"FreeObjects: "<<stats_->FreeObjects_<<std::endl;
	if(stats_->FreeObjects_ > 0){
		//update acounting info
		stats_->ObjectsInUse_ += 1;
		stats_->FreeObjects_ -= 1;
		stats_->Allocations_ += 1;
		if(stats_->Allocations_ > stats_->MostObjects_ )
			stats_->MostObjects_ = stats_->Allocations_;
		// Remove first object from free list for client
		label = reinterpret_cast<char*>(FreeList_);
		FreeList_ = FreeList_->Next;

	}
	else if (stats_->FreeObjects_ == 0 && stats_->PagesInUse_ < config_.MaxPages_){
		//check page limit before adding new page
		//create another page and link to preivous page
		NewPage = new char[stats_->PageSize_];
		GenericObject* oldPage = PageList_;
		PageList_ = reinterpret_cast<GenericObject* >(NewPage);
		PageList_->Next = oldPage;
		oldPage = NULL;

		//2. Casting the page to a GenericObject* and adjusting pointers.
		PageList_ = reinterpret_cast<GenericObject* >(NewPage);
		PageList_->Next = NULL;
		stats_->PagesInUse_ += 1;//update acounting info
		stats_->FreeObjects_ += config_.ObjectsPerPage_;//update acounting info

		//3. Casting the 1st 16-byte block to GenericObject* and putting on free list
		FreeList_ = reinterpret_cast<GenericObject*>(NewPage+sizeof(size_t));
		FreeList_->Next = NULL;

		//4. Casting the 2nd 16-byte block to GenericObject* and putting on free list
		GenericObject* currentObj;
		char* NextObj;
		currentObj = FreeList_;
		NextObj = reinterpret_cast<char* >(FreeList_ + stats_->ObjectSize_); 
		FreeList_ = reinterpret_cast<GenericObject* >(NextObj);
		FreeList_->Next = currentObj;
		currentObj = NULL;
		NextObj = NULL;

		//5. Do the same for the 3rd and 4th blocks
		currentObj = FreeList_;
		NextObj = reinterpret_cast<char* >(FreeList_ + stats_->ObjectSize_); 
		FreeList_ = reinterpret_cast<GenericObject* >(NextObj);
		FreeList_->Next = currentObj;
		currentObj = NULL;
		NextObj = NULL;

		currentObj = FreeList_;
		NextObj = reinterpret_cast<char* >(FreeList_ + stats_->ObjectSize_); 
		FreeList_ = reinterpret_cast<GenericObject* >(NextObj);
		FreeList_->Next = currentObj;
		currentObj = NULL;
		NextObj = NULL;

		// Remove first object from free list for client
		label = reinterpret_cast<char*>(FreeList_);
		FreeList_ = FreeList_->Next;

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

	return (void*)label;
}

// Returns an object to the free list for the client (simulates delete)
// Throws an exception if the the object can't be freed. (Invalid object)
void ObjectAllocator::Free(void *Object){
	GenericObject* temp = FreeList_;
	FreeList_ =reinterpret_cast<GenericObject* >(Object);
	FreeList_->Next = temp;
	temp = NULL;

	//update acounting info
	stats_->ObjectsInUse_ -= 1;
	stats_->FreeObjects_ += 1;
	stats_->Deallocations_ += 1;
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

