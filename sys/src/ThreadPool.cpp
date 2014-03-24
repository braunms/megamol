/*
 * ThreadPool.cpp
 *
 * Copyright (C) 2006 - 2008 by Universitaet Stuttgart (VIS). 
 * Alle Rechte vorbehalten.
 */

#include "vislib/ThreadPool.h"

#include <climits>
#ifndef _WIN32
#include <sched.h>
#endif /* _WIN32 */

#include "the/assert.h"
#include "vislib/AutoLock.h"
#include "the/argument_exception.h"
#include "the/invalid_operation_exception.h"
#include "vislib/SystemInformation.h"
#include "the/trace.h"
#include "the/not_supported_exception.h"


/*
 * vislib::sys::ThreadPool::ThreadPool
 */
vislib::sys::ThreadPool::ThreadPool(void) : cntActiveThreads(0), 
        cntTotalThreads(0), evtAllCompleted(true), isQueueOpen(true), 
        semBlockWorker(0l, LONG_MAX) {
    // Nothing to do.
}


/*
 * vislib::sys::ThreadPool::~ThreadPool
 */
vislib::sys::ThreadPool::~ThreadPool(void) {
    this->Terminate(true);
    this->listeners.Clear();
}


/*
 * vislib::sys::ThreadPool::AbortPendingUserWorkItems
 */
size_t vislib::sys::ThreadPool::AbortPendingUserWorkItems(void) {
    size_t retval = 0;
    AutoLock lock(this->lockQueue);

    SingleLinkedList<WorkItem>::Iterator it = this->queue.GetIterator();
    while (it.HasNext() && this->semBlockWorker.TryLock()) {
        WorkItem& workItem = it.Next();
        retval++;
        this->fireUserWorkItemAborted(workItem);
    }

    for (size_t i = 0; i < retval; i++) {
        this->queue.RemoveFirst();
    }

    return retval;
}


/*
 * vislib::sys::ThreadPool::AddListener
 */
void vislib::sys::ThreadPool::AddListener(ThreadPoolListener *listener) {
    THE_ASSERT(listener != NULL);

    this->lockListeners.Lock();
    if ((listener != NULL) && !this->listeners.Contains(listener)) {
        this->listeners.Append(listener);
    }
    this->lockListeners.Unlock();
}


/*
 * vislib::sys::ThreadPool::GetActiveThreads
 */
size_t vislib::sys::ThreadPool::GetActiveThreads(void) const {
    AutoLock lock(this->lockThreadCounters);
    return this->cntActiveThreads;
}


/*
 * vislib::sys::ThreadPool::GetAvailableThreads
 */
size_t vislib::sys::ThreadPool::GetAvailableThreads(void) const {
    AutoLock lock(this->lockThreadCounters);
    return (this->cntTotalThreads - this->cntActiveThreads);
}


/*
 * vislib::sys::ThreadPool::GetTotalThreads
 */
size_t vislib::sys::ThreadPool::GetTotalThreads(void) const {
    AutoLock lock(this->lockThreadCounters);
    return (this->cntTotalThreads);
}


/*
 * vislib::sys::ThreadPool::CountUserWorkItems
 */
size_t vislib::sys::ThreadPool::CountUserWorkItems(void) const {
    AutoLock lock(this->lockQueue);
    return this->queue.Count();
}


/*
 * vislib::sys::ThreadPool::QueueUserWorkItem
 */
void vislib::sys::ThreadPool::QueueUserWorkItem(Runnable *runnable, 
        void *userData, const bool createDefaultThreads) {
    WorkItem workItem;
    workItem.runnable = runnable;
    workItem.runnableFunction = NULL;
    workItem.userData = userData;

    this->queueUserWorkItem(workItem, createDefaultThreads);
}


/*
 * vislib::sys::ThreadPool::QueueUserWorkItem
 */
void vislib::sys::ThreadPool::QueueUserWorkItem(Runnable::Function runnable, 
        void *userData, const bool createDefaultThreads) {
    WorkItem workItem;
    workItem.runnable = NULL;
    workItem.runnableFunction = runnable;
    workItem.userData = userData;

    this->queueUserWorkItem(workItem, createDefaultThreads);
}


/*
 * vislib::sys::ThreadPool::RemoveListener
 */
void vislib::sys::ThreadPool::RemoveListener(ThreadPoolListener *listener) {
    THE_ASSERT(listener != NULL);

    this->lockListeners.Lock();
    this->listeners.RemoveAll(listener);
    this->lockListeners.Unlock();
}



