#include "pch.h"
#include "CGameManager.h"
#include "jobQueue.h"



CGameManager::CGameManager()
{
	_jobQueue = new ClientEventScheduler();
}

CGameManager::~CGameManager()
{
	ClearQueuedEvents();
	delete _jobQueue;
}

void CGameManager::InitGame()
{
	// 플레이어 초기 위치 셋팅
	_players[0].SetPosition(XMFLOAT3(0,  0, -18));
	_players[1].SetPosition(XMFLOAT3(10, 0, -18));
	_players[2].SetPosition(XMFLOAT3(15, 0, -18));
	_players[3].SetPosition(XMFLOAT3(20, 0, -18));

	// 발전기 위치 셋팅
	_generators[0]._pos = XMFLOAT3(-23.12724f, 1.146619f, 1.814123f);
	_generators[1]._pos = XMFLOAT3(23.08867f, 1.083242f, 3.155997f);
	_generators[2]._pos = XMFLOAT3(0.6774719f, 1.083242f, -23.05909f);

	_generators[0].ResetState();
	_generators[1].ResetState();
	_generators[2].ResetState();

	_gState = GAMESTATE::IN_GAME;
	_history.Clear();
	ClearQueuedEvents();

}

void CGameManager::Update(float eTime)
{
	if (GAMESTATE::IN_GAME != _gState) return;
	_jobQueue->DoTasks();

	for (auto& i : _players) if(!i.m_hide) i.Update(eTime);
}

void CGameManager::LateUpdate(float eTime)
{
	int32 m_activeGenCnt = 0;
	if (GAMESTATE::IN_GAME != _gState) return;
	for (auto& i : _players)if (!i.m_hide) i.LateUpdate(eTime);
	for (auto& i : _generators) if (i._IsActive) m_activeGenCnt++;

	if (m_activeGenCnt >= GENCNT)
	{
		if (!_bExitReady) _bExitReady = true;
	}

    _gState = CheckGameState();
}

void CGameManager::AddEventAfterTime(float after, QueueEvent* qe)
{
	_jobQueue->PushTask(qe, after);
}

void CGameManager::AddEvent(QueueEvent* qe)
{
	_jobQueue->PushTask(qe);
}

void CGameManager::ClearQueuedEvents()
{
	_jobQueue->Clear();
}

GAMESTATE CGameManager::CheckGameState()
{

	int32 crawlCnt = 0;
	int32 escapeCnt = 0;

	for (auto& i : _players)
	{
		if ((int32)PLAYER_BEHAVIOR::CRAWL == i.GetBehavior()) crawlCnt += 1;
		if (true == i.GetEscaped()) escapeCnt += 1;
	}
	if ((PLAYERNUM - 1) == crawlCnt) _gState = GAMESTATE::BOSS_WIN;
	else if ((crawlCnt + escapeCnt) == (PLAYERNUM - 1) && _bExitReady) _gState = GAMESTATE::EMP_WIN;

	return _gState;
}

void CGameManager::ResetGame()
{
	ClearQueuedEvents();
	// 월드 히스토리 초기화
	_history.Clear();
	// 플레이어 상태 초기화
	for (auto& i : _players) i.ResetState();
	// 발전기 상태 초기화
	for (auto& i : _generators) i.ResetState();
	// 플레이어 초기 위치 셋팅
	_players[0].SetPosition(XMFLOAT3(0, 0.25f, -18));
	_players[1].SetPosition(XMFLOAT3(10, 0.25f, -18));
	_players[2].SetPosition(XMFLOAT3(15, 0.25f, -18));
	_players[3].SetPosition(XMFLOAT3(20, 0.25f, -18));
	_bExitReady = false;

	_gState = GAMESTATE::NONE;



}

