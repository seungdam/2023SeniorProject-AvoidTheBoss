#pragma once
const ULONG MAX_SAMPLE_COUNT = 50; // 50회의 프레임 처리시간을 누적하여 평균한다.

class CTimer
{
public:
	CTimer();
	~CTimer() = default;

	void Tick(float fLockFPS = 0.0f); // 타이머 시간 갱신
	void Start();
	void Stop();
	void Reset();

	unsigned long GetFrameRate(LPTSTR lpszString = NULL, int nCharacters = 0); // 프레임 레이트 반환
	float GetTimeElapsed(); // 프레임 평균 경과 시간 반환
	float GetTotalTime();
public:
	unsigned long	_nWorldFrame;   //초당 프레임
private:
	double			_fTimeScale; //Scale Counter의 양 1초 단위로 바꾸기 위해서 생성
	float			_fTimeElapsed; // 평균 프레임 처리 시간 1 / Frame 초

	__int64			_nBasePerformanceCounter;
	__int64			_nPausedPerformanceCounter;
	__int64			_nStopPerformanceCounter;
	__int64			_nCurrentPerformanceCounter;
	__int64			_nLastPerformanceCounter;

	__int64			_nPerformanceFrequencyPerSec; //컴퓨터의 Performance Frequency

	float			_fFrameTime[MAX_SAMPLE_COUNT]; //프레임 시간을 누적하기 위한 배열
	ULONG			_nSampleCount; //샘플링 할 프레임 개수

	unsigned long	_nCurrentFrameRate;  //1초 동안 나온 프레임 수
	unsigned long	_nFramesPerSecond;   //초당 프레임

	float			_fFPSTimeElapsed;    //프레임 레이트 계산 소요 시간

	bool			_bStopped;
};