/*
 * vislib::sys::ThreadPool::SetThreadCount
 */
void vislib::sys::ThreadPool::SetThreadCount(const size_t threadCount) {
    AutoLock lock(this->lockThreadCounters);

    if (threadCount < this->cntTotalThreads) {
        throw the::argument_exception("The number of threads in the thread pool "
            "cannot be reduced.", __FILE__, __LINE__);
    }

    for (; this->cntTotalThreads < threadCount; this->cntTotalThreads++) {
        (new RunnableThread<Worker>())->Start(this);
    }
}


/*
 * vislib::sys::ThreadPool::Terminate
 */
void vislib::sys::ThreadPool::Terminate(const bool abortPending) {
    this->lockQueue.Lock();
    this->isQueueOpen = false;

    if (abortPending) {
        this->AbortPendingUserWorkItems();
    }

    this->lockThreadCounters.Lock();
    if (this->cntTotalThreads > 0) {
        this->lockThreadCounters.Unlock();
        this->lockQueue.Unlock();

        this->Wait();

        this->lockQueue.Lock();
        this->lockThreadCounters.Lock();
        THE_ASSERT(this->queue.IsEmpty());
        THE_ASSERT(!this->semBlockWorker.TryLock());

        this->evtAllCompleted.Reset();
        for (size_t i = 0; i < this->cntTotalThreads; i++) {
            this->semBlockWorker.Unlock();
        }

        this->lockThreadCounters.Unlock();
        this->lockQueue.Unlock();

        this->evtAllCompleted.Wait();

    } else {
        this->lockThreadCounters.Unlock();
        this->lockQueue.Unlock();
    }
}


/*
 * vislib::sys::ThreadPool::Worker::Worker
 */
vislib::sys::ThreadPool::Worker::Worker(void) : Runnable(), pool(NULL) {
    // Nothing to do.
}


/*
 * vislib::sys::ThreadPool::Worker::~Worker
 */
vislib::sys::ThreadPool::Worker::~Worker(void) {
    // Nothing to do.
}


/*
 * unsigned int vislib::sys::ThreadPool::Worker::Run
 */
unsigned int vislib::sys::ThreadPool::Worker::Run(void *pool) {
    THE_ASSERT(pool != NULL);
    this->pool = static_cast<ThreadPool *>(pool);

    THE_TRACE(THE_TRCCHL_DEFAULT, THE_TRCLVL_INFO, "Worker thread [%u] started.\n", 
        Thread::CurrentID());

    while (true) {

        /* Wait for work. */
        THE_TRACE(THE_TRCCHL_DEFAULT, THE_TRCLVL_INFO, "ThreadPool thread [%u] is waiting for "
            "work ...\n", Thread::CurrentID());
        this->pool->semBlockWorker.Lock();

        /* Acquire locks. */
        this->pool->lockQueue.Lock();
        this->pool->lockThreadCounters.Lock();

        /*
         * We use an empty queue as trigger for a thread to leave: If we wake a
         * thread and it does not find any work to do, it should exit.
         */
        if (this->pool->queue.IsEmpty()) {
            THE_TRACE(THE_TRCCHL_DEFAULT, THE_TRCLVL_INFO, "ThreadPool thread [%u] is "
                "exiting ...\n", Thread::CurrentID());
            if (--this->pool->cntTotalThreads == 0) {
                this->pool->evtAllCompleted.Set();
            }
            this->pool->lockThreadCounters.Unlock();
            this->pool->lockQueue.Unlock();
            delete this;
            return 0;
        }

        /* Get the work item and mark thread as active. */
        THE_ASSERT(!this->pool->queue.IsEmpty());
        WorkItem workItem = this->pool->queue.First();
        this->pool->queue.RemoveFirst();
        this->pool->cntActiveThreads++;

        /* Release locks while working. */
        this->pool->lockThreadCounters.Unlock();
        this->pool->lockQueue.Unlock();

        /* Do the work. */
        THE_TRACE(THE_TRCCHL_DEFAULT, THE_TRCLVL_INFO, "ThreadPool thread [%u] is working ...\n",
            Thread::CurrentID());
        THE_ASSERT((workItem.runnable != NULL) 
            || (workItem.runnableFunction != NULL));
        unsigned int exitCode = (workItem.runnable != NULL)
            ? workItem.runnable->Run(workItem.userData)
            : workItem.runnableFunction(workItem.userData);
        THE_TRACE(THE_TRCCHL_DEFAULT, THE_TRCLVL_INFO, "ThreadPool thread [%u] completed work "
            "item with exit code %u\n", Thread::CurrentID(), exitCode);

        this->pool->fireUserWorkItemCompleted(workItem, exitCode);

        /* Mark the thread as inactive and signal event if necessary. */
        this->pool->lockQueue.Lock();
        this->pool->lockThreadCounters.Lock();
        if ((--this->pool->cntActiveThreads == 0)   // SFX. Must be first!
                && this->pool->queue.IsEmpty()) {
            this->pool->evtAllCompleted.Set();
        }
        this->pool->lockThreadCounters.Unlock();
        this->pool->lockQueue.Unlock();
    }
}


