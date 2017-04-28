#include "../tl_common.h"
#include "../mcu/clock.h"
#include "i2c.h"

#if(MCU_CORE_TYPE != MCU_CORE_8263)

/*****
 * brief: the function initial the relevant gpio as i2c.
 *        when enable one group of pins as i2c, the other two groups of pin should be disable the i2c function.
 *        in addition, when disable pin as i2c, we can enable the pin as gpio.
 * param[in] gpio_sda -- the pin as data line of i2c.
 * param[in] gpio_scl -- the pin as clock line of i2c.
 * return none
 */
void i2c_pin_initial(u32 gpio_sda, u32 gpio_scl)
{
	gpio_set_func(gpio_sda,AS_I2C);  //disable gpio function
	gpio_set_func(gpio_scl,AS_I2C);  //disable gpio function
#if (MCU_CORE_TYPE == MCU_CORE_8266)
	//reg_gpio_pe_gpio &= ~ BIT(7);  //E7 and F1 are the pin of i2c module in 8266 while it is not in 8267.
	//reg_gpio_pf_gpio &= ~ BIT(1);
	analog_write(20, analog_read(20) | (GPIO_PULL_UP_10K<<2) | (GPIO_PULL_UP_10K<<6));	//  CK, DI, pullup 10K
#elif((MCU_CORE_TYPE == MCU_CORE_8261)||(MCU_CORE_TYPE == MCU_CORE_8267)||(MCU_CORE_TYPE == MCU_CORE_8269))
	unsigned char gpio_num = (unsigned char)(gpio_sda>>8);
	switch(gpio_num){
	/**** A3 and A4 as i2c function. default gpio function****/
	case 0x00:
		reg_gpio_config_func0 |= (BIT_RNG(3,4));      //enable A3/A4 as i2c function
		if(reg_gpio_config_func1 & (BIT_RNG(6,7))){
			reg_gpio_config_func1 &= (~BIT_RNG(6,7)); //disable B6/B7 as i2c function
			gpio_set_func(GPIO_PB6|GPIO_PB7,AS_GPIO); //enable B6/B7 as gpio function
		}
		if(reg_gpio_config_func2 & (BIT_RNG(0,1))){
			reg_gpio_config_func2 &= (~BIT_RNG(0,1));  //disable C0/C1 as i2c function
			gpio_set_func(GPIO_PC0|GPIO_PC1, AS_GPIO); //enable C0/C1 as gpio function
		}
		analog_write (0x0b,analog_read(0x0b)|0x28);   //10k pull_up resistor
		break;
	/**** B6 and B7 as i2c function. default i2c function ****/
	case 0x01:
		reg_gpio_config_func1 |= (BIT_RNG(6,7));      //enable B6/B7 i2c function
		if(reg_gpio_config_func0 & (BIT_RNG(3,4))){
			reg_gpio_config_func0 &= (~BIT_RNG(3,4)); // disable A3/A4 as i2c function
			gpio_set_func(GPIO_PA3|GPIO_PA4,AS_GPIO); // enable A3/A4 as gpio function
		}
		if(reg_gpio_config_func2 & (BIT_RNG(0,1))){
			reg_gpio_config_func2 &= (~BIT_RNG(0,1)); //disable C0/C1 as i2c function
			gpio_set_func(GPIO_PC0|GPIO_PC1,AS_GPIO); //enable C0/C1 as gpio function
		}
		analog_write (0x0e,analog_read(0x0e)|0x0c);   //10k pull_up resistor
		break;
	/**** C0 and C1 as i2c function. default gpio function ****/
	case 0x02:
		reg_gpio_config_func2 |= (BIT_RNG(0,1));      //enable C0/C1 as i2c function
		if(reg_gpio_config_func0 & (BIT_RNG(3,4))){
			reg_gpio_config_func0 &= (~BIT_RNG(3,4)); //disable A3/A4 as i2c function
			gpio_set_func(GPIO_PA3|GPIO_PA4,AS_GPIO); // enable A3/A4 as gpio function
		}
		if(reg_gpio_config_func0 & (BIT_RNG(3,4))){
			reg_gpio_config_func0 &= (~BIT_RNG(3,4)); // disable A3/A4 as i2c function
			gpio_set_func(GPIO_PA3|GPIO_PA4,AS_GPIO); // enable A3/A4 as gpio function
		}
		analog_write (0x0e,analog_read(0x0e)|0xc0);   //10k pull_up resistor
		break;
	default:
		break;
	}
#endif
	gpio_set_input_en(gpio_sda,1); //enable input
	gpio_set_input_en(gpio_scl,1); //enable input
}
/**
 * @brief      This function set the id of slave device and the speed of I2C interface
 *             note: the param ID contain the bit of writting or reading.
 *             eg:the parameter 0x5C. the reading will be 0x5D and writting 0x5C.
 * @param[in]  slave_id - the id of slave device.it contains write or read bit,the lsb is write or read bit.
 *                       ID|0x01 indicate read. ID&0xfe indicate write.
 * @param[in]  div_clock - the division factor of I2C clock, 
 *             I2C clock = System clock / (4*DivClock);if the datasheet you look at is 2*,pls modify it.
 * @return     none
 */
