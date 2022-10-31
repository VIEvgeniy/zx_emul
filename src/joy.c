#include "joy.h"

volatile int us_val=0;
void d_sleep_us(uint us)
{
    for(uint i=0;i<us;i++)
    {
        for(int j=0;j<25;j++)
        {
            us_val++;
        }
    }
}
void blink(uint8_t count)
{
    for(int i=0;i<count;i++)
    {
        sleep_ms(200);
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
        sleep_ms(100);
        gpio_put(PICO_DEFAULT_LED_PIN, 0);
    }
}

void Joy_init(Joy *j)
{
    gpio_init(j->clock_pin);
    gpio_set_dir(j->clock_pin,GPIO_OUT);
    gpio_init(j->latch_pin);
    gpio_set_dir(j->latch_pin,GPIO_OUT);

    gpio_init(j->data_pin);
    gpio_set_dir(j->data_pin,GPIO_IN);
    gpio_pull_up(j->data_pin);
    gpio_put(j->latch_pin,0);
    j->data = 0;
    j->old_data = 0;
    j->mode = 0;
};

void Joy_get_data(Joy *j)
{
    j->old_data = j->data;

    gpio_put(j->latch_pin,1);
    d_sleep_us(12);
    gpio_put(j->latch_pin,0);
    d_sleep_us(6);

    for(int i=0;i<8;i++)
    {   
       
        gpio_put(j->clock_pin,0);  
        d_sleep_us(10);
        j->data<<=1;
        j->data|=gpio_get(j->data_pin);
        d_sleep_us(10);

        
        gpio_put(j->clock_pin,1); 
        d_sleep_us(10);

    }
};
inline bool Joy_get_left(Joy *j)
{
    return ((~j->data))&0x02; 
}
inline bool Joy_is_left(Joy *j)
{
    return (((j->data)&0x02)^((j->old_data)&0x02)); 
}
inline bool Joy_get_right(Joy *j)
{
    return (~j->data)&0x01; 
}
inline bool Joy_is_right(Joy *j)
{
    return (((j->data)&0x01)^((j->old_data)&0x01)); 
}
inline bool Joy_get_down(Joy *j)
{
    return (~j->data)&0x04; 
}
inline bool Joy_is_down(Joy *j)
{
    return (((j->data)&0x04)^((j->old_data)&0x04)); 
}
inline bool Joy_get_up(Joy *j)
{
    return (~j->data)&0x08; 
}
inline bool Joy_is_up(Joy *j)
{
    return (((j->data)&0x08)^((j->old_data)&0x08)); 
}
inline bool Joy_get_A(Joy *j)
{
    return (~j->data)&0x80; 
}
inline bool Joy_is_A(Joy *j)
{
    return (((j->data)&0x80)^((j->old_data)&0x80)); 
}
inline bool Joy_get_B(Joy *j)
{
    return (~j->data)&0x40; 
}
inline bool Joy_is_B(Joy *j)
{
    return (((j->data)&0x40)^((j->old_data)&0x40)); 
}
inline bool Joy_get_select(Joy *j)
{
    return (~j->data)&0x20; 
}
inline bool Joy_is_select(Joy *j)
{
    return ((((j->data)&0x20))^((j->old_data)&0x20)); 
}
inline bool Joy_get_start(Joy *j)
{
    return (~j->data)&0x10; 
}
inline bool Joy_is_start(Joy *j)
{
    return (((j->data)&0x10)^((j->old_data)&0x10)); 
}

uint8_t Joy_get_kempstom_data(Joy *j)
{
    return ~((j->data&0x0f)|((j->data>>3)&0x30)|((j->data<<3)&0x80)|((j->data<<1)&0x40));
}
bool Joy_is_new_data(Joy *j)
{
    return ((j->data)^(j->old_data));
}

