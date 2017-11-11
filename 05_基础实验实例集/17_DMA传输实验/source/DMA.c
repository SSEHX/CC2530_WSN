/*******************************************************************************
 * 文件名称：ADC(Single Conversion)_Ex.c
 * 功    能：CC253x系列片上系统基础实验--- DMA控制器(块传输模式  触发源编号0)
 * 描    述：用CC253x片内DMA控制器将一字符串从源地址转移到目标地址，采用块传输模
 *           式，传输长度为该字符串的长度，源地址和目标地址的增量都设为1，在PC串
             口助手显示相应信息。
 * 作    者：HJL
 * 日    期：2012-9-11
 ******************************************************************************/


/* 包含头文件 */
/********************************************************************/
#include "ioCC2530.h"    // CC2530的头文件，包含对CC2530的寄存器、中断向量等的定义
#include "stdio.h"       // C语言标准输入/输出库的头文件
#include "String.h"      // C语言字符串库的头文件
/********************************************************************/
#define LED1 P1_0     
#define SW1 P1_2      // P1_2定义为P1.2


/* 定义DMA配置参数数据结构类型 */
/********************************************************************/
#pragma bitfields=reversed    // 采用大端格式数字的意思是用于几位的意思
                              //通知编译器完成特定的操作
typedef struct 
{
  unsigned char SRCADDRH;          // 源地址高字节
  unsigned char SRCADDRL;          // 源地址低字节
  unsigned char DESTADDRH;         // 目标地址高字节
  unsigned char DESTADDRL;         // 目标地址低字节
  unsigned char VLEN      : 3;     // 可变长度传输模式选择
  unsigned char LENH      : 5;     // 传输长度高字节
  unsigned char LENL      : 8;     // 传输长度低字节
  unsigned char WORDSIZE  : 1;     // 字节/字传输
  unsigned char TMODE     : 2;     // 传输模式选择
  unsigned char TRIG      : 5;     // 触发事件选择
  unsigned char SRCINC    : 2;     // 源地址增量
  unsigned char DESTINC   : 2;     // 目标地址增量
  unsigned char IRQMASK   : 1;     // 中断使能
  unsigned char M8        : 1;     // 7/8 bits 字节(只用於字节传输模式)
  unsigned char PRIORITY  : 2;     // 优先级
} DMA_CONFIGURATIONPARAMETERS;
#pragma bitfields=default  // 恢复为默认的小端格式
/********************************************************************/


