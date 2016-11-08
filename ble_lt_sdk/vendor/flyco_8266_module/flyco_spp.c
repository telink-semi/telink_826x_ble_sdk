/*
 * flyco_spp.c
 *
 *  Created on: 2016-6-15
 *      Author: yafei.tu
 */

#include "../../proj/tl_common.h"
#include "../../proj/drivers/flash.h"
#include "../../proj/drivers/uart.h"
#include "../../proj_lib/ble/ble_common.h"
#include "../../proj_lib/pm.h"
#include "../../proj_lib/ble/ble_ll.h"
#include "flyco_spp.h"
#include "../../proj_lib/ble/blt_config.h"

/**********************************************************************
 * GLOBAL VARIABLES
 */
u8 flyco_version[4] = FLYCO_VERSION;
extern const u16 TelinkSppServiceUUID;
extern const u16 TelinkSppDataServer2ClientUUID;
extern const u16 TelinkSppDataClient2ServiceUUID;
extern u8 module_EnterSleepOnConnTerminate(void *arg);

extern u8  ble_devName[MAX_DEV_NAME_LEN];
extern u8 rfpower;
extern u8 spp_s2c_hdl;
u8 bls_adv_enable;

flyco_spp_AppCallbacks_t *flyco_spp_cbs;

u8 devNameT[26] = {0};//Temporary storage device name, data structure:[12��DevName1��6��DevName2]
u8 set_devname1_flg = 0;//Set devname1 flag

#if( TL_LIB_8266 || MCU_CORE_TYPE == MCU_CORE_8266)
	#define	UART_SEND  			     uart_Send_kma
#else
    #define	UART_SEND				 uart_Send
#endif

/**********************************************************************
 * LOCAL VARIABLES
 */
//Notice:flash_erase_sector should place at user_init(), Do Not user this function in main_loop()!!!
 int adv_interval_index;
 int rf_power_index;
 int adv_timeout_index;
 int adv_data_index;
 int devname_index;
 int devname1_index;
 int devname2_index;
 int identified_index;
 int baudrate_index;

/////////////////////////////////////FLASH data save and read management////////////////////////////////
// FLASH struct : index | 00 | data[30]
//read user data in FLASH
void flyco_load_para_addr(u32 addr, int* index, u8* p, u8 len){
	int idx=0;
	for (idx=0; idx < 4*1024; idx+=32)//4K per sector
	{
		if (*(u16 *)(addr+idx) == U16_MAX)	//end
		{
			break;
		}
	}

	idx -= 32;

	if(idx < 0 || idx == 4*1024){
		return;
	}

	*index = idx;
	memcpy (p, (u32 *)(addr+idx+2), len);
}
//erase user data in FLASH
void flyco_erase_para(u32 addr, int* index){
	if(*index>= 4*1024){
		nv_manage_t p;
		p.curNum = 0x01;
		memcpy (p.data, (u32*)(addr+ *index + 2), 30);
		*index = 0;
		flash_erase_sector(addr);
		extern void save_para(u32 addr, int* index, u8* buf, u16 len);
		save_para(addr, index, (u8*)&p, 32);
	}
}

void save_para(u32 addr, int* index, u8* buf, u16 len){
	if(*index-32 >= 0){
		u8 clr[2] = {0};
		flash_write_page(addr + *index - 32, 2, clr);
	}
	nv_manage_t p;
	p.curNum = 0x01;
	p.tmp    = 0xFF;
	memcpy(p.data, buf, len);
	flash_write_page((addr + *index), len + 2, (u8*)&p);
	*index += 32;
}

