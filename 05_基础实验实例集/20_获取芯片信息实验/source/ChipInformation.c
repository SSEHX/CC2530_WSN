/*******************************************************************************
 * 文件名称：ChipInformation.c
 * 功    能：CC253x系列片上系统基础实验--- 获取芯片的信息
 * 描    述：获取CC253x系列片上系统的信息，在PC串口助手上显示。
 * 作    者：HJL
 * 日    期：2010-04-18
 ******************************************************************************/

/* 包含头文件 */
/********************************************************************/
#include "ioCC2530.h"    // CC2530的头文件，包含对CC2530的寄存器、中断向量等的定义
#include "stdio.h"       // C语言标准输入/输出库的头文件
/********************************************************************/

/* 定义枚举类型 */
/********************************************************************/
enum SYSCLK_SRC{XOSC_32MHz,RC_16MHz};      // 定义系统时钟源(主时钟源)枚举类型
/********************************************************************/

/*********************************************************************
* 函数名称：delay
 * 功    能：软件延时
 * 入口参数：无
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void delay(unsigned int time)
{ unsigned int i;
  unsigned char j;
  for(i = 0; i < time; i++)
  {  for(j = 0; j < 240; j++)
      {   asm("NOP");    // asm是内嵌汇编，nop是空操作,执行一个指令周期
          asm("NOP");
          asm("NOP");
       }  
   }  
}

/*********************************************************************
 * 函数名称：SystemClockSourceSelect
 * 功    能：选择系统时钟源(主时钟源)
 * 入口参数：source
 *             XOSC_32MHz  32MHz晶体振荡器
 *             RC_16MHz    16MHz RC振荡器
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void SystemClockSourceSelect(enum SYSCLK_SRC source)
{
  unsigned char clkconcmd,clkconsta;
   if(source == RC_16MHz)
  {             
    CLKCONCMD &= 0x80;
    CLKCONCMD |= 0x49;    //01001001   
  }
  else if(source == XOSC_32MHz)
  {
    CLKCONCMD &= 0x80;
  }  
  
  /* 等待所选择的系统时钟源(主时钟源)稳定 */
  clkconcmd = CLKCONCMD;             // 读取时钟控制寄存器CLKCONCMD
  do
  {
    clkconsta = CLKCONSTA;           // 读取时钟状态寄存器CLKCONSTA
  } while(clkconsta != clkconcmd);  // 直到选择的系统时钟源(主时钟源)已经稳定 
}



/*********************************************************************
 * 函数名称：InitUART0
 * 功    能：UART0初始化
 *           P0.2  RX                  
 *           P0.3  TX
 *           波特率：57600
 *           数据位：8
 *           停止位：1
 *           奇偶校验：无
 * 入口参数：无
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void initUART0(void)
{
  /* 片内外设引脚位置采用上电复位默认值，即PERCFG寄存器采用默认值 */
    PERCFG = 0x00;	//位置 1 P0 口     
    /* UART0相关引脚初始化 
       P0.2——RX，      P0.3——TX      P0.4——CT，      P0.5——RT   */     
    P0SEL = 0x3c;	//P0 用作串口, P0.2、P0.3、P0.4、P0.5作为片内外设I/O  
    
    /*波特率57600,8位数据位，1位停止位,无奇偶校验****/
    U0BAUD = 216; 
    U0GCR = 10;  //32M晶振
  
  /* USART模式选择 */
    U0CSR |= 0x80;  // UART模式
    U0UCR |= 0x80;  // 进行USART清除
   
   UTX0IF = 0;  // 清零UART0 TX中断标志
   EA = 1;   //使能全局中断
}


