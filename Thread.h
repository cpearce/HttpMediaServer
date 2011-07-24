#ifndef __THREAD_H__
#define __THREAD_H__

// Platform independent thread wrapper.

class Runnable {
public:
  virtual void Run() = 0;
};

class Thread {
public:
  static Thread* Create(Runnable* aRunnable);
  virtual ~Thread() {}
  virtual void Join() = 0;
  virtual void Start() = 0;
protected:
  Thread(Runnable* aRunnable)
    : mRunnable(aRunnable) {}
  Runnable* mRunnable;
};

void Thread_Test();

#endif
