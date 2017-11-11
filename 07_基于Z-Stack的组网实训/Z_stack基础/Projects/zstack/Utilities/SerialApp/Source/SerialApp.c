/*******************************************************************************
 * 文件名称：SerialApp.c
 * 功    能：SerialApp实验。
 * 实验内容：本实验主要向用户演示基于ZigBee的无线数据透明传输系统。用ZigBee无线
 *           数传模块替代串口连接线。PC通过RS232串行接口发送数据给ZigBee无线数
 *           传模块，模块接收数据后通过射频收发器将数据发送给另一端的ZigBee无线
 *           数传模块，该模块通过射频收发器接收数据后直接发送到RS232串行接口供PC
 *           机接收。
 * 实验设备：SK-SmartRF05EB（装配有SK-CC2530EM）
 *           LEFT键：     发送“匹配描述符请求”
 *           RIGHT键：    发送“终端设备绑定请求”         
 *           LED_Y(黄色)：当成功建立网络或加入网络后此LED点亮
 *           LED_B(蓝色)：当与另一设备成功建立应用层的逻辑连接后此LED点亮
 *           
 *           我们对两个设备在应用层建立逻辑链接做个具体说明，假设A和B是网络中的
 *           两个设备，我们希望在它们的应用层建立逻辑链接，可以采用下面两种方法：
 *           （1）“匹配描述符请求”
 *           按下设备A上的LEFT键，开始在应用层建立逻辑链接，如果成功则设备A上的
 *           LED_B(蓝色)点亮，这时我们就建立了单向的A->B的逻辑链接。此后，设备A
 *           可以向设备B发送信息。
 *           按下设备B上的LEFT键，开始在应用层建立逻辑链接，如果成功则设备B上的
 *           LED_B(蓝色)点亮，这时我们就建立了单向的B->A的逻辑链接。此后，设备B
 *           可以向设备A发送信息。
 *　　　　　 经过前面两步的操作，设备A与设备B之间就建立好了应用层的逻辑链接，之
 *           后它们可以互发信息。
 *           （2）“终端设备绑定请求”
 *            在APS_DEFAULT_MAXBINDING_TIME规定时间内（默认为16秒）分别按下设备
 *            A和设备B上的RIGHT键，将建立设备A与设备B之间的绑定，若成功则设备A和
 *            设备B上的LED_B(蓝色)点亮。此时设备A与设备B之间就建立好了应用层的逻
 *            辑链接，之后它们可以互发信息。若没有成功（例如超过规定时间），则设
 *            备上的LED_B(蓝色)闪烁，表示绑定没有成功。
 ******************************************************************************/


/* 包含头文件 */
/********************************************************************/
#include "AF.h"
#include "OnBoard.h"
#include "OSAL_Tasks.h"
#include "SerialApp.h"
#include "ZDApp.h"
#include "ZDObject.h"
#include "ZDProfile.h"

#include "hal_drivers.h"
#include "hal_key.h"
#if defined ( LCD_SUPPORTED )
  #include "hal_lcd.h"
#endif
#include "hal_led.h"
#include "hal_uart.h"
/********************************************************************/


//#define SERIAL_APP_LOOPBACK  TRUE  // 若使用串口环回测试请去掉本行开头的"//"


/* 宏 */
/* 
   缓冲区处理
   请认真阅读对在下面声明otaBuf变量时的重要注释，对otaBuf变量的操作我
   们强制使用该宏而不直接使用osal_mem_free()函数。
 */
/********************************************************************/
#define FREE_OTABUF() { \
  if ( otaBuf ) \
  { \
    osal_mem_free( otaBuf ); \
  } \
  if ( otaBuf2 ) \
  { \
    SerialApp_SendData( otaBuf2, otaLen2 ); \
    otaBuf2 = NULL; \
  } \
  else \
  { \
    otaBuf = NULL; \
  } \
}
/********************************************************************/



/* 常量 */
/********************************************************************/
/*
   串口号
   若用户没有定义SERIAL_APP_PORT项，则默认定义SERIAL_APP_PORT的值为0。
 */
#if !defined( SERIAL_APP_PORT )
  #define SERIAL_APP_PORT  0
#endif


/*
   串口通信波特率
   若用户没有定义SERIAL_APP_BAUD项，则默认定义SERIAL_APP_BAUD为HAL_UART_BR_38400，
   即默认使用的波特率为38400bps。对于CC2430,只支持38400bps和115200bps。
 */
#if !defined( SERIAL_APP_BAUD )
  #define SERIAL_APP_BAUD  HAL_UART_BR_38400
  //#define SERIAL_APP_BAUD  HAL_UART_BR_115200
