#define PICO_FLASH_SPI_CLKDIV 4
// #define PICO_FLASH_SIZE_BYTES (4 * 1024 * 1024)
//PICO_FLASH_SPI_CLKDIV

#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/flash.h"
#include <hardware/sync.h>
#include <hardware/irq.h>
#include "hardware/watchdog.h"
#include "hardware/clocks.h"
#include "hardware/structs/systick.h"
#include "hardware/pwm.h"
#include "hardware/vreg.h"


#include "g_config.h"
#include "VGA.h"
#include "ps2.h"
#include "PIO_program1.h"

#include "zx_emu/zx_machine.h"
#include "zx_emu/aySoundSoft.h"

#include "joy.h"


void software_reset()
{
    watchdog_enable(1, 1);
    while(1);
}

//функция ввода загрузки спектрума
uint8_t valLoad=0;
#define PIN_ZX_LOAD (22)
bool hw_zx_get_bit_LOAD()
{
    uint8_t out;
    out=gpio_get(PIN_ZX_LOAD);
    valLoad=out*10;
    return out;
};

void* ZXThread()
{
    zx_machine_init();
    zx_machine_main_loop_start();
    G_PRINTF_INFO("END spectrum emulation\n");
    return NULL;
}

void  inInit(uint gpio)
{
    gpio_init(gpio);
    gpio_set_dir(gpio,GPIO_IN);
    gpio_pull_up(gpio);

}

//функции вывода звука спектрума
#define ZX_AY_PWM_PIN0 (26)
#define ZX_AY_PWM_PIN1 (27)
#define ZX_BEEP_PIN (28)

#define TST_PIN (29)

uint8_t valBEEP=0;
uint8_t valSAVE=0;
bool b_beep;
bool b_save;


void __not_in_flash_func(hw_zx_set_snd_out)(bool val) {b_beep=val;gpio_put(ZX_BEEP_PIN,b_beep^b_save);};
void __not_in_flash_func(hw_zx_set_save_out)(bool val){b_save=val;gpio_put(ZX_BEEP_PIN,b_beep^b_save);};


void PWM_init_pin(uint pinN)
    {
        gpio_set_function(pinN, GPIO_FUNC_PWM);
        uint slice_num = pwm_gpio_to_slice_num(pinN);
        
        pwm_config c_pwm=pwm_get_default_config();
        pwm_config_set_clkdiv(&c_pwm,1.0);
        pwm_config_set_wrap(&c_pwm,255);//MAX PWM value
        pwm_init(slice_num,&c_pwm,true);
    }

bool __not_in_flash_func(AY_timer_callback)(repeating_timer_t *rt) 
    {
        static uint inx;
        static uint8_t out=0;       
       // gpio_put(TST_PIN,inx++&1);
        pwm_set_gpio_level(ZX_AY_PWM_PIN0,out);
        pwm_set_gpio_level(ZX_AY_PWM_PIN1,out);
        uint8_t* AY_data=get_AY_Out(5);
        out=AY_data[0]+AY_data[1]+AY_data[2]+valLoad;//+valBEEP+valSAVE;

        return true;
    }
bool zx_flash_callback(repeating_timer_t *rt) {zx_machine_flashATTR();};

Joy joy2 = {16, 14, 15, 0, 0, 0};
Joy joy1 = {2, 5, 4, 0, 0, 0};

