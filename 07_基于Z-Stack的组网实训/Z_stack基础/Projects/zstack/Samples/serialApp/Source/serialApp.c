/*******************************************************************************
 * 文件名称：GenericApp.c
 * 功    能：GenericApp实验。
 * 实验内容：一个ZigBee网络中的某两个设备通过“终端设备绑定请求”或“匹配描述符
 *           请求”建立应用层的逻辑链接后，以5秒为周期互发"HelloZIGBEE"信息。
 * 实验设备：
          一个PC通过串口连接一个使用本应用实例的ZigBee设备来收发数据，另一个PC通
过串口连接另一个使用本应用实例的ZigBee设备来收发数据， 实现在pc端的串口双向通讯。本实验演示了以zigbee设备来实现无线串口数据传输的方法。

 *           LED_R(红色 D6)：当成功建立网络或加入网络后此LED点亮
 *           LEB_G(绿色 D5)：当与另一设备成功建立应用层的逻辑连接后此LED点亮
*           LED_B(蓝色 D4): 当发送"Hello ZIGBEE"信息时此LED取反
 *          LED_B(蓝色 D3)：：当接收到"Hello World"信息时此LED闪烁
 *           
  ******************************************************************************/


/* 包含头文件 */
/********************************************************************/
#include "OSAL.h"
#include "AF.h"
#include "ZDApp.h"
#include "ZDObject.h"
#include "ZDProfile.h"
#include "serialApp.h"
#include "DebugTrace.h"

#if !defined( WIN32 )
  #include "OnBoard.h"
#endif

/* HAL */
#include "hal_lcd.h"
#include "hal_led.h"
#include "hal_key.h"
#include "hal_uart.h"
/********************************************************************/


/* 簇ID列表 */
/* 该列表应该用本应用专用的簇ID来填充 */
const cId_t GenericApp_ClusterList[GENERICAPP_MAX_CLUSTERS] =
{
  GENERICAPP_CLUSTERID
};


/* 设备的简单描述符 */
const SimpleDescriptionFormat_t GenericApp_SimpleDesc =
{
  GENERICAPP_ENDPOINT,              //  端点号
  GENERICAPP_PROFID,                //  Profile ID
  GENERICAPP_DEVICEID,              //  设备ID
  GENERICAPP_DEVICE_VERSION,        //  设备版本
  GENERICAPP_FLAGS,                 //  标识
  GENERICAPP_MAX_CLUSTERS,          //  输入簇的数量
  (cId_t *)GenericApp_ClusterList,  //  输入簇列表
  GENERICAPP_MAX_CLUSTERS,          //  输出簇的数量
  (cId_t *)GenericApp_ClusterList   //  输出簇列表
};


/* 设备的端点描述符 */
/* 我们在此处定义了设备的端点描述符，但是我们在GenericApp_Init()函数
   中对该描述符进行填充。
   我们也可以在此处用"const"关键字来定义并填充一个端点描述符的结构体
   常量。
   很明显，我们此处采用第一种方式，换句话说，我们在RAM里定义了端点描
   述符。
*/
endPointDesc_t GenericApp_epDesc;


/* 本地变量 */
/********************************************************************/
uint8   rbuf[256], sbuf[256];
/*
   本应用的任务ID变量。当SampleApp_Init()函数被调用时，
   该变量可以获得任务ID值。
*/
byte GenericApp_TaskID;   


/*
   设备的网络状态变量：协调器/路由器/终端设备/设备不在网络中
*/
devStates_t GenericApp_NwkState;


/*
   传输ID（计数器）变量
*/
byte GenericApp_TransID;  


/*
   "Hello World"信息的目的地址结构体变量
*/
afAddrType_t GenericApp_DstAddr;
/********************************************************************/


/* 本地函数 */
/********************************************************************/
void GenericApp_ProcessZDOMsgs( zdoIncomingMsg_t *inMsg );
void GenericApp_HandleKeys( byte shift, byte keys );
void GenericApp_MessageMSGCB( afIncomingMSGPacket_t *pckt );
void GenericApp_SendTheMessage(byte *buf, uint8 len);
uint16  uart_read(uint8 *buf, uint16 maxlen);
/********************************************************************/


