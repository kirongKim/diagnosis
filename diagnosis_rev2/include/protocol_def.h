#ifndef __PROTOCOL_DEF_H
#define __PROTOCOL_DEF_H

#define NETWORK_NOT_DEFINED         -0x1
#define NETWORK_IN_SERVICE          0x0
#define NETWORK_NO_SERVICE          0x1
#define NETWORK_LIMITED_SERVICE     0x2

#define SIM_NOT_DEFINED             -0x1
#define SIM_READY                   0x0
#define TEST_SIM_READY              0x1
#define LIMITED_SIM                 0x2
#define NO_SIM                      0x3
#define NOT_INIT                    0x4

#define GPS_NOT_DEFINED             -0x1
#define GPS_STOP                    0x0
#define GPS_WORKING                 0x1

#define UPDATE_STATUS_SUCCESS       0x0
#define UPDATE_STATUS_FAILURE       0x1

#define RNDIS_NOT_DEFINED           -0x1
#define RNDIS_DISCONNECTED          0x0
#define RNDIS_CONNECTED             0x1

#endif  // __PROTOCOL_DEF_H
