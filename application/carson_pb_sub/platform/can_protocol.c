/* 
*  Author: Adam Huang
*  Date:2017/01/23
*/

#include "can_protocol.h"
#include "protocol.h"
#include "platform.h"
#include "app_platform.h"
#include "stm32f1xx.h"
//#include "serial_leds.h"
#include "stm32f1xx_powerboard.h"
#include "Debug.h"
//#include "UltraSonic.h"

#include "fifo.h"

#include "led_ctrl.h"


#define CanProtocolLog(format, ...)  custom_log("can protocol", format, ##__VA_ARGS__)

__IO uint32_t flag = 0xff;
//CanTxMsgTypeDef TxMessage;
CanRxMsgTypeDef RxMessage;
CanTxMsgTypeDef TxMessage;

extern platform_can_driver_t  platform_can_drivers[];
uint8_t CanTxdataBuff[CAN_LONG_FRAME_LENTH_MAX] = {0};
#if 0
uint8_t CanUpdataBuff[64];
uint8_t CanRxdataBuff[64];
#endif


uint8_t swVersion[] = SW_VERSION;
uint8_t hwVersion[] = HW_VERSION;

CAN_TXDATA_STRUCT  CommandProcessing( uint32_t func_ID, uint8_t* dstPtr, uint8_t* pdata, uint32_t len );
//CAN_TXDATA_STRUCT FirmwareUpgrade(uint32_t ID,uint8_t* pdata,uint32_t len);

can_fifo_t can_fifo_ram;
can_fifo_t *can_fifo = &can_fifo_ram;

can_pkg_t can_pkg[CAN_FIFO_SIZE] = {0};

 
typedef void (*CallBackFunc)(void);

typedef struct 
{
	uint32_t val;
	CallBackFunc callback;	
}CALLBACK_T;

const CALLBACK_T ID_table[]=
{
	{0,NULL},
	{1,NULL},
};

CALLBACK_T FuncId_table[] = {
	{0x00,NULL},
};

#if 0
extern uint8_t GetKeyValue(mico_gpio_t gpio);
uint32_t my_id;
uint8_t GetCanMacId(void)
{
    uint8_t key_value = 0;

    key_value |=  GetKeyValue(MICO_GPIO_KEY_S0);
    key_value |=  GetKeyValue(MICO_GPIO_KEY_S1)<<1;
    key_value |=  GetKeyValue(MICO_GPIO_KEY_S2)<<2;
    key_value |=  GetKeyValue(MICO_GPIO_KEY_S3)<<3;
    key_value |=  GetKeyValue(MICO_GPIO_KEY_S4)<<4;
    key_value |=  GetKeyValue(MICO_GPIO_KEY_S5)<<5;
    key_value = key_value;
    if((key_value != 0) && (key_value < 0x0f))
    {
        return key_value + 0x60;
    }
    CanProtocolLog("Ultrasonic CAN MAC ID out of range ! ! ! \r\n");
    return 0x60;
        
}
#endif
/**
  * @brief  rx msg handle
  * @param   
  * @retval 
	* @RevisionHistory
  */
CALLBACK_T* FuncIdHandle(uint32_t funcid)
{
    int i;
    int func_num = sizeof(FuncId_table)/sizeof(FuncId_table[0]);
    for(i = 0;i <func_num;i++)
    {
      if(FuncId_table[i].val == funcid)
      {
        return (&FuncId_table[i]);
      }
    }
    return NULL;
}

#define ONLYONCE       0x00
#define BEGIAN         0x01
#define TRANSING       0x02
#define END            0x03


