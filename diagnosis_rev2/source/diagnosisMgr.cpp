/**
*
* @brief   diagnosisMgr Class
* @details Diagnosis manager
* @author  SL Corporation
* @date    2021-04
* @version 1.0.0
*
*/
#include "diagnosisMgr.hpp"
#include "log.h"
#include "ByteBuffer.hpp"
#include "EventFactory.hpp"

#include <iostream>       // std::cout
#include <string>
#include <typeinfo>       // operator typeid

#include <stdlib.h>
#include "modem_status_def.h"

#include "slipcManager.hpp"

using namespace::std;

#define EVENT_HEADER_LENGTH 4
#define EVENT_BODY_LENGTH_UPPER 2
#define EVENT_BODY_LENGTH_LOWER 3

#define FILE_BATT "/sys/class/ticdev/ticdev1.0/vbat"
//#define FILE_BATT "/data/test_folder/jack/vbat"
#define FILE_CAM  "/sys/devices/platform/soc/88c000.i2c/i2c-2/2-0048/gmls2_status"
#define HIGH_VOLTAGE 11250
#define LOW_VOLTAGE 10750
#define HIGH   0X02
#define NORMAL 0X00
#define LOW    0x01
#define THREAD_SLEEP 30

template <class T>
ThreadPool::ThreadPool* Singleton<T>::m_threadPool = new ThreadPool::ThreadPool(MAX_NUMBER_THREADS);

struct _modemStatus
{
    bool     runningStatus;
    uint16_t simStatus;
    uint16_t networkStatus;
    uint16_t serviceStatus;
    uint16_t gpsStatus;
    uint16_t otaStatus;
    uint16_t rndisStatus;
};
_modemStatus mModemStatus;
struct _peripheralDeviceStatus
{
    uint8_t AccSensor;
    uint8_t camera;
    uint8_t radar;
};
_peripheralDeviceStatus mPeripheralDeviceStatus;
uint8_t mRadarStatus;
uint8_t mCameraStatus;
volatile bool connectedWithSentinel = false;
volatile bool systemSleep = false;
uint8_t mBatteryLevel = NORMAL;
uint8_t mAccSensorStatus = 0;

/**
*
* @brief handle_routine_modem
* This function runs as thread to communicate with the modem manager
*
*/
void * diagnosisMgr::handle_routine_modem(void *arg) {
    char buf[MAXBUF];
    ssize_t req_len;

    // waiting for client
    bool allowed = CDiagServerSocket::GetInstance()->Listening(MODEM_DIAG_TCP_PORT);

    // get client socket descriptor
    int socket = CDiagServerSocket::GetInstance()->getSocket(MODEM_DIAG_TCP_PORT);  // 50006

    while((socket > 0) && allowed) {
        req_len = CDiagServerSocket::GetInstance()->recv_data(socket, (char *)buf, sizeof(buf), 0);

        if(req_len <= 0) {
            DBG_LOG("we consider socket is closed.\n");
            CDiagServerSocket::GetInstance()->closeSocket(MODEM_DIAG_TCP_PORT);
            break;
        } else {
            DBG_LOG("received from modem manager!!! length=%lu\n", req_len);
            std::string buffer;
            for(unsigned int i = 0; i < req_len ; i++) {
                char temp[8] ={0,};
                snprintf(temp, 8, "%02x ", buf[i]);
                
                buffer.append(temp);
                if(i%8 == 7) {
                    buffer.append("\n");
                    DBG_LOG(buffer.c_str());
                    buffer.clear();
                }
            }
            if(!buffer.empty()) {
                buffer.append("\n");
                DBG_LOG("DBG : check this %s\n", buffer.c_str());
            }

            int remain;
            int totalLength = 0;
            for (remain = req_len; remain > 0; ) {
                if(remain < 6) {
                    DBG_LOG("invalid length\n");
                    break;
                }

                uint16_t bodyLength = ((buf[totalLength + EVENT_BODY_LENGTH_UPPER] << 8) & 0xff00) | (buf[totalLength + EVENT_BODY_LENGTH_LOWER] & 0xff);
                totalLength += (EVENT_HEADER_LENGTH + bodyLength);

                stringbuf strbuf;
                strbuf.sputn(&buf[req_len - remain], totalLength);
                strbuf.pubseekoff(0, ios::beg);

                shared_ptr<CEventPacket> event = CEventFactory::createEvent(strbuf);
                if(event) {
                    DBG_LOG("%s\n", event->toString().c_str());
                    diagnosisMgr::getInstance().handle_Event_Modem(event);
                }
                remain = req_len - totalLength;
                DBG_LOG("DBG : bodyLength(%d), remain(%d)\n", bodyLength, remain);
            }
#if 0
            stringbuf mybuf;
            mybuf.sputn(buf, req_len);

            uint16_t eventId;

            get(mybuf, eventId);
            DBG_LOG("event :%04x\n", eventId);
            mybuf.pubseekoff(0, ios::beg);
            shared_ptr<CEventPacket> event = CEventFactory::createEvent(mybuf);
            if(event) {
                DBG_LOG("[%s]\n", typeid(event).name());
                DBG_LOG("%s\n", event->toString().c_str());
                diagnosisMgr::getInstance().handle_Event(event);
            } else {
                DBG_LOG("%s\n", "Unknown event received!!");
            }
#endif
        }
    }

    if(gSignalStatus) diagnosisMgr::getInstance().handle_ReconnectModem();
    return NULL;
}