#endif


/*
   接收缓冲区阈值
   若用户没有定义SERIAL_APP_THRESH，则默认定义SERIAL_APP_THRESH的值为48，
   当接收缓冲区空间少于该阈值时，调用串口接收回调函数rxCB()。
 */
#if !defined( SERIAL_APP_THRESH )
  #define SERIAL_APP_THRESH  48
#endif


/*
   串口每一次最多可以接收的数据量（以字节为单位）
   若用户没有定义SERIAL_APP_RX_MAX，则当使用DMA方式时默认定义SERIAL_APP_RX_MAX的
   值为128，不使用DMA方式时默认定义该值为64。
 */
#if !defined( SERIAL_APP_RX_MAX )
  #if (defined( HAL_UART_DMA )) && HAL_UART_DMA
    #define SERIAL_APP_RX_MAX  128
  #else
    #define SERIAL_APP_RX_MAX  64
  #endif
#endif


/*
   串口每一次最多可以发送的数据量（以字节为单位）
   若用户没有定义SERIAL_APP_TX_MAX，则当使用DMA方式时默认定义SERIAL_APP_TX_MAX的
   值为128，不使用DMA方式时默认定义该值为64。
 */
#if !defined( SERIAL_APP_TX_MAX )
  #if (defined( HAL_UART_DMA )) && HAL_UART_DMA
  #define SERIAL_APP_TX_MAX  128
  #else
    #define SERIAL_APP_TX_MAX  64
  #endif
#endif


/*
   一个字节被接收到后在调用rxCB()回调函数之前的空闲时间（以毫秒为单位）
   若用户没有定义SERIAL_APP_IDLE，则默认定义SERIAL_APP_IDLE的值为6。
 */
#if !defined( SERIAL_APP_IDLE )
  #define SERIAL_APP_IDLE  6
#endif


/*
   每个OTA信息（无线收发的信息）的应用数据部分的最大数据量（以字节为单位）
   若用户没有定义SERIAL_APP_RX_CNT，则当使用DMA方式时默认定义SERIAL_APP_RX_CNT的
   值为80，不使用DMA方式时默认定义该值为6。
 */
#if !defined( SERIAL_APP_RX_CNT )
  #if (defined( HAL_UART_DMA )) && HAL_UART_DMA
    #define SERIAL_APP_RX_CNT  80
  #else
    #define SERIAL_APP_RX_CNT  6
  #endif
#endif


/*
   环回测试使能
   环回测试是指用户PC通过串口发数据给CC2430，CC2430收到后通过串口将数据再传回给
   用户PC。环回测试与无线数传应用无关，它仅作为测试用户PC与CC2430进行串口通信是
   否成功的检查工具。
   若用户没有定义SERIAL_APP_LOOPBACK，则默认定义SERIAL_APP_LOOPBACK为FALSE，即不
   使能环回测试。
 */
#if !defined( SERIAL_APP_LOOPBACK )
  #define SERIAL_APP_LOOPBACK  FALSE
#endif


/*
   环回测试所使用的事件及超时时间
   若使能了环回测试，则定义环回测试所使用的事件及超时时间
 */
#if SERIAL_APP_LOOPBACK
  #define SERIALAPP_TX_RTRY_EVT      0x0010  // 串口重发送事件
  #define SERIALAPP_TX_RTRY_TIMEOUT  250     // 串口重发发超时时间
#endif


/*
   响应信息的数据量
   若用户没有定义SERIAL_APP_RSP_CNT，则默认定义SERIAL_APP_RSP_CNT的值为4。
 */
#define SERIAL_APP_RSP_CNT  4


/* 
   簇ID列表 
  该列表应该用本应用专用的簇ID来填充
*/

const cId_t SerialApp_ClusterList[SERIALAPP_MAX_CLUSTERS] =
{
  SERIALAPP_CLUSTERID1,  // 发送应用数据
  SERIALAPP_CLUSTERID2   // 发送响应信息
};


/* 
   设备的简单描述符 
 */
const SimpleDescriptionFormat_t SerialApp_SimpleDesc =
{
  SERIALAPP_ENDPOINT,              //  端点号
  SERIALAPP_PROFID,                //  Profile ID
  SERIALAPP_DEVICEID,              //  设备ID
  SERIALAPP_DEVICE_VERSION,        //  设备版本
  SERIALAPP_FLAGS,                 //  标识
  SERIALAPP_MAX_CLUSTERS,          //  输入簇的数量
  (cId_t *)SerialApp_ClusterList,  //  输入簇列表
  SERIALAPP_MAX_CLUSTERS,          //  输出簇的数量
  (cId_t *)SerialApp_ClusterList   //  输出簇列表
};