void i2c_master_init0(unsigned char slave_id, unsigned char div_clock)
{
	reg_i2c_set = (div_clock|(slave_id<<8)|(FLD_I2C_MODE_MASTER << 24));
	reg_rst_clk0 |= FLD_CLK_I2C_EN; //enable i2c clock
	reg_spi_sp &= (~BIT(7));		//force PADs act as I2C; i2c and spi share the hardware of IC
}

/**
 * @brief      This function set the id of slave device and the speed of I2C interface
 *             note: the param ID contain the bit of writting or reading.
 *             eg:the parameter 0x5C. the reading will be 0x5D and writting 0x5C.
 * @param[in]  slave_id - the id of slave device.it contains write or read bit,the lsb is write or read bit.
 *                       ID|0x01 indicate read. ID&0xfe indicate write.
 * @param[in]  i2c_speed is in Khz. for example: i2c_speed is 200, indicate 200k          
 * @return     none
 */
void i2c_master_init1(unsigned char slave_id, unsigned int i2c_speed)
{
	//i2c_clock = system_clock/ 4*reg_0x00;
	reg_i2c_set = (CLOCK_SYS_CLOCK_1MS/(4*i2c_speed))|(slave_id<<8)|(FLD_I2C_MODE_MASTER << 24);
	reg_rst_clk0 |= FLD_CLK_I2C_EN; //enable i2c clock
	reg_spi_sp &= (~BIT(7));		//force PADs act as I2C; i2c and spi share the hardware of IC
}
/**
 *  @brief      the function config the ID of slave and mode of slave.
 *  @param[in]  device_id - it contains write or read bit,the lsb is write or read bit.
 *              ID|0x01 indicate read. ID&0xfe indicate write.
 *  @param[in]  i2c_mode - set slave mode. slave has two modes, one is DMA mode, the other is MAPPING mode.
 *  @param[in]  pbuf - if slave mode is MAPPING, set the first address of buffer master write or read slave.
 *              notice: the buffer must align 128 bytes. the write address is pbuf while the read address is pbuf+64.
 *  @return     none
 */
void i2c_slave_init(unsigned char device_id,enum I2C_SLAVE_MODE i2c_mode,unsigned char* pbuf)
{
	reg_i2c_id = device_id;
	if(i2c_mode == I2C_SLAVE_MAP){
		reg_i2c_mode = (FLD_I2C_ADDR_AUTO | FLD_I2C_MEM_MAP);
		reg_i2c_mem_map = (unsigned short*)pbuf;
	}
	reg_i2c_mode &= (~FLD_I2C_MODE_MASTER); //enable slave mode
	reg_spi_sp &= (~BIT(7));		//force PADs act as I2C; i2c and spi share the hardware of IC
}

/**
 * @brief      This function writes one byte to the slave device at the specified address
 * @param[in]  addr - pointer to the address where the one byte data will be written
 * @param[in]  addr_len - length in byte of the address, which makes this function is  
 *             compatible for slave device with both one-byte address and two-byte address
 * @param[in]  data - the one byte data will be written via I2C interface
 * @return     none
 * @ timing chart : start + ID(w) + addr(1or2) + data + stop
 */
