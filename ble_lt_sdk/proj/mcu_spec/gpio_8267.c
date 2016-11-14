

#include "../config/user_config.h"
#include "../mcu/config.h"

//#if (1)

#if(__TL_LIB_8267__ || (MCU_CORE_TYPE == MCU_CORE_8267) || \
	__TL_LIB_8261__ || (MCU_CORE_TYPE == MCU_CORE_8261) || \
	__TL_LIB_8269__ || (MCU_CORE_TYPE == MCU_CORE_8269)	)

#include "../common/types.h"
#include "../common/compatibility.h"
#include "../common/bit.h"
#include "../common/utility.h"
#include "../common/static_assert.h"
#include "../mcu/compiler.h"
#include "../mcu/register.h"
#include "../mcu/anareg.h"
#include "../mcu/analog.h"

#include "../mcu/gpio.h"



#define    MOUSE_GPIO_MAX    19
u32 mouse_gpio_table[MOUSE_GPIO_MAX] =           //for mouse only       u16 is enough
{
	    GPIO_PA0, // GPIO_SWS		0
		GPIO_PA1, // GPIO_PWM3		1
		GPIO_PA2, // GPIO_MSDI		2
		GPIO_PA3, // GPIO_MCLK		3
		GPIO_PB2, // GPIO_MSDO		4
		GPIO_PB3, // GPIO_MSCN		5
		GPIO_PB5, // GPIO_DM		6
		GPIO_PB6, // GPIO_DP		7
		GPIO_PC0, // GPIO_PWM0		8
		GPIO_PC1, // GPIO_GP1		9
		GPIO_PC2, // GPIO_PWM1		10
		GPIO_PC4, // GPIO_PWM2		11
		GPIO_PC5, // GPIO_GP3		12
		GPIO_PC6, // GPIO_GP4		13
		GPIO_PE0, // GPIO_GP14		14
		GPIO_PE6, // GPIO_CN		15
		GPIO_PE7, // GPIO_DI		16
		GPIO_PF0, // GPIO_DO		17
		GPIO_PF1, // GPIO_CK		18
};


/************
 *
 * gpio:         indicate the pin
 * up_down:      1 need pull up, 0 need pull down
 */
const unsigned char resistor_at[MOUSE_GPIO_MAX*2] ={
	0x0a, 4,     //PA0	SWS		0
	0x0a, 6,     //PA1	PWM3	1
	0x0b, 0,     //PA2	MSDI	2
	0x0b, 2,     //PA3	MCLK	3

	0x0d, 0,	 //PB2	MSDO	4
	0x0d, 2,	 //PB3	MSCN	5
	0x0d, 6,	 //PB5	DM		6
	0x0e, 0,	 //PB6	DP		7

	0x0e, 4,	 //PC0	PWM0	8
	0x0e, 6,	 //PC1	GP1		9
	0x0f, 0,	 //PC2	PWM1	10
	0x0f, 4,	 //PC4	PWM2	11
	0x0f, 6,	 //PC5	GP3		12
	0x10, 0,	 //PC6	GP4		13

	0x12, 4,	 //PE0	GP14	14
	0x14, 0,	 //PE6	CN		15
	0x14, 2,	 //PE7	DI		16
	0x14, 4,	 //PF0	DO		17
	0x14, 6,	 //PF1	CK		18
};

/************
 *
 * gpio:         indicate the pin
 * up_down:      0xFF    : float
 * 				 1    : 1M   pullup
 * 				 2    : 10K  pullup
 * 				 3    : 100K pulldown
 *
 *     BIT(7.6)   BIT(5.4)   BIT(3.2)   BIT(1.0)
mask_not 0x3f       0xcf	  0xf3       0xfc

 0a		 PA1         PA0
 0b		 PA5         PA4      PA3        PA2		0
 0c		 PB1         PB0      PA7        PA6
 0d		 PB5         PB4      PB3        PB2		1
 0e		 PC1         PC0      PB7        PB6
 0f		 PC5         PC4      PC3        PC2		2
 10		 PD1         PD0      PC7        PC6
 11		 PD5         PD4      PD3        PD2		3
 12		 PE1         PE0      PD7        PD6
 08		 PE3         PE2
 */
 
//if GPIO_DP,please check usb_dp_pullup_en() valid or not first.
void gpio_setup_up_down_resistor(u32 gpio, u32 up_down) {
	u8 r_val;

	if(up_down == PM_PIN_UP_DOWN_FLOAT) {
		r_val = 0;
	}
	else if(up_down == PM_PIN_PULLUP_1M) {
		r_val = PM_PIN_PULLUP_1M;
	}
	else if(up_down == PM_PIN_PULLUP_10K) {
		r_val = PM_PIN_PULLUP_10K;
	}
	else {
		r_val = PM_PIN_PULLDOWN_100K;
	}

    u8 pin = gpio & 0xff;
    u8 base_ana_reg = 0x0b + ((gpio >> 8) << 1);
    u8 mask_not = 0xfc;   //default for  PX2  PX6
    u8 shift_num = 0;

    if(GPIO_PE2 == gpio){
        base_ana_reg = 0x08;
        mask_not = 0xcf;
        shift_num = 4;
    }else if(GPIO_PE3 == gpio){
        base_ana_reg = 0x08;
        mask_not = 0x3f;
        shift_num = 6;
    }else{
        if(pin & 0x03) {
            base_ana_reg -= 1;
        }
        else if(pin & 0xc0) {
            base_ana_reg += 1;
        }
        else {
        }

        if(pin & 0x88) { //PX3  PX7
            mask_not = 0xf3;
            shift_num = 2;
        }
        else if(pin & 0x11) {   //PX0  PX4
            mask_not = 0xcf;
            shift_num = 4;
        }
        else if(pin & 0x22) {   //PX1  PX5
            mask_not = 0x3f;
            shift_num = 6;
        }
    }

    if(GPIO_DP == gpio){
        //usb_dp_pullup_en (0);
    }

	analog_write(base_ana_reg, (analog_read(base_ana_reg) & mask_not) | (r_val << shift_num));
}


void gpio_set_wakeup(u32 pin, u32 level, int en)
{
    u8 bit = pin & 0xff;
    if (en) {
        BM_SET(reg_gpio_irq_en0(pin), bit);
    }
    else {
        BM_CLR(reg_gpio_irq_en0(pin), bit);
    }
    if(level){
        BM_CLR(reg_gpio_pol(pin), bit);
    }else{
        BM_SET(reg_gpio_pol(pin), bit);
    }
}


#endif

