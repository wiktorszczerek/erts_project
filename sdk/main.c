#include "xparameters.h"
#include "xgpio.h"
#include "sd_card.h"
#include "dma.h"
#include "xil_cache.h"
#include "xscutimer.h"

#define ONE_SECOND 325000000

int main()
{
//    Xil_DCacheEnable();

	xil_printf("== PROGRAM START ==\r\n");

	int status, val;

	// TIMER
	XScuTimer Timer;
	XScuTimer_Config *ConfigPtr;
	XScuTimer *TimerInstancePtr = &Timer;
	ConfigPtr = XScuTimer_LookupConfig (XPAR_PS7_SCUTIMER_0_DEVICE_ID);
	status = XScuTimer_CfgInitialize (TimerInstancePtr, ConfigPtr, ConfigPtr->BaseAddr);
	if(status != XST_SUCCESS)
	{
		xil_printf("Timer init() failed\r\n");
	}
	u32 timer_start, timer_stop;

	// BMP
	struct pixel IMG[IMG_WIDTH * IMG_HEIGHT];
	struct bmp_header bmp_head;

	// GPIO
	XGpio led, sw;
	XGpio_Initialize(&led, XPAR_AXI_GPIO_0_DEVICE_ID);
	XGpio_SetDataDirection(&led, 1, 0x0);
	XGpio_Initialize(&sw, XPAR_AXI_GPIO_1_DEVICE_ID);
	XGpio_SetDataDirection(&sw, 1, 0xffffffff);


	// SD - Read lenka
	sd_mount(0);

	sd_open("lenka.bmp", FA_OPEN_EXISTING | FA_READ);
	sd_read_bmp_header(&bmp_head);
	sd_read_bmp_data(&bmp_head, IMG);
	sd_close();

	xil_printf("DEBUG || IMG TEST | BGR: %u %u %u\r\n", IMG[0].blue, IMG[0].green, IMG[0].red);
	xil_printf("DEBUG || IMG TEST | BGR: %u %u %u\r\n", IMG[1].blue, IMG[1].green, IMG[1].red);
	xil_printf("DEBUG || IMG TEST | BGR: %u %u %u\r\n", IMG[32].blue, IMG[32].green, IMG[32].red);


	// SD - file for new IMG
	xil_printf("DEBUG || Opening BMP for saving... \r\n");
	if(sd_open("test.bmp", FA_CREATE_NEW | FA_WRITE))
	{
		xil_printf("ERROR || Can't open BMP file\r\n");
	}
	else
		xil_printf("DEBUG || BMP opened.\r\n");
	sd_write_bmp_header(&bmp_head);


	// DMA setup
	dma_init_simple();

	u8* TxBufferPtr;
	u8* RxBufferPtr;

	TxBufferPtr = (u8*)TX_BUFFER_BASE;
	RxBufferPtr = (u8*)RX_BUFFER_BASE;

	uint16_t tx_index = 0;

	// SENDING ENTIRE IMAGE!
	xil_printf("DEBUG || DMA data\r\n");
	XScuTimer_RestartTimer(&Timer);

	uint16_t img_index = 0;
	for(uint32_t j=0; j<128*4; ++j)
	{
		tx_index = 0;
		for(uint8_t i=0; i<32; ++i) // 32pix every iteration of j (128B)
		{
			TxBufferPtr[tx_index] = IMG[img_index].blue;
			TxBufferPtr[tx_index+1] = IMG[img_index].green;
			TxBufferPtr[tx_index+2] = IMG[img_index].red;
			TxBufferPtr[tx_index+3] = 0x00;
			tx_index += 4;
			img_index++;
		}
		Xil_DCacheFlushRange((UINTPTR)TxBufferPtr, 128);
//		xil_printf("DEBUG || DMA sending part %d...\r\n",j);
		status = dma_send_simple(TxBufferPtr, 128);
	}

	timer_start = XScuTimer_GetCounterValue(&Timer);
	XScuTimer_Start(&Timer);

//	xil_printf("DEBUG || DMA sent!\r\n");
//	xil_printf("DEBUG || DMA receiving...\r\n");


	for(uint32_t j=0; j<128*4; ++j)
	{
//		xil_printf("DEBUG || DMA receiving part %d\r\n", j);
		status = dma_recv_simple(RxBufferPtr, 128);
		for(uint8_t i=3; i<128; i=i+4)
		{
			RxBufferPtr[i] = 0xFF;
		}
//		xil_printf("DEBUG || DMA received! Saving packet to BMP\r\n");
		sd_write_bmp_packet(RxBufferPtr, 128);
	}
	XScuTimer_Stop(&Timer);
	timer_stop = XScuTimer_GetCounterValue(&Timer);
	xil_printf("DEBUG || TIMER: Execution time is approx: %lu", timer_stop - timer_start);
	xil_printf("DEBUG || DMA received!\r\n");


	// SD - closing file for new IMG
	xil_printf("DEBUG || Closing BMP\r\n");
	sd_close();
	xil_printf("DEBUG || BMP saved\r\n");

	// Entering placeholder loop
	xil_printf("DEBUG || Entering loop \r\n");
	while(1)
	{
		val = XGpio_DiscreteRead(&sw, 1);
		XGpio_WriteReg(XPAR_AXI_GPIO_0_BASEADDR, 0, val);
		for(int i=0; i<1000000; ++i){};
	}
}