/*********************************************************************
 * 函数名称：GenericApp_Init
 * 功    能：GenericApp的初始化函数。
 * 入口参数：task_id  由OSAL分配的任务ID。该ID被用来发送消息和设定定时
 *           器。
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void GenericApp_Init( byte task_id )
{
  GenericApp_TaskID = task_id;
  GenericApp_NwkState = DEV_INIT;
  GenericApp_TransID = 0;

  /* 
     可以在此处添加对设备硬件的初始化代码。 
     若硬件是本应用专用的，可以将对该硬件的初始化代码添加在此处。 
     若硬件不是本应用专用的，可以将初始化代码添加在Zmain.c文件中的main()函数。
  */

  /* 初始化"Hello World"信息的目的地址 */
  GenericApp_DstAddr.addrMode = (afAddrMode_t)AddrNotPresent;
  GenericApp_DstAddr.endPoint = 0;
  GenericApp_DstAddr.addr.shortAddr = 0;

  /* 填充端点描述符 */
  GenericApp_epDesc.endPoint = GENERICAPP_ENDPOINT;
  GenericApp_epDesc.task_id = &GenericApp_TaskID;
  GenericApp_epDesc.simpleDesc
            = (SimpleDescriptionFormat_t *)&GenericApp_SimpleDesc;
  GenericApp_epDesc.latencyReq = noLatencyReqs;

  /* 注册端点描述符 */
  afRegister( &GenericApp_epDesc );

  /* 注册按键事件，将所有按键事件发送给本应用任务GenericApp_TaskID */
  RegisterForKeys( GenericApp_TaskID );

  /* 若包含了LCD_SUPPORTED编译选项，则在LCD上进行相应的显示 */
#if defined ( LCD_SUPPORTED )
#if defined ( ZIGBEEPRO )
    HalLcdWriteString( "GenericApp(ZigBeePRO)", HAL_LCD_LINE_2 );
#else
    HalLcdWriteString( "GenericApp(ZigBee07)", HAL_LCD_LINE_2 );
#endif
#endif
  
  /* ZDO信息注册 */
  /* 注册ZDO的簇End_Device_Bind_rsp，将收到的End_Device_Bind_rsp事件
     发送给本应用任务GenericApp_TaskID
   */
  ZDO_RegisterForZDOMsg( GenericApp_TaskID, End_Device_Bind_rsp );
  
  /* ZDO信息注册 */
  /* 注册ZDO的簇Match_Desc_rsp，将收到的Match_Desc_rsp事件发送给本应
     用任务GenericApp_TaskID
   */  
  ZDO_RegisterForZDOMsg( GenericApp_TaskID, Match_Desc_rsp );
}


/*********************************************************************
 * 函数名称：GenericApp_ProcessEvent
 * 功    能：GenericApp的任务事件处理函数。
 * 入口参数：task_id  由OSAL分配的任务ID。
 *           events   准备处理的事件。该变量是一个位图，可包含多个事件。
 * 出口参数：无
 * 返 回 值：尚未处理的事件。
 ********************************************************************/
UINT16 GenericApp_ProcessEvent( byte task_id, UINT16 events )
{
  afIncomingMSGPacket_t *MSGpkt;
  afDataConfirm_t *afDataConfirm;
  uint16 len;
  /* 定义用于保存AF数据确认信息各字段值的变量 */
  byte sentEP;
  ZStatus_t sentStatus;
  byte sentTransID;       
  (void)task_id;  // 防止编译器报错

  if ( events & SYS_EVENT_MSG )
  {
    MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( GenericApp_TaskID );
    while ( MSGpkt )
    {
      switch ( MSGpkt->hdr.event )
      {
        /* ZDO信息输入事件 */
        case ZDO_CB_MSG:
          // 调用ZDO信息输入事件处理函数
          GenericApp_ProcessZDOMsgs( (zdoIncomingMsg_t *)MSGpkt );
          break;
        
        /* 按键事件 */
        case KEY_CHANGE:
          GenericApp_HandleKeys( ((keyChange_t *)MSGpkt)->state, 
                                 ((keyChange_t *)MSGpkt)->keys );
          break;

        /* AF数据确认事件 */
        case AF_DATA_CONFIRM_CMD:
          // 读取AF数据确认信息并做相应处理
          afDataConfirm = (afDataConfirm_t *)MSGpkt;
          sentEP = afDataConfirm->endpoint;
          sentStatus = afDataConfirm->hdr.status;
          sentTransID = afDataConfirm->transID;
          (void)sentEP;
          (void)sentTransID;

          // 若AF数据确认信息表明刚才发送的数据不成功则做相应处理
          if ( sentStatus != ZSuccess )
          {
            // 用户可在此处添加自己的处理代码
          }
          break;

        /* RF输入信息事件 */ 
        /*接收到ZIGBEE数据*/  
        case AF_INCOMING_MSG_CMD:
          GenericApp_MessageMSGCB( MSGpkt );  // 调用输入信息事件处理函数
          break;

        /* ZDO状态改变事件 */  
        case ZDO_STATE_CHANGE:
          GenericApp_NwkState = (devStates_t)(MSGpkt->hdr.status);// 读取设备状态
          /* 若设备是协调器或路由器或终端  */
          if ( (GenericApp_NwkState == DEV_ZB_COORD)
              || (GenericApp_NwkState == DEV_ROUTER)
              || (GenericApp_NwkState == DEV_END_DEVICE) )
          {
            osal_start_timerEx( GenericApp_TaskID,GENERICAPP_SEND_MSG_EVT, 4000);
            HalLedSet ( HAL_LED_4, HAL_LED_MODE_FLASH );
          }
      }
      osal_msg_deallocate( (uint8 *)MSGpkt );  // 释放存储器

      // 获取下一个系统消息事件
      MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( GenericApp_TaskID );
    }

    return (events ^ SYS_EVENT_MSG);  // 返回未处理的事件
  }

  /* 事件GENERICAPP_SEND_MSG_EVT */
  /* 该事件在前面的"case ZDO_STATE_CHANGE:"代码部分可能被触发 */
  if ( events & GENERICAPP_SEND_MSG_EVT )
  {
        /* 触发发GENERICAPP_READ_UART_EVT信息的事件 */
            osal_start_timerEx( GenericApp_TaskID,GENERICAPP_READ_UART_EVT, 100);
            GenericApp_HandleKeys( 0,HAL_KEY_SW_6 );
      return (events ^ GENERICAPP_SEND_MSG_EVT);  // 返回未处理的事件
  }
   /* 事件GENERICAPP_SEND_MSG_EVT */
  /* 该事件接收串口数据，并通过zigbee发送*/
  if ( events & GENERICAPP_READ_UART_EVT )
  {
      /* 该接收串口数据*/
      len=uart_read(rbuf,256);
    if(len) 
    {   //有串口数据
        osal_memcpy( sbuf, rbuf, len);
 #if defined UARTSEL        
        HalUARTWrite(HAL_UART_PORT_0, sbuf, len);
#endif
        //通过zigbee发送
        GenericApp_SendTheMessage(sbuf,  len);
    }
     /* 再次触发GENERICAPP_READ_UART_EVT */
    osal_start_timerEx( GenericApp_TaskID,GENERICAPP_READ_UART_EVT, 20);
  }
 
  /* 丢弃未知事件 */
  return 0;
}


