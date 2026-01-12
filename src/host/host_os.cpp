#include "dolphin/os.h"
#include "dolphin/os/OSThread.h"
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

static std::mutex g_reportMutex;
static bool g_reportEnabled = true;

u8 __OSReport_disable = 0;
u8 __OSReport_Error_disable = 0;
u8 __OSReport_Warning_disable = 0;
u8 __OSReport_System_disable = 0;
u8 __OSReport_enable = 1;

static OSThread g_mainThread = {};
static void* g_threadSpecific[OS_THREAD_SPECIFIC_MAX] = {nullptr, nullptr};

extern "C" {

void OSReportInit(void) {}

void OSReportDisable(void) { g_reportEnabled = false; }
void OSReportEnable(void) { g_reportEnabled = true; }
void OSReportForceEnableOff(void) { g_reportEnabled = false; }
void OSReportForceEnableOn(void) { g_reportEnabled = true; }

void OSVReport(const char* msg, va_list list) {
    if (!g_reportEnabled || __OSReport_disable) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_reportMutex);
    std::vfprintf(stderr, msg, list);
}

void OSReport(const char* msg, ...) {
    if (!g_reportEnabled || __OSReport_disable) {
        return;
    }
    va_list list;
    va_start(list, msg);
    OSVReport(msg, list);
    va_end(list);
}

void OSReport_Error(const char* fmt, ...) {
    if (!g_reportEnabled || __OSReport_Error_disable) {
        return;
    }
    va_list list;
    va_start(list, fmt);
    std::lock_guard<std::mutex> lock(g_reportMutex);
    std::vfprintf(stderr, fmt, list);
    va_end(list);
}

void OSReport_Warning(const char* fmt, ...) {
    if (!g_reportEnabled || __OSReport_Warning_disable) {
        return;
    }
    va_list list;
    va_start(list, fmt);
    std::lock_guard<std::mutex> lock(g_reportMutex);
    std::vfprintf(stderr, fmt, list);
    va_end(list);
}

void OSReport_System(const char* fmt, ...) {
    if (!g_reportEnabled || __OSReport_System_disable) {
        return;
    }
    va_list list;
    va_start(list, fmt);
    std::lock_guard<std::mutex> lock(g_reportMutex);
    std::vfprintf(stderr, fmt, list);
    va_end(list);
}

void OSReport_FatalError(const char* fmt, ...) {
    va_list list;
    va_start(list, fmt);
    std::lock_guard<std::mutex> lock(g_reportMutex);
    std::vfprintf(stderr, fmt, list);
    va_end(list);
    std::abort();
}

void OSAttention(const char* msg, ...) {
    va_list list;
    va_start(list, msg);
    std::lock_guard<std::mutex> lock(g_reportMutex);
    std::vfprintf(stderr, msg, list);
    va_end(list);
}

void OSVAttention(const char* fmt, va_list args) {
    std::lock_guard<std::mutex> lock(g_reportMutex);
    std::vfprintf(stderr, fmt, args);
}

void OSPanic(const char* file, int line, const char* msg, ...) {
    std::lock_guard<std::mutex> lock(g_reportMutex);
    std::fprintf(stderr, "PANIC %s:%d ", file, line);
    va_list list;
    va_start(list, msg);
    std::vfprintf(stderr, msg, list);
    va_end(list);
    std::fprintf(stderr, "\n");
    std::abort();
}

void OSFatal(GXColor fg, GXColor bg, const char* msg) {
    (void)fg;
    (void)bg;
    std::lock_guard<std::mutex> lock(g_reportMutex);
    std::fprintf(stderr, "FATAL: %s\n", msg ? msg : "");
    std::abort();
}

u32 OSGetConsoleType(void) {
    return OS_CONSOLE_PC_EMULATOR;
}

u32 OSGetResetCode(void) {
    return 0;
}

void OSInit(void) {}
void OSRegisterVersion(const char* id) { (void)id; }

void* OSGetArenaHi(void) { return nullptr; }
void* OSGetArenaLo(void) { return nullptr; }
void OSSetArenaHi(void* newHi) { (void)newHi; }
void OSSetArenaLo(void* newLo) { (void)newLo; }

static void* alignedAlloc(u32 size, u32 align) {
    if (align == 0) {
        align = 16;
    }
#ifdef _WIN32
    return _aligned_malloc(size, align);
#else
    size_t alignedSize = size;
    if (alignedSize % align != 0) {
        alignedSize += align - (alignedSize % align);
    }
    return std::aligned_alloc(align, alignedSize);
#endif
}

void* OSAllocFromArenaLo(u32 size, u32 align) {
    return alignedAlloc(size, align);
}

void* OSAllocFromArenaHi(u32 size, u32 align) {
    return alignedAlloc(size, align);
}

void* OSAllocFromMEM1ArenaLo(u32 size, u32 align) {
    return alignedAlloc(size, align);
}

u32 OSGetPhysicalMemSize(void) {
    return 0;
}

