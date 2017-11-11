/*******************************************************************************
 * 文件名称：ZMain.c
 * 功    能：main函数入口。
 ******************************************************************************/


/* 包含头文件 */
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


/* 系统运行前最大VDD检测采样次数 */
#define MAX_VDD_SAMPLES  3
#define ZMAIN_VDD_LIMIT  HAL_ADC_VDD_LIMIT_4


/* 外部函数 */
/********************************************************************/
extern bool HalAdcCheckVdd (uint8 limit);


/* 本地函数 */
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
 * 函数名称：main 
 * 功    能：主函数。
 * 入口参数：无   
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
int main( void )
{
  /* 关闭中断 */
  osal_int_disable( INTS_ALL );

  /* 初始化系统时钟及LED等 */
  HAL_BOARD_INIT();

  /* 检测供电电压 */ 
 // zmain_vdd_check();

  /* 初始化堆栈 */
  zmain_ram_init();

  /* 初始化主板外围I/O */
  InitBoard( OB_COLD );

  /* 初始化硬件抽象层驱动 */
  HalDriverInit();

  /* 初始化系统NV非易失性存储 */
  osal_nv_init( NULL );

  /* 初始化基础NV项 */
  zgInit();

  /* 初始化MAC */ 
  ZMacInit();

  /* 确定扩展地址 */
  zmain_ext_addr();

  /* 初始化应用框架 */
#ifndef NONWK
  afInit(); // AF应用框架不是系统的任务，因此调用它的初始化程序
#endif

  /* 初始化操作系统 */
  osal_init_system();

  /* 允许中断 */
  osal_int_enable( INTS_ALL );

  /* 最终板级初始化 */
  InitBoard( OB_READY );

  /* 显示该设备信息 */
  zmain_dev_info();

  /* 如果定义了LCD，则在LCD上显示设备信息 */
#ifdef LCD_SUPPORTED
  zmain_lcd_init();
#endif

#ifdef WDT_IN_PM1
  /* 如果看门狗被使用，此处使能 */
  WatchDogEnable( WDTIMX );
#endif

  osal_start_system(); // 进入系统调度，无返回

  return ( 0 );
}


/*********************************************************************
 * 函数名称：zmain_vdd_check
 * 功    能：系统电压检测函数。检查VDD是否正常
 * 入口参数：无
 * 出口参数：如果VDD不正常，那么闪烁LED。
 * 返 回 值：无
 ********************************************************************/
#if 1
static void zmain_vdd_check( void )
{
  uint8 vdd_passed_count = 0;
  bool toggle = 0;

  /* 检测正常次数是否达到设定值 */
  while ( vdd_passed_count < MAX_VDD_SAMPLES )
  {
    /* 如果VDD电压检测通过 */
    if ( HalAdcCheckVdd (ZMAIN_VDD_LIMIT) )
    {
      vdd_passed_count++;    // 正常电压采样次数加1
      MicroWait (10000);     // 等待 10毫秒后重新采样
    }
    /* 如果VDD电压检测未通过 */
    else
    {
      vdd_passed_count = 0;  // 清零计数值
      MicroWait (50000);     // 等待50毫秒
      MicroWait (50000);     // 继续等待50毫秒后重新采样
    }

    /* 如果VDD电压检测未通过，则切换LED1 LED2亮灭状态 */
    if (vdd_passed_count == 0)
    {
      if ((toggle = !(toggle)))
        HAL_TOGGLE_LED1();
      else
        HAL_TOGGLE_LED2();
    }
  }

  /* 熄灭LED1 LED2 */
  HAL_TURN_OFF_LED1();
  HAL_TURN_OFF_LED2();
}
#endif

/*********************************************************************
 * 函数名称：zmain_ext_addr
 * 功    能：确定外部地址。有效扩展地址将执行优先级搜索并且将结果
 *           写入到系统NV非易失性存储器中。
 * 入口参数：无
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void zmain_ext_addr(void)
{
  uint8 nullAddr[Z_EXTADDR_LEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  uint8 writeNV = TRUE;

  /* 首先检查系统NV中是否存在扩展地址 */
  if ((SUCCESS != osal_nv_item_init(ZCD_NV_EXTADDR, Z_EXTADDR_LEN, NULL))  ||
      (SUCCESS != osal_nv_read(ZCD_NV_EXTADDR, 0, Z_EXTADDR_LEN, aExtendedAddress)) ||
      (osal_memcmp(aExtendedAddress, nullAddr, Z_EXTADDR_LEN)))
  {
    /* 读取扩展地址 */
    HalFlashRead(HAL_FLASH_IEEE_PAGE, HAL_FLASH_IEEE_OSET, aExtendedAddress, Z_EXTADDR_LEN);

    if (osal_memcmp(aExtendedAddress, nullAddr, Z_EXTADDR_LEN))
    {
      if (!osal_memcmp((uint8 *)(P_INFOPAGE+HAL_INFOP_IEEE_OSET), nullAddr, Z_EXTADDR_LEN))
      {
        osal_memcpy(aExtendedAddress, (uint8 *)(P_INFOPAGE+HAL_INFOP_IEEE_OSET), Z_EXTADDR_LEN);
      }
      else  // 未找到有效扩展地址
      {
        uint8 idx;
        
#if !defined ( NV_RESTORE )
        writeNV = FALSE;  // 将产生临时IEEE地址，不被写入NV
#endif

        /* 产生扩展地址随机数 */
        for (idx = 0; idx < (Z_EXTADDR_LEN - 2);)
        {
          uint16 randy = osal_rand();
          aExtendedAddress[idx++] = LO_UINT16(randy);
          aExtendedAddress[idx++] = HI_UINT16(randy);
        }
        
        /* ZigBee设备类型标识符 */
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

  /* 根据上述设置MAC扩展地址 */
  (void)ZMacSetReq(MAC_EXTENDED_ADDRESS, aExtendedAddress);
}


/*********************************************************************
 * 函数名称：zmain_dev_info
 * 功    能：在LCD上显示IEEE地址。
 * 入口参数：无
 * 出口参数：无
 * 返 回 值：无
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
  
  /* 显示扩展地址 */
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
 * 函数名称：zmain_ram_init
 * 功    能：初始化RAM堆栈。
 * 入口参数：无
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void zmain_ram_init( void )
{
  uint8 *end;
  uint8 *ptr;

  /* 初始化调用（参数）堆栈 */
  end = (uint8*)CSTK_BEG;
  ptr = (uint8*)(*( __idata uint16*)(CSTK_PTR));
  while ( --ptr > end )
    *ptr = STACK_INIT_VALUE;

  /* 初始化返回（地址）堆栈 */
  ptr = (uint8*)RSTK_END - 1;
  while ( --ptr > (uint8*)SP )
    *(__idata uint8*)ptr = STACK_INIT_VALUE;
}


#ifdef LCD_SUPPORTED
/*********************************************************************
 * 函数名称：zmain_lcd_init
 * 功    能：在启动过程中初始化LCD。
 * 入口参数：无
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void zmain_lcd_init ( void )
{
#ifdef SERIAL_DEBUG_SUPPORTED
  {
    HalLcdWriteString( "EMDOOR ELECTRONICS", HAL_LCD_LINE_1 );

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
