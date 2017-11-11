/*******************************************************************************
* 文 件 名：SHT11.c
* 功    能：SHT11温湿度传感器驱动。
* 硬件连接：SHT11与CC2530的硬件连接关系如下所示：
*
*           SHT11                  CC2530
*           DATA                    p0.1//P0.7
*           SCK                     p1.7//P0.0
*******************************************************************************/

#include "hal_types.h"
#include "OSAL.h"
#include "OnBoard.h"
#include "hal_sht11.h"
#include "math.h"


#define IO_IN                            0
#define IO_OUT                           1


#define IO_DIR_PORT_PIN(port, pin, dir)  \
   do {                                  \
      if (dir == IO_OUT)                 \
         P##port##DIR |= (0x01<<(pin));  \
      else                               \
         P##port##DIR &= ~(0x01<<(pin)); \
   }while(0)



/*******************************************************************************
 * 函数名称：SHT11_PIN_Init
 *
 * 功能描述：CC2430与SHT11相连接的引脚初始化
 *
 * 参    数：无
 *
 * 返 回 值：无
 ******************************************************************************/
void Hal_SHT11_Init(void)
{
  P1SEL = P1SEL & 0x7f;
  IO_DIR_PORT_PIN(1, 7, IO_OUT);      //p1.7 // P0.0方向为输出
  IO_DIR_PORT_PIN(0, 1, IO_OUT);      //p0.1// P0.7方向为输出
  DATA = 1;
  SCK = 0;
}


/*******************************************************************************
 * 函数名称：s_write_byte
 *
 * 功能描述：向SHT11写1个字节并检测SHT11是否应答
 *
 * 参    数：value  要写入的1字节数据
 *
 * 返 回 值：error=1表明SHT11未应答
 ******************************************************************************/
char s_write_byte(unsigned char value)
{
  static unsigned char i,j,error=0;

  IO_DIR_PORT_PIN(0, 1, IO_OUT);      //p0.1// P0.7方向为输出

  /* 从高到低逐位发送 */
  for (i=0x80;i>0;i/=2)               // 移位掩码
  {
    if(i & value)
      DATA = 1;
    else
      DATA = 0;

    SCK = 1;
    for(j=0;j<24;j++ )  asm("nop");         	
    SCK = 0;
  }

  DATA=1;                              // 释放DATA线
  SCK=1;                               // 第9个SCK
  IO_DIR_PORT_PIN(0, 1, IO_IN);        //p0.1// P0.7方向为输入
  error=DATA;                          // 检查应答 (SHT11将拉底DATA作为应答)
  SCK=0;
  return error;                        // error=1表明SHT11未应答
}


/*******************************************************************************
 * 函数名称：s_read_byte
 *
 * 功能描述：从SHT11读1个字节并当输入参数ack=1时给出应答
 *
 * 参    数：ack  应答标志
 *
 * 返 回 值：error=1表明SHT11未应答
 ******************************************************************************/
char s_read_byte(unsigned char ack)
{
  unsigned char i,j,val=0;

  IO_DIR_PORT_PIN(0, 1, IO_OUT);      //p0.1// P0.7方向为输出

  DATA=1;                               // 释放DATA线

  IO_DIR_PORT_PIN(0, 1, IO_IN);         //p0.1// P0.7方向为输入
  /* 从高到低逐位读取 */
  for (i=0x80;i>0;i/=2)                 // 移位掩码
  {
    SCK = 1;
    if (DATA) val=(val | i);
    SCK = 0;  					
  }

  IO_DIR_PORT_PIN(0, 1, IO_OUT);         //p0.1// P0.2方向为输出
  DATA = !ack;                           // 当ack=1时拉底DATA线
  SCK = 1;                               // 第9个SCK
  for(j=0;j<24;j++ )  asm("nop");         	
  SCK = 0;						
  DATA = 1;                              // 释放DATA线
  return val;
}


/*******************************************************************************
 * 函数名称：s_transstart
 *
 * 功能描述：发送一个"启动传输"序列
 *                 _____         ________
 *           DATA:      |_______|
 *                     ___     ___
 *           SCK : ___|   |___|   |______
 *
 * 参    数：无
 *
 * 返 回 值：无
 ******************************************************************************/
void s_transstart(void)
{
   unsigned char j;

   IO_DIR_PORT_PIN(0, 1, IO_OUT);         //p0.1// P0.7方向为输出

   DATA = 1; SCK = 0;                     // 初始状态
   for(j=0;j<8;j++ )  asm("nop");         	
   SCK = 1;
   for(j=0;j<8;j++ )  asm("nop");         	
   DATA = 0;
   for(j=0;j<8;j++ )  asm("nop");         	
   SCK = 0;
   for(j=0;j<24;j++ ) asm("nop");         	
   SCK = 1;
   for(j=0;j<8;j++ )  asm("nop");         	
   DATA = 1;		
   for(j=0;j<8;j++ )  asm("nop");         	
   SCK= 0;		
}


/*******************************************************************************
 * 函数名称：s_connectionreset
 *
 * 功能描述：通信复位
 *                 _____________________________________________________         ________
 *           DATA:                                                      |_______|
 *                    _    _    _    _    _    _    _    _    _        ___     ___
 *           SCK : __| |__| |__| |__| |__| |__| |__| |__| |__| |______|   |___|   |______
 *
 * 参    数：无
 *
 * 返 回 值：无
 ******************************************************************************/
