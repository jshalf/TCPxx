#ifndef __LIST_TMPL_
#define __LIST_TMPL_

#include <stdio.h>
#include <iostream.h>
#include <string.h>
#include "NewString.hh"
/* List Class:  This a stupid templated implementation of an associative
    array of the sort you see in just about every issue of Dr. Dobb's Journal.
    It is used primarily to support the below MemLock class.
*/
template <class T,class K>
class List
{
  class Link
  {
    T data;
    K key;
    Link *next,*prev;
  public:
    Link(T d,K k,Link *nxt,Link *prv=0):
    data(d),key(k),next(nxt),prev(prv)
    {
      if(prv)
	prv->next=this;
      if(nxt)
	nxt->prev=this;
    }
    ~Link()
    {
      if(prev)
	prev->next=next;
      if(next)
	next->prev=prev;	
    }
    inline T getValue(){return data;}
    inline K getKey() {return key;}
    inline Link *getNext() {return next;}
    inline Link *getPrev() {return prev;}
  }; 
  Link *head;
  int size;
  int (*compare)(K,K);
public:
  List():head(0),size(0),compare(0){}
  ~List()
  {
    while(head)
    {
      Link *tmp=head->getNext();
      delete head;
      head=tmp;
    }
  }
  void setCompareFunction(int (*cmp)(K,K)) // if not set, use default == comparison
  {
    compare=cmp;
  }
  void add(T data,K key)
  {
    head=new List(data,key);
  }
  T find(K key)
  {
    for(Link *item=head;item;item=item->next)
    {
      if( (compare && compare(item->getKey(),key)) || (item->getKey()==key) )
	return item->getData();
    }
    return 0;
  }
  void remove(K key)
  {
    for(Link *item=head;item;item=item->next)
    {
      if( (compare && compare(item->getKey(),key)) || (item->getKey()==key) )
      {
	if(item=head)
	  head=item->next;
	delete item;
      }
    }
  }
};

#endif // __LIST_TMPL_