/* 
   端点描述符 
*/
const endPointDesc_t SerialApp_epDesc =
{
  SERIALAPP_ENDPOINT,
 &SerialApp_TaskID,
  (SimpleDescriptionFormat_t *)&SerialApp_SimpleDesc,
  noLatencyReqs
};
/********************************************************************/



/* 全局变量 */
/********************************************************************/
/*
   本应用的任务ID变量。当SerialApp_Init()函数被调用时，   该变量可以
   获得任务ID值。
*/
uint8 SerialApp_TaskID;    

/*
   传输序号
 */
static uint8 SerialApp_MsgID;

/*
   发送信息的目的地址
 */
static afAddrType_t SerialApp_DstAddr;

/*
   发送序号
 */
static uint8 SerialApp_SeqTx;

/*
   定义两个指向缓冲区的指针otaBuf，otaBuf2
   这两个指针所指向的缓冲区需要由OSAL动态分配。对于本应用，释放这两个指针所指向
   的缓冲区空间不要使用osal_mem_free()函数，应该使用本文件开始处定义的宏
   FREE_OTABUF()
 */
static uint8 *otaBuf;
static uint8 *otaBuf2;
static uint8 otaLen;
static uint8 otaLen2;

/*
   重传次数
 */
static uint8 rtryCnt;

/*
   接收序号
 */
static uint8 SerialApp_SeqRx;

/*
   响应信息的目的地址
 */
static afAddrType_t SerialApp_RspDstAddr;

/*
   存放响应信息的缓冲区
 */
static uint8 rspBuf[ SERIAL_APP_RSP_CNT ];

/*
   环回测试所使用的缓冲区及变量
   若使能了环回测试，则定义环回测试所使用的缓冲区及变量
 */
#if SERIAL_APP_LOOPBACK
static uint8 rxLen;  // 接收缓冲区中的数据长度
static uint8 rxBuf[SERIAL_APP_RX_CNT];  // 接收缓冲区
#endif
/********************************************************************/


/* 本地函数 */
/********************************************************************/
static void SerialApp_ProcessZDOMsgs( zdoIncomingMsg_t *inMsg );
static void SerialApp_HandleKeys( uint8 shift, uint8 keys );
static void SerialApp_ProcessMSGCmd( afIncomingMSGPacket_t *pkt );
static void SerialApp_SendData( uint8 *buf, uint8 len );

#if SERIAL_APP_LOOPBACK
static void rxCB_Loopback( uint8 port, uint8 event );
#else
static void rxCB( uint8 port, uint8 event );
#endif
/********************************************************************/



/*********************************************************************
 * 函数名称：SerialApp_Init
 * 功    能：SerialApp的初始化函数。
 * 入口参数：task_id  由OSAL分配的任务ID。该ID被用来发送消息和设定定时
 *           器。
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void SerialApp_Init( uint8 task_id )
{
  halUARTCfg_t uartConfig;     // 定义串口配置结构体变量

  SerialApp_MsgID = 0x00;      // 初始化传输序号
  SerialApp_SeqRx = 0xC3;      // 初始化接收序号为十进制195
  SerialApp_TaskID = task_id;  // 获取应用任务ID

  /* 初始化发送信息目的地址 */
  SerialApp_DstAddr.endPoint = 0;
  SerialApp_DstAddr.addr.shortAddr = 0;
  SerialApp_DstAddr.addrMode = (afAddrMode_t)AddrNotPresent;
  
  /* 初始化响应信息的目的地址 */
  SerialApp_RspDstAddr.endPoint = 0;
  SerialApp_RspDstAddr.addr.shortAddr = 0;
  SerialApp_RspDstAddr.addrMode = (afAddrMode_t)AddrNotPresent;

  /* 注册端点描述符 */
  afRegister( (endPointDesc_t *)&SerialApp_epDesc );

  /* 注册按键事件，将所有按键事件发送给本应用任务SerialApp_TaskID */
  RegisterForKeys( task_id );

  /* 串口初始化 */
  uartConfig.configured           = TRUE;              
  uartConfig.baudRate             = SERIAL_APP_BAUD;   // 波特率
  uartConfig.flowControl          = TRUE;              // 流控使能
  uartConfig.flowControlThreshold = SERIAL_APP_THRESH; // 流控阈值
  uartConfig.rx.maxBufSize        = SERIAL_APP_RX_MAX; // 最大接收量
  uartConfig.tx.maxBufSize        = SERIAL_APP_TX_MAX; // 最大发送量
  uartConfig.idleTimeout          = SERIAL_APP_IDLE;   // 空闲时间
  uartConfig.intEnable            = TRUE;              // 中断使能
