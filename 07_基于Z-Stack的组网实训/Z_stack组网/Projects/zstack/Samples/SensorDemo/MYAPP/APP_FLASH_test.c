/**************************************************************************************************
  Filename:       sapi.c
  Revised:        $Date: 2010-01-06 16:39:32 -0800 (Wed, 06 Jan 2010) $
  Revision:       $Revision: 21446 $

  Description:    Z-Stack Simple Application Interface.

**************************************************************************************************/

/******************************************************************************
 * INCLUDES
 */

#include "ZComDef.h"
#include "hal_drivers.h"
#include "OSAL.h"
#include "OSAL_Tasks.h"
//#include "OSAL_Custom.h"

#if defined ( MT_TASK )
  #include "MT.h"
  #include "MT_TASK.h"
#endif

#include "nwk.h"
#include "APS.h"
#include "ZDApp.h"

#include "osal_nv.h"
#include "NLMEDE.h"
#include "AF.h"
#include "OnBoard.h"
#include "nwk_util.h"
#include "ZDProfile.h"
#include "ZDObject.h"
#include "hal_led.h"
#include "hal_key.h"
#include "hal_lcd.h"
#include "sapi.h"
#include "MT_SAPI.h"
#include "UART_PRINT.h"
#include "app_flash.h"

void test_read_flash2 (void );
void test_read_flash1 (void );
void    test_nv_item(void);
void    delay(uint16 i);
uint8   zb_Readchancel(void);
void   zb_Writechancel(uint8 ch);
void    read_appnv(uint16 id, uint8 len,void * buf);
void    write_appnv(uint16 id, uint8 len,void * buf);


const uint32 chn[16]={0x800,0x1000,0x2000,0x4000,0x8000,0x10000,0x20000,0x40000,
    0x80000,0x100000,0x200000,0x400000,0x800000,0x1000000,0x2000000,0x4000000};
/******************************************************************************
 * @fn          
 *
 * @brief       .
 *
 * @param       
 *
 *
 * @return      
 *
 *******************************************************************/


void    delay(uint16 i)
{
    uint8   j;
    while(i--)
    {
        for(j=0; j<250; j--)
        {
            asm("NOP");
            asm("NOP");
            asm("NOP");
            asm("NOP");
            asm("NOP");
            asm("NOP");
            asm("NOP");
            asm("NOP");
        }
    }
}

uint8   zb_Readchancel(void)
{
    uint8   ch;
    uint32  chancel;
    zb_ReadConfiguration(ZCD_NV_CHANLIST,4, &chancel );
    uart_printf("CHANCEL VALUE:%8lx\r\n" ,chancel);
    //uart_printf("0x123456:%08lx\r\n" ,(uint32)0x123456); 
    delay(65535);
    for(ch=11; ch<27; ch++)
    {
        if(chancel == chn[ch-11] )
           return ch; 
    }
    return  0;
}

void   zb_Writechancel(uint8 ch)
{
    
    uint32  chancel;
    if ( (ch<11) || (ch>26))
        return;
    chancel = chn[ch-11];
    zb_WriteConfiguration(ZCD_NV_CHANLIST, 4, &chancel);
}
/******************************************************************************
 * @fn          
 *
 * @brief     test for pand_id and chancel rean and write   .
 *
 * @param       
 *
 *
 * @return      
 *
 *******************************************************************/

void test_read_flash1 (void )
{
    uint16 pand[]={0x5a9e, 0x1234,0x2356,0xf567};
    uint8 ch[] = {11,21,26,19};
    static  uint8   cnt=0;
    uint8   chancel;
    uint16  pand_id;
    //delay(65535);

    //HalUARTWrite(HAL_UART_PORT_0,"\r\nTEST Start!...\r\n",18);
    uart_printf("\r\nTEST START!\r\n");
    
    zb_ReadConfiguration(ZCD_NV_PANID,2, &pand_id );
    chancel=zb_Readchancel();
    uart_printf("pand_id0:%04x\r\n" , pand_id);
    uart_printf("chancel0:%d\r\n" , chancel);
    
    if(cnt>3)
       cnt=0; 
   
    
    zb_WriteConfiguration(ZCD_NV_PANID, 2, &pand[cnt] );
    zb_Writechancel(ch[cnt]);
    zb_ReadConfiguration(ZCD_NV_PANID,2, &pand_id );
    chancel=zb_Readchancel();
    uart_printf("pand_id:%04x\r\n" , pand_id);
    uart_printf("chancel:%d\r\n\r\n" , chancel);
    cnt++;
    
    	
}
void    test_nv_item(void)
{
    uint8  size;
    static uint8   i=0;
    const uint16 dat[16]={0x80,0x87,0x82,0x83,0x84,0x201,0x202,0x203,0x204,0x205,0x206,0x207,0x208,0x209,0x20a};
    //uart_printf("\r\n nv item 地址--数据长度test\r\n" );
    //for(i=0;i<16;i++)
    {
        size=osal_nv_item_len( dat[i]);
        uart_printf("地址:0x%x " ,  dat[i]);
        uart_printf("数据长度:%d\r\n" ,  size);
        i++;
        i &=0x0f;
    }

}
/******************************************************************************
 * @fn          
 *
 * @brief     test for flash rean and write   .
 *
 * @param       
 *
 *
 * @return      
 *
 *******************************************************************/
#define applen  16
void    read_appnv(uint16 id,uint8 len,void * buf)
{
    uint8   f1;
    uint32 u32;
    f1=osal_nv_read(id, 0,applen, buf); 
    if( f1 != ZSUCCESS)
     {
          uart_printf("READ ERROR:%d\r\n", f1);
          uart_printf("adress:%x, len:%d\r\n",id, len);
    }
    
}
void    write_appnv(uint16 id, uint8 len,void * buf)
{
    uint8   f1;
    f1=zb_WriteConfiguration(id,applen, buf);
    if( f1 != ZSUCCESS)
        {
          uart_printf("WRITE ERROR:%d\r\n", f1);
          uart_printf("adress:%x, len:%d\r\n",id, len);
    }
}


void test_read_flash2 (void )
{
    uint8 dat[2][16]={{0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f},
    {0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f} };
    uint32  d32[2]={0x123456, 0x98765432};
    static   uint8 st=0;
    //static   uint16 nvid=APP_NV; 
    static   uint16 nvid=0x88;
    static   uint8 nvl=1, nvinit=0;
    uint8 dat1[16];
    if(!nvinit)
    {
        osal_nv_item_init(nvid, applen, NULL );
        uart_printf("\r\n初始化nv!\r\n");
    }
    nvinit=1;
    
    if(nvl>16)
    {  
        nvl=1;
       // nvid++;
    }
   // uart_printf("\r\nstart nvid:%x, len:%d:\r\n", nvid,nvl);
    
    //read_appnv(nvid, nvl, dat1);
    //uart_printf("DATA0:\r\n");
    //uart_datas(dat1,16);
    
    write_appnv(nvid, nvl,dat[st]);
    //write_appnv(&d32[st]);
    
    read_appnv(nvid, nvl, dat1);
   // uart_printf("DATA1:\r\n");
    uart_datas(dat1,16);
    nvl++;
    st++;
    st &=1;
}


void test_read_flash (void )
{
   //HalUARTInit();
    //test_read_flash1();
    test_read_flash2();
   //test_nv_item();
   //while(1);
    
}