OSTick OSGetTick(void) {
    return static_cast<OSTick>(OSGetTime());
}

OSTime OSGetTime(void) {
    using namespace std::chrono;
    auto now = steady_clock::now().time_since_epoch();
    return static_cast<OSTime>(duration_cast<microseconds>(now).count());
}

void OSTicksToCalendarTime(OSTime ticks, OSCalendarTime* td) {
    if (!td) {
        return;
    }
    using namespace std::chrono;
    auto us = microseconds(ticks);
    auto now = system_clock::now();
    auto sys = system_clock::time_point(duration_cast<system_clock::duration>(us));
    if (ticks == 0) {
        sys = now;
    }
    std::time_t t = system_clock::to_time_t(sys);
    std::tm tmv = {};
#ifdef _WIN32
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    td->sec = tmv.tm_sec;
    td->min = tmv.tm_min;
    td->hour = tmv.tm_hour;
    td->mday = tmv.tm_mday;
    td->mon = tmv.tm_mon + 1;
    td->year = tmv.tm_year + 1900;
    td->wday = tmv.tm_wday;
    td->yday = tmv.tm_yday;
    td->msec = 0;
    td->usec = 0;
}

OSTime OSCalendarTimeToTicks(OSCalendarTime* td) {
    if (!td) {
        return 0;
    }
    std::tm tmv = {};
    tmv.tm_sec = td->sec;
    tmv.tm_min = td->min;
    tmv.tm_hour = td->hour;
    tmv.tm_mday = td->mday;
    tmv.tm_mon = td->mon - 1;
    tmv.tm_year = td->year - 1900;
    std::time_t t = std::mktime(&tmv);
    return static_cast<OSTime>(t) * 1000000;
}

BOOL OSEnableInterrupts(void) { return TRUE; }
BOOL OSDisableInterrupts(void) { return TRUE; }
BOOL OSRestoreInterrupts(BOOL level) { return level; }

void OSInitThreadQueue(OSThreadQueue* queue) {
    if (!queue) {
        return;
    }
    queue->head = nullptr;
    queue->tail = nullptr;
}

void OSSleepThread(OSThreadQueue* queue) { (void)queue; }
void OSWakeupThread(OSThreadQueue* queue) { (void)queue; }

s32 OSSuspendThread(OSThread* thread) { (void)thread; return 0; }
s32 OSResumeThread(OSThread* thread) { (void)thread; return 0; }

OSThread* OSGetCurrentThread(void) { return &g_mainThread; }

s32 OSEnableScheduler(void) { return 0; }
s32 OSDisableScheduler(void) { return 0; }

void OSCancelThread(OSThread* thread) { (void)thread; }
void OSClearStack(u8 val) { (void)val; }
BOOL OSIsThreadSuspended(OSThread* thread) { (void)thread; return FALSE; }
BOOL OSIsThreadTerminated(OSThread* thread) { (void)thread; return FALSE; }
void OSYieldThread(void) {}

int OSCreateThread(OSThread* thread, void* (*func)(void*), void* param, void* stack, u32 stackSize, OSPriority priority, u16 attr) {
    if (!thread || !func) {
        return 0;
    }
    thread->priority = priority;
    thread->base = priority;
    thread->stackBase = static_cast<u8*>(stack);
    thread->stackEnd = reinterpret_cast<u32*>(static_cast<u8*>(stack) + stackSize);
    thread->attr = attr;
    thread->val = param;
    thread->context.srr0 = reinterpret_cast<u32>(func);
    return 1;
}

void OSExitThread(void* val) { (void)val; std::abort(); }
int OSJoinThread(OSThread* thread, void* val) { (void)thread; (void)val; return 0; }
void OSDetachThread(OSThread* thread) { (void)thread; }

int OSSetThreadPriority(OSThread* thread, OSPriority priority) {
    if (!thread) {
        return 0;
    }
    thread->priority = priority;
    return 1;
}

s32 OSGetThreadPriority(OSThread* thread) {
    if (!thread) {
        return 0;
    }
    return thread->priority;
}

OSThread* OSSetIdleFunction(OSIdleFunction idleFunction, void* param, void* stack, u32 stackSize) {
    (void)idleFunction;
    (void)param;
    (void)stack;
    (void)stackSize;
    return nullptr;
}

OSThread* OSGetIdleFunction(void) { return nullptr; }

s32 OSCheckActiveThreads(void) { return 0; }

void OSSetThreadSpecific(s32 index, void* ptr) {
    if (index < 0 || index >= OS_THREAD_SPECIFIC_MAX) {
        return;
    }
    g_threadSpecific[index] = ptr;
}

void* OSGetThreadSpecific(s32 index) {
    if (index < 0 || index >= OS_THREAD_SPECIFIC_MAX) {
        return nullptr;
    }
    return g_threadSpecific[index];
}

OSSwitchThreadCallback OSSetSwitchThreadCallback(OSSwitchThreadCallback callback) {
    (void)callback;
    return nullptr;
}

}
