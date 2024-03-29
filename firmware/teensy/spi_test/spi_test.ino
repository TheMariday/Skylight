/*
Adapted from tjaekel's code here: https://forum.pjrc.com/threads/72792-Teensy-4-1-SPI-Slave-here-it-is
*/

#define bufferSize 400*3*24

#include <cstdint>

uint32_t spiRx[bufferSize];
volatile int spiRxIdx;
volatile int spiRxComplete = 0;

#if !defined(_SPISlave_T4_H_)
#define _SPISlave_T4_H_

#include <Arduino.h>
#include <circular_buffer.h>
#include <SPI.h>

typedef enum SPI_BITS {
    SPI_8_BITS = 8,
    SPI_16_BITS = 16,
    SPI_32_BITS = 32,
} SPI_BITS;

typedef void (*_SPI_ptr)();

#define SPISlave_T4_CLASS template<SPIClass* port = nullptr, SPI_BITS bits = SPI_8_BITS>
#define SPISlave_T4_FUNC template<SPIClass* port, SPI_BITS bits>
#define SPISlave_T4_OPT SPISlave_T4<port, bits>

extern SPIClass SPI;

class SPISlave_T4_Base {
public:
    virtual void SLAVE_ISR();
};

static SPISlave_T4_Base* _LPSPI4 = nullptr;

SPISlave_T4_CLASS class SPISlave_T4 : public SPISlave_T4_Base {
public:
    SPISlave_T4();
    void begin();
    uint32_t transmitErrors();
    void onReceive(_SPI_ptr handler) { _spihandler = handler; }
    bool active();
    bool available();
    void sniffer(bool enable = 1);
    void swapPins(bool enable = 1);
    void pushr(uint32_t data);
    uint32_t popr();

private:
    _SPI_ptr _spihandler = nullptr;
    void SLAVE_ISR();
    int _portnum = 0;
    uint32_t nvic_irq = 0;
    uint32_t transmit_errors = 0;
    bool sniffer_enabled = 0;
};


#define SLAVE_CR spiAddr[4]
#define SLAVE_FCR spiAddr[22]
#define SLAVE_IER spiAddr[6]
#define SLAVE_CFGR0 spiAddr[8]
#define SLAVE_CFGR1 spiAddr[9]
#define SLAVE_TDR spiAddr[25]
#define SLAVE_RDR spiAddr[29]
#define SLAVE_SR spiAddr[5]
#define SLAVE_TCR_REFRESH spiAddr[24] = (0UL << 27) | LPSPI_TCR_FRAMESZ(bits - 1)
#define SLAVE_PORT_ADDR volatile uint32_t *spiAddr = &(*(volatile uint32_t*)(0x40394000 + (0x4000 * _portnum)))
#define SLAVE_PINS_ADDR volatile uint32_t *spiAddr = &(*(volatile uint32_t*)(0x401F84EC + (_portnum * 0x10)))


void lpspi4_slave_isr() {
    _LPSPI4->SLAVE_ISR();
}


SPISlave_T4_FUNC SPISlave_T4_OPT::SPISlave_T4() {
    if ( port == &SPI ) {
        _LPSPI4 = this;
        _portnum = 3;
        CCM_CCGR1 |= (3UL << 6);
        nvic_irq = 32 + _portnum;
        _VectorsRam[16 + nvic_irq] = lpspi4_slave_isr;

        /* Alternate pins not broken out on Teensy 4.0/4.1 for LPSPI4 */
        SLAVE_PINS_ADDR;
        spiAddr[0] = 0; /* PCS0_SELECT_INPUT */
        spiAddr[1] = 0; /* SCK_SELECT_INPUT */
        spiAddr[2] = 0; /* SDI_SELECT_INPUT */
        spiAddr[3] = 0; /* SDO_SELECT_INPUT */
        IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_03 = 0x3; /* LPSPI4 SCK (CLK) */
        IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_01 = 0x3; /* LPSPI4 SDI (MISO) */
        IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_02 = 0x3; /* LPSPI4 SDO (MOSI) */
        IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_00 = 0x3; /* LPSPI4 PCS0 (CS) */
    }
}


SPISlave_T4_FUNC void SPISlave_T4_OPT::swapPins(bool enable) {
    SLAVE_PORT_ADDR;
    SLAVE_CR &= ~LPSPI_CR_MEN; /* Disable Module */
    SLAVE_CFGR1 = (SLAVE_CFGR1 & 0xFCFFFFFF) | (enable) ? (3UL << 24) : (0UL << 24);
    SLAVE_CR |= LPSPI_CR_MEN; /* Enable Module */
    if ( sniffer_enabled ) sniffer();
}


SPISlave_T4_FUNC void SPISlave_T4_OPT::sniffer(bool enable) {
    SLAVE_PORT_ADDR;
    sniffer_enabled = enable;
    if ( port == &SPI ) {
        if ( sniffer_enabled ) {
            if ( SLAVE_CFGR1 & (3UL << 24) ) { /* if pins are swapped */
                IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_03 = 0x3; /* LPSPI4 SCK (CLK) */
                IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_01 = 0x0; /* LPSPI4 SDI (MISO) */
                IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_02 = 0x3; /* LPSPI4 SDO (MOSI) */
                IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_00 = 0x3; /* LPSPI4 PCS0 (CS) */
            }
            else {
                IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_03 = 0x3; /* LPSPI4 SCK (CLK) */
                IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_01 = 0x3; /* LPSPI4 SDI (MISO) */
                IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_02 = 0x0; /* LPSPI4 SDO (MOSI) */
                IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_00 = 0x3; /* LPSPI4 PCS0 (CS) */
            }
        }
        else {
            IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_03 = 0x3; /* LPSPI4 SCK (CLK) */
            IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_01 = 0x3; /* LPSPI4 SDI (MISO) */
            IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_02 = 0x3; /* LPSPI4 SDO (MOSI) */
            IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_00 = 0x3; /* LPSPI4 PCS0 (CS) */
        }
    }
}


