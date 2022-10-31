#include "string.h"
#include "stdbool.h"


#include "zx_machine.h"
#include "aySoundSoft.h"

#include "util.h"
#include "zx_ROM.h"
#include "128-0.h"
#include "128-1.h"

#include "z80.h"
#include "../g_config.h"
#include "hardware/structs/systick.h"

////цвета спектрума в формате 6 бит
// uint8_t zx_color[]={
//         0b00000000,
//         0b00000010,
//         0b00100000,
//         0b00100010,
//         0b00001000,
//         0b00001010,
//         0b00101000,
//         0b00101010,
//         0b00000000,
//         0b00000011,
//         0b00110000,
//         0b00110011,
//         0b00001100,
//         0b00001111,
//         0b00111100,
//         0b00111111
//     };
//переменные состояния спектрума
z80 cpu;

ZX_Input_t zx_input;
bool zx_state_48k_MODE_BLOCK=false;
uint8_t zx_RAM_bank_active=3;
#if (ZX_BPP==4)
uint8_t zx_Border_color=0x00;
static uint8_t zx_colors_2_pix[384*4];//предпосчитанные сочетания 2 цветов
#endif
#if (ZX_BPP==8)
uint16_t zx_Border_color=0x00;
static uint16_t zx_colors_2_pix[384*4];//предпосчитанные сочетания 2 цветов
#endif
uint8_t* zx_cpu_ram[4];//4 области памяти CPU
uint8_t* zx_video_ram;//4 области памяти CPU

uint8_t* zx_ram_bank[8];//8 банков памяти
uint8_t* zx_rom_bank[4];//4 области ПЗУ (48к 128к TRDOS и резерв для какого либо режима(типа тест))

typedef struct zx_vbuf_t
{
    uint8_t* data;
    bool is_displayed;
}zx_vbuf_t;

zx_vbuf_t zx_vbuf[ZX_NUM_GBUF];
zx_vbuf_t* zx_vbuf_active;


//выделение памяти может быть изменено в зависимости от платформы
uint8_t RAM[16384*8];
//uint8_t VBUFS[ZX_SCREENW*ZX_SCREENH*ZX_NUM_GBUF*ZX_BPP/8];

//

uint8_t FAST_FUNC(zx_keyboardDecode)(uint8_t addrH)
{

    //быстрый опрос

    switch (addrH)
    {
    case 0b11111111: return 0xff;break;
    case 0b11111110: return ~zx_input.kb_data[0];break;
    case 0b11111101: return ~zx_input.kb_data[1];break;
    case 0b11111011: return ~zx_input.kb_data[2];break;
    case 0b11110111: return ~zx_input.kb_data[3];break;
    case 0b11101111: return ~zx_input.kb_data[4];break;
    case 0b11011111: return ~zx_input.kb_data[5];break;
    case 0b10111111: return ~zx_input.kb_data[6];break;
    case 0b01111111: return ~zx_input.kb_data[7];break;

    }

    //несколько адресных линий в 0 - медленный опрос
    uint8_t dataOut=0;

    for(uint8_t i=0;i<8;i++)
    {
        if ((addrH&1)==0) dataOut|=zx_input.kb_data[i];//работаем в режиме нажатая клавиша=1
        addrH>>=1;
    };

    return ~dataOut;//инверсия, т.к. для спектрума нажатая клавиша = 0;
};


//функции чтения памяти и ввода-вывода
static uint8_t FAST_FUNC(read_z80)(void* userdata, uint16_t addr)
{
         if (addr<16384) return zx_cpu_ram[0][addr];
         if (addr<32768) return zx_cpu_ram[1][addr-16384];
         if (addr<49152) return zx_cpu_ram[2][addr-32768];
         return zx_cpu_ram[3][addr-49152];
}

static void FAST_FUNC(write_z80)(void* userdata, uint16_t addr, uint8_t val)
{
        if (addr<16384) return;//запрещаем писать в ПЗУ
        if (addr<32768) {zx_cpu_ram[1][addr-16384]=val;return;};
        if (addr<49152) {zx_cpu_ram[2][addr-32768]=val;return;};
        zx_cpu_ram[3][addr-49152]=val;
}

