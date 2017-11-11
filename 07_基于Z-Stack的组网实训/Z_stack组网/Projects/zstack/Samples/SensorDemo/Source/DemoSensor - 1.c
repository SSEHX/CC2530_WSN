/*******************************************************************************
 * 文件名称：DemoSensor.c
 * 功    能：SensorApp 　　 无线传感器监控演示实验。
 * 实验内容：网关节点为协调器建立网络后，传感器节点发现该网络并加入到该网络中，
 *           并且将采集转换得到的温度值、湿度值、电压值等传送给采集设备。
 *           由网关设备转发数据至PC上位机软件WSN Monitor，以GUI方式显示。
 * 实验设备：SK-SmartRF05BB(装配有SK-CC2530EM)
 *           SK-SmartRF05BB-DOWN键：    向父节点发送传感器数据
 *
 *           D1(绿色)：                 向采集节点发送数据时闪烁
 *           D3(黄色)：                 成功加入协调器建立的网络后闪烁  
 *    
 ******************************************************************************/


/* 包含头文件 */
/********************************************************************/
#include "ZComDef.h"
#include "OSAL.h"
#include "sapi.h"
#include "hal_key.h"
#include "hal_lcd.h"
#include "hal_led.h"
#include "hal_adc.h"
#include "hal_mcu.h"
#include "hal_uart.h"
#include "DemoApp.h"
/********************************************************************/


#define REPORT_FAILURE_LIMIT                4
#define ACK_REQ_INTERVAL                    5 // each 5th packet is sent with ACK request

// General UART frame offsets
#define FRAME_SOF_OFFSET                    0
#define FRAME_LENGTH_OFFSET                 1 
#define FRAME_CMD0_OFFSET                   2
#define FRAME_CMD1_OFFSET                   3
#define FRAME_DATA_OFFSET                   4

// ZB_RECEIVE_DATA_INDICATION offsets
#define ZB_RECV_SRC_OFFSET                  0
#define ZB_RECV_CMD_OFFSET                  2
#define ZB_RECV_LEN_OFFSET                  4
#define ZB_RECV_DATA_OFFSET                 6
#define ZB_RECV_FCS_OFFSET                  8

// ZB_RECEIVE_DATA_INDICATION frame length
#define ZB_RECV_LENGTH                      15

// PING response frame length and offset
#define SYS_PING_RSP_LENGTH                 7 
#define SYS_PING_CMD_OFFSET                 1

// Stack Profile
#define ZIGBEE_2007                         0x0040
#define ZIGBEE_PRO_2007                     0x0041

#ifdef ZIGBEEPRO
#define STACK_PROFILE                       ZIGBEE_PRO_2007             
#else 
#define STACK_PROFILE                       ZIGBEE_2007
#endif

#define CPT_SOP                             0xFE
#define SYS_PING_REQUEST                    0x0021
#define SYS_PING_RESPONSE                   0x0161
#define ZB_RECEIVE_DATA_INDICATION          0x8746

// Application States
#define APP_INIT                            0    // Initial state
#define APP_START                           1    // Sensor has joined network
#define APP_BIND                            2    // Sensor is in process of binding
#define APP_REPORT                          4    // Sensor is in reporting state

// Application osal event identifiers
// Bit mask of events ( from 0x0000 to 0x00FF )
#define MY_START_EVT                        0x0001
#define MY_REPORT_EVT                       0x0002
#define MY_FIND_COLLECTOR_EVT               0x0004
#define MY_SEND_EVT               0x0010

// ADC definitions for CC2430/CC2530 from the hal_adc.c file

#define HAL_ADC_REF_125V    0x00    /* Internal 1.25V Reference */
#define HAL_ADC_DEC_064     0x00    /* Decimate by 64 : 8-bit resolution */
#define HAL_ADC_DEC_128     0x10    /* Decimate by 128 : 10-bit resolution */
#define HAL_ADC_DEC_512     0x30    /* Decimate by 512 : 14-bit resolution */
#define HAL_ADC_CHN_VDD3    0x0f    /* Input channel: VDD/3 */
#define HAL_ADC_CHN_TEMP    0x0e    /* Temperature sensor */

/* 宏定义 */
/********************************************************************/

/* 本地变量 */
/********************************************************************/
static uint8 appState =           APP_INIT;
static uint8 reportState =        FALSE;
static uint8 reportFailureNr =    0;
static uint16 myReportPeriod =    2000;         // 毫秒
static uint16 myBindRetryDelay =  2000;         // 毫秒
static uint16 parentShortAddr;
/********************************************************************/

