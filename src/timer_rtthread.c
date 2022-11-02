#include <stdbool.h>
#include <rtthread.h>
#include <rtdevice.h>
#include "canfestival.h"
#include "timer.h"
#include "timers_driver.h"

#define DBG_TAG "app.CANopen"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>


/************************** Module variables **********************************/
static rt_sem_t canfstvl_timer_sem = RT_NULL;
static rt_device_t canfstvl_timer_dev=RT_NULL;
static rt_hwtimerval_t last_timer_val;

static struct rt_thread canopen_timer_thread;
ALIGN(RT_ALIGN_SIZE) RT_SECTION(".ccm")
static char canopen_timer_thread_stack[1024];


void setTimer(TIMEVAL value)
{
	rt_hwtimerval_t val;
	
	val.sec = value / 1000000;
	val.usec = value % 1000000;
	/// Avoid invalid 0 timeout.
	/// See here for detailed reference: https://club.rt-thread.org/ask/question/11194.html .
	if(val.usec < MIN_TIMER_TIMEOUT_US){
		val.usec = MIN_TIMER_TIMEOUT_US;
	}

	last_timer_val.sec = 0;
	last_timer_val.usec = 0;
	if(rt_device_write(canfstvl_timer_dev, 0, &val, sizeof(val)) == 0) {
        LOG_E("CANopen set timer failed, err = %d", rt_get_errno());
	}
}

TIMEVAL getElapsedTime(void)
{
	rt_hwtimerval_t val;
    
	rt_device_read(canfstvl_timer_dev, 0, &val, sizeof(val));

	return (val.sec - last_timer_val.sec) * 1000000 + (val.usec - last_timer_val.usec);
}

static void canopen_timer_thread_entry(void* parameter)
{	
	while(1)
	{
		if(rt_sem_take(canfstvl_timer_sem, RT_WAITING_FOREVER) != RT_EOK) {
            LOG_E("canfestival take timer sem failed");
            return;
		}

		EnterMutex();
		rt_size_t read_size = rt_device_read(canfstvl_timer_dev, 0, &last_timer_val, sizeof(last_timer_val));
		if( read_size == 0) {
            LOG_E("canfestival read timer failed, err = %d", rt_get_errno());
		} else {
            TimeDispatch();
		}
		LeaveMutex();
	}
}


static rt_err_t timer_timeout_cb(rt_device_t dev, rt_size_t size)
{
	rt_sem_release(canfstvl_timer_sem);
    
    return RT_EOK;
}


void initTimer(void)
{
	rt_err_t err;
	rt_hwtimer_mode_t mode;
	int freq = 1000000;

	canfstvl_timer_sem = rt_sem_create("canfstvl", 0, RT_IPC_FLAG_PRIO);

	canfstvl_timer_dev = rt_device_find(CANFESTIVAL_TIMER_DEVICE_NAME);
	RT_ASSERT(canfstvl_timer_dev != RT_NULL);
	err = rt_device_open(canfstvl_timer_dev, RT_DEVICE_OFLAG_RDWR);
	if (err != RT_EOK)
    {
        rt_kprintf("CanFestival open timer Failed! err=%d\n", err);
        return;
    }
	rt_device_set_rx_indicate(canfstvl_timer_dev, timer_timeout_cb);
	err = rt_device_control(canfstvl_timer_dev, HWTIMER_CTRL_FREQ_SET, &freq);
    if (err != RT_EOK)
    {
        rt_kprintf("Set Freq=%dhz Failed\n", freq);
    }

    mode = HWTIMER_MODE_ONESHOT;
    err = rt_device_control(canfstvl_timer_dev, HWTIMER_CTRL_MODE_SET, &mode);
	rt_device_read(canfstvl_timer_dev, 0, &last_timer_val, sizeof(last_timer_val));

	err = rt_thread_init(&canopen_timer_thread,"cf_timer",
                           canopen_timer_thread_entry, RT_NULL,
                           &canopen_timer_thread_stack[0],sizeof(canopen_timer_thread_stack),
                           CANFESTIVAL_TIMER_THREAD_PRIO, 20);
    if (err == RT_EOK) rt_thread_startup(&canopen_timer_thread);

}

static TimerCallback_t init_callback;
void StartTimerLoop(TimerCallback_t _init_callback) 
{
	init_callback = _init_callback;
	EnterMutex();
	SetAlarm(NULL, 0, init_callback, 5, 0);
	LeaveMutex();
}


