#include <stdio.h>
#include <glib.h>
#include "diagnosisMgr.hpp"
#include "log.h"
#include "eventPacket.hpp"
#include "eventSocket.hpp"
#include "DiagServerSocket.hpp"
#include "AccelerometerDataInd.hpp"
#include "AccelerometerStatusInd.hpp"
#include "BatteryLevelInd.hpp"
#include "CameraStatusEvent.hpp"
#include "ModemStatusEvent.hpp"
#include "RadarConfigurationEvent.hpp"
#include "RadarStatusEvent.hpp"
#include "common_util.h"

gboolean handle_keyboard (GIOChannel *source, GIOCondition cond, void *data);

int main(int argc, char **argv)
{
    //CAppCommon *pAppCommon = new CAppCommon(ID_PROCESS_MANAGER, pLoop);
    std::string base = base_name(argv[0]);
    app_name = base.c_str();

    initLog();

    DBG_LOG("started!!\n");

    DBG_LOG("\n");
    DBG_LOG("------------------------------------------------------------\n");
    DBG_LOG("                SENTINEL DIAGNOSIS MANAGER                  \n");
    DBG_LOG("------------------------------------------------------------\n");
    DBG_LOG("Project Name     : Sentinel\n");
    DBG_LOG("Application Name : Diagnosis manager\n");
    DBG_LOG("Software Version : V2.0.0\n");
    DBG_LOG("Processor        : Qualcomm C410\n");
    DBG_LOG("------------------------------------------------------------\n");
    DBG_LOG("\n");

    bool enableKeyInput = false;
    int c;
    while ((c = getopt(argc, argv, "c")) != -1) {
        switch(c) {
            case 'c':
            enableKeyInput = true;
            break;
            default:
            break;
        }
    }

    if(enableKeyInput) {
        GIOChannel *io_stdin = g_io_channel_unix_new (fileno (stdin));
        DBG_LOG("KeyInput enabled!!\n");
        guint event_source_id = g_io_add_watch (io_stdin, G_IO_IN, (GIOFunc)handle_keyboard, NULL);

    }

    diagnosisMgr::getInstance().startApplication();

    //diagnosisMgr::getInstance().stopApplication();
    DBG_LOG("exit main()\n");
}


gboolean handle_keyboard (GIOChannel *source, GIOCondition cond, void *data) {
    gchar *str = NULL;

    if (g_io_channel_read_line (source, &str, NULL, NULL, NULL) != G_IO_STATUS_NORMAL) {
        DBG_LOG("handle_keyboard: %s\n", str);
        return TRUE;
    }

    //DBG_LOG("handle_keyboard : %c\n", g_ascii_tolower (str[0]));
    //DBG_LOG("handle_keyboard : %s\n", str);
    //processCommand(str, data);

    uint16_t eventId = strtol(str, NULL, 16);
    DBG_LOG("eventId :%04x\n", eventId);

    switch(eventId) {
        case CAMERA_STATUS_EVENT: {
            DBG_LOG("send CAMERA_STATUS_EVENT\n");
            auto event = make_shared<CCameraStatusEvent>(eventId, (uint16_t)(sizeof(uint8_t) + sizeof(uint8_t)), (uint8_t)0x00, (uint8_t)0x00);
            CEventSocket::GetInstance()->send_data(event->getPacket().str().c_str(), event->getPacket().str().size());
        }
        break;
        case RADAR_STATUS_EVENT: {
            DBG_LOG("send RADAR_STATUS_EVENT\n");
            auto event = make_shared<CRadarStatusEvent>(eventId, (uint16_t)(sizeof(uint8_t) + sizeof(uint8_t)),(uint8_t)0x00, (uint8_t)0x00);
            CEventSocket::GetInstance()->send_data(event->getPacket().str().c_str(), event->getPacket().str().size());
        }
        break;
        case MODEM_STATUS_EVENT: {
            DBG_LOG("send MODEM_STATUS_EVENT\n");
            auto event = make_shared<CModemStatusEvent>(eventId, (uint16_t)(sizeof(uint8_t) + sizeof(uint16_t) + sizeof(uint8_t)),
                (uint8_t)0x00, (uint16_t)0x00, (uint8_t)0x00);
            CEventSocket::GetInstance()->send_data(event->getPacket().str().c_str(), event->getPacket().str().size());
        }
        break;
        case ACCELEROMETER_STATUS_IND: {
            DBG_LOG("send ACCELEROMETER_STATUS_IND\n");
            auto event = make_shared<CAccelerometerStatusInd>(eventId, (uint16_t)(sizeof(uint8_t) + sizeof(uint8_t)),(uint8_t)0x00, (uint8_t)0x00);
            CEventSocket::GetInstance()->send_data(event->getPacket().str().c_str(), event->getPacket().str().size());
        }
        break;
        case ACCELEROMETER_DATA_IND: {
            DBG_LOG("send ACCELEROMETER_DATA_IND\n");
            auto event = make_shared<CAccelerometerDataInd>(eventId, (int16_t)0x0005, (int16_t)0x0103, (int16_t)0x00A2, (uint8_t)0x00);
            CEventSocket::GetInstance()->send_data(event->getPacket().str().c_str(), event->getPacket().str().size());
        }
        break;
        case BATTERY_LEVEL_IND: {
            DBG_LOG("send BATTERY_LEVEL_IND\n");
            auto event = make_shared<CBatteryLevelInd>(eventId, (int16_t)(sizeof(uint8_t)+sizeof(uint16_t)+sizeof(uint8_t)), (uint8_t)0x01, (uint16_t)0x4C1D, (uint8_t)0x00);
            CEventSocket::GetInstance()->send_data(event->getPacket().str().c_str(), event->getPacket().str().size());
        }
        break;
        default:
            DBG_LOG("invalid event id!!!\n");
        break;
    }

    return TRUE;
}
