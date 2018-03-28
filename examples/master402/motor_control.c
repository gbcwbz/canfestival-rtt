#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include <rtthread.h>
#include <rtdevice.h>

#ifdef RT_USING_FINSH
#include <finsh.h>
#endif

#include "canfestival.h"
#include "timers_driver.h"
#include "master402_od.h"
#include "master402_canopen.h"

#define SYNC_DELAY rt_thread_delay(RT_TICK_PER_SECOND/50)
#define PROFILE_POSITION_MODE 1
#define ENCODER_RES (2500 * 4)

void servo_on(uint8_t nodeId)
{
	UNS32 speed;

	speed = ENCODER_RES * 30;
	modes_of_operation_6060 = PROFILE_POSITION_MODE;
	profile_velocity_6081 = speed;
	target_position_607a = 0;

	control_word_6040 = 0x06;
	SYNC_DELAY;
	control_word_6040 = 0x0f;
}
#ifdef RT_USING_FINSH
FINSH_FUNCTION_EXPORT(servo_on, set servo on);
#endif


void relative_move(int32_t position, int32_t speed)
{
	target_position_607a = position;
	profile_velocity_6081 = speed;

	control_word_6040 = 0x6f;
	SYNC_DELAY;
	control_word_6040 = 0x7f;
}
#ifdef RT_USING_FINSH
FINSH_FUNCTION_EXPORT_ALIAS(relative_move, relmove, relative move);
#endif

void motorstate(void)
{
	rt_kprintf("ControlWord 0x%0X\n", control_word_6040);
	rt_kprintf("StatusWord 0x%0X\n", status_word_6041);
	rt_kprintf("current position %d\n", position_actual_value_6063);
	rt_kprintf("current speed %d\n", velocity_actual_value_606c);
}
#ifdef RT_USING_FINSH
FINSH_FUNCTION_EXPORT(motorstate, print motor state);
#endif