/* 若使能了环回测试功能 */
#if SERIAL_APP_LOOPBACK
  uartConfig.callBackFunc         = rxCB_Loopback;     // 回调函数
/* 若未使能环回测试功能 */
#else
  uartConfig.callBackFunc         = rxCB;              // 回调函数
#endif
  HalUARTOpen (SERIAL_APP_PORT, &uartConfig);          // 打开串口

  /* 若包含了LCD_SUPPORTED编译选项，则在LCD上进行相应的显示 */
#if defined ( LCD_SUPPORTED )
#if defined ( ZIGBEEPRO )
  HalLcdWriteString( "SerialApp(ZigBeePRO)", HAL_LCD_LINE_2 );
#else
  HalLcdWriteString( "SerialApp(ZigBee2007)", HAL_LCD_LINE_2 );
#endif
#endif

  /* ZDO信息注册 */
  /* 注册ZDO的簇End_Device_Bind_rsp，将收到的End_Device_Bind_rsp事件
     发送给本应用任务SerialApp_TaskID
   */    
  ZDO_RegisterForZDOMsg( SerialApp_TaskID, End_Device_Bind_rsp );
  
  /* ZDO信息注册 */
  /* 注册ZDO的簇Match_Desc_rsp，将收到的Match_Desc_rsp事件发送给本应
     用任务SerialApp_TaskID
   */  
  ZDO_RegisterForZDOMsg( SerialApp_TaskID, Match_Desc_rsp );
}


/*********************************************************************
 * 函数名称：SerialApp_ProcessEvent
 * 功    能：SerialApp的任务事件处理函数。
 * 入口参数：task_id  由OSAL分配的任务ID。
 *           events   准备处理的事件。该变量是一个位图，可包含多个事件。
 * 出口参数：无
 * 返 回 值：尚未处理的事件。
 ********************************************************************/
UINT16 SerialApp_ProcessEvent( uint8 task_id, UINT16 events )
{
  /* 系统消息事件 */
  if ( events & SYS_EVENT_MSG )
  {
    afIncomingMSGPacket_t *MSGpkt;

    while ( (MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive(
                                                          SerialApp_TaskID )))
    {
      switch ( MSGpkt->hdr.event )
      {
        /* ZDO信息输入事件 */
        case ZDO_CB_MSG:
          // 调用ZDO信息输入事件处理函数
          SerialApp_ProcessZDOMsgs( (zdoIncomingMsg_t *)MSGpkt );
          break;
        
        /* 按键事件 */  
        case KEY_CHANGE:
          // 调用按键事件处理函数
          SerialApp_HandleKeys( ((keyChange_t *)MSGpkt)->state,
                                ((keyChange_t *)MSGpkt)->keys );
          break;
        
        /* AF输入信息事件 */
        case AF_INCOMING_MSG_CMD:
          // 调用输入信息事件处理函数
          SerialApp_ProcessMSGCmd( MSGpkt );
          break;
  
        default:
          break;
      }

      osal_msg_deallocate( (uint8 *)MSGpkt );  // 释放存储器
    }

    return ( events ^ SYS_EVENT_MSG );  // 返回未处理的事件
  }

  /* 发送数据事件 */
  if ( events & SERIALAPP_MSG_SEND_EVT )
  {
    SerialApp_SendData( otaBuf, otaLen );  // 调用发送数据处理函数

    return ( events ^ SERIALAPP_MSG_SEND_EVT );
  }

  /* 发送数据重传事件 */
  if ( events & SERIALAPP_MSG_RTRY_EVT )
  {
    /* 若重传计数不为0 */
    if ( --rtryCnt )
    {
      /* 发送OTA信息（需要重传的发送数据） */
      AF_DataRequest( &SerialApp_DstAddr,
                      (endPointDesc_t *)&SerialApp_epDesc,
                       SERIALAPP_CLUSTERID1, otaLen, otaBuf,
                      &SerialApp_MsgID, 0, AF_DEFAULT_RADIUS );
      
      /* 在指定时间SERIALAPP_MSG_RTRY_TIMEOUT到时后触发发送数据重传事件 */
      osal_start_timerEx( SerialApp_TaskID, SERIALAPP_MSG_RTRY_EVT,
                                            SERIALAPP_MSG_RTRY_TIMEOUT );
    }
    else
    {
      FREE_OTABUF();  // 处理缓冲区
    }

    return ( events ^ SERIALAPP_MSG_RTRY_EVT );
  }

  /* 响应信息重传事件 */
  if ( events & SERIALAPP_RSP_RTRY_EVT )
  {
    /* 发送OTA信息（需要重传的响应信息）*/
    afStatus_t stat = AF_DataRequest( &SerialApp_RspDstAddr,
                                      (endPointDesc_t *)&SerialApp_epDesc,
                                       SERIALAPP_CLUSTERID2,
                                       SERIAL_APP_RSP_CNT, rspBuf,
                                      &SerialApp_MsgID, 0, AF_DEFAULT_RADIUS );

    /* 若发送OTA信息（需要重传的响应信息）不成功*/
    if ( stat != afStatus_SUCCESS )
    {
      /* 在指定时间SERIALAPP_RSP_RTRY_TIMEOUT到时后触发响应信息重传事件 */
      osal_start_timerEx( SerialApp_TaskID, SERIALAPP_RSP_RTRY_EVT,
                                            SERIALAPP_RSP_RTRY_TIMEOUT );
    }

    return ( events ^ SERIALAPP_RSP_RTRY_EVT );
  }

  /* 若使能了环回测试 */
#if SERIAL_APP_LOOPBACK
  /* 串口重发送事件 */
  if ( events & SERIALAPP_TX_RTRY_EVT )
  { /* 若接收缓冲区中有数据 */
    if ( rxLen )
    { /* 若将接收缓冲区中的数据写入到串口不成功 */
      if ( !HalUARTWrite( SERIAL_APP_PORT, rxBuf, rxLen ) )
      { /* 在指定时间SERIALAPP_TX_RTRY_TIMEOUT后触发串口重发送事件 */
        osal_start_timerEx( SerialApp_TaskID, SERIALAPP_TX_RTRY_EVT,
                                              SERIALAPP_TX_RTRY_TIMEOUT );
      }
      /* 若将接收缓冲区中的数据写入到串口成功 */
      else
      {
        rxLen = 0;  // 清零接收缓冲区中数据长度变量
      }
    }

    return ( events ^ SERIALAPP_TX_RTRY_EVT );
  }
#endif

  /* 丢弃未知事件 */ 
  return ( 0 ); 
}


