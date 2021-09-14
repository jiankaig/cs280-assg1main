#include "ObjectAllocator.h"

// Creates the ObjectManager per the specified values
// Throws an exception if the construction fails. (Memory allocation problem)
ObjectAllocator::ObjectAllocator(size_t ObjectSize, const OAConfig& config){
	//unsigned x = config.MaxPages_;
	//unsigned y = config.ObjectsPerPage_;

	//1. Request memory for first page
	size_t pageSize = 4 + 4 * ObjectSize;//define number of bytes in page

	//Initiate by creating first object and next pointer 
	FirstPage_ = new GenericObject();
	GenericObject* temp = FirstPage_;
	GenericObject* NextObject;
	temp->Next = NextObject;

	//create remainder of objects in a page
	for (size_t i = 1; i < pageSize; i++) {
		NextObject = new GenericObject();//create next object
		temp = NextObject;//point to newly created object
		NextObject = temp->Next;//point to next one
	}
	
}

// Destroys the ObjectManager (never throws)
ObjectAllocator::~ObjectAllocator(){
	
}

// Take an object from the free list and give it to the client (simulates new)
// Throws an exception if the object can't be allocated. (Memory allocation problem)
void* ObjectAllocator::Allocate(const char *label){
	
}

// Returns an object to the free list for the client (simulates delete)
// Throws an exception if the the object can't be freed. (Invalid object)
void ObjectAllocator::Free(void *Object){
	
}


// Calls the callback fn for each block still in use
unsigned ObjectAllocator::DumpMemoryInUse(DUMPCALLBACK fn) const{
	
}

// Calls the callback fn for each block that is potentially corrupted
unsigned ObjectAllocator::ValidatePages(VALIDATECALLBACK fn) const{
	
}

// Frees all empty page
unsigned ObjectAllocator::FreeEmptyPages(){
	
}

// Testing/Debugging/Statistic methods
// true=enable, false=disable
void ObjectAllocator::SetDebugState(bool State){
	
}

// returns a pointer to the internal free list
const void* ObjectAllocator::GetFreeList() const{
	
	
}

// returns a pointer to the internal page list
const void* ObjectAllocator::GetPageList() const{
	
}
	
// returns the configuration parameters
OAConfig ObjectAllocator::GetConfig() const{
	
}  
 
// returns the statistics for the allocator 
OAStats ObjectAllocator::GetStats() const{
	
}         

