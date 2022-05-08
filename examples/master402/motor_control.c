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

#define CONTROL_WORD_DISABLE_VOLTAGE 0x00
#define CONTROL_WORD_SHUTDOWN 0x06
#define CONTROL_WORD_SWITCH_ON 0x07
#define CONTROL_WORD_ENABLE_OPERATION 0x0F

#define STATUS_WORD_TARGET_REACHED_BIT (1 << 10)
#define STATUS_WORD_FOLLOWING_ERROR_BIT (1 << 12)

// bits for Profile Position mode
#define CONTROL_WORD_NEW_SETPOINT_BIT (1 << 4)
#define CONTROL_WORD_CHANGE_SET_IMMEDIATELY_BIT (1 << 5)
#define CONTROL_WORD_CHANGE_RELATIVE_BIT (1 << 6)
#define STATUS_WORD_SETPOINT_ACKNOWLEDGE_BIT (1 << 12)

#ifdef RT_USING_MSH
static void cmd_motor_on(int argc, char* argv[])
{
    int nodeId = 1;
    if(argc > 1) {
        nodeId = atoi(argv[1]);
    }
    (void)nodeId;

	modes_of_operation_6060 = PROFILE_POSITION_MODE;
	profile_velocity_6081 = 0;
	target_position_607a = 0;

	control_word_6040 = CONTROL_WORD_SHUTDOWN;
	SYNC_DELAY;
	control_word_6040 = CONTROL_WORD_SWITCH_ON;
	SYNC_DELAY;
	control_word_6040 = CONTROL_WORD_ENABLE_OPERATION;
}
MSH_CMD_EXPORT_ALIAS(cmd_motor_on, motor_on, power on motor driver);

static void cmd_motor_off(int argc, char* argv[])
{
    int nodeId = 1;
    if(argc > 1) {
        nodeId = atoi(argv[1]);
    }
    (void)nodeId;


	control_word_6040 = CONTROL_WORD_SHUTDOWN;
	SYNC_DELAY;
	control_word_6040 = CONTROL_WORD_DISABLE_VOLTAGE;
}
MSH_CMD_EXPORT_ALIAS(cmd_motor_off, motor_off, power off motor driver);


static void cmd_motor_relmove(int argc, char* argv[])
{
    int32_t position = 0;
    int32_t speed = 20;

    if(argc < 2) {
        rt_kprintf("Usage: motor_relmove [position] <speed>\n");
    }

    position = atoi(argv[1]);
    if(argc > 2) {
        speed = atoi(argv[2]);
    }

    rt_kprintf("move to position: %d, speed: %d\n", position, speed);


	target_position_607a = position;
	profile_velocity_6081 = speed;

	SYNC_DELAY;
	control_word_6040 = (CONTROL_WORD_ENABLE_OPERATION);
	SYNC_DELAY;
	control_word_6040 = 0x6f;
}
MSH_CMD_EXPORT_ALIAS(cmd_motor_relmove, motor_relmove, move motor to relative position);

static void cmd_motor_state(void)
{
	rt_kprintf("ControlWord 0x%0X\n", control_word_6040);
	rt_kprintf("StatusWord 0x%0X\n", status_word_6041);
	rt_kprintf("current position %d\n", position_actual_value_6063);
	rt_kprintf("current speed %d\n", velocity_actual_value_606c);
}
MSH_CMD_EXPORT_ALIAS(cmd_motor_state, motor_state, print states of motors);

#endif
