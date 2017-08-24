#include "../../proj/tl_common.h"
#include "../../proj/mcu/watchdog_i.h"
#include "../../vendor/common/user_config.h"
#include "../../proj_lib/rf_drv.h"
#include "../../proj_lib/pm.h"
#include "../../proj_lib/ble/blt_config.h"
#include "../../proj_lib/ble/ll/ll.h"
#include "../../proj/drivers/uart.h"

#if (__PROJECT_8261_DRIVER_TEST__ || __PROJECT_8266_DRIVER_TEST__ || __PROJECT_8267_DRIVER_TEST__ || __PROJECT_8269_DRIVER_TEST__)

extern my_fifo_t hci_rx_fifo;
extern void user_init();
extern void main_loop (void);



int timer0_irq_cnt = 0;
int timer1_irq_cnt = 0;
int timer2_irq_cnt = 0;
_attribute_ram_code_ void irq_handler(void)
{



#if (DRIVER_TEST_MODE == TEST_HW_TIMER)
	if(reg_tmr_sta & FLD_TMR_STA_TMR0){
		reg_tmr_sta = FLD_TMR_STA_TMR0; //clear irq status
		timer0_irq_cnt ++;
		DBG_CHN0_TOGGLE;
	}

	if(reg_tmr_sta & FLD_TMR_STA_TMR1){
		reg_tmr_sta = FLD_TMR_STA_TMR1; //clear irq status
		timer1_irq_cnt ++;
		DBG_CHN1_TOGGLE;
	}

	if(reg_tmr_sta & FLD_TMR_STA_TMR2){
		reg_tmr_sta = FLD_TMR_STA_TMR2; //clear irq status
		timer2_irq_cnt ++;
		DBG_CHN2_TOGGLE;
	}
#endif


#if (0)
	unsigned char irqS = reg_dma_rx_rdy0;
    if(irqS & FLD_DMA_UART_RX)	//rx
    {
    	reg_dma_rx_rdy0 = FLD_DMA_UART_RX;
    	u8* w = hci_rx_fifo.p + (hci_rx_fifo.wptr & (hci_rx_fifo.num-1)) * hci_rx_fifo.size;
    	if(w[0]!=0)
    	{
    		my_fifo_next(&hci_rx_fifo);
    		u8* p = hci_rx_fifo.p + (hci_rx_fifo.wptr & (hci_rx_fifo.num-1)) * hci_rx_fifo.size;
    		reg_dma0_addr = (u16)((u32)p);
    	}
    }

    if(irqS & FLD_DMA_UART_TX)	//tx
    {
    	reg_dma_rx_rdy0 = FLD_DMA_UART_TX;
		#if (MCU_CORE_TYPE == MCU_CORE_8266)
				uart_clr_tx_busy_flag();
		#endif
    }
#endif
}

int main (void) {
	blc_pm_select_internal_32k_crystal();

	cpu_wakeup_init(CRYSTAL_TYPE);

	set_tick_per_us (CLOCK_SYS_CLOCK_HZ/1000000);
	clock_init();

	gpio_init();

	rf_drv_init(CRYSTAL_TYPE);

	user_init ();

	while (1) {
#if (MODULE_WATCHDOG_ENABLE)
		wd_clear(); //clear watch dog
#endif
		main_loop ();
	}
}




#endif   //end of __PROJECT_826x_DRIVER_TEST__
