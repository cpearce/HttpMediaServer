#include "Thread.h"
#include "stdio.h"
#include <iostream>
#include "assert.h"

using namespace std;

Thread::Thread() {
  mHandle = CreateThread (
      0, // Security attributes
      0, // Stack size
      ThreadEntry,
      (void*)this,
      CREATE_SUSPENDED,
      &mId);
  assert(mHandle != 0);
  if (!mHandle) {
    mState = eError;
    cerr << "Can't create thread!\n";
  } else {
    mState = eInitialized;
  }
}


DWORD WINAPI Thread::ThreadEntry(void *aThis) {
  Thread* t = static_cast<Thread*>(aThis);
  t->mState = eStarted;
  t->Run();
  t->mState = eFinished;
  t->mHandle = 0;
  return 0;
}


Thread::~Thread() {
  assert(mState == eFinished);
  CloseHandle(mHandle);
}


void Thread::WaitToFinish() {
  if (mState == eStarted || mState == eInitialized) {
    WaitForSingleObject(mHandle, INFINITE);
    assert(mState == eFinished);
  }
}


void Thread::Start() {
  if (mState == eInitialized) {
    mState = eStarted;
    ResumeThread(mHandle);
  }
}


// Simple thread to sum the numbers between in range.
class SumThread : public Thread {
public:
  SumThread(int start, int end) 
    : mStart(start)
    , mEnd(end)
    , mResult(0)
  { }

  virtual ~SumThread() { }
  virtual void Run() {
    mResult = 0;
    for (int i=mStart; i<mEnd; i++) {
      mResult += i;
    }
  }

  int mStart, mEnd, mResult;
};


// Trys joining a thread before it's started.
class WaiterThread : public Thread {
public:
  WaiterThread(SumThread &sumThread)
    : mSumThread(sumThread)
    , mWasRun(false) {}
  virtual ~WaiterThread() {}
  virtual void Run() {
    mSumThread.WaitToFinish();
    mWasRun = true;
  }
  bool mWasRun;
  SumThread& mSumThread;
};



void Thread::Test() {
  SumThread t1(0,10);
  SumThread *t2 = new SumThread(10,20);
  t1.Start();
  t2->Start();

  t1.WaitToFinish();
  t2->WaitToFinish();

  assert(t1.mResult == 45);
  assert(t2->mResult == 145);

  delete t2;

  SumThread t3(20,30);
  WaiterThread t4(t3);
  t4.Start();
  t3.Start();

  t3.WaitToFinish();
  t4.WaitToFinish();

  assert(t4.mWasRun);
}