SPISlave_T4_FUNC bool SPISlave_T4_OPT::active() {
    SLAVE_PORT_ADDR;
    return ( !(SLAVE_SR & (1UL << 9)) ) ? 1 : 0;
}


SPISlave_T4_FUNC bool SPISlave_T4_OPT::available() {
    SLAVE_PORT_ADDR;
    return ( (SLAVE_SR & (1UL << 8)) ) ? 1 : 0;
}


SPISlave_T4_FUNC void SPISlave_T4_OPT::pushr(uint32_t data) {
    SLAVE_PORT_ADDR;
    SLAVE_TDR = data;
}


SPISlave_T4_FUNC uint32_t SPISlave_T4_OPT::popr() {
    SLAVE_PORT_ADDR;
    uint32_t data = SLAVE_RDR;
    SLAVE_SR = (1UL << 8); /* Clear WCF */
    return data;
}

extern uint32_t spiRx[bufferSize];
extern volatile int spiRxIdx;

SPISlave_T4_FUNC void __attribute__((section(".fustrun"))) SPISlave_T4_OPT::SLAVE_ISR() {

    SLAVE_PORT_ADDR;

#if 0
    if ( _spihandler ) {
    _spihandler();
    SLAVE_SR = 0x3F00;
    asm volatile ("dsb");
    return;
  }
#endif

    if ( SLAVE_SR & (1UL << 11) ) {
        /* transmit error, clear flag, check cabling */
        SLAVE_SR = (1UL << 11);
        transmit_errors++;
    }
    if ( (SLAVE_SR & (1UL << 1)) ) {
        spiRx[spiRxIdx] = SLAVE_RDR;
        if (spiRxIdx < bufferSize) spiRxIdx++;
        SLAVE_SR = (1UL << 1);
    }

    if ( (SLAVE_SR & (1UL << 0)) ) {
        SLAVE_TDR = 0x34;
    }

    if ( (SLAVE_SR & (1UL << 9)) ) {
        spiRxComplete = 1;
    }

    SLAVE_SR = 0x3F00;
    asm volatile ("dsb");
}

SPISlave_T4_FUNC void SPISlave_T4_OPT::begin() {
    SLAVE_PORT_ADDR;
    SLAVE_CR = LPSPI_CR_RST; /* Reset Module */
    SLAVE_CR = 0; /* Disable Module */
    SLAVE_FCR = 0;    //x10001; /* 1x watermark for RX and TX */
    SLAVE_IER = 0x01 | 0x200; /* Interrupt enable bits */
    SLAVE_CFGR0 = 0;
    SLAVE_CFGR1 = 0;
    SLAVE_CR |= LPSPI_CR_MEN | LPSPI_CR_DBGEN; /* Enable Module, Debug Mode */
    SLAVE_SR = 0x3F00; /* Clear status register */
    SLAVE_TCR_REFRESH;
    SLAVE_TDR = 0x0; /* dummy data, must populate initial TX slot */
    NVIC_ENABLE_IRQ(nvic_irq);
    NVIC_SET_PRIORITY(nvic_irq, 1);
}

#endif





#include <OctoWS2811.h>


const int channelCount = 24;
const int channelSize = 400;
const int pins[] = {23,22,21,20,19,18,17,16,15,14, 41,40,39,38,37,36,35,34,33, 28,29,30,31,32};


DMAMEM int displayMemory[channelSize * channelCount * 3 / 4];
int drawingMemory[channelSize * channelCount * 3 / 4];

byte channelToPin[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}; // intialise them all to point to 0. this is how we disable some channels without having to re-do the buffer

OctoWS2811 leds(channelSize, displayMemory, drawingMemory, WS2811_GRB | WS2811_800kHz, channelCount, channelToPin);

SPISlave_T4<&SPI, SPI_8_BITS> mySPI;


void setup() {
    Serial.begin(115200);	//baudrate does not matter (is USB VCP anyway)
    mySPI.begin();
    leds.begin();
}


void loop() {

    if (spiRxComplete) {

      Serial.print("timestamp "); Serial.println(millis());
      
      Serial.print("Data recieved: "); Serial.println(spiRxIdx);

      if(spiRxIdx == channelCount)
      {
        bool update = false;
        for(int channelId=0; channelId<channelCount; channelId++)
        {
          if(spiRx[channelId] == 1 && channelToPin[channelId] == 0) 
          {
            update = true;
            channelToPin[channelId] = pins[channelId];
            Serial.print("Channel "); Serial.print(channelId); Serial.println(" enabled");
          }
          
          if(spiRx[channelId] == 0 && channelToPin[channelId] != 0) 
          {
            update = true;
            channelToPin[channelId] = 0;
            Serial.print("Channel "); Serial.print(channelId); Serial.println(" disabled");
          }

        }
        if(update)
        {
          leds.begin(channelSize, displayMemory, drawingMemory, WS2811_GRB | WS2811_800kHz, channelCount, channelToPin);
        }
      }

      if(spiRxIdx == channelCount * channelSize * 3)
      {
        for(int ledIndex=0; ledIndex<channelCount*channelSize; ledIndex++)
        {
           leds.setPixel(ledIndex, spiRx[ledIndex*3], spiRx[ledIndex*3 + 1], spiRx[ledIndex*3 + 2]);
        }
      }

      if(spiRxIdx == 1)
      {
        leds.show();
        Serial.println("Showing LEDS");
      }
        
      spiRxComplete = 0;
      spiRxIdx = 0;
    }
}

