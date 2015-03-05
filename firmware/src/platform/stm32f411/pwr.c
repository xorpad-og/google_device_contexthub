#include <plat/inc/pwr.h>
#include <stddef.h>

struct StmRcc {
    volatile uint32_t CR; 
    volatile uint32_t PLLCFGR;
    volatile uint32_t CFGR;
    volatile uint32_t CIR; 
    volatile uint32_t AHB1RSTR;
    volatile uint32_t AHB2RSTR;
    volatile uint32_t AHB3RSTR;
    uint8_t unused0[4];
    volatile uint32_t APB1RSTR;   
    volatile uint32_t APB2RSTR;
    uint8_t unused1[8]; 
    volatile uint32_t AHB1ENR;    
    volatile uint32_t AHB2ENR;    
    volatile uint32_t AHB3ENR;
    uint8_t unused2[4];
    volatile uint32_t APB1ENR;    
    volatile uint32_t APB2ENR;
    uint8_t unused3[8]; 
    volatile uint32_t AHB1LPENR;  
    volatile uint32_t AHB2LPENR;
    volatile uint32_t AHB3LPENR;
    uint8_t unused4[4];
    volatile uint32_t APB1LPENR;
    volatile uint32_t APB2LPENR;
    uint8_t unused5[8]; 
    volatile uint32_t BDCR;
    volatile uint32_t CSR;
    uint8_t unused6[8]; 
    volatile uint32_t SSCGR; 
    volatile uint32_t PLLI2SCFGR;
};

#define RCC ((struct StmRcc*)RCC_BASE)

struct StmPwr {
    volatile uint32_t CR;
    volatile uint32_t CSR;
};

#define PWR ((struct StmPwr*)PWR_BASE)

/* RCC bit definitions */
#define  RCC_BDCR_LSEON 0x00000001UL
#define RCC_BDCR_LSERDY 0x00000002UL
#define RCC_BDCR_RTCSEL_LSE 0x00000100UL
#define RCC_BDCR_RTCEN 0x00008000UL
#define RCC_BDCR_BDRST 0x00010000UL

/* PWR bit definitions */
#define PWR_CR_DBP 0x00000100UL

static uint32_t gSysClk = 16000000UL;

#define RCC_REG(_bus, _type) ({                                 \
        static const uint32_t clockRegOfsts[] = {               \
            offsetof(struct StmRcc, AHB1##_type),               \
            offsetof(struct StmRcc, AHB2##_type),               \
            offsetof(struct StmRcc, AHB3##_type),               \
            offsetof(struct StmRcc, APB1##_type),               \
            offsetof(struct StmRcc, APB2##_type)                \
        }; /* indexed by PERIPH_BUS_* */                        \
        (volatile uint32_t *)(RCC_BASE + clockRegOfsts[_bus]);  \
    })                                                          \

void pwrUnitClock(uint32_t bus, uint32_t unit, bool on)
{
    volatile uint32_t *reg = RCC_REG(bus, ENR);

    if (on)
        *reg |= unit;
    else
        *reg &=~ unit;
}

void pwrUnitReset(uint32_t bus, uint32_t unit, bool on)
{
    volatile uint32_t *reg = RCC_REG(bus, RSTR);

    if (on)
        *reg |= unit;
    else
        *reg &=~ unit;
}

uint32_t pwrGetBusSpeed(uint32_t bus)
{
    uint32_t cfg = RCC->CFGR;
    uint32_t ahbDiv, apb1Div, apb2Div;
    uint32_t ahbSpeed, apb1Speed, apb2Speed;
    static const uint8_t ahbSpeedShifts[] = {1, 2, 3, 4, 6, 7, 8, 9};

    ahbDiv = (cfg >> 4) & 0x0F;
    apb1Div = (cfg >> 10) & 0x07;
    apb2Div = (cfg >> 13) & 0x07;

    ahbSpeed = (ahbDiv & 0x08) ? (gSysClk >> ahbSpeedShifts[ahbDiv & 0x07]) : gSysClk;
    apb1Speed = (apb1Div & 0x04) ? (ahbSpeed >> ((apb1Div & 0x03) + 1)) : ahbSpeed;
    apb2Speed = (apb2Div & 0x04) ? (ahbSpeed >> ((apb2Div & 0x03) + 1)) : ahbSpeed;

    if (bus == PERIPH_BUS_AHB1 || bus == PERIPH_BUS_AHB2 || bus == PERIPH_BUS_AHB3)
        return ahbSpeed;

    if (bus == PERIPH_BUS_APB1)
        return apb1Speed;

    if (bus == PERIPH_BUS_APB2)
        return apb2Speed;

    /* WTF...? */
    return 0;
}

void pwrEnableAndClockRtc(void)
{
    /* Enable power clock */
    RCC->APB1ENR |= PERIPH_APB1_PWR;
    /* Reset backup domain */
    RCC->BDCR |= RCC_BDCR_BDRST;
    /* Exit reset of backup domain */
    RCC->BDCR &= ~RCC_BDCR_BDRST;


    /* Enable write permission for backup domain */
    pwrEnableWriteBackupDomainRegs();
    /* Prevent compiler reordering across this boundary. */
    asm volatile("":::"memory");
    /* Set LSE as backup domain clock source */
    RCC->BDCR |= RCC_BDCR_LSEON;
    /* Wait for LSE to be ready */
    while ((RCC->BDCR & RCC_BDCR_LSERDY) == 0);
    /* Set LSE as RTC clock source */
    RCC->BDCR |= RCC_BDCR_RTCSEL_LSE;
    /* Enable RTC */
    RCC->BDCR |= RCC_BDCR_RTCEN;
}

void pwrEnableWriteBackupDomainRegs(void)
{
    PWR->CR |= PWR_CR_DBP;
}

void pwrSystemInit(void)
{
    RCC->CR |= 1;                             //HSI on
    while (!(RCC->CR & 2));                    //wait for HSI
    RCC->CFGR = 0x00000000;                   //all busses at HSI speed
    RCC->CR &= 0x0000FFF1;                    //HSI on, all else off
}
