/*
   Copyright (c) 2011, Chris Pearce

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   - Neither the name of Chris Pearce nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
   PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE ORGANISATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "Thread.h"
#include "stdio.h"
#include <iostream>
#include "assert.h"


#ifdef _WIN32

#include "windows.h"
#include <process.h>

using namespace std;

class Win32Thread : public Thread {
public:
  Win32Thread(Runnable* aRunnable);
  ~Win32Thread();
  void Join();
  void Start();

private:
  HANDLE mHandle;
  static unsigned __stdcall ThreadEntry(void *aThis);

  enum ThreadState {
    eError = -1,
    eInitialized = 1,
    eStarted = 2,
    eFinished = 3,
  };

  // State tracking, for debug purposes.
  ThreadState mState;
};

Win32Thread::Win32Thread(Runnable* aRunnable) 
  :Thread(aRunnable)
{
  mHandle = (HANDLE)_beginthreadex(
      0, // Security attributes
      0, // Stack size: auto
      ThreadEntry,
      (void*)this,
      CREATE_SUSPENDED,
      0);
  assert(mHandle != 0);
  if (!mHandle) {
    mState = eError;
    cerr << "Can't create thread!\n";
  } else {
    mState = eInitialized;
  }
}

unsigned __stdcall Win32Thread::ThreadEntry(void *aThis) {
  Win32Thread* t = static_cast<Win32Thread*>(aThis);
  t->mState = eStarted;
  t->mRunnable->Run();
  t->mState = eFinished;
  t->mHandle = 0;
  return 0;
}

Win32Thread::~Win32Thread() {
  assert(mState == eFinished);
  CloseHandle(mHandle);
}

void Win32Thread::Join() {
  if (mState == eStarted || mState == eInitialized) {
    WaitForSingleObject(mHandle, INFINITE);
    assert(mState == eFinished);
  }
}

void Win32Thread::Start() {
  if (mState == eInitialized) {
    mState = eStarted;
    ResumeThread(mHandle);
  }
}

Thread* Thread::Create(Runnable *aRunnable) {
  return new Win32Thread(aRunnable);
}

#else
// Assume pthreads are supported...

#include "pthread.h"

class PThread : public Thread {
public:

  static void* PThreadRunner(void* aThread);

  PThread(Runnable* aRunnable) 
    : Thread(aRunnable) {}

  ~PThread() {
  }

  void Join() {
    pthread_join(mThread, NULL);
  }

  void Start() {
    pthread_create(&mThread, 0, &PThreadRunner, this);
  }

  void CallRun() {
    mRunnable->Run();
  }

private:
  pthread_t mThread;
};

void* PThread::PThreadRunner(void* aThread) {
  PThread* p = static_cast<PThread*>(aThread);
  p->CallRun();
}

Thread* Thread::Create(Runnable *aRunnable) {
  return new PThread(aRunnable);
}

#endif // LINUX


// Simple thread to sum the numbers between in range.
class Sum : public Runnable {
public:
  Sum(int start, int end) 
    : mStart(start)
    , mEnd(end)
    , mResult(0)
  { }

  virtual void Run() {
    mResult = 0;
    for (int i=mStart; i<mEnd; i++) {
      mResult += i;
    }
  }

  int mStart, mEnd, mResult;
};

// Tries joining a thread before it's started.
class Waiter : public Runnable {
public:
  Waiter(Thread* aSumThread)
    : mSumThread(aSumThread)
    , mWasRun(false) {}
  virtual void Run() {
    mSumThread->Join();
    mWasRun = true;
  }
  bool mWasRun;
  Thread* mSumThread;
};

#ifdef _DEBUG
void Thread_Test() {
  Sum s1(0,10);
  Sum *s2 = new Sum(10,20);

  Thread* t1 = Thread::Create(&s1);
  Thread* t2 = Thread::Create(s2);
  t1->Start();
  t2->Start();

  t1->Join();
  t2->Join();

  assert(s1.mResult == 45);
  assert(s2->mResult == 145);

  delete s2;
  delete t2;
  delete t1;

  Sum s3(20,30);
  Thread* t3 = Thread::Create(&s3);
  Waiter w(t3);
  Thread* t4 = Thread::Create(&w);
  t4->Start();
  t3->Start();

  t3->Join();
  t4->Join();

  assert(w.mWasRun);

  delete t3;
  delete t4;
}
#endif
