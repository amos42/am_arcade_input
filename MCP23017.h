

/*
 * MCP23017 Defines
 */
#define MPC23017_GPIOA_MODE		0x00
#define MPC23017_GPIOB_MODE		0x01
#define MPC23017_GPIOA_PULLUPS_MODE	0x0c
#define MPC23017_GPIOB_PULLUPS_MODE	0x0d
#define MPC23017_GPIOA_READ             0x12
#define MPC23017_GPIOB_READ             0x13

/*
 * Defines for I2C peripheral (aka BSC, or Broadcom Serial Controller)
 */

#define BSC1_C		*(bsc1 + 0x00)
#define BSC1_S		*(bsc1 + 0x01)
#define BSC1_DLEN	*(bsc1 + 0x02)
#define BSC1_A		*(bsc1 + 0x03)
#define BSC1_FIFO	*(bsc1 + 0x04)

#define BSC_C_I2CEN	(1 << 15)
#define BSC_C_INTR	(1 << 10)
#define BSC_C_INTT	(1 << 9)
#define BSC_C_INTD	(1 << 8)
#define BSC_C_ST	(1 << 7)
#define BSC_C_CLEAR	(1 << 4)
#define BSC_C_READ	1

#define START_READ	BSC_C_I2CEN|BSC_C_ST|BSC_C_CLEAR|BSC_C_READ
#define START_WRITE	BSC_C_I2CEN|BSC_C_ST

#define BSC_S_CLKT	(1 << 9)
#define BSC_S_ERR	(1 << 8)
#define BSC_S_RXF	(1 << 7)
#define BSC_S_TXE	(1 << 6)
#define BSC_S_RXD	(1 << 5)
#define BSC_S_TXD	(1 << 4)
#define BSC_S_RXR	(1 << 3)
#define BSC_S_TXW	(1 << 2)
#define BSC_S_DONE	(1 << 1)
#define BSC_S_TA	1

#define CLEAR_STATUS	BSC_S_CLKT|BSC_S_ERR|BSC_S_DONE


/* I2C UTILS */
static void i2c_init(void) {
    INP_GPIO(2);
    SET_GPIO_ALT(2, 0);
    INP_GPIO(3);
    SET_GPIO_ALT(3, 0);
}

static void wait_i2c_done(void) {
    while ((!((BSC1_S) & BSC_S_DONE))) {
        udelay(100);
    }
}

// Function to write data to an I2C device via the FIFO.  This doesn't refill the FIFO, so writes are limited to 16 bytes
// including the register address. len specifies the number of bytes in the buffer.

static void i2c_write(char dev_addr, char reg_addr, char *buf, unsigned short len) {

    int idx;

    BSC1_A = dev_addr;
    BSC1_DLEN = len + 1; // one byte for the register address, plus the buffer length

    BSC1_FIFO = reg_addr; // start register address
    for (idx = 0; idx < len; idx++)
        BSC1_FIFO = buf[idx];

    BSC1_S = CLEAR_STATUS; // Reset status bits (see #define)
    BSC1_C = START_WRITE; // Start Write (see #define)

    wait_i2c_done();

}

// Function to read a number of bytes into a  buffer from the FIFO of the I2C controller

static void i2c_read(char dev_addr, char reg_addr, char *buf, unsigned short len) {
    unsigned short bufidx;

    i2c_write(dev_addr, reg_addr, NULL, 0);

    bufidx = 0;

    memset(buf, 0, len); // clear the buffer

    BSC1_DLEN = len;
    BSC1_S = CLEAR_STATUS; // Reset status bits (see #define)
    BSC1_C = START_READ; // Start Read after clearing FIFO (see #define)

    do {
        // Wait for some data to appear in the FIFO
        while ((BSC1_S & BSC_S_TA) && !(BSC1_S & BSC_S_RXD));

        // Consume the FIFO
        while ((BSC1_S & BSC_S_RXD) && (bufidx < len)) {
            buf[bufidx++] = BSC1_FIFO;
        }
    } while ((!(BSC1_S & BSC_S_DONE)));
}

/*  ------------------------------------------------------------------------------- */

