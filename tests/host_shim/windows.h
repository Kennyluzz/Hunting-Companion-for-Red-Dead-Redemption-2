#pragma once
// Minimal stand-in for <windows.h> so platform-neutral sources can be unit-tested
// on the build host. Only what those sources use; locks are real (std::shared_mutex).
#include <shared_mutex>

struct SRWLOCK { std::shared_mutex* mutex; };
#define SRWLOCK_INIT {new std::shared_mutex}
inline void AcquireSRWLockExclusive(SRWLOCK* lock) { lock->mutex->lock(); }
inline void ReleaseSRWLockExclusive(SRWLOCK* lock) { lock->mutex->unlock(); }
inline void AcquireSRWLockShared(SRWLOCK* lock) { lock->mutex->lock_shared(); }
inline void ReleaseSRWLockShared(SRWLOCK* lock) { lock->mutex->unlock_shared(); }