/**
*
* @brief handle_routine_commmgr
* This function runs as thread to communicate with communication manager
*
*/
void * diagnosisMgr::handle_routine_commmgr(void *arg) {
    char buf[MAXBUF];
    ssize_t req_len;

    // waiting for client
    bool allowed = CDiagServerSocket::GetInstance()->Listening(COMMMGR_DIAG_TCP_PORT);

    // get client socket descriptor
    int socket = CDiagServerSocket::GetInstance()->getSocket(COMMMGR_DIAG_TCP_PORT);

    while((socket > 0) && allowed) {
        req_len = CDiagServerSocket::GetInstance()->recv_data(socket, (char *)buf, sizeof(buf), 0);

        if(req_len <= 0) {
            DBG_LOG("we consider socket is closed!!!\n");
            CDiagServerSocket::GetInstance()->closeSocket(COMMMGR_DIAG_TCP_PORT);
            break;
        } else {
            DBG_LOG("received from Communication manager. req_len : %lu\n", req_len);
            std::string buffer;
            for(unsigned int i = 0; i < req_len ; i++) {
                char temp[8] ={0,};
                snprintf(temp, 8, "%02x ", buf[i]);
                buffer.append(temp);
                if(i%8 == 7) {
                    buffer.append("\n");
                    DBG_LOG(buffer.c_str());
                    buffer.clear();
                }
            }
            if(!buffer.empty()) {
                buffer.append("\n");
                DBG_LOG(buffer.c_str());
            }

            size_t remain = req_len;

            stringbuf mybuf;
            mybuf.sputn(buf, req_len);

            uint16_t eventId;

            get(mybuf, eventId);
            DBG_LOG("event :%04x\n", eventId);
            mybuf.pubseekoff(0, ios::beg);
            shared_ptr<CEventPacket> event = CEventFactory::createEvent(mybuf);
            if(event) {
                DBG_LOG("[%s]\n", typeid(event).name());
                DBG_LOG("%s\n", event->toString().c_str());
                diagnosisMgr::getInstance().handle_Event_Radar(event);
            } else {
                DBG_LOG("%s\n", "Unknown event received!!");
            }
        }
    }

    if(gSignalStatus) diagnosisMgr::getInstance().handle_ReconnectCommmgr();
    return NULL;
}