/*
 * vislib::sys::ThreadPool::ThreadPool
 */
vislib::sys::ThreadPool::ThreadPool(const ThreadPool& rhs) {
    throw the::not_supported_exception("ThreadPool::ThreadPool", __FILE__, 
        __LINE__);
}


/*
 * vislib::sys::ThreadPool::fireUserWorkItemAborted
 */
void vislib::sys::ThreadPool::fireUserWorkItemAborted(WorkItem& workItem) {
    AutoLock lock(this->lockListeners);

    SingleLinkedList<ThreadPoolListener *>::Iterator it 
        = this->listeners.GetIterator();
    if (workItem.runnable != NULL) {
        while (it.HasNext()) {
            it.Next()->OnUserWorkItemAborted(*this, workItem.runnable, 
                workItem.userData);
        }
    } else {
        THE_ASSERT(workItem.runnableFunction != NULL);
        while (it.HasNext()) {
            it.Next()->OnUserWorkItemAborted(*this, workItem.runnableFunction,
                workItem.userData);
        }
    }
}


/*
 * vislib::sys::ThreadPool::fireUserWorkItemCompleted
 */
void vislib::sys::ThreadPool::fireUserWorkItemCompleted(WorkItem& workItem,
                                                        const unsigned int exitCode) {
    AutoLock lock(this->lockListeners);

    SingleLinkedList<ThreadPoolListener *>::Iterator it 
        = this->listeners.GetIterator();
    if (workItem.runnable != NULL) {
        while (it.HasNext()) {
            it.Next()->OnUserWorkItemCompleted(*this, workItem.runnable, 
                workItem.userData, exitCode);
        }
    } else {
        THE_ASSERT(workItem.runnableFunction != NULL);
        while (it.HasNext()) {
            it.Next()->OnUserWorkItemCompleted(*this, workItem.runnableFunction,
                workItem.userData, exitCode);
        }
    }
}


/*
 * vislib::sys::ThreadPool::queueUserWorkItem
 */
void vislib::sys::ThreadPool::queueUserWorkItem(WorkItem& workItem,
        const bool createDefaultThreads) {
    /* Sanity checks. */
    if ((workItem.runnable == NULL) && (workItem.runnableFunction == NULL)) {
        throw the::argument_exception("workItem", __FILE__, __LINE__);
    }
    if ((workItem.runnable != NULL) && (workItem.runnableFunction != NULL)) {
        throw the::argument_exception("workItem", __FILE__, __LINE__);
    }

    /* Add work item to queue. */
    this->lockQueue.Lock();
    if (this->isQueueOpen) {
        this->queue.Append(workItem);
        this->evtAllCompleted.Reset();  // Signal unfinished work.
        this->semBlockWorker.Unlock();  // Wake workers.
        this->lockQueue.Unlock();
#ifndef _WIN32
        // Linux has the most crap scheduler I have ever seen ...
        sched_yield();
#endif /* _WIN32 */
    } else {
        this->lockQueue.Unlock();
        throw the::invalid_operation_exception("The user work item queue has been closed, "
            "because the thread pool is being terminated.", __FILE__, __LINE__);
    }

    /* Create threads if requested. */
    this->lockThreadCounters.Lock();
    if (createDefaultThreads && (this->cntTotalThreads < 1)) {
        this->SetThreadCount(SystemInformation::ProcessorCount());
    }
    this->lockThreadCounters.Unlock();
}


/*
 * vislib::sys::ThreadPool::operator =
 */
vislib::sys::ThreadPool& vislib::sys::ThreadPool::operator =(
        const ThreadPool& rhs) {
    if (this != &rhs) {
        throw the::argument_exception("rhs", __FILE__, __LINE__);
    }

    return *this;
}
