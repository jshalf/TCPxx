// FlexArrayTmpl.hh
#ifndef __FLEXARRAY_TMPL_HH_
#define __FLEXARRAY_TMPL_HH_

#ifdef SGI
#include <new.h>
#endif

//#include "Arch.h"
//#define DOUBLEWORD 8 // sgi doubleword == -align64 at a minimum
#define DOUBLEWORD 1
/*-------------
 Template: FlexArray
 Purpose: Simplifies dynamically allocated array management since it contains
 array size information and manages array resizing.  However, since speed-critical
 operations like indexing (the [] operator) are inlined, it adds no speed
 penalty for the most common speed-critical operations.
 Method Summary:
 constructor: The initial "size" is 0 (the size of the array that is accessible
 using the indexing operator [].  However, the internally allocated size is
 a minimum of 8 elements.
 [] : Indexing operator.  Acts just like [] would for a generic array.  Since
 it is inlined, there is actually no penalty in performance compared to
 using a generic array.
 append : Add an element to the end of the array, resizing the array if necessary.
 The arrays are allocated in blocks so it doesn't need to realloc on every append.
 For arrays < 1024 elements long, it doubles the array size on each realloc.  When
 larger than 1024 elements, it adds 1024 elements to the array on every realloc.
 setSize():   Sets the internal and externally availible array sizes to the
 specified size; shrinking a larger internal array if necessary.
 getSize():   Return the current externally availible array size.  The internal
 size may be larger.	
---------------*/
template<class T>
class FlexArray {
  int size,maxsize;
  T *data;
  // Internal reallocation procedure.
  void grow(int targetSize=1);
public:
	// constructor
  FlexArray(int sz=0);
  // copy constructor
  FlexArray(FlexArray &src);
  FlexArray &operator=(FlexArray<T> &src);
  // destructor
  ~FlexArray();
  inline int getSize(){return size;}
  int setSize(int s);
  int append(T value);
  int append(T *values,int nvals);
#ifdef FLEXARRAYDEBUG
  T &operator[](int index){
    if(index>=size || index<0) { // range check
      printf("FlexArray:: Bad index. Arraysize=%u Index=%u\n",
	     size,index);
      index=0;
    }
    return (data[index]); 
  }
#else
  inline T &operator[](int index){ return (data[index]); }
#endif
  // purge? which forces array delete
  inline void purge(){ delete[] data; data=0; maxsize=size=0; }
  //inline operator T*() { return data; }  // typecasting operator
  inline T *getData() { return data; } // sometimes you need direct access to the contained array.
};

template<class T>
FlexArray<T>::FlexArray(int sz):size(sz){
  /*
  if(sz>DOUBLEWORD) maxsize=sz;
  else maxsize=DOUBLEWORD;  always allocate in doubleword chunks
  data = new T[maxsize];
  */
  maxsize=sz; // problem here is that virtually guarauntees reallocation
  if(maxsize>0) data = new T[maxsize];
  else data = 0;
}

template<class T>
FlexArray<T>::~FlexArray(){
  if(maxsize>0 && data)
    delete[] data;
  size=maxsize=0;
  data=0;
}


template<class T>
void FlexArray<T>::grow(int targetSize){
  do{
    if(!maxsize) maxsize=1;
    if(maxsize<1024)
      maxsize<<=1; // double in size
    else
      maxsize+=1024; // increase by 1k element block
  }while(maxsize<targetSize);
  T *newdata=new T[maxsize];
  // has to do its own copy because realloc is not always compatible with
  // new/delete on all machines
  if(data){
    for(int i=0;i<size;i++)
      newdata[i]=data[i];
    delete[] data;
  }
  data=newdata;
}

template<class T>
FlexArray<T>::FlexArray(FlexArray<T> &src):data(0),size(0),maxsize(0){
  setSize(src.getSize());
  //puts("\tFlexArray<T>copy constructor***********");
  for(int i=0;i<src.getSize();i++)
    data[i]=src.data[i];
    //this->data[i]=src->data[i];

    //    (*this)[i]=src[i]; // a full stupid copy!!
}

template<class T>
FlexArray<T> &FlexArray<T>::operator=(FlexArray<T> &src){
  if(this == &src) return *this;
  setSize(src.getSize());
  //puts("\t\tFlexArray<T>copy operator");
  for(int i=0;i<src.getSize();i++)
    data[i]=src[i]; // a full stupid copy!!
  return *this;
}

template<class T>
int FlexArray<T>::setSize(int s){
  if(s<size){
    size=s;
    return size; // can't allow shrinkage below current size
  }
  if(s==maxsize){
    size=maxsize;
    return size;
  }
  // otherwise if maxsize>s>size>
  maxsize=s;
  T *newdata=new T[maxsize];
  if(data){
    for(int i=0;i<size && i<maxsize;i++)
      newdata[i]=data[i];
    delete[] data;
  }
  data=newdata;
  size=maxsize;
  return size;
}

template<class T>
int FlexArray<T>::append(T value){
  if(size>=maxsize) // failsafe check
    grow(size+1); // this array only increases in size
  data[size++]=value;
  return size;
}

template<class T>
int FlexArray<T>::append(T *values,int nvals){
  if(size+nvals >= maxsize)
    grow(size+nvals);//this array only increases in size
  for(int i=0;i<nvals;i++)
    data[size++]=values[i];
  return size;
}

/*
  1) Refcount must be fixed
  2) Need to have more flexibility in how the block allocation
     is defined (eg. memory allocation blocksize)  Use obstacks?
     This would require a more global memory pool
*/

#endif // __FLEXARRAY_TMPL_HH_