int main(void)
{  
    vreg_set_voltage(VREG_VOLTAGE_1_20);
    sleep_ms(100);
    // set_sys_clock_khz(252000, true);
    // sleep_ms(10);

    set_sys_clock_khz(252000, false);
    stdio_init_all();
    g_delay_ms(100);

    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    G_PRINTF("Main Program Start!!!\n");
    G_PRINTF("CPU clock=%d Hz\n",clock_get_hz(clk_sys));
   
    //пин ввода звука
    inInit(PIN_ZX_LOAD);
    //пины вывода звука
    PWM_init_pin(ZX_AY_PWM_PIN0);
    PWM_init_pin(ZX_AY_PWM_PIN1);
    repeating_timer_t timer;
    int hz = 44000;
    gpio_init(TST_PIN);
    gpio_set_dir(TST_PIN,GPIO_OUT);
    
    gpio_init(ZX_BEEP_PIN);
    gpio_set_dir(ZX_BEEP_PIN,GPIO_OUT);
   // negative timeout means exact delay (rather than delay between callbacks)
    if (!add_repeating_timer_us(-1000000 / hz, AY_timer_callback, NULL, &timer)) {
        G_PRINTF_ERROR("Failed to add timer\n");
        return 1;
    }

    repeating_timer_t zx_flash_timer;
    hz=1;
    if (!add_repeating_timer_us(-1000000 / hz, zx_flash_callback, NULL, &zx_flash_timer)) {
        G_PRINTF_ERROR("Failed to add zx flash timer\n");
        return 1;
    }

 



    g_delay_ms(100);

    startVGA();
    start_PS2_capture();
    //multicore_launch_core1( start_PS2_capture);

    //ZXThread();
    multicore_launch_core1(ZXThread);


    G_PRINTF("starting main loop \n");

    ZX_Input_t zx_input;
   // memset(&zx_input,0,sizeof(ZX_Input_t));
    //kb_u_state kb_st;
   // memset(&kb_st,0,sizeof(kb_st));
    Joy_init(&joy1);
    Joy_init(&joy2);
    uint inx=0;        
  
    convert_kb_u_to_kb_zx(&kb_st_ps2,zx_input.kb_data);
    zx_machine_input_set(&zx_input);

    while(1)
    {   
        if (decode_PS2())
        {
            if (((kb_st_ps2.u[1]&KB_U1_L_CTRL)||(kb_st_ps2.u[1]&KB_U1_R_CTRL))&&((kb_st_ps2.u[1]&KB_U1_L_ALT)||(kb_st_ps2.u[1]&KB_U1_R_ALT))&&(kb_st_ps2.u[2]&KB_U2_DELETE))
            {
                G_PRINTF_INFO("restart\n");
                software_reset();
            }
            convert_kb_u_to_kb_zx(&kb_st_ps2,zx_input.kb_data);
            

            uint inx_f1=0;
            switch (kb_st_ps2.u[3])
            {
            case KB_U3_F1: inx_f1=1;break;
            case KB_U3_F2: inx_f1=2;break;
            case KB_U3_F3: inx_f1=3;break;
            case KB_U3_F4: inx_f1=4;break;
            case KB_U3_F5: inx_f1=5;break;
            case KB_U3_F6: inx_f1=6;break;
            case KB_U3_F7: inx_f1=7;break;
            case KB_U3_F8: inx_f1=8;break;
            case KB_U3_F9: inx_f1=9;break;
            case KB_U3_F10: inx_f1=10;break;
            case KB_U3_F11: inx_f1=11;break;
            case KB_U3_F12: inx_f1=12;break;
            
            default:
                break;
            }
            if (inx_f1)
            if ((kb_st_ps2.u[1]&KB_U1_L_CTRL)||(kb_st_ps2.u[1]&KB_U1_R_CTRL)) 
                    {
                        G_PRINTF_INFO("save slot %d\n",inx_f1);
                    }
                else{
                        G_PRINTF_INFO("load slot %d\n",inx_f1);

                    };
            zx_machine_input_set(&zx_input);
        };
        g_delay_ms(1);
        //опрос джоя
        // if (!(inx++&0xF))  new_data_joy=d_joy_get_data();   
        if (!(inx++&0xF)) {Joy_get_data(&joy1);Joy_get_data(&joy2);}
        //continue;

        if(Joy_is_new_data(&joy1))Joy_to_zx(&joy1,&zx_input);
        if(Joy_is_new_data(&joy2))Joy_to_zx(&joy2,&zx_input);
        zx_machine_input_set(&zx_input);
    }
}