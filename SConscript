
from building import *

cwd     = GetCurrentDir()
src = Split("""
src/emcy.c
src/lifegrd.c
src/nmtMaster.c
src/nmtSlave.c
src/objacces.c
src/pdo.c
src/sdo.c
src/states.c
src/sync.c
src/timer.c
src/can_rtthread.c
src/timer_rtthread.c
""")
CPPPATH = [GetCurrentDir() + '/inc']

group = DefineGroup('CanFestival', src, depend = ['RT_USING_CAN', 'RT_USING_HWTIMER'], CPPPATH = CPPPATH)

Return('group')