void CanTX(mico_can_t can_type, uint32_t CANx_ID,uint8_t* pdata,uint16_t len)
{
  //return ;
	uint16_t t_len;
	uint16_t roundCount;
	uint8_t modCount;
	CAN_DATA_UNION TxMsg = {0};
	//CanTxMsgTypeDef *TxMessage = platform_can_drivers[can_type].handle->pTxMsg;
    
	t_len = len;
	roundCount = t_len/7;
	modCount = t_len%7;
	
	TxMessage.ExtId = CANx_ID;
	TxMessage.IDE   = CAN_ID_EXT;					 //��չģʽ
	TxMessage.RTR   = CAN_RTR_DATA;				 //���͵�������
	//if(roundCount <= 1)
    if(t_len <= 7)
    {
        TxMsg.CanData_Struct.SegPolo = ONLYONCE;
        TxMessage.DLC = t_len+1;		
        
        
        memcpy(&TxMessage.Data[1],pdata,t_len);
        TxMessage.Data[0] = TxMsg.CanData[0];
        
        if((CAN_USED->TSR&0x1C000000))
        {
            MicoCanMessageSend(MICO_CAN1, &TxMessage);//
        }
        return ;
    }
    
	{
		int num;
        {
            for(num = 0; num < roundCount; num++)
            {		
        //SET SEGPOLO				
                if( num == 0)
                {
                    TxMsg.CanData_Struct.SegPolo = BEGIAN;
                }
                else
                {
                    TxMsg.CanData_Struct.SegPolo = TRANSING;
                }
                
                if( modCount == 0 && num == roundCount-1)
                {
                    TxMsg.CanData_Struct.SegPolo = END;
                }
                            
                TxMsg.CanData_Struct.SegNum = num;
                memcpy(TxMsg.CanData_Struct.Data, &pdata[num*7], 7);
                memcpy(TxMessage.Data, TxMsg.CanData, 8);
                TxMessage.DLC = 8;
                if((CAN_USED->TSR&0x1C000000))
                {
                    MicoCanMessageSend(MICO_CAN1, &TxMessage);//���ͱ���	
                }
                
                //TRANSMIT LAST MSG
                if( modCount !=0 && num == roundCount-1 )
                {
                    num++;
                    TxMsg.CanData_Struct.SegPolo = END;
                    TxMsg.CanData_Struct.SegNum = num;
                    memcpy(TxMsg.CanData_Struct.Data,&pdata[num*7],modCount);
                    memcpy(TxMessage.Data,TxMsg.CanData,modCount+1);
                    TxMessage.DLC = modCount+1;
                    if((CAN_USED->TSR&0x1C000000))
                    {
                        MicoCanMessageSend(MICO_CAN1, &TxMessage);//
                    }
                }
            }
            
        }
        
	}
}
#if 0
CAN_TXDATA_STRUCT FirmwareUpgrade(uint32_t ID,uint8_t* pdata,uint32_t len)
{
	uint8_t PageNum=0;
	int DataNum=0; 
	uint8_t DeltAdd=0;
	uint8_t MD5Check[16]={0};
	int DataSize=0;
	FLASH_Status FlashEraseState; 
	FLASH_Status FlashWriteState; 
	CAN_TXDATA_STRUCT CanData;
	uint8_t EraseState=0;
	uint8_t WriteState=0;
	uint8_t rx_id;
	uint8_t rx_data[16];
	uint8_t tx_data[32];
	uint8_t rx_len,tx_len;
	uint8_t *ptx_data;
	int i;
	
	Md5Context MD5Ctx;
	MD5_HASH md5_ret;
	
	rx_id = ID;
	rx_len = 0 ;
	memcpy(rx_data,pdata,rx_len);
	//Éý¼¶×¼±¸
	switch(rx_id)
	{
		case 0x00:
				EraseState=0;
				DataNum=0;
				memcpy(MD5Check,&rx_data,16);
				DataSize = (rx_data[3]<<24)+(rx_data[2]<<16)+(rx_data[1]<<8)+rx_data[0];
				//³¬¹ý´óÐ¡
				if(DataSize>128*1024)
				{
					EraseState |= 0x01;
				}
				else
				{
					EraseState &= 0x0E;
					FLASH_Unlock();
					RCC_HSICmd(ENABLE);																									
					FLASH_SetLatency(FLASH_Latency_2);
					FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);

					for(PageNum=0;PageNum<0x40;PageNum++)
					{
						IWDG_ReloadCounter();
						
						FlashEraseState=FLASH_ErasePage(UPDATEADDRESS+PageNum*0x800);
						if(FlashEraseState!=4)
						{
							EraseState =0x02;
							break;
						}
						else
						{
							EraseState &=0x0D;
						}
					}				
					RCC_HSICmd(DISABLE);	
				}
				tx_data[0] = ID;
				tx_data[1] = EraseState;	
				CanData.len = 2;
				ptx_data = tx_data;

				break;
	
		//Éý¼¶Êý¾Ý°ü
		case 0x01:
				WriteState=0;
				DeltAdd = 8;	//TBD
				IWDG_ReloadCounter();															
			//½«µ±Ç°½ÓÊÕµÄÊý¾ÝÓëÉÏ´ÎµÄÊý¾Ý½øÐÐ±È½Ï
				if(memcmp(rx_data,RxMessage.Data,8))
				{									
					//°ë×ÖÐ´Èë
					for(i=0;i<DeltAdd;i+=2)
					{
						unsigned short tempData;														
						RCC_HSICmd(ENABLE);
						tempData = (rx_data[i]<<8)+rx_data[i+1];
						FlashWriteState=FLASH_ProgramHalfWord(UPDATEADDRESS+DataNum*2,tempData);																											
						if(FlashWriteState!= 4)
						{
							DataNum-=i/2;
							WriteState = 0x01;
							break;
						}
						else
						{
							DataNum ++;
							WriteState &= 0x0E;
						}
					}
					
					if(DeltAdd%2)
					{
						unsigned short tempData;
						tempData = (0xFF00)+rx_data[i*2+4];
						FlashWriteState=FLASH_ProgramHalfWord(UPDATEADDRESS+DataNum*2,tempData);	
						if(FlashWriteState!= 4)
						{
							DataNum-=i/2;
							WriteState |= 0x02;
							break;
						}
						else
						{
							DataNum ++;
							WriteState &= 0x0D;
						}
						RCC_HSICmd(DISABLE);
					}
				//½«Êý¾Ý±£´æÖÁ±äÁ¿ÓÃÓÚÓëÏÂ´Î½ÓÊÕµÄÊý¾Ý½øÐÐ±È½Ï
					if(WriteState==0)
					{
						memcpy(rx_data,RxMessage.Data,8);		
					}															
				}
				else
				{
					//×Ö·û´®ÖØ¸´
					 WriteState = 0x00;
				}
				tx_data[0] = ID;
				tx_data[1] = WriteState;
				CanData.len = 2;	
				ptx_data = tx_data;				
			break;
		//Éý¼¶Íê³É
		case 0x02:
				UpdateFlag=0;
				DataNum=0;
				WriteState=0;
				Md5Initialise(&MD5Ctx);
				Md5Update( &MD5Ctx, (uint8_t *)UPDATEADDRESS, DataSize);
				Md5Finalise( &MD5Ctx, &md5_ret );
				if(memcmp(md5_ret.bytes, MD5Check, 16)!=0)
				{
						WriteState |= 0x01;																											
				}							
				else
				{
					WriteState &=0x0E;
					bootTable.boot_table_t.length = DataSize;
					bootTable.boot_table_t.start_address = UPDATEADDRESS;
					bootTable.boot_table_t.type = 'A';
					bootTable.boot_table_t.upgrade_type = 'U';
					RCC_HSICmd(ENABLE);
					//²Á³ý²ÎÊýÒ³
					for(PageNum=0;PageNum<0x02;PageNum++)
					{
						IWDG_ReloadCounter();
						FlashWriteState=FLASH_ErasePage(PARAMSADDRESS+PageNum*0x800);
						if(FlashWriteState!= 4)
						{
							DataNum-=i/2;
							WriteState |= 0x02;
							break;
						}
						else
						{
							WriteState &= 0x0D;
						}
					}
					
					if(WriteState==0x00)
					{
						//Ð´²ÎÊýÇø
						for(i=0;i<16;i+=4)
						{
							unsigned int paraData;
							paraData = (bootTable.data[i+3]<<24)+(bootTable.data[i+2]<<16)+(bootTable.data[i+1]<<8)+bootTable.data[i];
							FlashWriteState=FLASH_ProgramWord(PARAMSADDRESS+DataNum*4,paraData);
							if(FlashWriteState!= 4)
							{
								DataNum-=i/2;
								WriteState |= 0x04;
								break;
							}
							else
							{
								DataNum ++;
								WriteState &= 0x0B;
							}
						}
						
						for(i=16;i<24;i+=4)
						{
							unsigned int paraData;
							paraData = (bootTable.data[i+3]<<24)+(bootTable.data[i+2]<<16)+(bootTable.data[i+1]<<8)+bootTable.data[i];
							FlashWriteState=FLASH_ProgramWord(PARAMSADDRESS+DataNum*4,paraData);
							if(FlashWriteState!= 4)
							{
								DataNum-=i/2;
								WriteState |= 0x08;
								break;
							}
							else
							{
								DataNum ++;
								WriteState &= 0x07;
							}
						}																
					}	
					RCC_HSICmd(DISABLE);																					
				}	
				
				tx_data[1] = WriteState;	
				tx_data[0] = 0x02;												
				FLASH_Lock();
				DataNum = 0;													
				CanData.len = 2;
				ptx_data = tx_data;
			break;
				
		default :
			  rx_id = 0xFF;
			  tx_len   = 0x06;
			  ptx_data = tx_data;
			break;
	}
	CanData.len = tx_len;
	CanData.FuncID = rx_id;
	CanData.pdata = ptx_data;
	return CanData;
}
#endif
/*******************************ID HANDLE END*************************************************/