/*********************************************************************
 * 函数名称：GenericApp_ProcessZDOMsgs
 * 功    能：GenericApp的ZDO信息输入事件处理函数。
 * 入口参数：无
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void GenericApp_ProcessZDOMsgs( zdoIncomingMsg_t *inMsg )
{
  /* ZDO信息中的簇ID */
  switch ( inMsg->clusterID )
  {
    /* ZDO的簇End_Device_Bind_rsp */
    case End_Device_Bind_rsp:
      /* 若绑定成功 */
      if ( ZDO_ParseBindRsp( inMsg ) == ZSuccess )
      {
      
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
            /* 设置"Hello World"信息的目的地址 */
            GenericApp_DstAddr.addrMode = (afAddrMode_t)Addr16Bit;
            GenericApp_DstAddr.addr.shortAddr = pRsp->nwkAddr;
            
            // 采用第一个端点，用户可以修改此处代码以搜索所有端点
            GenericApp_DstAddr.endPoint = pRsp->epList[0];

            /* 点亮LED4 */
            
            HalLedSet( HAL_LED_4, HAL_LED_MODE_ON );
          }
          osal_mem_free( pRsp );  // 释放指针变量pRsp所占用的存储器
        }
      }
      break;
  }
}


/*********************************************************************
 * 函数名称：GenericApp_HandleKeys
 * 功    能：GenericApp的按键事件处理函数。
 * 入口参数：shift  若为真，则启用按键的第二功能。本函数不关心该参数。
 *           keys   按键事件的掩码。对于本应用，合法的项目为：
 *                  HAL_KEY_SW_2     SK-SmartRF05EB的RIGHT键
 *                  HAL_KEY_SW_4     SK-SmartRF05EB的LEFT键
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void GenericApp_HandleKeys( byte shift, byte keys )
{
  zAddrType_t dstAddr;
  (void)shift;
  
  /* HAL_KEY_SW_1被按下，发送“终端设备绑定请求” */
  
  if ( keys & HAL_KEY_SW_6)
  {
#if defined (KEY_TO_UART)
      HalUARTWrite(HAL_UART_PORT_0,"KEY1 PRESS!\r\n",13);
#endif          
      

    /* 为强制端点发送一个“终端设备绑定请求”（单播）*/
    HalLedSet ( HAL_LED_4, HAL_LED_MODE_FLASH );
    dstAddr.addrMode = Addr16Bit;
    dstAddr.addr.shortAddr = 0x0000; // 目的地址为网络中协调器的16位短地址0x0000
    ZDP_EndDeviceBindReq( &dstAddr, NLME_GetShortAddr(), 
                            GenericApp_epDesc.endPoint,
                            GENERICAPP_PROFID,
                            GENERICAPP_MAX_CLUSTERS, (cId_t *)GenericApp_ClusterList,
                            GENERICAPP_MAX_CLUSTERS, (cId_t *)GenericApp_ClusterList,
                            FALSE );

  }

  /* HAL_KEY_SW_2被按下，发送“匹配描述符请求” */
   
  if ( keys &HAL_KEY_SW_7 )
  {
#if defined  (KEY_TO_UART)
      HalUARTWrite(HAL_UART_PORT_0,"KEY2 PRESS!\r\n",13);
#endif   
     
      
 
    /* 发送一个“匹配描述符请求”（广播）*/
    HalLedSet ( HAL_LED_4, HAL_LED_MODE_FLASH );
    dstAddr.addrMode = AddrBroadcast;
    dstAddr.addr.shortAddr = NWK_BROADCAST_SHORTADDR;
    ZDP_MatchDescReq( &dstAddr, NWK_BROADCAST_SHORTADDR,
                        GENERICAPP_PROFID,
                        GENERICAPP_MAX_CLUSTERS, (cId_t *)GenericApp_ClusterList,
                        GENERICAPP_MAX_CLUSTERS, (cId_t *)GenericApp_ClusterList,
                        FALSE );
   
  }
}


