#include "data.h"

void master402_heartbeatError(CO_Data* d, UNS8 heartbeatID);
void master402_initialisation(CO_Data* d);
void master402_preOperational(CO_Data* d);
void master402_operational(CO_Data* d);
void master402_stopped(CO_Data* d);

void master402_post_sync(CO_Data* d);
void master402_post_TPDO(CO_Data* d);
void master402_storeODSubIndex(CO_Data* d, UNS16 wIndex, UNS8 bSubindex);
void master402_post_emcy(CO_Data* d, UNS8 nodeID, UNS16 errCode, UNS8 errReg, const UNS8 errSpec[5]);