/**
*
* @brief handle_routine
* This function runs as thread to communicate with the Sentinel application
*
*/
void * diagnosisMgr::handle_routine(void *arg) {
    int socket = CEventSocket::GetInstance()->getSocket();
    char buf[MAXBUF];
    ssize_t req_len;

    connectedWithSentinel = true;
    while(1) {
// TEST
CSLipcManager::getInstance().sendMessage(0x00000001, 0x00000008, 0x00, 0x00000500, NULL, 0);

        req_len = CEventSocket::GetInstance()->recv_data(socket, (char *)buf, sizeof(buf), 0);

        if(req_len < 0) {
            DBG_LOG("we consider socket is closed!!!\n");
            CEventSocket::GetInstance()->closeSocket();
            connectedWithSentinel = false;
            break;
        } else {
            DBG_LOG("received from Main application(sentinel)!!! req_len : %lu\n", req_len);
            std::string buffer;
            for(unsigned int i = 0; i < req_len ; i++) {
                char temp[8] ={0,};
                snprintf(temp, 8, "%02x ", buf[i]);
                buffer.append(temp);
                if(i%8 == 7) {
                    buffer.append("\n");
                    DBG_LOG(buffer.c_str());
                    buffer.clear();
                }
            }
            if(!buffer.empty()) {
                buffer.append("\n");
                DBG_LOG(buffer.c_str());
            }

            int remain;
            int totalLength = 0;
            for (remain = req_len; remain > 0; ) {
                uint16_t bodyLength = ((buf[totalLength + EVENT_BODY_LENGTH_UPPER] << 8) & 0xff00) | (buf[totalLength + EVENT_BODY_LENGTH_LOWER] & 0xff);
                totalLength += (EVENT_HEADER_LENGTH + bodyLength);

                stringbuf strbuf;
                strbuf.sputn(&buf[req_len - remain], totalLength);
                strbuf.pubseekoff(0, ios::beg);

                shared_ptr<CEventPacket> event = CEventFactory::createEvent(strbuf);
                if(event) {
                    DBG_LOG("%s\n", event->toString().c_str());
                    diagnosisMgr::getInstance().handle_Event_Main(event);
                } else {
                    DBG_LOG("%s\n", "Unknown event received!!");
                }
                remain = req_len - totalLength;
                DBG_LOG("DBG : bodyLength(%d), remain(%d)\n", bodyLength, remain);
            }

#if 0
            stringbuf mybuf;
            mybuf.sputn(buf, req_len);

            uint16_t eventId;
            get(mybuf, eventId);
            DBG_LOG("event :%04x\n", eventId);

            mybuf.pubseekoff(0, ios::beg);
            shared_ptr<CEventPacket> event = CEventFactory::createEvent(mybuf);
            if(event) {
                DBG_LOG("[%s]\n", typeid(event).name());
                DBG_LOG("[diagnosisMgr] Received : event = %s\n", event->toString().c_str());
                diagnosisMgr::getInstance().handle_Event_Main(event);
            } else {
                DBG_LOG("%s\n", "Unknown event received!!");
            }
#endif
        }
    }

    if(gSignalStatus) diagnosisMgr::getInstance().handle_Reconnect();
    return NULL;
}

