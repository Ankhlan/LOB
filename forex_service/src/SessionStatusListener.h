/*
 * SessionStatusListener.h - Using Windows CRITICAL_SECTION for compatibility
 */
#pragma once
#include "ForexConnect.h"
#include <windows.h>
#include <atomic>
#include <iostream>

class SessionStatusListener : public IO2GSessionStatus
{
private:
    std::atomic<long> m_refCount{1};
    CRITICAL_SECTION m_cs;
    HANDLE m_event;
    IO2GSessionStatus::O2GSessionStatus m_status;
    bool m_connected;

public:
    SessionStatusListener()
        : m_status(IO2GSessionStatus::Disconnected)
        , m_connected(false)
    {
        InitializeCriticalSection(&m_cs);
        m_event = CreateEvent(NULL, TRUE, FALSE, NULL);  // Manual reset event
        std::cout << "[SSL] constructor" << std::endl;
    }

    virtual ~SessionStatusListener()
    {
        std::cout << "[SSL] destructor" << std::endl;
        CloseHandle(m_event);
        DeleteCriticalSection(&m_cs);
    }

    // IAddRef implementation
    virtual long addRef()
    {
        return m_refCount.fetch_add(1, std::memory_order_relaxed) + 1;
    }

    virtual long release()
    {
        long refCount = m_refCount.fetch_sub(1, std::memory_order_acq_rel) - 1;
        if (refCount == 0)
            delete this;
        return refCount;
    }

    // IO2GSessionStatus implementation
    virtual void onSessionStatusChanged(IO2GSessionStatus::O2GSessionStatus status)
    {
        std::cout << "[SSL] onSessionStatusChanged status=" << (int)status << std::endl;
        EnterCriticalSection(&m_cs);
        m_status = status;

        switch (status)
        {
        case IO2GSessionStatus::Connected:
            std::cout << "[SSL] Connected!" << std::endl;
            m_connected = true;
            SetEvent(m_event);
            break;

        case IO2GSessionStatus::Disconnected:
        case IO2GSessionStatus::SessionLost:
            std::cout << "[SSL] Disconnected" << std::endl;
            m_connected = false;
            SetEvent(m_event);
            break;

        case IO2GSessionStatus::TradingSessionRequested:
            std::cout << "[SSL] TradingSessionRequested" << std::endl;
            SetEvent(m_event);
            break;
            
        default:
            std::cout << "[SSL] Other status: " << (int)status << std::endl;
            break;
        }
        LeaveCriticalSection(&m_cs);
    }

    virtual void onLoginFailed(const char *error)
    {
        std::cout << "[SSL] onLoginFailed: " << (error ? error : "unknown") << std::endl;
        EnterCriticalSection(&m_cs);
        SetEvent(m_event);
        LeaveCriticalSection(&m_cs);
    }

    // Helper methods
    bool waitEvents(int timeoutMs = 30000)
    {
        std::cout << "[SSL] waitEvents..." << std::endl;
        DWORD result = WaitForSingleObject(m_event, (DWORD)timeoutMs);
        std::cout << "[SSL] waitEvents result=" << result << std::endl;
        return (result == WAIT_OBJECT_0);
    }

    void reset()
    {
        std::cout << "[SSL] reset" << std::endl;
        ResetEvent(m_event);
    }

    bool isConnected() const
    {
        return m_connected;
    }

    IO2GSessionStatus::O2GSessionStatus getStatus() const
    {
        return m_status;
    }
};