/* 定义枚举类型 */
/********************************************************************/
enum SYSCLK_SRC{XOSC_32MHz,RC_16MHz};      // 定义系统时钟源(主时钟源)枚举类型
/*********************************************************************

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
 * 函数名称：init
 * 功    能：初始化系统IO
 * 入口参数：无
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void initIO(void)
{   P1SEL &= ~0x05;     // 设置LED1、SW1为普通IO口
    P1DIR |= 0x001 ;    // 设置LED1在P1.0为输出    
    P1DIR &= ~0X04;	//Sw1按键在 P1.2,设定为输入    
    LED1= 0;            // LED灭
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
     
  /* P0口外设优先级采用上电复位默认值，即P2DIR寄存器采用默认值 */
  /* 第一优先级：USART0
     第二优先级：USART1
     第三优先级：Timer1   */
    
  /* UART0波特率设置 */
  /* 波特率：57600   
     当使用32MHz 晶体振荡器作为系统时钟时，要获得57600波特率需要如下设置：
         UxBAUD.BAUD_M = 216
         UxGCR.BAUD_E = 10
         该设置误差为0.03%
   */
  U0BAUD = 216;
  U0GCR = 10;
  
  /* USART模式选择 */
  U0CSR |= 0x80;  // UART模式
  
  /* UART0配置 ，以下配置参数采用上电复位默认值：
         硬件流控：无
         奇偶校验位(第9位)：奇校验
         第9位数据使能：否
         奇偶校验使能：否
         停止位：1个
         停止位电平：高电平
         起始位电平：低电平
   */
   U0UCR |= 0x80;  // 进行USART清除
   
  /* 用於发送的位顺序采用上电复位默认值，即U0GCR寄存器采用上电复位默认值 */
  /* LSB先发送 */
  
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
void UART0SendString(unsigned char *str)
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
  SystemClockSourceSelect(XOSC_32MHz);  // 选择32MHz晶体振荡器作为系统时钟源(主时钟源)
  initIO();  // IO端口初始化
  initUART0();  //初始化端口 
   /* 定义DMA源地址空间并初始化为将被DMA传输的字符串数据 */
  unsigned char srcStr[]="块传送数据：DMA transfer.";  
  /* 定义DMA目标地址空间 */
  unsigned char destStr[sizeof(srcStr)];
  
 /* 定义DMA配置数据结构体变量 */
  DMA_CONFIGURATIONPARAMETERS dmaCH;
  
  /* 设置DMA配置参数 */
  /* 源地址 */
  dmaCH.SRCADDRH = (unsigned char)(((unsigned short)(&srcStr)) >> 8);
  dmaCH.SRCADDRL = (unsigned char)(((unsigned short)(&srcStr)) & 0x00FF);
  /* 目标地址 */
  dmaCH.DESTADDRH = (unsigned char)(((unsigned short)(&destStr)) >> 8);
  dmaCH.DESTADDRL = (unsigned char)(((unsigned short)(&destStr)) & 0x00FF);  
  /* 可变长度模式选择 */
  dmaCH.VLEN = 0x00;      // 使用DMA传输长度
  /* 传输长度 */
  dmaCH.LENH = (unsigned char)(((unsigned short)(sizeof(srcStr))) >> 8);
  dmaCH.LENL = (unsigned char)(((unsigned short)(sizeof(srcStr))) & 0x00FF);  
  /* 字节/字模式选择 */
  dmaCH.WORDSIZE = 0x00;  // 字节传输
  /* 传输模式选择 */
  dmaCH.TMODE = 0x01;     // 块传输模式  
  /* 触发源选择 */
  dmaCH.TRIG = 0x00;      // 通过写DMAREQ来触发
  /* 源地址增量 */
  dmaCH.SRCINC = 0x01;    // 每次传输完成后源地址加1字节/字 
  /* 目标地址增量 */
  dmaCH.DESTINC = 0x01;   // 每次传输完成后目标地址加1字节/字  
  /* 中断使能 */
  dmaCH.IRQMASK = 0x00;   // 禁止DMA产生中断
  /* M8 */
  dmaCH.M8 = 0x00;        // 1个字节为8比特  
  /* 优先级设置 */
  dmaCH.PRIORITY = 0x02;  // 高级

  /* 使用DMA通道0 */
  DMA0CFGH = (unsigned char)(((unsigned short)(&dmaCH)) >> 8);
  DMA0CFGL = (unsigned char)(((unsigned short)(&dmaCH)) & 0x00FF); 
   while(1)
  {
    /***************给srcStr赋值**************/
    sprintf(srcStr,"块传送数据：DMA transfer.");  
    
    /* 在PC串口助手上显示DMA传输的源地址和目标地址 */
    unsigned char s[31];
    sprintf(s,"数据源地址: 0x%04X， ",(unsigned short)(&srcStr));  //格式化字符串   
    UART0SendString(s);   
   
    sprintf(s,"目标地址: 0x%04X；",(unsigned short)(&destStr));
    UART0SendString(s);
    memset(destStr, 0, sizeof(destStr));  // 清除DMA目标地址空间的内容
   
    UART0SendString("按SW1开始DMA传送...\r\n\r\n");  // 从UART0发送字符串
       
    /* 使DMA通道0进入工作状态 */
    DMAARM = 0x01;  
 
   /* 清除DMA中断标志 */
    DMAIRQ = 0x00; 
    
    LED1=0;
    
    /* 等待用户按下任意键(除复位键外)*/
    while(SW1 == 1);
    
    if(SW1 == 0)	//低电平有效
        {     while(SW1 == 0); //等待用户松开按键              
               /* 触发DMA传输 */
               DMAREQ = 0x01;
  
               /* 等待DMA传输完成 */
               while((DMAIRQ & 0x01) == 0);
  
              /* 验证DMA传输数据的正确性 */
              unsigned char i,errors = 0;
              for(i=0;i<sizeof(srcStr);i++)
                {
                    if(srcStr[i] != destStr[i]) errors++;     
                }
  
              /* 在PC串口助手上显示DMA传输结果 */
              if(errors)
               {
                  LED1 = 0;
                  UART0SendString("传输错误！\r\n\r\n\r\n");  // 从UART0发送字符串
               }
              else
                {
                  LED1 =1;
                  sprintf(s,"%s  传输成功!\r\n\r\n\r\n",destStr);  //格式化字符串
                  UART0SendString(s);  // 从UART0发送字符串
                }           
           } //if
      } //while
}