/*******************************************************************************
 * 文件名称：esp_data.c
 * 功    能：ESP数据文件。
 ******************************************************************************/


/* 包含头文件 */
/********************************************************************/
#include "ZComDef.h"
#include "OSAL_Clock.h"
#include "ZDConfig.h"
#include "se.h"
#include "esp.h"
#include "zcl_general.h"
#include "zcl_se.h"
#include "zcl_key_establish.h"
/********************************************************************/


/* 常量定义 */
/********************************************************************/
#define ESP_DEVICE_VERSION      0
#define ESP_FLAGS               0
#define ESP_HWVERSION           1
#define ESP_ZCLVERSION          1

/*
  基础簇
 */
const uint8 espZCLVersion = ESP_ZCLVERSION;
const uint8 espHWVersion = ESP_HWVERSION;
const uint8 espManufacturerName[] = { 16, 'S','I','K','A','I',' ','E','L','E','C','T','R','O','N','I','C' };
const uint8 espModelId[] = { 16, 'S','K','0','0','0','1',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ' };
const uint8 espDateCode[] = { 16, '2','0','1','0','0','3','0','8',' ',' ',' ',' ',' ',' ',' ',' ' };
const uint8 espPowerSource = POWER_SOURCE_MAINS_1_PHASE;
uint8 espLocationDescription[] = { 16, ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ' };
uint8 espPhysicalEnvironment = PHY_UNSPECIFIED_ENV;
uint8 espDeviceEnabled = DEVICE_ENABLED;

/*
  识别簇属性
 */
uint16 espIdentifyTime = 0;
uint32 espTime = 0;
uint8 espTimeStatus = 0x01;

/*
  简单测量簇 - 读取信息设置属性
 */
uint8 espCurrentSummationDelivered[] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 };
uint8 espCurrentSummationReceived[] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 };
uint8 espCurrentMaxDemandDelivered[] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 };
uint8 espCurrentMaxDemandReceived[] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 };
uint8 espCurrentTier1SummationDelivered[] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 };
uint8 espCurrentTier1SummationReceived[] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 };
uint8 espCurrentTier2SummationDelivered[] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 };
uint8 espCurrentTier2SummationReceived[] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 };
uint8 espCurrentTier3SummationDelivered[] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 };
uint8 espCurrentTier3SummationReceived[] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 };
uint8 espCurrentTier4SummationDelivered[] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 };
uint8 espCurrentTier4SummationReceived[] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 };
uint8 espCurrentTier5SummationDelivered[] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 };
uint8 espCurrentTier5SummationReceived[] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 };
uint8 espCurrentTier6SummationDelivered[] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 };
uint8 espCurrentTier6SummationReceived[] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 };
uint8 espDFTSummation[] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 };
uint16 espDailyFreezeTime = 0x01;
int8 espPowerFactor = 0x01;
UTCTime espSnapshotTime = 0x00;
UTCTime espMaxDemandDeliverdTime = 0x00;
UTCTime espMaxDemandReceivedTime = 0x00;

/*
  简单测量簇 - 测量状态属性
 */
uint8 espStatus = 0x12;

/*
  简单测量簇 - 格式化属性
 */
uint8 espUnitOfMeasure = 0x01;
uint24 espMultiplier = 0x01;
uint24 espDivisor = 0x01;
uint8 espSummationFormating = 0x01;
uint8 espDemandFormatting = 0x01;
uint8 espHistoricalConsumptionFormatting = 0x01;

/*
  简单测量簇 - esp历史消耗属性
 */
uint24 espInstanteneousDemand = 0x01;
uint24 espCurrentdayConsumptionDelivered = 0x01;
uint24 espCurrentdayConsumptionReceived = 0x01;
uint24 espPreviousdayConsumptionDelivered = 0x01;
uint24 espPreviousdayConsumtpionReceived = 0x01;
UTCTime espCurrentPartialProfileIntervalStartTime = 0x1000;
uint24 espCurrentPartialProfileIntervalValue = 0x0001;
uint8 espMaxNumberOfPeriodsDelivered = 0x01;

/*
  需求响应和负载控制
 */
uint8 espUtilityDefinedGroup = 0x00;
uint8 espStartRandomizeMinutes = 0x00;
uint8 espStopRandomizeMinutes = 0x00;
uint8 espSignature[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};

