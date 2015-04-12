#ifndef __ORDEREDLIST__TMPL_
#define __ORDEREDLIST__TMPL_
/*------------------------------------------------------------
  Stupid Tmpl implementation of linked list (because STL is a big 
	fat pig of a library that breaks almost every compiler!)
  Typical use of the orderd list type
  
   List<sphere *> SphereList;
   sphere *s;
   SphereList.reset();
   while(s=SphereList.getNext())
       s->visible=1;
------------------------------------------------------------*/
#include <stdio.h>
#include <iostream>
#include <string.h>

#define FOREACH(elem , list) list.reset(); while((elem = list.getNext()))

template <class T>
class OrderedList
{
  class LLink
  {		
  public:
    const T data;		
    LLink *next,*prev;
    LLink(const T d,LLink *nxt,LLink *prv=0):
    data(d),next(nxt),prev(prv)
    {
      if(nxt)
	nxt->prev=this;
      if(prv)
	prv->next=this;
    }
    ~LLink()
    {
      if(prev)
	prev->next=next;
      if(next)
	next->prev=prev;	
    }
  };
  LLink *head,*current,*tail;
  int size;
public:
  OrderedList():head(0),size(0),current(0) {}
  ~OrderedList(){
    while(head) {
      LLink *tmp=head->next;
      delete head;
      head=tmp;
    }
  }
  inline void push_back(const T data){ this->add(data); }
  void add(const T data)
  {
    head=new LLink(data,head);
    size++;
  }
  void remove(const T data){
    LLink *l=head;
    // make sure its not null
    if(!l) return;
    // do removal of all instances
    while(l){
      LLink *nxt=l->next;
      if(l->data==data) {
	// fixup links
	if(current==l) current=l->next;
	if(head==l) head=l->next;
	if(tail==l) tail=l->prev;
	delete l;
	size--;
      }
      l=nxt;
    }
    if(head==0) { tail=0; current=0; size=0;}
  }
  void del_last()
  { 
	if (current)
	{
		if (head==current->prev)
		{
			if (current->prev->prev)
				head=current->prev->prev;
			else
				head=current;
		}
		//if (tail==current->prev) still has to be figured out what to do here...
	 	delete current->prev; 
		size--; 
	}
  }

  void reset() { current=head; }

  T getNext() { 
    T rval;
    if(!current) return 0;
    rval=current->data;
    current=current->next;
    return rval;
  }		
  inline int getSize(){return size;}
};

#endif // __ORDEREDLIST__TMPL_