/**
*
* @brief handle_routine_internal
* Check the peripheral device status (camera, battery)
*
*/
void * diagnosisMgr::handle_routine_internal(void *arg) {
    FILE *pFile;
    char buf[16] = {0,};
    std::string myString;
    uint16_t voltageLevel = 0;
    uint8_t level = 0;
    bool checkHighLevel = false;
    bool checkLowLevel = false;
    bool checkNormalLevel = false;
    bool sendBatteryInfo = false;
    bool sendCameraInfo = false;
    mCameraStatus = 0x01; //initialized

    while(true) {
        if(!connectedWithSentinel || systemSleep) {
            // not connected with Sentinel application.
            DBG_LOG("Not connected with Sentinel or system is going to a sleep mode. %d %d\n", connectedWithSentinel, systemSleep);
            sleep(THREAD_SLEEP);
            continue;
        }

        /* 1. Check battery level
        * High voltage (0x02)   : over 11.25V
        * Normal voltage (0x00) : 10.75 ~ 11.25V
        * Low voltage (0x01)    : under 10.75V
        */
        pFile =fopen(FILE_BATT, "r");
        if( pFile == NULL ) {
            ERROR_LOG(" file open error (battery) !\n");
        } else {
            if( fgets(buf, 16, pFile) ) {
                myString = buf;
                voltageLevel = std::stoi(myString);
                DBG_LOG("read value (battery) = %d\n", voltageLevel);

                if((voltageLevel * 2) >= HIGH_VOLTAGE) {
                    // high voltage
                    if(level != HIGH) {
                        if(checkHighLevel) {
                            // Send battery level
                            sendBatteryInfo = true;
                            level = HIGH;
                            checkHighLevel = false;
                        } else {
                            checkHighLevel = true;
                        }
                        checkNormalLevel = false;
                        checkLowLevel = false;
                    }
                } else if((voltageLevel * 2) <= LOW_VOLTAGE) {
                    // low voltage
                    if(level != LOW) {
                        if(checkLowLevel) {
                            // Send battery level
                            sendBatteryInfo = true;
                            level = LOW;
                            checkLowLevel = false;
                        } else {
                            checkLowLevel = true;
                        }
                        checkHighLevel = false;
                        checkNormalLevel = false;
                    }
                } else {
                    // normal
                    if(level != NORMAL) {
                        if(checkNormalLevel) {
                            // Send battery level
                            sendBatteryInfo = true;
                            level = NORMAL;
                            checkNormalLevel = false;
                        } else {
                            checkNormalLevel = true;
                        }
                        checkLowLevel = false;
                        checkHighLevel = false;
                    }
                }
            }
            fclose(pFile);

            if(sendBatteryInfo) {
                if(CEventSocket::GetInstance()->getSocket() > 0) {
                    sendBatteryInfo = false;
                    DBG_LOG("Send battery level information : level(%d) \n", level);
                    auto ev = make_shared<CBatteryLevelInd>(BATTERY_LEVEL_IND, (int16_t)(sizeof(uint8_t)+sizeof(uint16_t)+sizeof(uint8_t)), 
                                                            (uint8_t)level, (uint16_t)voltageLevel, (uint8_t)0x00);
                    ev->toString();
                    CEventSocket::GetInstance()->send_data(ev->getPacket().str().c_str(), ev->getPacket().str().size());
                }
            }
        }

        //2. Check camera
        /*
        * TC에 확인 결과 disconnect 와 power off 상태를 구분할 방법이 없음 (04-12)
        * 항상 normal 상태로 올려 줄 수 밖에 없음
        * 만약 카메라가 연결되어 있지 않은 상태를 확인할 수 있으면, 에러 상태를 전송할 수 있음.
        */
        pFile = fopen(FILE_CAM, "r");
        if( pFile == NULL ) {
            ERROR_LOG("[ERROR] file open error (camera) !\n");
        } else {
            if( fgets(buf, 16, pFile) ) {
                myString = buf;

                if(mCameraStatus != NORMAL) {
                    DBG_LOG("CAMERA STATUS : read value = %s\n", myString.c_str());
                    mCameraStatus = NORMAL;  // normal status (0x00)
                    sendCameraInfo = true;
                }
            }
            fclose(pFile);

            if(sendCameraInfo) {
                if(CEventSocket::GetInstance()->getSocket() > 0) {
                    sendCameraInfo = false;
                    DBG_LOG("send CAMERA_STATUS_EVENT\n");
                    auto event = make_shared<CCameraStatusEvent>(CAMERA_STATUS_EVENT, (uint16_t)(sizeof(uint8_t) + sizeof(uint8_t)),
                                                                (uint8_t)0x00, (uint8_t)0x00);
                    event->toString();
                    CEventSocket::GetInstance()->send_data(event->getPacket().str().c_str(), event->getPacket().str().size());
                }
            }
        }

        sleep(THREAD_SLEEP); // 30 seconds
    }

    return NULL;
}

/**
*
* @brief handle_ReconnectModem
*
*/
void diagnosisMgr::handle_ReconnectModem() {
    sleep(2);
    m_threadPool->EnqueueJob(handler_ReconnectModem);
}

/**
*
* @broef handle_ReconnectCommmgr
*
*/
void diagnosisMgr::handle_ReconnectCommmgr() {
    sleep(2);
    m_threadPool->EnqueueJob(handler_ReconnectCommmgr);
}

/**
*
* @brief handle_Reconnect
*
*/
void diagnosisMgr::handle_Reconnect() {
    sleep(2);
    m_threadPool->EnqueueJob(handler_Reconnect);
}

/**
*
* @brief handler_ReconnectModem
*
*/
void diagnosisMgr::handler_ReconnectModem() {
    if(!CDiagServerSocket::GetInstance()->Init(handle_routine_modem, MODEM_DIAG_TCP_PORT))
        diagnosisMgr::getInstance().handle_ReconnectModem();
}

/**
*
* @brief handler_ReconnectCommmgr
*
*/
void diagnosisMgr::handler_ReconnectCommmgr() {
    if(!CDiagServerSocket::GetInstance()->Init(handle_routine_commmgr, COMMMGR_DIAG_TCP_PORT))
        diagnosisMgr::getInstance().handle_ReconnectCommmgr();
}