static uint8_t FAST_FUNC(in_z80)(z80* const z, uint8_t port) {

  uint8_t portH=z->_hi_addr_port;
  uint8_t portL=port;
  uint16_t port16=(portH<<8)|portL;

  if (port16&1)
    {
          uint16_t not_port16=~port16;
          if (not_port16&0x20) {return zx_input.kempston;}//kempston{return 0xff;};
//          if (port16 == 0xfffd) return AY_get_reg();  //fffd
          if (((not_port16&0x0002)==0x0002)&&((port16&0xC000)==0xC000)) return AY_get_reg();  //fffd



    }
        else
    {
        //загрузка с магнитофона и опрос клавиатуры

        if (hw_zx_get_bit_LOAD())
                                
        {

            uint8_t out_data=zx_keyboardDecode(portH);
            return out_data;
        }
        else
        {

            uint8_t out_data=zx_keyboardDecode(portH);
            return(out_data&0b10111111);
        };


    }

//   if (port16==0xFFFD){
//             //G_PRINTF_INFO("AY addr input=0x%04x\n",port16);
//             //G_PRINTF_INFO("AY data read=0x%02x\n",AY_get_reg());

//             return AY_get_reg();           
//     }
//   //------------------

//   if (portL==0x1F){return zx_input.kempston;}//kempston{return 0xff;};//
//   if (portL==0xFE){         //загрузка с магнитофона и опрос клавиатуры

//                                 if (hw_zx_get_bit_LOAD())
//                                // if (1)
//                                 {

//                                    uint8_t out_data=zx_keyboardDecode(portH);
//                                    return out_data;
//                                 }
//                               else
//                                 {

//                                   uint8_t out_data=zx_keyboardDecode(portH);
//                                   return(out_data&0b10111111);
//                                 };


//                      }

  return 0xFF;
}

static void FAST_FUNC(out_z80)(z80* const z, uint8_t port, uint8_t val) {
  uint8_t portH=z->_hi_addr_port;
  uint8_t portL=port;
  uint16_t port16=(portH<<8)|portL;

  if (port16&1)
    {
          uint16_t not_port16=~port16;

           if (((not_port16&0x0002)==0x0002)&&((port16&0xc000)==0xc000)) { AY_select_reg(val); return;} //fffd
           if (((not_port16&0x4002)==0x4002)&&((port16&0x8000)==0x8000)) { AY_set_reg(val); return;} //bffd

           //Расширение памяти и экран Spectrum-128
          if (((not_port16&0x8002)==0x8002))//7ffd
          //if (port16==0X7FFD)
           {
            if (zx_state_48k_MODE_BLOCK) return; //защёлка для 48к, запрешает манипуляцию банками
            //переключение банка памяти
            zx_RAM_bank_active=val&0x7;
            zx_cpu_ram[3]=zx_ram_bank[val&0x7];

            if (val&8) zx_video_ram=zx_ram_bank[7];else zx_video_ram=zx_ram_bank[5];
            if (val&16) zx_cpu_ram[0]=zx_rom_bank[0]; else  zx_cpu_ram[0]=zx_rom_bank[1];
            if (val&32) zx_state_48k_MODE_BLOCK=true;
            return;
            //
            }; 
            
          

    }
  else
    {
        #define OUT_SND_MASK (0b00011000)
            hw_zx_set_snd_out(val&0b10000);
            hw_zx_set_save_out(val&0b01000);
        //    if ((val&OUT_SND_MASK)!=oldSoundOutValue) {outSoundOutState^=1;digitalWrite(ZX_OUT_BEEP,outSoundOutState);};
        //    oldSoundOutValue=value&OUT_SND_MASK;
            //
            //digital_Write(ZX_OUT_BEEP,(value&0b00010000)?1:0); //если только звук без выхода записи#if (ZX_NUM_GBUF==1)
        #if (ZX_BPP==4)
            zx_Border_color=((val&0x7)<<4)|(val&0x7);//дублируем для 4 битного видеобуфера
        #else
            zx_Border_color=(zx_color[val&7]<<8)|zx_color[val&7];
        #endif
            

    }


  //для 128к версии
//   if (port16==0X7FFD)
//     {
//       if (zx_state_48k_MODE_BLOCK) return; //защёлка для 48к, запрешает манипуляцию банками
//       //переключение банка памяти
//       zx_RAM_bank_active=val&0x7;
//       zx_cpu_ram[3]=zx_ram_bank[val&0x7];

//       if (val&8) zx_video_ram=zx_ram_bank[7];else zx_video_ram=zx_ram_bank[5];
//       if (val&16) zx_cpu_ram[0]=zx_rom_bank[0]; else  zx_cpu_ram[0]=zx_rom_bank[1];
//       if (val&32) zx_state_48k_MODE_BLOCK=true;
//       return;
//      //
//     };
  //---------
  //чип AY
//   if (port16==0xFFFD){
//             //G_PRINTF_INFO("AY select  register=%d\n",val);
//             AY_select_reg(val);
//             return;
//     }
//   if (port16==0xBFFD){
//             //G_PRINTF_INFO("AY data write=0x%02x\n",val);
//             AY_set_reg(val);
//             return;
//     }
  //------------------

//   #define OUT_SND_MASK (0b00011000)
//   if (portL==0xFE) {
//     hw_zx_set_snd_out(val&0b10000);
//     hw_zx_set_save_out(val&0b01000);
// //    if ((val&OUT_SND_MASK)!=oldSoundOutValue) {outSoundOutState^=1;digitalWrite(ZX_OUT_BEEP,outSoundOutState);};
// //    oldSoundOutValue=value&OUT_SND_MASK;
//     //
//     //digital_Write(ZX_OUT_BEEP,(value&0b00010000)?1:0); //если только звук без выхода записи#if (ZX_NUM_GBUF==1)
// #if (ZX_BPP==4)
//     zx_Border_color=((val&0x7)<<4)|(val&0x7);//дублируем для 4 битного видеобуфера
// #else
//     zx_Border_color=(zx_color[val&7]<<8)|zx_color[val&7];
// #endif
//     }
}


