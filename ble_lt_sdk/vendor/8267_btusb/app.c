#include "../../proj/tl_common.h"
#include "../../proj_lib/rf_drv.h"
#include "../../proj_lib/pm.h"
#include "../../proj_lib/ble/ble_ll.h"
#include "../../proj_lib/ble/ll_whitelist.h"
#include "../../proj/drivers/keyboard.h"
#include "../common/tl_audio.h"
#include "../common/blt_led.h"
#include "../../proj_lib/ble/trace.h"
#include "../../proj/mcu/pwm.h"
#include "../../proj_lib/ble/service/ble_ll_ota.h"
#include "../../proj/drivers/audio.h"
#include "../../proj/drivers/adc.h"
#include "../../proj_lib/ble/blt_config.h"

#define			APP_BTUSB_CDC_CLASS			1

#if (HCI_ACCESS==HCI_USE_UART)
#include "../../proj/drivers/uart.h"
#endif

#define			HCI_TX_FIFO_NUM		8
MYFIFO_INIT(hci_tx_fifo, 72, HCI_TX_FIFO_NUM);

MYFIFO_INIT(blt_rxfifo, 64, 8);

//MYFIFO_INIT(blt_txfifo, 40, 16);
////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////
int		dongle_pairing_enable;
//////////////////////////////////////////////////////////////////////////////
//	Initialization: MAC address, Adv Packet, Response Packet
//////////////////////////////////////////////////////////////////////////////
u8  tbl_mac [] = {0xe1, 0xe1, 0xe2, 0xe3, 0xe4, 0xc7};

/////////////////////////// led management /////////////////////
enum{
	LED_POWER_ON = 0,
	LED_AUDIO_ON,	//1
	LED_AUDIO_OFF,	//2
	LED_SHINE_SLOW, //3
	LED_SHINE_FAST, //4
	LED_SHINE_OTA, //5
};

const led_cfg_t led_cfg[] = {
	    {1000,    0,      1,      0x00,	 },    //power-on, 1s on
	    {100,	  0 ,	  0xff,	  0x02,  },    //audio on, long on
	    {0,	      100 ,   0xff,	  0x02,  },    //audio off, long off
	    {500,	  500 ,   3,	  0x04,	 },    //1Hz for 3 seconds
	    {250,	  250 ,   6,	  0x04,  },    //2Hz for 3 seconds
	    {125,	  125 ,   200,	  0x08,  },    //4Hz for 50 seconds
};

void rf_customized_param_load(void)
{
	  //flash 0x77000 customize freq_offset adjust cap value, if not customized, default ana_81 is 0xd0
	 if( (*(unsigned char*) CUST_CAP_INFO_ADDR) != 0xff ){
		 //ana_81<4:0> is cap value(0x00 - 0x1f)
		 analog_write(0x81, (analog_read(0x81)&0xe0) | ((*(unsigned char*) CUST_CAP_INFO_ADDR)&0x1f) );
	 }

	 //flash 0x77040 customize TP0, flash 0x77041 customize TP1
	 if( ((*(unsigned char*) (CUST_TP_INFO_ADDR)) != 0xff) && ((*(unsigned char*) (CUST_TP_INFO_ADDR+1)) != 0xff) ){
		 rf_update_tp_value(*(unsigned char*) (CUST_TP_INFO_ADDR), *(unsigned char*) (CUST_TP_INFO_ADDR+1));
	 }
}

void app_led_en (int id, int en)
{
	id &= 7;
	en = !(LED_ON_LEVAL ^ en);
	if (id == 0)
	{
		gpio_write(GPIO_LED_RED, en);
	}
	else if (id == 1)
	{
		gpio_write(GPIO_LED_GREEN, en);
	}
	else if (id == 2)
	{
		gpio_write(GPIO_LED_BLUE, en);
	}
	else if (id == 3)
	{
		gpio_write(GPIO_LED_WHITE, en);
	}
}
//////////////////////////////////////////
// Service Discovery
//////////////////////////////////////////
typedef void (*main_service_t) (void);

main_service_t		main_service = 0;

u16	conn_handle;					//	handle of  connection

void app_register_service (void *p)
{
	main_service = p;
}

int app_l2cap_handler (u16 conn, u8 *raw_pkt)
{
	u8 *p = raw_pkt + 12;
	blc_hci_send_data (conn | BLM_CONN_HANDLE | HCI_FLAG_ACL_BT_STD, p, p[1]);	//

	return 0;
}

