#pragma once

#include "windows.h"

class Thread {
public:
  Thread();
  virtual ~Thread();

  virtual void Run() = 0;
  virtual void WaitToFinish(); // Join.
  virtual void Start();

  static void Test();
private:
  HANDLE mHandle;
  DWORD mId;
  static DWORD WINAPI ThreadEntry(void *aThis);

  enum ThreadState {
    eError = -1,
    eInitialized = 1,
    eStarted = 2,
    eFinished = 3,
  };

  // State tracking, for debug purposes.
  ThreadState mState;
};


// Copied from: http://www.relisoft.com/win32/active.html
// Simple mutex.
class Mutex
{
    friend class Lock;
public:
    Mutex () { InitializeCriticalSection (& _critSection); }
    ~Mutex () { DeleteCriticalSection (& _critSection); }
    void Acquire ()
    {
        EnterCriticalSection (& _critSection);
    }
    void Release ()
    {
        LeaveCriticalSection (& _critSection);
    }
private:
    CRITICAL_SECTION _critSection;
};

// Copied from: http://www.relisoft.com/win32/active.html
// Stack-based mutex acquire/release.
class Lock
{
public:
    // Acquire the state of the semaphore
    Lock ( Mutex & mutex )
        : _mutex(mutex)
    {
        _mutex.Acquire();
    }
    // Release the state of the semaphore
    ~Lock ()
    {
        _mutex.Release();
    }
private:
    Mutex & _mutex;
};

// Copied from: http://www.relisoft.com/win32/active.html
// An Event is a signalling device that threads use to communicate with
// each other. You embed an Event in your active object. Then you make
// the captive thread wait on it until some other thread releases it.
// Remember however that if your captive thread waits on a event it can't 
// be terminated.
class Event
{
public:
    Event ()
    {
        // start in non-signaled state (red light)
        // auto reset after every Wait
        _handle = CreateEvent (0, FALSE, FALSE, 0);
    }

    ~Event ()
    {
        CloseHandle (_handle);
    }

    // put into signaled state
    void Release () { SetEvent (_handle); } // Notify
    void Wait ()
    {
        // Wait until event is in signaled (green) state
        WaitForSingleObject (_handle, INFINITE);
    }
    operator HANDLE () { return _handle; }
private:
    HANDLE _handle;
};