/*
  密钥建立
 */
uint16 espKeyEstablishmentSuite = CERTIFICATE_BASED_KEY_ESTABLISHMENT;

/*
  簇定义 - 用户簇ID
 */
CONST zclAttrRec_t espAttrs[ESP_MAX_ATTRIBUTES] =
{
  /* 通用基础簇属性 */
  {
    ZCL_CLUSTER_ID_GEN_BASIC,             // 簇ID - 在建立时被定义(比如：zcl.h)
    {  // 属性记录
      ATTRID_BASIC_ZCL_VERSION,           // 属性ID - 在簇库头文件中(比如：zcl_general.h)
      ZCL_DATATYPE_UINT8,                 // 数据类型 - 在 zcl.h中
      ACCESS_CONTROL_READ,                // 存储控制变量 - 在 zcl.h中
      (void *)&espZCLVersion              // 指向属性变量的指针
    }
  },
  {                                       // 下同
    ZCL_CLUSTER_ID_GEN_BASIC,
    {  // 属性记录
      ATTRID_BASIC_HW_VERSION,
      ZCL_DATATYPE_UINT8,
      ACCESS_CONTROL_READ,
      (void *)&espHWVersion
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    {  // 属性记录
      ATTRID_BASIC_MANUFACTURER_NAME,
      ZCL_DATATYPE_CHAR_STR,
      ACCESS_CONTROL_READ,
      (void *)espManufacturerName
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    {  // 属性记录
      ATTRID_BASIC_MODEL_ID,
      ZCL_DATATYPE_CHAR_STR,
      ACCESS_CONTROL_READ,
      (void *)espModelId
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    {  // 属性记录
      ATTRID_BASIC_DATE_CODE,
      ZCL_DATATYPE_CHAR_STR,
      ACCESS_CONTROL_READ,
      (void *)espDateCode
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    {   // 属性记录
      ATTRID_BASIC_POWER_SOURCE,
      ZCL_DATATYPE_ENUM8,
      ACCESS_CONTROL_READ,
      (void *)&espPowerSource
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    {  // 属性记录
      ATTRID_BASIC_LOCATION_DESC,
      ZCL_DATATYPE_CHAR_STR,
      (ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE),
      (void *)espLocationDescription
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    {  // 属性记录
      ATTRID_BASIC_PHYSICAL_ENV,
      ZCL_DATATYPE_ENUM8,
      (ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE),
      (void *)&espPhysicalEnvironment
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    {  // 属性记录
      ATTRID_BASIC_DEVICE_ENABLED,
      ZCL_DATATYPE_BOOLEAN,
      (ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE),
      (void *)&espDeviceEnabled
    }
  },

  /* 识别簇属性 */
  {
    ZCL_CLUSTER_ID_GEN_IDENTIFY,
    {  // 属性记录
      ATTRID_IDENTIFY_TIME,
      ZCL_DATATYPE_UINT16,
      (ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE),
      (void *)&espIdentifyTime
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
      (void *)&espTime
    }
  },
  // 在 SE 应用中, 仅主时钟将被使用。因此标记存储控制仅能够被读
  {
    ZCL_CLUSTER_ID_GEN_TIME,
    { // 属性记录
      ATTRID_TIME_STATUS,
      ZCL_DATATYPE_BITMAP8,
      ACCESS_CONTROL_READ,
      (void *)&espTimeStatus
    }
  },

  // SE 属性 
  {
    ZCL_CLUSTER_ID_SE_SIMPLE_METERING,          // 簇ID - 在配置文件中被定义(比如：se.h)
    { // 属性记录
      ATTRID_SE_CURRENT_SUMMATION_DELIVERED,    // 属性ID - 在簇库头文件中(比如：zcl_general.h)
      ZCL_DATATYPE_UINT48,                      // 数据类型 - 在 zcl.h 文件中
      ACCESS_CONTROL_READ,                      // 变量存储控制 - 在 zcl.h 文件中
      (void *)espCurrentSummationDelivered      // 指向属性变量
    }
  },
  {                                             // 下同
    ZCL_CLUSTER_ID_SE_SIMPLE_METERING,         
    {  // 属性记录
      ATTRID_SE_CURRENT_SUMMATION_RECEIVED, 
      ZCL_DATATYPE_UINT48,                      
      ACCESS_CONTROL_READ,                  
      (void *)espCurrentSummationReceived      
    }
  },
  {
    ZCL_CLUSTER_ID_SE_SIMPLE_METERING, 
    {  // 属性记录
      ATTRID_SE_CURRENT_MAX_DEMAND_DELIVERED,  
      ZCL_DATATYPE_UINT48,                  
      ACCESS_CONTROL_READ,                    
      (void *)espCurrentMaxDemandDelivered     
    }
  },
  {
    ZCL_CLUSTER_ID_SE_SIMPLE_METERING,        
    {  // 属性记录
      ATTRID_SE_CURRENT_MAX_DEMAND_RECEIVED,
      ZCL_DATATYPE_UINT48,                    
      ACCESS_CONTROL_READ,             
      (void *)espCurrentMaxDemandReceived      
    }
  },
  {
    ZCL_CLUSTER_ID_SE_SIMPLE_METERING,  
    {  // 属性记录
      ATTRID_SE_CURRENT_TIER1_SUMMATION_DELIVERED,  
      ZCL_DATATYPE_UINT48,                 
      ACCESS_CONTROL_READ,                        
      (void *)espCurrentTier1SummationDelivered  
    }
  },
  {
    ZCL_CLUSTER_ID_SE_SIMPLE_METERING,            
    { // 属性记录
      ATTRID_SE_CURRENT_TIER1_SUMMATION_RECEIVED,  
      ZCL_DATATYPE_UINT48,                     
      ACCESS_CONTROL_READ,                         
      (void *)espCurrentTier1SummationReceived     
    }
  },
  {
    ZCL_CLUSTER_ID_SE_SIMPLE_METERING, 
    {  // 属性记录
      ATTRID_SE_CURRENT_TIER2_SUMMATION_DELIVERED,    
      ZCL_DATATYPE_UINT48,                       
      ACCESS_CONTROL_READ,                          
      (void *)espCurrentTier2SummationDelivered
    }
  },
  {
    ZCL_CLUSTER_ID_SE_SIMPLE_METERING,              
    {  // 属性记录
      ATTRID_SE_CURRENT_TIER2_SUMMATION_RECEIVED, 
      ZCL_DATATYPE_UINT48,                        
      ACCESS_CONTROL_READ,             
      (void *)espCurrentTier2SummationReceived     
    }
  },
  {
    ZCL_CLUSTER_ID_SE_SIMPLE_METERING,
    {  // 属性记录
      ATTRID_SE_CURRENT_TIER3_SUMMATION_DELIVERED,  
      ZCL_DATATYPE_UINT48,                 
      ACCESS_CONTROL_READ,                          
      (void *)espCurrentTier3SummationDelivered     
    }
  },
  {
    ZCL_CLUSTER_ID_SE_SIMPLE_METERING,            
    {  // 属性记录
      ATTRID_SE_CURRENT_TIER3_SUMMATION_RECEIVED,
      ZCL_DATATYPE_UINT48,                        
      ACCESS_CONTROL_READ,                  
      (void *)espCurrentTier3SummationReceived     
    }
  },
  {
    ZCL_CLUSTER_ID_SE_SIMPLE_METERING,  
    {  // 属性记录
      ATTRID_SE_CURRENT_TIER4_SUMMATION_DELIVERED, 
      ZCL_DATATYPE_UINT48,                
      ACCESS_CONTROL_READ,                           
      (void *)espCurrentTier4SummationDelivered  
    }
  },
  {
    ZCL_CLUSTER_ID_SE_SIMPLE_METERING,             
    { // 属性记录
      ATTRID_SE_CURRENT_TIER4_SUMMATION_RECEIVED, 
      ZCL_DATATYPE_UINT48,                       
      ACCESS_CONTROL_READ,      
      (void *)espCurrentTier4SummationReceived    
    }
  },
  {
    ZCL_CLUSTER_ID_SE_SIMPLE_METERING,              
    {  // 属性记录
      ATTRID_SE_CURRENT_TIER5_SUMMATION_DELIVERED, 
      ZCL_DATATYPE_UINT48,                           
      ACCESS_CONTROL_READ,                
      (void *)espCurrentTier5SummationDelivered    
    }
  },
  {
    ZCL_CLUSTER_ID_SE_SIMPLE_METERING,             
    { // 属性记录
      ATTRID_SE_CURRENT_TIER5_SUMMATION_RECEIVED,
      ZCL_DATATYPE_UINT48,                           
      ACCESS_CONTROL_READ,                     
      (void *)espCurrentTier5SummationReceived      
    }
  },
   {
    ZCL_CLUSTER_ID_SE_SIMPLE_METERING,   
    {  // 属性记录
      ATTRID_SE_CURRENT_TIER6_SUMMATION_DELIVERED,  
      ZCL_DATATYPE_UINT48,                           
      ACCESS_CONTROL_READ,                         
      (void *)espCurrentTier6SummationDelivered     
    }
  },
  {
    ZCL_CLUSTER_ID_SE_SIMPLE_METERING,             
    { // 属性记录
      ATTRID_SE_CURRENT_TIER6_SUMMATION_RECEIVED,  
      ZCL_DATATYPE_UINT48,                          
      ACCESS_CONTROL_READ,                   
      (void *)espCurrentTier6SummationReceived     
    }
  },
  {
    ZCL_CLUSTER_ID_SE_SIMPLE_METERING,
    {  // 属性记录
      ATTRID_SE_DFT_SUMMATION,              
      ZCL_DATATYPE_UINT48,              
      ACCESS_CONTROL_READ,      
      (void *)espDFTSummation 
    }
  },
  {
    ZCL_CLUSTER_ID_SE_SIMPLE_METERING,      
    {  // 属性记录
      ATTRID_SE_DAILY_FREEZE_TIME, 
      ZCL_DATATYPE_UINT16,                 
      ACCESS_CONTROL_READ,          
      (void *)&espDailyFreezeTime        
    }
  },
  {
    ZCL_CLUSTER_ID_SE_SIMPLE_METERING,     
    { // 属性记录
      ATTRID_SE_POWER_FACTOR,    
      ZCL_DATATYPE_INT8,                    
      ACCESS_CONTROL_READ,           
      (void *)&espPowerFactor              
    }
  },
  {
    ZCL_CLUSTER_ID_SE_SIMPLE_METERING,
    { // 属性记录
      ATTRID_SE_READING_SNAPSHOT_TIME,      
      ZCL_DATATYPE_UTC,                  
      ACCESS_CONTROL_READ,       
      (void *)&espSnapshotTime            
    }
  },
    {
    ZCL_CLUSTER_ID_SE_SIMPLE_METERING, 
    { // 属性记录
      ATTRID_SE_CURRENT_MAX_DEMAND_DELIVERD_TIME,   
      ZCL_DATATYPE_UTC,                        
      ACCESS_CONTROL_READ,                  
      (void *)&espMaxDemandDeliverdTime             
    }
  },
    {
    ZCL_CLUSTER_ID_SE_SIMPLE_METERING,  
    { // 属性记录
      ATTRID_SE_CURRENT_MAX_DEMAND_RECEIVED_TIME,  
      ZCL_DATATYPE_UTC,                   
      ACCESS_CONTROL_READ,                        
      (void *)&espMaxDemandReceivedTime    
    }
  },
  {
    ZCL_CLUSTER_ID_SE_SIMPLE_METERING,      
    { // 属性记录
      ATTRID_SE_STATUS,              
      ZCL_DATATYPE_BITMAP8,                
      ACCESS_CONTROL_READ,              
      (void *)&espStatus                   
    }
  },

  {
    ZCL_CLUSTER_ID_SE_SIMPLE_METERING,  
    { // 属性记录
      ATTRID_SE_UNIT_OF_MEASURE,           
      ZCL_DATATYPE_ENUM8,                 
      ACCESS_CONTROL_READ,                 
      (void *)&espUnitOfMeasure            
    }
  },
  {
    ZCL_CLUSTER_ID_SE_SIMPLE_METERING,
    { // 属性记录
      ATTRID_SE_MULTIPLIER,                
      ZCL_DATATYPE_UINT24,                 
      ACCESS_CONTROL_READ,                 
      (void *)&espMultiplier           
    }
  },
  {
    ZCL_CLUSTER_ID_SE_SIMPLE_METERING,     
    { // 属性记录
      ATTRID_SE_DIVISOR,                   
      ZCL_DATATYPE_UINT24,                  
      ACCESS_CONTROL_READ,                
      (void *)&espDivisor             
    }
  },
  {
    ZCL_CLUSTER_ID_SE_SIMPLE_METERING,      
    { // 属性记录
      ATTRID_SE_SUMMATION_FORMATTING,       
      ZCL_DATATYPE_BITMAP8,               
      ACCESS_CONTROL_READ,                 
      (void *)&espSummationFormating      
    }
  },
  {
    ZCL_CLUSTER_ID_SE_SIMPLE_METERING,      
    { // 属性记录
      ATTRID_SE_DEMAND_FORMATTING,          
      ZCL_DATATYPE_BITMAP8,                 
      ACCESS_CONTROL_READ,                
      (void *)&espDemandFormatting          
    }
  },
  {
    ZCL_CLUSTER_ID_SE_SIMPLE_METERING,               
    { // 属性记录
      ATTRID_SE_HISTORICAL_CONSUMPTION_FORMATTING,    
      ZCL_DATATYPE_BITMAP8,                         
      ACCESS_CONTROL_READ,                          
      (void *)&espHistoricalConsumptionFormatting     
    }
  },
  {
    ZCL_CLUSTER_ID_SE_SIMPLE_METERING,
    { // 属性记录
      ATTRID_SE_INSTANTANEOUS_DEMAND,
      ZCL_DATATYPE_UINT24,
      ACCESS_CONTROL_READ,
      (void *)&espInstanteneousDemand
    }
  },
  {
    ZCL_CLUSTER_ID_SE_SIMPLE_METERING,
    { // 属性记录
      ATTRID_SE_CURRENTDAY_CONSUMPTION_DELIVERED,
      ZCL_DATATYPE_UINT24,
      ACCESS_CONTROL_READ,
      (void *)&espCurrentdayConsumptionDelivered
    }
  },
  {
    ZCL_CLUSTER_ID_SE_SIMPLE_METERING,
    {  // 属性记录
      ATTRID_SE_CURRENTDAY_CONSUMPTION_RECEIVED,
      ZCL_DATATYPE_UINT24,
      ACCESS_CONTROL_READ,
      (void *)&espCurrentdayConsumptionReceived
    }
  },
  {
    ZCL_CLUSTER_ID_SE_SIMPLE_METERING,
    {  // 属性记录
      ATTRID_SE_PREVIOUSDAY_CONSUMPTION_DELIVERED,
      ZCL_DATATYPE_UINT24,
      ACCESS_CONTROL_READ,
      (void *)&espPreviousdayConsumptionDelivered
    }
  },
  {
    ZCL_CLUSTER_ID_SE_SIMPLE_METERING,
    {  // 属性记录
      ATTRID_SE_PREVIOUSDAY_CONSUMPTION_RECEIVED,
      ZCL_DATATYPE_UINT24,
      ACCESS_CONTROL_READ,
      (void *)&espPreviousdayConsumtpionReceived
    }
  },
  {
    ZCL_CLUSTER_ID_SE_SIMPLE_METERING,
    { // 属性记录
      ATTRID_SE_CURRENT_PARTIAL_PROFILE_INTERVAL_START_TIME,
      ZCL_DATATYPE_UTC,
      ACCESS_CONTROL_READ,
      (void *)&espCurrentPartialProfileIntervalStartTime
    }
  },

  {
    ZCL_CLUSTER_ID_SE_SIMPLE_METERING,
    { // 属性记录
      ATTRID_SE_CURRENT_PARTIAL_PROFILE_INTERVAL_VALUE,
      ZCL_DATATYPE_UINT24,
      ACCESS_CONTROL_READ,
      (void *)&espCurrentPartialProfileIntervalValue
    }
  },

  {
    ZCL_CLUSTER_ID_SE_SIMPLE_METERING,
    {  // 属性记录
      ATTRID_SE_MAX_NUMBER_OF_PERIODS_DELIVERED,
      ZCL_DATATYPE_UINT8,
      ACCESS_CONTROL_READ,
      (void *)&espMaxNumberOfPeriodsDelivered
    }
  },
  {
    ZCL_CLUSTER_ID_SE_LOAD_CONTROL,
    {  // 属性记录
      ATTRID_SE_UTILITY_DEFINED_GROUP,
      ZCL_DATATYPE_UINT8,
      ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE,
      (void *)&espUtilityDefinedGroup
    }
  },
  {
    ZCL_CLUSTER_ID_SE_LOAD_CONTROL,
    {  // 属性记录
      ATTRID_SE_START_RANDOMIZE_MINUTES,
      ZCL_DATATYPE_UINT8,
      ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE,
      (void *)&espStartRandomizeMinutes
    }
  },
  {
    ZCL_CLUSTER_ID_SE_LOAD_CONTROL,
    {  // 属性记录
      ATTRID_SE_STOP_RANDOMIZE_MINUTES,
      ZCL_DATATYPE_UINT8,
      ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE,
      (void *)&espStopRandomizeMinutes
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_KEY_ESTABLISHMENT,
    {  // 属性记录
      ATTRID_KEY_ESTABLISH_SUITE,
      ZCL_DATATYPE_BITMAP16,
      ACCESS_CONTROL_READ,
      (void *)&espKeyEstablishmentSuite
    }
  },
};


/*
  簇选项定义
 */
zclOptionRec_t espOptions[ESP_MAX_OPTIONS] =
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
    ZCL_CLUSTER_ID_SE_LOAD_CONTROL,
    ( AF_EN_SECURITY ),
  },
  {
    ZCL_CLUSTER_ID_SE_SIMPLE_METERING,
    ( AF_EN_SECURITY ),
  },
  {
    ZCL_CLUSTER_ID_SE_MESSAGE,
    ( AF_EN_SECURITY ),
  },
  {
    ZCL_CLUSTER_ID_SE_SE_TUNNELING,
    ( AF_EN_SECURITY ),
  },
  {
    ZCL_CLUSTER_ID_SE_PRE_PAYMENT,
    ( AF_EN_SECURITY ),
  },
};


/* 
  ESP最大输入簇数量
 */
#define ESP_MAX_INCLUSTERS       6
const cId_t espInClusterList[ESP_MAX_INCLUSTERS] =
{
  ZCL_CLUSTER_ID_GEN_BASIC,
  ZCL_CLUSTER_ID_GEN_IDENTIFY,
  ZCL_CLUSTER_ID_SE_PRICING,
  ZCL_CLUSTER_ID_SE_LOAD_CONTROL,
  ZCL_CLUSTER_ID_SE_SIMPLE_METERING,
  ZCL_CLUSTER_ID_SE_MESSAGE
};

/* 
  ESP最大输出簇数量
 */
#define ESP_MAX_OUTCLUSTERS       6
const cId_t espOutClusterList[ESP_MAX_OUTCLUSTERS] =
{
  ZCL_CLUSTER_ID_GEN_BASIC,
  ZCL_CLUSTER_ID_GEN_IDENTIFY,
  ZCL_CLUSTER_ID_SE_PRICING,
  ZCL_CLUSTER_ID_SE_LOAD_CONTROL,
  ZCL_CLUSTER_ID_SE_SIMPLE_METERING,
  ZCL_CLUSTER_ID_SE_MESSAGE
};

SimpleDescriptionFormat_t espSimpleDesc =
{
  ESP_ENDPOINT,                   //  uint8 Endpoint;
  ZCL_SE_PROFILE_ID,              //  uint16 AppProfId[2];
  ZCL_SE_DEVICEID_ESP,            //  uint16 AppDeviceId[2];
  ESP_DEVICE_VERSION,             //  int   AppDevVer:4;
  ESP_FLAGS,                      //  int   AppFlags:4;
  ESP_MAX_INCLUSTERS,             //  uint8  AppNumInClusters;
  (cId_t *)espInClusterList,      //  cId_t *pAppInClusterList;
  ESP_MAX_OUTCLUSTERS,            //  uint8  AppNumInClusters;
  (cId_t *)espOutClusterList      //  cId_t *pAppInClusterList;
};