/*********************************************************************
 * 函数名称：SerialApp_ProcessZDOMsgs
 * 功    能：SerialApp的ZDO信息输入事件处理函数。
 * 入口参数：inMsg  指向输入的ZDO信息的指针
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void SerialApp_ProcessZDOMsgs( zdoIncomingMsg_t *inMsg )
{
  /* ZDO信息中的簇ID */
  switch ( inMsg->clusterID )
  {
    /* ZDO的簇End_Device_Bind_rsp */
    case End_Device_Bind_rsp:
      /* 若绑定成功 */
      if ( ZDO_ParseBindRsp( inMsg ) == ZSuccess )
      {
        /* 点亮LED4 */
        /* 对于SK-SmartRF05EB而言，LED4是LED_B（蓝色） */
        HalLedSet( HAL_LED_4, HAL_LED_MODE_ON );
      }
/* 若定义了BLINK_LEDS宏 */
#if defined(BLINK_LEDS)
      /* 若绑定失败 */
      else
      {
        /* 闪烁LED4 */
        /* 对于SK-SmartRF05EB而言，LED4是LED_B（蓝色） */
        HalLedSet ( HAL_LED_4, HAL_LED_MODE_FLASH );
      }
#endif
      break;

    /* ZDO的簇Match_Desc_rsp */      
    case Match_Desc_rsp:
      {
        /* 获取指向响应信息的指针 */
        ZDO_ActiveEndpointRsp_t *pRsp = ZDO_ParseEPListRsp( inMsg );
        /* 若指向响应信息的指针变量pRsp的值不为空(NULL) */
        if ( pRsp )
        { /* 指向响应信息的指针变量pRsp的值有效 */
          if ( pRsp->status == ZSuccess && pRsp->cnt )
          {
            /* 设置发送信息的目的地址 */
            SerialApp_DstAddr.addrMode = (afAddrMode_t)Addr16Bit;
            SerialApp_DstAddr.addr.shortAddr = pRsp->nwkAddr;
            // 采用第一个端点，用户可以修改此处代码以搜索所有端点
            SerialApp_DstAddr.endPoint = pRsp->epList[0];
            
            /* 点亮LED4 */
            /* 对于SK-SmartRF05EB而言，LED4是LED_B（蓝色） */
            HalLedSet( HAL_LED_4, HAL_LED_MODE_ON );
          }
          osal_mem_free( pRsp );  // 释放指针变量pRsp所占用的存储器
        }
      }
      break;
  }
}


