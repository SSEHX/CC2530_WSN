/*************************************************************************************************
  Filename:       sh10.h
 
 *****/


#ifndef SIMPLE_SH10_H
#define SIMPLE_SH10_H

typedef struct __WENSHIDU__
{
	unsigned   char gewei;
	unsigned   char shiwei;
	unsigned   char DateString1[4];
}WENSHIDU;
extern  WENSHIDU S;

extern  void call_sht11(void); 
extern  void connectionreset(void);
#endif


