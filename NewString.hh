#ifndef __NEWSTRING_H_
#define __NEWSTRING_H_

#include <stdio.h>
#include <iostream>
#include <string.h>
using namespace std;
/***********************************************************
Name: NewString
Purpose: Dynamically allocate space in which to copy
         and copy a passed-in string to that space.
Params: s=The string to copy
Returns: Pointer to dynamically allocated copy of the string.
         NULL on failure
************************************************************/
char *NewString(char *s);
#endif