//////////////////////////////////////////////////////////
// event call back
//////////////////////////////////////////////////////////
int app_event_callback (u32 h, u8 *p, int n)
{
	static u32 event_cb_num;
	event_cb_num++;
	int send_to_hci = 1;

	if (h == (HCI_FLAG_EVENT_BT_STD | HCI_EVT_LE_META))		//LE event
	{
		u8 subcode = p[0];

	//------------ ADV packet --------------------------------------------
		if (subcode == HCI_SUB_EVT_LE_ADVERTISING_REPORT)	// ADV packet
		{
			u8 n = (hci_tx_fifo.wptr - hci_tx_fifo.rptr) & 63;
			if (n >= HCI_TX_FIFO_NUM - 3)
			{
				send_to_hci = 0;
			}
		}

	//------------ connection complete -------------------------------------
		else if (subcode == HCI_SUB_EVT_LE_CONNECTION_COMPLETE)	// connection complete
		{
			event_connection_complete_t *pc = (event_connection_complete_t *)p;
			if (!pc->status)							// status OK
			{
				conn_handle = pc->handle;				// connection handle
				app_led_en (conn_handle, 1);
			}
		}

	//------------ connection update complete -------------------------------
		else if (subcode == HCI_SUB_EVT_LE_CONNECTION_UPDATE_COMPLETE)	// connection update
		{

		}
	}

	//------------ disconnect -------------------------------------
	else if (h == (HCI_FLAG_EVENT_BT_STD | HCI_CMD_DISCONNECTION_COMPLETE))		//disconnect
	{

		event_disconnection_t	*pd = (event_disconnection_t *)p;
		app_led_en (pd->handle, 0);
		//terminate reason
		if(pd->reason == HCI_ERR_CONN_TIMEOUT){

		}
		else if(pd->reason == HCI_ERR_REMOTE_USER_TERM_CONN){  //0x13

		}
		else if(pd->reason == SLAVE_TERMINATE_CONN_ACKED || pd->reason == SLAVE_TERMINATE_CONN_TIMEOUT){

		}
	}

	if (send_to_hci)
	{
		blc_hci_send_data (h, p, n);
	}

	return 0;
}

/////////////////////////////////////////////////////////////////////
// main loop flow
/////////////////////////////////////////////////////////////////////
int main_idle_loop ()
{
	static u32 tick_loop;
	tick_loop ++;

	// bcopy
	////////////////////////////////////// BLE entry /////////////////////////////////
	blm_main_loop ();

	extern int blc_hci_proc ();
	blc_hci_proc ();
	////////////////////////////////////// UI entry /////////////////////////////////
	device_led_process();

	dongle_pairing_enable = !gpio_read (SW1_GPIO);
	static u32 gpio2 = 0;
	u8 gpio =	!gpio_read (SW2_GPIO);
	if (gpio & !gpio2)
	{
		app_led_en (3, 1);
		ll_whiteList_reset ();
		u8 mac[6] = {0xc3, 0xe1, 0xe2, 0xe3, 0xe4, 0xc7};
		blm_ll_createConnection (10, 10, 0, 0, mac, 0,	// scan_min scan_wnd policy adr_type mac own_adr_type,
								  20, 40, 0, 100,  0, 0);	// connection: min max latency timeout ce_min ce_max
	}
	gpio2 = gpio;


	////////////////////////////////////// USB /////////////////////////////////
	btusb_handle_irq ();
	return 0;
}

void main_loop ()
{
	main_idle_loop ();

	if (main_service)
	{
		main_service ();
		main_service = 0;
	}
}

///////////////////////////////////////////
void user_init()
{
	rf_customized_param_load();  //load customized freq_offset cap value and tp value

	usb_log_init ();
	REG_ADDR8(0x74) = 0x53;
	REG_ADDR16(0x7e) = 0x08d0;
	REG_ADDR8(0x74) = 0x00;
	btusb_init ();

	btusb_select_cdc_device (APP_BTUSB_CDC_CLASS);

	usb_dp_pullup_en (1);  //open USB enum

	/////////////////// keyboard drive/scan line configuration /////////

	//blc_l2cap_register_handler (blc_l2cap_packet_receive);
	blc_enable_hci_master_handler ();
	blm_register_event_data_callback (app_event_callback, app_l2cap_handler);
	att_register_idle_func (main_idle_loop);

#if (HCI_ACCESS==HCI_USE_USB)
	usb_bulk_drv_init (0);

#if APP_BTUSB_CDC_CLASS
	blc_register_hci_handler (blc_hci_rx_from_usb, blc_hci_tx_to_usb);
#else
	blc_register_hci_handler (blc_acl_from_btusb, blc_hci_tx_to_btusb);
#endif

#else	//uart
	//one gpio should be configured to act as the wakeup pin if in power saving mode; pending
	//todo:uart init here
	uart_io_init(UART_GPIO_8267_PB2_PB3);
	CLK32M_UART115200;
	uart_BuffInit((u8 *)(&T_rxdata_buf), sizeof(T_rxdata_buf), (u8 *)(&T_txdata_buf));
	blc_register_hci_handler (blc_rx_from_uart, blc_hci_tx_to_uart);
#endif

	////////////////// BLE slave initialization ////////////////////////////////////
	u32 *pmac = (u32 *) CFG_ADR_MAC;
	if (*pmac != 0xffffffff)
	{
	    memcpy (tbl_mac, pmac, 6);
	}
    else
    {
        //TODO : should write mac to flash after pair OK
        tbl_mac[0] = (u8)rand();
        flash_write_page (CFG_ADR_MAC, 6, tbl_mac);
    }

	rf_set_power_level_index (RF_POWER_8dBm);

	ble_master_init (tbl_mac);
	blm_ll_setScanEnable (0, 0);
	//ble_set_debug_adv_channel (37);

	/////////////////////////////////////////////////////////////////
	//ll_whiteList_reset();

	//device_led_init(GPIO_LED, 1);
}