/*
  传感器设备输入输出命令
 */
#define NUM_OUT_CMD_SENSOR                2
#define NUM_IN_CMD_SENSOR                 0

/* 
  传感器设备输出命令列表 
 */
const cId_t zb_OutCmdList[NUM_OUT_CMD_SENSOR] =
{
  SENSOR_REPORT_CMD_ID,
  DUMMY_REPORT_CMD_ID
};

/* 
  传感器设备的简单描述符 
 */
const SimpleDescriptionFormat_t zb_SimpleDesc =
{
  MY_ENDPOINT_ID,             //  端点号
  MY_PROFILE_ID,              //  Profile ID
  DEV_ID_SENSOR,              //  设备ID
  DEVICE_VERSION_SENSOR,      //  设备版本
  0,                          //  预留
  NUM_IN_CMD_SENSOR,          //  输入命令数量
  (cId_t *) NULL,             //  输入命令列表
  NUM_OUT_CMD_SENSOR,         //  输出命令数量
  (cId_t *) zb_OutCmdList     //  输出命令列表
};
/******************************************************************************/


/* 本地函数 */
/********************************************************************/
void uartRxCB( uint8 port, uint8 event );
static void sendReport(void);
static uint8 readTemp(void);
static uint8 readVoltage(void);
/********************************************************************/



/*********************************************************************
 * 函数名称：zb_HandleOsalEvent
 * 功    能：操作系统事件处理函数。
 * 入口参数：event  由OSAL分配的事件event。该位掩码包含了被设置的事件。
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void zb_HandleOsalEvent( uint16 event )
{
  if( event & ZB_ENTRY_EVENT )
  { 
    /* 闪烁LED1 来指示启动/加入一个网络 */
    HalLedBlink ( HAL_LED_1, 0, 50, 250 );
     
    /* 启动设备 */  
    zb_StartRequest();
  }
  
  if ( event & MY_REPORT_EVT )
  {
    if ( appState == APP_REPORT ) 
    {
      HAL_TOGGLE_LED4();
      sendReport(); // 发送报告
      osal_start_timerEx( sapi_TaskID, MY_REPORT_EVT, myReportPeriod );
    }
  }
  
  if ( event & MY_FIND_COLLECTOR_EVT )
  {
    /* 删除原来的绑定 */
    if ( appState==APP_REPORT ) 
    {
      zb_BindDevice( FALSE, SENSOR_REPORT_CMD_ID, (uint8 *)NULL );
    }
    
    appState = APP_BIND;
    
    // 闪烁LED2(红色)来指示发现和绑定
    HalLedBlink ( HAL_LED_2, 0, 50, 500 );
    
    /* 查找并绑定到一个采集设备 */
    zb_BindDevice( TRUE, SENSOR_REPORT_CMD_ID, (uint8 *)NULL );
    //zb_BindDevice( TRUE, DUMMY_REPORT_CMD_ID, (uint8 *)NULL );   // ****************
   osal_start_timerEx( sapi_TaskID, MY_REPORT_EVT,myReportPeriod+1000 );
    reportState = TRUE;
  }
}


/*********************************************************************
 * 函数名称：zb_HandleKeys
 * 功    能：按键事件处理函数。
 * 入口参数：shift  若为真，则启用按键的第二功能。本函数不关心该参数。
 *           keys   按键事件的掩码。对于本应用，合法的按键为：
 *                  HAL_KEY_SW_3     SK-SmartRF05BB的DOWN键
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void zb_HandleKeys( uint8 shift, uint8 keys )
{
  if ( keys & HAL_KEY_SW_3 )
  {
    /* 开始报告 */
    osal_set_event( sapi_TaskID, MY_REPORT_EVT );
    reportState = TRUE;
  } 
}


