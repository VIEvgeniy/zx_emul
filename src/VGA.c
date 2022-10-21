#include "hardware/clocks.h"

#include "hardware/structs/pll.h"
#include "hardware/structs/systick.h"

#include "hardware/dma.h"
#include "hardware/irq.h"
//#include <string.h> 
#include "pico/mem_ops.h"
#include "g_config.h"
#include "VGA.h"
#include "PIO_program1.h"




uint8_t *v_buf;
static int dma_chan_ctrl;
//int dma_chan_for_copy_line;

#define SIZE_LINE_VGA (400)
//количество буферов ДМА можно увеличить, если будет подёргивание
#define N_DMA_BUF 8
#define SIZE_DMA_BUF (SIZE_LINE_VGA*N_DMA_BUF)
static uint8_t DMA_BUF[4][SIZE_DMA_BUF];
static uint8_t* DMA_BUF_ADDR[4];

//копирование из памяти в память выполняется быстрее заполнения константой
// void  inline memset32(uint32_t* dst,const uint32_t data, uint32_t size)
//     {
       
//         dst[0]=data;
//         for(int i=1;i<size;i++){*++dst=*(dst-1);};
//     }
void  __not_in_flash_func(memset64)(uint64_t* dst,const uint64_t data, uint32_t size)
    {
        
        dst[0]=data;
        for(int i=1;i<size;i++){*++dst=*(dst-1);};

        // dst[0]=data;
        // for(int i=size;--i;){*++dst=*(dst-1);};

    }

static uint32_t line_active;
static uint16_t colors[256];


void __not_in_flash_func(dma_handler_VGA)() {
    static uint32_t inx_buf_dma;  

    dma_hw->ints0 = 1u << dma_chan_ctrl;
    dma_channel_set_read_addr(dma_chan_ctrl,&DMA_BUF_ADDR[inx_buf_dma&3], false);

    inx_buf_dma++;
    
    uint8_t* buf8=DMA_BUF[inx_buf_dma&3];
    uint32_t* buf32;
    uint64_t* buf64;

    for(int ib=N_DMA_BUF;ib--;)
    {
        buf8=(DMA_BUF[inx_buf_dma&3])+(N_DMA_BUF-1-ib)*SIZE_LINE_VGA;
        
        line_active++;
        if (line_active==525) line_active=0;

        if (line_active>34 && line_active<515)
            {
               
                
                 if (N_DMA_BUF>8)
                {
                   

                    buf64=( uint64_t*)buf8;
                    memset64(buf64   ,0x8080808080808080,6);
                    memset64(buf64+6,0xc0c0c0c0c0c0c0c0,3);
                    memset64(buf64+(SIZE_LINE_VGA-8)/8,0xc0c0c0c0c0c0c0c0,1);
                }
                

               
                
                uint32_t line=(line_active-35)/2;
                
                //uint64_t* p_src64=(uint64_t*)&v_buf[line*SCREEN_W];
                uint8_t* p_src8=(uint8_t*)&v_buf[line*SCREEN_W/2];

                uint64_t* p_dst64=(uint64_t*)&buf8[72];//начало пиксельной части VGA строки  
                uint16_t* p_dst16=(uint16_t*)p_dst64;//

                static uint64_t *p_src64old; 
                //const uint64_t mask64=0x3030303030303030;
                //const uint8_t mask8=0x30;
             
                
              

                if (line_active&1)
                //if (true)
                    {
                        p_src64old=p_dst64;

                        for(uint16_t ii=SCREEN_W/64;ii--;) 
                            {
                               *p_dst16++=colors[*p_src8++];
                               *p_dst16++=colors[*p_src8++];
                               *p_dst16++=colors[*p_src8++];
                               *p_dst16++=colors[*p_src8++];
                               
                               *p_dst16++=colors[*p_src8++];
                               *p_dst16++=colors[*p_src8++];
                               *p_dst16++=colors[*p_src8++];
                               *p_dst16++=colors[*p_src8++];

                               *p_dst16++=colors[*p_src8++];
                               *p_dst16++=colors[*p_src8++];
                               *p_dst16++=colors[*p_src8++];
                               *p_dst16++=colors[*p_src8++];
                               
                               *p_dst16++=colors[*p_src8++];
                               *p_dst16++=colors[*p_src8++];
                               *p_dst16++=colors[*p_src8++];
                               *p_dst16++=colors[*p_src8++];

                               *p_dst16++=colors[*p_src8++];
                               *p_dst16++=colors[*p_src8++];
                               *p_dst16++=colors[*p_src8++];
                               *p_dst16++=colors[*p_src8++];
                               
                               *p_dst16++=colors[*p_src8++];
                               *p_dst16++=colors[*p_src8++];
                               *p_dst16++=colors[*p_src8++];
                               *p_dst16++=colors[*p_src8++];

                               *p_dst16++=colors[*p_src8++];
                               *p_dst16++=colors[*p_src8++];
                               *p_dst16++=colors[*p_src8++];
                               *p_dst16++=colors[*p_src8++];
                               
                               *p_dst16++=colors[*p_src8++];
                               *p_dst16++=colors[*p_src8++];
                               *p_dst16++=colors[*p_src8++];
                               *p_dst16++=colors[*p_src8++];
                            }
                       
                        
                    }
                else
                    {
                        //дублирование
                        for(uint16_t ii=SCREEN_W/8;ii--;) 
                            {
                                *p_dst64++=*p_src64old++;
                            }

                    }
                


            }
        else if (line_active<2)
            {

               
#ifdef NUM_V_BUF
                                
                                
                                if(line_active==0)
                                {


                                    uint8_t* v_buf_i=g_gbuf;
                                    bool is_new_frame=false;
                                    uint new_inx=0;

                                    for (int i=1;i<NUM_V_BUF;i++)
                                    {
                                        uint8_t inx=(i+draw_vbuf_inx)%NUM_V_BUF;
                                        if(!(is_show_frame[inx]))//нашли свободный кадр для отображения
                                        {
                                            new_inx=inx;
                                            is_new_frame=true;
                                            break;
                                        }
                                        
                                    };

                                    if ((is_new_frame))
                                        {
                                            is_show_frame[show_vbuf_inx]=true;
                                            show_vbuf_inx=new_inx;
                                            v_buf=g_gbuf+V_BUF_SZ*new_inx;
                                        };
                    

                                };
                               
#endif           




                buf64=( uint64_t*)buf8;
                memset64(buf64   ,0x0,6);
                memset64(buf64+6,0x4040404040404040,(SIZE_LINE_VGA-48)/8);
            }
        else
            {
              


                buf64=( uint64_t*)buf8;
                memset64(buf64   ,0x8080808080808080,6);
                memset64(buf64+6,0xc0c0c0c0c0c0c0c0,(SIZE_LINE_VGA-48)/8);

                
            };

    }
}

