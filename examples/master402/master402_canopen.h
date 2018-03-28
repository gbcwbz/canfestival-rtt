#ifndef __AGV_CANOPEN_H__
#define __AGV_CANOPEN_H__

#define CONTROLLER_NODEID 	1
#define SERVO_NODEID 		2
#define PDO_TRANSMISSION_TYPE 1

extern CO_Data *OD_Data;
void canopen_start_thread_entry(void *parameter);

#endif