/*********************************************************************
 * 函数名称：SerialApp_HandleKeys
 * 功    能：SerialApp的按键事件处理函数。
 * 入口参数：shift  若为真，则启用按键的第二功能。本函数不关心该参数。
 *           keys   按键事件的掩码。对于本应用，合法的项目为：
 *                  HAL_KEY_SW_2     SK-SmartRF05EB的RIGHT键
 *                  HAL_KEY_SW_4     SK-SmartRF05EB的LEFT键
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void SerialApp_HandleKeys( uint8 shift, uint8 keys )
{
  zAddrType_t dstAddr;
  (void)shift;
  
  /* HAL_KEY_SW_2被按下，发送“终端设备绑定请求” */
  /* HAL_KEY_SW_2对应SK-SmartRF05EB的RIGHT键 */  
  if ( keys & HAL_KEY_SW_2 )
  {
    /* 熄灭LED4 */
    /* 对于SK-SmartRF05EB而言，LED4是LED_B（蓝色） */     
    HalLedSet ( HAL_LED_4, HAL_LED_MODE_OFF );
      
    /* 为强制端点发送一个“终端设备绑定请求”（单播）*/
    dstAddr.addrMode = Addr16Bit;
    dstAddr.addr.shortAddr = 0x0000; // 目的地址为网络中协调器的16位短地址0x0000
    ZDP_EndDeviceBindReq( &dstAddr, NLME_GetShortAddr(), 
                          SerialApp_epDesc.endPoint,
                          SERIALAPP_PROFID,
                          SERIALAPP_MAX_CLUSTERS, (cId_t *)SerialApp_ClusterList,
                          SERIALAPP_MAX_CLUSTERS, (cId_t *)SerialApp_ClusterList,
                          FALSE );
  }

  /* HAL_KEY_SW_4被按下，发送“匹配描述符请求” */
  /* HAL_KEY_SW_4对应SK-SmartRF05EB的LEFT键 */   
  if ( keys & HAL_KEY_SW_4 )
  {
    /* 熄灭LED4 */
    /* 对于SK-SmartRF05EB而言，LED4是LED_B（蓝色） */
    HalLedSet ( HAL_LED_4, HAL_LED_MODE_OFF );
   
    /* 发送一个“匹配描述符请求”（广播）*/
    dstAddr.addrMode = AddrBroadcast;
    dstAddr.addr.shortAddr = NWK_BROADCAST_SHORTADDR;
    ZDP_MatchDescReq( &dstAddr, NWK_BROADCAST_SHORTADDR,
                      SERIALAPP_PROFID,
                      SERIALAPP_MAX_CLUSTERS, (cId_t *)SerialApp_ClusterList,
                      SERIALAPP_MAX_CLUSTERS, (cId_t *)SerialApp_ClusterList,
                      FALSE );
  }
}


