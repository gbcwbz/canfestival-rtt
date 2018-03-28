#include <stdint.h>
#include <rtthread.h>
#include <finsh.h>
#include "canfestival.h"
#include "canopen_callback.h"
#include "timers_driver.h"
#include "master402_canopen.h"
#include "master402_od.h"
#include "motor_control.h"

#define PRODUCER_HEARTBEAT_TIME 500
#define CONSUMER_HEARTBEAT_TIME 1000

struct servo_config_state
{
	uint8_t state;
	uint8_t try_cnt;
	struct rt_semaphore finish_sem;
};


void InitNodes(CO_Data* d, UNS32 id);

static void config_servo_param(uint8_t nodeId, struct servo_config_state *conf);
static void config_lift_rotate_servo_param(uint8_t nodeId, struct servo_config_state *conf);
static struct servo_config_state servo_conf[4];
static void config_servo(uint8_t nodeId);
static void config_single_servo(void *parameter);

CO_Data *OD_Data = &agv_Data;
s_BOARD agv_board  = {"0", "1M"};

void canopen_init(void)
{
	OD_Data->heartbeatError = agv_heartbeatError;
	OD_Data->initialisation = agv_initialisation;
	OD_Data->preOperational = agv_preOperational;
	OD_Data->operational = agv_operational;
	OD_Data->stopped   = agv_stopped;
	OD_Data->post_sync = agv_post_sync;
	OD_Data->post_TPDO = agv_post_TPDO;
	OD_Data->storeODSubIndex = (storeODSubIndex_t)agv_storeODSubIndex;
	OD_Data->post_emcy = (post_emcy_t)agv_post_emcy;

	canOpen(&agv_board, OD_Data);
	initTimer();

	// Start timer thread
	StartTimerLoop(&InitNodes);
}

void InitNodes(CO_Data* d, UNS32 id)
{
	setNodeId(OD_Data, 0x01);
	setState(OD_Data, Initialisation);
}

void Exit(CO_Data* d, UNS32 id)
{

}

static void slaveBootupHdl(CO_Data* d, UNS8 nodeId)
{
	rt_thread_t tid;

	tid = rt_thread_create("co_cfg", config_single_servo, (void *)nodeId, 1024, 12 + nodeId, 2);
	if(tid == RT_NULL)
	{
		rt_kprintf("canopen config thread start failed!\n");
	}
	else
	{
		rt_thread_startup(tid);
	}
}

void canopen_start_thread_entry(void *parameter)
{
	UNS32 sync_id, size;
	UNS8 data_type, sub_cnt;
	UNS32 consumer_heartbeat_time;

	rt_thread_delay(200);
	config_servo(SERVO_NODEID);
	OD_Data->post_SlaveBootup = slaveBootupHdl;
	consumer_heartbeat_time = (2 << 16) | CONSUMER_HEARTBEAT_TIME;
	size = 4;
	writeLocalDict(OD_Data, 0x1016, 1, &consumer_heartbeat_time, &size, 0);
	consumer_heartbeat_time = (3 << 16) | CONSUMER_HEARTBEAT_TIME;
	writeLocalDict(OD_Data, 0x1016, 2, &consumer_heartbeat_time, &size, 0);
	sub_cnt = 2;
	size = 1;
	writeLocalDict(OD_Data, 0x1016, 0, &sub_cnt, &size, 0);
	data_type = uint32;
	setState(OD_Data, Operational);
	masterSendNMTstateChange(OD_Data, SERVO_NODEID, NMT_Start_Node);
	size = 4;
	readLocalDict(OD_Data, 0x1005, 0, &sync_id, &size, &data_type, 0);
	sync_id |= (1 << 30);
	writeLocalDict(OD_Data, 0x1005, 0, &sync_id, &size, 0);
}

static void config_servo(uint8_t nodeId)
{
	servo_conf[nodeId - 2].state = 0;
	servo_conf[nodeId - 2].try_cnt = 0;
	rt_sem_init(&(servo_conf[nodeId - 2].finish_sem), "servocnf", 0, RT_IPC_FLAG_FIFO);

	EnterMutex();
	if(nodeId == LEFT_SERVO_NODEID || nodeId == RIGHT_SERVO_NODEID)
	{
		config_servo_param(nodeId, &servo_conf[nodeId - 2]);
	}
	else
	{
		config_lift_rotate_servo_param(nodeId, &servo_conf[nodeId - 2]);
	}
	LeaveMutex();
	rt_sem_take(&(servo_conf[nodeId - 2].finish_sem), RT_WAITING_FOREVER);
}