// THIS IS GOOD.
// sending 128B or 32 pixels (32*4B)

// Line buffer (sort of) = 256pix = 256*4B = 1024B

// whole img 16384pix -> 16384/256 = 64 Line buffers

//	uint16_t img_index = 0;
//	for(uint8_t k=0; k<64; ++k) // 64 line buffers of 256pix -> so 1024 in k indexes
//	{
//		for(uint8_t j=0; j<8; ++j) // 8 packets of 32pix every iteration of k (8*32pix = 256pix or 1024B)
//		{
//			tx_index = 0;
//			for(uint8_t i=0; i<32; ++i) // 32pix every iteration of j (128B)
//			{
//				TxBufferPtr[tx_index] = IMG[img_index].blue;
//				TxBufferPtr[tx_index+1] = IMG[img_index].green;
//				TxBufferPtr[tx_index+2] = IMG[img_index].red;
//				TxBufferPtr[tx_index+3] = 0x00;
//				tx_index += 4;
//				img_index++;
//			}
//			Xil_DCacheFlushRange((UINTPTR)TxBufferPtr, 128);
//			xil_printf("DEBUG || DMA sending part %d...\r\n",j);
//			status = dma_send_simple(TxBufferPtr, 128);
//			xil_printf("DEBUG || DMA sent!\r\n");
//		}
//
//		for(uint8_t j=0; j<8; ++j)
//		{
//			xil_printf("DEBUG || DMA receiving part %d\r\n", j);
//			status = dma_recv_simple(RxBufferPtr, 128);
//			xil_printf("DEBUG || DMA received! Saving packet to BMP\r\n");
//			for(uint8_t i=3; i<128; i=i+4)
//			{
//				RxBufferPtr[i] = 0xFF;
//			}
//			sd_write_bmp_packet(RxBufferPtr, 128);
//		}
//	}
















//	XMy_test my_test;
//	XMy_test_Config* my_test_cfg;

//	my_test_cfg = XMy_test_LookupConfig(XPAR_MY_TEST_0_DEVICE_ID);
//	if(!my_test_cfg)
//		xil_printf("ERROR || Cannot load config for my_test\r\n");
//	status = XMy_test_CfgInitialize(&my_test, my_test_cfg);
//	if(status != XST_SUCCESS)
//		xil_printf("ERROR || Cannot initialize my_test\r\n");
//	XMy_test_Initialize(&my_test, XPAR_MY_TEST_0_DEVICE_ID);
//	XMy_test_EnableAutoRestart(&my_test);
//	XMy_test_Start(&my_test);


//dma_init();
//xil_printf("SENDING PACKET\r\n");
//u32* Packet = (u32 *) TX_BUFFER_BASE;
//u8* TxPacket = (u8 *) Packet;
//
//u8 Value = 0xC;
//for(int i = 0; i < 0x20; i ++) {
//	TxPacket[i] = Value;
//
//	Value = (Value + 1) & 0xFF;
//}
//
////	for(int j=0; j<64; j++)
////	{
//	err = dma_send_packet(TxPacket);
//	if(err != XST_SUCCESS)
//		xil_printf("ERROR || DMA SEND: %d\r\n", err);
////	}
//
//xil_printf("CHECKING PACKET\r\n");
//err = dma_check_result();
//if(err != XST_SUCCESS)
//	xil_printf("ERROR || DMA CHECK: %d\r\n", err);


//sd_open("test.txt", FA_CREATE_ALWAYS | FA_WRITE);
//sd_write((void*)writeText, sizeof(writeText), 0);
//sd_close();

/*
 * canny setup
 */
//	canny_cfg = XCanny_edge_detection_LookupConfig(XPAR_CANNY_EDGE_DETECTION_0_DEVICE_ID);
//	if(!canny_cfg)
//		xil_printf("ERROR || Cannot load config for Canny.\r\n");
//	status = XCanny_edge_detection_CfgInitialize(&canny, canny_cfg);
//	if(status != XST_SUCCESS)
//		xil_printf("ERROR || Cannot initialize Canny.\r\n");
//	XCanny_edge_detection_Initialize(&canny, XPAR_CANNY_EDGE_DETECTION_0_DEVICE_ID);
//
//	XCanny_edge_detection_Set_hist_lthr(&canny, 20);
//	XCanny_edge_detection_Set_hist_hthr(&canny, 80);
//
//	xil_printf("THRESHOLDS: LOW: %u, HIGH: %u\r\n", XCanny_edge_detection_Get_hist_lthr(&canny),
//			XCanny_edge_detection_Get_hist_hthr(&canny));
