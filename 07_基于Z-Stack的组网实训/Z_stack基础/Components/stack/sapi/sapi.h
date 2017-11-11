/*******************************************************************************
 * 文件名称：sapi.h
 * 功    能：SAPI函数的头文件   
 ******************************************************************************/

#ifndef SAPI_H
#define SAPI_H


/* 包含头文件 */
/********************************************************************/
#include "ZComDef.h"
#include "af.h"
/********************************************************************/


/* 宏定义 */
/********************************************************************/
/*
  允许绑定定时器任务事件
 */
#define ZB_ALLOW_BIND_TIMER               0x4000  

/*
  绑定定时器任务事件
 */
#define ZB_BIND_TIMER                     0x2000

/*
  主应用程序入口任务事件
 */
#define ZB_ENTRY_EVENT                    0x1000

/*
  用户任务事件
 */
#define ZB_USER_EVENTS                    0x00FF


/*
  搜索设备类型
 */
#define ZB_IEEE_SEARCH                    1

/*
  设备信息常量
 */
#define ZB_INFO_DEV_STATE                 0
#define ZB_INFO_IEEE_ADDR                 1
#define ZB_INFO_SHORT_ADDR                2
#define ZB_INFO_PARENT_SHORT_ADDR         3
#define ZB_INFO_PARENT_IEEE_ADDR          4
#define ZB_INFO_CHANNEL                   5
#define ZB_INFO_PAN_ID                    6
#define ZB_INFO_EXT_PAN_ID                7

/*
  SAPI发送数据请求目的地址
 */
#define ZB_BINDING_ADDR                   INVALID_NODE_ADDR
#define ZB_BROADCAST_ADDR                 0xffff

/*
  SAPI状态值
 */
#define ZB_SUCCESS                        ZSuccess
#define ZB_FAILURE                        ZFailure
#define ZB_INVALID_PARAMETER              ZInvalidParameter
#define ZB_ALREADY_IN_PROGRESS            ZSapiInProgress
#define ZB_TIMEOUT                        ZSapiTimeout
#define ZB_INIT                           ZSapiInit
#define ZB_AF_FAILURE                     afStatus_FAILED
#define ZB_AF_MEM_FAIL                    afStatus_MEM_FAIL
#define ZB_AF_INVALID_PARAMETER           afStatus_INVALID_PARAMETER

/*
  SAPI 扫描周期值
 */
#define ZB_SCAN_DURATION_0                0   //  19.2 ms
#define ZB_SCAN_DURATION_1                1   //  38.4 ms
#define ZB_SCAN_DURATION_2                2   //  76.8 ms
#define ZB_SCAN_DURATION_3                3   //  153.6 ms
#define ZB_SCAN_DURATION_4                4   //  307.2 ms
#define ZB_SCAN_DURATION_5                5   //  614.4 ms
#define ZB_SCAN_DURATION_6                6   //  1.23 sec
#define ZB_SCAN_DURATION_7                7   //  2.46 sec
#define ZB_SCAN_DURATION_8                8   //  4.92 sec
#define ZB_SCAN_DURATION_9                9   //  9.83 sec
#define ZB_SCAN_DURATION_10               10  //  19.66 sec
#define ZB_SCAN_DURATION_11               11  //  39.32 sec
#define ZB_SCAN_DURATION_12               12  //  78.64 sec
#define ZB_SCAN_DURATION_13               13  //  157.28 sec
#define ZB_SCAN_DURATION_14               14  //  314.57 sec

/*
  设备类型定义
 */
#define ZG_DEVICETYPE_COORDINATOR      0x00
#define ZG_DEVICETYPE_ROUTER           0x01
#define ZG_DEVICETYPE_ENDDEVICE        0x02

/*
  网络列表定义
 */
typedef struct
{
  uint16 panID;
  uint8 channel;
} zb_NetworkList_t;

/*
  回调事件定义
 */
typedef struct
{
  osal_event_hdr_t hdr;
  uint16 data;
} sapi_CbackEvent_t;


