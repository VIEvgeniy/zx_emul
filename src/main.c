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

#define D_JOY_DATA_PIN (16)
#define D_JOY_CLK_PIN (14)
#define D_JOY_LATCH_PIN (15)
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
uint8_t d_joy_get_data()
{
    uint8_t data;
    gpio_put(D_JOY_LATCH_PIN,1);
    //gpio_put(D_JOY_CLK_PIN,1);
    d_sleep_us(12);
    gpio_put(D_JOY_LATCH_PIN,0);
    d_sleep_us(6);

    for(int i=0;i<8;i++)
    {   
       
        gpio_put(D_JOY_CLK_PIN,0);  
        d_sleep_us(10);
        data<<=1;
        data|=gpio_get(D_JOY_DATA_PIN);
        d_sleep_us(10);

        
        gpio_put(D_JOY_CLK_PIN,1); 
        d_sleep_us(10);

    }
    return data;

};
void d_joy_init()
{
    gpio_init(D_JOY_CLK_PIN);
    gpio_set_dir(D_JOY_CLK_PIN,GPIO_OUT);
    gpio_init(D_JOY_LATCH_PIN);
    gpio_set_dir(D_JOY_LATCH_PIN,GPIO_OUT);

    gpio_init(D_JOY_DATA_PIN);
    gpio_set_dir(D_JOY_DATA_PIN,GPIO_IN);
    //gpio_pull_up(D_JOY_DATA_PIN);
    gpio_pull_down(D_JOY_DATA_PIN);
    gpio_put(D_JOY_LATCH_PIN,0);


}
int main(void)
{  
    vreg_set_voltage(VREG_VOLTAGE_1_20);
    sleep_ms(100);
    // set_sys_clock_khz(252000, true);
    // sleep_ms(10);

    set_sys_clock_khz(252000, false);
    stdio_init_all();
    g_delay_ms(100);

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
    d_joy_init();
    uint8_t data_joy=0;
    uint inx=0;        
    uint8_t new_data_joy;

    uint8_t new_start_button;
    uint8_t start_button=0;
    uint8_t start_key=1;

    uint8_t new_select_button;
    uint8_t select_button=0;
    uint8_t select_key=1;

    uint8_t new_B_button;
    uint8_t B_button=0;
    uint8_t B_key=1;

    // uint8_t joy_mode=0;

    // char joy_mode_str[4][12] = {"kempston","sinclair1","sinclair2","cursor"};
    // #define JOY_KEMPSTON_MODE 0
    // #define JOY_SINCLAIR1_MODE 1
    // #define JOY_SINCLAIR2_MODE 2
    // #define JOY_CURSOR_MODE 3

    convert_kb_u_to_kb_zx(&kb_st_ps2,zx_input.kb_data);

    while(1)
    {   
        if (decode_PS2())
        {
            convert_kb_u_to_kb_zx(&kb_st_ps2,zx_input.kb_data);
            
            if (((kb_st_ps2.u[1]&KB_U1_L_CTRL)||(kb_st_ps2.u[1]&KB_U1_R_CTRL))&&((kb_st_ps2.u[1]&KB_U1_L_ALT)||(kb_st_ps2.u[1]&KB_U1_R_ALT))&&(kb_st_ps2.u[2]&KB_U2_DELETE))
                {
                    G_PRINTF_INFO("restart\n");
                    software_reset();
                }

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
        if (!(inx++&0xF)) {
            new_data_joy=d_joy_get_data();   

            //кнопка старт нажимает "0", select по очереди нажимает от 1 до 4
            new_start_button=((~new_data_joy&0x10)>>4)&1; 
            new_select_button=((~new_data_joy&0x20)>>5)&1; 
            new_B_button=((~new_data_joy&0x40)>>6)&1; 
            // if((start_button!=new_start_button)&&(select_button!=new_select_button)&&(new_select_button==new_start_button))
            // {
            //     joy_mode=(joy_mode+start_button)&3;
            //     // printf("%d\n",joy_mode);
            //     start_button=new_start_button;
            //     select_button=new_select_button;
            //     zx_input.kb_data[3]=0;
            // }
            // else
            // {
                if(start_button!=new_start_button)
                {
                    start_button=new_start_button;
                    if(start_button) zx_input.kb_data[6]|=start_key;else zx_input.kb_data[6]&=~start_key;
                }            
                if(select_button!=new_select_button)
                {
                    select_button=new_select_button;
                    if(select_button) zx_input.kb_data[3]|=select_key;
                    else 
                    {
                        zx_input.kb_data[3]&=~select_key;
                        select_key+=select_key;if(select_key>8)select_key=1;
                    }
                }
                if(B_button!=new_B_button)
                {
                    B_button=new_B_button;
                    if(B_button) zx_input.kb_data[4]|=B_key;else zx_input.kb_data[4]&=~B_key;
                }            
                // switch (joy_mode)
                // {
                // case JOY_KEMPSTON_MODE:
                //     break;
                // case JOY_SINCLAIR1_MODE:
                //     break;
                // case JOY_SINCLAIR2_MODE:
                //     break;
                // case JOY_CURSOR_MODE:
                //     break;
                // }
            // }
        }
        
        //continue;

        if ((new_data_joy!=data_joy)/*&&(joy_mode==JOY_KEMPSTON_MODE)*/)
        {
            data_joy=new_data_joy;
            data_joy=(data_joy&0x0f)|((data_joy>>2)&0x30)|((data_joy<<3)&0x80)|((data_joy<<1)&0x40);

            zx_input.kempston=(~data_joy);
            zx_machine_input_set(&zx_input);            
            data_joy=new_data_joy;

            // G_PRINTF_INFO("data joy=0x%02x\n",~data_joy);
            //g_delay_ms(1000);
            

        }
        
    }
    
}