/*********************************************************************
 * 函数名称：UART0SendByte
 * 功    能：UART0发送一个字节
 * 入口参数：c
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void UART0SendByte(unsigned char c)
{
  U0DBUF = c;       // 将要发送的1字节数据写入U0DBUF(串口 0 收发缓冲器)  
  while (!UTX0IF);  // 等待TX中断标志，即U0DBUF就绪
  UTX0IF = 0;       // 清零TX中断标志 
}


/*********************************************************************
 * 函数名称：UART0SendString
 * 功    能：UART0发送一个字符串
 * 入口参数：str
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void UART0SendString( unsigned char *str)
{
  while(1)
  {
    if(*str == '\0') break;  // 遇到结束符，退出
    UART0SendByte(*str++);   // 发送一字节
  } 
}


/*********************************************************************
 * 函数名称：main
 * 功    能：main函数入口
 * 入口参数：无
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void main(void)
{
  char s[31];
  unsigned char i;
  char xdat[8];
  
  SystemClockSourceSelect(XOSC_32MHz);  // 选择32MHz晶体振荡器作为系统时钟源(主时钟源)
  initUART0(); //初始化端口
    
  /* 在串口助手上显示相关信息 */
  UART0SendString("CC253x 获取芯片的信息...\r\n\r\n"); // 获取芯片的信息
  delay(30000);
  
  /* 获取并显示芯片型号 */ 
  if(CHIPID == 0xA5)
  { 
    UART0SendString("类型:CC2530...\r\n\r\n");
  }
  else if(CHIPID == 0xB5)
  {
    UART0SendString("类型:CC2531...\r\n\r\n");
  }
  else
  {
    UART0SendString("类型:未知...\r\n\r\n");
  }
  delay(30000);
  
  /* 获取并显示芯片ID */
  sprintf(s,"芯片ID:0x%02X...\r\n\r\n",CHIPID);
  UART0SendString((unsigned char *)s);
  delay(30000);
  
  /* 获取并显示芯片版本 */
  sprintf(s,"芯片版本：Version:0x%02X...\r\n\r\n",CHVER);
  UART0SendString((unsigned char *)s);
  delay(30000);
  
  /* 获取并显示片内FLASH容量 */
  if(((CHIPINFO0 & 0x70) >> 4) == 1)
  {
    UART0SendString("FLASH容量:32 KB...\r\n\r\n");
  }
  else if(((CHIPINFO0 & 0x70) >> 4) == 2)
  {
    UART0SendString("FLASH容量:64 KB...\r\n\r\n");
  }
  else if(((CHIPINFO0 & 0x70) >> 4) == 3)
  {
    UART0SendString("FLASH容量:128 KB...\r\n\r\n");
  }
  else if(((CHIPINFO0 & 0x70) >> 4) == 4)
  { 
    UART0SendString("FLASH容量:256 KB...\r\n\r\n");
  }
  else
  {
    UART0SendString("FLASH容量:未知...\r\n\r\n");
  }
  delay(30000);
  
  /* 获取并显示片内SRAM容量 */
  sprintf(s,"SRAM容量:%2d KB...\r\n\r\n",(CHIPINFO1 & 0x07)+1);
  UART0SendString( (unsigned char *)s ); 
  delay(60000);
  
  /* 分屏显示，先清屏 */
   UART0SendString("CC253x 获取芯片的其他信息...\r\n\r\n");//获取芯片的信息
  delay(30000);
  
  /* 获取并显示片内是否有USB控制器 */
  if(CHIPINFO0 & 0x08)
  {
    UART0SendString("有USB控制器...\r\n\r\n");
  }
  else
  {
    UART0SendString("无USB控制器...\r\n\r\n");
  }
 delay(30000);

  
  /* 获取并显示芯片出厂时具有的来自TI所具有的IEEE地址范围的IEEE地址 */
    
  for(i=0;i<8;i++)
  {
    xdat[i] = *(unsigned char volatile __xdata *)(0x780C + i);
  }
  
  sprintf(s,"IEEE地址：%02X%02X%02X%02X%02X%02X%02X%02X",
            xdat[0],xdat[1],xdat[2],xdat[3], xdat[4],xdat[5],xdat[6],xdat[7]);
  
  UART0SendString((unsigned char *)s); 
  delay(30000);
  while(1);
}