static void mk_mcp23017_read_packet(struct mk_pad * pad, unsigned char *data) {
    int i;
    char resultA, resultB;
    i2c_read(pad->mcp23017addr, MPC23017_GPIOA_READ, &resultA, 1);
    i2c_read(pad->mcp23017addr, MPC23017_GPIOB_READ, &resultB, 1);

    // read direction
    for (i = 0; i < 4; i++) {
        data[i] = !((resultA >> mk_arcade_gpioa_maps[i]) & 0x1);
    }
    // read buttons on gpioa
    for (i = 4; i < 8; i++) {
        data[i] = !((resultA >> mk_arcade_gpioa_maps[i]) & 0x1);
    }
    // read buttons on gpiob
    for (i = 8; i < 16; i++) {
        data[i] = !((resultB >> (mk_arcade_gpiob_maps[i-8])) & 0x1);
    }
}


static int __init mk_setup_pad(struct mk *mk, int idx, int pad_type_arg) {
    struct mk_pad *pad = &mk->pads[idx];
    struct input_dev *input_dev;
    int i, pad_type;
    int err;
    char FF = 0xFF;
    pr_err("pad type : %d\n",pad_type_arg);

    if (pad_type_arg >= MK_MAX) {
        pad_type = MK_ARCADE_MCP23017;
    } else {
        pad_type = pad_type_arg;
    }

    pad->type = pad_type;
    pad->mcp23017addr = pad_type_arg;
    snprintf(pad->phys, sizeof (pad->phys),
            "input%d", idx);

    input_dev->name = mk_names[pad_type];
    input_dev->phys = pad->phys;
    input_dev->id.bustype = BUS_PARPORT;
    input_dev->id.vendor = 0x0001;
    input_dev->id.product = pad_type;
    input_dev->id.version = 0x0100;

    input_set_drvdata(input_dev, mk);

    input_dev->open = mk_open;
    input_dev->close = mk_close;

    input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);

    for (i = 0; i < 2; i++) {
        input_set_abs_params(input_dev, ABS_X + i, -1, 1, 0, 0);
    }

	if (pad_type != MK_ARCADE_MCP23017) {
        mk_current_arcade_buttons = mk_max_arcade_buttons;
        if(mk_uses_hotkey != 0) mk_current_arcade_buttons ++;
	} else { //Checking for MCP23017 so it gets 4 more buttons registered to it.
		mk_current_arcade_buttons = mk_max_mcp_arcade_buttons;
	}

    for (i = 0; i < mk_current_arcade_buttons; i++){
        __set_bit(mk_arcade_gpio_btn[i], input_dev->keybit);
    }

    mk->pad_count[pad_type]++;

    // asign gpio pins
    switch (pad_type) {
        case MK_ARCADE_MCP23017:
            // nothing to asign if MCP23017 is used
            break;
    }

    // initialize gpio if not MCP23017, else initialize i2c
    if(pad_type == MK_ARCADE_MCP23017){
        i2c_init();
        udelay(1000);
        // Put all GPIOA inputs on MCP23017 in INPUT mode
        i2c_write(pad->mcp23017addr, MPC23017_GPIOA_MODE, &FF, 1);
        udelay(1000);
        // Put all inputs on MCP23017 in pullup mode
        i2c_write(pad->mcp23017addr, MPC23017_GPIOA_PULLUPS_MODE, &FF, 1);
        udelay(1000);
        // Put all GPIOB inputs on MCP23017 in INPUT mode
        i2c_write(pad->mcp23017addr, MPC23017_GPIOB_MODE, &FF, 1);
        udelay(1000);
        // Put all inputs on MCP23017 in pullup mode
        i2c_write(pad->mcp23017addr, MPC23017_GPIOB_PULLUPS_MODE, &FF, 1);
        udelay(1000);
        // Put all inputs on MCP23017 in pullup mode a second time
        // Known bug : if you remove this line, you will not have pullups on GPIOB 
        i2c_write(pad->mcp23017addr, MPC23017_GPIOB_PULLUPS_MODE, &FF, 1);
        udelay(1000);
    }

    err = input_register_device(pad->dev);
    if (err)
        goto err_free_dev;

    return 0;

err_free_dev:
    input_free_device(pad->dev);
    pad->dev = NULL;
    return err;
}