//

void zx_machine_init()
{
    //привязка реальной RAM памяти к банкам
    for(int i=0;i<8;i++)
    {
        zx_ram_bank[i]=&RAM[i*16384];
    }
//    привязка ROM памяти
    zx_rom_bank[0]=&ROM[3*16384];//48k
    zx_rom_bank[1]=&ROM[2*16384];//128k
    zx_rom_bank[2]=&ROM[1*16384];//TRDOS
    zx_rom_bank[3]=&ROM[0*16384];//GLUK
    
    //  zx_rom_bank[0]=fuse_roms_128_1_rom;//48k
    //  zx_rom_bank[1]=fuse_roms_128_0_rom;//128k


    zx_cpu_ram[0]=zx_rom_bank[1];
    zx_cpu_ram[1]=zx_ram_bank[5];
    zx_cpu_ram[2]=zx_ram_bank[2];
    zx_cpu_ram[3]=zx_ram_bank[3];
    zx_video_ram=zx_ram_bank[5];
    zx_RAM_bank_active=3;
    zx_state_48k_MODE_BLOCK=false;
    //выделение графических буферов
    // for(int i=0;i<ZX_NUM_GBUF;i++)
    // {
    //     zx_vbuf[i].is_displayed=true;
    //     zx_vbuf[i].data=&VBUFS[i*(ZX_SCREENW*ZX_SCREENH*ZX_BPP/8)];
    // }
    zx_vbuf[0].is_displayed=true;
    zx_vbuf[0].data=g_gbuf;
    zx_vbuf_active=&zx_vbuf[0];

    //инициализация процессора

     z80_init(&cpu);
     cpu.read_byte = read_z80;
     cpu.write_byte = write_z80;
     cpu.port_in = in_z80;
     cpu.port_out = out_z80;


    G_PRINTF_INFO("zx machine initialized\n");
};


void FAST_FUNC(zx_machine_input_set)(ZX_Input_t* input_data){memcpy(&zx_input,input_data,sizeof(ZX_Input_t));};

