/*******************************************************************************
 * 文件名称：hal_rfid.c
 * 功    能：RFID芯片TRF7960驱动
 * 硬件连接：TRF7960与CC253x的硬件连接关系如下：
 *
 *          TRF7960                 CC253x
 *           IRQ                     P0.1
 *           CLK                     P1.0
 *           MOSI                    P0.7
 *           MISO                    P1.4
 *           SS                      P0.0
 ******************************************************************************/


/* 包含头文件 */
/********************************************************************/
#include "hal_mcu.h"
#include "hal_defs.h"
#include "hal_types.h"
#include "hal_drivers.h"
#include "hal_rfid.h"
#include "osal.h"
#include "hal_board.h"
/********************************************************************/


#ifdef RFID_SENSOR

/* 宏定义 */
/*********************************************************************/
#define BUF_LENGTH                      100 // 最大帧字节数量


/* 本地变量 */
/*********************************************************************/
unsigned char rfid_buf[BUF_LENGTH];  // 定义MSP430微控制器与TRF7961通信接收缓冲区
signed   char RXTXstate;
unsigned char RXErrorFlag; // 定义接收错误标志
unsigned char rfid_flags;  // 定义存储标志位(在仿冲撞中使用)
unsigned char i_reg;       // 中断寄存器变量
unsigned char CollPoss;    // 定义冲撞发生位置变量
unsigned int mask = 0x80;
unsigned char UIDString[9];
bool uidfound;
/*********************************************************************/


/* 本地变量 */
/*********************************************************************/
void halMcuWaitUs(uint16 usec);
void SPIStartCondition(void);
void SPIStopCondition(void);
void WriteCont(unsigned char *pbuf, unsigned char lenght);
void DirectCommand(unsigned char *pbuf);
void RAWwrite(unsigned char *pbuf, unsigned char lenght);
void InitialSettings(void);
void ReInitialize15693Settings(void);
void InterruptHandlerReader(unsigned char *Register);
HAL_ISR_FUNCTION( halRFIDPort0Isr, P0INT_VECTOR );
void TRF7960_IO_Init_SPI(void);
void EnableSlotCounter(void);
void DisableSlotCounter(void);
void Check_ireg(void);
void InventoryRequest(unsigned char *mask, unsigned char lenght);
unsigned char RequestCommand(unsigned char *pbuf, unsigned char lenght, unsigned char brokenBits, char noCRC);
/*********************************************************************/


/*******************************************************************************
 * 函数名称： halMcuWaitUs
 * 功    能：忙等待功能。等待指定的微秒数。不同的指令所需的时钟周期数
 *           量不同。一个周期的持续时间取决于MCLK。
 * 入口参数：usec  延时时长，单位Wie微秒
 * 出口参数：无
 * 返 回 值：无
 * 注    意：此功能高度依赖于MCU架构和编译器。
 ******************************************************************************/
