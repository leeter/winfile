/********************************************************************

   spinlock.h

   Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License.

********************************************************************/
#pragma once

#include <windows.h>

constexpr unsigned int YIELD_ITERATION = 30; // yield after 30 iterations

// This class acts as a synchronization object similar to a mutex

struct SpinLock
{
	CRITICAL_SECTION m_criticalSection;

public:
	SpinLock()
	{
		InitializeCriticalSectionAndSpinCount(&m_criticalSection, YIELD_ITERATION);
	}

	~SpinLock()
	{
		DeleteCriticalSection(&m_criticalSection);
	}

	void lock();
	void unlock();
};


void SpinLock::lock()
{
	EnterCriticalSection(&m_criticalSection);
}


void SpinLock::unlock()
{
	LeaveCriticalSection(&m_criticalSection);
}