uint8_t* FAST_FUNC(zx_machine_screen_get)(uint8_t* current_screen)
{
#if (ZX_NUM_GBUF==1)
    return zx_vbuf[0].data; //если буфер 1, то вариантов нет
#else
    //для нескольких буферов надо возвращать ранее неотображённый, если найдётся
    uint8_t* out_data=current_screen;
    zx_vbuf_t* current_out_zx_vbuf=NULL;
    for(int i=0;i<ZX_NUM_GBUF;i++)
    {
        if (zx_vbuf[i].data==current_screen) current_out_zx_vbuf=&zx_vbuf[i];//запомнить текущий буфер
        if (!zx_vbuf[i].is_displayed) out_data=zx_vbuf[i].data;//неотображённый ещё буфер

    }
    //если нашли неотображённый ранее буфер экрана, прошлый надо освободить для отрисовки
    if ((out_data!=current_screen)&&(current_out_zx_vbuf!=NULL)) current_out_zx_vbuf->is_displayed=true;


    return out_data;

#endif
};
void FAST_FUNC(zx_machine_flashATTR)(void)
{
    static bool stateFlash=true;
    stateFlash^=1;
    #if ZX_BPP==4
        if (stateFlash) memcpy(zx_colors_2_pix+512,zx_colors_2_pix,512); else memcpy(zx_colors_2_pix+512,zx_colors_2_pix+1024,512);
    #else
        if (stateFlash) memcpy(zx_colors_2_pix+512*2,zx_colors_2_pix,512*2); else memcpy(zx_colors_2_pix+512*2,zx_colors_2_pix+1024*2,512*2);
    #endif
    
}
//инициализация массива предпосчитанных цветов
void init_zx_2_pix_buffer()
{
    for(uint16_t i=0;i<384;i++)
    {
         uint8_t color=(uint8_t)i&0x7f;
         uint8_t color0=(color>>3)&0xf;
         uint8_t color1=(color&7)|(color0&0x08);

         //убрать ярко чёрный
        //  if (color0==0x80) color0=0;
        //  if (color1==0x80) color1=0;
         
         if (i>128)
         {
             //инверсные цвета для мигания
            uint8_t color_tmp=color0;
            color0=color1;
            color1=color_tmp;

         }

        for(uint8_t k=0;k<4;k++)
        {
             switch (k)
                {
                case 0:

                    zx_colors_2_pix[i*4+k]=(ZX_BPP==4)?(color0<<4)|color0:(zx_color[color0]<<8)|zx_color[color0];

                    break;
                case 2:
                    zx_colors_2_pix[i*4+k]=(ZX_BPP==4)?(color0<<4)|color1:(zx_color[color0]<<8)|zx_color[color1];

                    break;
                case 1:
                    zx_colors_2_pix[i*4+k]=(ZX_BPP==4)?(color1<<4)|color0:(zx_color[color1]<<8)|zx_color[color0];

                    break;
                case 3:
                    zx_colors_2_pix[i*4+k]=(ZX_BPP==4)?(color1<<4)|color1:(zx_color[color1]<<8)|zx_color[color1];


                    break;
                    }
        }

    }

}


