/*******************************************************************************
 * ゅン嘿ZMain.c
 *     mainㄧ计
 *     POWER
 * ら    戳2010-03-08
 ******************************************************************************/


/* 繷ゅン */
/********************************************************************/
#include "ZComDef.h"
#include "OSAL.h"
#include "OSAL_Nv.h"
#include "OnBoard.h"
#include "ZMAC.h"

#ifndef NONWK
  #include "AF.h"
#endif

/* Hal */
#include "hal_lcd.h"
#include "hal_led.h"
#include "hal_adc.h"
#include "hal_drivers.h"
#include "hal_assert.h"
#include "hal_flash.h"
/********************************************************************/


/* ╰参笲︽玡程VDD浪代妓Ω计 */
#define MAX_VDD_SAMPLES  3
#define ZMAIN_VDD_LIMIT  HAL_ADC_VDD_LIMIT_4


/* 场ㄧ计 */
/********************************************************************/
extern bool HalAdcCheckVdd (uint8 limit);


/* セㄧ计 */
/********************************************************************/
static void zmain_dev_info( void );
static void zmain_ext_addr( void );
static void zmain_ram_init( void );
static void zmain_vdd_check( void );

#ifdef LCD_SUPPORTED
static void zmain_lcd_init( void );
#endif
/********************************************************************/



/*********************************************************************
 * ㄧ计嘿main
 *     ㄧ计
 * 把计礚
 * 把计礚
 *   礚
 ********************************************************************/
int main( void )
{
  /* 闽超い耞 */
  osal_int_disable( INTS_ALL );

  /* ﹍て╰参牧のLED单 */
  HAL_BOARD_INIT();

  /* 浪代ㄑ筿筿溃 */ 
  zmain_vdd_check();

  /* ﹍て帮刺 */
  zmain_ram_init();

  /* ﹍て狾瞅I/O */
  InitBoard( OB_COLD );

  /* ﹍て祑ン┾禜糷臱笆 */
  HalDriverInit();

  /* ﹍て╰参NV獶ア┦纗 */
  osal_nv_init( NULL );

  /* ﹍て膀娄NV兜 */
  zgInit();

  /* ﹍てMAC */ 
  ZMacInit();

  /* 絋﹚耎甶 */
  zmain_ext_addr();

  /* ﹍て莱ノ琜 */
#ifndef NONWK
  afInit(); // AF莱ノ琜ぃ琌╰参ヴ叭秸ノウ﹍て祘
#endif

  /* ﹍て巨╰参 */
  osal_init_system();

  /* す砛い耞 */
  osal_int_enable( INTS_ALL );

  /* 程沧狾﹍て */
  InitBoard( OB_READY );

  /* 陪ボ赣砞称獺 */
  zmain_dev_info();

  /* 狦﹚竡LCD玥LCD陪ボ砞称獺 */
#ifdef LCD_SUPPORTED
  zmain_lcd_init();
#endif

#ifdef WDT_IN_PM1
  /* 狦砆ㄏノ矪ㄏ */
  WatchDogEnable( WDTIMX );
#endif

  osal_start_system(); // 秈╰参秸礚

  return ( 0 );
}


/*********************************************************************
 * ㄧ计嘿zmain_vdd_check
 *     ╰参筿溃浪代ㄧ计浪琩VDD琌タ盽
 * 把计礚
 * 把计狦VDDぃタ盽ê或皗脅LED
 *   礚
 ********************************************************************/
static void zmain_vdd_check( void )
{
  uint8 vdd_passed_count = 0;
  bool toggle = 0;

  /* 浪代タ盽Ω计琌笷砞﹚ */
  while ( vdd_passed_count < MAX_VDD_SAMPLES )
  {
    /* 狦VDD筿溃浪代硄筁 */
    if ( HalAdcCheckVdd (ZMAIN_VDD_LIMIT) )
    {
      vdd_passed_count++;    // タ盽筿溃妓Ω计1
      MicroWait (10000);     // 单 10睝穝妓
    }
    /* 狦VDD筿溃浪代ゼ硄筁 */
    else
    {
      vdd_passed_count = 0;  // 睲箂璸计
      MicroWait (50000);     // 单50睝
      MicroWait (50000);     // 膥尿单50睝穝妓
    }

    /* 狦VDD筿溃浪代ゼ硄筁玥ち传LED1 LED2獹防篈 */
    if (vdd_passed_count == 0)
    {
      if ((toggle = !(toggle)))
        HAL_TOGGLE_LED1();
      else
        HAL_TOGGLE_LED2();
    }
  }

  /* 憾防LED1 LED2 */
  HAL_TURN_OFF_LED1();
  HAL_TURN_OFF_LED2();
}


/*********************************************************************
 * ㄧ计嘿zmain_ext_addr
 *     絋﹚场Τ耎甶盢磅︽纔穓盢挡狦
 *           糶╰参NV獶ア┦纗竟い
 * 把计礚
 * 把计礚
 *   礚
 ********************************************************************/
