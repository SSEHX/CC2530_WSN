/*******************************************************************************
 * 文件名称：Flash_Ex.c
 * 功    能：CC253x系列片上系统基础实验--- Flash读写实验
 * 描    述：向CC253x片内FLASH BANK7的前8个字节写入8字节数据。写入之前，先进行相
 *           应的FLASH页(112页)擦除，然后通过DMA FLASH写操作进行数据的写入。
  *          CC253x系列片上系统芯片必须为CC2530F256或CC2531
 * 注    意：由於CC253x系列片上系统的片内FLASH的擦除/写入次数是有限的，大约20000
 *           次，因此不建议多次进行本实验。
 * 作    者：HJL
 * 日    期：2012-9-11
 ******************************************************************************/

/* 包含头文件 */
/********************************************************************/
#include "ioCC2530.h"    // CC2530的头文件，包含对CC2530的寄存器、中断向量等的定义
#include "stdio.h"       // C语言标准输入/输出库的头文件
#include "String.h"      // C语言字符串库的头文件
/********************************************************************/
//定义端口：p1.2
#define SW1  P1_2     // P1_2定义为SW1

/* 定义枚举类型 */
/********************************************************************/
enum SYSCLK_SRC{XOSC_32MHz,RC_16MHz};      // 定义系统时钟源(主时钟源)枚举类型
/********************************************************************/


/* 定义DMA配置参数数据结构类型 */
/********************************************************************/
#pragma bitfields=reversed  // 采用大端格式
typedef struct 
{
  unsigned char srcAddrH;
  unsigned char srcAddrL;
  unsigned char dstAddrH;
  unsigned char dstAddrL;
  unsigned char xferLenV;
  unsigned char xferLenL;
  unsigned char ctrlA;
  unsigned char ctrlB;
} flashdrvDmaDesc_t;
#pragma bitfields=default  // 恢复为默认的小端格式
/********************************************************************/
/* 定义DMA配置数据结构体变量 */
flashdrvDmaDesc_t flashdrvDmaDesc;

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
 * 函数名称：initIO
 * 功    能：初始化系统IO
 * 入口参数：无
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void initIO()
{    P1SEL &= ~0x04;     // 设置LED1、SW1为普通IO口
     P1DIR &= ~0X04;	//Sw1按键在 P1.2,设定为输入   
}