void i2c_write_byte(unsigned char* addr, int addr_len, unsigned char data)
{
	reg_i2c_id &= (~BIT(0)); //ID|rw_bit; if rw_bit=1,read data; if rw_bit=0,write data
	if(addr_len == 1){
		reg_i2c_adr = addr[0];
		reg_i2c_ctrl = (FLD_I2C_CMD_START|FLD_I2C_CMD_ID|FLD_I2C_CMD_ADR);
	}
	else if(addr_len == 2){
		reg_i2c_adr = addr[0];
		reg_i2c_do  = addr[1];
		reg_i2c_ctrl = (FLD_I2C_CMD_START|FLD_I2C_CMD_ID|FLD_I2C_CMD_ADR|FLD_I2C_CMD_DO);
	}
	else{
		while(1);
	}
	while(reg_i2c_status & FLD_I2C_CMD_BUSY);
	reg_i2c_di = data;
	reg_i2c_ctrl = FLD_I2C_CMD_DI;
	while(reg_i2c_status & FLD_I2C_CMD_BUSY);
	
	reg_i2c_ctrl = FLD_I2C_CMD_STOP;
	while(reg_i2c_status & FLD_I2C_CMD_BUSY);
}
/**
 * @brief      This function reads one byte from the slave device at the specified address
 * @param[in]  Addr - pointer to the address where the one byte data will be read
 * @param[in]  AddrLen - length in byte of the address, which makes this function is  
 *             compatible for slave device with both one-byte address and two-byte address
 * @return     the one byte data read from the slave device via I2C interface
 * @timing chart: start+ID(w)+addr(1or2)+ restart + ID(r) + data + stop
 */
unsigned char i2c_read_byte(unsigned char* addr, unsigned char addr_len)
{
	unsigned char read_data = 0;
	//start+ID(w)+addr(1or2)
	reg_i2c_id &= (~BIT(0)); //ID|rw_bit; if rw_bit=1,read data; if rw_bit=0,write data
	if(addr_len == 1){
		reg_i2c_adr = addr[0];
		reg_i2c_ctrl = (FLD_I2C_CMD_START|FLD_I2C_CMD_ID|FLD_I2C_CMD_ADR);
	}
	else if(addr_len == 2){
		reg_i2c_adr = addr[0];
		reg_i2c_do  = addr[1];
		reg_i2c_ctrl = (FLD_I2C_CMD_START|FLD_I2C_CMD_ID|FLD_I2C_CMD_ADR|FLD_I2C_CMD_DO);
	}
	else{
		while(1);
	}
	while(reg_i2c_status & FLD_I2C_CMD_BUSY);
	
	//restart+ID(r)
	reg_i2c_id |= BIT(0);  //ID|rw_bit; if rw_bit=1,read data; if rw_bit=0,write data
	
	reg_rst0 |= FLD_RST_I2C;    //reset i2c module and occur "restart"
	reg_rst0 &= (~FLD_RST_I2C);
	reg_i2c_ctrl = (FLD_I2C_CMD_START|FLD_I2C_CMD_ID);
	while(reg_i2c_status & FLD_I2C_CMD_BUSY);
	//
	reg_i2c_ctrl = (FLD_I2C_CMD_DI|FLD_I2C_CMD_READ_ID|FLD_I2C_CMD_NAK); //enable read and send 8 clock
	while(reg_i2c_status & FLD_I2C_CMD_BUSY);
	read_data = reg_i2c_di;
	//stop
	reg_i2c_ctrl = FLD_I2C_CMD_STOP;
	while(reg_i2c_status & FLD_I2C_CMD_BUSY);
	return read_data;
}
/**
 * @brief      This function writes a bulk of data to the slave device at the specified address
 * @param[in]  addr - pointer to the address where the data will be written
 * @param[in]  addr_len - length in byte of the address, which makes this function is  
 *             compatible for slave device with both one-byte address and two-byte address
 * @param[in]  pbuf - pointer to the data will be written via I2C interface
 * @param[in]  len - length in byte of the data will be written via I2C interface
 * @return     none
 */
void i2c_burst_write(unsigned char *addr, int addr_len, unsigned char * pbuf, int len)
{
    int i = 0;
    unsigned short tmp_addr = 0;

    memcpy(&tmp_addr, addr, addr_len);

    for (i = 0; i < len; i++) {
        i2c_write_byte(&tmp_addr, addr_len, pbuf[i]);
        tmp_addr++;
    }
}
/**
 * @brief      This function reads a bulk of data from the slave device at the specified address
 * @param[in]  addr - pointer to the address where the data will be read
 * @param[in]  addr_len - length in byte of the address, which makes this function is  
 *             compatible for slave device with both one-byte address and two-byte address
 * @param[out] pbuf - pointer to the buffer will cache the data read via I2C interface
 * @param[in]  len - length in byte of the data will be read via I2C interface
 * @return     none
 */