/* 外部变量 */
/********************************************************************/
extern uint8 sapi_TaskID;
extern endPointDesc_t sapi_epDesc;

#ifdef __cplusplus
extern "C"
{
#endif

extern const SimpleDescriptionFormat_t zb_SimpleDesc;
/********************************************************************/



/*********************************************************************
 * 函数名称：zb_SystemReset
 * 功    能：系统复位
 * 入口参数：无
 * 出口参数：无 
 * 返 回 值：无
 ********************************************************************/
extern void zb_SystemReset ( void );


/*********************************************************************
 * 函数名称：zb_StartRequest
 * 功    能：开始ZigBee协议栈。当协议栈启动后，首先从NV中读取配置参数；
 *           然后设备加入该网络。
 * 入口参数：无
 * 出口参数：无 
 * 返 回 值：无
 ********************************************************************/
extern void zb_StartRequest ( void );


/*********************************************************************
 * 函数名称：zb_PermitJoiningRequest
 * 功    能：控制设备允许加入网络，或者不允许加入网络
 * 入口参数：destination  设备的地址，通常为本地设备地址或表示所有
 *                        协调器或者路由器的特殊广播地址【0xFFFC】
 *                        通过这种方式可以控制单个设备或者整个网络。
 *           timeout      允许加入的规定时间（单位为秒）
 *                        如果是0x00,表示不允许加入网络
 *                        如果是0xff,表示一直允许加入网络
 * 出口参数：无   
 * 返 回 值：ZB_SUCCESS   允许加入网络
 *           其他错误代码 不允许加入
 ********************************************************************/
extern uint8 zb_PermitJoiningRequest ( uint16 destination, uint8 timeout );


/*********************************************************************
 * 函数名称：zb_BindDevice
 * 功    能：控制2个设备间建立或者解除绑定
 * 入口参数：create       TRUE表示创建绑定，FLASH 表示解除绑定
 *           commandId    命令ID，绑定标识符，基于某命令的绑定
 *           pDestination 设备64位IEEE地址
 * 出口参数：绑定操作的状态   
 * 返 回 值：无
 ********************************************************************/
extern void zb_BindDevice ( uint8 create, uint16 commandId, uint8 *pDestination );


/*********************************************************************
 * 函数名称：zb_AllowBind
 * 功    能：使设备在规定的时间内处于允许绑定模式。
 * 入口参数：timeout    允许绑定的规定时间（单位为秒）
 * 出口参数：ZB_SUCCESS 设备进入允许绑定模式，否则报错
 * 返 回 值：无
 ********************************************************************/
extern void zb_AllowBind (  uint8 timeout );


/*********************************************************************
 * 函数名称：zb_SendDataRequest
 * 功    能：发送数据请求函数
 * 入口参数：destination    被发送数据的目的地址。该地址为：
 ×　　　　                 - 设备16位短地址【0-0xFFFD】
 *                          - ZB_BROADCAST_ADDR 向网络中的所有设备发送数据。
 *                          - ZB_BINDING_ADDR 向先前绑定的设备发送数据。
 *           commandId      随数据发送的命令ID。如果使用ZB_BINDING_ADDR
 *                          该命令同样被用来指示绑定被使用。
 *           len            pData数据长度
 *           pData          发送的数据指针
 *　         handle         指示发送数据请求
 ×          txOptions      如果为TRUE，则要求从目的地址返回应答
 ×          radius        最大跳数量
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
extern void zb_SendDataRequest ( uint16 destination, uint16 commandId, uint8 len,
                          uint8 *pData, uint8 handle, uint8 ack, uint8 radius );


/*********************************************************************
 * 函数名称：zb_ReadConfiguration
 * 功    能：从NV读取配置信息。
 * 入口参数：configId    配置信息标识符。 
 *           len         配置信息数据长度
 *           pValue      存储配置信息缓冲区
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
extern uint8 zb_ReadConfiguration( uint8 configId, uint8 len, void *pValue );


/*********************************************************************
 * 函数名称：zb_WriteConfiguration
 * 功    能：向NV写入配置信息。
 * 入口参数：configId    配置信息标识符。 
 *           len         配置信息数据长度
 *           pValue      存储配置信息缓冲区
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
extern uint8 zb_WriteConfiguration( uint8 configId, uint8 len, void *pValue );


/*********************************************************************
 * 函数名称：zb_GetDeviceInfo
 * 功    能：获取设备信息。
 * 入口参数：param    设备信息标识符。 
 *           pValue   存储设备信息缓冲区
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
extern void zb_GetDeviceInfo ( uint8 param, void *pValue );


/*********************************************************************
 * 函数名称：zb_FindDeviceRequest
 * 功    能：确定网络中设备的短地址。设备通过调用本函数来初始化并且
 *           被在相同网络中的成员搜索到。当搜索完成后，
 *           zb_FindDeviceConfirm回调函数将被调用。
 * 入口参数：searchType  查找类型。 
 *           searchKey   查找值
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
extern void zb_FindDeviceRequest( uint8 searchType, void *searchKey );


/*********************************************************************
 * 函数名称：zb_HandleOsalEvent
 * 功    能：操作系统事件处理函数。
 * 入口参数：event  由OSAL分配的事件event。该位掩码包含了被设置的事件。
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
extern void zb_HandleOsalEvent( uint16 event );


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
extern void zb_StartConfirm( uint8 status );


/*********************************************************************
 * 函数名称：zb_SendDataConfirm
 * 功    能：发送数据确认函数。本函数在发送数据操作请求后被ZigBee协议栈
 *           所调用。
 * 入口参数：handle  数据传输识别句柄。
 *           status  操作状态。 
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
extern void zb_SendDataConfirm( uint8 handle, uint8 status );


/*********************************************************************
 * 函数名称：zb_BindConfirm
 * 功    能：绑定确认函数。本函数在绑定操作请求后被ZigBee协议栈所调用。
 * 入口参数：commandId  绑定确定后的命令ID。
 *           status     操作状态。 
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
extern void zb_BindConfirm( uint16 commandId, uint8 status );


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
extern void zb_FindDeviceConfirm( uint8 searchType, uint8 *searchKey, uint8 *result );


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
extern void zb_ReceiveDataIndication( uint16 source, uint16 command, uint16 len, uint8 *pData  );


/*********************************************************************
 * 函数名称：zb_AllowBindConfirm
 * 功    能：允许绑定确认函数。指示另外一个设备试图绑定到该设备。
 * 入口参数：source   绑定源地址。 
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
extern void zb_AllowBindConfirm( uint16 source );


/*********************************************************************
 * 函数名称：zb_HandleKeys
 * 功    能：按键事件处理函数。
 * 入口参数：shift  若为真，则启用按键的第二功能。本函数不关心该参数。
 *           keys   按键事件的掩码。对于本应用，合法的按键为：
 *                  HAL_KEY_SW_1     SK-SmartRF10GW的UP键
 *                  HAL_KEY_SW_2     SK-SmartRF10GW的RIGHT键
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
extern void zb_HandleKeys( uint8 shift, uint8 keys );


/*********************************************************************
 * 函数名称：SAPI_ProcessEvent
 * 功    能：SimpleApp的任务事件处理函数。
 * 入口参数：task_id  由OSAL分配的任务ID。
 *           events   准备处理的事件。该变量是一个位图，可包含多个事件。
 * 出口参数：无
 * 返 回 值：尚未处理的事件。
 ********************************************************************/
extern UINT16 SAPI_ProcessEvent( byte task_id, UINT16 events );


/*********************************************************************
 * 函数名称：SAPI_Init
 * 功    能：SimpleApp的初始化函数。
 * 入口参数：task_id  由OSAL分配的任务ID。该ID被用来发送消息和设定定时
 *           器。
 * 出口参数：无
 * 返 回 值：无
 ********************************************************************/
extern void SAPI_Init( byte task_id );


//extern void osalAddTasks( void );

#ifdef __cplusplus
}
#endif

#endif
