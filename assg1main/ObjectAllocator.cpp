#include "ObjectAllocator.h"

// Creates the ObjectManager per the specified values
// Throws an exception if the construction fails. (Memory allocation problem)
ObjectAllocator::ObjectAllocator(size_t ObjectSize, const OAConfig& config)
: config_(config)
{
	stats_->ObjectSize_ = ObjectSize;
	stats_->PageSize_ = 4 + 4 * ObjectSize;
	//unsigned x = config.MaxPages_;
	//unsigned y = config.ObjectsPerPage_;
	
	//1. Request memory for first page
	size_t pageSize = 4 + 4 * ObjectSize;//define number of bytes in page
	char* cFirstPage;
	try{
		cFirstPage = new char[pageSize];	
		PageList_ = NULL;
		FreeList_ = NULL;

		//2. Casting the page to a GenericObject* and adjusting pointers.
		PageList_ = reinterpret_cast<GenericObject* >(cFirstPage);
		PageList_->Next = NULL;

		//3. Casting the 1st 16-byte block to GenericObject* and putting on free list
		FreeList_ = reinterpret_cast<GenericObject*>(cFirstPage+4);
		FreeList_->Next = NULL;

		//4. Casting the 2nd 16-byte block to GenericObject* and putting on free list
		GenericObject* temp;
		temp = reinterpret_cast<GenericObject*>(cFirstPage+20);//replace with sizeof() or ObjectSize
		temp->Next = FreeList_->Next;
		FreeList_ = temp;
		temp = NULL;

		//5. Do the same for the 3rd and 4th blocks
		temp = reinterpret_cast<GenericObject*>(cFirstPage+36);
		temp->Next = FreeList_->Next;
		FreeList_ = temp;
		temp = NULL;

		temp = reinterpret_cast<GenericObject*>(cFirstPage+52);
		temp->Next = FreeList_->Next;
		FreeList_ = temp;
		temp = NULL;
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
void* ObjectAllocator::Allocate(const char *label){
	// Remove first object from free list for client
	label = reinterpret_cast<char*>(FreeList_);
	FreeList_ = FreeList_->Next;
	return (void*)label;
}

// Returns an object to the free list for the client (simulates delete)
// Throws an exception if the the object can't be freed. (Invalid object)
void ObjectAllocator::Free(void *Object){
	GenericObject* temp = FreeList_;
	FreeList_ =reinterpret_cast<GenericObject* >(Object);
	FreeList_->Next = temp;
	temp = NULL;
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
	return nullptr;
}

// returns a pointer to the internal page list
const void* ObjectAllocator::GetPageList() const{
	return nullptr;
}
	
// returns the configuration parameters
OAConfig ObjectAllocator::GetConfig() const{
	return config_;
}  
 
// returns the statistics for the allocator 
OAStats ObjectAllocator::GetStats() const{
	return *stats_;
}         

