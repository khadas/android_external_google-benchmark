// ----------------------------------------------------------------------
// CycleClock
//    A CycleClock tells you the current time in Cycles.  The "time"
//    is actually time since power-on.  This is like time() but doesn't
//    involve a system call and is much more precise.
//
// NOTE: Not all cpu/platform/kernel combinations guarantee that this
// clock increments at a constant rate or is synchronized across all logical
// cpus in a system.
//
// If you need the above guarantees, please consider using a different
// API. There are efforts to provide an interface which provides a millisecond
// granularity and implemented as a memory read. A memory read is generally
// cheaper than the CycleClock for many architectures.
//
// Also, in some out of order CPU implementations, the CycleClock is not
// serializing. So if you're trying to count at cycles granularity, your
// data might be inaccurate due to out of order instruction execution.
// ----------------------------------------------------------------------

#ifndef BENCHMARK_CYCLECLOCK_H_
#define BENCHMARK_CYCLECLOCK_H_

#include <stdint.h>

#if defined(OS_MACOSX)
# include <mach/mach_time.h>
#endif
// For MSVC, we want to use '_asm rdtsc' when possible (since it works
// with even ancient MSVC compilers), and when not possible the
// __rdtsc intrinsic, declared in <intrin.h>.  Unfortunately, in some
// environments, <windows.h> and <intrin.h> have conflicting
// declarations of some other intrinsics, breaking compilation.
// Therefore, we simply declare __rdtsc ourselves. See also
// http://connect.microsoft.com/VisualStudio/feedback/details/262047
#if defined(COMPILER_MSVC) && !defined(_M_IX86)
extern "C" uint64_t __rdtsc();
#pragma intrinsic(__rdtsc)
#endif
#include <sys/time.h>

// NOTE: only i386 and x86_64 have been well tested.
// PPC, sparc, alpha, and ia64 are based on
//    http://peter.kuscsik.com/wordpress/?p=14
// with modifications by m3b.  See also
//    https://setisvn.ssl.berkeley.edu/svn/lib/fftw-3.0.1/kernel/cycle.h
namespace benchmark {
namespace cycleclock {
  // This should return the number of cycles since power-on.  Thread-safe.
  static inline int64_t Now() {
#if defined(OS_MACOSX)
    // this goes at the top because we need ALL Macs, regardless of
    // architecture, to return the number of "mach time units" that
    // have passed since startup.  See sysinfo.cc where
    // InitializeSystemInfo() sets the supposed cpu clock frequency of
    // macs to the number of mach time units per second, not actual
    // CPU clock frequency (which can change in the face of CPU
    // frequency scaling).  Also note that when the Mac sleeps, this
    // counter pauses; it does not continue counting, nor does it
    // reset to zero.
    return mach_absolute_time();
#elif defined(__i386__)
    int64_t ret;
    __asm__ volatile ("rdtsc" : "=A" (ret) );
    return ret;
#elif defined(__x86_64__) || defined(__amd64__)
    uint64_t low, high;
    __asm__ volatile ("rdtsc" : "=a" (low), "=d" (high));
    return (high << 32) | low;
#elif defined(__powerpc__) || defined(__ppc__)
    // This returns a time-base, which is not always precisely a cycle-count.
    int64_t tbl, tbu0, tbu1;
    asm("mftbu %0" : "=r" (tbu0));
    asm("mftb  %0" : "=r" (tbl));
    asm("mftbu %0" : "=r" (tbu1));
    tbl &= -static_cast<int64>(tbu0 == tbu1);
    // high 32 bits in tbu1; low 32 bits in tbl  (tbu0 is garbage)
    return (tbu1 << 32) | tbl;
#elif defined(__sparc__)
    int64_t tick;
    asm(".byte 0x83, 0x41, 0x00, 0x00");
    asm("mov   %%g1, %0" : "=r" (tick));
    return tick;
#elif defined(__ia64__)
    int64_t itc;
    asm("mov %0 = ar.itc" : "=r" (itc));
    return itc;
#elif defined(COMPILER_MSVC) && defined(_M_IX86)
    // Older MSVC compilers (like 7.x) don't seem to support the
    // __rdtsc intrinsic properly, so I prefer to use _asm instead
    // when I know it will work.  Otherwise, I'll use __rdtsc and hope
    // the code is being compiled with a non-ancient compiler.
    _asm rdtsc
#elif defined(COMPILER_MSVC)
    return __rdtsc();
#elif defined(ARMV3)
#if defined(ARMV6)  // V6 is the earliest arch that has a standard cyclecount
    uint32_t pmccntr;
    uint32_t pmuseren;
    uint32_t pmcntenset;
    // Read the user mode perf monitor counter access permissions.
    asm("mrc p15, 0, %0, c9, c14, 0" : "=r" (pmuseren));
    if (pmuseren & 1) {  // Allows reading perfmon counters for user mode code.
      asm("mrc p15, 0, %0, c9, c12, 1" : "=r" (pmcntenset));
      if (pmcntenset & 0x80000000ul) {  // Is it counting?
        asm("mrc p15, 0, %0, c9, c13, 0" : "=r" (pmccntr));
        // The counter is set up to count every 64th cycle
        return static_cast<int64>(pmccntr) * 64;  // Should optimize to << 6
      }
    }
#endif
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return static_cast<int64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
#elif defined(__mips__)
    // mips apparently only allows rdtsc for superusers, so we fall
    // back to gettimeofday.  It's possible clock_gettime would be better.
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return static_cast<int64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
#else
// The soft failover to a generic implementation is automatic only for ARM.
// For other platforms the developer is expected to make an attempt to create
// a fast implementation and use generic version if nothing better is available.
#error You need to define CycleTimer for your OS and CPU
#endif
  }
}  // end namespace cycleclock
}  // end namespace benchmark

#endif  // BENCHMARK_CYCLECLOCK_H_