/*********************************************************************
 * 函数名称：zb_StartConfirm
 * 功    能：启动确认函数。本函数在开启一个设备操作请求后被ZigBee
 *           协议栈所调用。
 * 入口参数：status  启动操作的状态值。如果值为ZB_SUCCESS，指示启动
 *           设备操作成功完成。如果不是，则延时myStartRetryDelay后
 *           向操作系统发起MY_START_EVT事件。
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void zb_StartConfirm( uint8 status )
{
  /* 如果设备成功启动，改变应用状态 */
  if ( status == ZB_SUCCESS ) 
  {
    appState = APP_START;
    
    /* 点亮LED1来指示节点已经启动 */
    HalLedSet( HAL_LED_1, HAL_LED_MODE_ON );
    
    /* 存储父节点短地址 */
    zb_GetDeviceInfo(ZB_INFO_PARENT_SHORT_ADDR, &parentShortAddr);
    
    /* 设置事件绑定到采集节点 */
    osal_set_event( sapi_TaskID, MY_FIND_COLLECTOR_EVT );  
  }
}


/*********************************************************************
 * 函数名称：zb_SendDataConfirm
 * 功    能：发送数据确认函数。本函数在发送数据操作请求后被ZigBee协议栈
 *           所调用。
 * 入口参数：handle  数据传输识别句柄。
 *           status  操作状态。 
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void zb_SendDataConfirm( uint8 handle, uint8 status )
{
  if(status != ZB_SUCCESS) 
  {
    /* 如果发送数据不成功，则增加发送失败计数器，并与设定值比较 */
    if ( ++reportFailureNr >= REPORT_FAILURE_LIMIT ) 
    {
       /* 停止报告事件 */
       osal_stop_timerEx( sapi_TaskID, MY_REPORT_EVT );
       
       /* 失败报告后设备绑定到新的父节点上将自动启动*/
       reportState=TRUE; // 该语句作用为，类似按键触发报告事件
        
       /* 尝试绑定到新的父节点 */
       osal_set_event( sapi_TaskID, MY_FIND_COLLECTOR_EVT );
       reportFailureNr=0;
    }
  }
  else // 操作成功 status == SUCCESS
  {
    /* 清零失败计数器 */
    reportFailureNr=0;
  }
}


/*********************************************************************
 * 函数名称：zb_BindConfirm
 * 功    能：绑定确认函数。本函数在绑定操作请求后被ZigBee协议栈所调用。
 * 入口参数：commandId  绑定确定后的命令ID。
 *           status     操作状态。 
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void zb_BindConfirm( uint16 commandId, uint8 status )
{
  if( status == ZB_SUCCESS )
  {   
    appState = APP_REPORT; // 绑定成功
    HalLedSet( HAL_LED_2, HAL_LED_MODE_ON );
    
    if ( reportState ) 
    {
      /* 开始报告事件 */
      osal_set_event( sapi_TaskID, MY_REPORT_EVT );
    }
  }
  else  // 如果绑定不成功，则继续寻找采集节点
  {
    osal_start_timerEx( sapi_TaskID, MY_FIND_COLLECTOR_EVT, myBindRetryDelay );
  }
}


/*********************************************************************
 * 函数名称：zb_AllowBindConfirm
 * 功    能：允许绑定确认函数。指示另外一个设备试图绑定到该设备。
 * 入口参数：source   绑定源地址。 
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void zb_AllowBindConfirm( uint16 source )
{
}


/*********************************************************************
 * 函数名称：zb_FindDeviceConfirm
 * 功    能：查找设备确认函数。本函数在查找设备操作请求后被ZigBee
 *           协议栈所调用。
 * 入口参数：searchType  查找类型。 
 *           searchKey   查找值
 *           result      查找结果
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void zb_FindDeviceConfirm( uint8 searchType, uint8 *searchKey, uint8 *result )
{
}


/*********************************************************************
 * 函数名称：zb_ReceiveDataIndication
 * 功    能：接收数据处理函数。本函数被ZigBee协议栈调用，用来通知应用
 *           程序数据已经从各个设备中接收到。
 *           协议栈所调用。
 * 入口参数：source   接收源地址。 
 *           command  命令值。
 *           len      数据长度。
 *           pData    设备发送过来的数据串。
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void zb_ReceiveDataIndication( uint16 source, uint16 command, uint16 len, uint8 *pData  )
{
}


/*********************************************************************
 * 函数名称：sendReport
 * 功    能：发送报告函数。
 * 入口参数：无
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
#define SENSOR_TEMP_OFFSET                0
#define SENSOR_VOLTAGE_OFFSET             1
#define SENSOR_PARENT_OFFSET              2
#define SENSOR_REPORT_LENGTH              4

static void sendReport(void)
{
  uint8 pData[SENSOR_REPORT_LENGTH];
  static uint8 reportNr=0;
  uint8 txOptions;
  
  /* 读取并报告温度值 */
  pData[SENSOR_TEMP_OFFSET] =  readTemp();
  
  /* 读取并报告电压值 */
  pData[SENSOR_VOLTAGE_OFFSET] = readVoltage(); 
    
  /* 获取父节点短地址 */
  pData[SENSOR_PARENT_OFFSET] =  HI_UINT16(parentShortAddr);
  pData[SENSOR_PARENT_OFFSET + 1] =  LO_UINT16(parentShortAddr);
  
  /* 在每个ACK_INTERVAL报告后发送ACK请求 */
  /* 如果报告发送失败，那么在将下个报告中发送ACK请求 */
  if ( ++reportNr<ACK_REQ_INTERVAL && reportFailureNr==0 ) 
  {
    txOptions = AF_TX_OPTIONS_NONE;
  }
  else 
  {
    txOptions = AF_MSG_ACK_REQUEST;
    reportNr = 0;
  }

  /* 本实验基于绑定方式传播数据,目的地址为 0xFFFE */
  /* 为命令ID建立绑定 */
  
  zb_SendDataRequest( 0xFFFE, SENSOR_REPORT_CMD_ID, SENSOR_REPORT_LENGTH, pData, 0, txOptions, 0 );
  //zb_SendDataRequest( 0xFFFE, DUMMY_REPORT_CMD_ID, SENSOR_REPORT_LENGTH, pData, 0, txOptions, 0 );  //********
}