/**
*
* @brief handler_Reconnect
*
*/
void diagnosisMgr::handler_Reconnect() {
    if(!CEventSocket::GetInstance()->Init(handle_routine, diagnosisMgr::getInstance().getServerPort()))
        diagnosisMgr::getInstance().handle_Reconnect();
}

/**
*
* @brief handle_Event_modem
*
*/
void diagnosisMgr::handle_Event_Modem(shared_ptr<CEventPacket> event) {
    m_threadPool->EnqueueJob(handler_Event_Modem, event);
}

/**
*
* @brief handle_Event_Radar
*
*/
void diagnosisMgr::handle_Event_Radar(shared_ptr<CEventPacket> event) {
    m_threadPool->EnqueueJob(handler_Event_Radar, event);
}


/**
*
* @brief handle_Event_Main 
*
*/
void diagnosisMgr::handle_Event_Main(shared_ptr<CEventPacket> event) {
    m_threadPool->EnqueueJob(handler_Event_Main, event);
}

/**
*
* @brief handler_Event_Modem
*
*/
void diagnosisMgr::handler_Event_Modem(shared_ptr<CEventPacket> event) {
    switch(event->getEventId()) {
        case MODEM_DIAG_STATUS_NORAML:
        {
            uint16_t status = event->getEventSubValue();
            DBG_LOG("Modem status normal: status(%d)\n", status);
            // it means that modem is running
            mModemStatus.runningStatus = true;
        }
        break;

        case MODEM_DIAG_SIM_STATUS:
        {
            uint16_t status = event->getEventSubValue16bit();
            DBG_LOG("Modem SIM status : status(%d)\n", status);
            // normal(0x0000) test sim(0x0001) limited sim(0x0002) no sim(0x0003) not init(0x0004)
            mModemStatus.simStatus = status;
        }
        break;

        case MODEM_DIAG_NETWORK_STATUS:
        {
            uint16_t status = event->getEventSubValue16bit();
            DBG_LOG("Modem Network status: status(%d)\n", status);
            // normal(0x0000) no service(0x0001) limited service(0x0002)
            mModemStatus.networkStatus = status;
        }
        break;

        case MODEM_DIAG_GPS_STATUS:
        {
            uint16_t status = event->getEventSubValue16bit();
            DBG_LOG("Modem GPS status: status(%d)\n", status);
            // stop or working
            mModemStatus.gpsStatus = status;
        }
        break;

        case MODEM_DIAG_MODE_SW_UPDATE_STATUS:
        {
            uint16_t status = event->getEventSubValue16bit();
            DBG_LOG("Modem SW Update status: status(%d)\n", status);
            // success(0x0000) failure(0x0001)
            mModemStatus.otaStatus = status;
        }
        break;

        case MODEM_DIAG_RNDIS_INTERFACE_STATUS:
        {
            uint16_t status = event->getEventSubValue16bit();
            DBG_LOG("Modem RNDIS status: status(%d)\n", status);
            // disconnected(0x0000) connected(0x0001)
            mModemStatus.rndisStatus = status;
        }
        break;

        default:
        break;
    }
}