void i2c_burst_read(unsigned char* addr, int addr_len, unsigned char * pbuf, int len)
{
    int i = 0;
    unsigned short tmp_addr = 0;

    memcpy(&tmp_addr, addr, addr_len);

    for (i = 0; i < len; i++) {
        pbuf[i] = i2c_read_byte(&tmp_addr, addr_len);
        tmp_addr++;
    }
}

/**
 *  @brief      the write format in dma mode telink design. pls refer to datasheet.
 *  @param[in]  addr - the register that master write data to slave in. support one byte and two bytes. i.e param2 AddrLen may be 1 or 2.
 *  @param[in]  addr_len - the length of register. enum 1 or 2. based on the spec of i2c slave.
 *  @param[in]  pbuf - the first SRAM buffer address to write data to slave in.
 *  @param[in]  len - the length of data master write to slave.
 *  @return     none
 *  @ timing chart: start + ID(w) + addr(1or2) + pbuf[0] +...+ pbuf[len] + stop
 */
void i2c_write_dma(unsigned short addr, unsigned char addr_len, unsigned char* pbuf, int len)
{
	int idx = 0;	
	//start + ID(w) + addr(1or2)
	reg_i2c_id &= (~BIT(0)); //ID|rw_bit; if rw_bit=1,read data; if rw_bit=0,write data
	if(addr_len == 1){
		reg_i2c_adr = (unsigned char)addr;
		reg_i2c_ctrl = (FLD_I2C_CMD_START|FLD_I2C_CMD_ID|FLD_I2C_CMD_ADR);
	}
	else if(addr_len == 2){
		reg_i2c_adr = (unsigned char)(addr>>8);
		reg_i2c_do  = (unsigned char)addr;
		reg_i2c_ctrl = (FLD_I2C_CMD_START|FLD_I2C_CMD_ID|FLD_I2C_CMD_ADR|FLD_I2C_CMD_DO);
	}
	else{
		while(1);
	}
	while(reg_i2c_status & FLD_I2C_CMD_BUSY);
	//write data to slave
	for(idx=0;idx<len;idx++){
		reg_i2c_di = pbuf[idx];
		reg_i2c_ctrl = FLD_I2C_CMD_DI;
		while(reg_i2c_status & FLD_I2C_CMD_BUSY);
	}
	//stop
	reg_i2c_ctrl = FLD_I2C_CMD_STOP;
	while(reg_i2c_status & FLD_I2C_CMD_BUSY);
 }
/**
 * @brief      the read format in dma mode telink design. pls refer to datasheet.
 * @param[in]  addr - the register master read data from slave in. support one byte and two bytes.
 * @param[in]  addr_len - the length of register. two data: 1 or 2.
 * @param[in]  pbuf - the first address of SRAM buffer master store data in.
 * @param[in]  len - the length of data master read from slave.
 * @return     none.
 * @timing chart: start + ID(w) + addr(1or2) + stop + start + ID(r) + pbuf[0] + ...+ pbuf[len] + stop
 */
void i2c_read_dma(unsigned short addr, unsigned char addr_len, unsigned char* pbuf, int len)
{
	int idx = 0;
	//start + ID(w) + addr(1or2) + stop
	reg_i2c_id &= (~BIT(0)); //ID|rw_bit; if rw_bit=1,read data; if rw_bit=0,write data
	if(addr_len == 1){
		reg_i2c_adr = (unsigned char)addr;
		reg_i2c_ctrl = (FLD_I2C_CMD_START|FLD_I2C_CMD_ID|FLD_I2C_CMD_ADR|FLD_I2C_CMD_STOP);
	}
	else if(addr_len == 2){
		reg_i2c_adr = (unsigned char)(addr>>8);
		reg_i2c_do  = (unsigned char)addr;
		reg_i2c_ctrl = (FLD_I2C_CMD_START|FLD_I2C_CMD_ID|FLD_I2C_CMD_ADR|FLD_I2C_CMD_DO|FLD_I2C_CMD_STOP);
	}
	else{
		while(1);
	}
	while(reg_i2c_status & FLD_I2C_CMD_BUSY);
	// start + ID(r)
	reg_i2c_id |= BIT(0);  //ID|rw_bit; if rw_bit=1,read data; if rw_bit=0,write data
	reg_i2c_ctrl = (FLD_I2C_CMD_START|FLD_I2C_CMD_ID);
	while(reg_i2c_status & FLD_I2C_CMD_BUSY);
	
	len--;
	while(len){
		reg_i2c_ctrl = (FLD_I2C_CMD_DI|FLD_I2C_CMD_READ_ID);
		while(reg_i2c_status & FLD_I2C_CMD_BUSY);
		pbuf[idx] = reg_i2c_di;
		idx++;
		len--;
	}
	reg_i2c_ctrl = (FLD_I2C_CMD_DI|FLD_I2C_CMD_READ_ID|FLD_I2C_CMD_NAK);
	while(reg_i2c_status & FLD_I2C_CMD_BUSY);
	pbuf[idx] = reg_i2c_di;
	//stop
	reg_i2c_ctrl = FLD_I2C_CMD_STOP;
	while(reg_i2c_status & FLD_I2C_CMD_BUSY);
}
/**
 *   @brief      the write format in mapping mode telink design. pls refer to datasheet.
 *   @param[in]  pbuf - the first address of data master write to slave.
 *   @param[in]  len - the length of data to write.
 *   @return     none
 *   @timing chart: start + ID(w) + pbuf[0] + pbuf[...] + pbuf[len] + stop
 */