#pragma optimize=none
void halMcuWaitUs(uint16 usec)
{
  usec>>= 1;
  while(usec--)
  {
    asm("NOP");
    asm("NOP");
    asm("NOP");
    asm("NOP");
    asm("NOP");
    asm("NOP");
    asm("NOP");
    asm("NOP");
    asm("NOP");
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


/*******************************************************************************
 * 函数名称： halMcuWaitMs
 * 功    能：忙等待功能。等待指定的毫秒数。
 * 入口参数：msec  延时时长，单位为毫秒
 * 出口参数：无
 * 返 回 值：无
 * 注    意：此功能高度依赖于MCU架构和编译器。
 ******************************************************************************/
#pragma optimize=none
void halMcuWaitMs(uint16 msec)
{
  while(msec--)
    halMcuWaitUs(1000);
}


/*******************************************************************************
* 函数名称：SPIStartCondition()
* 功    能：串行(SPI)模式开始通信条件
* 入口参数：无
* 出口参数：无
* 说    明：MSP430微控制器与TRF7961使用串行(SPI)模式通信，开始启动命令
*******************************************************************************/
void SPIStartCondition(void)
{
  CLKGPIOset();  // 设置CLK为普通GPIO
  CLKPOUTset();  // 设置CLK方向为输出
  CLKON();       // CLK时钟开启（高）
}


/*******************************************************************************
* 函数名称：SPIStopCondition()
* 功    能：串行(SPI)模式停止通信条件
* 入口参数：无
* 出口参数：无
* 说    明：MSP430微控制器与TRF7961使用串行(SPI)模式通信，停止命令
*******************************************************************************/
void SPIStopCondition(void)
{
  CLKGPIOset(); // 设置CLK为普通GPIO
  CLKOFF();     // CLK时钟关闭（低）
  CLKPOUTset(); // 设置CLK方向为输出
  CLKOFF();     // CLK时钟关闭（低）
}


/*******************************************************************************
* 函数名称：WriteSingle()
* 功    能：写单个寄存器或者特殊地址的多个寄存器命令
* 入口参数：*pbuf            将要写入的数据           
*           lenght           写入数据的长度 
* 出口参数：无
* 说    明：写命令。
*******************************************************************************/
void WriteSingle(unsigned char *pbuf, unsigned char lenght)
{
  unsigned char i,j;

  /* IO口模拟SPI模式 */
  L_SlaveSelect(); // SS管脚输出低，SPI启动
  CLKOFF();        // CLK时钟关闭（低）
        
  while(lenght > 0)
  {
    *pbuf = (0x1f & *pbuf); // 取低5位B0-B4 寄存器地址数据 格式为000XXXXX
            
    for(i = 0; i < 2; i++)  // 以单个寄存器，先发送地址，再发送数据或命令
    {
      for(j = 0; j < 8; j++)
      {
        if (*pbuf & mask) // 设置数据位
          SIMOON();
        else
          SIMOOFF();
                   
        CLKON(); // 关联CLK时钟信息，发送数据
        CLKOFF();
        mask >>= 1;  // 标志位右移
      }
                
      mask = 0x80;
      pbuf++;
      lenght--;
    }
  }

  H_SlaveSelect(); // SS管脚输出高，SPI停止
}


/*******************************************************************************
* 函数名称：WriteCont()
* 功    能：连续写寄存器或者特殊地址的多个寄存器命令
* 入口参数：*pbuf            将要写入的数据           
*           lenght           写入数据的长度 
* 出口参数：无
* 说    明：连续地址写命令。
*******************************************************************************/
void WriteCont(unsigned char *pbuf, unsigned char lenght)
{
  unsigned char j;

  /* IO口模拟SPI模式 */
  L_SlaveSelect(); // SS管脚输出低，SPI启动
  CLKOFF(); // CLK时钟关闭（低）

  *pbuf = (0x20 | *pbuf); // 取位B5 寄存器地址数据 连续标志位 格式为001XXXXX
  *pbuf = (0x3f &*pbuf);  // 取低6位B0-B5 寄存器地址数据
  
  while(lenght > 0)
  {
    for(j=0;j<8;j++)
    {
      if (*pbuf & mask) // 设置数据位
        SIMOON();
      else
        SIMOOFF();

      CLKON(); // 关联CLK时钟信息，发送数据
      CLKOFF();
      mask >>= 1; // 标志位右移
    }

    mask = 0x80;                            
    pbuf++;
    lenght--;
  }

  H_SlaveSelect(); // SS管脚输出高，SPI停止
}


/*******************************************************************************
* 函数名称：ReadSingle()
* 功    能：读单个寄存器
* 入口参数：*pbuf            将要读取的数据           
*           lenght           读取数据的长度 
* 出口参数：无
* 说    明：无
*******************************************************************************/
void ReadSingle(unsigned char *pbuf, unsigned char lenght)
{ 
  unsigned char j;

  /* IO口模拟SPI模式 */
  L_SlaveSelect(); // SS管脚输出低，SPI启动

  while(lenght > 0)
  {
    *pbuf = (0x40 | *pbuf); // 取位B6 寄存器地址数据 单个标志位 格式为01XXXXXX
    *pbuf = (0x5f &*pbuf);  // 取低7位B0-B6 寄存器地址数据
    
    for(j = 0; j < 8; j++)
    {
      if (*pbuf & mask)  // 设置数据位
        SIMOON();
      else
        SIMOOFF();
                
      CLKON();  // 关联CLK时钟信息，发送数据
      CLKOFF();
      mask >>= 1;  // 标志位右移
    } 
    
    mask = 0x80;
    *pbuf = 0;     // 开始读取处理
            
    for(j = 0; j < 8; j++)
    {
      *pbuf <<= 1; // 数据左移
      CLKON();     // 关联CLK时钟信息，发送数据
      CLKOFF();

      if (SOMISIGNAL) // 判断SOMI引脚
        *pbuf |= 1;  // 若为高电平，则将数据或上 1
    } 

    pbuf++;
    lenght--;
  }

  H_SlaveSelect(); // SS管脚输出高，SPI停止
} 


/*******************************************************************************
* 函数名称：ReadCont()
* 功    能：连续读寄存器或者特殊地址的多个寄存器命令
* 入口参数：*pbuf            将要读取的数据           
*           lenght           读取数据的长度 
* 出口参数：无
* 说    明：连续地址写命令。
*******************************************************************************/
void ReadCont(unsigned char *pbuf, unsigned char lenght)
{
  unsigned char j;
  
  /* IO口模拟SPI模式 */
  L_SlaveSelect();        // SS管脚输出低，SPI启动
  *pbuf = (0x60 | *pbuf); // 取位B6B5 寄存器地址数据 单个标志位 格式为011XXXXX
  *pbuf = (0x7f & *pbuf); // 取低7位B0-B6 寄存器地址数据

  for(j = 0; j < 8; j++)  // 写寄存器地址
  {
    if (*pbuf & mask)     // 设置数据位
      SIMOON();
    else
      SIMOOFF();
     
    CLKON();    // 关联CLK时钟信息，发送数据
    CLKOFF();
    mask >>= 1; // 标志位右移
  }
  
  mask = 0x80;

  while(lenght > 0)  // 开始读取处理
  {
    *pbuf = 0;  // 清空缓冲区
    
    for(j = 0; j < 8; j++)
    {
      *pbuf <<= 1;  // 数据左移
      CLKON();      // 关联CLK时钟信息，发送数据
      CLKOFF();
      if (SOMISIGNAL)
        *pbuf |= 1;
    }

    pbuf++;   // 读结束处理
    lenght--;
  }

  H_SlaveSelect(); // SS管脚输出高，SPI停止
}  


/*******************************************************************************
* 函数名称：DirectCommand()
* 功    能：直接命令可发送一个命令到阅读器芯片
* 入口参数：*pbuf            将要发送的命令数据           
* 出口参数：无
* 说    明：直接命令。
*******************************************************************************/
void DirectCommand(unsigned char *pbuf)
{
  unsigned char j;

  /* IO口模拟SPI模式 */
  L_SlaveSelect();        // SS管脚输出低，SPI启动
  *pbuf = (0x80 | *pbuf); // 取位B7 寄存器地址数据 命令标志位 格式为1XXXXXXX
  *pbuf = (0x9f & *pbuf); // 命令值

  for(j = 0; j < 8; j++)  // 写寄存器地址
  {
    if (*pbuf & mask)  // 设置数据位
      SIMOON();
    else
      SIMOOFF();

    CLKON();  // 关联CLK时钟信息，发送数据
    CLKOFF();
    mask >>= 1; // 标志位右移
  } 
  mask = 0x80;

  CLKON();   // 增加额外时钟脉冲
  CLKOFF();
  H_SlaveSelect();  // SS管脚输出高，SPI停止
}   


/*******************************************************************************
* 函数名称：RAWwrite()
* 功    能：直接写数据或命令到阅读器芯片
* 入口参数：*pbuf           将要发送的命令数据    
*           lenght          写入数据或命令的长度    
* 出口参数：无
* 说    明：直接写。
*******************************************************************************/
void RAWwrite(unsigned char *pbuf, unsigned char lenght)
{ 
  unsigned char j;

  /* IO口模拟SPI模式 */
  L_SlaveSelect(); // SS管脚输出低，SPI启动
  
  while(lenght > 0)
  {
    for(j = 0; j < 8; j++) // 写寄存器地址
    {
      if (*pbuf & mask)  // 设置数据位
        SIMOON();
      else
        SIMOOFF();

      CLKON();  // 关联CLK时钟信息，发送数据
      CLKOFF();
      mask >>= 1; // 标志位右移
    }
            
    mask = 0x80;
 
    pbuf++;
    lenght--;
  }  

  H_SlaveSelect(); // SS管脚输出高，SPI停止
}


/*******************************************************************************
* 函数名称：InitialSettings()
* 功    能：初始化TRF7960设置
* 入口参数：无 
* 出口参数：无
* 说    明：设置频率输出及调制深度
*******************************************************************************/
void InitialSettings(void)
{
  unsigned char command[2];

  command[0] = ModulatorControl;                  
  command[1] = 0x21;  // 调制和系统时钟控制：0x21 - 6.78MHz OOK(100%)
  //command[1] = 0x31; // 调制和系统时钟控制：0x31 - 13.56MHz OOK(100%)

  WriteSingle(command, 2);
}


/*******************************************************************************
* 函数名称：ReInitialize15693Settings()
* 功    能：重新初始化ISO15693设置
* 入口参数：无 
* 出口参数：无
* 说    明：包括重新初始化接收无应答等待时间和接收等待时间
*******************************************************************************/
void ReInitialize15693Settings(void)
{
  unsigned char command[2];

  command[0] = RXNoResponseWaitTime;  // 接收无应答等待时间
  command[1] = 0x14;
  WriteSingle(command, 2);

  command[0] = RXWaitTime; // 接收等待时间                   
  command[1] = 0x20;
  WriteSingle(command, 2);
}


/*******************************************************************************
* 函数名称：InterruptHandlerReader()
* 功    能：阅读器中断处理程序
* 入口参数：*Register           中断状态寄存器 
* 出口参数：无
* 说    明：处理外部中断服务程序
*           IRQ中断状态寄存器说明如下：
*
*   位      位名称      功能                         说明
*   B7      Irq_tx      TX结束而IRQ置位         指示TX正在处理中。该标志位在TX开始时被设置，但是中断请求是在TX完成时才发送。
*   B6      Irq_srx     RX开始而IRQ置位         指示RX SOF已经被接收到并且RX正在处理中。该标志位在RX开始时被设置，但是中断请求是在RX完成时才发送。
*   B5      Irq_fifo    指示FIFO为1/3>FIFO>2/3  指示FIFO高或者低（小于4或者大于8）。
*   B4      Irq_err1    CRC错误                 接收CRC
*   B3      Irq_err2    奇偶校验错误            (在ISO15693和Tag-it协议中未使用)
*   B2      Irq_err3    字节成帧或者EOF错误 
*   B1      Irq_col     冲撞错误                ISO14443A和ISO15693单副载波。
*   B0      Irq_noresp  无响应中断              指示MCU可以发送下一个槽命令。
*******************************************************************************/
void InterruptHandlerReader(unsigned char *Register)
{
  unsigned char len;

  if(*Register == 0xA0) // A0 = 10100000 指示TX发送结束，并且在FIFO中剩下3字节数据
  {                
    i_reg = 0x00;
  }

  else if(*Register == BIT7) // BIT7 = 10000000 指示TX发送结束
  {            
    i_reg = 0x00;
    *Register = Reset;   // 在TX发送结束后 执行复位操作
    DirectCommand(Register);
  }

  else if((*Register & BIT1) == BIT1) // BIT1 = 00000010 冲撞错误
  {                           
    i_reg = 0x02; // 设置RX结束

    *Register = StopDecoders; // 在TX发送结束后复位FIFO
    DirectCommand(Register);

    CollPoss = CollisionPosition;
    ReadSingle(&CollPoss, 1);

    len = CollPoss - 0x20; // 获取FIFO中的有效数据字节数量

    if((len & 0x0f) != 0x00) 
      len = len + 0x10; // 如果接收到不完整字节，则加上一个字节
    len = len >> 4;

    if(len != 0x00)
    {
      rfid_buf[RXTXstate] = FIFO; // 将接收到的数据写到缓冲区的当前位置                               
      ReadCont(&rfid_buf[RXTXstate], len);
      RXTXstate = RXTXstate + len;
    }

    *Register = Reset;       // 执行复位命令
    DirectCommand(Register);

    *Register = IRQStatus;  // 获取IRQ中断状态寄存器地址
    *(Register + 1) = IRQMask;

    if (SPIMODE)  // 读取寄存器
      ReadCont(Register, 2);
    else
      ReadSingle(Register, 1);

    IRQCLR();  // 清中断
  }
  else if(*Register == BIT6) // BIT6 = 01000000 接收开始
  {   
    if(RXErrorFlag == 0x02) // RX接收标志位指示EOF已经被接收，并且指示在FIFO寄存器中未被读字节的数量
    {
      i_reg = 0x02;
      return;
    }

    *Register = FIFOStatus;
    ReadSingle(Register, 1); // 读取在FIFO中剩下字节的数量
    *Register = (0x0F & *Register) + 0x01;
    rfid_buf[RXTXstate] = FIFO; // 将接收到的数据写到缓冲区的当前位置
                                                                                	
    ReadCont(&rfid_buf[RXTXstate], *Register);
    RXTXstate = RXTXstate +*Register;

    *Register = TXLenghtByte2; // 读取是否有不完整的字节及其位数量
    ReadCont(Register, 1);

    if((*Register & BIT0) == BIT0) // 00000001 无响应中断
    {
      *Register = (*Register >> 1) & 0x07; // 标记前5位
      *Register = 8 - *Register;
      rfid_buf[RXTXstate - 1] &= 0xFF << *Register;
    }

    *Register = Reset;   // 最后一个字节被读取后复位FIFO
    DirectCommand(Register);

    i_reg = 0xFF; // 指示接收函数这些字节已经是最后字节
  }
  else if(*Register == 0x60) // 0x60 = 01100000 RX已经完毕 并且有9个字节在FIFO中
  {                            
    i_reg = 0x01; // 设置标志位
    rfid_buf[RXTXstate] = FIFO;
    ReadCont(&rfid_buf[RXTXstate], 9); // 从FIFO中读取9个字节数据
    RXTXstate = RXTXstate + 9;

    if(IRQPORT & IRQPin) // 如果中断管脚为高电平
    {
      *Register = IRQStatus; // 获取IRQ中断状态寄存器地址
      *(Register + 1) = IRQMask;
      
      if (SPIMODE) // 读取寄存器
        ReadCont(Register, 2);
      else
        ReadSingle(Register, 1);
            
      IRQCLR(); // 清中断

      if(*Register == 0x40) // 0x40 = 01000000 接收结束
      {  
        *Register = FIFOStatus;
        ReadSingle(Register, 1); // 读取在FIFO中剩下字节的数量
        *Register = 0x0F & (*Register + 0x01);
        rfid_buf[RXTXstate] = FIFO; // 将接收到的数据写到缓冲区的当前位置
                                                                                	
        ReadCont(&rfid_buf[RXTXstate], *Register);
        RXTXstate = RXTXstate +*Register;

        *Register = TXLenghtByte2; // 读取是否有不完整的字节及其位数量
        ReadSingle(Register, 1);         

        if((*Register & BIT0) == BIT0) // 00000001 无响应中断
        {
          *Register = (*Register >> 1) & 0x07; // 标记前5位
          *Register = 8 -*Register;
          rfid_buf[RXTXstate - 1] &= 0xFF << *Register;
        }   

        i_reg = 0xFF;      // 指示接收函数这些字节已经是最后字节
        *Register = Reset; // 在最后字节被读取后复位FIFO
        DirectCommand(Register);
      }
      else if(*Register == 0x50) // 0x50 = 01010000接收结束并且发生CRC错误
      {        
        i_reg = 0x02;
      }
    } 
    else                                        
    {
      Register[0] = IRQStatus; // 获取IRQ中断状态寄存器地址
      Register[1] = IRQMask;
      
      if (SPIMODE)
        ReadCont(Register, 2); // 读取寄存器
      else
        ReadSingle(Register, 1);
            
      if(Register[0] == 0x00) 
        i_reg = 0xFF; // 指示接收函数这些字节已经是最后字节
        }
    }
    else if((*Register & BIT4) == BIT4) // BIT4 = 00010000 指示CRC错误
    {                      
      if((*Register & BIT5) == BIT5) // BIT5 = 00100000 指示FIFO中有9个字节
      {
        i_reg = 0x01;   // 接收完成
        RXErrorFlag = 0x02;
      }
      else
        i_reg = 0x02; // 停止接收        
    }
    else if((*Register & BIT2) == BIT2) // BIT2 = 00000100  字节成帧或者EOF错误
    {                       
      if((*Register & BIT5) == BIT5) // BIT5 = 00100000 指示FIFO中有9个字节
      {
        i_reg = 0x01;  // 接收完成
        RXErrorFlag = 0x02;
      }
      else
        i_reg = 0x02; // 停止接收 
    }
    else if(*Register == BIT0) // BIT0 = 00000001 中断无应答
    {                      
      i_reg = 0x00;
    }
    else
    {   
      i_reg = 0x02;

      *Register = StopDecoders; // 在TX发送接收后复位FIFO
      DirectCommand(Register);

      *Register = Reset;
      DirectCommand(Register);

      *Register = IRQStatus;  // 获取IRQ中断状态寄存器地址
      *(Register + 1) = IRQMask;

      if (SPIMODE)
        ReadCont(Register, 2); // 读取寄存器
      else
        ReadSingle(Register, 1);
        
      IRQCLR(); // 清中断
  }
}


/*******************************************************************************
 * 函数名称：HAL_ISR_FUNCTION
 * 功    能：P0口输入中断的中断服务程序
 * 入口参数：无
 * 出口参数：无
 * 返 回 值：无
 ******************************************************************************/
HAL_ISR_FUNCTION( halRFIDPort0Isr, P0INT_VECTOR )
{
  unsigned char Register[4];
  
  if (HAL_RFID_PORT_PXIFG & HAL_RFID_BIT)
  {
    do
    {
      IRQCLR(); // 清端口2中断标志位
      Register[0] = IRQStatus; // 获取IRQ中断状态寄存器地址
      Register[1] = IRQMask;   // 虚拟读 Dummy read
  
      if (SPIMODE)  // 读取寄存器
        ReadCont(Register, 2);
      else
        ReadSingle(Register, 1); 

      if(*Register == 0xA0) // A0 = 10100000 指示TX发送结束，并且在FIFO中剩下3字节数据
      {   
        goto FINISH; // 跳转到FINISH处
      }
        
      InterruptHandlerReader(&Register[0]); // 执行中断服务程序
    }while((IRQPORT & IRQPin) == IRQPin);
  }
FINISH:
  ;  
}


/*******************************************************************************
 * 函数名称：TRF7960_IO_Init_SPI
 * 功    能：对控制TRF7960的IO进行初始化,采用SPI方式
 * 入口参数：无
 * 出口参数：无
 * 返 回 值：无
 ******************************************************************************/
void TRF7960_IO_Init_SPI(void)
{
  /* P0.0,P0.1,P0.7为通用IO，不影响P0口其他引脚 */
  P0SEL &= ~0x83;
  
  /* P1.0,P1.4为通用IO，不影响P1口其他引脚 */
  P1SEL &= ~0x11; 
  
  /* P0.0,P0.7方向为输出 */
  P0DIR |= 0x81;
  
  /* P1.0方向为输出 */
  P1DIR |= 0x01;
  
  /* P0.1方向为输入 */
  P0DIR &= ~0x02;
   
  /* P1.4方向为输入 */
  P1DIR &= ~0x10;
  
  /* P0.1为输入上拉 */
  P0INP &= ~0x02;  // P0.1选择为上拉/下拉（而非三态）模式
  P2INP &= ~0x20;  // 将P0口所有选择为上拉/下拉（而非三态）模式的引脚配置为上拉 
  
  /* P1.4为输入上拉 */
  P1INP &= ~0x10;  // P1.4选择为上拉/下拉（而非三态）模式
  P2INP &= ~0x40;  // 将P1口所有选择为上拉/下拉（而非三态）模式的引脚配置为上拉
  
  /* P0.1输入中断为上升沿触发，并使能P0.1中断（未打开全局中断）*/
  PICTL &= ~0x01;  // P0口输入中断为上升沿触发
  P0IEN |= 0x02;   // P0.1中断使能
} 


/*******************************************************************************
 * 函数名称：HalRFIDInit
 * 功    能：CC253x操作TRF7960管脚设置及TRF7960芯片初始化
 * 入口参数：无
 * 出口参数：无
 * 返 回 值：无
 *******************************************************************************/
void HalRFIDInit( void )
{
  TRF7960_IO_Init_SPI();
  //InitialSettings(); // 初始化设置：设置MSP430时钟频率为6.78MHz及OOK调制模式
}


/*******************************************************************************
* 函数名称：EnableSlotCounter()
* 功    能：使能槽计数功能。
* 入口参数：无
* 出口参数：无     
* 说    明：该函数使能槽计数功能，用于多个槽时。
*******************************************************************************/
void EnableSlotCounter(void)
{
  rfid_buf[41] = IRQMask;  // 下个计数槽
  rfid_buf[40] = IRQMask;
  ReadSingle(&rfid_buf[41], 1); // 读取缓冲区数据
  rfid_buf[41] |= BIT0;         // 在缓冲区寄存器0x41位置设置BIT0有效
  WriteSingle(&rfid_buf[40], 2);
}


/*******************************************************************************
* 函数名称：DisableSlotCounter()
* 功    能：禁止槽计数功能。
* 入口参数：无
* 出口参数：无     
* 说    明：该函数使槽计数功能停止。
*******************************************************************************/
void DisableSlotCounter(void)
{
  rfid_buf[41] = IRQMask;  // 下个计数槽
  rfid_buf[40] = IRQMask;
  ReadSingle(&rfid_buf[41], 1); // 读取缓冲区数据
  rfid_buf[41] &= 0xfe;         // 在缓冲区寄存器0x41位置设置BIT0无效
  WriteSingle(&rfid_buf[40], 2);
}


/*******************************************************************************
* 函数名称：InventoryRequest()
* 功    能：ISO15693协议卡片总量请求命令。
* 入口参数：*mask       标记命令
*           lenght      命令长度
* 出口参数：无     
* 说    明：执行该函数可以使ISO15693协议标准总量命令循环16时间槽或者1个时间槽.
*           其中：0x14表示16槽；0x17表示1个槽。
*           注意：在脱机模式下，接收到UID码将被显示到LCM图形显示屏上。
*******************************************************************************/
void InventoryRequest(unsigned char *mask, unsigned char lenght)
{
  unsigned char i = 1, j, command[2], NoSlots, found = 0;
  unsigned char *PslotNo, slotNo[17];
  unsigned char NewMask[8], NewLenght, masksize;
  int size;
  unsigned int k = 0;

  rfid_buf[0] = ModulatorControl; // 调制和系统时钟控制：0x21 - 6.78MHz OOK(100%)
  rfid_buf[1] = 0x21;
  WriteSingle(rfid_buf, 2);
 
  /* 如果使用SPI串行模式的低数据率，那么 RXNoResponseWaitTime 需要被重新设置 */
  if(SPIMODE)
  {
    if((rfid_flags & BIT1) == 0x00) // 低数据比特率
    {
      rfid_buf[0] = RXNoResponseWaitTime;
      rfid_buf[1] = 0x2F;
      WriteSingle(rfid_buf, 2);
    }
    else // 高数据比特率
    {
      rfid_buf[0] = RXNoResponseWaitTime;
      rfid_buf[1] = 0x13;
      WriteSingle(rfid_buf, 2);
    }
  }
       
  slotNo[0] = 0x00;

  if((rfid_flags & BIT5) == 0x00) // 位5标志位指示槽的数量
  {                       
    NoSlots = 17; // 位5为0x00，表示选择16槽模式
    EnableSlotCounter();
  }
  else // 如果位5不为0x00，表示选择1个槽模式
    NoSlots = 2;

  PslotNo = &slotNo[0]; // 槽数量指针
    
  /* 如果lenght是4或者8，那么masksize 标记大小值为 1  */
  /* 如果lenght是12或者16，那么masksize 标记大小值为 2，并依次类推 */
  masksize = (((lenght >> 2) + 1) >> 1);
  size = masksize + 3; // mask value + mask lenght + command code + rfid_flags

  rfid_buf[0] = 0x8f;
  rfid_buf[1] = 0x91;  // 发送带CRC校验
  rfid_buf[2] = 0x3d;  // 连续写模式
  rfid_buf[3] = (char) (size >> 8);
  rfid_buf[4] = (char) (size << 4);
  rfid_buf[5] = rfid_flags;  // ISO15693 协议标志rfid_flags
  rfid_buf[6] = 0x01;        // 仿冲撞命令值

  /* 可以在此加入AFI应用族标识符 */
  
  rfid_buf[7] = lenght; // 标记长度 masklenght
    
  if(lenght > 0)
  {
    for(i = 0; i < masksize; i++) 
      rfid_buf[i + 8] = *(mask + i);
  }                   

  command[0] = IRQStatus;
  command[1] = IRQMask;  // 虚拟读(Dummy read)
  ReadCont(command, 1);

  IRQCLR();  // 清中断标志位
  IRQON();   // 中断开启

  RAWwrite(&rfid_buf[0], masksize + 8); // 将数据写入到FIFO缓冲区中

  i_reg = 0x01;  // 设置中断标志值
  halMcuWaitMs(20);

  for(i = 1; i < NoSlots; i++) // 寻卡循环1个槽或者16个槽
  {       
    /* 初始化全局计数器 */
    RXTXstate = 1; // 设置标志位，其接收位存储于rfid_buf[1]起始位置

    k = 0;
    halMcuWaitMs(20);
                
    while(i_reg == 0x01) // 等待RX接收结束
    {           
      k++;

      if(k == 0xFFF0)
      {
        i_reg = 0x00;
        RXErrorFlag = 0x00;
        break;
      }
    }

    command[0] = RSSILevels; // 读取信号强度值 RSSI
    ReadSingle(command, 1);

    if(i_reg == 0xFF) // 如果字节已经是最后字节，接收到UID数据
    {     
      found = 1;
                 
      for(j = 3; j < 11; j++)
      {
        UIDString[j-3] = rfid_buf[j];  // 发送ISO15693 UID码
      }

      UIDString[8] = command[0];
    }
    else if(i_reg == 0x02) // 如果有冲撞发生
    { 
      PslotNo++;
      *PslotNo = i;
    }
    else if(i_reg == 0x00) // 如果定时时间到，中断发生
      ;
    else
      ;

    command[0] = Reset;    // 在接收下一个槽之前，使用直接命令复位FIFO
    DirectCommand(command);

    if((NoSlots == 17) && (i < 16)) // 如果在16个槽模式下，未循环16个槽，则需要发送EOF命令(下个槽)
    {                   
      command[0] = StopDecoders;
      DirectCommand(command);  // 停止解码
      command[0] = RunDecoders;               
      DirectCommand(command);             
      command[0] = TransmitNextSlot;
      DirectCommand(command); // 传送下一个槽
    }
    else if((NoSlots == 17) && (i == 16)) // 如果在16个槽模式下，循环了16个槽，则需要发送停止槽计数命令
    {                   
      DisableSlotCounter(); // 停止槽计数
    }
    else if(NoSlots == 2) // 如果是单个槽模式，则跳出本 for 循环
      break;
  }

  if(found)
  {                       
    uidfound = 1;
  }
  else
  {
    found = 0;
    uidfound = 0;
  }

  NewLenght = lenght + 4;  // 标记长度为4比特位倍数
  masksize = (((NewLenght >> 2) + 1) >> 1) - 1;

  /* 如果是16个槽模式，及槽指针不为0x00, 则递归调用本函数，再次寻找卡片 */
  while((*PslotNo != 0x00) && (NoSlots == 17)) 
  {
    *PslotNo = *PslotNo - 1;
    for(i = 0; i < 8; i++) 
      NewMask[i] = *(mask + i); //首先将标记值拷贝到新标记数组中

    if((NewLenght & BIT2) == 0x00)
      *PslotNo = *PslotNo << 4;

    NewMask[masksize] |= *PslotNo;  // 标记值被改变
    InventoryRequest(&NewMask[0], NewLenght); // 递归调用 InventoryRequest 函数
    PslotNo--; // 槽递减
  }  
       
  IRQOFF(); // 仿冲撞过程结束，关闭中断
}  


/*******************************************************************************
* 函数名称：RequestCommand()
* 功    能：卡片协议请求命令及处理和计时阅读器到卡片的应答。
* 入口参数：*prfid_buf           命令值
*           lenght          命令长度
*           brokenBits      不完整字节的位数量
*           noCRC           是否有CRC校验
* 出口参数：1     
* 说    明：该函数为协议请求命令，若返回1，则说明该函数成功执行，
*           若返回0或者不返回，则异常。
*******************************************************************************/
unsigned char RequestCommand(unsigned char *pbuf, unsigned char lenght, unsigned char brokenBits, char noCRC)
{
  unsigned char index, command;                

  RXTXstate = lenght;                             
  *pbuf = 0x8f;
  
  if(noCRC) 
    *(pbuf + 1) = 0x90; // 传输不带CRC校验
  else
    *(pbuf + 1) = 0x91; // 传输带CRC校验
    
  *(pbuf + 2) = 0x3d;
  *(pbuf + 3) = RXTXstate >> 4;
  *(pbuf + 4) = (RXTXstate << 4) | brokenBits;

  if(lenght > 12)
    lenght = 12;

  if(lenght == 0x00 && brokenBits != 0x00)
  {
    lenght = 1;
    RXTXstate = 1;
  }

  RAWwrite(pbuf, lenght + 5); // 以直接写FIFO模式发送命令

  IRQCLR(); // 清中断标志位
  IRQON();

  RXTXstate = RXTXstate - 12;
  index = 17;
  i_reg = 0x01;
    
  while(RXTXstate > 0)
  {
    if(RXTXstate > 9) // 在RXTXstate全局变量中未发送的字节数量如果大于9             
      lenght = 10;  // 长度为10，其中包括FIFO中的9个字节及1个字节的地址值
    else if(RXTXstate < 1)  // 如果该值小于1，则说明所有的字节已经发送到FIFO中，并从中断返回
      break;
    else   // 所有的值已经全部被发送
      lenght = RXTXstate + 1;         

    rfid_buf[index - 1] = FIFO; // 向FIFO缓冲器中写入9个或者更少字节的数据，将用于发送
    WriteCont(&rfid_buf[index - 1], lenght);
    RXTXstate = RXTXstate - 9; // 写9个字节到FIFO中
    index = index + 9;
  } 

  RXTXstate = 1; // 设置标志位，其接收位存储于rfid_buf[1]起始位置

  while(i_reg == 0x01) // 等待传送结束
    halMcuWaitMs(60);

  i_reg = 0x01;
  halMcuWaitMs(60);

  /* 如果中断标志位错误，则先复位后发送下个槽命令 */
  if((((rfid_buf[5] & BIT6) == BIT6) && ((rfid_buf[6] == 0x21) || (rfid_buf[6] == 0x24) || (rfid_buf[6] == 0x27) || (rfid_buf[6] == 0x29)))
    || (rfid_buf[5] == 0x00 && ((rfid_buf[6] & 0xF0) == 0x20 || (rfid_buf[6] & 0xF0) == 0x30 || (rfid_buf[6] & 0xF0) == 0x40)))
  {
    halMcuWaitMs(60);
    command = Reset;
    DirectCommand(&command);
    command = TransmitNextSlot;
    DirectCommand(&command);
  }  
  
  while(i_reg == 0x01); // 等待接收完毕

  IRQOFF(); // 关闭中断
  
  return(1); // 函数全部执行完毕，返回 1
}

#endif