/*
* handler function for radar(communication manager)
* 컴매니저로부터 아래 데이터들을 받으면 메인으로 바로 바로 전달을 해야 하나 ?
*/
void diagnosisMgr::handler_Event_Radar(shared_ptr<CEventPacket> event) {
    switch(event->getEventId()) {
        case COMM_DIAG_RADAR_STATUS_NORAML:
        {
            DBG_LOG("Radar Status : Normal\n");
            uint16_t eventId = RADAR_STATUS_EVENT;
            mRadarStatus = 0x00;
            if(CEventSocket::GetInstance()->getSocket() > 0) {
                auto ev = make_shared<CRadarStatusEvent>(eventId,(uint16_t)(sizeof(uint8_t) + sizeof(uint8_t)), (uint8_t)mRadarStatus, (uint8_t)0x00);
                DBG_LOG("[TX] RADAR_STATUS_EVENT - Normal : %s\n", ev->toString().c_str());
                CEventSocket::GetInstance()->send_data(ev->getPacket().str().c_str(), ev->getPacket().str().size());
            }
        }
        break;

        case COMM_DIAG_RADAR_SETUP_FAIL:
        {
            DBG_LOG("Radar status : Setup Fail\n");
            uint16_t eventId = RADAR_STATUS_EVENT;
            mRadarStatus = 0x01;
            if(CEventSocket::GetInstance()->getSocket() > 0) {
                auto ev = make_shared<CRadarStatusEvent>(eventId,(uint16_t)(sizeof(uint8_t) + sizeof(uint8_t)), (uint8_t)mRadarStatus, (uint8_t)0x00);
                DBG_LOG("[TX] RADAR_STATUS_EVENT - setup fail : %s\n", ev->toString().c_str());
                CEventSocket::GetInstance()->send_data(ev->getPacket().str().c_str(), ev->getPacket().str().size());
            }
        }
        break;

        case COMM_DIAG_RADAR_RUN_FAIL:
        {
            DBG_LOG("Radar status : Run Fail\n");
            uint16_t eventId = RADAR_STATUS_EVENT;
            mRadarStatus = 0x02;
            if(CEventSocket::GetInstance()->getSocket() > 0) {
                auto ev = make_shared<CRadarStatusEvent>(eventId,(uint16_t)(sizeof(uint8_t) + sizeof(uint8_t)), (uint8_t)mRadarStatus, (uint8_t)0x00);
                DBG_LOG("[TX] RADAR_STATUS_EVENT - run fail : %s\n", ev->toString().c_str());
                CEventSocket::GetInstance()->send_data(ev->getPacket().str().c_str(), ev->getPacket().str().size());
            }
        }
        break;

        case COMM_DIAG_RADAR_OTA_FAIL:
        {
            DBG_LOG("Radar status : OTA Fail\n");
            uint16_t eventId = RADAR_STATUS_EVENT;
            mRadarStatus = 0x04;
            if(CEventSocket::GetInstance()->getSocket() > 0) {
                auto ev = make_shared<CRadarStatusEvent>(eventId,(uint16_t)(sizeof(uint8_t) + sizeof(uint8_t)), (uint8_t)mRadarStatus, (uint8_t)0x00);
                DBG_LOG("[TX] RADAR_STATUS_EVENT - ota fail : %s\n", ev->toString().c_str());
                CEventSocket::GetInstance()->send_data(ev->getPacket().str().c_str(), ev->getPacket().str().size());
            }
        }
        break;

        default:
        break;
    }
}