u8 nv_write(u8 id, u8 *buf, u16 len){
	switch(id){
	case NV_FLYCO_ITEM_BAUD_RATE:
		save_para(BAUD_RATE_ADDR, &baudrate_index, buf, len);
	break;
	case NV_FLYCO_ITEM_RF_POWER:
		save_para(RF_POWER_ADDR, &rf_power_index, buf, len);
	break;
	case NV_FLYCO_ITEM_IDENTIFIED:
		save_para(IDENTIFIED_ADDR, &identified_index, buf, len);
	break;
	case NV_FLYCO_ITEM_ADV_TIMEOUT:
		save_para(ADV_TIMEOUT_ADDR, &adv_timeout_index, buf, len);
	break;
	case NV_FLYCO_ITEM_DEV_NAME1:
		save_para(DEV_NAME1_ADDR, &devname1_index, buf, len);
	break;
	case NV_FLYCO_ITEM_DEV_NAME2:
		save_para(DEV_NAME2_ADDR, &devname2_index, buf, len);
	break;
	case NV_FLYCO_ITEM_DEV_NAME:
		save_para(DEV_NAME_ADDR, &devname_index, buf, len);
	break;
	case NV_FLYCO_ITEM_ADV_DATA:
		save_para(ADV_DATA_ADDR, &adv_data_index, buf, len);
	break;
	case NV_FLYCO_ITEM_ADV_INTERVAL:
		save_para(ADV_INTERVAL_ADDR, &adv_interval_index, buf, len);
	break;
   }
   return 0;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////the code below is just for demonstration of the event callback only////////////
void event_handler(u32 h, u8 *para, int n)
{
	if((h&HCI_FLAG_EVENT_TLK_MODULE)!= 0)			//module event
	{
		switch((u8)(h&0xff))
		{
			case BLT_EV_FLAG_ADV:
			{
				gpio_write (BLE_STA_OUT, 0);
			}
			break;
			case BLT_EV_FLAG_SCAN_RSP:
				break;
			case BLT_EV_FLAG_CONNECT:
			{
				gpio_write (BLE_STA_OUT, 1);
				bls_l2cap_requestConnParamUpdate (16, 40, 0, 400);  //interval=20ms~50ms latency=0 timeout=4s
			}
				break;
			case BLT_EV_FLAG_TERMINATE:
			{
				gpio_write(BLE_STA_OUT,0);//LOW
			}
				break;
			case BLT_EV_FLAG_PAIRING_BEGIN:
				break;
			case BLT_EV_FLAG_PAIRING_FAIL:
				break;
			case BLT_EV_FLAG_ENCRYPTION_CONN_DONE:
				break;
			case BLT_EV_FLAG_USER_TIMER_WAKEUP:
				break;
			case BLT_EV_FLAG_GPIO_EARLY_WAKEUP:
				break;
			case BLT_EV_FLAG_CHN_MAP_REQ:
				break;
			case BLT_EV_FLAG_CONN_PARA_REQ:
				break;
			case BLT_EV_FLAG_CHN_MAP_UPDATE:
				break;
			case BLT_EV_FLAG_CONN_PARA_UPDATE:
				break;
			case BLT_EV_FLAG_SET_WAKEUP_SOURCE:
				bls_pm_setWakeupSource(PM_WAKEUP_CORE|PM_WAKEUP_PAD);
				break;
			case BLT_EV_FLAG_ADV_DURATION_TIMEOUT:
				break;
			default:
				break;
		}
	}
}

/////////////////////////////////////spp process ///////////////////////////////////////////////////
MYFIFO_INIT(hci_tx_fifo, 72, 8);
//#define		UART_WB_NUM			8
//u8		uart_wb[UART_WB_NUM][64];
//u8		uart_wb_wptr = 0;
//u8		uart_wb_rptr = 0;
//u8		uart_rb[256];

int	flyco_uart_push_fifo (u16 st, int n, u8 *p)
{
	u8 *pw = my_fifo_wptr (&hci_tx_fifo);
	if (!pw || n >= hci_tx_fifo.size)
	{
		return -1;
	}

	pw[0] = n + 3;
	pw[1] = st;
	pw[2] = st >> 8;
	pw[3] = n;
	if (n)
	{
		memcpy (pw + 4, p, n);
	}
	my_fifo_next (&hci_tx_fifo);
	return 0;
//	int num = (uart_wb_wptr - uart_wb_rptr) & 31;
//	if (num >= UART_WB_NUM)
//	{
//		return 0;
//	}
//	u8 *pw = uart_wb[uart_wb_wptr++ & (UART_WB_NUM - 1)];
//	pw[0] = n + 3;
//	pw[1] = st;
//	pw[2] = st >> 8;
//	pw[3] = n;
//	if (n)
//	{
//		memcpy (pw + 4, p, n);
//	}
//	return 1;
}

uart_data_t T_rxdata_for_flyco;
int flyco_rx_from_uart (void)//UART data send to Master,we will handler the data as CMD or DATA
{
	if(rx_uart_w_index==rx_uart_r_index)  //rx buff empty
	{
        return 0;
	}

	u32 rx_len = T_rxdata_buf[rx_uart_w_index].len + 4 > sizeof(T_rxdata_for_flyco) ? sizeof(T_rxdata_for_flyco) : T_rxdata_buf[rx_uart_w_index].len + 4;
	memcpy(&T_rxdata_for_flyco, &T_rxdata_buf[rx_uart_w_index], rx_len);
	memset(&T_rxdata_buf[rx_uart_w_index],0,sizeof(uart_data_t));

	if (rx_len)
	{
		flyco_module_uartCmdHandler(T_rxdata_for_flyco.data, rx_len - 4);//handler SPP CMD/DATA
	}
	rx_uart_w_index = (rx_uart_w_index + 1)&0x01;

	return 0;
}

int flyco_tx_to_uart ()//Master data send to UART,we will handler the data as CMD or DATA
{
	u8 *p = my_fifo_get (&hci_tx_fifo);

	extern unsigned char uart_tx_is_busy();
	if (p && !uart_tx_is_busy ())
	{
		flyco_module_masterCmdHandler(p+4, p[3]);
		my_fifo_pop (&hci_tx_fifo);
	}
    return 0;
//	if (uart_wb_rptr != uart_wb_wptr && !uart_tx_is_busy ())
//	{
//		u8 *p = uart_wb[uart_wb_rptr & (UART_WB_NUM-1)];
//
//		flyco_module_masterCmdHandler(p+4, p[3]);
//
//		uart_wb_rptr++;
//	}
//    return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////

/**********************************************************************
 * LOCAL FUNCTIONS
 */
void flyco_spp_onModuleCmd(flyco_spp_cmd_t *pp);
void flyco_spp_cmdHandler(u8 *pCmd);
void flyco_spp_dataHandler(u8 *pData, u16 len);
void flyco_spp_module_rsp2cmd(u8 cmdid, u8 *payload, u8 len);

void flyco_spp_dataReceivedMasterHandler(u8 *data, u32 len);
void flyco_spp_cmdReceivedMasterHandler(u8 *pCmd);
void flyco_spp_onModuleReceivedMasterCmd(flyco_spp_cmd_t *pp);
void flyco_spp_received_master_rsp2cmd(u8 cmdid, u8 *payload, u8 len);

#if HEARTBEAT_FLYCO
int module_wait_forDisconnectTimer1Cb(void *arg);// for Disconnect ack mobile phone for FLYCO
#endif

/*********************************************************************
 * @fn      reverse_data
 *
 * @brief   reverse the data.
 *
 * @param   None
 *
 * @return  None
 */
void reverse_data(u8 *p,u8 len,u8*rp){
	for(u8 i=0;i<len;i++)
		*(rp+len-1-i)=*(p+i);
}

void flyco_module_uartCmdHandler(unsigned char* p, u32 len){

	if(IS_FLYCO_SPP_DATA(p)){////DATA    !IS_FLYCO_SPP_CMD(p)) {
		bls_att_pushNotifyData(spp_s2c_hdl, p, len);
	}
	else if(IS_FLYCO_SPP_CMD(p)){ //CMD
		flyco_spp_cmd_t* p_temp = (flyco_spp_cmd_t*)p;
		//Check whether the data length is qualified, frame header length 7byte data length check bit 1byte
		if(len == ((p_temp->len) + OFFSETOF(flyco_spp_cmd_t, data) + 1)){
			flyco_spp_cmdHandler(p);
		}
    }
}

void flyco_module_masterCmdHandler(u8 *p, u32 len){
	if(IS_FLYCO_SPP_DATA(p)){////DATA    !IS_FLYCO_SPP_CMD(p)) {
		flyco_spp_dataReceivedMasterHandler(p, len);
	}
	else if(IS_FLYCO_SPP_CMD(p)){ //CMD
		flyco_spp_cmd_t* p_temp = (flyco_spp_cmd_t*)p;
		//Check whether the data length is qualified, frame header length 7byte data length check bit 1byte
		if(len == ((p_temp->len) + OFFSETOF(flyco_spp_cmd_t, data) + 1)){
			flyco_spp_cmdReceivedMasterHandler(p);
		}
	}
}
/*********************************************************************
 * @fn      flyco_spp_onModuleCmd
 *
 * @brief   Module command handler.
 *
 * @param   None
 *
 * @return  None
 */
u32 spp_cmd_restart_tick;
u32 spp_cmd_disconnect_tick;
u32 spp_cmd_deep_sleep_tick;
u32 spp_cmd_get_rssi_tick;
u32 spp_cmd_set_baudrate_tick;

u8 spp_cmd_restart_flg;
u8 spp_cmd_disconnect_flg;
u8 spp_cmd_deep_sleep_flg;
u8 spp_cmd_get_rssi_flg;
u8 spp_cmd_set_baudrate_flg;
u32 baud_rate_tmp = 0;
void flyco_spp_onModuleCmd(flyco_spp_cmd_t *pp) {

	switch(pp->cmdID) {
		case FLYCO_SPP_CMD_MODULE_GET_MAC:{

			u8 rp[6],tmp_mac [6];
			if(pp->len == 0){
				bls_ll_readBDAddr(tmp_mac);
				reverse_data(tmp_mac,BLE_ADDR_LEN,rp);
				flyco_spp_module_rsp2cmd(pp->cmdID, rp, BLE_ADDR_LEN);
			}
		}
		break;

		case FLYCO_SPP_CMD_MODULE_GET_RSSI:{

			if(pp->len == 0){
				//extern u8 blt_state;
				u8 rssi;
				if(bls_ll_getCurrentState() != BLS_LINK_STATE_CONN)
					rssi = 0x80;//If not connected, according to FLYCO cmd provision rssi=0x80;
				else
					rssi = -110;//ble_rssi - 110; //The reference range:-85dB��RSSI��5dB

#if HEARTBEAT_FLYCO
				spp_cmd_get_rssi_tick = clock_time();
				spp_cmd_get_rssi_flg = 1;
#endif
				//if(rssi == 0x80 || (-85 <= rssi && rssi <= 5))
				flyco_spp_module_rsp2cmd(pp->cmdID, &rssi, 1);
			}
		}
		break;

		case FLYCO_SPP_CMD_GET_MODULE_STATE:{

			u8 connectStatus = 0x00;
			if(pp->len == 0){
				//extern u8 blt_state;
				if(bls_ll_getCurrentState() == BLS_LINK_STATE_CONN)
					connectStatus = 0x01;
				flyco_spp_module_rsp2cmd(pp->cmdID, &connectStatus, 1);
			}
		}
		break;

		case FLYCO_SPP_CMD_MODULE_RESTART:{//Do not send the instruction frequently,otherwise the ble module maybe crash!

			if(pp->len == 0){
				flyco_spp_module_rsp2cmd(pp->cmdID, NULL, 0);
				//restart after result sent
				spp_cmd_restart_tick = clock_time();
				spp_cmd_restart_flg = 1;
			}
		}
		break;

		case FLYCO_SPP_CMD_MODULE_DISCONNECT:{

			if(pp->len == 0){
				flyco_spp_module_rsp2cmd(pp->cmdID, NULL, 0);
				//disconnect after result sent
				spp_cmd_disconnect_tick = clock_time();
				spp_cmd_disconnect_flg = 1;
			}
		}
		break;

		case FLYCO_SPP_CMD_MODULE_SET_SLEEP:{

			if(pp->len == 0){
				flyco_spp_module_rsp2cmd(pp->cmdID, NULL, 0);
				spp_cmd_deep_sleep_tick  = clock_time();
				spp_cmd_deep_sleep_flg = 1;
			}
		}
		break;

		case FLYCO_SPP_CMD_MODULE_GET_DEVNAME1:{

			if(pp->len == 0){
				u8 devName1[20] = {DEFLUT_DEV_NAME1_LEN, DEFLUT_DEV_NAME1};
				flyco_load_para_addr(DEV_NAME1_ADDR, &devname1_index, devName1, 20);

				flyco_spp_module_rsp2cmd(pp->cmdID, devName1 + 1, devName1[0]);
			}
		}
		break;

		case FLYCO_SPP_CMD_MODULE_GET_DEVNAME2:{
			if(pp->len == 0){
				u8 devName2[20] = {DEFLUT_DEV_NAME2_LEN, DEFLUT_DEV_NAME2};
				flyco_load_para_addr(DEV_NAME2_ADDR, &devname2_index, devName2, 20);

				flyco_spp_module_rsp2cmd(pp->cmdID, devName2 + 1, devName2[0]);
			}
		}
		break;

		case FLYCO_SPP_CMD_MODULE_SET_DEVNAME1:{

			if(pp->len == 0x0C){//data length of 12 bytes, 8 bytes of additional CMD and check data
				flyco_spp_set_dev_name *p = (flyco_spp_set_dev_name *)pp;
				devNameT[0] = 12;
				//To ensure that the device name 1 length is 12 bytes, insufficient fill space
				memcpy(devNameT + 1, p->data, 12);//first 1+12 Bytes
				set_devname1_flg = 1;
				flyco_spp_module_rsp2cmd(pp->cmdID, NULL, 0);
			}
		}
		break;

		case FLYCO_SPP_CMD_MODULE_SET_DEVNAME2:{

			if(pp->len == 0x06){//Data length must be 6 bytes, the remaining fill space, the last 1 bytes of null
				flyco_spp_set_dev_name *p = (flyco_spp_set_dev_name *)pp;
				devNameT[13] = 6;
				//To ensure that the device name 2 length is 6 bytes, insufficient fill space
				memcpy(devNameT + 14, p->data, 6);//last 1+6 Bytes

				flyco_spp_module_rsp2cmd(pp->cmdID, NULL, 0);

				if(set_devname1_flg){//Merge tow device names, configure the Bluetooth device name
				    set_devname1_flg = 0; 
					u8 devName[20];
					u8 devNameTmp1[20], devNameTmp2[20];

					memcpy(devNameTmp1, devNameT, 13);// * 1 1 1 1 1 1 1 1 1 1 1 1��* 1 1 1 1 space Null��
					memcpy(devNameTmp2, devNameT + 13, 7);

					nv_write(NV_FLYCO_ITEM_DEV_NAME1, devNameTmp1, 20);
					nv_write(NV_FLYCO_ITEM_DEV_NAME2, devNameTmp2, 20);

					devName[0] = 12 + 6 + 1;
					devName[1] = 0x09;
					memcpy(devName + 2, devNameT + 1, 12);
					memcpy(devName + 14, devNameT + 14, 6);
					nv_write(NV_FLYCO_ITEM_DEV_NAME, devName, 20);

					memset(ble_devName, 0, MAX_DEV_NAME_LEN); //clear device name
					memcpy(ble_devName, devName, devName[0]+1); //cop data to att table

					bls_ll_setScanRspData(devName, devName[0]+1); //change scan response device name

					//restart ble module
				}

			}
		}
		break;

		case FLYCO_SPP_CMD_MODULE_GET_BAUDRATE:{

			if(pp->len == 0){
				u8 param[3] = {0x25, 0x80, 0xef};//Default bode rate:9600
				flyco_load_para_addr(BAUD_RATE_ADDR, &baudrate_index, param, 3);
				flyco_spp_module_rsp2cmd(pp->cmdID, param, ((param[2]!=0xef)?3:2));
			}
		}
		break;

		case FLYCO_SPP_CMD_MODULE_SET_BAUDRATE :{

			u32 baudrate = 0;
			u8 chkparam =0;
			flyco_spp_cmd_baud_rate_t *p = (flyco_spp_cmd_baud_rate_t *)pp;
			for(u8 i = 0; i< p->len; i++)
				baudrate += ((u32)p->rate[i])<<(8*(p->len-1-i));
			if(p->len == 2)
				p->rate[2] = 0xef;

			switch (baudrate){//parameter checkout;
			case 9600:
			case 115200:
				break;
			default:
				chkparam = 1;
			}
			if(!chkparam){
				nv_write(NV_FLYCO_ITEM_BAUD_RATE, p->rate, 3); //store baud rate param
				flyco_spp_module_rsp2cmd(pp->cmdID, NULL, 0);
			    spp_cmd_set_baudrate_tick = clock_time();//Delay 50ms to reply to the module to receive the characteristic code
			    spp_cmd_set_baudrate_flg = 1;
			    baud_rate_tmp = baudrate;
			}
		}
		break;

		case FLYCO_SPP_CMD_MODULE_GET_ADV_ENABLE_FLAG:{

			if(pp->len == 0){
				flyco_spp_module_rsp2cmd(pp->cmdID, &bls_adv_enable, 1);
		    }
		}
		break;

		case FLYCO_SPP_CMD_MODULE_SET_ADV_ENABLE_FLAG:{// cmdid = 0x0b
			flyco_spp_moduleCmd_advEnable_t *p = (flyco_spp_moduleCmd_advEnable_t *)pp;
			if(pp->len == 1){
				(p->enable) &= 0x01;//parameter checkout; Minimum byte valid 0 or 1
				bls_ll_setAdvEnable(p->enable);
				flyco_spp_module_rsp2cmd(pp->cmdID, NULL, 0);
			}

		}
		break;

		case FLYCO_SPP_CMD_MODULE_GET_IDENTIFIED:{

			if(pp->len == 0){
				u8 identified[6];
				flyco_load_para_addr(IDENTIFIED_ADDR, &identified_index, identified, 6);
				flyco_spp_module_rsp2cmd(pp->cmdID, identified, 6);
			}
		}
		break;

		case FLYCO_SPP_CMD_MODULE_SET_IDENTIFIED:{

			if(pp->len == 6){
				flyco_spp_cmd_identified_t *p = (flyco_spp_cmd_identified_t *)pp;
				nv_write(NV_FLYCO_ITEM_IDENTIFIED, p->data, 6); //store identified code param

				flyco_spp_module_rsp2cmd(pp->cmdID, NULL, 0);
			}
		}
		break;

		case FLYCO_SPP_CMD_MODULE_GET_ADV_DATA:{

			if(pp->len == 0){
				//The first value of the broadcast array is the total length of the broadcast data!
				u8 advData[20] = {5, 'F', 'L', 'Y', 'C', 'O'};//FLYCO default adv data��FLYCO
				flyco_load_para_addr(ADV_DATA_ADDR, &adv_data_index, advData, 20);

				flyco_spp_module_rsp2cmd(pp->cmdID, advData + 1, advData[0]);
			}
		}
		break;

		case FLYCO_SPP_CMD_MODULE_SET_ADV_DATA:{

			if(pp->len < 20){//Maximum 20 bytes
				flyco_spp_cmd_adv_data_t *p = (flyco_spp_cmd_adv_data_t *)pp;
				u8 len = (u8)p->len;

				bls_ll_setAdvData(p->data, len);

				u8 advData[20] = {0};
				advData[0] = len;
				memcpy(advData + 1, p->data, len);
				nv_write(NV_FLYCO_ITEM_ADV_DATA, advData, 20);

				flyco_spp_module_rsp2cmd(pp->cmdID, NULL, 0);
			}
		}
		break;

		case FLYCO_SPP_CMD_MODULE_GET_ADV_INTV:{

			if(pp->len == 0){
				u32 advInterval =((u32)DEFLUT_ADV_INTERVAL *5) >>3;
				u8 rp[2];
				flyco_load_para_addr(ADV_INTERVAL_ADDR, &adv_interval_index, (u8 *)&advInterval, 4);
				advInterval = (advInterval *5) >>3;//unit:0.625ms
				reverse_data((u8*)&advInterval,2,rp);
				flyco_spp_module_rsp2cmd(pp->cmdID, rp, 2);
			}
		}
		break;

		case FLYCO_SPP_CMD_MODULE_SET_ADV_INTV:{

			if(pp->len == 2){
				u32 para = 0;
				flyco_spp_cmd_adv_intv_t *p = (flyco_spp_cmd_adv_intv_t *)pp;

				for(u8 i = 0; i< p->len; i++)
					para += ((u32)p->intv[i])<<(8*(p->len-1-i));
				para = (para <<3) / 5;//unit:0.625ms! 160 corresponding to 100ms

				blt_set_advinterval(para);

				nv_write(NV_FLYCO_ITEM_ADV_INTERVAL, (u8 *)&para, 2);

				flyco_spp_module_rsp2cmd(pp->cmdID, NULL, 0);
			}
		}
		break;

		case FLYCO_SPP_CMD_MODULE_GET_RF_PWR:{

			if(pp->len == 0){
				u8 rf_power = rfpower;
				flyco_load_para_addr(RF_POWER_ADDR, &rf_power_index, (u8 *)&rf_power, 1);
				flyco_spp_module_rsp2cmd(pp->cmdID, &rf_power, 1);
			}
		}
		break;

		case FLYCO_SPP_CMD_MODULE_SET_RF_PWR:{

			if(pp->len == 1){
				flyco_spp_cmd_rf_pwr_t *p = (flyco_spp_cmd_rf_pwr_t *)pp;
				u8 new_rfpower = (u8) p->pwr;
				if(1 <= new_rfpower && new_rfpower <= 8){//Transmit power: 1-8 level, constraint
					rf_set_power_level_index(new_rfpower);
					nv_write(NV_FLYCO_ITEM_RF_POWER, (u8*)&new_rfpower, 1);

					flyco_spp_module_rsp2cmd(pp->cmdID, NULL, 0);
				}
			}
		}
		break;

		case FLYCO_SPP_CMD_MODULE_GET_VERSION_NUM:{

			if(pp->len == 0){
				flyco_spp_module_rsp2cmd(pp->cmdID, flyco_version, 4);
			}
		}
		break;

		case FLYCO_SPP_CMD_MODULE_GET_ADV_TIMEOUT:{

			u32 timeout = DEFLUT_ADV_TIMEOUT;
			if(pp->len == 0){
				flyco_load_para_addr(ADV_TIMEOUT_ADDR, &adv_timeout_index, (u8 *)&timeout, 4);

				timeout /=1000;
				flyco_spp_module_rsp2cmd(pp->cmdID, (u8*)&timeout, 1);
			}
		}
		break;

		case FLYCO_SPP_CMD_MODULE_SET_ADV_TIMEOUT:{//Adv time set, boot or wake up after the parameter effect!
			if(pp->len == 1){
				flyco_spp_cmd_adv_timeout_t *p = (flyco_spp_cmd_adv_timeout_t *)pp;
				u32 timeout = ((u32)p->tim) * 1000;//unit��ms, enlarge 1000��timeout unit:s
				nv_write(NV_FLYCO_ITEM_ADV_TIMEOUT, (u8 *)&timeout, 4);

				flyco_spp_module_rsp2cmd(pp->cmdID, NULL, 0);
			}
		}
		break;

		case FLYCO_SPP_CMD_MODULE_GET_SERVICE_UUID:{

			u16 service_uuid = TelinkSppServiceUUID, rp;
			reverse_data((u8*)&service_uuid, 2, (u8*)&rp);
			if(pp->len == 0){
				flyco_spp_module_rsp2cmd(pp->cmdID, (u8*)&rp, 2);
			}
		}
		break;

		case FLYCO_SPP_CMD_MODULE_GET_SERVICE_WRITE_UUID:{

			u16 service_write_uuid = TelinkSppDataClient2ServiceUUID, rp;
			reverse_data((u8*)&service_write_uuid, 2, (u8*)&rp);
			if(pp->len == 0){
				flyco_spp_module_rsp2cmd(pp->cmdID, (u8*)&rp, 2);
			}
		}
		break;

		case FLYCO_SPP_CMD_MODULE_GET_SERVICE_NOTIFY_UUID:{

			u16 service_notify_uuid = TelinkSppDataServer2ClientUUID, rp;
			reverse_data((u8*)&service_notify_uuid, 2, (u8*)&rp);
			if(pp->len == 0){
				flyco_spp_module_rsp2cmd(pp->cmdID, (u8*)&rp, 2);
			}
		}
		break;

		default://Instruction not defined!
		break;
	}

}

/*********************************************************************
 * @fn      flyco_spp_cmdHandler
 *
 * @brief   Command handler for recevied SPP command.
 *
 * @param   None
 *
 * @return  None
 */
void flyco_spp_cmdHandler(u8 *pCmd) {

	flyco_spp_cmd_t *p = (flyco_spp_cmd_t *)pCmd;
	u8 dataSum=0,check=0;
	for(u8 i=0;i<p->len;i++)dataSum += p->data[i];
	check = dataSum + p->cmdID + p->len;

	if((0 <= p->cmdID && p->cmdID < FLYCO_SPP_CMD_MODULE_MAX) //FLYCO CMD ID from 0 to FLYCO_SPP_CMD_MODULE_MAX-1.
	                  && (check == p->data[p->len])) {
		flyco_spp_onModuleCmd(p);
	}

}

/*********************************************************************
 * @fn      flyco_spp_module_rsp2cmd
 *
 * @brief   Reply to the instructions sent by MCU.
 *
 * @param   None
 *
 * @return  None
 */
void flyco_spp_module_rsp2cmd(u8 cmdid, u8 *payload, u8 len) {
	u8 ptmp[24];
	u8 sign[5]={0x66,0x65,0x69,0x6b,0x65};
    u8 dataSum = 0;

    flyco_spp_rsp2cmd_t * pEvt = (flyco_spp_rsp2cmd_t *)(ptmp + 4);

	memcpy(pEvt->signature,sign,5);
	pEvt->cmdID = cmdid;
	pEvt->len   = len ;
	for(u8 i=0;i<len;i++)
	   dataSum += payload[i];
	pEvt->data[len] = cmdid + dataSum + pEvt->len;//check
	memcpy(pEvt->data, payload, len);

	uart_data_t* pp = (uart_data_t*)ptmp;

	pp->len = (pEvt->len) + 8;//5(signature)+1(cmdid)+1(length)+len(data length)+1(chk)

	UART_SEND((u8 *)ptmp);
}

/*********************************************************************
 * @fn      flyco_spp_received_master_rsp2cmd
 *
 * @brief   Reply to the instructions sent by mobile phone app.
 *
 * @param   None
 *
 * @return  None
 */
void flyco_spp_received_master_rsp2cmd(u8 cmdid, u8 *payload, u8 len){
	u8 ptmp[20];
	u8 sign[5]={0x66,0x65,0x69,0x6b,0x65};
	u8 dataSum = 0;

	flyco_spp_rsp2cmd_t * pEvt = (flyco_spp_rsp2cmd_t *)(ptmp);

	memcpy(pEvt->signature,sign,5);
	pEvt->cmdID = cmdid;
	pEvt->len   = len ;
	for(u8 i=0;i<len;i++)
	   dataSum += payload[i];
	pEvt->data[len] = cmdid + dataSum + pEvt->len;//check
	memcpy(pEvt->data, payload, len);

	bls_att_pushNotifyData(spp_s2c_hdl, (u8 *)pEvt, (pEvt->len) + 8);//5(signature)+1(cmdid)+1(length)+len(data length)+1(chk)

}

/*********************************************************************
 * @fn      flyco_spp_dataReceivedMasterHandler
 *
 * @brief   Mobile app send data directly to the serial port(SPP mode).
 *
 * @param   None
 *
 * @return  None
 */
void flyco_spp_dataReceivedMasterHandler(u8 *data, u32 len) {
	extern uart_data_t T_txdata_buf;

	memcpy(&T_txdata_buf.data, data, len);
	T_txdata_buf.len = len;

	UART_SEND((u8 *)(&T_txdata_buf));
}

/*********************************************************************
 * @fn      flyco_spp_cmdReceivedMasterHandler
 *
 * @brief   Handle the command sent by mobile phone app.
 *
 * @param   None
 *
 * @return  None
 */
void flyco_spp_cmdReceivedMasterHandler(u8 *pCmd){

	flyco_spp_cmd_t *p = (flyco_spp_cmd_t *)pCmd;
	u8 dataSum=0,check=0;
	for(u8 i=0;i<p->len;i++)
		dataSum += p->data[i];
	check = dataSum + p->cmdID + p->len;

	if((0 <= p->cmdID && p->cmdID < FLYCO_SPP_CMD_MODULE_MAX) //FLYCO CMD ID from 0 to FLYCO_SPP_CMD_MODULE_MAX-1.
					  && (check == p->data[p->len])) {
		flyco_spp_onModuleReceivedMasterCmd(p);
	}

}

void flyco_spp_onModuleReceivedMasterCmd(flyco_spp_cmd_t *pp) {

	switch(pp->cmdID) {
		case FLYCO_SPP_CMD_MODULE_GET_MAC:{

			u8 rp[6],tmp_mac [6];
			if(pp->len == 0){
				bls_ll_readBDAddr(tmp_mac);
				reverse_data(tmp_mac,BLE_ADDR_LEN,rp);
				flyco_spp_received_master_rsp2cmd(pp->cmdID, rp, BLE_ADDR_LEN);
			}
		}
		break;

		case FLYCO_SPP_CMD_MODULE_GET_RSSI:{

			if(pp->len == 0){

#if HEARTBEAT_FLYCO
				spp_cmd_get_rssi_tick = clock_time();
				spp_cmd_get_rssi_flg = 1;
#endif
				//extern u8 blt_state;
				u8 rssi;
				if(bls_ll_getCurrentState() != BLS_LINK_STATE_CONN)
					rssi = 0x80;//If not connected, according to FLYCO cmd provision rssi=0x80;
				else
					rssi = -40;//ble_rssi - 110; //The reference range:-85dB��RSSI��5dB
				//if(rssi == 0x80 || (-85 <= rssi && rssi <= 5))
				flyco_spp_received_master_rsp2cmd(pp->cmdID, &rssi, 1);
			}
		}
		break;

		case FLYCO_SPP_CMD_GET_MODULE_STATE:{

			u8 connectStatus = 0x00;
			if(pp->len == 0){
				//extern u8 blt_state;
				if(bls_ll_getCurrentState() == BLS_LINK_STATE_CONN)
					connectStatus = 0x01;
				flyco_spp_received_master_rsp2cmd(pp->cmdID, &connectStatus, 1);
			}
		}
		break;

		case FLYCO_SPP_CMD_MODULE_RESTART:{//Do not send the instruction frequently,otherwise the ble module maybe crash!

			if(pp->len == 0){
				flyco_spp_received_master_rsp2cmd(pp->cmdID, NULL, 0);
				//restart after result sent
				spp_cmd_restart_tick = clock_time();
				spp_cmd_restart_flg = 1;
			}
		}
		break;

		case FLYCO_SPP_CMD_MODULE_DISCONNECT:{

			if(pp->len == 0){
				flyco_spp_received_master_rsp2cmd(pp->cmdID, NULL, 0);
				//disconnect after result sent
				spp_cmd_disconnect_tick = clock_time();
				spp_cmd_disconnect_flg = 1;
			}
		}
		break;

		case FLYCO_SPP_CMD_MODULE_SET_SLEEP:{

			if(pp->len == 0){
				flyco_spp_received_master_rsp2cmd(pp->cmdID, NULL, 0);
				spp_cmd_deep_sleep_tick  = clock_time();
				spp_cmd_deep_sleep_flg = 1;
			}
		}
		break;

		case FLYCO_SPP_CMD_MODULE_GET_DEVNAME1:{

			if(pp->len == 0){
				u8 devName1[20] = {DEFLUT_DEV_NAME1_LEN, DEFLUT_DEV_NAME1};
				flyco_load_para_addr(DEV_NAME1_ADDR, &devname1_index, devName1, 20);

				flyco_spp_received_master_rsp2cmd(pp->cmdID, devName1 + 1, devName1[0]);
			}
		}
		break;

		case FLYCO_SPP_CMD_MODULE_GET_DEVNAME2:{
			if(pp->len == 0){
				u8 devName2[20] = {DEFLUT_DEV_NAME2_LEN, DEFLUT_DEV_NAME2};
				flyco_load_para_addr(DEV_NAME2_ADDR, &devname2_index, devName2, 20);

				flyco_spp_received_master_rsp2cmd(pp->cmdID, devName2 + 1, devName2[0]);
			}
		}
		break;

		case FLYCO_SPP_CMD_MODULE_SET_DEVNAME1:{

			if(pp->len == 0x0C){//data length of 12 bytes, 8 bytes of additional CMD and check data
				flyco_spp_set_dev_name *p = (flyco_spp_set_dev_name *)pp;
				devNameT[0] = 12;
				//To ensure that the device name 1 length is 12 bytes, insufficient fill space
				memcpy(devNameT + 1, p->data, 12);//first 1+12 Bytes
				set_devname1_flg = 1;
				flyco_spp_received_master_rsp2cmd(pp->cmdID, NULL, 0);
			}
		}
		break;

		case FLYCO_SPP_CMD_MODULE_SET_DEVNAME2:{

			if(pp->len == 0x06){//Data length must be 6 bytes, the remaining fill space, the last 1 bytes of null
				flyco_spp_set_dev_name *p = (flyco_spp_set_dev_name *)pp;
				devNameT[13] = 6;
				//To ensure that the device name 2 length is 6 bytes, insufficient fill space
				memcpy(devNameT + 14, p->data, 6);//last 1+6 Bytes

				flyco_spp_received_master_rsp2cmd(pp->cmdID, NULL, 0);

				if(set_devname1_flg){//Merge tow device names, configure the Bluetooth device name
				    set_devname1_flg = 0;
					u8 devName[20];
					u8 devNameTmp1[20], devNameTmp2[20];

					memcpy(devNameTmp1, devNameT, 13);// * 1 1 1 1 1 1 1 1 1 1 1 1��* 1 1 1 1 space Null��
					memcpy(devNameTmp2, devNameT + 13, 7);

					nv_write(NV_FLYCO_ITEM_DEV_NAME1, devNameTmp1, 20);
					nv_write(NV_FLYCO_ITEM_DEV_NAME2, devNameTmp2, 20);

					devName[0] = 12 + 6 + 1;
					devName[1] = 0x09;
					memcpy(devName + 2, devNameT + 1, 12);
					memcpy(devName + 14, devNameT + 14, 6);
					nv_write(NV_FLYCO_ITEM_DEV_NAME, devName, 20);

					memset(ble_devName, 0, MAX_DEV_NAME_LEN); //clear device name
					memcpy(ble_devName, devName, devName[0]+1); //cop data to att table

					bls_ll_setScanRspData(devName, devName[0]+1); //change scan response device name

					//restart ble module
				}

			}
		}
		break;

		case FLYCO_SPP_CMD_MODULE_GET_BAUDRATE:{

			if(pp->len == 0){
				u8 param[3] = {0x25, 0x80, 0xef};//Default bode rate:9600
				flyco_load_para_addr(BAUD_RATE_ADDR, &baudrate_index, param, 3);
				flyco_spp_received_master_rsp2cmd(pp->cmdID, param, ((param[2]!=0xef)?3:2));
			}
		}
		break;

		case FLYCO_SPP_CMD_MODULE_SET_BAUDRATE :{

			u32 baudrate = 0;
			u8 chkparam =0;
			flyco_spp_cmd_baud_rate_t *p = (flyco_spp_cmd_baud_rate_t *)pp;
			for(u8 i = 0; i< p->len; i++)
				baudrate += ((u32)p->rate[i])<<(8*(p->len-1-i));
			if(p->len == 2)
				p->rate[2] = 0xef;

			switch (baudrate){//parameter checkout;
			case 9600:
			case 115200:
				break;
			default:
				chkparam = 1;
			}
			if(!chkparam){
				nv_write(NV_FLYCO_ITEM_BAUD_RATE, p->rate, 3); //store baud rate param

				flyco_spp_received_master_rsp2cmd(pp->cmdID, NULL, 0);
				WaitMs(30);//Delay 50ms to reply to the module to receive the characteristic code
				if(baudrate == 9600){
					CLK16M_UART9600;
				}
				else{
					CLK16M_UART115200;
				}
			}
		}
		break;

		case FLYCO_SPP_CMD_MODULE_GET_ADV_ENABLE_FLAG:{

			if(pp->len == 0){
				flyco_spp_received_master_rsp2cmd(pp->cmdID, &bls_adv_enable, 1);
		    }
		}
		break;

		case FLYCO_SPP_CMD_MODULE_SET_ADV_ENABLE_FLAG:{// cmdid = 0x0b
			flyco_spp_moduleCmd_advEnable_t *p = (flyco_spp_moduleCmd_advEnable_t *)pp;
			if(pp->len == 1){
				(p->enable) &= 0x01;//parameter checkout; Minimum byte valid 0 or 1
				bls_ll_setAdvEnable(p->enable);

				flyco_spp_received_master_rsp2cmd(pp->cmdID, NULL, 0);
			}

		}
		break;

		case FLYCO_SPP_CMD_MODULE_GET_IDENTIFIED:{

			if(pp->len == 0){
				u8 identified[6];
				flyco_load_para_addr(IDENTIFIED_ADDR, &identified_index, identified, 6);
				flyco_spp_received_master_rsp2cmd(pp->cmdID, identified, 6);
			}
		}
		break;

		case FLYCO_SPP_CMD_MODULE_SET_IDENTIFIED:{

			if(pp->len == 6){
				flyco_spp_cmd_identified_t *p = (flyco_spp_cmd_identified_t *)pp;
				nv_write(NV_FLYCO_ITEM_IDENTIFIED, p->data, 6); //store identified code param

				flyco_spp_received_master_rsp2cmd(pp->cmdID, NULL, 0);
			}
		}
		break;

		case FLYCO_SPP_CMD_MODULE_GET_ADV_DATA:{

			if(pp->len == 0){
				//The first value of the broadcast array is the total length of the broadcast data!
				u8 advData[20] = {5, 'F', 'L', 'Y', 'C', 'O'};//FLYCO default adv data��FLYCO
				flyco_load_para_addr(ADV_DATA_ADDR, &adv_data_index, advData, 20);
				flyco_spp_received_master_rsp2cmd(pp->cmdID, advData + 1, advData[0]);
			}
		}
		break;

		case FLYCO_SPP_CMD_MODULE_SET_ADV_DATA:{

			if(pp->len < 20){//Maximum 20 bytes
				flyco_spp_cmd_adv_data_t *p = (flyco_spp_cmd_adv_data_t *)pp;
				u8 len = (u8)p->len;

				bls_ll_setAdvData(p->data, len);

				u8 advData[20] = {0};
				advData[0] = len;
				memcpy(advData + 1, p->data, len);
				nv_write(NV_FLYCO_ITEM_ADV_DATA, advData, 20);

				flyco_spp_received_master_rsp2cmd(pp->cmdID, NULL, 0);
			}
		}
		break;

		case FLYCO_SPP_CMD_MODULE_GET_ADV_INTV:{

			if(pp->len == 0){
				u32 advInterval =((u32)DEFLUT_ADV_INTERVAL *5) >>3;
				u8 rp[2];
				flyco_load_para_addr(ADV_INTERVAL_ADDR, &adv_interval_index, (u8 *)&advInterval, 4);
				advInterval = (advInterval *5) >>3;//unit:0.625ms
				reverse_data((u8*)&advInterval,2,rp);
				flyco_spp_received_master_rsp2cmd(pp->cmdID, rp, 2);
			}
		}
		break;

		case FLYCO_SPP_CMD_MODULE_SET_ADV_INTV:{

			if(pp->len == 2){
				u32 para = 0;
				flyco_spp_cmd_adv_intv_t *p = (flyco_spp_cmd_adv_intv_t *)pp;

				for(u8 i = 0; i< p->len; i++)
					para += ((u32)p->intv[i])<<(8*(p->len-1-i));
				para = (para <<3) / 5;//unit:0.625ms! 160 corresponding to 100ms

				blt_set_advinterval(para);

				nv_write(NV_FLYCO_ITEM_ADV_INTERVAL, (u8 *)&para, 2);

				flyco_spp_received_master_rsp2cmd(pp->cmdID, NULL, 0);
			}
		}
		break;

		case FLYCO_SPP_CMD_MODULE_GET_RF_PWR:{

			if(pp->len == 0){
				u8 rf_power = rfpower;
				flyco_load_para_addr(RF_POWER_ADDR, &rf_power_index, (u8 *)&rf_power, 1);
				flyco_spp_received_master_rsp2cmd(pp->cmdID, &rf_power, 1);
			}
		}
		break;

		case FLYCO_SPP_CMD_MODULE_SET_RF_PWR:{

			if(pp->len == 1){
				flyco_spp_cmd_rf_pwr_t *p = (flyco_spp_cmd_rf_pwr_t *)pp;
				u8 new_rfpower = (u8) p->pwr;
				if(1 <= new_rfpower && new_rfpower <= 8){//Transmit power: 1-8 level, constraint
					rf_set_power_level_index(new_rfpower);
					nv_write(NV_FLYCO_ITEM_RF_POWER, (u8*)&new_rfpower, 1);

					flyco_spp_received_master_rsp2cmd(pp->cmdID, NULL, 0);
				}
			}
		}
		break;

		case FLYCO_SPP_CMD_MODULE_GET_VERSION_NUM:{

			if(pp->len == 0){
				flyco_spp_received_master_rsp2cmd(pp->cmdID, flyco_version, 4);
			}
		}
		break;

		case FLYCO_SPP_CMD_MODULE_GET_ADV_TIMEOUT:{

			u32 timeout = DEFLUT_ADV_TIMEOUT;
			if(pp->len == 0){
				flyco_load_para_addr(ADV_TIMEOUT_ADDR, &adv_timeout_index, (u8 *)&timeout, 4);

				timeout /=1000;
				flyco_spp_received_master_rsp2cmd(pp->cmdID, (u8*)&timeout, 1);
			}
		}
		break;

		case FLYCO_SPP_CMD_MODULE_SET_ADV_TIMEOUT:{//Adv time set, boot or wake up after the parameter effect!
			if(pp->len == 1){
				flyco_spp_cmd_adv_timeout_t *p = (flyco_spp_cmd_adv_timeout_t *)pp;
				u32 timeout = ((u32)p->tim) * 1000;//unit��ms, enlarge 1000��timeout unit:s
				nv_write(NV_FLYCO_ITEM_ADV_TIMEOUT, (u8 *)&timeout, 4);
				bls_ll_setAdvDuration(timeout, 1);
				flyco_spp_received_master_rsp2cmd(pp->cmdID, NULL, 0);
			}
		}
		break;

		case FLYCO_SPP_CMD_MODULE_GET_SERVICE_UUID:{

			u16 service_uuid = TelinkSppServiceUUID, rp;
			reverse_data((u8*)&service_uuid, 2, (u8*)&rp);
			if(pp->len == 0){
				flyco_spp_received_master_rsp2cmd(pp->cmdID, (u8*)&rp, 2);
			}
		}
		break;

		case FLYCO_SPP_CMD_MODULE_GET_SERVICE_WRITE_UUID:{

			u16 service_write_uuid = TelinkSppDataClient2ServiceUUID, rp;
			reverse_data((u8*)&service_write_uuid, 2, (u8*)&rp);
			if(pp->len == 0){
				flyco_spp_received_master_rsp2cmd(pp->cmdID, (u8*)&rp, 2);
			}
		}
		break;

		case FLYCO_SPP_CMD_MODULE_GET_SERVICE_NOTIFY_UUID:{

			u16 service_notify_uuid = TelinkSppDataServer2ClientUUID, rp;
			reverse_data((u8*)&service_notify_uuid, 2, (u8*)&rp);
			if(pp->len == 0){
				flyco_spp_received_master_rsp2cmd(pp->cmdID, (u8*)&rp, 2);
			}
		}
		break;

		default://Instruction not defined!
		break;
	}
}

//user Timer callback proc for spp cmd ack
void blt_user_timerCb_proc(void){
	//restart spp cmd
	if(spp_cmd_restart_flg && clock_time_exceed(spp_cmd_restart_tick , 10000)){  //spp cmd ack restart timeout
		spp_cmd_restart_flg =0;
		cpu_sleep_wakeup(DEEPSLEEP_MODE, PM_WAKEUP_TIMER, clock_time() + 5000 * sys_tick_per_us);
	}
    //disconnect spp cmd
	if(bls_ll_getCurrentState() == BLS_LINK_STATE_CONN && spp_cmd_disconnect_flg && clock_time_exceed(spp_cmd_disconnect_tick , 1000)){  //spp cmd ack disconnect timeout
		spp_cmd_disconnect_flg = 0;
		bls_ll_terminateConnection(HCI_ERR_REMOTE_USER_TERM_CONN);
	}
    //deep sleep spp cmd
	if(spp_cmd_deep_sleep_flg && clock_time_exceed(spp_cmd_deep_sleep_tick , 20000)){  //spp cmd ack deep sleep timeout
		spp_cmd_deep_sleep_flg = 0;
		cpu_sleep_wakeup(1, PM_WAKEUP_PAD, 0);
	}
    //set baud rate spp cmd
	if(spp_cmd_set_baudrate_flg && clock_time_exceed(spp_cmd_set_baudrate_tick , 20000)){
		spp_cmd_set_baudrate_flg = 0;
//		u8  baudratetmp[3] = {0};
//		flyco_load_para_addr(BAUD_RATE_ADDR, &baudrate_index, baudratetmp, 3);
//		u32 baudrate = 0;
//		if(baudratetmp[2] != 0xef)
//			baudrate = (baudratetmp[0]<<16) + (baudratetmp[1]<<8) + baudratetmp[2];
//		else
//			baudrate = (baudratetmp[0]<<8) + baudratetmp[1];
		if(baud_rate_tmp == 9600)
			CLK16M_UART9600;
		else if(baud_rate_tmp == 115200)
			CLK16M_UART115200;
		else//default baud rate
			CLK16M_UART9600;
	}
//	if(bls_ll_getCurrentState() == BLS_LINK_STATE_CONN && spp_cmd_get_rssi_flg && clock_time_exceed(spp_cmd_get_rssi_tick , 300000)){//bigger than 3s,slave will terminate
//		spp_cmd_get_rssi_flg = 0;
//		bls_ll_terminateConnection(HCI_ERR_REMOTE_USER_TERM_CONN);
//	}
}


