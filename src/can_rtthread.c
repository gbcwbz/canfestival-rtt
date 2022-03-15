#include <stdint.h>
#include <rtthread.h>
#include <rtdevice.h>

#include "canfestival.h"
#include "timers_driver.h"

struct can_app_struct
{
    const char *name;
    struct rt_semaphore sem;
};

static rt_device_t candev = RT_NULL;
static CO_Data * OD_Data = RT_NULL;

static struct can_app_struct can_data =
{
    CANFESTIVAL_CAN_DEVICE_NAME
};

static rt_err_t  can1ind(rt_device_t dev,  rt_size_t size)
{
    rt_sem_release(&can_data.sem);
    return RT_EOK;
}

unsigned char canSend(CAN_PORT notused, Message *m)
{
	struct rt_can_msg msg;

	msg.id = m->cob_id;
	msg.ide = 0;
	msg.rtr = m->rtr;
	msg.len = m->len;
	memcpy(msg.data, m->data, m->len);
    RT_ASSERT(candev != RT_NULL);
	rt_device_write(candev, 0, &msg, sizeof(msg));
	
    return 0;
}

void canopen_recv_thread_entry(void* parameter)
{
    struct can_app_struct *canpara = (struct can_app_struct *) parameter;
	struct rt_can_msg msg;
	Message co_msg;

    candev = rt_device_find(canpara->name);
    RT_ASSERT(candev);
    rt_sem_init(&can_data.sem, "co-rx", 0, RT_IPC_FLAG_FIFO);
    rt_device_open(candev, (RT_DEVICE_OFLAG_RDWR | RT_DEVICE_FLAG_INT_RX | RT_DEVICE_FLAG_INT_TX));
    rt_device_set_rx_indicate(candev, can1ind);

    while (1)
    {
        if (rt_sem_take(&can_data.sem, RT_WAITING_FOREVER) == RT_EOK)
        {
			while (rt_device_read(candev, 0, &msg, sizeof(msg)) == sizeof(msg))
			{
				co_msg.cob_id = msg.id;
				co_msg.len = msg.len;
				co_msg.rtr = msg.rtr;
				memcpy(co_msg.data, msg.data, msg.len);
				EnterMutex();
				canDispatch(OD_Data, &co_msg);
				LeaveMutex();
			}
		}
    }
}

CAN_PORT canOpen(s_BOARD *board, CO_Data * d)
{
	rt_thread_t tid;
    
	OD_Data = d;
    tid = rt_thread_create("cf_recv",
                           canopen_recv_thread_entry, &can_data,
                           1024, CANFESTIVAL_RECV_THREAD_PRIO, 20);
    if (tid != RT_NULL) rt_thread_startup(tid);

    return 0;
}