#if 0
static void setModulePowerOnOff( uint8_t module, uint8_t onoff )
{
  switch( module )
  {
  case MODULE_5V_EN:
    if( onoff == 0x01 ) {
      BSP_Power_OnOff( POWER_5V_EN, POWER_ON );
    } 
    else {
      BSP_Power_OnOff( POWER_5V_EN, POWER_OFF );
    }
    break;
  case MODULE_12V_HEAD_RK_EN:
    if( onoff == 0x01 ) {
      BSP_Power_OnOff( POWER_12V_HEAD_RK_EN, POWER_ON );
    } 
    else {
      BSP_Power_OnOff( POWER_12V_HEAD_RK_EN, POWER_OFF );
    }
    break;
  case MODULE_12V_CHEST_RK_EN:
    if( onoff == 0x01 ) {
      BSP_Power_OnOff( POWER_12V_CHEST_RK_EN, POWER_ON );
    } 
    else {
      BSP_Power_OnOff( POWER_12V_CHEST_RK_EN, POWER_OFF );
    }
    break;
  case MODULE_24V_LEFT_HAND_MOTER_EN:
    if( onoff == 0x01 ) {
      BSP_Power_OnOff( POWER_24V_LEFT_HAND_MOTER_EN, POWER_ON );
    } 
    else {
      BSP_Power_OnOff( POWER_24V_LEFT_HAND_MOTER_EN, POWER_OFF );
    }
    break;
  case MODULE_24V_RIGHT_HAND_MOTOR_EN:
    if( onoff == 0x01 ) {
      BSP_Power_OnOff( POWER_24V_RIGHT_HAND_MOTOR_EN, POWER_ON );
    } 
    else {
      BSP_Power_OnOff( POWER_24V_RIGHT_HAND_MOTOR_EN, POWER_OFF );
    }
    break;
  case MODULE_24V_HEAD_MOTOR_EN:
    if( onoff == 0x01 ) {
      BSP_Power_OnOff( POWER_24V_HEAD_MOTOR_EN, POWER_ON );
    } 
    else {
      BSP_Power_OnOff( POWER_24V_HEAD_MOTOR_EN, POWER_OFF );
    }
    break;
  case MODULE_12V_SWITCH_EN:
    if( onoff == 0x01 ) {
      BSP_Power_OnOff( POWER_12V_SWITCH_EN, POWER_ON );
    } 
    else {
      BSP_Power_OnOff( POWER_12V_SWITCH_EN, POWER_OFF );
    }
    break;
  case MODULE_12V_ROUTER_EN:
    if( onoff == 0x01 ) {
      BSP_Power_OnOff( POWER_12V_ROUTER_EN, POWER_ON );
    } 
    else {
      BSP_Power_OnOff( POWER_12V_ROUTER_EN, POWER_OFF );
    }
    break;
  case MODULE_12V_CARD_READER_EN:
    if( onoff == 0x01 ) {
      BSP_Power_OnOff( POWER_12V_CARD_READER_EN, POWER_ON );
    } 
    else {
      BSP_Power_OnOff( POWER_12V_CARD_READER_EN, POWER_OFF );
    }
    break;
  case MODULE_12V_AUDIO_PA_EN:
    if( onoff == 0x01 ) {
      BSP_Power_OnOff( POWER_12V_AUDIO_PA_EN, POWER_ON );
    } 
    else {
      BSP_Power_OnOff( POWER_12V_AUDIO_PA_EN, POWER_OFF );
    }
    break;
  case MODULE_5V_LEFT_HAND_MOTOR_EN:
    if( onoff == 0x01 ) {
      BSP_Power_OnOff( POWER_5V_LEFT_HAND_MOTOR_EN, POWER_ON );
    } 
    else {
      BSP_Power_OnOff( POWER_5V_LEFT_HAND_MOTOR_EN, POWER_OFF );
    }
    break;
  case MODULE_5V_RIGHT_HAND_MOTOR_EN:
    if( onoff == 0x01 ) {
      BSP_Power_OnOff( POWER_5V_RIGHT_HAND_MOTOR_EN, POWER_ON );
    } 
    else {
      BSP_Power_OnOff( POWER_5V_RIGHT_HAND_MOTOR_EN, POWER_OFF );
    }
    break;
  case MODULE_5V_HEAD_MOTOR_EN:
    if( onoff == 0x01 ) {
      BSP_Power_OnOff( POWER_5V_HEAD_MOTOR_EN, POWER_ON );
    } 
    else {
      BSP_Power_OnOff( POWER_5V_HEAD_MOTOR_EN, POWER_OFF );
    }
    break;
  case MODULE_5V_RESERVE_EN:
    if( onoff == 0x01 ) {
      BSP_Power_OnOff( POWER_5V_RESERVE_EN, POWER_ON );
    } 
    else {
      BSP_Power_OnOff( POWER_5V_RESERVE_EN, POWER_OFF );
    }
    break;
  case MODULE_5V_LEDX2_EN:
    if( onoff == 0x01 ) {
      BSP_Power_OnOff( POWER_5V_LEDX2_EN, POWER_ON );
    } 
    else {
      BSP_Power_OnOff( POWER_5V_LEDX2_EN, POWER_OFF );
    }
    break;
  case MODULE_5V_REPAIR_EN:
    if( onoff == 0x01 ) {
      BSP_Power_OnOff( POWER_5V_REPAIR_EN, POWER_ON );
    } 
    else {
      BSP_Power_OnOff( POWER_5V_REPAIR_EN, POWER_OFF );
    }
    break;
  case MODULE_5V_TOUCH_EN:
    if( onoff == 0x01 ) {
      BSP_Power_OnOff( POWER_5V_TOUCH_EN, POWER_ON );
    } 
    else {
      BSP_Power_OnOff( POWER_5V_TOUCH_EN, POWER_OFF );
    }
    break;
  case MODULE_5V_KEYBOARD_EN:
    if( onoff == 0x01 ) {
      BSP_Power_OnOff( POWER_5V_KEYBOARD_EN, POWER_ON );
    } 
    else {
      BSP_Power_OnOff( POWER_5V_KEYBOARD_EN, POWER_OFF );
    }
    break;
  case MODULE_5V_CARD_READER_EN:
    if( onoff == 0x01 ) {
      BSP_Power_OnOff( POWER_5V_CARD_READER_EN, POWER_ON );
    } 
    else {
      BSP_Power_OnOff( POWER_5V_CARD_READER_EN, POWER_OFF );
    }
    break;
  case MODULE_5V_CODE_EN:
    if( onoff == 0x01 ) {
      BSP_Power_OnOff( POWER_5V_CODE_EN, POWER_ON );
    } 
    else {
      BSP_Power_OnOff( POWER_5V_CODE_EN, POWER_OFF );
    }
    break;
    
    case MODULE_5V_CAMERA_EN:
    if( onoff == 0x01 ) {
      BSP_Power_OnOff( POWER_5V_CAMERA_EN, POWER_ON );
    } 
    else {
      BSP_Power_OnOff( POWER_5V_CAMERA_EN, POWER_OFF );
    }
    break;
  case MODULE_5V_HD_CAMERA_EN:
    if( onoff == 0x01 ) {
      BSP_Power_OnOff( POWER_5V_HD_CAMERA_EN, POWER_ON );
    } 
    else {
      BSP_Power_OnOff( POWER_5V_HD_CAMERA_EN, POWER_OFF );
    }
    break;
  case MODULE_5V_HEAD_LED_EN:
    if( onoff == 0x01 ) {
      BSP_Power_OnOff( POWER_5V_HEAD_LED_EN, POWER_ON );
    } 
    else {
      BSP_Power_OnOff( POWER_5V_HEAD_LED_EN, POWER_OFF );
    }
    break;
  case MODULE_24V_HD_CAMERA:
    if( onoff == 0x01 ) {
      BSP_Power_OnOff( POWER_24V_HD_CAMERA, POWER_ON );
    } 
    else {
      BSP_Power_OnOff( POWER_24V_HD_CAMERA, POWER_OFF );
    }
    break;
  case MODULE_24V_CAMERA:
    if( onoff == 0x01 ) {
      BSP_Power_OnOff( POWER_24V_CAMERA, POWER_ON );
    } 
    else {
      BSP_Power_OnOff( POWER_24V_CAMERA, POWER_OFF );
    }
    break;
    case MODULE_5V_HEAD_TOUCH:
    if( onoff == 0x01 ) {
      BSP_Power_OnOff( POWER_5V_HEAD_TOUCH, POWER_ON );
    } 
    else {
      BSP_Power_OnOff( POWER_5V_HEAD_TOUCH, POWER_OFF );
    }
    break;
    }
}
#endif