static void config_single_servo(void *parameter)
{
	uint32_t nodeId;
	nodeId = (uint32_t)parameter;
	servo_off(nodeId);
	config_servo(nodeId);
	masterSendNMTstateChange(OD_Data, nodeId, NMT_Start_Node);
}


static void config_servo_param_cb(CO_Data* d, UNS8 nodeId)
{
	UNS32 abortCode;
	UNS8 res;
	struct servo_config_state *conf;

	conf = &servo_conf[nodeId - 2];
	res = getWriteResultNetworkDict(OD_Data, nodeId, &abortCode);
	closeSDOtransfer(OD_Data, nodeId, SDO_CLIENT);
	if(res != SDO_FINISHED)
	{
		conf->try_cnt++;
		rt_kprintf("write SDO failed!  nodeId = %d, abortCode = 0x%08X\n", nodeId, abortCode);
		if(conf->try_cnt < 3)
		{
			config_servo_param(nodeId, conf);
		}
		else
		{
			rt_sem_release(&(conf->finish_sem));
			conf->state = 0;
			conf->try_cnt = 0;
			rt_kprintf("SDO config try count > 3, config failed!\n");
		}
	}
	else
	{
		conf->state++;
		conf->try_cnt = 0;
		config_servo_param(nodeId, conf);
	}
}

