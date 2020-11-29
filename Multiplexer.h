
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


static void mk_multiplexer_read_packet(struct mk_pad * pad, unsigned char *data) {
    int i, value;
    int addr0 = pad->gpio_maps[0];
    int addr1 = pad->gpio_maps[1];
    int addr2 = pad->gpio_maps[2];
    int addr3 = pad->gpio_maps[3];
    int readp = pad->gpio_maps[4];
    int startoffs = pad->start_offs;
    int loopcount = pad->button_count;

    for (i = 0; i < loopcount; i++) {
        int addr = i + startoffs;
        putGpioValue(addr0, addr & 1);
        putGpioValue(addr1, (addr >> 1) & 1);
        putGpioValue(addr2, (addr >> 2) & 1);
        putGpioValue(addr3, (addr >> 3) & 1);
        udelay(5);
        value = GET_GPIO(readp);
        data[i] = (value == 0)? 1 : 0;
    }
    for (i = loopcount; i < mk_current_arcade_buttons; i++) {
        data[i] = (value == 0)? 1 : 0;
    }
}

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

static void mk_input_report(struct mk_pad * pad, unsigned char * data) {
    struct input_dev * dev = pad->dev;
    int j;
    
    input_report_abs(dev, ABS_Y, !data[0]-!data[1]);
    input_report_abs(dev, ABS_X, !data[2]-!data[3]);
    for (j = 4; j < mk_current_arcade_buttons; j++) {
        input_report_key(dev, mk_arcade_btn[j - 4], data[j]);
    }
    input_sync(dev);

    for(j = 0; j < mk_current_arcade_buttons; j++)
        pad->current_button_state[i] = data[i];
}

static void mk_process_packet(struct mk *mk) {

    unsigned char data[mk_data_size];
    struct mk_pad *pad;
    int i, j;

    for (i = 0; i < MK_MAX_DEVICES; i++) {
        pad = &mk->pads[i];
        if (pad->type == MK_ARCADE_GPIO || pad->type == MK_ARCADE_GPIO_BPLUS || pad->type == MK_ARCADE_GPIO_TFT || pad->type == MK_ARCADE_GPIO_CUSTOM) {
            mk_gpio_read_packet(pad, data);
            //mk_input_report(pad, data);
        } else if (pad->type == MK_ARCADE_MCP23017) {
            mk_mcp23017_read_packet(pad, data);
            //mk_input_report(pad, data);
        } else if (pad->type == MK_ARCADE_GPIO_MULTIPLEXER) {
            mk_multiplexer_read_packet(pad, data);
            //mk_input_report(pad, data);
        } else if (pad->type == MK_ARCADE_GPIO_74HC165) {
            mk_74hc165_read_packet(pad, data);
            //mk_input_report(pad, data);
        } else {
            continue;
        }

        mk_input_report(pad, data);
    }

}

/*
 * mk_timer() initiates reads of console pads data.
 */

static void mk_timer(unsigned long private) {
    struct mk *mk = (void *) private;
    mk_process_packet(mk);
    mod_timer(&mk->timer, jiffies + MK_REFRESH_TIME);
}

static int mk_open(struct input_dev *dev) {
    struct mk *mk = input_get_drvdata(dev);
    int err;

    err = mutex_lock_interruptible(&mk->mutex);
    if (err)
        return err;

    if (!mk->used++)
        mod_timer(&mk->timer, jiffies + MK_REFRESH_TIME);

    mutex_unlock(&mk->mutex);
    return 0;
}

static void mk_close(struct input_dev *dev) {
    struct mk *mk = input_get_drvdata(dev);

    mutex_lock(&mk->mutex);
    if (!--mk->used) {
        del_timer_sync(&mk->timer);
    }
    mutex_unlock(&mk->mutex);
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

    if (pad_type == MK_ARCADE_GPIO_CUSTOM) {
        // if the device is custom, be sure to get correct pins
        if (gpio_cfg.nargs < 1) {
            pr_err("Custom device needs gpio argument\n");
            return -EINVAL;
        } else if(gpio_cfg.nargs != 12){
             pr_err("Invalid gpio argument\n", pad_type);
             return -EINVAL;
        }
    } else if (pad_type == MK_ARCADE_GPIO_MULTIPLEXER) {
        // if the device is multiplexer, be sure to get correct pins
        if (gpio_cfg.nargs < 1) {
            pr_err("Multiplexer device needs gpio argument\n");
            return -EINVAL;
        } else if(gpio_cfg.nargs != 5){
             pr_err("Invalid gpio argument\n", pad_type);
             return -EINVAL;
        }
    } else if (pad_type == MK_ARCADE_GPIO_74HC165) {
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
        case MK_ARCADE_GPIO_MULTIPLEXER:
            memcpy(pad->gpio_maps, gpio_cfg.mk_arcade_gpio_maps_custom, 5 *sizeof(int));
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

    // initialize gpio if not MCP23017, else initialize i2c
    if(pad_type == MK_ARCADE_GPIO_MULTIPLEXER) {
        for (i = 0; i < 5; i++) {
            printk("GPIO = %d\n", pad->gpio_maps[i]);
        }
        setGpioAsOutput(pad->gpio_maps[0]);
        setGpioAsOutput(pad->gpio_maps[1]);
        setGpioAsOutput(pad->gpio_maps[2]);
        setGpioAsOutput(pad->gpio_maps[3]);
        setGpioAsInput(pad->gpio_maps[4]);
        setGpioPullUps(getPullUpMask(&pad->gpio_maps[4], 1));
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

static struct mk __init *mk_probe(int *pads, int n_pads) {
    struct mk *mk;
    int i;
    int count = 0;
    int err;

    mk = kzalloc(sizeof (struct mk), GFP_KERNEL);
    if (!mk) {
        pr_err("Not enough memory\n");
        err = -ENOMEM;
        goto err_out;
    }

    mutex_init(&mk->mutex);
    setup_timer(&mk->timer, mk_timer, (long) mk);

    for (i = 0; i < n_pads && i < MK_MAX_DEVICES; i++) {
        if (!pads[i])
            continue;

        err = mk_setup_pad(mk, i, pads[i]);
        if (err)
            goto err_unreg_devs;

        count++;
    }

    if (count == 0) {
        pr_err("No valid devices specified\n");
        err = -EINVAL;
        goto err_free_mk;
    }

    return mk;

err_unreg_devs:
    while (--i >= 0)
        if (mk->pads[i].dev)
            input_unregister_device(mk->pads[i].dev);
err_free_mk:
    kfree(mk);
err_out:
    return ERR_PTR(err);
}