/*
static OSStatus recModuleControlFrameProcess( serial_t *serial )
{
  OSStatus err = kNoErr;
  recModuleControlFrame_t *recModuleControlFrame;

  

  recModuleControlFrame = (recModuleControlFrame_t *)serial->rx_buf.offset;
  require_action( recModuleControlFrame , exit, err = kGeneralErr );

  switch( recModuleControlFrame->module )
  {
    case MODULE_5V_EN:     
    case MODULE_12V_HEAD_RK_EN:
    case MODULE_RESERVE_1:

    case MODULE_12V_CHEST_RK_EN:
    case MODULER_RESERVE_2:
    case MODULE_24V_LEFT_HAND_MOTER_EN:
    case MODULE_24V_RIGHT_HAND_MOTOR_EN:

    case MODULE_24V_HEAD_MOTOR_EN:
    case MODULE_12V_SWITCH_EN:
    case MODULE_12V_ROUTER_EN:
    case MODULE_12V_CARD_READER_EN:

    case MODULE_12V_AUDIO_PA_EN:
    case MODULE_5V_LEFT_HAND_MOTOR_EN:
    case MODULE_5V_RIGHT_HAND_MOTOR_EN:
    case MODULE_5V_HEAD_MOTOR_EN:


    case MODULE_5V_RESERVE_EN:
    case MODULE_5V_LEDX2_EN:
    case MODULE_5V_REPAIR_EN:
    case MODULE_5V_TOUCH_EN:

    case MODULE_5V_KEYBOARD_EN:
    case MODULE_5V_CARD_READER_EN:
    case MODULE_5V_CODE_EN:
    case MODULE_5V_CAMERA_EN:

    case MODULE_5V_HD_CAMERA_EN:
    case MODULE_5V_HEAD_LED_EN:
    case MODULE_24V_HD_CAMERA:
    case MODULE_24V_CAMERA:

    case MODULE_5V_HEAD_TOUCH:
      SetModulePowerOnOff( recModuleControlFrame->module, recModuleControlFrame->control );
      break;

    default:
      //err = ackModuleControlFrameProcess( serial, HW_NO_SUPPORT );
      goto exit;
      break;
  }

  //err = ackModuleControlFrameProcess( serial, ACK_SUCCESS );

exit:
  return err;
}
*/

