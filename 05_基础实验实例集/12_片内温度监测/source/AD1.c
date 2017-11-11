/*******************************************************************************
 * 文件名称：AD1.c
 * 功    能：CC253x系列片上系统基础实验--- AD(片内温度)
 * 描    述：本实验使用CC253x系列片上系统的片内温度传感器作为AD源，采用单端转
             换模式，将相应的ADC转换后的片内温度值显示在PC的串口助手上。
******************************************************************************/
/* 包含头文件 */
/********************************************************************/
#include "ioCC2530.h"    // CC2530的头文件，包含对CC2530的寄存器、中断向量等的定义
#include "stdio.h"       // C语言标准输入/输出库的头文件
//#include "temp.h"

/********************************************************************/

//定义led灯端口
#define LED1 P1_0     // P1_0定义为P1.0
#define uint unsigned int 
#define uchar unsigned char

/******定义枚举类型 *******************************************************/
enum SYSCLK_SRC{XOSC_32MHz,RC_16MHz};  // 定义系统时钟源(主时钟源)枚举类型

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
     
  /* P0口外设优先级采用上电复位默认值，即P2DIR寄存器采用默认值 */
  /* 第一优先级：USART0
     第二优先级：USART1
     第三优先级：Timer1   */
    
  /* UART0波特率设置 */
  /* 波特率：57600   
     当使用16MHz 晶体振荡器作为系统时钟时，要获得57600波特率需要如下设置：
         UxBAUD.BAUD_M = 216
         UxGCR.BAUD_E = 11
         该设置误差为0.03%
   */
  U0BAUD = 216;
  U0GCR = 11;
  
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
 * 入口参数： *str
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
 * 函数名称：getTemperature
 * 功    能：进行AD 转换，将得到的结果求均值后将 AD 结果转换为温度返回
 * 入口参数：无
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
float getTemperature(void)
{
   signed short int value;
   ADCCON3 = (0x3E);       //选择内部参考电压；12位分辨率；对片内温度传感器采样
   ADCCON1 |= 0x30;        //选择ADC的启动模式为手动
   ADCCON1 |= 0x40;        //启动AD转化
   while(!(ADCCON1 & 0x80));    //等待ADC转化结束
   value = ADCL >> 2;
   value |= ((int)ADCH << 6);     //8位转为16为，后补6个0，取得最终转化结果，存入value中
     /* 若adcvalue<0，就认为它为0 */
    if(value < 0) value = 0;  
    return value*0.06229-311.43; //根据公式计算出温度值
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
   P1DIR |= 0x01;   /* 配置P1.0的方向为输出 */
   SystemClockSourceSelect(RC_16MHz);
   initUART0();  // UART0初始化  
  
  /*********以下代码采集片内温度值并处理**********/ 
   char i;
   float avgTemp;
   unsigned char output[]="";
   UART0SendString("测试 CC2530片内温度传感器!\r\n");
    while(1)
    {
       LED1 =1;  //LED亮，开始采集并发往串口
       avgTemp = getTemperature();
       for(i = 0 ; i < 64 ; i++)    //连续采样64次；
        {
           avgTemp += getTemperature();
           avgTemp = avgTemp/2;  //每采样1次，取1次平均值
       }
       output[0] = (unsigned char)(avgTemp)/10 + 48;  //十位
       output[1] = (unsigned char)(avgTemp)%10 + 48;  //个位
       output[2] = '.';   //小数点
       output[3] = (unsigned char)(avgTemp*10)%10+48;  //十分位
       output[4] = (unsigned char)(avgTemp*100)%10+48;  //百分位
       output[5] = '\0';  //字符串结束符
       
       UART0SendString(output);
       UART0SendString("℃\n");
       LED1 = 0;       //LED熄灭，表示转换结束       
       delay(40000);
       delay(40000);
    }
}