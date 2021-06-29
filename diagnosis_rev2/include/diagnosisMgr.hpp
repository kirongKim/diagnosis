/**
 * @file diagnosisMgr.hpp
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2020-09-07
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#ifndef _DIAGNOSISMGR_HPP
#define _DIAGNOSISMGR_HPP

#include <pthread.h>


#include "singleton.hpp"
#include "DiagServerSocket.hpp"
#include "eventPacket.hpp"
#include "common.h"

class diagnosisMgr : public Singleton<diagnosisMgr>
{
    friend class Singleton<diagnosisMgr>;

public:
    virtual void startApplication() {
        m_pLoop = g_main_loop_new (NULL, FALSE);
        install_sig_handler();
        CTimerUtil::AddSigAction();

        // 1. Modem
        if(!CDiagServerSocket::GetInstance()->Init(handle_routine_modem, MODEM_DIAG_TCP_PORT)) {
            handle_ReconnectModem();
        }

        // 2. Communication manager
        if(!CDiagServerSocket::GetInstance()->Init(handle_routine_commmgr, COMMMGR_DIAG_TCP_PORT)) {
            handle_ReconnectCommmgr();
        }

        // 3. Main application(sentinel)
        if(!CEventSocket::GetInstance()->Init(handle_routine, getServerPort())){
            handle_Reconnect();
        }

        if(pthread_create(&receive_thread, NULL, handle_routine_internal, NULL)) {
            ERROR_LOG("[Critical] thread create error !!\n");
        }
 
        g_main_loop_run(m_pLoop);
    }

public:
    virtual uint16_t getServerPort() { return DIAGNOSIS_TCP_PORT; }
    static void *handle_routine_modem(void *arg);
    static void *handle_routine_commmgr(void *arg);
    static void *handle_routine(void *arg);
    static void *handle_routine_internal(void *arg);

    void handle_ReconnectModem();
    void handle_ReconnectCommmgr();
    void handle_Reconnect();

    static void handler_ReconnectModem();
    static void handler_ReconnectCommmgr();
    static void handler_Reconnect();

    void handle_Event_Modem(shared_ptr<CEventPacket> event);
    void handle_Event_Radar(shared_ptr<CEventPacket> event);
    void handle_Event_Main(shared_ptr<CEventPacket> event);

    static void handler_Event_Modem(shared_ptr<CEventPacket> event);
    static void handler_Event_Radar(shared_ptr<CEventPacket> event);
    static void handler_Event_Main(shared_ptr<CEventPacket> event);

protected:
    diagnosisMgr(){}

private:
    virtual ~diagnosisMgr(){}
    pthread_t receive_thread;

};
#endif  // _DIAGNOSISMGR_HPP