//////  function id define  //////
#define CAN_FUN_ID_RESET        0x06
#define CAN_FUN_ID_WRITE        0x01
#define CAN_FUN_ID_READ         0x02
#define CAN_FUN_ID_TRIGGER      0x03


//////  source id define  //////
#define CAN_SOURCE_ID_READ_VERSION      0x01
#define CAN_SOURCE_ID_MODULE_STATE      0x81
#define CAN_SOURCE_ID_ERROR_STATE       0x82
#define CAN_SOURCE_ID_READ_ADC_DATA     0x83
#define CAN_SOURCE_ID_READ_RK_STATE     0x84
#define CAN_SOURCE_ID_PWM_LED           0x85        


#define CAN_READ_DATA               0x80

void UploadAdcData(void)
{
    CAN_ID_UNION id;
    id.CanID_Struct.ACK = 1;
    id.CanID_Struct.DestMACID = 0;////
    id.CanID_Struct.FUNC_ID = CAN_FUN_ID_TRIGGER;
    id.CanID_Struct.SourceID = CAN_SOURCE_ID_READ_ADC_DATA;
    id.CanID_Struct.SrcMACID = CAN_SUB_PB_ID;////

    CanTX( MICO_CAN1, id.CANx_ID, (uint8_t *)voltageConvert, sizeof(voltageData_t) );
}

