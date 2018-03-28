#include <stdint.h>
#include <rtthread.h>
#include <rtdevice.h>

#include "canfestival.h"
#include "timers_driver.h"

struct can_app_struct
{
    const char *name;
    struct rt_can_filter_config *filter;
    rt_uint8_t eventopt;
    struct rt_event event;
};

static rt_err_t  can1ind(rt_device_t dev,  void *args, rt_int32_t hdr, rt_size_t size);

static rt_device_t candev = RT_NULL;
static struct can_app_struct can_data;
static CO_Data * OD_Data = RT_NULL;

struct rt_can_filter_item filter1item[1] =
{
	RT_CAN_FILTER_ITEM_INIT(0x180, 0, 0, 1, 0, can1ind, &can_data.event)
};

struct rt_can_filter_config filter1 =
{
    1,
    1,
    filter1item,
};

static struct can_app_struct can_data =
{
    CANFESTIVAL_CAN_DRIVER_NAME,
    &filter1,
    RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR,
};

static rt_err_t  can1ind(rt_device_t dev,  void *args, rt_int32_t hdr, rt_size_t size)
{
    rt_event_t pevent = (rt_event_t)args;
    rt_event_send(pevent, 1 << (hdr));
    return RT_EOK;
}

unsigned char canSend(CAN_PORT notused, Message *m)
{
	struct rt_can_msg msg;

	msg.hdr = can_data.filter->items[1].hdr;
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
    rt_uint32_t e;
	Message co_msg;

    candev = rt_device_find(canpara->name);
    RT_ASSERT(candev);
    rt_event_init(&canpara->event, canpara->name, RT_IPC_FLAG_FIFO);
    rt_device_open(candev, (RT_DEVICE_OFLAG_RDWR | RT_DEVICE_FLAG_INT_RX | RT_DEVICE_FLAG_INT_TX));
    rt_device_control(candev, RT_CAN_CMD_SET_FILTER, canpara->filter);

    while (1)
    {
        if (rt_event_recv(&canpara->event,
                          (1 << canpara->filter->items[0].hdr),
                          canpara->eventopt,
                          RT_WAITING_FOREVER, &e) != RT_EOK)
        {
            continue;
        }

		if (e & (1 << canpara->filter->items[0].hdr))
		{
			msg.hdr = canpara->filter->items[0].hdr;
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


