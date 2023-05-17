#include "main.h"

#define DEBUG_ON

//#define DEBUG_UART0
#define DEBUG_UART5

// cbs: config[0] 7:6
// icelock: config[0] 12
// lock (flash) config[0] 1

// 0xffffff3f	cbs
// 0xffffff3d	cbs + lock
// 0xffffef3f	cbs + icelock
// 0xffffef3d	cbs + icelock + lock

//const uint32_t user_config[3] __attribute__ ((section (".bootm03x_userconfig"))) = { 0xffffff3f, 0xffffffff, 0xffffffff };

volatile const uint32_t __attribute__ ((section (".bootm03x_version"))) boot_version = BOOTLOADER_VERSION;

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

//static void SYS_Init(void) {
//
//    /* Unlock protected registers */
//    SYS_UnlockReg();
//
//    /* Enable HIRC clock */
//    CLK->PWRCTL |= CLK_PWRCTL_HIRCEN_Msk;
//
//    /* Waiting for HIRC clock ready */
//    while((CLK->STATUS & CLK_STATUS_HIRCSTB_Msk) != CLK_STATUS_HIRCSTB_Msk);
//
//    /* Switch HCLK clock source to HIRC */
//    CLK->CLKSEL0 = (CLK->CLKSEL0 & ~CLK_CLKSEL0_HCLKSEL_Msk ) | CLK_CLKSEL0_HCLKSEL_HIRC ;
//
//    /* Enable UART0 clock */
//    /* Switch UART0 clock source to HIRC */
//#if defined DEBUG_UART0
//    CLK->APBCLK0 |= CLK_APBCLK0_UART0CKEN_Msk ;
//    CLK->CLKSEL1 = (CLK->CLKSEL1 & ~CLK_CLKSEL1_UART0SEL_Msk) | CLK_CLKSEL1_UART0SEL_HIRC;
//#elif defined DEBUG_UART5
//    CLK->APBCLK0 |= CLK_APBCLK0_UART5CKEN_Msk ;
//    CLK->CLKSEL1 = (CLK->CLKSEL1 & ~CLK_CLKSEL3_UART5SEL_Msk) | CLK_CLKSEL3_UART5SEL_HIRC;
//#endif
//
//    /* Update System Core Clock */
//    SystemCoreClockUpdate();
//
//#if defined DEBUG_UART0
//    /* Set PB multi-function pins for UART0 RXD=PB.12 and TXD=PB.13 */
//    SYS->GPB_MFPH = (SYS->GPB_MFPH & ~(SYS_GPB_MFPH_PB12MFP_Msk | SYS_GPB_MFPH_PB13MFP_Msk))
//                    |(SYS_GPB_MFPH_PB12MFP_UART0_RXD | SYS_GPB_MFPH_PB13MFP_UART0_TXD);
//
//    UART0->BAUD = UART_BAUD_MODE2 | UART_BAUD_MODE2_DIVIDER(__HIRC, 921600);
//	UART0->LINE = UART_WORD_LEN_8 | UART_PARITY_NONE | UART_STOP_BIT_1;
//#elif defined DEBUG_UART5
//    SYS->GPB_MFPL &= ~(SYS_GPB_MFPL_PB5MFP_Msk | SYS_GPB_MFPL_PB4MFP_Msk);
//    SYS->GPB_MFPL |= (SYS_GPB_MFPL_PB5MFP_UART5_TXD | SYS_GPB_MFPL_PB4MFP_UART5_RXD);
//
//    UART5->BAUD = UART_BAUD_MODE2 | UART_BAUD_MODE2_DIVIDER(__HIRC, 921600);
//    UART5->LINE = UART_WORD_LEN_8 | UART_PARITY_NONE | UART_STOP_BIT_1;
//#endif
//
//    /* Lock protected registers */
//    SYS_LockReg();
//}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

