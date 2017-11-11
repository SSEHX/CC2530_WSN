/*******************************************************************************
 * 文件名称：hal_rfid.h
 * 功    能：RFID芯片TRF7960驱动头文件
 * 硬件连接：TRF7960与CC253x的硬件连接关系如下：
 *
 *          TRF7960                 CC253x
 *           IRQ                     P0.1
 *           CLK                     P1.0
 *           MOSI                    P0.7
 *           MISO                    P1.4
 *           SS                      P0.0
 ******************************************************************************/


#ifndef HAL_RFID_H
#define HAL_RFID_H

#ifdef __cplusplus
extern "C"
{
#endif

/* TRF7960串行/并行通信模式选择 */
/********************************************************************/
#define PAR                 0
#define SPI                 1
#define SPIMODE             SPI     
/********************************************************************/
  
  
/* 外部变量声明 */
/********************************************************************/
extern unsigned char rfid_flags; // 定义存储标志位(在仿冲撞中使用)
extern unsigned char UIDString[9];
extern bool uidfound;


/* 外部函数声明 */
/********************************************************************/
extern void halMcuWaitMs(uint16 msec);
extern void WriteSingle(unsigned char *pbuf, unsigned char lenght);
extern void ReadSingle(unsigned char *pbuf, unsigned char lenght);
extern void ReadCont(unsigned char *pbuf, unsigned char lenght);
extern void InitialSettings(void);
extern void HalRFIDInit( void );
extern void InventoryRequest(unsigned char *mask, unsigned char lenght);
/********************************************************************/


/* 宏定义 */
/********************************************************************/

/* 
  位定义 
 */
#define BIT0                (0x0001)
#define BIT1                (0x0002)
#define BIT2                (0x0004)
#define BIT3                (0x0008)
#define BIT4                (0x0010)
#define BIT5                (0x0020)
#define BIT6                (0x0040)
#define BIT7                (0x0080)
#define BIT8                (0x0100)
#define BIT9                (0x0200)
#define BITA                (0x0400)
#define BITB                (0x0800)
#define BITC                (0x1000)
#define BITD                (0x2000)
#define BITE                (0x4000)
#define BITF                (0x8000)


/* TRF7961时钟CLK管脚连接 - CC2530管脚P1.0 */
/* CLK          -       P1.0 */
#define DATA_CLK_PORT       P1_0   //CLK管脚定义
#define DATA_CLK_BIT        BV(0)
#define DATA_CLK_SEL        P1SEL
#define DATA_CLK_DIR        P1DIR

/*-----------------------------------------------------------------------------*/
#define CLKGPIOset()        DATA_CLK_SEL &= ~DATA_CLK_BIT  //设置为GPIO功能
#define CLKFUNset()         DATA_CLK_SEL |= DATA_CLK_BIT   //设置为主功能函数
#define CLKPOUTset()        DATA_CLK_DIR |= DATA_CLK_BIT   //设置方向为输出
#define CLKON()             DATA_CLK_PORT = 1              //输出高电平
#define CLKOFF()            DATA_CLK_PORT = 0              //输出低电平
/*-----------------------------------------------------------------------------*/


/* TRF7960串行SlaveSelect管脚连接 - CC2530 CS管脚P0.0 */
/* SlaveSelect  -       P0.0 */
/*-----------------------------------------------------------------------------*/
#define SlaveSelectPin          BV(0)
#define SlaveSelectPort         P0_0
#define SlaveSelectGPIOset()    P0DIR &= ~SlaveSelectPin    //设置为GPIO
#define SlaveSelectPortSet()    P0DIR |= SlaveSelectPin     //设置方向为输出
#define H_SlaveSelect()         SlaveSelectPort = 1     //输出高电平
#define L_SlaveSelect()         SlaveSelectPort = 0     //输出低电平
/*-----------------------------------------------------------------------------*/


/* TRF7960串行SIMO管脚连接 - CC2530管脚P0.7 */
/* SIMO         -       P0.7 */
/*-----------------------------------------------------------------------------*/
#define SIMOPin             BV(7)
#define SIMOPort            P0_7
#define SIMOGPIOset()       P1SEL &= ~SIMOPin  //设置为GPIO功能
#define SIMOFUNset()        P1SEL |= SIMOPin   //设置为主功能函数
#define SIMOSet()           P1DIR |= SIMOPin           //设置方向为输出
#define SIMOON()            SIMOPort = 1               //输出高电平
#define SIMOOFF()           SIMOPort = 0               //输出低电平
/*-----------------------------------------------------------------------------*/


/* TRF7960串行SOMI管脚连接 - CC2530管脚P1.4 */
/* SOMI         -       P1.4 */
/*-----------------------------------------------------------------------------*/
#define SOMIPin             BV(4) 
#define SOMISIGNAL          P1_4
#define SOMIGPIOset()       P1SEL &= ~SOMIPin  //设置为GPIO功能
#define SOMIFUNset()        P1SEL |= SOMIPin   //设置为主功能函数
#define SOMISet()           P1DIR |= SOMIPin           //设置方向为输出
/*-----------------------------------------------------------------------------*/


/* 
  中断管脚定义 
  TRF7960中断IRQ管脚连接 - CC2530管脚P0.1
  IRQ          -       P0.1
 */
#define IRQ                    P0_1   //IRQ中断管脚定义
#define HAL_RFID_PORT          P0
#define HAL_RFID_BIT           BV(1)
#define HAL_RFID_SEL           P0SEL
#define HAL_RFID_DIR           P0DIR

#define HAL_RFID_PORT_EDGEBIT  BV(0)  // 中断边沿定义
#define HAL_RFID_PORT_EDGE     HAL_KEY_FALLING_EDGE// 下降沿

#define HAL_RFID_PORT_IEN      IEN1   // CPU中断寄存器标志
#define HAL_RFID_PORT_IENBIT   BV(5)  // 端口0中断标志位
#define HAL_RFID_PORT_ICTL     P0IEN  // 端口中断控制寄存器
#define HAL_RFID_PORT_ICTLBIT  BV(1)  // P0IEN - P0.1 使能/禁止位
#define HAL_RFID_PORT_PXIFG    P0IFG  // 中断标志
/********************************************************************/

#define IRQPin              BV(1)                        //第1脚
#define IRQPORT             P0                           //端口2
#define IRQPinset()         P0DIR &= ~IRQPin             //设置方向为输入
#define IRQON()             HAL_RFID_PORT_ICTL |= HAL_RFID_PORT_ICTLBIT; \
                            HAL_RFID_PORT_IEN |= HAL_RFID_PORT_IENBIT;    //IRQ 中断开启

#define IRQOFF()            HAL_RFID_PORT_ICTL &= ~HAL_RFID_PORT_ICTLBIT; \
                            HAL_RFID_PORT_IEN &= ~HAL_RFID_PORT_IENBIT;   //IRQ 中断关闭

#define IRQEDGEset()        PICTL &= ~HAL_RFID_PORT_EDGEBIT;            //上升沿中断

#define IRQCLR()            HAL_RFID_PORT_PXIFG &= ~0x02; \
                            P0IF = 0;
                              

/* TRF7960芯片地址定义 */
/*=============================================================================*/
#define ChipStateControl        0x00                        //芯片状态控制寄存器
#define ISOControl              0x01                        //ISO协议控制寄存器
#define ISO14443Boptions        0x02                        //ISO14443B协议可选寄存器
#define ISO14443Aoptions        0x03                        //ISO14443A协议可选寄存器
#define TXtimerEPChigh          0x04                        //TX发送高字节寄存器
#define TXtimerEPClow           0x05                        //TX发送低字节寄存器
#define TXPulseLenghtControl    0x06                        //TX发送脉冲长度控制寄存器
#define RXNoResponseWaitTime    0x07                        //RX接收无应答等待时间寄存器
#define RXWaitTime              0x08                        //RX接收等待时间寄存器
#define ModulatorControl        0x09                        //调制器及系统时钟寄存器
#define RXSpecialSettings       0x0A                        //RX接收特殊设置寄存器
#define RegulatorControl        0x0B                        //电压调整和I/O控制寄存器
#define IRQStatus               0x0C                        //IRQ中断状态寄存器
#define IRQMask                 0x0D                        //冲撞位置及中断标记寄存器
#define CollisionPosition       0x0E                        //冲撞位置寄存器
#define RSSILevels              0x0F                        //信号接收强度寄存器
#define RAMStartAddress         0x10                        //RAM 开始地址，共7字节长 (0x10 - 0x16)
#define NFCID                   0x17                        //卡片标识符
#define NFCTargetLevel          0x18                        //目标卡片RSSI等级寄存器
#define NFCTargetProtocol       0x19                        //目标卡片协议寄存器
#define TestSetting1            0x1A                        //测试设置寄存器1
#define TestSetting2            0x1B                        //测试设置寄存器2
#define FIFOStatus              0x1C                        //FIFO状态寄存器
#define TXLenghtByte1           0x1D                        //TX发送长度字节1寄存器
#define TXLenghtByte2           0x1E                        //TX发送长度字节2寄存器
#define FIFO                    0x1F                        //FIFO数据寄存器

/* TRF7960芯片命令定义 */
/*=============================================================================*/
#define Idle                    0x00                        //空闲命令
#define SoftInit                0x03                        //软件初始化命令
#define InitialRFCollision      0x04                        //初始化RF冲撞命令
#define ResponseRFCollisionN    0x05                        //应答RF冲撞N命令
#define ResponseRFCollision0    0x06                        //应答RF冲撞0命令
#define Reset                   0x0F                        //复位命令
#define TransmitNoCRC           0x10                        //传送无CRC校验命令
#define TransmitCRC             0x11                        //传送带CRC校验命令
#define DelayTransmitNoCRC  	0x12                        //定时传送无CRC校验命令
#define DelayTransmitCRC    	0x13                        //定时传送带CRC校验命令
#define TransmitNextSlot        0x14                        //传送下一个槽命令
#define CloseSlotSequence       0x15                        //关闭槽序列命令
#define StopDecoders            0x16                        //停止解码命令
#define RunDecoders             0x17                        //运行解码命令
#define ChectInternalRF         0x18                        //清空内部RF接收命令
#define CheckExternalRF         0x19                        //清空外部RF接收命令
#define AdjustGain              0x1A                        //增益调节命令
/*=============================================================================*/

#ifdef __cplusplus
}
#endif

#endif


