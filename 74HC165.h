static const int mk_data_size = 32;

static const int mk_max_arcade_buttons = 13;
static const int mk_max_mcp_arcade_buttons = 16;
static const int mk_max_mux_arcade_buttons = 16;

static int mk_current_arcade_buttons = 0;
static int mk_uses_hotkey = 2; // 0 - unuse, 1 - hotkey, 2 - fn key

// Map of the gpios :                           up, down, left, right, start, select, a,  b,  tr, y,  x,  tl  hk
static const int mk_arcade_gpio_maps[]      = { 4,  17,    27,  22,    10,    9,      25, 24, 23, 18, 15, 14, 2 };
// 2nd joystick on the b+ GPIOS                  up, down, left, right, start, select, a,  b,  tr, y,  x,  tl hk
static const int mk_arcade_gpio_maps_bplus[] = { 11, 5,    6,    13,    19,    26,     21, 20, 16, 12, 7,  8, 3 };

// Map of the mcp23017 on GPIOA                  up, down, left, right, start, select, a, b
static const int mk_arcade_gpioa_maps[]      = { 0,  1,    2,    3,     4,     5,      6, 7 };
// Map of the mcp23017 on GPIOB                  tr, y, x, tl, c, tr2, z, tl2
static const int mk_arcade_gpiob_maps[]      = { 0,  1, 2, 3,  4, 5,   6, 7 };

// Map joystick on the b+ GPIOS with TFT         up, down, left, right, start, select, a,  b, tr, y,  x,  tl
static const int mk_arcade_gpio_maps_tft[]   = { 21, 13,   26,   19,    5,     6,      22, 4, 20, 17, 27, 16 };

static const short mk_arcade_gpio_btn[] = {
	BTN_START, BTN_SELECT, BTN_A, BTN_B, BTN_TR, BTN_Y, BTN_X, BTN_TL, BTN_C, BTN_TR2, BTN_Z, BTN_TL2, BTN_HOTKEY
};

static const short mk_arcade_btn[] = {
	BTN_START, BTN_SELECT, BTN_A, BTN_B, BTN_TR, BTN_Y, BTN_X, BTN_TL, BTN_C, BTN_TR2, BTN_Z, BTN_TL2, 

    BTN_MISC + 0, BTN_MISC + 1, BTN_MISC + 2, BTN_MISC + 3, BTN_MISC + 4, BTN_MISC + 5, BTN_MISC + 6, BTN_MISC + 7, 
    BTN_MISC + 8, BTN_MISC + 9, BTN_MISC + 10, BTN_MISC + 11, BTN_MISC + 12, BTN_MISC + 13, BTN_MISC + 14, BTN_MISC + 15
};

static void mk_74hc165_read_packet(struct mk_pad * pad, unsigned char *data) {
    int i, idx, value;
    int ld = pad->gpio_maps[0];
    int cl = pad->gpio_maps[1];
    int readp = pad->gpio_maps[2];
    int startoffs = pad->start_offs;
    int loopcount = pad->button_count;

    putGpioValue(ld, 0);
    udelay(5);
    putGpioValue(ld, 1);
    idx = 0;
    for (i = 0; i < startoffs; i++) {
        value = GET_GPIO(readp);
    }
    for (i = 0; i < loopcount; i++) {
        value = GET_GPIO(readp);
        data[i] = (value == 0)? 1 : 0;
    }
    for (i = loopcount; i < mk_current_arcade_buttons; i++) {
        data[i] = (value == 0)? 1 : 0;
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

    if (pad_type < 1 || pad_type >= MK_MAX) {
        pr_err("Pad type %d unknown\n", pad_type);
        return -EINVAL;
    }

   if (pad_type == MK_ARCADE_GPIO_74HC165) {
        // if the device is 74HC165, be sure to get correct pins
        if (gpio_cfg.nargs < 1) {
            pr_err("74HC165 device needs gpio argument\n");
            return -EINVAL;
        } else if(gpio_cfg.nargs != 3){
             pr_err("Invalid gpio argument\n", pad_type);
             return -EINVAL;
        }
    }

    pr_err("pad type : %d\n",pad_type);
    pad->dev = input_dev = input_allocate_device();
    if (!input_dev) {
        pr_err("Not enough memory for input device\n");
        return -ENOMEM;
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
        case MK_ARCADE_GPIO_74HC165:
            memcpy(pad->gpio_maps, gpio_cfg.mk_arcade_gpio_maps_custom, 3 *sizeof(int));
            pad->start_offs = 0;
            pad->button_count = mk_current_arcade_buttons;
            if (ext_cfg.nargs >= 1) {
                pad->start_offs = ext_cfg.args[0];
                if (ext_cfg.nargs >= 2) {
                    pad->button_count = ext_cfg.args[1];
                }
            }
            break;
    }

    if(pad_type == MK_ARCADE_GPIO_74HC165) {
        for (i = 0; i < 3; i++) {
            printk("GPIO = %d\n", pad->gpio_maps[i]);
        }
        setGpioAsOutput(pad->gpio_maps[0]);
        setGpioAsOutput(pad->gpio_maps[1]);
        setGpioAsInput(pad->gpio_maps[2]);
        setGpioPullUps(getPullUpMask(&pad->gpio_maps[2], 1));
        printk("GPIO configured for pad%d\n", idx);
    } else {
        for (i = 0; i < mk_max_arcade_buttons; i++) {
            printk("GPIO = %d\n", pad->gpio_maps[i]);
            if(pad->gpio_maps[i] != -1){    // to avoid unused buttons
                 setGpioAsInput(pad->gpio_maps[i]);
            }                
        }
        setGpioPullUps(getPullUpMask(pad->gpio_maps, 12));
        printk("GPIO configured for pad%d\n", idx);
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
