#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "hardware/sync.h"
#include "hardware/gpio.h"
#include "hardware/vreg.h"

#include "dvi.h"
#include "dvi_serialiser.h"
#include "common_dvi_pin_configs.h"
#include "tmds_encode.h"


#include <stdio.h>
#include <stdarg.h>

#include "GUI_Paint.h"

// DVDD 1.2V (1.1V seems ok too)
#define FRAME_WIDTH 640
#define FRAME_HEIGHT 480
#define VREG_VSEL VREG_VOLTAGE_1_10
#define DVI_TIMING dvi_timing_640x480p_60hz

// RGB111 bitplaned framebuffer
#define PLANE_SIZE_BYTES (FRAME_WIDTH * FRAME_HEIGHT / 8)
#define Scale3_RED 0x4
#define Scale3_GREEN 0x2
#define Scale3_BLUE 0x1
#define Scale3_BLACK 0x0
#define Scale3_WHITE 0x7
uint8_t framebuf[3 * PLANE_SIZE_BYTES];


struct dvi_inst dvi0;

void core1_main()
{
	dvi_register_irqs_this_core(&dvi0, DMA_IRQ_0);
	dvi_start(&dvi0);
	while (true)
	{
		for (uint y = 0; y < FRAME_HEIGHT; ++y)
		{
			uint32_t *tmdsbuf=0;
			queue_remove_blocking_u32(&dvi0.q_tmds_free, &tmdsbuf);
			for (uint component = 0; component < 3; ++component)
			{
				tmds_encode_1bpp(
					(const uint32_t *)&framebuf[y * FRAME_WIDTH / 8 + component * PLANE_SIZE_BYTES],
					tmdsbuf + component * FRAME_WIDTH / DVI_SYMBOLS_PER_WORD,
					FRAME_WIDTH);
			}
			queue_add_blocking_u32(&dvi0.q_tmds_valid, &tmdsbuf);
		}
	}
}

int main()
{
	vreg_set_voltage(VREG_VSEL);
	sleep_ms(10);
	set_sys_clock_khz(DVI_TIMING.bit_clk_khz, true);

	setup_default_uart();

	dvi0.timing = &DVI_TIMING;
	dvi0.ser_cfg = DVI_DEFAULT_SERIAL_CONFIG;
	dvi_init(&dvi0, next_striped_spin_lock_num(), next_striped_spin_lock_num());
	Paint_NewImage(framebuf, FRAME_WIDTH, FRAME_HEIGHT, 0, Scale3_WHITE);
	Paint_SetScale(3);
	multicore_launch_core1(core1_main);
	Paint_Clear(Scale3_BLACK);
	while (1)
	{
		Paint_DrawRectangle(100,100,108,110,Scale3_RED,1,DRAW_FILL_FULL);
		Paint_DrawRectangle(108,100,116,110,Scale3_GREEN,1,DRAW_FILL_FULL);
		Paint_DrawRectangle(100,110,108,120,Scale3_GREEN | Scale3_RED,1,DRAW_FILL_FULL);
		Paint_DrawRectangle(108,110,116,120,Scale3_WHITE,1,DRAW_FILL_FULL);
		sleep_ms(500);
		Paint_DrawRectangle(100,100,108,110,Scale3_BLACK,1,DRAW_FILL_FULL);
		Paint_DrawRectangle(108,100,116,110,Scale3_GREEN,1,DRAW_FILL_FULL);
		Paint_DrawRectangle(100,110,108,120,Scale3_GREEN | Scale3_RED,1,DRAW_FILL_FULL);
		Paint_DrawRectangle(108,110,116,120,Scale3_GREEN,1,DRAW_FILL_FULL);
        sleep_ms(500);
	}
}
