/**************************************************************************************************
  Filename:       appflash.c
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
#include "DemoApp.h"

void test_read_flash2 (void );
void test_read_flash1 (void );
void    test_nv_item(void);
void    delay(uint16 i);
uint8   zb_Readchancel(void);
void   zb_Writechancel(uint8 ch);
void    read_appnv(uint16 id, uint8 len,void * buf);
void    write_appnv(uint16 id, uint8 len,void * buf);
void    zb_Readpandid(void * buf);
void   zb_Writepandid(void * buf);


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
uint8 calcXOR(uint8 *pBuf, uint8 len)
{
  uint8 rtrn = 0;

  while (len--)
  {
    rtrn ^= *pBuf++;
  }

  return rtrn;
}


void    zb_Readpandid(void * buf)
{
    zb_ReadConfiguration(ZCD_NV_PANID,2, buf );
}
void   zb_Writepandid(void * buf)
{
    zb_WriteConfiguration(ZCD_NV_PANID, 2, buf );
}


uint8   zb_Readchancel(void)
{
    uint8   ch;
    uint32  chancel;
    zb_ReadConfiguration(ZCD_NV_CHANLIST,4, &chancel );
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
void    read_appconfig(void * buf)
{
    uint8   f1;
    osal_nv_item_init(APP_NV, 4, NULL );
    f1=osal_nv_read(APP_NV, 0,4, buf); 
    if( f1 != ZSUCCESS)
     {
          uart_printf("READ ERROR:%d\r\n", f1);
     }
}


void    write_appconfig(void * buf)
{
    uint8   f1;
    osal_nv_item_init(APP_NV, 4, NULL );
    f1=zb_WriteConfiguration(APP_NV,4, buf);
    if( f1 != ZSUCCESS)
        {
          uart_printf("WRITE ERROR:%d\r\n", f1);
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
// e9 e3 len pandidl pandidh chancel dat[16]
void    configset(uint8 * pBuf,uint8 len, uint8 logtype)
{
    
    if((pBuf[0]==0xe9) && (pBuf[1]==0xe3))
     {
#ifndef NO_CHK
          if(pBuf[len-1]!=calcXOR(pBuf+2,len-3))
          {         // chk error
              
              pBuf[2]=0x01;
              pBuf[3]=0x01;
              pBuf[4]=calcXOR(pBuf+2,2);
              HalUARTWrite(HAL_UART_PORT_0, pBuf, 5);
              return;
          }
#endif          
          // 55 aa 66 77 Á¬½Ó
          if((pBuf[3]==0x55) && (pBuf[4]==0xaa)&& (pBuf[5]==0x66)&& (pBuf[6]==0x77))
          {
                pBuf[2]=0x05;
                pBuf[7]=logtype;
                pBuf[8]=calcXOR(pBuf+2,6);
                HalUARTWrite(HAL_UART_PORT_0, pBuf, 9);
                return;                    
          }
          // write 
          if(pBuf[3]==0x01)
          {
            Dtype=pBuf[9];
            serl=pBuf[7];
            serh=pBuf[8];
            zb_Writepandid(pBuf+4); 
            zb_Writechancel(*(pBuf+6));
            write_appconfig(pBuf+7);
            pBuf[2]=0x01;
              pBuf[3]=0x00;
              pBuf[4]=calcXOR(pBuf+2,2);
              
              HalUARTWrite(HAL_UART_PORT_0, pBuf, 5);
              osal_start_timerEx( sapi_TaskID, ZB_RST_EVENT,300 );
              
              // Reset the device with new configuration
             //   zb_SystemReset();
            return;
          }
          // read 
          if(pBuf[3]==0x02)
          {
            configread(pBuf+4);
            pBuf[2]=7;
            if((logtype==00) || (logtype==01))
                   pBuf[2]=4;
              pBuf[3]=0x00;
              pBuf[pBuf[2]+3]=calcXOR(pBuf+2,pBuf[2]+1);
              HalUARTWrite(HAL_UART_PORT_0, pBuf,pBuf[2]+4);
            return;
          }
     }
}
                           
void    configread(uint8 * pBuf)
{
    
    zb_Readpandid(pBuf);
    *(pBuf+2)=zb_Readchancel();
    read_appconfig(pBuf+3);
    Dtype=pBuf[5];
    if(Dtype==0xff)
        Dtype=0x24;
    serl=pBuf[3];
    serh=pBuf[4];
    if ((serl==0xff) && (serh==0xff) )
    {
          serl=0x34;
          serh=0x12;
    }
}


void test_read_flash (void )
{
   //HalUARTInit();
    //test_read_flash1();
    //test_read_flash2();
   //test_nv_item();
   //while(1);
    
}