static void config_servo_param(uint8_t nodeId, struct servo_config_state *conf)
{
	switch(conf->state)
	{
	case 0:
		{ // disable Slave's TPDO
			UNS32 TPDO_COBId = 0x80000180 + nodeId;
			writeNetworkDictCallBack(OD_Data, nodeId, 0x1800, 1, 
				4, uint32, &TPDO_COBId, config_servo_param_cb, 0);
		}
		break;
	case 1:
		{
			UNS8 trans_type = PDO_TRANSMISSION_TYPE;
			writeNetworkDictCallBack(OD_Data, nodeId, 0x1800, 2, 
				1, uint8, &trans_type, config_servo_param_cb, 0);
		}
		break;
	case 2:
		{
			UNS8 pdo_map_cnt = 0;
			writeNetworkDictCallBack(OD_Data, nodeId, 0x1A00, 0, 
				1, uint8, &pdo_map_cnt, config_servo_param_cb, 0);
		}
		break;
	case 3:
		{
			UNS32 pdo_map_val = 0x60410010;
			writeNetworkDictCallBack(OD_Data, nodeId, 0x1A00, 1, 
				4, uint32, &pdo_map_val, config_servo_param_cb, 0);
		}
		break;
	case 4:
		{
			UNS32 pdo_map_val = 0x60630020;
			writeNetworkDictCallBack(OD_Data, nodeId, 0x1A00, 2, 
				4, uint32, &pdo_map_val, config_servo_param_cb, 0);
		}
		break;
	case 5:
		{
			UNS8 pdo_map_cnt = 2;
			writeNetworkDictCallBack(OD_Data, nodeId, 0x1A00, 0, 
				1, uint8, &pdo_map_cnt, config_servo_param_cb, 0);
		}
		break;
	case 6:
		{ // enable Slave's TPDO
			UNS32 TPDO_COBId = 0x00000180 + nodeId;
			writeNetworkDictCallBack(OD_Data, nodeId, 0x1800, 1, 
				4, uint32, &TPDO_COBId, config_servo_param_cb, 0);
		}
		break;
	case 7:
		{ // disable Slave's TPDO
			UNS32 TPDO_COBId = 0x80000280 + nodeId;
			writeNetworkDictCallBack(OD_Data, nodeId, 0x1801, 1, 
				4, uint32, &TPDO_COBId, config_servo_param_cb, 0);
		}
		break;
	case 8:
		{
			UNS8 trans_type = PDO_TRANSMISSION_TYPE_SLOW;
			writeNetworkDictCallBack(OD_Data, nodeId, 0x1801, 2, 
				1, uint8, &trans_type, config_servo_param_cb, 0);
		}
		break;
	case 9:
		{
			UNS8 pdo_map_cnt = 0;
			writeNetworkDictCallBack(OD_Data, nodeId, 0x1A01, 0, 
				1, uint8, &pdo_map_cnt, config_servo_param_cb, 0);
		}
		break;
	case 10:
		{
			UNS32 pdo_map_val = 0x606c0020;
			writeNetworkDictCallBack(OD_Data, nodeId, 0x1A01, 1, 
				4, uint32, &pdo_map_val, config_servo_param_cb, 0);
		}
		break;
	case 11:
		{
			UNS32 pdo_map_val = 0x60780010;
			writeNetworkDictCallBack(OD_Data, nodeId, 0x1A01, 2, 
				4, uint32, &pdo_map_val, config_servo_param_cb, 0);
		}
		break;
	case 12:
		{
			UNS8 pdo_map_cnt = 2;
			writeNetworkDictCallBack(OD_Data, nodeId, 0x1A01, 0, 
				1, uint8, &pdo_map_cnt, config_servo_param_cb, 0);
		}
		break;
	case 13:
		{ // enable Slave's TPDO
			UNS32 TPDO_COBId = 0x00000280 + nodeId;
			writeNetworkDictCallBack(OD_Data, nodeId, 0x1801, 1, 
				4, uint32, &TPDO_COBId, config_servo_param_cb, 0);
		}
		break;
	case 14:
		{ // disable Slave's RPDO
			UNS32 RPDO_COBId = 0x80000200 + nodeId;
			writeNetworkDictCallBack(OD_Data, nodeId, 0x1400, 1, 
				4, uint32, &RPDO_COBId, config_servo_param_cb, 0);
		}
		break;
	case 15:
		{
			UNS8 trans_type = PDO_TRANSMISSION_TYPE;
			writeNetworkDictCallBack(OD_Data, nodeId, 0x1400, 2, 
				1, uint8, &trans_type, config_servo_param_cb, 0);
		}
		break;
	case 16:
		{
			UNS8 pdo_map_cnt = 0;
			writeNetworkDictCallBack(OD_Data, nodeId, 0x1600, 0, 
				1, uint8, &pdo_map_cnt, config_servo_param_cb, 0);
		}
		break;
	case 17:
		{
			UNS32 pdo_map_val = 0x60400010;
			writeNetworkDictCallBack(OD_Data, nodeId, 0x1600, 1, 
				4, uint32, &pdo_map_val, config_servo_param_cb, 0);
		}
		break;
	case 18:
		{
			UNS32 pdo_map_val = 0x60600008;
			writeNetworkDictCallBack(OD_Data, nodeId, 0x1600, 2, 
				4, uint32, &pdo_map_val, config_servo_param_cb, 0);
		}
		break;
	case 19:
		{
			UNS8 pdo_map_cnt = 2;
			writeNetworkDictCallBack(OD_Data, nodeId, 0x1600, 0, 
				1, uint8, &pdo_map_cnt, config_servo_param_cb, 0);
		}
		break;
	case 20:
		{ // enable Slave's RPDO
			UNS32 RPDO_COBId = 0x00000200 + nodeId;
			writeNetworkDictCallBack(OD_Data, nodeId, 0x1400, 1, 
				4, uint32, &RPDO_COBId, config_servo_param_cb, 0);
		}
		break;
	case 21:
		{ // disable Slave's RPDO
			UNS32 RPDO_COBId = 0x80000300 + nodeId;
			writeNetworkDictCallBack(OD_Data, nodeId, 0x1401, 1, 
				4, uint32, &RPDO_COBId, config_servo_param_cb, 0);
		}
		break;
	case 22:
		{
			UNS8 trans_type = PDO_TRANSMISSION_TYPE;
			writeNetworkDictCallBack(OD_Data, nodeId, 0x1401, 2, 
				1, uint8, &trans_type, config_servo_param_cb, 0);
		}
		break;
	case 23:
		{
			UNS8 pdo_map_cnt = 0;
			writeNetworkDictCallBack(OD_Data, nodeId, 0x1601, 0, 
				1, uint8, &pdo_map_cnt, config_servo_param_cb, 0);
		}
		break;
	case 24:
		{
			UNS32 pdo_map_val = 0x607a0020;
			writeNetworkDictCallBack(OD_Data, nodeId, 0x1601, 1, 
				4, uint32, &pdo_map_val, config_servo_param_cb, 0);
		}
		break;
	case 25:
		{
			UNS32 pdo_map_val = 0x60810020;
			writeNetworkDictCallBack(OD_Data, nodeId, 0x1601, 2, 
				4, uint32, &pdo_map_val, config_servo_param_cb, 0);
		}
		break;
	case 26:
		{
			UNS8 pdo_map_cnt = 2;
			writeNetworkDictCallBack(OD_Data, nodeId, 0x1601, 0, 
				1, uint8, &pdo_map_cnt, config_servo_param_cb, 0);
		}
		break;	
	case 27:
		{ // enable Slave's RPDO
			UNS32 TPDO_COBId = 0x00000300 + nodeId;
			writeNetworkDictCallBack(OD_Data, nodeId, 0x1401, 1, 
				4, uint32, &TPDO_COBId, config_servo_param_cb, 0);
		}
		break;
	case 28:
		{ // disable Slave's RPDO
			UNS32 RPDO_COBId = 0x80000400 + nodeId;
			writeNetworkDictCallBack(OD_Data, nodeId, 0x1402, 1, 
				4, uint32, &RPDO_COBId, config_servo_param_cb, 0);
		}
		break;
	case 29:
		{
			UNS8 trans_type = PDO_TRANSMISSION_TYPE;
			writeNetworkDictCallBack(OD_Data, nodeId, 0x1402, 2, 
				1, uint8, &trans_type, config_servo_param_cb, 0);
		}
		break;
	case 30:
		{
			UNS8 pdo_map_cnt = 0;
			writeNetworkDictCallBack(OD_Data, nodeId, 0x1602, 0, 
				1, uint8, &pdo_map_cnt, config_servo_param_cb, 0);
		}
		break;
	case 31:
		{
			UNS32 pdo_map_val = 0x60830020;
			writeNetworkDictCallBack(OD_Data, nodeId, 0x1602, 1, 
				4, uint32, &pdo_map_val, config_servo_param_cb, 0);
		}
		break;
	case 32:
		{
			UNS32 pdo_map_val = 0x60840020;
			writeNetworkDictCallBack(OD_Data, nodeId, 0x1602, 2, 
				4, uint32, &pdo_map_val, config_servo_param_cb, 0);
		}
		break;
	case 33:
		{
			UNS8 pdo_map_cnt = 2;
			writeNetworkDictCallBack(OD_Data, nodeId, 0x1602, 0, 
				1, uint8, &pdo_map_cnt, config_servo_param_cb, 0);
		}
		break;	
	case 34:
		{ // enable Slave's RPDO
			UNS32 TPDO_COBId = 0x00000400 + nodeId;
			writeNetworkDictCallBack(OD_Data, nodeId, 0x1402, 1, 
				4, uint32, &TPDO_COBId, config_servo_param_cb, 0);
		}
		break;
	case 35:
		{
			UNS16 producer_heartbeat_time = PRODUCER_HEARTBEAT_TIME;
			writeNetworkDictCallBack(OD_Data, nodeId, 0x1017, 0, 
				2, uint16, &producer_heartbeat_time, config_servo_param_cb, 0);
		}
		break;
	case 36:
	{
		UNS16 spped_kp = SPEED_KP_SET_VALUE;
		writeNetworkDictCallBack(OD_Data, nodeId, 0x60F9, 1, 
			2, uint16, &spped_kp, config_servo_param_cb, 0);
	}
		break;
	case 37:
	{
		INTEGER8 homing_mode = 35; // 以当前位置为原点
		writeNetworkDictCallBack(OD_Data, nodeId, 0x6098, 0, 
			1, int8, &homing_mode, config_servo_param_cb, 0);
	}
		break;
	case 38:
	{
		INTEGER32 homing_offset = 0;
		writeNetworkDictCallBack(OD_Data, nodeId, 0x607C, 0, 
			4, int32, &homing_offset, config_servo_param_cb, 0);
	}
		break;
	case 39:
		rt_sem_release(&(conf->finish_sem));
		break;
	default:
		break;
	}
}