void Joy_to_zx (Joy *J, ZX_Input_t *zx_input)
{
    printf("data joy=0x%02x\n",~J->data);
    printf("old data joy=0x%02x\n",~J->old_data);
    switch(J->mode)
    {
        case JOY_KEMPSTON_MODE:
            zx_input->kempston = Joy_get_kempstom_data(J);
        break;
        case JOY_SINCLAIR1_MODE:
            if (Joy_is_left(J)){if(Joy_get_left(J))zx_input->kb_data[4]|=1<<4;else zx_input->kb_data[4]&=~(1<<4);};
            if (Joy_is_right(J)){if(Joy_get_right(J))zx_input->kb_data[4]|=1<<3;else zx_input->kb_data[4]&=~(1<<3);};
            if (Joy_is_down(J)){if(Joy_get_down(J))zx_input->kb_data[4]|=1<<2;else zx_input->kb_data[4]&=~(1<<2);};
            if (Joy_is_up(J)){if(Joy_get_up(J))zx_input->kb_data[4]|=1<<1;else zx_input->kb_data[4]&=~(1<<1);};
            if (Joy_is_A(J)){if(Joy_get_A(J))zx_input->kb_data[4]|=1<<0;else zx_input->kb_data[4]&=~(1<<0);};
        break;
        case JOY_SINCLAIR2_MODE:
            if (Joy_is_left(J)){if(Joy_get_left(J))zx_input->kb_data[3]|=1<<0;else zx_input->kb_data[3]&=~(1<<0);};
            if (Joy_is_right(J)){if(Joy_get_right(J))zx_input->kb_data[3]|=1<<1;else zx_input->kb_data[3]&=~(1<<1);}
            if (Joy_is_down(J)){if(Joy_get_down(J))zx_input->kb_data[3]|=1<<2;else zx_input->kb_data[3]&=~(1<<2);}
            if (Joy_is_up(J)){if(Joy_get_up(J))zx_input->kb_data[3]|=1<<3;else zx_input->kb_data[3]&=~(1<<3);};
            if (Joy_is_A(J)){if(Joy_get_A(J))zx_input->kb_data[3]|=1<<4;else zx_input->kb_data[3]&=~(1<<4);}
        break;
        case JOY_CURSOR_MODE:
            if (Joy_is_left(J)){if(Joy_get_left(J)){zx_input->kb_data[0]|=1<<0;zx_input->kb_data[3]|=1<<4;}else{zx_input->kb_data[3]&=~(1<<4);zx_input->kb_data[0]&=~(1<<0);}}
            if (Joy_is_right(J)){if(Joy_get_right(J)){zx_input->kb_data[0]|=1<<0;zx_input->kb_data[4]|=1<<2;}else {zx_input->kb_data[4]&=~(1<<2);zx_input->kb_data[0]&=~(1<<0);}}
            if (Joy_is_down(J)){if(Joy_get_down(J)){zx_input->kb_data[0]|=1<<0;zx_input->kb_data[4]|=1<<4;}else {zx_input->kb_data[4]&=~(1<<4);zx_input->kb_data[0]&=~(1<<0);}}
            if (Joy_is_up(J)){if(Joy_get_up(J)){zx_input->kb_data[0]|=1<<0;zx_input->kb_data[4]|=1<<3;}else {zx_input->kb_data[4]&=~(1<<3);zx_input->kb_data[0]&=~(1<<0);}}
            if (Joy_is_A(J)){if(Joy_get_A(J))zx_input->kb_data[4]|=1<<0;else zx_input->kb_data[4]&=~(1<<0);}
        break;
        case JOY_KEYBOARD_MODE:
            if (Joy_is_left(J)){if(Joy_get_left(J))zx_input->kb_data[5]|=1<<1;else zx_input->kb_data[5]&=~(1<<1);};
            if (Joy_is_right(J)){if(Joy_get_right(J))zx_input->kb_data[5]|=1<<0;else zx_input->kb_data[5]&=~(1<<0);};
            if (Joy_is_down(J)){if(Joy_get_down(J))zx_input->kb_data[1]|=1<<0;else zx_input->kb_data[1]&=~(1<<0);}
            if (Joy_is_up(J)){if(Joy_get_up(J))zx_input->kb_data[2]|=1<<0;else zx_input->kb_data[2]&=~(1<<0);};
            if (Joy_is_A(J)){if(Joy_get_A(J))zx_input->kb_data[7]|=1<<0;else zx_input->kb_data[7]&=~(1<<0);}
        break;
    };
    uint8_t tmp_data = J->data | J->old_data;
    if(Joy_is_start(J)&&Joy_is_select(J)&&(!Joy_is_A(J))&&(!Joy_is_B(J))&&(!Joy_is_left(J))&&(!Joy_is_right(J))&&(!Joy_is_down(J))&&(!Joy_is_up(J)))
    {
        printf("restart\n");
        watchdog_enable(1, 1);
        while(1);
    }
    if (Joy_is_start(J)){if(Joy_get_start(J))zx_input->kb_data[6]|=1;else zx_input->kb_data[6]&=~1;}
    if(Joy_is_select(J)){
        if((J->mode += Joy_get_select(J))==JOY_MODE_COUNT)J->mode = 0;
        if(Joy_get_select(J)) blink(J->mode+1);
    }
    if (Joy_is_B(J)){if(Joy_get_B(J))zx_input->kb_data[4]|=1;else zx_input->kb_data[4]&=~1;}

    J->old_data = J->data;
}