static void zmain_ext_addr(void)
{
  uint8 nullAddr[Z_EXTADDR_LEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  uint8 writeNV = TRUE;

  /* 浪琩╰参NVい琌耎甶 */
  if ((SUCCESS != osal_nv_item_init(ZCD_NV_EXTADDR, Z_EXTADDR_LEN, NULL))  ||
      (SUCCESS != osal_nv_read(ZCD_NV_EXTADDR, 0, Z_EXTADDR_LEN, aExtendedAddress)) ||
      (osal_memcmp(aExtendedAddress, nullAddr, Z_EXTADDR_LEN)))
  {
    /* 弄耎甶 */
    HalFlashRead(HAL_FLASH_IEEE_PAGE, HAL_FLASH_IEEE_OSET, aExtendedAddress, Z_EXTADDR_LEN);

    if (osal_memcmp(aExtendedAddress, nullAddr, Z_EXTADDR_LEN))
    {
      if (!osal_memcmp((uint8 *)(P_INFOPAGE+HAL_INFOP_IEEE_OSET), nullAddr, Z_EXTADDR_LEN))
      {
        osal_memcpy(aExtendedAddress, (uint8 *)(P_INFOPAGE+HAL_INFOP_IEEE_OSET), Z_EXTADDR_LEN);
      }
      else  // ゼтΤ耎甶
      {
        uint8 idx;
        
#if !defined ( NV_RESTORE )
        writeNV = FALSE;  // 盢玻ネ羬IEEEぃ砆糶NV
#endif

        /* 玻ネ耎甶繦诀计 */
        for (idx = 0; idx < (Z_EXTADDR_LEN - 2);)
        {
          uint16 randy = osal_rand();
          aExtendedAddress[idx++] = LO_UINT16(randy);
          aExtendedAddress[idx++] = HI_UINT16(randy);
        }
        
        /* ZigBee砞称摸夹醚才 */
#if defined ZDO_COORDINATOR
        aExtendedAddress[idx++] = 0x10;
#elif defined RTR_NWK
        aExtendedAddress[idx++] = 0x20;
#else
        aExtendedAddress[idx++] = 0x30;
#endif
        aExtendedAddress[idx] = 0xF8;
      }
    }

    if ( writeNV )
    {
      (void)osal_nv_write(ZCD_NV_EXTADDR, 0, Z_EXTADDR_LEN, aExtendedAddress);
    }
  }

  /* 沮瓃砞竚MAC耎甶 */
  (void)ZMacSetReq(MAC_EXTENDED_ADDRESS, aExtendedAddress);
}


/*********************************************************************
 * ㄧ计嘿zmain_dev_info
 *     LCD陪ボIEEE
 * 把计礚
 * 把计礚
 *   礚
 ********************************************************************/
static void zmain_dev_info(void)
{
#ifdef LCD_SUPPORTED
  uint8 i;
  uint8 *xad;
  uint8 lcd_buf[Z_EXTADDR_LEN*2+1+5];
  uint8 ieeeAddr[]="IEEE:";
  
  for(i = 0; i < 5; i++)
  {
    lcd_buf[i] = ieeeAddr[i];
  }
  
  /* 陪ボ耎甶 */
  xad = aExtendedAddress + Z_EXTADDR_LEN - 1;

  for (i = 5; i < Z_EXTADDR_LEN*2 + 5; xad--)
  {
    uint8 ch;
    ch = (*xad >> 4) & 0x0F;
    lcd_buf[i++] = ch + (( ch < 10 ) ? '0' : '7');
    ch = *xad & 0x0F;
    lcd_buf[i++] = ch + (( ch < 10 ) ? '0' : '7');
  }
  lcd_buf[Z_EXTADDR_LEN*2+5] = '\0';

  HalLcdWriteString( (char*)lcd_buf, HAL_LCD_LINE_3 );
#endif
}


/*********************************************************************
 * ㄧ计嘿zmain_ram_init
 *     ﹍てRAM帮刺
 * 把计礚
 * 把计礚
 *   礚
 ********************************************************************/
static void zmain_ram_init( void )
{
  uint8 *end;
  uint8 *ptr;

  /* ﹍て秸ノ把计帮刺 */
  end = (uint8*)CSTK_BEG;
  ptr = (uint8*)(*( __idata uint16*)(CSTK_PTR));
  while ( --ptr > end )
    *ptr = STACK_INIT_VALUE;

  /* ﹍て帮刺 */
  ptr = (uint8*)RSTK_END - 1;
  while ( --ptr > (uint8*)SP )
    *(__idata uint8*)ptr = STACK_INIT_VALUE;
}


#ifdef LCD_SUPPORTED
/*********************************************************************
 * ㄧ计嘿zmain_lcd_init
 *     币笆筁祘い﹍てLCD
 * 把计礚
 * 把计礚
 *   礚
 ********************************************************************/
static void zmain_lcd_init ( void )
{
#ifdef SERIAL_DEBUG_SUPPORTED
  {
    HalLcdWriteString( "SIKAIELECTRONICS", HAL_LCD_LINE_1 );

#if defined( MT_MAC_FUNC )
#if defined( ZDO_COORDINATOR )
      HalLcdWriteString( "MAC-MT Coord", HAL_LCD_LINE_5 );
#else
      HalLcdWriteString( "MAC-MT Device", HAL_LCD_LINE_5 );
#endif
#elif defined( MT_NWK_FUNC )
#if defined( ZDO_COORDINATOR )
      HalLcdWriteString( "NWK Coordinator", HAL_LCD_LINE_5 );
#else
      HalLcdWriteString( "NWK Device", HAL_LCD_LINE_5 );
#endif
#endif
  }
#endif 
}
#endif