/*********************************************************************
 * 函数名称：GenericApp_MessageMSGCB
 * 功    能：GenericApp的输入信息事件处理函数。本函数处理来自其他设备的
 *           任何输入数据。因此，可基于簇ID执行预定的动作。
 * 入口参数：pkt  指向输入信息的指针
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void GenericApp_MessageMSGCB( afIncomingMSGPacket_t *pkt )
{
  /* AF输入信息中的簇 */
  switch ( pkt->clusterId )
  {
    /* 簇GENERICAPP_CLUSTERID */
    case GENERICAPP_CLUSTERID:
   
      /* 熄灭LED1 */
      /* 对于SK-SmartRF05EB而言，LED1是LED_G（绿色） */
      HalLedSet ( HAL_LED_1, HAL_LED_MODE_OFF );      
      
      /* 闪烁LED1 */
      HalLedBlink( HAL_LED_1, 2, 50, 250 ); 
   /* UART上显示接收到的信息 */    
      HalUARTWrite(HAL_UART_PORT_0,pkt->cmd.Data, osal_strlen(pkt->cmd.Data));
    /* 若包含了LCD_SUPPORTED编译选项，则在LCD上显示接收到的信息 */      
#if defined( LCD_SUPPORTED )
      HalLcdWriteString( (char*)pkt->cmd.Data, HAL_LCD_LINE_4 );
      HalLcdWriteString( "rcvd", HAL_LCD_LINE_5 );
/* 若包含了WIN32编译选项，则在IAR调试环境下打印出接收到的信息 */
#elif defined( WIN32 )
      WPRINTSTR( pkt->cmd.Data );
#endif
    break;
  }
}


/*********************************************************************
 * 函数名称：GenericApp_SendTheMessage
 * 功    能：发送信息。
 * 入口参数：buf, len 
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
void GenericApp_SendTheMessage(byte *buf, uint8 len)
{
  
  if ( AF_DataRequest( &GenericApp_DstAddr, &GenericApp_epDesc,
                       GENERICAPP_CLUSTERID,
                       len + 1,
                       (byte *)buf,
                       &GenericApp_TransID,
                       AF_DISCV_ROUTE, 
                       AF_DEFAULT_RADIUS ) == afStatus_SUCCESS )
  {
    // 发送请求执行成功，用户可添加自己相应的处理代码
    
       HalLedSet ( HAL_LED_2, HAL_LED_MODE_TOGGLE );      
         
  }
  else
  {
        HalLedBlink( HAL_LED_2, 0, 50, 250 );
      // 发送请求执行失败，用户可添加自己相应的处理代码
  }
}

/**********************************************************
func: 接收串口数据包
参数：
      1. *buf -- 串口接收数据所存放的地方
      2. maxlen -- 串口接收数据的最大长度
retuen: uint16 接收到的数据长度
***********************************************************/
uint16  uart_read(uint8 *buf, uint16 maxlen)
{
    uint16 slen=0; 
    uint8 dtime=0;
    while(dtime<20)
    {    
        while(HalUARTRead(HAL_UART_PORT_0,buf+slen, 1))
        {
            slen++;
            if(slen>=maxlen)
                return  slen;
            dtime=0;
        }
        MicroWait (30);     // Wait 30us; 
        dtime++;
    }    
   return  slen; 
}
    
