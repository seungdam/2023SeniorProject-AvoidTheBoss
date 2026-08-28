#include "pch.h"
#include "Timer.h"

CTimer::CTimer()
{
	::QueryPerformanceFrequency((LARGE_INTEGER*)&_nPerformanceFrequencyPerSec);
	::QueryPerformanceCounter((LARGE_INTEGER*)&_nLastPerformanceCounter);
	_fTimeScale = 1.0 / (double)_nPerformanceFrequencyPerSec;

	_nBasePerformanceCounter = _nLastPerformanceCounter;
	_nPausedPerformanceCounter = 0;
	_nStopPerformanceCounter = 0;

	_nSampleCount = 0;
	_nCurrentFrameRate = 0;
	_nFramesPerSecond = 0;
	_fFPSTimeElapsed = 0.0f;
}



void CTimer::Start()
{
	__int64 nPerformanceCounter;
	::QueryPerformanceCounter((LARGE_INTEGER*)&nPerformanceCounter);
	if (_bStopped)
	{
		_nPausedPerformanceCounter += (nPerformanceCounter - _nStopPerformanceCounter);
		_nLastPerformanceCounter = nPerformanceCounter;
		_nStopPerformanceCounter = 0;
		_bStopped = false;
	}
}

void CTimer::Stop()
{
	if (!_bStopped)
	{
		::QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&_nStopPerformanceCounter));
		_bStopped = true;
	}
}

void CTimer::Reset()
{
	auto nPerformanceCounter = (__int64)0;
	::QueryPerformanceCounter((LARGE_INTEGER*)&nPerformanceCounter);

	_nBasePerformanceCounter = nPerformanceCounter;
	_nLastPerformanceCounter = nPerformanceCounter;
	_nStopPerformanceCounter = 0;

	_bStopped = false;
}

void CTimer::Tick(float fLockFPS)
{
	if (_bStopped)
	{
		_fTimeElapsed = 0.0f;
		return;
	}

	//마지막으로 이 함수를 호출한 이후 경과한 시간을 계산한다.
	::QueryPerformanceCounter((LARGE_INTEGER*)&_nCurrentPerformanceCounter);
	auto fTimeElapsed = static_cast<float>((_nCurrentPerformanceCounter - _nLastPerformanceCounter) * _fTimeScale);

	//현재 시간을 m_nLastTime에 저장한다.
	_nLastPerformanceCounter = _nCurrentPerformanceCounter;

	/* 마지막 프레임 처리 시간과 현재 프레임 처리 시간의 차이가 1초보다 작으면 현재 프레임 처리 시간
을 m_fFrameTime[0]에 저장한다. */
	if (fabsf(fTimeElapsed - _fTimeElapsed) < 1.0f) // 오차가 적다면
	{
		// 배열 값들을 한칸 씩 미룬다.
		::memmove(&_fFrameTime[1], _fFrameTime, (MAX_SAMPLE_COUNT - 1) * sizeof(float));
		_fFrameTime[0] = fTimeElapsed;
		if (_nSampleCount < MAX_SAMPLE_COUNT) _nSampleCount++;
	}

	//초당 프레임 수를 1 증가시키고 현재 프레임 처리 시간을 누적하여 저장한다.
	_nFramesPerSecond++;
	_nWorldFrame++;
	_fFPSTimeElapsed += fTimeElapsed;
	if (_fFPSTimeElapsed > 1.0f) // 1초가 넘어가면 프레임 카운트 0
	{

		_nCurrentFrameRate = _nFramesPerSecond; // 60fps 대비 얼마나 나오는가?
		_nFramesPerSecond = 0; //월드 프레임은 계속 유지하도록 한다.
		_fFPSTimeElapsed = 0.0f;
	}


	//누적된 프레임 처리 시간의 평균을 구하여 프레임 처리 시간을 구한다.
	_fTimeElapsed = 0.0f;
	for (auto i = 0; i < _nSampleCount; i++)
	{
		_fTimeElapsed += _fFrameTime[i];
	}
	if (_nSampleCount > 0)
	{
		_fTimeElapsed /= _nSampleCount;
	}
}

unsigned long  CTimer::GetFrameRate(LPTSTR lpszString, int nCharacters)
{
	//현재 프레임 레이트를 문자열로 변환하여 lpszString 버퍼에 쓰고 “ FPS”와 결합한다.
	if (lpszString)
	{
		_itow_s(_nCurrentFrameRate, lpszString, nCharacters, 10);
		wcscat_s(lpszString, nCharacters, _T(" FPS)"));
	}
	return(_nCurrentFrameRate);
}

float CTimer::GetTimeElapsed()
{
	return(_fTimeElapsed);
}

float CTimer::GetTotalTime()
{
	if (_bStopped)
	{
		return(float(((_nStopPerformanceCounter - _nPausedPerformanceCounter) - _nBasePerformanceCounter) * _fTimeScale));
	}

	return(float(((_nCurrentPerformanceCounter - _nPausedPerformanceCounter) - _nBasePerformanceCounter) * _fTimeScale));
}