/*********************************************************************
 * 函数名称：SerialApp_ProcessMSGCmd
 * 功    能：SerialApp的输入信息事件处理函数。本函数处理来自其他设备的
 *           任何输入数据。因此，可基于簇ID执行预定的动作。
 * 入口参数：pkt  指向输入信息的指针
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void SerialApp_ProcessMSGCmd( afIncomingMSGPacket_t *pkt )
{
  uint8 stat;
  uint8 seqnb;
  uint8 delay;

  /* AF输入信息中的簇 */
  switch ( pkt->clusterId )
  {
  /* 簇SERIALAPP_CLUSTERID1 */
  /* 发送数据 */
  case SERIALAPP_CLUSTERID1:
    seqnb = pkt->cmd.Data[0];  // 取出发送数据的发送序号

    /* 若接收到的发送数据不是重复数据 */
    if ( (seqnb > SerialApp_SeqRx) ||                    // Normal
        ((seqnb < 0x80 ) && ( SerialApp_SeqRx > 0x80)) ) // Wrap-around
    {
      /* 若将接收到的发送数据成功写入串口 */
      if ( HalUARTWrite( SERIAL_APP_PORT, pkt->cmd.Data+1,
                                         (pkt->cmd.DataLength-1) ) )
      {
        /*
           将刚才发送数据的发送序号作为接收序号以便在下次接收到
           发送数据时作为判断依据
         */
        SerialApp_SeqRx = seqnb;  
        stat = OTA_SUCCESS;  // 状态标志为成功
      }
      /* 若将接收到的发送数据写入串口失败，说明串口忙 */
      else
      {
        stat = OTA_SER_BUSY;  // 状态标志为串口忙 
      }
    }
    /* 若接收到的发送数据是重复数据 */
    else
    {
      stat = OTA_DUP_MSG;  // 状态标志为重复数据
    }

    /* 给OTA信息流控选择一个恰当的延时 */
    delay = (stat == OTA_SER_BUSY) ? SERIALAPP_NAK_DELAY : SERIALAPP_ACK_DELAY;

    /* 构造并发送响应信息 */
    rspBuf[0] = stat;                  // 针对接收到的发送数据给出的处理状态
    rspBuf[1] = seqnb;                 // 确认序号（接收到的发送数据的发送序号）
    rspBuf[2] = LO_UINT16( delay );    // OTA信息流控延时
    rspBuf[3] = HI_UINT16( delay );
    stat = AF_DataRequest( &(pkt->srcAddr), (endPointDesc_t*)&SerialApp_epDesc,
                            SERIALAPP_CLUSTERID2, SERIAL_APP_RSP_CNT , rspBuf,
                           &SerialApp_MsgID, 0, AF_DEFAULT_RADIUS );

    /* 若OTA信息（响应信息）发送不成功 */
    if ( stat != afStatus_SUCCESS )
    {
      /* 在指定时间SERIALAPP_RSP_RTRY_TIMEOUT到时后触发响应信息重传事件 */
      osal_start_timerEx( SerialApp_TaskID, SERIALAPP_RSP_RTRY_EVT,
                                            SERIALAPP_RSP_RTRY_TIMEOUT );

      /* 为响应信息重传存储所需的目的地址 */
      osal_memcpy(&SerialApp_RspDstAddr, &(pkt->srcAddr), sizeof( afAddrType_t ));
    }
    break;

  /* 簇SERIALAPP_CLUSTERID2 */
  /* 响应信息 */
  case SERIALAPP_CLUSTERID2:
    /* 若对方发来的响应信息表明发送数据已被对方成功接收或
       发送数据对于对方来说是重复数据
    */
    if ( (pkt->cmd.Data[1] == SerialApp_SeqTx) &&
        ((pkt->cmd.Data[0] == OTA_SUCCESS) ||
         (pkt->cmd.Data[0] == OTA_DUP_MSG)) )
    {
      /* 停止发送数据重传事件的触发 */
      osal_stop_timerEx( SerialApp_TaskID, SERIALAPP_MSG_RTRY_EVT );
      
      FREE_OTABUF();  // 缓冲区处理
    }
    /* 若对方发来的响应信息表明发送数据未被对方成功接收
       请注意：此处成功接收是指将接收到的数据成功写入串口。
    */
    else
    {
      /* 从对方发来的响应信息中取出对方所要求进行OTA信息流控的延时 */
      delay = BUILD_UINT16( pkt->cmd.Data[2], pkt->cmd.Data[3] );
      
      /* 在指定时间delay到时后触发发送数据重传事件 */
      osal_start_timerEx( SerialApp_TaskID, SERIALAPP_MSG_RTRY_EVT, delay );
    }
    break;

    default:
      break;
  }
}


/*********************************************************************
 * 函数名称：SerialApp_SendData
 * 功    能：SerialApp的发送数据数据函数
 * 入口参数：buf  指向发送数据缓冲区的指针
 *           len  发送数据长度
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void SerialApp_SendData( uint8 *buf, uint8 len )
{
  afStatus_t stat;

  *buf = ++SerialApp_SeqTx;  // 计算发送序号

  otaBuf = buf;   // 用otaBuf指向保存发送数据的缓冲区
  otaLen = len+1; // 由于增加了发送序号这个1字节的字段，所以发送数据长度加1

  /* 发送OTA信息（发送数据）*/
  stat = AF_DataRequest( &SerialApp_DstAddr,
                         (endPointDesc_t *)&SerialApp_epDesc,
                          SERIALAPP_CLUSTERID1,
                          otaLen, otaBuf,
                          &SerialApp_MsgID, 0, AF_DEFAULT_RADIUS );

  /* 若发送数据被成功发送或产生了存储器错误 */
  if ( (stat == afStatus_SUCCESS) || (stat == afStatus_MEM_FAIL) )
  {
    /* 在指定时间SERIALAPP_MSG_RTRY_TIMEOUT到时后触发发送数据重传事件 */
    osal_start_timerEx( SerialApp_TaskID, SERIALAPP_MSG_RTRY_EVT,
                      SERIALAPP_MSG_RTRY_TIMEOUT );
    
    rtryCnt = SERIALAPP_MAX_RETRIES;  // 将最大重传次数赋值给重传次数计数变量
  }
  /* 否则处理缓冲区 */
  else
  {
    FREE_OTABUF();
  }
}