/********************************************************************************
 * 函数名称:    FLASHDRV_Write
 * 功    能：   写flash 
 * 入口参数：   unsigned short addr,unsigned char *buf,unsigned short cnt  
 *  		     addr - flash字地址: 字节地址/4，如addr=0x1000则字节地址为0x4000
 *  		     buf - 数据缓冲区.
 *   		     cnt - 字数， 实际写入cnt*4字节
 * 出口参数：		无
 * 返 回 值：		无
/****************************************************/
void FLASHDRV_Write(unsigned short addr,
                    unsigned char *buf,
                    unsigned short cnt)
{
  flashdrvDmaDesc.srcAddrH = (unsigned char) ((unsigned short)buf >> 8);
  flashdrvDmaDesc.srcAddrL = (unsigned char) (unsigned short) buf;
  flashdrvDmaDesc.dstAddrH = (unsigned char) ((unsigned short)&FWDATA >> 8);
  flashdrvDmaDesc.dstAddrL = (unsigned char) (unsigned short) &FWDATA;
  flashdrvDmaDesc.xferLenV =
    (0x00 << 5) |               // use length
    (unsigned char)(unsigned short)(cnt >> 6);  // length (12:8). Note that cnt is flash word
  flashdrvDmaDesc.xferLenL = (unsigned char)(unsigned short)(cnt * 4);
  flashdrvDmaDesc.ctrlA =
    (0x00 << 7) | // word size is byte
    (0x00 << 5) | // single byte/word trigger mode
    18;           // trigger source is flash
  flashdrvDmaDesc.ctrlB =
    (0x01 << 6) | // 1 byte/word increment on source address
    (0x00 << 4) | // zero byte/word increment on destination address
    (0x00 << 3) | // The DMA is to be polled and shall not issue an IRQ upon completion.
    (0x00 << 2) | // use all 8 bits for transfer count
    0x02; // DMA priority high

  DMAIRQ &= ~( 1 << 0 ); // clear IRQ
  DMAARM = (0x01 << 0 ); // arm DMA

  FADDRL = (unsigned char)addr;
  FADDRH = (unsigned char)(addr >> 8);
  FCTL |= 0x02;         // Trigger the DMA writes.
  while (FCTL & 0x80);  // Wait until writing is done.
}
void FLASHDRV_Init(void)
{
  DMA0CFGH = (unsigned char) ((unsigned short) &flashdrvDmaDesc >> 8);
  DMA0CFGL = (unsigned char) (unsigned short) &flashdrvDmaDesc;
  DMAIE = 1;
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
  /* 定义要写入FLASH的8字节数据 */
  /* 采用DMA FLASH写操作，因此该数组的首地址作为DMA通道的源地址 */
  unsigned char FlashData[8] = {0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88};
  unsigned char i;
  char s[31];
  char xdat[8];
   SystemClockSourceSelect(XOSC_32MHz);  // 选择32MHz晶体振荡器作为系统时钟源(主时钟源)
 
  initIO();  // IO端口初始化
  initUART0();  //初始化端口 
  FLASHDRV_Init();
  while(1)
  {    
   /* 将FLASH BANK7 映射到XDATA存储器空间的XBANK区域，即XDATA存储器空间0x8000-0xFFFF */
    MEMCTR |= 0x07;
   
   /* 在串口助手上显示相关信息 */
    UART0SendString("XDATA存储器前8个地址中的内容：\r\n");
 
  /* 显示XDATA存储器空间0x8000 - 0x8007这8个地址中的内容 */
  /* 由於于之前已经将FLASH BANK7映射到了XDATA存储器空间的XBANK区域(0x8000-0xFFFF), 
     因此实际上显示的是FLASH BANK7中前8个字节存储的内容。 */
    for(i=0;i<8;i++)
    {
      xdat[i] = *(unsigned char volatile __xdata *)(0x8000 + i);
    }
  
    sprintf(s,"原先：%02X%02X%02X%02X%02X%02X%02X%02X \r\n\r\n",
              xdat[0],xdat[1],xdat[2],xdat[3],xdat[4],xdat[5],xdat[6],xdat[7]);
    UART0SendString(s);
    UART0SendString("按SW1键擦除...... \r\n");
    /* 等待用户按下SW1键*/
    while(SW1 == 1);
    delay(100);
    while(SW1 == 0);  //等待用户松开按键
  /* 擦除一个FLASH页 */
  /* 此处擦除FLASH BANK7中的第一个页，即112页, P9=112<<1 */
    EA = 0;                // 禁止中断
    while (FCTL & 0x80);   // 查询FCTL.BUSY并等待FLASH控制器就绪
    FADDRH = 112 << 1;     // 设置FADDRH[7:1]以选择所需要擦除的页
    FCTL |= 0x01;          // 设置FCTL.ERASE为1以启动一个页擦除操作
    asm("NOP");
    while (FCTL & 0x80);   // 等待页擦除操作完成(大约20ms)
    EA = 1;                // 使能中断
  /* 显示XDATA存储器空间0x8000 - 0x8007这8个地址中的内容 */
    for(i=0;i<8;i++)
     {
        xdat[i] = *(unsigned char volatile __xdata *)(0x8000 + i);
     }
    sprintf(s,"擦除后：%02X%02X%02X%02X%02X%02X%02X%02X \r\n\r\n",
              xdat[0],xdat[1],xdat[2],xdat[3],xdat[4],xdat[5],xdat[6],xdat[7]);
    UART0SendString(s);
  
    UART0SendString("按SW1键开始写入'1122334455667788'...... \r\n");
   
     /* 等待用户按下SW1键*/
    while(SW1 == 1);
    delay(100);
    while(SW1 == 0);  //等待用户松开按键 
    //写操作 地址112*512(实际地址为112*2048），数据FlashData，字节2*4
    FLASHDRV_Write( (unsigned int)112*512, FlashData, 2);
 
   /* 显示XDATA存储器空间0x8000 - 0x8007这8个地址中的内容 */
   for(i=0;i<8;i++)
   {
      xdat[i] = *(unsigned char volatile __xdata *)(0x8000 + i);
   }
  
    sprintf(s,"写入后：%02X%02X%02X%02X%02X%02X%02X%02X \r\n\r\n",
            xdat[0],xdat[1],xdat[2],xdat[3],xdat[4],xdat[5],xdat[6],xdat[7]);
    UART0SendString(s);
     /* 等待用户按下SW1键*/
    UART0SendString("按SW1键重新开始...\r\n");
    while(SW1 == 1);
    delay(100);
    while(SW1 == 0);  //等待用户松开按键 
  }
}