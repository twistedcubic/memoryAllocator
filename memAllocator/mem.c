#include <stdio.h>
#include <math.h>
#include <sys/mman.h>
#include <stdlib.h>
#include "mymem.h"
#include <pthread.h>

//heads of two free lists, should allocate to prescribed region?
void *slabFreeHeader ; //head of slab free list
struct FreeHeader *nextFreeHeader ;//head of nextfit free list
int init = 0; //number of times mem_init has been called
int slabS; //slab size
int slabss; //not rounded slabsize
void *middle;//start of nextFit region
void *begin; void *end;
pthread_mutex_t foo_mutex;

void *Mem_Init(int sizeOfRegion, int slabSize)
{
  init += 1;
  if (init > 1)
    return NULL;
  pthread_mutex_init(&foo_mutex, NULL);

  void *mm = mmap(NULL, sizeOfRegion, PROT_WRITE|PROT_READ, MAP_PRIVATE|MAP_ANON, -1, 0);//munmap!!
  if (mm == MAP_FAILED)
    {
      perror("mmap"); 
      return NULL;
    }
  //round slabSize to the nearest multiple of 16
  slabS = ceil(slabSize/(float)16)*16; //should be equal to the round up value
  slabss = slabSize;
  //start free list for slab
  slabFreeHeader = mm; //allocate 8 bytes for next free slab

  nextFreeHeader = (struct FreeHeader*)((char*)mm + (int)floor(sizeOfRegion*1/(float)4)); //start from second quarter
  middle = nextFreeHeader;
  //put in the right pointers to the next free slabs
  void **chk = mm;
  for(chk = mm; chk < middle; chk = (char*)chk +slabS)
    {//8 bytes for void*
      *chk = (char*)chk + slabS; //free header pointing to next free slab
      if (*chk == middle)
	*chk = NULL;    
    }

  nextFreeHeader = (struct FreeHeader*)nextFreeHeader;//cast base address to pointer to freeHeader struct
  nextFreeHeader->length = floor(sizeOfRegion*3/(float)4) - 16;
  nextFreeHeader->next = NULL; 

  begin = mm;
  end = (char*)mm + sizeOfRegion;

  return mm;
}

//slab allocator
void *slabA(int s)
{
  //walk through and check for free chunks!

  if(slabFreeHeader == middle)
    {
      slabFreeHeader = NULL;
      return NULL;
    }
  //use slabFreeHeader, update the header pointers
  if ((slabFreeHeader >= middle) || (slabFreeHeader == NULL)) //points to end, no more slab space available
    return NULL;

  void **old = slabFreeHeader; //slabFreeHeader should always be free, beginning of slab freelist
  
  *old = (char*)slabFreeHeader + slabS; //slabFreeHeader should contain an address to the next free chunk
  slabFreeHeader = (char*)slabFreeHeader + slabS; //update slabFreeHeader
  //zero out slab before giving to user!
  char *looping;
  for(looping = (char*)old; looping <= (char*)old+slabS; looping++)
    *looping = 0;
  return old;    
}

void *nextA(int s)
{
  if (nextFreeHeader == end || nextFreeHeader == NULL)
    {
      nextFreeHeader = NULL;
      return NULL;
    }

  //round to multiples of 16
  int size = ceil(s/(16.0))*16;
  //check to see if memory is big enough. If enough, convert to allocHeader and return it, if not, go to next freeHeader
  struct FreeHeader *looping = nextFreeHeader;
  struct FreeHeader *before = nextFreeHeader; //the freeHeader before looping
  int i = 0;
  for (looping = nextFreeHeader; looping->next != NULL && ((looping->next)->length) < size && ((looping->next) < end); looping = looping->next) //find big enough chunk 
    {i++;}
  //no good fit found
  if (looping == NULL || looping->next > end)
    return NULL;
  before = looping;
  if(i > 0)
    looping = looping->next; //looping has moved from nextFreeheader
  //remove looping from freelist
  if((looping->length - size - 16)>=16 ) //if chunk big enough, let before point to middle of looping chunk
    {struct FreeHeader *new = (struct FreeHeader*)((char*)looping+16+size);
      if (i > 0)      
	before->next = new;
      new->next = looping->next;
      new->length = looping->length - size - 16;
      if(i == 0)//nextFreeHeader still points at looping, which is used. If i>0, leave nextFreeHeader alone
	{nextFreeHeader = new;
	  nextFreeHeader->next = new->next;
	  nextFreeHeader->length = new->length;
	}
    }
  else //let before point to chunk after looping, i.e. remove looping from freelist
    {before->next = looping->next;
      if(i==0)
	{nextFreeHeader = looping->next;
	  if(nextFreeHeader != NULL)
	    {
	      nextFreeHeader->next = (looping->next)->next;
	      nextFreeHeader->length = (looping->next)->length;
	    }
	}
    }
  
  //zero out memory before returning! 
  char *j = looping;
  for (j = (char*)looping + 16; j < (char*)looping+size+16; j++)
    {
      *j = 0;
    }
  //cast from freeHeader to AllocatedHeader
  struct AllocatedHeader *ret = (struct AllocatedHeader*)looping;
  ret->length = size;
  ret->magic = MAGIC;
  //new nextFreeHeader 
  //nextFreeHeader = (struct FreeHeader *)((char*)nextFreeHeader + size + sizeof(struct FreeHeader)); 
  return ((char*)ret + sizeof(struct AllocatedHeader));
}

