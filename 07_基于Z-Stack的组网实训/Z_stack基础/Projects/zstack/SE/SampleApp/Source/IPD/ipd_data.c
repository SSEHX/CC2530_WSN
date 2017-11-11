/*******************************************************************************
 * 文件名称：ipd_data.c
 * 功    能：IPD数据文件。
 ******************************************************************************/


/* 包含头文件 */
/********************************************************************/
#include "OSAL.h"
#include "ZDConfig.h"

#include "se.h"
#include "ipd.h"
#include "zcl_general.h"
#include "zcl_key_establish.h"
/********************************************************************/


/* 常量定义 */
/********************************************************************/
#define IPD_DEVICE_VERSION      0
#define IPD_FLAGS               0

#define IPD_HWVERSION           1
#define IPD_ZCLVERSION          1

/*
  基础簇
 */
const uint8 ipdZCLVersion = IPD_ZCLVERSION;
const uint8 ipdHWVersion = IPD_HWVERSION;
const uint8 ipdManufacturerName[] = { 16, 'S','I','K','A','I',' ','E','L','E','C','T','R','O','N','I','C' };
const uint8 ipdModelId[] = { 16, 'S','K','0','0','0','1',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ' };
const uint8 ipdDateCode[] = { 16, '2','0','1','0','0','3','0','8',' ',' ',' ',' ',' ',' ',' ',' ' };
const uint8 ipdPowerSource = POWER_SOURCE_MAINS_1_PHASE;

uint8 ipdLocationDescription[] = { 16, ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ' };
uint8 ipdPhysicalEnvironment = PHY_UNSPECIFIED_ENV;
uint8 ipdDeviceEnabled = DEVICE_ENABLED;

/*
  识别簇属性
 */
uint16 ipdIdentifyTime = 0;
uint32 ipdTime = 0;
uint8 ipdTimeStatus = 0x01;

/*
  密钥建立
 */
uint16 ipdKeyEstablishmentSuite = CERTIFICATE_BASED_KEY_ESTABLISHMENT;

/*
  簇定义 - 用户簇ID
 */
CONST zclAttrRec_t ipdAttrs[IPD_MAX_ATTRIBUTES] =
{
  /* 通用基础簇属性 */
  {
    ZCL_CLUSTER_ID_GEN_BASIC,             // 簇ID - 在建立时被定义(比如：zcl.h)
    {  // 属性记录
      ATTRID_BASIC_ZCL_VERSION,           // 属性ID - 在簇库头文件中(比如：zcl_general.h)
      ZCL_DATATYPE_UINT8,                 // 数据类型 - 在 zcl.h中
      ACCESS_CONTROL_READ,                // 存储控制变量 - 在 zcl.h中
      (void *)&ipdZCLVersion              // 指向属性变量
    }
  },
  {                                       // 下同
    ZCL_CLUSTER_ID_GEN_BASIC,
    {  // 属性记录
      ATTRID_BASIC_HW_VERSION,
      ZCL_DATATYPE_UINT8,
      ACCESS_CONTROL_READ,
      (void *)&ipdHWVersion
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    {  // 属性记录
      ATTRID_BASIC_MANUFACTURER_NAME,
      ZCL_DATATYPE_CHAR_STR,
      ACCESS_CONTROL_READ,
      (void *)ipdManufacturerName
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    {  // 属性记录
      ATTRID_BASIC_MODEL_ID,
      ZCL_DATATYPE_CHAR_STR,
      ACCESS_CONTROL_READ,
      (void *)ipdModelId
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    {  // 属性记录
      ATTRID_BASIC_DATE_CODE,
      ZCL_DATATYPE_CHAR_STR,
      ACCESS_CONTROL_READ,
      (void *)ipdDateCode
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    {   // 属性记录
      ATTRID_BASIC_POWER_SOURCE,
      ZCL_DATATYPE_ENUM8,
      ACCESS_CONTROL_READ,
      (void *)&ipdPowerSource
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    {  // 属性记录
      ATTRID_BASIC_LOCATION_DESC,
      ZCL_DATATYPE_CHAR_STR,
      (ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE),
      (void *)ipdLocationDescription
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    {  // 属性记录
      ATTRID_BASIC_PHYSICAL_ENV,
      ZCL_DATATYPE_ENUM8,
      (ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE),
      (void *)&ipdPhysicalEnvironment
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    {  // 属性记录
      ATTRID_BASIC_DEVICE_ENABLED,
      ZCL_DATATYPE_BOOLEAN,
      (ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE),
      (void *)&ipdDeviceEnabled
    }
  },

  /* 识别簇属性 */
  {
    ZCL_CLUSTER_ID_GEN_IDENTIFY,
    {  // 属性记录
      ATTRID_IDENTIFY_TIME,
      ZCL_DATATYPE_UINT16,
      (ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE),
      (void *)&ipdIdentifyTime
    }
  },

  /* 时间簇属性 */
  // 在 SE 应用中, 仅主时钟将被使用。因此标记存储控制仅能够被读
  {
    ZCL_CLUSTER_ID_GEN_TIME,
    { // 属性记录
      ATTRID_TIME_TIME,
      ZCL_DATATYPE_UTC,
      ACCESS_CONTROL_READ,
      (void *)&ipdTime
    }
  },
  // 在 SE 应用中, 仅主时钟将被使用。因此标记存储控制仅能够被读
  {
    ZCL_CLUSTER_ID_GEN_TIME,
    { // 属性记录
      ATTRID_TIME_STATUS,
      ZCL_DATATYPE_BITMAP8,
      ACCESS_CONTROL_READ,
      (void *)&ipdTimeStatus
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_KEY_ESTABLISHMENT,
    {  // 属性记录
      ATTRID_KEY_ESTABLISH_SUITE,
      ZCL_DATATYPE_BITMAP16,
      ACCESS_CONTROL_READ,
      (void *)&ipdKeyEstablishmentSuite
    }
  },
};


/*
  簇选项定义
 */
zclOptionRec_t ipdOptions[IPD_MAX_OPTIONS] =
{
  /* 通用簇选项 */
  {
    ZCL_CLUSTER_ID_GEN_TIME,                 // 簇ID - 在基础层中定义(比如：zcl.h)
    ( AF_EN_SECURITY /*| AF_ACK_REQUEST*/ ), // 选项 - 在 AF头文件中定义(比如：AF.h)
  },

  /* 智能能源簇选项 */
  {
    ZCL_CLUSTER_ID_SE_PRICING,
    ( AF_EN_SECURITY ),
  },
  {
    ZCL_CLUSTER_ID_SE_MESSAGE,
    ( AF_EN_SECURITY ),
  },
};


/* 
  IPD最大输入簇数量
 */
#define IPD_MAX_INCLUSTERS       4
const cId_t ipdInClusterList[IPD_MAX_INCLUSTERS] =
{
  ZCL_CLUSTER_ID_GEN_BASIC,
  ZCL_CLUSTER_ID_GEN_IDENTIFY,
  ZCL_CLUSTER_ID_SE_PRICING,
  ZCL_CLUSTER_ID_SE_MESSAGE
};

/* 
  IPD最大输出簇数量
 */
#define IPD_MAX_OUTCLUSTERS       4
const cId_t ipdOutClusterList[IPD_MAX_OUTCLUSTERS] =
{
  ZCL_CLUSTER_ID_GEN_BASIC,
  ZCL_CLUSTER_ID_GEN_IDENTIFY,
  ZCL_CLUSTER_ID_SE_PRICING,
  ZCL_CLUSTER_ID_SE_MESSAGE
};

SimpleDescriptionFormat_t ipdSimpleDesc =
{
  IPD_ENDPOINT,                       //  uint8 Endpoint;
  ZCL_SE_PROFILE_ID,                  //  uint16 AppProfId[2];
  ZCL_SE_DEVICEID_IN_PREMISE_DISPLAY, //  uint16 AppDeviceId[2];
  IPD_DEVICE_VERSION,                 //  int   AppDevVer:4;
  IPD_FLAGS,                          //  int   AppFlags:4;
  IPD_MAX_INCLUSTERS,                 //  uint8  AppNumInClusters;
  (cId_t *)ipdInClusterList,          //  cId_t *pAppInClusterList;
  IPD_MAX_OUTCLUSTERS,                //  uint8  AppNumInClusters;
  (cId_t *)ipdOutClusterList          //  cId_t *pAppInClusterList;
};


