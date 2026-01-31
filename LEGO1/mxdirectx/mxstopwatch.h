#ifndef _MxStopWatch_h
#define _MxStopWatch_h

#include "assert.h"

#include <limits.h> // ULONG_MAX
#include <math.h>
#include <mortar/mortar_stdinc.h>
#include <mortar/mortar_timer.h>
#ifdef MINIWIN
#include "miniwin/windows.h"
#else
#include <windows.h>
#endif

//////////////////////////////////////////////////////////////////////////////
//
// MxStopWatch
//
// NOTE:	MxStopWatch measures elapsed (wall clock) time.
//

#define HUGE_VAL_IMMEDIATE 1.7976931348623157e+308

// SIZE 0x18
class MxStopWatch {
public:
	MxStopWatch();
	~MxStopWatch() {}

	void Start();
	void Stop();
	void Reset();

	double ElapsedSeconds() const;

protected:
	uint64_t TicksPerSeconds() const;

private:
	uint64_t m_startTick; // 0x00
	// ??? when we provide LARGE_INTEGER arithmetic, use a
	//     LARGE_INTEGER m_elapsedTicks rather than m_elapsedSeconds
	double m_elapsedSeconds;    // 0x0c
	uint64_t m_ticksPerSeconds; // 0x14
};

// FUNCTION: BETA10 0x100d8ba0
inline MxStopWatch::MxStopWatch()
{
	Reset();
	m_ticksPerSeconds = MORTAR_GetPerformanceFrequency();
}

// FUNCTION: BETA10 0x100d8be0
inline void MxStopWatch::Start()
{
	m_startTick = MORTAR_GetPerformanceCounter();
}

// FUNCTION: BETA10 0x100d8f50
inline void MxStopWatch::Stop()
{
	uint64_t endTick;

	endTick = MORTAR_GetPerformanceCounter();

	m_elapsedSeconds = (double) (endTick - m_startTick) / (double) m_ticksPerSeconds;
}

// FUNCTION: BETA10 0x100d8c10
inline void MxStopWatch::Reset()
{
	m_startTick = 0;
	m_elapsedSeconds = 0.;
}

// FUNCTION: BETA10 0x100d8c60
inline uint64_t MxStopWatch::TicksPerSeconds() const
{
	uint64_t ticksPerSeconds;

	ticksPerSeconds = MORTAR_GetPerformanceFrequency();
	assert(ticksPerSeconds);

	return ticksPerSeconds;
}

// FUNCTION: BETA10 0x100d9020
inline double MxStopWatch::ElapsedSeconds() const
{
	return m_elapsedSeconds;
}

// SYNTHETIC: LEGO1 0x100a6fc0
// SYNTHETIC: BETA10 0x100d8e70
// MxStopWatch::~MxStopWatch

//////////////////////////////////////////////////////////////////////////////
//
// MxFrequencyMeter
//

// SIZE 0x20
class MxFrequencyMeter {
public:
	MxFrequencyMeter();

	void StartOperation();
	void EndOperation();
	double Frequency() const;
	void Reset();

	unsigned int OperationCount() const;
	double ElapsedSeconds() const;

	void IncreaseOperationCount(unsigned int);

private:
	unsigned int m_operationCount; // 0x00
	MxStopWatch m_stopWatch;       // 0x08
};

//////////////////////////////////////////////////////////////////////////////
//
// MxFrequencyMeter implementation
//

// FUNCTION: BETA10 0x1017dd80
inline MxFrequencyMeter::MxFrequencyMeter() : m_operationCount(0)
{
}

// FUNCTION: BETA10 0x1017deb0
inline void MxFrequencyMeter::StartOperation()
{
	m_stopWatch.Start();
}

// FUNCTION: BETA10 0x1017df10
inline void MxFrequencyMeter::EndOperation()
{
	m_stopWatch.Stop();
	m_operationCount++;
}

inline double MxFrequencyMeter::Frequency() const
{
	double elapsedSeconds = m_stopWatch.ElapsedSeconds();

	if (elapsedSeconds > 0) {
		return m_operationCount / elapsedSeconds;
	}
	else {
		if (m_operationCount) {
			// operations performed - no time elapsed
			return HUGE_VAL;
		}
		else {
			// no operations performed - no time elapsed
			return 0;
		}
	}
}

// FUNCTION: BETA10 0x1017dee0
inline void MxFrequencyMeter::Reset()
{
	m_stopWatch.Reset();
	m_operationCount = 0;
}

inline unsigned int MxFrequencyMeter::OperationCount() const
{
	return m_operationCount;
}

// FUNCTION: BETA10 0x1017df40
inline void MxFrequencyMeter::IncreaseOperationCount(unsigned int delta)
{
	m_operationCount += delta;
}

// FUNCTION: BETA10 0x1017df60
inline double MxFrequencyMeter::ElapsedSeconds() const
{
	return m_stopWatch.ElapsedSeconds();
}

// SYNTHETIC: LEGO1 0x100abd10
// SYNTHETIC: BETA10 0x1017de40
// MxFrequencyMeter::~MxFrequencyMeter

#endif /* _MxStopWatch_h */