//цвета спектрума в формате 6 бит
const uint8_t zx_color_6b[]={
        0b00000000,
        0b00000010,
        0b00100000,
        0b00100010,
        0b00001000,
        0b00001010,
        0b00101000,
        0b00101010,
        0b00000000,
        0b00000011,
        0b00110000,
        0b00110011,
        0b00001100,
        0b00001111,
        0b00111100,
        0b00111111
    };

void startVGA()
{
    v_buf=g_gbuf;
    //для каждого видеорежима свои синхросигналы
    for(int i=0;i<256;i++) 
        {
            uint8_t ci=i;

            //смена порядка цветов R<>G
            // ci&=0b10011001;
            // ci|=(i<<1)&0b01000100;
            // ci|=(i>>1)&0b00100010;
            
            uint8_t ch=(ci&0xf0)>>4;
            uint8_t cl=ci&0xf;

            //убрать ярко-чёрный
            if (ch==0x08) ch=0;
            if (cl==0x08) cl=0;
            
            //конвертация в 6 битный формат цвета
            ch=zx_color_6b[ch&0xf];
            cl=zx_color_6b[cl&0xf];

            colors[i]=((ch)<<8)|(cl)|0xc0c0;

        };
    //инициализация PIO
    //загрузка программы в один из PIO
    uint offset = pio_add_program(PIO_p1, &pio_program1);
    uint sm=SM_out;

    for(int i=0;i<8;i++){gpio_init(beginVGA_PIN+i);
    gpio_set_dir(beginVGA_PIN+i,GPIO_OUT);pio_gpio_init(PIO_p1, beginVGA_PIN+i);};//резервируем под выход PIO
  
    pio_sm_set_consecutive_pindirs(PIO_p1, sm, beginVGA_PIN, 8, true);//конфигурация пинов на выход
    //pio_sm_config c = pio_vga_program_get_default_config(offset); 

    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + pio_program1_wrap_target, offset + pio_program1_wrap);


    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);//увеличение буфера TX за счёт RX до 8-ми 
    sm_config_set_out_shift(&c, true, true, 8);
    sm_config_set_out_pins(&c, beginVGA_PIN, 8);
    pio_sm_init(PIO_p1, sm, offset, &c);

    pio_sm_set_enabled(PIO_p1, sm, true);


    float fdiv=clock_get_hz(clk_sys)/25175000.0;//частота VGA по умолчанию
    uint32_t div32=(uint32_t) (fdiv * (1 << 16));
    PIO_p1->sm[sm].clkdiv=div32&0xfffff000; //делитель для конкретной sm
    PIO_p1->txf[sm]=SM_out;//выбор подпрограммы по номеру SM
    //инициализация DMA

    
    dma_chan_ctrl = dma_claim_unused_channel(true);
    int dma_chan = dma_claim_unused_channel(true);
    //основной ДМА канал для данных
    dma_channel_config c0 = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&c0, DMA_SIZE_8);

    channel_config_set_read_increment(&c0, true);
    channel_config_set_write_increment(&c0, false);

    uint dreq=DREQ_PIO1_TX0+sm;
    if (PIO_p1==pio0) dreq=DREQ_PIO0_TX0+sm;

    channel_config_set_dreq(&c0, dreq);
    channel_config_set_chain_to(&c0, dma_chan_ctrl);                        // chain to other channel
   
    dma_channel_configure(
        dma_chan,
        &c0,
        &PIO_p1->txf[sm], // Write address 
        &DMA_BUF[0][0],             // read address 
        SIZE_DMA_BUF, //
        false             // Don't start yet
    );
    //канал DMA для контроля основного канала
    dma_channel_config c1 = dma_channel_get_default_config(dma_chan_ctrl);
    channel_config_set_transfer_data_size(&c1, DMA_SIZE_32);
    
    channel_config_set_read_increment(&c1, false);
    channel_config_set_write_increment(&c1, false);
    channel_config_set_chain_to(&c1, dma_chan);                         // chain to other channel
    //channel_config_set_dreq(&c1, DREQ_PIO0_TX0);

   


    DMA_BUF_ADDR[0]=&DMA_BUF[0][0];
    DMA_BUF_ADDR[1]=&DMA_BUF[1][0];
    DMA_BUF_ADDR[2]=&DMA_BUF[2][0];
    DMA_BUF_ADDR[3]=&DMA_BUF[3][0];

    dma_channel_configure(
        dma_chan_ctrl,
        &c1,
        &dma_hw->ch[dma_chan].read_addr, // Write address 
        &DMA_BUF_ADDR[0],             // read address
        1, // 
        false             // Don't start yet
    );
    dma_channel_set_read_addr(dma_chan, &DMA_BUF_ADDR[0], false);

    dma_channel_set_irq0_enabled(dma_chan_ctrl, true);

  

    systick_hw->csr = 0x5;
    systick_hw->rvr = 0xFFFFFFFF;
    //test
    for(int i=0;i<2400;i++) dma_handler_VGA();
    g_delay_ms(100);

    
    uint32_t t0=(uint32_t)systick_hw->cvr;
    uint32_t sum_t=0;
    for(int i=0;i<100;i++)
    {
        t0=(uint32_t)systick_hw->cvr;
        dma_handler_VGA();
        uint32_t t1=(uint32_t)systick_hw->cvr;
        G_PRINTF("line=%d ,time str %d\n",line_active,t0-t1);
        sum_t+=t0-t1;
    }
        G_PRINTF("time str SR= %d\n",sum_t/(100*N_DMA_BUF));
    g_delay_ms(100);
    
    
    memset64(v_buf,0x77777777fffff4cf,4800);
    
    for(int y=0;y<240;y++)
        for(int x=0;x<320;x++)
            {
                if (x==y)
                    v_buf[y*160+x/2]&=x&1?0x0f:0xf0;
            }    
    //end test

     // Configure the processor to run dma_handler() when DMA IRQ 0 is asserted
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler_VGA);
    irq_set_enabled(DMA_IRQ_0, true);
    dma_start_channel_mask((1u << dma_chan_ctrl)) ;


    G_PRINTF_INFO("init VGA\n");
};