static void SendChar_ToUART(int ch) {
#ifdef DEBUG_ON
#if defined DEBUG_UART0
    while (UART0->FIFOSTS & UART_FIFOSTS_TXFULL_Msk);
    	UART0->DAT = ch;
#elif defined DEBUG_UART5
    while (UART5->FIFOSTS & UART_FIFOSTS_TXFULL_Msk);
        UART5->DAT = ch;
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
#endif
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

static void bin2hex32(uint8_t *bin) {
	char hex[10];
	char *phex = &hex[7];
	uint8_t temp;
	size_t i=4;

	while(i--) {
		temp = *bin & 0x0f;
		if (temp < 10)
			temp += 0x30;
		else
			temp += 0x37;
		*phex-- = temp;

		temp = (*bin++ & 0xf0) >> 4;
		if (temp < 10)
			temp += 0x30;
		else
			temp += 0x37;
		*phex-- = temp;
	}
	hex[8] = ' ';
	hex[9] = '\0';
	PutString(hex);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

static uint32_t crc32_hw(uint32_t address, uint32_t len) {

	FMC->ISPCMD  = FMC_ISPCMD_RUN_CKS;
	FMC->ISPADDR = address;
	FMC->ISPDAT  = len;
	FMC->ISPTRG  = FMC_ISPTRG_ISPGO_Msk;

	while (FMC->ISPSTS & FMC_ISPSTS_ISPBUSY_Msk) { }

	FMC->ISPCMD = FMC_ISPCMD_READ_CKS;
	FMC->ISPADDR  = FMC_APROM_BASE;
	FMC->ISPTRG = FMC_ISPTRG_ISPGO_Msk;

	while (FMC->ISPSTS & FMC_ISPSTS_ISPBUSY_Msk) { }

	return(FMC->ISPDAT);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

void main() {

	uint32_t FW_LEN;
	uint32_t MEM_LEN;
	uint32_t checksum_read;
	uint32_t checksum_calc;
	uint32_t flash_address_src;
	uint32_t flash_address_dst;
	uint32_t flash_data;
	uint32_t count;
	uint32_t config_data;
	uint8_t bootloader_version_str[5] = {
			0,//(BOOTLOADER_VERSION >> 8) + 0x30,
			'.',
			0,//(BOOTLOADER_VERSION & 0xff) + 0x30,
			' ',
			0
	};

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// init

	/* Unlock protected registers */
	SYS_UnlockReg();

	/* Enable HIRC clock */
	CLK->PWRCTL |= CLK_PWRCTL_HIRCEN_Msk;

	/* Waiting for HIRC clock ready */
	while((CLK->STATUS & CLK_STATUS_HIRCSTB_Msk) != CLK_STATUS_HIRCSTB_Msk);

	/* Switch HCLK clock source to HIRC */
	CLK->CLKSEL0 = (CLK->CLKSEL0 & ~CLK_CLKSEL0_HCLKSEL_Msk ) | CLK_CLKSEL0_HCLKSEL_HIRC ;

	CLK->PCLKDIV = (CLK_PCLKDIV_APB0DIV_DIV1 | CLK_PCLKDIV_APB1DIV_DIV1);

	/* Enable UART0 clock */
	CLK->APBCLK0 |= CLK_APBCLK0_UART5CKEN_Msk ;

	/* Switch UART0 clock source to HIRC */
	CLK->CLKSEL3 = (CLK->CLKSEL3 & ~CLK_CLKSEL3_UART5SEL_Msk) | CLK_CLKSEL3_UART5SEL_HIRC;

	/* Update System Core Clock */
	SystemCoreClockUpdate();


	/* Set PB multi-function pins for UART0 RXD=PB.12 and TXD=PB.13 */
	SYS->GPA_MFPL = SYS_GPA_MFPL_PA5MFP_UART5_TXD | SYS_GPA_MFPL_PA4MFP_UART5_RXD;

	FMC_ENABLE_CFG_UPDATE();
	FMC_ENABLE_AP_UPDATE();

	UART_Open(UART5, 921600);
	UART_SetLine_Config(UART5, 0, UART_WORD_LEN_8, UART_PARITY_NONE, UART_STOP_BIT_1);
	UART5->FIFO = (UART5->FIFO & ~ UART_FIFO_RFITL_Msk) | UART_FIFO_RFITL_1BYTE;

    FMC_Open();

    PutString("<< bootm03x v");
    bootloader_version_str[0] = (boot_version >> 8) + 0x30;
    bootloader_version_str[2] = (boot_version & 0xff) + 0x30;
    PutString(bootloader_version_str);

	/* Enable FMC ISP function */
	FMC->ISPCTL |=  FMC_ISPCTL_ISPEN_Msk;

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// get transfer size from address 0x218 (every fw must have value). it will only change
	// for different flash size (other mcu) or for same flash size but with data in flash

	MEM_LEN = FMC_Read(FLASH_TRANSFER_LEN_ADDRESS);
	bin2hex32((uint8_t *) &MEM_LEN);

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// check maximum possible value for transfer len, which is half of a 512k flash device.

	if (MEM_LEN <= 0x40000) {

		PutString("a ");

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		// get len of firmware available in the transfer region

		FW_LEN = FMC_Read(MEM_LEN + FLASH_FIRMWARE_LEN_ADDRESS);

		bin2hex32((uint8_t *) &FW_LEN);

		if (FW_LEN < (MEM_LEN - 4)) {

			PutString("b ");

			// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
			// get crc at the end of the transfer region

			checksum_read = FMC_Read(MEM_LEN + FW_LEN);
			bin2hex32((uint8_t *) &checksum_read);

			// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
			// calculate crc from flash data in transfer region

			checksum_calc = crc32_hw(MEM_LEN, FW_LEN);
			bin2hex32((uint8_t *) &checksum_calc);

			// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
			// compare read crc and calculated crc

			if (checksum_read == checksum_calc) {

				PutString("c ");

				// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
				// erase main firmware region

				flash_address_src = FMC_APROM_BASE;
				while (flash_address_src < (FMC_APROM_BASE + MEM_LEN)) {

					FMC->ISPCMD = FMC_ISPCMD_PAGE_ERASE;
					FMC->ISPADDR = flash_address_src;
					FMC->ISPTRG = FMC_ISPTRG_ISPGO_Msk;

					while (FMC->ISPTRG & FMC_ISPTRG_ISPGO_Msk) { }

					if (FMC->ISPCTL & FMC_ISPCTL_ISPFF_Msk)
						FMC->ISPCTL |= FMC_ISPCTL_ISPFF_Msk;

					flash_address_src += FMC_FLASH_PAGE_SIZE;
				}

				// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
				// transfer new firmware to main region

				flash_address_src = MEM_LEN;
				flash_address_dst = FMC_APROM_BASE;
				count = FW_LEN;
				while(count--) {
					flash_data = FMC_Read(flash_address_src);
					FMC_Write(flash_address_dst, flash_data);
					flash_address_src += 4;
					flash_address_dst += 4;
				}

				// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
				// check crc in the main region

				checksum_read = FMC_Read(FMC_APROM_BASE + FW_LEN);
				bin2hex32((uint8_t *) &checksum_read);

				checksum_calc = crc32_hw(FMC_APROM_BASE, FW_LEN);
				bin2hex32((uint8_t *) &checksum_calc);

				if (checksum_read == checksum_calc)
					PutString("ok ");
				else
					PutString("error ");
			}
		}
	}

	if (FMC_ReadConfig(&config_data, 1) >= 0) {

		PutString("d ");

		// set aprom with IAP
		config_data |= 0x00000080;	// 10000000
		config_data &= 0xffffffbf;	// 10111111

		if (FMC_WriteConfig(&config_data, 1) == 0) {
			PutString("e ");
			FMC_SetVectorPageAddr(FMC_APROM_BASE);
		}
	}

	FMC_Close();
	SYS_LockReg();

	PutString("reset >>\n");
	UART_WAIT_TX_EMPTY(UART5);

	NVIC_SystemReset();

    while (1);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