/**
*
* @brief handler_Event_Main
*
*
*/
void diagnosisMgr::handler_Event_Main(shared_ptr<CEventPacket> event) {
    switch(event->getEventId()) {
        case MODEM_STATUS_REQUEST:  // 0x060B
        {
            /*
            * event Id : MODEM_STATUS_EVENT (0x0600), length(4) = message(3) + pad(1)
            * message (3bytes) = field(1) + value(2bytes)
            * Modem status normal : 0x06 0x00 0x00 0x04 0x00 0x00 0x00 0x00
            * SIM status : field(0x01) + value(0x0000 or 0x0001 or 0x0002 or 0x0003 or 0x0004)
            * Network status : field(0x02) + value(0x0000 or 0x0001 or 0x0002)
            * GPS status : field(0x03) + value(0x0000 or 0x0001)
            * OTA status : field(0x04) + value(0x0000 or 0x0001)
            * RNDIS status :  field(0x05) + value(0x0000 or 0x0001)
            */
            uint8_t type = 0x00;
            uint16_t errCode = 0x0000;
            uint16_t eventId = MODEM_STATUS_EVENT;

// 모뎀으로부터 상태값을 이미 받은 상태라고 가정을 하면.
            if(mModemStatus.rndisStatus != RNDIS_CONNECTED) {
                // disconnected : error
                type = 0x05;
                errCode = RNDIS_DISCONNECTED;
            } else if(mModemStatus.simStatus != SIM_READY) {
                // test sim, limited sim, no sim, not init
                type = 0x01;
                errCode = mModemStatus.simStatus;
            } else if(mModemStatus.networkStatus != NETWORK_IN_SERVICE) {
                // no service or limited service
                type = 0x02;
                errCode = mModemStatus.networkStatus;
            } else if(mModemStatus.otaStatus != 0x0000) {
                type = 0x04;
                errCode = mModemStatus.otaStatus;
            } else if(mModemStatus.gpsStatus != GPS_STOP) {
                type = 0x03;
                errCode = GPS_WORKING;
            }
            DBG_LOG("MODEM STATUS request from Main. type(%02x), errCode(%04x)\n", type, errCode);
// 만약 모뎀으로부터 상태값을 받지 못했을 때 어떻게 할지 검토가 필요하다.
           
            if(CEventSocket::GetInstance()->getSocket() > 0) { 
                auto ev = make_shared<CModemStatusEvent>(eventId,(int16_t)(sizeof(uint8_t)+sizeof(uint16_t)+sizeof(uint8_t)), 
                                                           (uint8_t)type, (uint16_t)errCode, (uint8_t)0x00);
                DBG_LOG("[TX] MODEM STATUS RESPONSE : %s\n", ev->toString().c_str());
                CEventSocket::GetInstance()->send_data(ev->getPacket().str().c_str(), ev->getPacket().str().size());
            }
        }
        break;

        /* Not Used. */
        case DIAG_APP_VERSION_REQUEST:
        {
            DBG_LOG("Diag application version request from Main\n");
        }
        break;

        case DIAG_SLEEP_WAKEUP_REQUEST:  // 0x0706
        {
            int sleepMode = event->getEventSubValue();
            if(sleepMode == 0) {
                systemSleep = true;
            } else {
                systemSleep = false;
            }
            DBG_LOG("DIAG SLEEP/WAKEUP request from Main. sleepMode(%d)\n", sleepMode);
            uint16_t eventId = DIAG_SLEEP_WAKEUP_RESPONSE;
            if(CEventSocket::GetInstance()->getSocket() > 0) {
                auto ev = make_shared<CSleepWakeupResponse>(eventId,(uint16_t)(sizeof(uint8_t) + sizeof(uint8_t)), (uint8_t)0x00, (uint8_t)0x00);
                DBG_LOG("[TX] DIAG SLEEP/WAKEUP RESPONSE : %s\n", ev->toString().c_str());
                CEventSocket::GetInstance()->send_data(ev->getPacket().str().c_str(), ev->getPacket().str().size());
            }
        }
        break;

        /* Not Used. */
        case ACCELEROMETER_STATUS_REQUEST:  // 0x0802
        {
            DBG_LOG("ACCELEROMETER_STATUS_REQUEST(0x0802) not used.\n");
        }
        break;

        /* Not Used. */
        case ACCELEROMETER_DATA_REQUEST:  // 0x0803
        {
            DBG_LOG("ACCELEROMETER_DATA_REQUEST(0x0803) not used.\n");
        }
        break;

        /* Not Used. */
        case BATTERY_LEVEL_REQUEST:  // 0x0901
        {
            DBG_LOG("BATTERY_LEVEL_REQUEST(0x0901) not used.\n");
        }
        break;

        /* Not Used. */
        case CAMERA_STATUS_REQUEST:  // 0x0402
        {
            DBG_LOG("CAMERA_STATUS_REQUEST(0x0402) not used.\n");
        }
        break;

        case HEARTBEAT_EVENT:  // 0x0300
        {
            DBG_LOG("HEARTBEAT event from Main\n");
        }
        break;

        case PERIPHERAL_DEVICE_POWER_EVENT:  // 0x0301
        {
            /*
            * device : Acc. sensor(0x01), Camera(0x02), Radar(0x04)
            * status : off(0x00), on(0x01)
            */
            uint8_t device = event->getEventSubValue();
            uint8_t status = event->getEventSubValue2nd();
            if(device == 0x01) {
                mPeripheralDeviceStatus.AccSensor = status;
            } else if(device == 0x02) {
                mPeripheralDeviceStatus.camera = status;
            } else if(device == 0x04) {
                mPeripheralDeviceStatus.radar = status;
            }
            DBG_LOG("PERIPHERAL DEVICE POWER event from Main. device(%d), status(%d)\n", device, status);
        }
        break;

// 시스템 상태 응답 클래스 만들어야 함.
        case SYSTEM_STATUS_REQUEST:  // 0x0704
        {
            DBG_LOG("System status request from Main\n");
            /* 0x0703, length(5) = message(4) + pad(1)
            * Ex.) 0x07 0x03 0x00 0x05 0x00 0x00 0x00 0x00 0x00
            * Normal status           = 0x00000000
            * Camera Error            = 0x00000001
            * Radar setup failure     = 0x00000002
            * Radar data read failure = 0x00000004
            * Radar OTA failure       = 0x00000008
            * Modem SIM error         = 0x00000010
            * Modem Network error     = 0x00000020
            * Modem Service error     = 0x00000040
            * Modem GPS status        = 0x00000080
            * Modem OTA failure       = 0x00000100
            * Modem RNDIS disconnect  = 0x00000200
            * Acc. error              = 0x00000400
            * Battery Low voltage     = 0x00000800
            * Battery High voltage    = 0x00001000
            */

            uint16_t eventId = SYSTEM_STATUS_RESPONSE;
            DBG_LOG("SYSTEM_STATUS_REQUEST : mCameraStatus(%d)\n", mCameraStatus);
            DBG_LOG("SYSTEM_STATUS_REQUEST : mRadarStatus(%d)\n", mRadarStatus);
            DBG_LOG("SYSTEM_STATUS_REQUEST : mModemStatus.simStatus(%d)\n", mModemStatus.simStatus);
            DBG_LOG("SYSTEM_STATUS_REQUEST : mModemStatus.networkStatus(%d)\n", mModemStatus.networkStatus);
            DBG_LOG("SYSTEM_STATUS_REQUEST : mModemStatus.serviceStatus(%d)\n", mModemStatus.serviceStatus);
            DBG_LOG("SYSTEM_STATUS_REQUEST : mModemStatus.gpsStatus(%d)\n", mModemStatus.gpsStatus);
            DBG_LOG("SYSTEM_STATUS_REQUEST : mModemStatus.otaStatus(%d)\n", mModemStatus.otaStatus);
            DBG_LOG("SYSTEM_STATUS_REQUEST : mModemStatus.rndisStatus(%d)\n", mModemStatus.rndisStatus);

            uint32_t result = (((mCameraStatus & 0x0f) & 0x0000000f) |
            (((mRadarStatus & 0x0f) << 1) & 0x0000000f) |
            ((mModemStatus.simStatus ? 0x00000010 : 0x00000000) & 0x000000f0) |
            ((mModemStatus.networkStatus ? 0x00000020 : 0x00000000) & 0x000000f0) |
            ((mModemStatus.serviceStatus ? 0x00000040 : 0x00000000) & 0x000000f0) |
            ((mModemStatus.gpsStatus ? 0x00000080 : 0x00000000) & 0x000000f0) |
            ((mModemStatus.otaStatus ? 0x00000100 : 0x00000000) & 0x00000f00) |
            ((mModemStatus.rndisStatus ? 0x00000000 : 0x00000200) & 0x00000f00) |
            ((mAccSensorStatus ? 0x00000400 : 0x00000000) & 0x00000f00));

            if(mBatteryLevel == HIGH) {
                result = result | 0x00001000;
            } else if(mBatteryLevel == LOW) {
                result = result | 0x00001000;
            }

            DBG_LOG("SYSTEM_STATUS_REQUEST : result(%08x)\n", result);

            if(CEventSocket::GetInstance()->getSocket() > 0) {
                auto ev = make_shared<CSystemStatusResponse>(eventId,(uint32_t)(sizeof(uint32_t) + sizeof(uint8_t)), (uint32_t)result, (uint8_t)0x00);
                DBG_LOG("[TX] SYSTEM STATUS RESPONSE : %s\n",ev->toString().c_str());
                CEventSocket::GetInstance()->send_data(ev->getPacket().str().c_str(), ev->getPacket().str().size());
            }
        }
        break;

        case DIAG_ARMED_EVENT:  // 0x0702
        {
            int armedMode = event->getEventSubValue();
            int ret;
            if(!armedMode) {
                // turn on the radar mcu (armed mode)
                ret = system("api_test radar-on");
            } else {
                // turn off the radar mcu (disarmed mode)
                ret = system("api_test radar-off");
            }
            DBG_LOG("Armed or Disarmed mode request from Main. armedMode(%d), ret(%d)\n", armedMode, ret);
        }
        break;

        default:
        break;
    }
}