#define CMD_NOT_FOUND   0
uint16_t CmdProcessing(CAN_ID_UNION *id, const uint8_t *data_in, const uint16_t data_in_len, uint8_t *data_out)
{
    //uint8_t data_out_len;
    id->CanID_Struct.ACK = 1;   
    id->CanID_Struct.DestMACID = id->CanID_Struct.SrcMACID;
    id->CanID_Struct.SrcMACID = CAN_SUB_PB_ID;
    id->CanID_Struct.res = 0;
    //id->CanID_Struct.FUNC_ID = 
    switch(id->CanID_Struct.FUNC_ID)
    {
        case CAN_FUN_ID_RESET:
            platform_mcu_reset();
            break;
        case CAN_FUN_ID_WRITE:
        case CAN_FUN_ID_READ:
            switch(id->CanID_Struct.SourceID)
            {
                case CAN_SOURCE_ID_READ_VERSION:
                    data_out[0] = data_in[0];
                    if(data_in[0] == 1)//read software version
                    {
                        memcpy(&data_out[1], SW_VERSION, sizeof(SW_VERSION));
                        //return strlen(SW_VERSION) + 1;
                        return sizeof(SW_VERSION) + 1;
                    }
                    else if(data_in[0] == 2)//protocol version
                    {
                        memcpy(&data_out[1], PROTOCOL_VERSION, sizeof(PROTOCOL_VERSION));
                        return sizeof(PROTOCOL_VERSION) +1;
                        
                    }
                    else if(data_in[0] == 3)//hardware version
                    {
                        memcpy(&data_out[1], HW_VERSION, sizeof(HW_VERSION));
                        return sizeof(HW_VERSION) + 1;
                    }
                    return CMD_NOT_FOUND;
                    break;
                case CAN_READ_DATA:
                
                    break;
                case CAN_SOURCE_ID_MODULE_STATE:
                    data_out[0] = data_in[0];
                    if(data_in[0] == 1)//write
                    {
                        if(data_in[1] == 1)//group num = 1
                        {
                            BSP_Power_OnOff((PowerEnable_TypeDef)(*(uint32_t *)(&data_in[2])),(PowerOnOff_TypeDef)data_in[3]);
                            //SetModulePowerOnOff(data_in[1],data_in[2]);
                            data_out[1] = data_in[1];
                            return 0;
                        }                       
                    }
                    else if(data_in[0] == 2)//read
                    {
                        
                        if(data_in[1] == 1)//group num = 1
                        {
                            uint32_t tmp = GetModulePowerState(POWER_ALL);
                            data_out[1] = data_in[1];
                            memcpy( &data_out[2],(uint8_t *)(&tmp) , 4 );
                            
                            return 6;
                        }                           
                    }
                    return CMD_NOT_FOUND;
                    break;
                case CAN_SOURCE_ID_ERROR_STATE: 
                    memcpy( data_out, voltageConvertData->faultBitTemp, 5 );
                    return 5;
                    
                case CAN_SOURCE_ID_READ_ADC_DATA:  
                    voltageDebug.isNeedUpload = YES;
                    voltageDebug.uploadRate = data_in[0];
                    return 0;
                case CAN_SOURCE_ID_READ_RK_STATE:
                  #define DATA_NONE       0xff
                    if(data_in[0] == 1)//read
                    {
                        data_out[1] = GetRkState(POWER_HEAD_RK);
                        data_out[2] = GetRkState(POWER_CHEST_RK);
                    }
                    if(data_in[0] == 2)//write
                    {
                        if(data_in[1] != DATA_NONE)
                        {
                            data_out[1] = SetRkState(POWER_HEAD_RK, data_in[1]);
                        }
                        if(data_in[2] != DATA_NONE)
                        {
                            data_out[2] = SetRkState(POWER_CHEST_RK, data_in[2]);
                        }       
                    }
                     data_out[0] = data_in[0];
                     return 3;
                case CAN_SOURCE_ID_PWM_LED:
                    #define DATA_NONE       0xff
                    data_out[0] = data_in[0];
                    if(data_in[0] == 1)//read brightness
                    {
                        
                        if(data_in[1] != DATA_NONE)
                        {
                            data_out[1] = GetLedBrightness(LED1);
                        }
                        else
                        {
                            data_out[1] = DATA_NONE;
                        }   
                        
                        if(data_in[2] != DATA_NONE)
                        {
                            data_out[2] = GetLedBrightness(LED2);
                        }
                        else
                        {
                            data_out[2] = DATA_NONE;
                        }
                        
                    }
                    if(data_in[0] == 2)//write
                    {
                        if(data_in[1] != DATA_NONE)
                        {
                            data_out[1] = SetLedBrightness(LED1,data_in[1]);
                        }
                        else
                        {
                            data_out[1] = DATA_NONE;
                        }
                        
                        
                        if(data_in[2] != DATA_NONE)
                        {
                            data_out[2] = SetLedBrightness(LED2,data_in[2]);
                        }
                        else
                        {
                            data_out[2] = DATA_NONE;
                        }
                    }
                    return 3;
                default :
                    break;
            }


        default: 
        break;
    }

    return CMD_NOT_FOUND;
}