/* 宏定义 */
/********************************************************************/
/*
   CC253x片内温度传感器温度换算系数，
   需要校准，不是很准确
 */
#define ADCVALUE_AT_TEMP_ZERO     1024     // 摄氏零度对应的ADC值
#define TEMP_COEFFICIENT          20       // 系数
/********************************************************************/
/*********************************************************************
 * 函数名称：readTemp
 * 功    能：读取ADC传感温度值函数。
 * 入口参数：无
 * 出口参数：无
 * 返 回 值：(tt / TEMP_COEFFICIENT)  温度值
 ********************************************************************/
static uint8 readTemp(void)
{
  volatile unsigned char tmp,n;
  signed short adcvalue;
  float tt;

  /*
     系统上电复位后，第一次从ADC读取的转换值总被认为是GND电平，我们执行下面的代
     码来绕过这个BUG。
   */
/************************************************************/
  /* 读取ADCL，ADCH来清零ADCCON1.EOC*/
  tmp = ADCL;     
  tmp = ADCH;
  /* 进行两次单次采样以绕过BUG */
  for(n=0;n<2;n++)
  {  
    /* 设置基准电压、抽取率和单端输入通道 */
    ADCCON3 = ((0x02 << 6) |  // 采用AVDD5引脚上的电压为基准电压
               (0x00 << 4) |  // 抽取率为64，相应的有效位为7位(最高位为符号位)
               (0x0C << 0));  // 选择单端输入通道12(GND)
  
    /* 等待转换完成 */
    while ((ADCCON1 & 0x80) != 0x80);

    /* 读取ADCL，ADCH来清零ADCCON1.EOC */
    tmp = ADCL;     
    tmp = ADCH;  
  }
/************************************************************/

  /* 连接并使能片内温度传感器 */
  TR0 |= 0x01;    // 连接片内温度传感器输出到片内ADC的输入通道14
  ATEST |= 0x01;  // 使能片内温度传感器
  ADCIF = 0;      // 清ADC中断标志位
  
  /* 设置基准电压、抽取率和单端输入通道 */
  ADCCON3 = ((0x00 << 6) |  // 采用内部基准电压1.15V
             (0x03 << 4) |  // 抽取率为512，相应的有效位为12位(最高位为符号位)
             (0x0E << 0));  // 选择单端输入通道14(片内温度传感器输出)

  /* 等待转换完成 */
  while ((ADCCON1 & 0x80) != 0x80);

  /* 从ADCL，ADCH读取转换值，此操作还清零ADCCON1.EOC */
  adcvalue = (signed short)ADCL;
  adcvalue |= (signed short)(ADCH << 8); 

  /* 若adcvalue小于0，就认为它为0 */
  if(adcvalue < 0) 
    adcvalue = 0;
    
  adcvalue >>= 4;  // 取出12位有效位
    
  /* 限制最低温度显示为0摄氏度 */
  if(adcvalue < ADCVALUE_AT_TEMP_ZERO)
    tt = 0;
    
  tt = adcvalue - ADCVALUE_AT_TEMP_ZERO;
    
  /* 限制最高温度显示为99摄氏度 */
  if(tt > (TEMP_COEFFICIENT * 99))
    tt = TEMP_COEFFICIENT * 99;
       
  ATEST = 0x00;  // 禁止片内温度传感器
  TR0 = 0x00;    // 返回到正常操作状态
  
  /* 换算出的温度值 */
  return (uint8)(tt / TEMP_COEFFICIENT);   // 返回温度值
}