/* 若使能了环回测试功能 */
#if SERIAL_APP_LOOPBACK
/*********************************************************************
 * 函数名称：rxCB_Loopback
 * 功    能：串口接收回调函数（环回测试时使用）
 * 入口参数：port   串口号
 *           event  串口事件
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void rxCB_Loopback( uint8 port, uint8 event )
{
  /* 若接收缓冲区中有数据 */
  if ( rxLen )
  {
    /* 若将接收缓冲区中的数据写入到串口不成功 */
    if ( !HalUARTWrite( SERIAL_APP_PORT, rxBuf, rxLen ) )
    {
      /* 在指定时间SERIALAPP_TX_RTRY_TIMEOUT后触发串口重发送事件 */
      osal_start_timerEx( SerialApp_TaskID, SERIALAPP_TX_RTRY_EVT,
                                            SERIALAPP_TX_RTRY_TIMEOUT );
      return;  // 返回
    }
    /* 若将接收缓冲区中的数据写入到串口成功 */
    else
    { /* 停止串口重发送事件 */
      osal_stop_timerEx( SerialApp_TaskID, SERIALAPP_TX_RTRY_EVT );
    }
  }

  /* 若从串口读取数据不成功(读出的数据长度为0) */
  if ( !(rxLen = HalUARTRead( port, rxBuf, SERIAL_APP_RX_CNT )) )
  {
    return;  // 返回
  }

  /* 若将已从串口读取的数据回写到串口成功 */
  if ( HalUARTWrite( SERIAL_APP_PORT, rxBuf, rxLen ) )
  {
    rxLen = 0;  // 清零接收缓冲区中数据长度变量
  }
  /* 若将已从串口读取的数据回写到串口不成功 */
  else
  { /* 在指定时间SERIALAPP_TX_RTRY_TIMEOUT后触发串口重发送事件 */ 
    osal_start_timerEx( SerialApp_TaskID, SERIALAPP_TX_RTRY_EVT,
                                          SERIALAPP_TX_RTRY_TIMEOUT );
  }
}


/* 若未使能环回测试功能 */
#else
/*********************************************************************
 * 函数名称：rxCB
 * 功    能：串口接收回调函数（正常应用时使用）
 * 入口参数：port   串口号
 *           event  串口事件
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
static void rxCB( uint8 port, uint8 event )
{
  uint8 *buf, len;

  /*
     本应用最多只使用由OSAL动态分配的2个存储器空间来存储待发送的OTA信息，
     这两个存储器空间分别由otaBuf和otaBuf2指针来指向。
     本应用总是将otaBuf指针指向的存储器空间作为无线发送缓冲区，而将otaBuf2指针
     指向的存储器空间作为后备缓冲区。当otaBuf指向的缓冲区中的数据被发送完后，将
     释放该缓冲区空间，若此时otaBuf2不为NULL，说明otaBuf2指向的缓冲区中包含数据，
     那么otaBuf将指向该缓冲区，而otaBuf2将被置为NULL。当otaBuf和otaBuf2都不为NULL，
     时，若此时用户PC通过串口发来待发送的OTA信息，这时软件将不进行处理而是直接跳
     出本函数，这是因为此时两个缓冲区中都有数据，无法再接收来自用户PC串口的数据。
     这样将导致CC2430的UART控制器对用户PC的串口进行硬件流控。
  */
  
  /* 若otaBuf2指针不为NULL */
  if ( otaBuf2 )
  {
    return;  // 返回
  }
  
  /* 若分配存储器空间不成功 */
  if ( !(buf = osal_mem_alloc( SERIAL_APP_RX_CNT )) )
  {
    return;  // 返回
  }

  /* 从串口读出数据存入缓冲区，保留缓冲区第一个字节用来存储发送序号 */
  len = HalUARTRead( port, buf+1, SERIAL_APP_RX_CNT-1 );

  /* 若从串口没读出数据 */
  if ( !len )  
  {
    osal_mem_free( buf );  // 释放已分配的缓冲区
    return;                // 返回
  }

  /* 若otaBuf不为NULL */
  if ( otaBuf )
  {
    otaBuf2 = buf;  // 用otaBuf2指向buf所指向的存储器空间
    otaLen2 = len;  // otaBuf2指向的存储器空间的长度
  }
  /* 若otaBuf为NULL */
  else
  {
    otaBuf = buf;  // 用otaBuf指向buf所指向的存储器空间
    otaLen = len;  // otaBuf指向的存储器空间的长度
    
    /*
       立刻设置发送数据事件
       注意：不要在本回调函数中直接调用SerialApp_SendData()函数来发送OTA信息，
             而是应该使用设置发送数据事件的方法。这样做的目的是为了防止本回调
             函数被长时间阻塞。
     */
    osal_set_event( SerialApp_TaskID, SERIALAPP_MSG_SEND_EVT );
  }
}
#endif