void s_connectionreset(void)
{
  unsigned char i;

  IO_DIR_PORT_PIN(0, 1, IO_OUT);            //p0.1// P0.2方向为输出

  DATA = 1; SCK = 0;                        // 初始状态

  /* 9个SCK 周期*/
  for(i=0;i<9;i++)
  {
    SCK = 1;
    SCK = 0;
  }

  s_transstart();                           // 发送一个"启动传输"序列
}


/*******************************************************************************
 * 函数名称：s_softreset
 *
 * 功能描述：软件复位
 *
 * 参    数：无
 *
 * 返 回 值：返回值为1表示SHT11未响应
 ******************************************************************************/
char s_softreset(void)
{
  unsigned char error=0;

  s_connectionreset();                       // 通信复位
  error+=s_write_byte(RESET);                // 发送"复位"命令给SHT11
  return error;                              // error=1表示SHT11未响应
}


/*******************************************************************************
 * 函数名称：s_read_statusreg
 *
 * 功能描述：读取状态寄存器和校验和
 *
 * 参    数：p_value      状态寄存器的值
 *           p_checksum   校验和
 *
 * 返 回 值：返回值为1表示SHT11未响应
 ******************************************************************************/
char s_read_statusreg(unsigned char *p_value, unsigned char *p_checksum)
{
  unsigned char error=0;

  s_transstart();                             // 发送一个"启动传输"序列
  error=s_write_byte(STATUS_REG_R);           // 发送"读状态寄存器"命令
  *p_value=s_read_byte(ACK);                  // 读状态寄存器
  *p_checksum=s_read_byte(noACK);             // 读校验和

  return error;                               // error=1表示SHT11未响应
}


/*******************************************************************************
 * 函数名称：s_write_statusreg
 *
 * 功能描述：写状态寄存器
 *
 * 参    数：p_value      状态寄存器的值
 *
 * 返 回 值：返回值为1表示SHT11未响应
 ******************************************************************************/
char s_write_statusreg(unsigned char *p_value)
{
  unsigned char error=0;

  s_transstart();                              // 发送一个"启动传输"序列
  error+=s_write_byte(STATUS_REG_W);           // 发送"写状态寄存器"命令
  error+=s_write_byte(*p_value);               // 写状态寄存器

  return error;                                // error=1表示SHT11未响应
}


/*******************************************************************************
 * 函数名称：s_measure
 *
 * 功能描述：进行一次测量(相对湿度或温度)
 *
 * 参    数：p_value      测量值
 *           checksum     校验和
 *           mode         TEMP表示进行温度测量
 *                        HUMI表示进行相对湿度测量
 *
 * 返 回 值：返回值为1表示SHT11未响应
 ******************************************************************************/
char s_measure(unsigned char *p_value, unsigned char *p_checksum, unsigned char mode)
{
  unsigned error=0;
  unsigned long i;

  s_transstart();                              // 发送一个"启动传输"序列
  switch(mode)                                 // 根据输入参数mode进行一次相应的测量
  {
    case TEMP	: error+=s_write_byte(MEASURE_TEMP); break;
    case HUMI	: error+=s_write_byte(MEASURE_HUMI); break;
    default     : break;	
  }

  for (i=0;i<165535;i++) if(DATA==0) break;    // 等待SHT11完成测量
  if(DATA) error+=1;                           // 测量错误

  *(p_value+1) = s_read_byte(ACK);             // 读第1个字节 (MSB)
  *(p_value) = s_read_byte(ACK);               // 读第2个字节 (LSB)
  *p_checksum =s_read_byte(noACK);             // 读校验和

  return error;
}


/*******************************************************************************
 * 函数名称：calc_sth11
 *
 * 功能描述：计算相对湿度和温度
 *
 * 参    数：p_humidity      SHT11采集到的相对湿度值(经过本函数的转换，该参数返回实际物理量的值)
 *           p_temperature   SHT11采集到的温度值(经过本函数的转换，该参数返回实际物理量的值)
 *
 * 返 回 值：无
 ******************************************************************************/
void calc_sth11(float *p_humidity ,float *p_temperature)
{
  const float C1=-4.0;                          // 12位
  const float C2=+0.0405;                       // 12 Bit
  const float C3=-0.0000028;                    // 12 Bit
  const float T1=+0.01;                         // 14位 5V
  const float T2=+0.00008;                      // 14位 5V	

  float rh=*p_humidity;                         // 相对湿度采集值 12位
  float t=*p_temperature;                       // 温度采集值 14位
  float rh_lin;                                 // 相对湿度的非线性补偿
  float rh_true;                                // 相对湿度物理量值
  float t_C;                                    // 温度物理量值

  t_C=t*0.01 - 40;                              // 计算温度物理量值
  rh_lin=C3*rh*rh + C2*rh + C1;                 // 计算相对湿度的非线性补偿
  rh_true=(t_C-25)*(T1+T2*rh)+rh_lin;           // 计算相对湿度物理量值

  /* 若计算出来的相对湿度物理量值超范围则截断 */
  if(rh_true>100)rh_true=100;
  if(rh_true<0.1)rh_true=0.1;

  *p_temperature=t_C;                            // 返回温度物理量值
  *p_humidity=rh_true;                           // 返回相对湿度物理量值
}


/*******************************************************************************
 * 函数名称：calc_dewpoint
 *
 * 功能描述：计算露点
 *
 * 参    数：h      相对湿度物理量值
 *           t      温度物理量值
 *
 * 返 回 值：露点值
 ******************************************************************************/
float calc_dewpoint(float h,float t)
{
  float logEx,dew_point;

  logEx=0.66077+7.5*t/(237.3+t)+(log10(h)-2);
  dew_point = (logEx - 0.66077)*237.3/(0.66077+7.5-logEx);

  return dew_point;
}






