void *Mem_Alloc(int size)
{
  pthread_mutex_lock(&foo_mutex);
  if(init == 0)
    {  pthread_mutex_unlock(&foo_mutex);
      return NULL;
    }
  //check for special size
  if (size == slabss)
    {
      void *s = slabA(size);
      if (s == NULL) //not enough slab memory, move on to nextA
	{
	  void *s2 = nextA(size);
	  if (s2 == NULL)
	    {  pthread_mutex_unlock(&foo_mutex);
	      return NULL; //return NULL if nextA fails	
	    }  
	  //assert(s2 != NULL);
	  pthread_mutex_unlock(&foo_mutex);
	  return s2; //if nextA succeeds
	}
      else //slabA must have succeeded
	{  pthread_mutex_unlock(&foo_mutex);
	return s;
	}      
    } else //does not match slabS, use nextA
    {
      void *s3 = nextA(size);
      if (s3 == NULL) //not enough memory
	{  pthread_mutex_unlock(&foo_mutex);	
	  return NULL;
	}
      else 
	{
	  pthread_mutex_unlock(&foo_mutex);
	  return s3;
	}
    }
}

int Mem_Free(void *ptr)
{
  pthread_mutex_lock(&foo_mutex);
  if (init == 0)
    {  pthread_mutex_unlock(&foo_mutex);
      return -1;
    }
//if not of 16 bytes granularity, return -1
  if( ((char*)ptr-(char*)begin)%16 != 0) 
    {  pthread_mutex_unlock(&foo_mutex);
      return -1;
    }
  if (ptr == NULL)
    {  pthread_mutex_unlock(&foo_mutex);
      return -1;
    }
  //for slabs, if already free, return
  //ptr = (int*)ptr;
  void **ptr11=(void**)ptr;
  if (*ptr11 < middle && *ptr11 > begin) //already freed   *((int*)ptr)
    {  pthread_mutex_unlock(&foo_mutex);
      return 0;
    }
  //if outside mapped region, print segfault
  if(ptr < begin || ptr > end)  //define begin and end!
    {printf("SEGFAULT\n");
      pthread_mutex_unlock(&foo_mutex);
      exit(1); //failure
    }
  
  //slab region
  if(ptr < middle)
    {//invalid slab region to free, in the middle of some slab
      if((ptr - begin)%slabS != 0)
	{ pthread_mutex_unlock(&foo_mutex);
        return -1;
	}
      //if(*ptr11 != NULL && (*ptr11 > middle || *ptr11 < begin)) //invalid slab region to free      	
//find the previous and next free regions; update slabFreeHeader      
      void **ptr1 = (void**)ptr;      
      if (slabFreeHeader > ptr || slabFreeHeader == middle || slabFreeHeader==NULL) //ptr is foremost being freed
	{
	  *ptr1 = slabFreeHeader;
	  slabFreeHeader = ptr1; //slabFreeHeader points to block just freed
	}else //need to traverse backwards
	{
	  void **loopingB;  
	  for (loopingB = (char*)ptr1 - slabS; (*loopingB < middle) && (*loopingB > begin); loopingB = loopingB - slabS); //has to stop because slabFreeHeader lies before
	  //loopingB = (int*)loopingB;
	  printf("got here!\n");
	  *ptr1 = *loopingB;	  
	  *loopingB = ptr1; //loopingB should store an address ///////////
	}      

    }else //nextFit region
    {if (nextFreeHeader==NULL) //no free chunks 
	{nextFreeHeader = (struct FreeHeader *)((char*)ptr - 16);
	  nextFreeHeader->length = ((struct AllocatedHeader *)((char*)ptr - 16))->length;
	  nextFreeHeader->next = NULL;
	  pthread_mutex_unlock(&foo_mutex);
	  return 0;
	}
	//walk through the freelist from the beginning
      ptr = (char*)ptr - sizeof(struct AllocatedHeader); //skip to front of AllocatedHeader
      struct FreeHeader *ptr2 = (struct FreeHeader *)ptr; 
      if (nextFreeHeader > ptr2)
	{
	  //what if ptr points in the middle and not a header?
	  //coalesce
	  if(((char*)ptr2 + ptr2->length + sizeof(struct FreeHeader))==nextFreeHeader) //remember skipped to before previous header
	    {
	      ptr2->length = ptr2->length + nextFreeHeader->length + 16;
	      ptr2->next = nextFreeHeader->next;
	      nextFreeHeader = ptr2;
	      //also check to the right!!
	    }
	  else {//nextFreeHeader comes before ptr2
	  ptr2->next = nextFreeHeader; 
	  nextFreeHeader->next = ptr2; 	  
	  }
	} else
	{
	  //walk through, nextFreeHeader before ptr
	  struct FreeHeader *looping = nextFreeHeader; 	  

	  //backwards check
	  for(looping = nextFreeHeader; looping->next != NULL && looping->next < ptr2; looping = looping->next)
	    {//found the nearest free chunk before ptr
	      //immediately before, coalesce
	      if(((char*)looping + looping->length + 16) == ptr2)
		{//check to the right first
		  if((char*)ptr2 + ptr2->length + 16 == looping->next) //coalesce to the right
		    {
		      ptr2->next = looping->next;
		    }
		  looping->next = ptr2->next;   //coalesce
		  looping->length += 16 + ptr2->length;		  
		  
		}
	      else{ //not physically immediately before
		//check forwards
		if((char*)ptr2 + ptr2->length + 16 == looping->next) //coalesce to the right
		  {
		    ptr2->next = looping->next;
		  }
		
		struct FreeHeader *temp;
		temp = looping->next;	      
		looping->next = ptr2; 
		ptr2->next = temp;	      

	      }
	    }
	}
    }  
  pthread_mutex_unlock(&foo_mutex);
  return 0;
}

void Mem_Dump()
{
  printf("rounded slab size:%d ", slabS);
  printf("begin:%p middle:%p end:%p\n", begin,middle,end);
}