void i2c_write_mapping(unsigned char* pbuf, int len)
{
	int idx = 0;
	reg_i2c_id &= (~BIT(0)); //ID|rw_bit; if rw_bit=1,read data; if rw_bit=0,write data
	reg_i2c_ctrl = (FLD_I2C_CMD_START|FLD_I2C_CMD_ID);
	while(reg_i2c_status & FLD_I2C_CMD_BUSY);
	
	//start to write data to slave
	for(idx = 0;idx < len; idx++){
		reg_i2c_di = pbuf[idx];
		reg_i2c_ctrl = FLD_I2C_CMD_DI;
		while(reg_i2c_status & FLD_I2C_CMD_BUSY);
	}
	//stop
	reg_i2c_ctrl = FLD_I2C_CMD_STOP;
	while(reg_i2c_status & FLD_I2C_CMD_BUSY);
}

/**
 *  @brief      read data from slave that is mapping mode.
 *  @param[in]  pbuf - the first address of SRAM buffer to store data.
 *  @param[in]  len - the length of data read from slave(byte).
 *  @return     none
 *  @timing chart: start + ID(r) + pbuf[0] + pbuf[...] + pbuf[len] + stop
 */
 void i2c_read_mapping(unsigned char* pbuf, int len)
 {
	 int idx = 0;
	 reg_i2c_id |= BIT(0);  //ID|rw_bit; if rw_bit=1,read data; if rw_bit=0,write data
	 reg_i2c_ctrl = (FLD_I2C_CMD_START|FLD_I2C_CMD_ID);
	 while(reg_i2c_status & FLD_I2C_CMD_BUSY);
	 
	 len--;
	 while(len){
		 reg_i2c_ctrl = (FLD_I2C_CMD_DI|FLD_I2C_CMD_READ_ID);
		 while(reg_i2c_status & FLD_I2C_CMD_BUSY);
		 pbuf[idx++] = reg_i2c_di;
		 len--;
	 }
	 reg_i2c_ctrl = (FLD_I2C_CMD_DI|FLD_I2C_CMD_READ_ID|FLD_I2C_CMD_NAK);
	 while(reg_i2c_status & FLD_I2C_CMD_BUSY);
	 pbuf[idx] = reg_i2c_di;
	 
	 //stop
	 reg_i2c_ctrl = FLD_I2C_CMD_STOP;
	 while(reg_i2c_status & FLD_I2C_CMD_BUSY);
 }
 
/*******************************************************************
 ***************** the following is simulate ***********************
 *******************************************************************/

#ifndef PIN_I2C_SCL
#define PIN_I2C_SCL				0
#endif
#ifndef PIN_I2C_SDA
#define PIN_I2C_SDA				0
#endif

static inline void i2c_sim_wait(void){
}
void i2c_sim_long_wait(void){
	CLOCK_DLY_600NS;
}
/*** Pulling the line to ground is considered a logical zero
 *   while letting the line float is a logical one.
 *   http://en.wikipedia.org/wiki/I%C2%B2C
 */
static inline void i2c_sim_scl_out(int v){
	gpio_set_output_en(PIN_I2C_SCL,(!v));
}

static inline int i2c_sim_scl_in(void){
	return gpio_read(PIN_I2C_SCL);
}

/***Pulling the line to ground is considered a logical zero
 *  while letting the line float is a logical one.
 *  http://en.wikipedia.org/wiki/I%C2%B2C
 */
static inline void i2c_sim_sda_out(int v){
	gpio_set_output_en(PIN_I2C_SDA,(!v));
}

