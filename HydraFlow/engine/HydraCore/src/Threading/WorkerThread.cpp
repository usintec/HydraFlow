#include <HydraCore/Threading/WorkerThread.h>

namespace Hydra {

WorkerThread::WorkerThread(StringView name, EntryPoint entryPoint)
    : m_Name(name)
{
    // Everything inside this lambda runs on the *new* thread, not the
    // thread constructing the WorkerThread.
    m_Thread = std::thread([this, entryPoint = std::move(entryPoint)]() mutable
    {
        // Name the OS thread and record its id before doing any real
        // work, so both are available immediately to anyone inspecting
        // this WorkerThread from another thread.
        Thread::SetCurrentThreadName(m_Name);
        m_ThreadId.store(Thread::GetCurrentThreadId(), std::memory_order_release);
        m_Running.store(true, std::memory_order_release);

        entryPoint();

        m_Running.store(false, std::memory_order_release);
    });
}

WorkerThread::~WorkerThread()
{
    Join();
}

void WorkerThread::Join()
{
    if (m_Thread.joinable())
    {
        m_Thread.join();
    }
}

} // namespace Hydra
