#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <list>
#include <queue>


int main(int argc,char *argv[]){
  std::queue<int> q;
  // std::queue<int>::iterator it;
  int i;
  for(i=0;i<20;i++) q.push(i);
  for(i=0;i<20;i++) {
    printf("Q top[%u] = %u\n",i,q.front());
    q.pop();
  }
  puts("Next:*****");
  
  for(i=0;i<20;i++) q.push(i);

  //for(it=q.begin();it!=q.end;it++){
  //  printf("Q iterator[%u]= %u\n",(*it));
  // }
  printf("Q size = %u\n",q.size());
}
/*
  requirements for look-back circ buffer
  1) scan for seq #
  2) Upon hit, return the # of intervening packets
  3) Then we can iterate dequeue the last few packets
  for resend (depending on OOO limit).

  4) On recv side, do aggressive NAKs for missing packets?
  Or embed the gap information in each ACK.
*/