void FAST_FUNC(zx_machine_main_loop_start)()
{
    //переменные для отрисовки экрана
    const int sh_y=56;
    const int sh_x=104;
    uint32_t inx_tick_screen=0;
    uint64_t tick_cpu=0;
    uint32_t x=0;
    uint32_t y=0;
    uint8_t* active_screen_buf=NULL;

    init_zx_2_pix_buffer();
#if (ZX_BPP==4)
    uint8_t* p_scr_str_buf=NULL;
#else
    uint16_t* p_scr_str_buf=NULL;
#endif
    uint8_t* p_zx_video_ram=NULL;
    uint8_t* p_zx_video_ramATTR=NULL;

    G_PRINTF_INFO("zx mashine starting\n");

    uint64_t dst_time_ns=ext_get_ns();
    uint32_t d_dst_time_ticks=0;
    uint32_t t0_time_ticks=0;

    int ticks_per_cycle=72;//63;
    int time_cycle_z80_ns=285;
    //G_PRINTF_DEBUG("time_ns=%lu trg time_ns=%lu\n",ext_get_ns(),dst_time_ns);//test
    systick_hw->csr = 0x5;
    systick_hw->rvr = 0xFFFFFF;

    G_PRINTF_DEBUG("time_tick=%d dtime tick=%d\n",get_ticks(),(1-0xffff00)&0xffffff);//test
    G_PRINTF_DEBUG("time_tick=%d dtime tick=%d\n",get_ticks(),(1-0xffff00)&0xffffff);//test

    while(1)
    {
        //if (inx_tick_screen<36) z80_gen_int(&cpu,0xFF);
       // while (ext_get_ns()<dst_time_ns) ;//ext_delay_us(1);
        
        while (((get_ticks()-t0_time_ticks)&0xffffff)<d_dst_time_ticks);
        t0_time_ticks=(t0_time_ticks+d_dst_time_ticks)&0xffffff;

        tick_cpu=cpu.cyc;
        z80_step(&cpu);
        uint32_t dt_cpu=cpu.cyc-tick_cpu;

        //dst_time_ns+=time_cycle_z80_ns*dt_cpu;
        d_dst_time_ticks=dt_cpu*ticks_per_cycle;

        inx_tick_screen+=dt_cpu;


        if (inx_tick_screen>=71680)
        {
        z80_gen_int(&cpu,0xFF);
        #if   (ZX_NUM_GBUF==1)
            //если буфер 1, то в него всегда и рисуем
            active_screen_buf=zx_vbuf[0].data;
        #else

            //реализовать подмену буфера

            active_screen_buf=NULL;

        #endif

            inx_tick_screen-=71680;
            y=0;//-sh_y;
            x=-sh_x-sh_y*448+2*inx_tick_screen;//подобрать по количеству пропускаемых строк до начала прорисовки

            if (inx_tick_screen==0) continue;
        };

        if (active_screen_buf==NULL) continue;

        for(int i=0;i<dt_cpu;i++)
        {
                x+=2;//по 2 пиксела за такт процессора

                if (x&7) continue;//для вывода блоками по 8 пикселей, для скорости, можно выводить попиксельно, для точности, но это затратнее. Надо проверять на демках
                if(x<0)  continue;//если х отрицательная(вне экрана), то дальнейшие действия бесмыссленны, ждём


                if (x==(448-sh_x)) //(448-sh_x)
                    {
                        //выход за пределы рисуемого экрана, чтобы не рисовать дальше ждём прерывания(луч в начале экрана), задав х большое отрицательное значение
                        if (y==(ZX_SCREENH-1)) {x=-71680;continue;};


                        x=-sh_x;//переставляем координату х в начало строки
                        y++;//переход на следующую строку

                        continue;
                    }


                if (x>=ZX_SCREENW)  continue;





                //смещения бордера
                const int dy=24;
                const int dx=32;

                if((x>=256+dx)||(x<dx)||(y<dy)||(y>=192+dy))//условия для бордера
                {
                //в начале каждой строки установить указатели на цвет и пикселы
                    if (x==0)
                        {


                                //установить указатель для отрисовки в начало строки(рисуем в промежуточный буфер)
                                p_scr_str_buf=active_screen_buf+y*((ZX_BPP==4)?ZX_SCREENW/2:ZX_SCREENW);


                                if ((y>=dy-1)&&(y<(192+dy)))//выставляем указатели на видеопамять изображения для следующей строки, но только на строках, где есть пикселы
                                {
                                    int ys=y-dy;
                                    //указатель на начало строки байтов пикселей
                                    p_zx_video_ram=zx_video_ram+(((ys&0b11000000)|((ys>>3)&7)|((ys&7)<<3))<<5);

                                    //указатель на начало строки байтов цветовых аттрибутов
                                    p_zx_video_ramATTR=zx_video_ram+(6144+((ys<<2)&0xFFE0));
                                }

                        }

                    //бордер
                    //8 пикселей одного цвета
                    *p_scr_str_buf++=zx_Border_color;
                    *p_scr_str_buf++=zx_Border_color;
                    *p_scr_str_buf++=zx_Border_color;
                    *p_scr_str_buf++=zx_Border_color;



                    continue;
                }




                //для любых 2 цветов есть 4 варианта их сочетания в предпосчитанном массиве
       
        #if (ZX_BPP==4)
                uint8_t* zx_colors_2_pix_current=zx_colors_2_pix+(4*(*p_zx_video_ramATTR++));
        #else
                uint16_t* zx_colors_2_pix_current=zx_colors_2_pix+(4*(*p_zx_video_ramATTR++));
        #endif
                uint8_t zx_pix8=*p_zx_video_ram++;
       
                //вывод блока из 8 пикселей
                *p_scr_str_buf++=zx_colors_2_pix_current[((zx_pix8>>6)&3)];
                *p_scr_str_buf++=zx_colors_2_pix_current[((zx_pix8>>4)&3)];
                *p_scr_str_buf++=zx_colors_2_pix_current[((zx_pix8>>2)&3)];
                *p_scr_str_buf++=zx_colors_2_pix_current[((zx_pix8>>0)&3)];


        }





        //G_PRINTF_INFO("zx cpu cycles=%lu\n",cpu.cyc);
        //g_delay_ms(1);
    }

};

