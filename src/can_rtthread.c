#include <stdint.h>
#include <rtthread.h>
#include <rtdevice.h>

#include "canfestival.h"
#include "timers_driver.h"

#define MAX_MUTEX_WAIT_TIME 5000
#define MAX_SEM_WAIT_TIME 5000

struct can_app_struct
{
    const char *name;
    struct rt_semaphore sem;
};

static rt_device_t candev = RT_NULL;
static CO_Data * OD_Data = RT_NULL;
static rt_mutex_t canfstvl_mutex = RT_NULL;

static struct can_app_struct can_data =
{
    CANFESTIVAL_CAN_DEVICE_NAME
};

void EnterMutex(void)
{
	if(rt_mutex_take(canfstvl_mutex, MAX_MUTEX_WAIT_TIME) != RT_EOK) {
		LOG_E("canfestival take mutex failed!");
	}
}

void LeaveMutex(void)
{
	if(rt_mutex_release(canfstvl_mutex) != RT_EOK) {
		LOG_E("canfestival release mutex failed!");
	}
}

static rt_err_t  can1ind(rt_device_t dev,  rt_size_t size)
{
    rt_err_t err = rt_sem_release(&can_data.sem);
    if(err != RT_EOK) {
		LOG_E("canfestival release receive semaphore failed!");
    }
    return err;
}

unsigned char canSend(CAN_PORT notused, Message *m)
{
    static int err_cnt = 0;

	struct rt_can_msg msg;

	msg.id = m->cob_id;
	msg.ide = 0;
	msg.rtr = m->rtr;
	msg.len = m->len;
	memcpy(msg.data, m->data, m->len);
    RT_ASSERT(candev != RT_NULL);

	rt_size_t write_size = rt_device_write(candev, 0, &msg, sizeof(msg));
	if(write_size != sizeof(msg)) {
		LOG_W("canfestival send failed, err = %d", rt_get_errno());
		if(++err_cnt >= 100) {
		    setState(OD_Data, Stopped);
		}
	    return 0xFF;
	}
	
	err_cnt = 0;
    return 0;
}

void canopen_recv_thread_entry(void* parameter)
{
    struct can_app_struct *canpara = (struct can_app_struct *) parameter;
	struct rt_can_msg msg;
	Message co_msg;

    candev = rt_device_find(canpara->name);
    RT_ASSERT(candev);
    rt_sem_init(&can_data.sem, "co-rx", 0, RT_IPC_FLAG_PRIO);
    rt_err_t err = rt_device_open(candev, (RT_DEVICE_OFLAG_RDWR | RT_DEVICE_FLAG_INT_RX | RT_DEVICE_FLAG_INT_TX));
    if( err != RT_EOK) {
		LOG_E("canfestival open device %s failed, err = %d", canpara->name, err);
        return;
    }

    err = rt_device_set_rx_indicate(candev, can1ind);
    if( err != RT_EOK) {
		LOG_E("canfestival set rx indicate failed, err = %d", err);
        return;
    }

    rt_size_t read_size = 0;
    while (1)
    {
        err = rt_sem_take(&can_data.sem, MAX_SEM_WAIT_TIME);
        if ( err != RT_EOK)
        {
            if(getState(OD_Data) == Operational) {
                LOG_W("canfestival wait receive timeout, err = %d", err);
            }
		} else {
		    read_size = rt_device_read(candev, 0, &msg, sizeof(msg));
			if( read_size == sizeof(msg))
			{
				co_msg.cob_id = msg.id;
				co_msg.len = msg.len;
				co_msg.rtr = msg.rtr;
				memcpy(co_msg.data, msg.data, msg.len);
				EnterMutex();
				canDispatch(OD_Data, &co_msg);
				LeaveMutex();
			} else if (read_size == 0){
                LOG_W("canfestival receive faild, err = %d", rt_get_errno());
			} else {
                LOG_W("canfestival receive size wrong, size = %u", read_size);
			}
		}
    }
}

CAN_PORT canOpen(s_BOARD *board, CO_Data * d)
{
	rt_thread_t tid;
	canfstvl_mutex = rt_mutex_create("canfstvl",RT_IPC_FLAG_PRIO);
    
	OD_Data = d;
    tid = rt_thread_create("cf_recv",
                           canopen_recv_thread_entry, &can_data,
                           1024, CANFESTIVAL_RECV_THREAD_PRIO, 20);
    if (tid != RT_NULL) rt_thread_startup(tid);

    return 0;
}