/*********************************************************************
 * 函数名称：readVoltage
 * 功    能：读取ADC传感电压值函数。
 * 入口参数：无
 * 出口参数：无
 * 返 回 值：voltagevalue  电压值
 ********************************************************************/
static uint8 readVoltage(void)
{
  volatile unsigned char tmp,n;
  signed short adcvalue;
  float voltagevalue;

  /*
     系统上电复位后，第一次从ADC读取的转换值总被认为是GND电平，执行
     下面的代码来绕过这个BUG。
   */
/************************************************************/
  /* 读取ADCL，ADCH来清零ADCCON1.EOC*/
  tmp = ADCL;     
  tmp = ADCH;
  /* 进行两次单次采样以绕过BUG */
  for(n=0;n<2;n++)
  {  
    /* 设置基准电压、抽取率和单端输入通道 */
    ADCCON3 = ((0x02 << 6) |  // 采用AVDD5引脚上的电压为基准电压
               (0x00 << 4) |  // 抽取率为64，相应的有效位为7位(最高位为符号位)
               (0x0C << 0));  // 选择单端输入通道12(GND)
  
    /* 等待转换完成 */
    while ((ADCCON1 & 0x80) != 0x80);

    /* 读取ADCL，ADCH来清零ADCCON1.EOC */
    tmp = ADCL;     
    tmp = ADCH;  
  }
/************************************************************/

  /* 设置基准电压、抽取率和单端输入通道 */
  ADCCON3 = ((0x00 << 6) |  // 采用内部基准电压1.15V
             (0x03 << 4) |  // 抽取率为512，相应的有效位为12位(最高位为符号位)
             (0x0F << 0));  // 选择单端输入通道15(VDD/3)
  
  /* 等待转换完成 */
  while ((ADCCON1 & 0x80) != 0x80);

  /* 从ADCL，ADCH读取转换值，此操作还清零ADCCON1.EOC */
  adcvalue = (signed short)ADCL;
  adcvalue |= (signed short)(ADCH << 8); 

  /* 若adcvalue小于0，就认为它为0 */
  if(adcvalue < 0) 
    adcvalue = 0;
    
  adcvalue >>= 4;  // 取出12位有效位
    
  /* 将转换值换算为实际电压值 */
  voltagevalue = ((adcvalue * 1.15) / 2047);  // 2047是模拟输入达到VREF时的满量程值
                                              // 由于有效位是12位(最高位为符号位)，
                                              // 所以正的满量程值为2047
                                              // 此处，VREF = 1.15V(内部基准电压)

  /* 转换为实际供电电压 */
  voltagevalue *= 3; 
  voltagevalue *= 10;  // 电压值放大10倍，便于上位机软件处理

  return (uint8)voltagevalue;
}


/******************************************************************************
 * @fn          uartRxCB
 *
 * @brief       Callback function for UART 
 *
 * @param       port - UART port
 *              event - UART event that caused callback 
 *
 * @return      none
 */
void uartRxCB( uint8 port, uint8 event )
{
  uint8 pBuf[RX_BUF_LEN];
  uint16 cmd;
  uint16 len;
  //EA=0;
  if ( event != HAL_UART_TX_EMPTY ) 
  {
  
    // Read from UART
    len = HalUARTRead( HAL_UART_PORT_0, pBuf, RX_BUF_LEN );
    
    if ( len>0 ) 
    {
      
       //HalUARTWrite(HAL_UART_PORT_0, pBuf, len);
      
      cmd = BUILD_UINT16(pBuf[SYS_PING_CMD_OFFSET+ 1], pBuf[SYS_PING_CMD_OFFSET]);
  
      if( (pBuf[FRAME_SOF_OFFSET] == CPT_SOP) && (cmd == SYS_PING_REQUEST) ) 
      {
        //sysPingReqRcvd();
      }
      // 参数设置
      configset(pBuf,len, LOG_TYPE); 
      
    }
  }
  // EA=1;
}
