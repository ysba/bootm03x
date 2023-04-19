//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

#include "main.h"

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

#define DEBUG_ON

//#define DEBUG_UART0
#define DEBUG_UART5


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

// cbs: config[0] 7:6
// icelock: config[0] 12
// lock (flash) config[0] 1
//const uint32_t user_config[3] __attribute__ ((section (".userconfig"))) = { 0xffffffff, 0xffffffff, 0xffffffff };		// cbs
volatile const uint32_t user_config[3] __attribute__ ((section (".userconfig"))) = { 0xffffff3d, 0xffffffff, 0xffffffff };		// cbs + lock

// 0xffffff3f	cbs
// 0xffffff3d	cbs + lock
// 0xffffef3f	cbs + icelock
// 0xffffef3d	cbs + icelock + lock

volatile const uint32_t __attribute__ ((section (".bootversion"))) boot_version = BOOTLOADER_VERSION;

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

static void SYS_Init(void) {

    /* Unlock protected registers */
    SYS_UnlockReg();

    /* Enable HIRC clock */
    CLK->PWRCTL |= CLK_PWRCTL_HIRCEN_Msk;

    /* Waiting for HIRC clock ready */
    while((CLK->STATUS & CLK_STATUS_HIRCSTB_Msk) != CLK_STATUS_HIRCSTB_Msk);

    /* Switch HCLK clock source to HIRC */
    CLK->CLKSEL0 = (CLK->CLKSEL0 & ~CLK_CLKSEL0_HCLKSEL_Msk ) | CLK_CLKSEL0_HCLKSEL_HIRC ;

    /* Enable UART0 clock */
    /* Switch UART0 clock source to HIRC */
#if defined DEBUG_UART0
    CLK->APBCLK0 |= CLK_APBCLK0_UART0CKEN_Msk ;
    CLK->CLKSEL1 = (CLK->CLKSEL1 & ~CLK_CLKSEL1_UART0SEL_Msk) | CLK_CLKSEL1_UART0SEL_HIRC;
#elif defined DEBUG_UART5
    CLK->APBCLK0 |= CLK_APBCLK0_UART5CKEN_Msk ;
    CLK->CLKSEL1 = (CLK->CLKSEL1 & ~CLK_CLKSEL3_UART5SEL_Msk) | CLK_CLKSEL3_UART5SEL_HIRC;
#endif

    /* Update System Core Clock */
    SystemCoreClockUpdate();

#if defined DEBUG_UART0
    /* Set PB multi-function pins for UART0 RXD=PB.12 and TXD=PB.13 */
    SYS->GPB_MFPH = (SYS->GPB_MFPH & ~(SYS_GPB_MFPH_PB12MFP_Msk | SYS_GPB_MFPH_PB13MFP_Msk))
                    |(SYS_GPB_MFPH_PB12MFP_UART0_RXD | SYS_GPB_MFPH_PB13MFP_UART0_TXD);

    UART0->BAUD = UART_BAUD_MODE2 | UART_BAUD_MODE2_DIVIDER(__HIRC, 921600);
	UART0->LINE = UART_WORD_LEN_8 | UART_PARITY_NONE | UART_STOP_BIT_1;
#elif defined DEBUG_UART5
    SYS->GPB_MFPL &= ~(SYS_GPB_MFPL_PB5MFP_Msk | SYS_GPB_MFPL_PB4MFP_Msk);
    SYS->GPB_MFPL |= (SYS_GPB_MFPL_PB5MFP_UART5_TXD | SYS_GPB_MFPL_PB4MFP_UART5_RXD);

    UART5->BAUD = UART_BAUD_MODE2 | UART_BAUD_MODE2_DIVIDER(__HIRC, 921600);
    UART5->LINE = UART_WORD_LEN_8 | UART_PARITY_NONE | UART_STOP_BIT_1;
#endif

    /* Lock protected registers */
    SYS_LockReg();
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

/*
 * @returns     Send value from UART debug port
 * @details     Send a target char to UART debug port .
 */
static void SendChar_ToUART(int ch) {
#ifdef DEBUG_ON
#if defined DEBUG_UART0
    while (UART0->FIFOSTS & UART_FIFOSTS_TXFULL_Msk);
    	UART0->DAT = ch;
//    if(ch == '\n') {
//        while (UART0->FIFOSTS & UART_FIFOSTS_TXFULL_Msk);
//        UART0->DAT = '\r';
//    }
#elif defined DEBUG_UART5
    while (UART5->FIFOSTS & UART_FIFOSTS_TXFULL_Msk);
        UART5->DAT = ch;
//        if(ch == '\n') {
//            while (UART5->FIFOSTS & UART_FIFOSTS_TXFULL_Msk);
//            UART5->DAT = '\r';
//        }
#endif
#endif
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

static void PutString(char *str) {
#ifdef DEBUG_ON
    while (*str != '\0') {
        SendChar_ToUART(*str++);
    }
    //SendChar_ToUART(' ');
#endif
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

int main() {

	uint32_t err;
	uint32_t firmware_len;
	uint32_t checksum_read;
	uint32_t checksum_calc;
	uint32_t flash_address_src;
	uint32_t flash_address_dst;
	uint32_t flash_data;
	uint32_t count;
	uint32_t  config_data[2];
	//uint8_t flash_buf[]

	SYS_Init();

    if (user_config[0] == 0xffffffff)
    	PutString("boot a1 ");
    else
    	PutString("boot a0 ");

    if (boot_version == 0xffffffff)
		PutString("boot av1 ");
	else
		PutString("boot av0 ");

    SYS_UnlockReg();
    FMC_Open();

	/* Enable FMC ISP function */
	//FMC->ISPCTL |=  FMC_ISPCTL_ISPEN_Msk;

	if (FMC_CheckAllOne(BOOT_TRANSFER_BASE, BOOT_FIRMWARE_LEN) != 0) {

		PutString("b0 ");

		firmware_len = FMC_Read(BOOT_TRANSFER_BASE + FLASH_FIRMWARE_LEN);

		if (firmware_len > (BOOT_FIRMWARE_LEN - 4)) {
			firmware_len = BOOT_FIRMWARE_LEN - 4;
		}

		checksum_read = FMC_Read(BOOT_TRANSFER_BASE + firmware_len);
		checksum_calc = FMC_GetChkSum(BOOT_TRANSFER_BASE, firmware_len);

		if (checksum_read == checksum_calc) {
			PutString("c0 ");

			// erase flash
//			flash_address_src = FMC_APROM_BASE;
//			while (flash_address_src < (FMC_APROM_BASE + BOOT_FIRMWARE_LEN)) {
//				err = FMC_Erase(flash_address_src);
//				if (err != 0)
//					break;
//				flash_address_src += FMC_FLASH_PAGE_SIZE;
//			}

			err = FMC_Erase_Bank(FMC_APROM_BASE);

			if (err == 0) {
				// flash erase successfull
				PutString("d0 ");
				flash_address_src = BOOT_TRANSFER_BASE;
				flash_address_dst = FMC_APROM_BASE;
				count = firmware_len;
				while(count--) {
					flash_data = FMC_Read(flash_address_src);
					FMC_Write(flash_address_dst, flash_data);
					flash_address_src += 4;
					flash_address_dst += 4;
				}

				checksum_read = FMC_Read(FMC_APROM_BASE + firmware_len);
				checksum_calc = FMC_GetChkSum(FMC_APROM_BASE, firmware_len);

				if (checksum_read == checksum_calc) {
					PutString("e0 ");

					// set boot source to aprom

					if (FMC_ReadConfig(config_data, 2) >= 0) {

						PutString("f0 ");

						// set aprom with IAP
						config_data[0] |= 0x80;
						config_data[0] &= ~0x40;

						if (FMC_WriteConfig(config_data, 2) == 0) {
							PutString("g0 ");
							NVIC_SystemReset();
						}
						else {
							PutString("g1 ");
						}
					}
					else {

						PutString("f1 ");
					}

				}
				else {
					PutString("e1 ");
				}
			}
			else {
				PutString("d1 ");
			}
		}
		else {
			PutString("c1 ");
		}
	}
	else {

		PutString("b1 ");
	}

	FMC_Close();
	SYS_LockReg();

	PutString("end\n");

    while (1);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
