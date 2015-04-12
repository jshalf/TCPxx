#ifndef __ELASTICBUFFER_H
#define __ELASTICBUFFER_H

// Elastic buffer stores raw data. (template it eventually?)
// It expands automatically to handle as much data as you feed it.
class ElasticBuffer
{
  int size,bufferSize;
  char *data;
public:
  ElasticBuffer(int initialSize=1);
  ~ElasticBuffer();  // Delete will deallocate the buffer.
  void add(char *buf,int bufsz);  // append to the buffer (will auto-resize)
  inline int getSize(){return size;} // returns number of bytes in buffer
  inline char *getData(){return data;} // returns a pointer to the data
  // Keep in mind that the data ptr. will be deallocated upon destruction
  // of the Elastic buffer object.  Furthermore, you do not want to ever
  // attempt to deallocate this data.  So, if this is a problem for you,
  // you might want to copy the data out of the buffer using the
  // copyData() method
  // It is possible that allowing a ref to the data is just an invitation
  // to bad programming style and bugs, so it may be a good idea to remove.
  // copyData() will allocate a new buffer unless you provide one.
  // If you provide your own and the size is less than the actual buffersize,
  // it will only copy the amount you specify.  If it is greater than the
  // buffersize, it will not copy past the size of the internal buffer.
  char *copyData(int long=0,char *buffer=NULL);
  // operator allows indexing of the data without getting the data itself.
  // automatically bounds the index to the array size.
  // can be used to loop over the contents of the buffer.
  inline char &operator[](int index){ return data[(index>size)?size-1:index]; }
};
#endif