static inline int i2c_sim_sda_in(void){
	return gpio_read(PIN_I2C_SDA);
}

static inline void i2c_sim_scl_init(void){
	gpio_set_func(PIN_I2C_SCL, AS_GPIO);
}

static inline void i2c_sim_sda_init(void){
	gpio_set_func(PIN_I2C_SDA, AS_GPIO);
	gpio_set_input_en(PIN_I2C_SDA, 1);
}

static inline void i2c_sim_scl_idle(void){
	gpio_set_output_en(PIN_I2C_SCL, 0);
	gpio_write(PIN_I2C_SCL, 0);
}

static inline void i2c_sim_sda_idle(void){
	gpio_set_output_en(PIN_I2C_SDA, 0);
	gpio_write(PIN_I2C_SDA, 0);
}


void i2c_sim_init(){}

/***
 *  Sets clock high, then data high.  This will do a stop if data was low.
 *  Then sets data low, which should be a start condition.
 *  After executing, data is left low, while clock is left high
*/
void i2c_sim_start(void)
{
	i2c_sim_scl_init();
	i2c_sim_sda_init();
	i2c_sim_sda_idle();
	i2c_sim_scl_idle();
	i2c_sim_sda_out(0);		//sda: 0
	i2c_sim_wait();

}

/***
 * puts data low, then clock low,
 * then clock high, then data high.
 * This should cause a stop, which
 * should idle the bus, I.E. both clk and data are high.
*/
void i2c_sim_stop(void)
{
	i2c_sim_sda_out(0);
	i2c_sim_wait();
	i2c_sim_scl_out(0);
	i2c_sim_wait();
	i2c_sim_scl_out(1);
	i2c_sim_wait();
	i2c_sim_sda_out(1);
}

static void i2c_sim_wirte_bit(int bit)
{
	i2c_sim_scl_out(0);
	i2c_sim_sda_out(bit);
	i2c_sim_long_wait();
	i2c_sim_scl_out(1);
}

/***
 *   Read a bit from I2C bus
 */
static int i2c_sim_read_bit(void){
	i2c_sim_wirte_bit(1);
	return i2c_sim_sda_in();
}

int i2c_sim_write_byte(u8 dat){
	int i = 0x80;
	while(i){
		i2c_sim_wirte_bit((dat & i));
		i = i >> 1;
	}
	return i2c_sim_read_bit();
}

u8 i2c_sim_read_byte(int last){
	u8 dat = 0;
	foreach(i, 8){
		i2c_sim_wirte_bit(1);
		if(i2c_sim_sda_in()){
			dat =(dat << 1) | 0x01;
		}else{
			dat = dat << 1;
		}
	}
	i2c_sim_wirte_bit(last);
	return dat;
}

void i2c_sim_write(u8 id, u8 addr, u8 dat)
{
	i2c_sim_start();
	i2c_sim_write_byte(id);
	i2c_sim_write_byte(addr);
	i2c_sim_write_byte(dat);
	i2c_sim_stop();
}

u8 i2c_sim_read(u8 id, u8 addr)
{
	u8 dat;
	i2c_sim_burst_read(id, addr, &dat, 1);
	return dat;
}

void i2c_sim_burst_read(u8 id, u8 addr,u8 *p, int n)
{
	i2c_sim_start();

	i2c_sim_write_byte(id);
	i2c_sim_write_byte(addr);
	//i2c_sim_sda_out(1);
	//i2c_sim_scl_out(0);
	//i2c_sim_long_wait();
	//i2c_sim_scl_out(1);
	//i2c_sim_sda_out(0);
	i2c_sim_stop();
	i2c_sim_start();

	i2c_sim_write_byte(id | 1);

	for(int k = 0; k < n; ++k){
		*p++ = i2c_sim_read_byte( k ==(n-1) );
	}
	i2c_sim_stop();

}

void i2c_sim_burst_write(u8 id, u8 addr,u8 *p, int n)
{
	i2c_sim_start();
	i2c_sim_write_byte(id);
	i2c_sim_write_byte(addr);
	foreach(i, n){
		i2c_sim_write_byte(*p++);
	}
	i2c_sim_stop();
}
void i2c_sim_reset_e2prom(void ){
	i2c_sim_sda_out(1);
	for(u8 i=0;i<8;i++){
		i2c_sim_scl_out(0);
		i2c_sim_long_wait();
		i2c_sim_scl_out(1);
		i2c_sim_long_wait();
	}
	i2c_sim_sda_out(0);
}
#endif