CAN_LONG_BUF_T can_long_frame_buf_ram;
CAN_LONG_BUF_T *can_long_frame_buf = &can_long_frame_buf_ram;

#define CAN_LONG_BUF_FULL   0xff
static uint8_t GetOneFreeBuf(void)
{
    for(uint8_t i = 0; i < CAN_LONG_BUF_NUM; i++)
    {
        if(can_long_frame_buf->can_rcv_buf[i].used_len == 0)
        {
            return i;
        }
    }
    return CAN_LONG_BUF_FULL;
}
static void FreeBuf(uint8_t index)
{
    can_long_frame_buf->can_rcv_buf[index].can_id = 0;
    can_long_frame_buf->can_rcv_buf[index].used_len = 0;
}
#define CAN_BUF_NO_THIS_ID      0xfe
static uint8_t GetTheBufById(uint32_t id)
{
    for(uint8_t i = 0; i < CAN_LONG_BUF_NUM; i++)
    {
        if(id == can_long_frame_buf->can_rcv_buf[i].can_id)
        {
            return i;
        }
    }
    return CAN_BUF_NO_THIS_ID;
}
void CanLongBufInit(void)
{ 
    can_long_frame_buf->GetOneFreeBuf = GetOneFreeBuf;
    can_long_frame_buf->GetTheBufById = GetTheBufById;
    can_long_frame_buf->FreeBuf = FreeBuf;
    
    //my_id = GetCanMacId();//test 
    
    FifoInit(can_fifo, can_pkg, CAN_FIFO_SIZE);
}

#define CAN_LONG_FRAME_TIME_OUT     5000/SYSTICK_PERIOD

#define CAN_COMM_TIME_OUT           5000
uint32_t can_comm_start_time;
void can_protocol_period( void )
{
    if(IsFifoEmpty(can_fifo) == FALSE)
    {  
        CAN_ID_UNION id;
        can_pkg_t can_pkg_tmp;
        uint16_t tx_len;
        //uint8_t test_data[] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28};
        CAN_DATA_UNION rx_buf;
        uint8_t buf_index;
        uint8_t seg_polo;
        uint8_t seg_num;
        uint8_t rx_data_len;
        
        FifoGetCanPkg(can_fifo, &can_pkg_tmp);
        
        memcpy(rx_buf.CanData,  can_pkg_tmp.data.CanData, can_pkg_tmp.len);
        id.CANx_ID = can_pkg_tmp.id.CANx_ID;
        seg_polo = can_pkg_tmp.data.CanData_Struct.SegPolo;
        seg_num = can_pkg_tmp.data.CanData_Struct.SegNum;
        rx_data_len = can_pkg_tmp.len;
        
        if(seg_polo == ONLYONCE)
        {
            //if( (id.CanID_Struct.SourceID < SOURCE_ID_PREPARE_UPDATE) && (id.CanID_Struct.SourceID > SOURCE_ID_CHECK_TRANSMIT) )
            {
                    //process the data here//
                    tx_len = CmdProcessing(&id, rx_buf.CanData_Struct.Data, rx_data_len - 1, CanTxdataBuff );
                    //process the data here//
                    
                    CanTX( MICO_CAN1, id.CANx_ID, CanTxdataBuff, tx_len );
                    //CanTX( MICO_CAN1, id.CANx_ID, test_data, sizeof(test_data) );
                    //CanTX( MICO_CAN1, id.CANx_ID, rx_buf.CanData, rx_data_len );
            }
        }
        else //long frame
        {
            for(uint8_t i = 0; i < CAN_LONG_BUF_NUM; i++)
            {
                if(can_long_frame_buf->can_rcv_buf[i].used_len > 0)
                {
                    if(os_get_time() - can_long_frame_buf->can_rcv_buf[i].start_time > CAN_LONG_FRAME_TIME_OUT)
                    {
                        can_long_frame_buf->FreeBuf(i);
                    }
                }     
            }
            
            if(seg_polo == BEGIAN)
            {
                buf_index = can_long_frame_buf->GetTheBufById(id.CANx_ID);
                if(buf_index == CAN_BUF_NO_THIS_ID)
                {
                    buf_index = can_long_frame_buf->GetOneFreeBuf();
                }
                else
                {
                    //
                }
                
                if((buf_index == CAN_LONG_BUF_FULL) || (buf_index >= CAN_LONG_BUF_NUM))
                {
                    CanProtocolLog("LONG FRAME RCV BUF IS FULL! ! ! !\r\n");
                    
                    goto exit;
                }
                memcpy(&can_long_frame_buf->can_rcv_buf[buf_index].rcv_buf[0], rx_buf.CanData_Struct.Data, CAN_ONE_FRAME_DATA_LENTH);
                can_long_frame_buf->can_rcv_buf[buf_index].used_len = CAN_ONE_FRAME_DATA_LENTH;
                can_long_frame_buf->can_rcv_buf[buf_index].can_id = id.CANx_ID;
                can_long_frame_buf->can_rcv_buf[buf_index].start_time = os_get_time();
                CanProtocolLog("begin\r\n");
            }
            else if((seg_polo == TRANSING) || (seg_polo == END))
            {
                buf_index = can_long_frame_buf->GetTheBufById(id.CANx_ID);
                if((buf_index == CAN_BUF_NO_THIS_ID) || (buf_index >= CAN_LONG_BUF_NUM))
                {
                    CanProtocolLog("ERROR ! !\r\n long buff index is %d",buf_index);
                    goto exit;
                }
                can_long_frame_buf->can_rcv_buf[buf_index].start_time = os_get_time();
                if(seg_polo == TRANSING)
                {
                    memcpy(&can_long_frame_buf->can_rcv_buf[buf_index].rcv_buf[seg_num*CAN_ONE_FRAME_DATA_LENTH], rx_buf.CanData_Struct.Data, CAN_ONE_FRAME_DATA_LENTH);
                    can_long_frame_buf->can_rcv_buf[buf_index].used_len += CAN_ONE_FRAME_DATA_LENTH;
                    CanProtocolLog("transing\r\n");
                }
                if(seg_polo == END)
                {
                    memcpy(&can_long_frame_buf->can_rcv_buf[buf_index].rcv_buf[seg_num*CAN_ONE_FRAME_DATA_LENTH], rx_buf.CanData_Struct.Data, rx_data_len - 1);
                    can_long_frame_buf->can_rcv_buf[buf_index].used_len += rx_data_len - 1; 
                    
                    //process the data here//
                    /**********************/
                    //process the data here//
                    
                    CanTX( MICO_CAN1, id.CANx_ID, can_long_frame_buf->can_rcv_buf[buf_index].rcv_buf, can_long_frame_buf->can_rcv_buf[buf_index].used_len);  // test :send the data back;             
                    can_long_frame_buf->FreeBuf(buf_index);
                    CanProtocolLog("end\r\n");
                }       
            }
        }
        
        
#if 0
        else if( CanData.FrameType == 1 )
        {
            if( CanData.CanID_U.SegmentNum <= 10 )//update prepare
            {
                if( CanData.CanID_U.ID != 0 )//muti data process
                {
                    CANUpDataLen = CanData.CanID_U.SegmentNum*8;
                    memcpy( &CanUpdataBuff[CanData.CanID_U.SegmentNum*8], RxMessage.Data, RxMessage.DLC );
                }
                else
                {
                    CANUpDataLen = CanData.CanID_U.SegmentNum*8 + RxMessage.DLC;
                    memcpy( &CanUpdataBuff[CanData.CanID_U.SegmentNum*8], RxMessage.Data, RxMessage.DLC );				
                    //update cmd process
                    TxCanData = FirmwareUpgrade( CanData.CanID_U.ID, CanUpdataBuff, CANUpDataLen );
                    CM_CAN_Tx( MICO_CAN1, CanData, TxCanData.pdata, TxCanData.len );
                    CANUpDataLen = 0;
                }
            }
            else//update data package
            {
                TxCanData = FirmwareUpgrade( CanData.CanID_U.ID, CanUpdataBuff, CANUpDataLen );
                CM_CAN_Tx( MICO_CAN1, CanData, TxCanData.pdata, TxCanData.len );
            }
        }
#endif
    }
   
    
exit:    
    return;
}






