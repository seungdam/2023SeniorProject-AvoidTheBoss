#include "../Platform/pch.h"
#include "../UIManager.h"
#include "../Core/SceneId.h"

#include "../Platform/DXSampleHelper.h"

#include <d2d1_1.h>
#include <wincodec.h>
//
//const float atb::client::ui::MaxRescueGaugeWidth = atb::client::config::DefaultWindowWidth / 4.0f;
//
//const float atb::client::ui::ProfileOffsetX = atb::client::config::DefaultWindowWidth * 0.01;
//const float atb::client::ui::ProfileOffsetY = atb::client::config::DefaultWindowHeight * 0.1;
//
//const float atb::client::ui::ProfileWidth = atb::client::config::DefaultWindowWidth * 0.1;
//const float atb::client::ui::ProfileHeight = atb::client::config::DefaultWindowHeight * 0.1;
//
//const float atb::client::ui::LargeProfileOffsetY = atb::client::ui::ProfileOffsetY * 7;
//const float atb::client::ui::LargeProfileWidth = atb::client::config::DefaultWindowWidth * 0.2;
//const float atb::client::ui::LargeProfileHeight = atb::client::config::DefaultWindowHeight * 0.2;
//
//const float atb::client::ui::StatusWidth = atb::client::ui::ProfileWidth * 0.8;
//const float atb::client::ui::StatusHeight = atb::client::ui::ProfileHeight * 0.8;

D2D1_RECT_F MakeLayoutRect(float cx, float cy , float width, float height)
{
    return D2D1_RECT_F{ cx - width / 2.0f , cy - height / 2.0f , cx + width / 2.0f , cy + height / 2.0f };
}

D2D1_RECT_F MakeLayoutRectByCorner(float left, float top, float width, float height)
{
    return D2D1_RECT_F{ left , top , left + width, top + height};
}

bool ContainsPoint(const D2D1_RECT_F& rect, const POINT& point) noexcept
{
    return rect.left <= point.x && point.x <= rect.right &&
        rect.top <= point.y && point.y <= rect.bottom;
}

UIManager::UIManager(
    ID2D1DeviceContext2* d2dContext,
    IDWriteFactory* writeFactory,
    UINT nWidth,
    UINT nHeight)
    : _pd2dDeviceContext(d2dContext), _pd2dWriteFactory(writeFactory)
{
	if (!_pd2dDeviceContext || !_pd2dWriteFactory)
	{
		throw std::invalid_argument("UIManager requires initialized D2D services");
	}
	wndWidth = static_cast<float>(nWidth);
    wndHeight = static_cast<float>(nHeight);
    try
    {
        InitializeResources();
    }
    catch (...)
    {
        ReleaseResources();
        throw;
    }
}

UIManager::~UIManager()
{
    ReleaseResources();
}

void UIManager::ShowLoginFeedback(const LoginFeedback feedback) noexcept
{
    const auto visibleIndex = static_cast<std::size_t>(feedback);
	for (std::size_t index = 0; index < std::size(_loginPopUpBtns); ++index)
	{
		_loginPopUpBtns[index].m_hide = index != visibleIndex;
	}
}

std::optional<UIManager::LoginFeedback> UIManager::TickLoginFeedback(const float elapsedSeconds) noexcept
{
    for (std::size_t index = 0; index < std::size(_loginPopUpBtns); ++index)
    {
        auto& feedback = _loginPopUpBtns[index];
		if (feedback.m_hide)
		{
			continue;
		}

		feedback.animTime -= elapsedSeconds;
        if (feedback.animTime <= 0.0f)
        {
            feedback.animTime = 1.0f;
            feedback.m_hide = true;
            return static_cast<LoginFeedback>(index);
        }
        break;
    }
    return std::nullopt;
}

bool UIManager::HitTest(const UiHitTarget target, const POINT& point) const noexcept
{
    const D2D1_RECT_F* rect = nullptr;
    switch (target)
    {
    case UiHitTarget::TitleId:
        rect = &_idpwTextBlocks[0].m_d2dLayoutRect;
        break;
    case UiHitTarget::TitlePassword:
        rect = &_idpwTextBlocks[1].m_d2dLayoutRect;
        break;
    case UiHitTarget::TitleLogin:
        rect = &_titleSceneBtns[0].d2dLayoutRect;
        break;
    case UiHitTarget::TitleRegister:
        rect = &_titleSceneBtns[2].d2dLayoutRect;
        break;
    case UiHitTarget::TitleQuit:
        rect = &_titleSceneBtns[1].d2dLayoutRect;
        break;
    case UiHitTarget::LobbyEnter:
        rect = &_lobbySceneBtns[0].d2dLayoutRect;
        break;
    case UiHitTarget::LobbyCreate:
        rect = &_lobbySceneBtns[1].d2dLayoutRect;
        break;
    case UiHitTarget::LobbyLogout:
        rect = &_lobbySceneBtns[2].d2dLayoutRect;
        break;
    case UiHitTarget::RoomReady:
        rect = &_roomBtns[0].d2dLayoutRect;
        break;
    case UiHitTarget::RoomLeave:
        rect = &_roomBtns[1].d2dLayoutRect;
        break;
    }
    return rect && ContainsPoint(*rect, point);
}

bool UIManager::TrySelectLobbyRoomSlot(const int32 slot, const POINT& point) noexcept
{
	if (slot < 0 || slot >= LobbyRoomSlotCount)
	{
		return false;
	}
	if (!ContainsPoint(_roomListLayout[slot], point))
	{
		return false;
	}
	_selectedRow = slot;
    return true;
}

void UIManager::AppendCredential(const int32 field, const wchar_t character)
{
    auto& text = _idpwTextBlocks[field == 1 ? 1 : 0].m_pstrText;
	if (text.length() <= 10)
	{
		text.push_back(character);
	}
}

void UIManager::BackspaceCredential(const int32 field)
{
    auto& text = _idpwTextBlocks[field == 1 ? 1 : 0].m_pstrText;
	if (!text.empty())
	{
		text.erase(text.length() - 1, 1);
	}
}

const std::wstring& UIManager::CredentialText(const int32 field) const noexcept
{
    return _idpwTextBlocks[field == 1 ? 1 : 0].m_pstrText;
}



ID2D1Bitmap1* UIManager::LoadPngFromFile(const wchar_t* filePath)
{
	const auto assetPath = GetAssetPath(filePath);
	ComPtr<IWICImagingFactory> factory;
    ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;
    ComPtr<IWICFormatConverter> converter;
    ComPtr<ID2D1Bitmap1> bitmap;

    ThrowIfFailed(CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory)));
    ThrowIfFailed(factory->CreateDecoderFromFilename(
        assetPath.c_str(),
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnDemand,
        &decoder));
    ThrowIfFailed(decoder->GetFrame(0, &frame));
    ThrowIfFailed(factory->CreateFormatConverter(&converter));
    ThrowIfFailed(converter->Initialize(
        frame.Get(),
        GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0f,
        WICBitmapPaletteTypeCustom));
    ThrowIfFailed(_pd2dDeviceContext->CreateBitmapFromWicBitmap(
        converter.Get(),
        nullptr,
        &bitmap));

    return bitmap.Detach();
}


void UIManager::UpdateRoomTextBlocks(UINT nIndex,const WCHAR* pstrUIText, const D2D1_RECT_F& pd2dLayoutRect, bool hide)
{
    _roomListTextBlocks[nIndex].m_pstrText.erase();
    _roomListTextBlocks[nIndex].m_pstrText.append(pstrUIText);

    _roomListTextBlocks[nIndex].m_hide = hide;
}

void UIManager::UpdateLobbySceneUI(const LobbyUiSnapshot& snapshot)
{
    WCHAR temp[3];
    D2D1_RECT_F newRect{ 0,0,0,0 };

    for (std::size_t index = 0; index < snapshot.rooms.size(); ++index)
    {
        if (!snapshot.rooms[index])
        {
            UpdateRoomTextBlocks(static_cast<UINT>(index), L"", newRect, true);
            continue;
        }

        const auto& room = *snapshot.rooms[index];
        std::wstring newText = L"ROOM [";
        _itow_s(room.roomNumber, temp, 10);
        temp[2] = '\0';
        newText.append(temp);
        newText.append(L"]");
        temp[0] = L'\0';
        _itow_s(room.memberCount, temp, 10);
        temp[2] = L'\0';
        newText.append(temp);
        newText.append(L"/4");
        UpdateRoomTextBlocks(static_cast<UINT>(index), newText.c_str(), newRect, false);
    }
}

void UIManager::UpdateRoomSceneUI(const RoomUiSnapshot& snapshot)
{
    _roomUiSnapshot = snapshot;
}

void UIManager::UpdateResultSceneUI(const ResultUiSnapshot& snapshot)
{
    _resultUiSnapshot = snapshot;
}


void UIManager::ReleaseResources()
{
	auto release = [](auto *&resource)
	{
		if (resource)
		{
			resource->Release();
		}
		resource = nullptr;
	};

	for (auto &item : _backGroundBitmaps)
	{
		release(item.resource);
	}
	for (auto &item : _loginPopUpBtns)
	{
		release(item.resource);
	}
	for (auto &item : _titleSceneBtns)
	{
		release(item.resource);
	}
	for (auto &item : _lobbySceneBtns)
	{
		release(item.resource);
	}
	for (auto &item : _roomBtns)
	{
		release(item.resource);
	}
	for (auto &item : _readyBtns)
	{
		release(item.resource);
	}
	for (auto &item : _userCardBtns)
	{
		release(item.resource);
	}
	for (auto &item : _charProfileBtns)
	{
		release(item.resource);
	}
	for (auto &item : _charStatusBitmaps)
	{
		release(item);
	}
	release(_HpBitmap);
    release(_crossHeadIcon.resource);
	for (auto &item : _attackedEffects)
	{
		release(item.resource);
	}
	for (auto &item : _generateBtns)
	{
		release(item.resource);
	}
	release(_rescueIcon.resource);

	for (auto &item : _idpwTextBlocks)
	{
		release(item.m_pd2dTextBrush);
	}
	release(_pTitleTextFormat);
    release(_pLobbySceneTextFormat);
    release(_redBrush);
    release(_grayBrush);
    release(_blackBrush);
    release(_whiteBrush);
    release(_greenBrush);

    _pd2dDeviceContext = nullptr;
    _pd2dWriteFactory = nullptr;
}

void UIManager::DrawOtherSceneBackGround(int32 Scene)
{
    switch (Scene)
    {
    case atb::SceneIndex(atb::SceneId::Title):
    case atb::SceneIndex(atb::SceneId::Lobby):
    case atb::SceneIndex(atb::SceneId::Room):
        _pd2dDeviceContext->DrawBitmap(_backGroundBitmaps[Scene].resource, D2D1_RECT_F{ 0,0,wndWidth,wndHeight });
        break;
    case atb::SceneIndex(atb::SceneId::InGame):
        break;
    case atb::SceneIndex(atb::SceneId::Result):
        if (_resultUiSnapshot.bossWon)
        {
            _pd2dDeviceContext->DrawBitmap(_backGroundBitmaps[3].resource, D2D1_RECT_F{ 0,0,wndWidth,wndHeight });
        }
		else
		{
			_pd2dDeviceContext->DrawBitmap(_backGroundBitmaps[4].resource, D2D1_RECT_F{0, 0, wndWidth, wndHeight});
		}

		break;
    }
}

void UIManager::DrawOtherSceneUI(int32 Scene,int32 idx)
{
    if (Scene == atb::SceneIndex(atb::SceneId::Title)) // 타이틀 씬
    {
        _pd2dDeviceContext->DrawBitmap(_titleSceneBtns[0].resource, _titleSceneBtns[0].d2dLayoutRect);
        _pd2dDeviceContext->DrawBitmap(_titleSceneBtns[1].resource, _titleSceneBtns[1].d2dLayoutRect);
        _pd2dDeviceContext->DrawBitmap(_titleSceneBtns[2].resource, _titleSceneBtns[2].d2dLayoutRect);

		for (int i = 0; i < 3; ++i)
		{
			if (!_loginPopUpBtns[i].m_hide)
			{
				_pd2dDeviceContext->DrawBitmap(_loginPopUpBtns[i].resource, _loginPopUpBtns[i].d2dLayoutRect,
				                               _loginPopUpBtns[i].animTime);
			}
		}
	}
    else if (Scene == atb::SceneIndex(atb::SceneId::Lobby)) // 로비 씬
    {
        _pd2dDeviceContext->DrawBitmap(_lobbySceneBtns[0].resource, _lobbySceneBtns[0].d2dLayoutRect);
        _pd2dDeviceContext->DrawBitmap(_lobbySceneBtns[1].resource, _lobbySceneBtns[1].d2dLayoutRect);
        _pd2dDeviceContext->DrawBitmap(_lobbySceneBtns[2].resource, _lobbySceneBtns[2].d2dLayoutRect);
    }
    else if (Scene == atb::SceneIndex(atb::SceneId::Room)) // 게임 룸 씬
    {
        _pd2dDeviceContext->DrawBitmap(_roomBtns[0].resource, _roomBtns[0].d2dLayoutRect);
        _pd2dDeviceContext->DrawBitmap(_roomBtns[1].resource, _roomBtns[1].d2dLayoutRect);

        for (std::size_t index = 0; index < _roomUiSnapshot.members.size(); ++index)
        {
            const auto& member = _roomUiSnapshot.members[index];
			if (member.occupied)
			{
				_pd2dDeviceContext->DrawBitmap(
                    _userCardBtns[index].resource, _userCardBtns[index].d2dLayoutRect);
			}

			_pd2dDeviceContext->DrawRectangle(
                _readyBtns[index].d2dLayoutRect, _blackBrush, 6.0f);
			if (member.ready)
			{
				_pd2dDeviceContext->DrawBitmap(
                    _readyBtns[index].resource, _readyBtns[index].d2dLayoutRect);
			}
		}
    }
}

void UIManager::DrawOtherSceneUITextBlock(int32 Scene)
{

    if (Scene == atb::SceneIndex(atb::SceneId::Title))
    {
        _pd2dDeviceContext->FillRectangle(_idpwTextBlocks[0].m_d2dLayoutRect, _grayBrush);
        _pd2dDeviceContext->FillRectangle(_idpwTextBlocks[1].m_d2dLayoutRect, _grayBrush);

        _pd2dDeviceContext->DrawRectangle(_idpwTextBlocks[0].m_d2dLayoutRect, _blackBrush,4.0);
        _pd2dDeviceContext->DrawRectangle(_idpwTextBlocks[1].m_d2dLayoutRect, _blackBrush,4.0);

		if (!_idpwTextBlocks[0].m_hide)
		{
			_pd2dDeviceContext->DrawText(
			    _idpwTextBlocks[0].m_pstrText.c_str(), (UINT)wcslen(_idpwTextBlocks[0].m_pstrText.c_str()),
			    _idpwTextBlocks[0].m_pdwFormat, _idpwTextBlocks[0].m_d2dLayoutRect, _idpwTextBlocks[0].m_pd2dTextBrush);
		}

		if (!_idpwTextBlocks[1].m_hide)
		{
			_pd2dDeviceContext->DrawText(
			    _idpwTextBlocks[1].m_pstrText.c_str(), (UINT)wcslen(_idpwTextBlocks[1].m_pstrText.c_str()),
			    _idpwTextBlocks[1].m_pdwFormat, _idpwTextBlocks[1].m_d2dLayoutRect, _idpwTextBlocks[1].m_pd2dTextBrush);
		}
	}
    else if (Scene == atb::SceneIndex(atb::SceneId::Lobby))
    {
        for (int i = 0; i < LobbyRoomSlotCount; ++i)
        {

            _pd2dDeviceContext->DrawRectangle(_roomListLayout[i], _blackBrush, 4.0f);
			if (!_roomListTextBlocks[i].m_hide)
			{
				_pd2dDeviceContext->DrawText(
				    _roomListTextBlocks[i].m_pstrText.c_str(), (UINT)wcslen(_roomListTextBlocks[i].m_pstrText.c_str()),
				    _roomListTextBlocks[i].m_pdwFormat, _roomListTextBlocks[i].m_d2dLayoutRect, _blackBrush);
			}
		}
		if (_selectedRow >= 0)
		{
			_pd2dDeviceContext->DrawRectangle(_roomListLayout[_selectedRow], _redBrush, 4.0f);
		}
	}
}

void UIManager::InitGameSceneUI(const GameUiSnapshot& snapshot)
{
	if (!snapshot.localPlayerIndex || *snapshot.localPlayerIndex >= atb::client::ui::GamePlayerCount)
	{
		return;
	}
	const int32 playerIndex = static_cast<int32>(*snapshot.localPlayerIndex);

	for (auto &profile : _charProfileBtns)
	{
		profile.m_hide = false;
	}
	for (auto &status : m_CharStatus)
	{
		status.m_hide = false;
	}
	_charProfileBtns[playerIndex].m_hide = true;
	if (playerIndex != 0)
	{
		_charProfileBtns[0].m_hide = true;
	}
	for (auto &i : _generateBtns)
	{
		i.m_hide = true;
	}
	// 플레이어 상태 UI 출력
    switch (playerIndex)
    {
        case 0:
        {
           // 사장님은 3명 모두 출력
            for (int i = 1; i < 4; ++i)
            {
                _charProfileBtns[i].d2dLayoutRect =
                    MakeLayoutRectByCorner(atb::client::ui::ProfileOffsetX, atb::client::ui::ProfileOffsetY + atb::client::ui::ProfileOffsetY * (i - 1),
                        atb::client::ui::ProfileWidth, atb::client::ui::ProfileHeight);
            }
            m_CharStatus[0].d2dLayoutRect =
                MakeLayoutRectByCorner(atb::client::ui::ProfileOffsetX + atb::client::ui::ProfileWidth, atb::client::ui::ProfileOffsetY, atb::client::ui::StatusWidth, atb::client::ui::StatusHeight);
            m_CharStatus[1].d2dLayoutRect =
                MakeLayoutRectByCorner(atb::client::ui::ProfileOffsetX + atb::client::ui::ProfileWidth, atb::client::ui::ProfileOffsetY * 2, atb::client::ui::StatusWidth, atb::client::ui::StatusHeight);
            m_CharStatus[2].d2dLayoutRect =
                MakeLayoutRectByCorner(atb::client::ui::ProfileOffsetX + atb::client::ui::ProfileWidth, atb::client::ui::ProfileOffsetY * 3, atb::client::ui::StatusWidth, atb::client::ui::StatusHeight);
            break;
        }
        // 나머지는 자신을 제외한 2명만 출력
        case 1:
        {

            _charProfileBtns[2].d2dLayoutRect =
                MakeLayoutRectByCorner(atb::client::ui::ProfileOffsetX, atb::client::ui::ProfileOffsetY, atb::client::ui::ProfileWidth, atb::client::ui::ProfileHeight);

            _charProfileBtns[3].d2dLayoutRect =
                MakeLayoutRectByCorner(atb::client::ui::ProfileOffsetX, atb::client::ui::ProfileOffsetY * 2, atb::client::ui::ProfileWidth, atb::client::ui::ProfileHeight);

            // 1 2 3 --> 0 1 2
            m_CharStatus[0].m_hide = true;
            m_CharStatus[1].d2dLayoutRect =
                MakeLayoutRectByCorner(atb::client::ui::ProfileOffsetX + atb::client::ui::ProfileWidth, atb::client::ui::ProfileOffsetY, atb::client::ui::StatusWidth, atb::client::ui::StatusHeight);
            m_CharStatus[2].d2dLayoutRect =
                MakeLayoutRectByCorner(atb::client::ui::ProfileOffsetX + atb::client::ui::ProfileWidth, atb::client::ui::ProfileOffsetY * 2, atb::client::ui::StatusWidth, atb::client::ui::StatusHeight);
        }
        break;
        case 2:
        {

            _charProfileBtns[1].d2dLayoutRect =
                MakeLayoutRectByCorner(atb::client::ui::ProfileOffsetX, atb::client::ui::ProfileOffsetY, atb::client::ui::ProfileWidth, atb::client::ui::ProfileHeight);
            _charProfileBtns[3].d2dLayoutRect =
                MakeLayoutRectByCorner(atb::client::ui::ProfileOffsetX, atb::client::ui::ProfileOffsetY * 2, atb::client::ui::ProfileWidth, atb::client::ui::ProfileHeight);

            m_CharStatus[1].m_hide = true;
            m_CharStatus[0].d2dLayoutRect =
                MakeLayoutRectByCorner(atb::client::ui::ProfileOffsetX + atb::client::ui::ProfileWidth, atb::client::ui::ProfileOffsetY, atb::client::ui::StatusWidth, atb::client::ui::StatusHeight);
            m_CharStatus[2].d2dLayoutRect =
                MakeLayoutRectByCorner(atb::client::ui::ProfileOffsetX + atb::client::ui::ProfileWidth, atb::client::ui::ProfileOffsetY * 2, atb::client::ui::StatusWidth, atb::client::ui::StatusHeight);
        }
        break;

        case 3:
        {
            _charProfileBtns[1].d2dLayoutRect =
                MakeLayoutRectByCorner(atb::client::ui::ProfileOffsetX, atb::client::ui::ProfileOffsetY, atb::client::ui::ProfileWidth, atb::client::ui::ProfileHeight);
            _charProfileBtns[2].d2dLayoutRect =
                MakeLayoutRectByCorner(atb::client::ui::ProfileOffsetX, atb::client::ui::ProfileOffsetY * 2, atb::client::ui::ProfileWidth, atb::client::ui::ProfileHeight);

            m_CharStatus[2].m_hide = true;

            m_CharStatus[0].d2dLayoutRect =
                MakeLayoutRectByCorner(atb::client::ui::ProfileOffsetX + atb::client::ui::ProfileWidth, atb::client::ui::ProfileOffsetY, atb::client::ui::StatusWidth, atb::client::ui::StatusHeight);
            m_CharStatus[1].d2dLayoutRect =
                MakeLayoutRectByCorner(atb::client::ui::ProfileOffsetX + atb::client::ui::ProfileWidth, atb::client::ui::ProfileOffsetY * 2, atb::client::ui::StatusWidth, atb::client::ui::StatusHeight);
        }
        break;
    }

    for (int i = 0; i < 3; ++i)
    {
        m_CharStatus[i].resource =  _charStatusBitmaps[0]; // normal status로 시작
    }

    m_myProfileLayout = MakeLayoutRectByCorner(atb::client::ui::ProfileOffsetX,atb::client::ui::LargeProfileOffsetY, atb::client::ui::LargeProfileWidth, atb::client::ui::LargeProfileHeight);

    // 플레이어 hp 출력

}

void UIManager::UpdateGameSceneUI(const GameUiSnapshot& snapshot)
{
	if (!snapshot.localPlayerIndex || *snapshot.localPlayerIndex >= atb::client::ui::GamePlayerCount)
	{
		return;
	}
	const std::size_t playerIndex = *snapshot.localPlayerIndex;

    for (std::size_t index = 1; index < snapshot.players.size(); ++index)
    {
		if (!snapshot.players[index] || index == playerIndex)
		{
			continue;
		}

		const int health = snapshot.players[index]->health;
        m_CharStatus[index - 1].resource = health == HealthIconCount
            ? _charStatusBitmaps[0]
            : health == 0 ? _charStatusBitmaps[2] : _charStatusBitmaps[1];
    }

	if (!snapshot.localEmployee || !snapshot.players[playerIndex])
	{
		return;
	}
	const EmployeeUiSnapshot& employee = *snapshot.localEmployee;

    for (int index = 0; index < _attackedUICount; ++index)
    {
        if (employee.invincible)
        {
            constexpr float maxOpacity = 0.5f;
            constexpr float bulletHoleOpacityExtra = 1.7f;
            constexpr float baseOpacityExtra = 0.5f;
            constexpr float outlineOpacityExtra = 1.0f;
            if (index == 4)
            {
                _attackedEffects[index].m_hide = false;
                _attackedEffectOpacity[index] = employee.uiCooldown <= 0.5f
                    ? (employee.uiCooldown / 0.5f) * maxOpacity * bulletHoleOpacityExtra
                    : ((1.0f - employee.uiCooldown) / 0.5f) * maxOpacity * bulletHoleOpacityExtra;
            }
            else if (index == 3)
            {
                if (employee.uiCooldown > 0.65f)
                {
                    _attackedEffects[index].m_hide = false;
                    _attackedEffectOpacity[index] = (employee.uiCooldown / 0.35f) * maxOpacity * bulletHoleOpacityExtra;
                }
                else if (employee.uiCooldown >= 0.3f)
                {
                    _attackedEffects[index].m_hide = false;
                    _attackedEffectOpacity[index] = (1.0f - employee.uiCooldown / 0.35f) * maxOpacity * bulletHoleOpacityExtra;
                }
            }
            else
            {
                _attackedEffects[index].m_hide = false;
            }

			if (employee.uiCooldown >= 0.0f && index < 3)
			{
				_attackedEffectOpacity[index] = employee.uiCooldown * maxOpacity;
			}
			if (index == 0)
			{
				_attackedEffectOpacity[index] *= baseOpacityExtra;
			}
			else if (index == 1)
			{
				_attackedEffectOpacity[index] *= outlineOpacityExtra;
			}
		}
        else
        {
            _attackedEffects[index].m_hide = true;
        }
    }

	for (auto &hp : m_HPUi)
	{
		hp.m_hide = true;
	}
	for (int index = 0; index < snapshot.players[playerIndex]->health; ++index)
	{
		m_HPUi[index].m_hide = false;
	}

	_generateBtns[21].m_hide = !employee.inGeneratorArea;
	if (!employee.inGeneratorArea)
	{
		for (int index = 0; index < 21; ++index)
		{
			_generateBtns[index].m_hide = true;
		}
	}

	if (employee.generatorInteractionActive && employee.generatorGauge)
    {
        const int gauge = static_cast<int>(*employee.generatorGauge);
		if (gauge < 1)
		{
			_generateBtns[0].m_hide = false;
		}
		else if (((gauge % 100) / 5) <= 19)
		{
			_generateBtns[((gauge % 100) / 5) + 1].m_hide = false;
		}
	}
    else
    {
		for (int index = 0; index < 21; ++index)
		{
			_generateBtns[index].m_hide = true;
		}
	}

	_rescueIcon.m_hide = !employee.rescueTargetAvailable;
    m_RescueGuage.m_hide = true;
    if (employee.rescueGauge && *employee.rescueGauge >= 0.0f && *employee.rescueGauge <= 100.0f)
    {
        const float dx = (*employee.rescueGauge * 5.8f / atb::client::ui::MaxRescueGaugeWidth) * 100.0f;
        m_RescueGuage.m_hide = false;
        m_RescueGuage.d2dLayoutRect[1] = MakeLayoutRect(
            atb::client::ui::CenterX + (-atb::client::ui::MaxRescueGaugeWidth + dx) / 2.0f, atb::client::ui::CenterY, dx, 50.0f);
    }
    else
    {
        m_RescueGuage.d2dLayoutRect[1] = m_RescueGuage.d2dLayoutRect[0];
    }
}

void UIManager::DrawGameSceneUI(int32 Scene, int32 localPlayerIndex)
{
	if (3 != Scene || localPlayerIndex < 0 || localPlayerIndex >= PLAYERNUM)
	{
		return;
	}
	// 고정 렌더링
    // 다른 캐릭터 초상화 , 내 캐릭터 초상화
    for (auto i : _charProfileBtns)
    {
		if (!i.m_hide)
		{
			_pd2dDeviceContext->DrawBitmap(i.resource, i.d2dLayoutRect, FullOpacity);
		}
	}

	for (auto i : _generateBtns)
	{
		if (!i.m_hide)
		{
			_pd2dDeviceContext->DrawBitmap(i.resource, i.d2dLayoutRect, FullOpacity);
		}
	}

	for (auto i : m_CharStatus)
	{
		if (!i.m_hide)
		{
			_pd2dDeviceContext->DrawBitmap(i.resource, i.d2dLayoutRect, FullOpacity);
		}
	}

	// 큰 초상화 그리기
    _pd2dDeviceContext->DrawBitmap(_charProfileBtns[localPlayerIndex].resource, m_myProfileLayout, FullOpacity);
    // HP 그리기
    if (localPlayerIndex != 0)
    {
		for (auto i : m_HPUi)
		{
			if (!i.m_hide)
			{
				_pd2dDeviceContext->DrawBitmap(i.resource, i.d2dLayoutRect, FullOpacity);
			}
		}
		for (int i = 0; i < _attackedUICount; i++)
        {
			if (!_attackedEffects[i].m_hide)
			{
				_pd2dDeviceContext->DrawBitmap(_attackedEffects[i].resource, _attackedEffects[i].d2dLayoutRect,
				                               _attackedEffectOpacity[i] * FullOpacity);
			}
		}
		if (!_rescueIcon.m_hide)
		{
			_pd2dDeviceContext->DrawBitmap(_rescueIcon.resource, _rescueIcon.d2dLayoutRect, FullOpacity);
		}

		if (!m_RescueGuage.m_hide)
        {
            _pd2dDeviceContext->FillRectangle(m_RescueGuage.d2dLayoutRect[1], _greenBrush);
            _pd2dDeviceContext->DrawRectangle(m_RescueGuage.d2dLayoutRect[0], _blackBrush, 5.0f);
        }
    }

	if (localPlayerIndex == 0)
	{
		_pd2dDeviceContext->DrawBitmap(_crossHeadIcon.resource, _crossHeadIcon.d2dLayoutRect,
		                               _crossHeadOpacity * FullOpacity);
	}
}

ID2D1SolidColorBrush* UIManager::CreateBrush(D2D1::ColorF d2dColor)
{
    ComPtr<ID2D1SolidColorBrush> brush;
    ThrowIfFailed(_pd2dDeviceContext->CreateSolidColorBrush(d2dColor, &brush));
    return brush.Detach();
}

IDWriteTextFormat* UIManager::CreateTextFormat(const WCHAR* pszFontName, float fFontSize)
{
    ComPtr<IDWriteTextFormat> format;
    ThrowIfFailed(_pd2dWriteFactory->CreateTextFormat(
        pszFontName,
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        fFontSize,
        L"en-us",
        &format));
    ThrowIfFailed(format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER));
    ThrowIfFailed(format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR));
    return format.Detach();
}

void UIManager::InitializeResources()
{
    // 브러시들
    _redBrush = CreateBrush(D2D1::ColorF::Red);
    _grayBrush = CreateBrush(D2D1::ColorF::Gray);
    _blackBrush = CreateBrush(D2D1::ColorF::Black);
    _greenBrush = CreateBrush(D2D1::ColorF::Green);
    // 폰트
    _pTitleTextFormat = CreateTextFormat(L"맑은 고딕", 40);
    _pLobbySceneTextFormat = CreateTextFormat(L"맑은 고딕", 40);
    // 배경 리소스들
    _backGroundBitmaps[0].resource = LoadPngFromFile(L"UI/Title.png");
    _backGroundBitmaps[1].resource = LoadPngFromFile(L"UI/Lobby.png");
    _backGroundBitmaps[2].resource = LoadPngFromFile(L"UI/Game.png");

    _backGroundBitmaps[3].resource = LoadPngFromFile(L"UI/Boss_Result.png");
    _backGroundBitmaps[4].resource = LoadPngFromFile(L"UI/Emp_Result.png");

    // 타이틀 씬에 필요한 버튼
    _titleSceneBtns[0].resource      = LoadPngFromFile(L"UI/Title_Start.png");
    _titleSceneBtns[1].resource      = LoadPngFromFile(L"UI/Title_Quit.png");
    _titleSceneBtns[2].resource      = LoadPngFromFile(L"UI/Title_Register.png");

    _titleSceneBtns[0].d2dLayoutRect = MakeLayoutRect(atb::client::ui::CenterX + atb::client::ui::TitleButtonXOffset, atb::client::ui::CenterY + atb::client::ui::TitleButtonYOffset, 200, 50);
    _titleSceneBtns[1].d2dLayoutRect = MakeLayoutRect(atb::client::ui::CenterX , atb::client::ui::CenterY + atb::client::config::DefaultWindowHeight / 2.5f, 200, 50);
    _titleSceneBtns[2].d2dLayoutRect = MakeLayoutRect(atb::client::ui::CenterX - atb::client::ui::TitleButtonXOffset, atb::client::ui::CenterY + atb::client::ui::TitleButtonYOffset, 200, 50);
    // ID / PW 입력 창
    _idpwTextBlocks[0].m_pd2dTextBrush = CreateBrush(D2D1::ColorF::White);
    _idpwTextBlocks[0].m_pdwFormat = _pTitleTextFormat;
    _idpwTextBlocks[0].m_pstrText = L"";


    _idpwTextBlocks[1].m_pd2dTextBrush = CreateBrush(D2D1::ColorF::White);
    _idpwTextBlocks[1].m_pdwFormat = _pTitleTextFormat;
    _idpwTextBlocks[1].m_pstrText = L"";

    _idpwTextBlocks[0].m_d2dLayoutRect = MakeLayoutRect(atb::client::ui::CenterX, atb::client::ui::CenterY + atb::client::config::DefaultWindowHeight / 4.0f,  400, atb::client::ui::FontSize);
    _idpwTextBlocks[1].m_d2dLayoutRect = MakeLayoutRect(atb::client::ui::CenterX, atb::client::ui::CenterY + atb::client::config::DefaultWindowHeight / 3.0f , 400, atb::client::ui::FontSize);

    _loginPopUpBtns[0].resource = LoadPngFromFile(L"UI/LOGIN_OK.png");
    _loginPopUpBtns[1].resource = LoadPngFromFile(L"UI/LOGIN_FAIL.png");
    _loginPopUpBtns[2].resource = LoadPngFromFile(L"UI/REG.png");

    for (int i = 0; i < 3; ++i)
    {
        _loginPopUpBtns[i].d2dLayoutRect = MakeLayoutRect(atb::client::ui::CenterX, atb::client::ui::CenterY, 300, 100);
        _loginPopUpBtns[i].m_hide = true;
    }


    // 로비 씬에 필요한 버튼
    _lobbySceneBtns[0].resource = LoadPngFromFile(L"UI/Enter_Room.png");
    _lobbySceneBtns[1].resource = LoadPngFromFile(L"UI/Create_Room.png");
    _lobbySceneBtns[2].resource = LoadPngFromFile(L"UI/Quit_Lobby.png");

    _lobbySceneBtns[0].d2dLayoutRect = MakeLayoutRectByCorner(atb::client::ui::LobbyButtonXOffset,        atb::client::ui::LobbyButtonYOffset, atb::client::config::DefaultWindowWidth / 3.0f, atb::client::config::DefaultWindowHeight / 4.0);
    _lobbySceneBtns[1].d2dLayoutRect = MakeLayoutRectByCorner(0,                           atb::client::ui::LobbyButtonYOffset, atb::client::config::DefaultWindowWidth / 3.0f, atb::client::config::DefaultWindowHeight / 4.0);
    _lobbySceneBtns[2].d2dLayoutRect = MakeLayoutRectByCorner(atb::client::ui::LobbyButtonXOffset * 2.0f, atb::client::ui::LobbyButtonYOffset, atb::client::config::DefaultWindowWidth / 3.0f, atb::client::config::DefaultWindowHeight / 4.0);


    _roomBtns[0].resource = LoadPngFromFile(L"UI/Ready_Game.png");
    _roomBtns[1].resource = LoadPngFromFile(L"UI/Quit_Game.png");
    _roomBtns[0].d2dLayoutRect = MakeLayoutRectByCorner(atb::client::ui::GameRoomButtonXOffset, atb::client::ui::GameRoomButtonYOffset, atb::client::config::DefaultWindowWidth / 5.0f, atb::client::config::DefaultWindowHeight / 7.0f);
    _roomBtns[1].d2dLayoutRect = MakeLayoutRectByCorner(atb::client::ui::GameRoomButtonXOffset + (atb::client::config::DefaultWindowWidth / 5.0f), atb::client::ui::GameRoomButtonYOffset, atb::client::config::DefaultWindowWidth / 5.0f, atb::client::config::DefaultWindowHeight / 7.0f);

    _readyBtns[0].resource = LoadPngFromFile(L"UI/Ready.png");
    _readyBtns[1].resource = LoadPngFromFile(L"UI/Ready2.png");
    _readyBtns[2].resource = LoadPngFromFile(L"UI/Ready3.png");
    _readyBtns[3].resource = LoadPngFromFile(L"UI/Ready4.png");

    _userCardBtns[0].resource = LoadPngFromFile(L"UI/READY_CARD1.png");
    _userCardBtns[1].resource = LoadPngFromFile(L"UI/READY_CARD2.png");
    _userCardBtns[2].resource = LoadPngFromFile(L"UI/READY_CARD3.png");
    _userCardBtns[3].resource = LoadPngFromFile(L"UI/READY_CARD4.png");

    _readyBtns[0].d2dLayoutRect = MakeLayoutRectByCorner(atb::client::ui::LobbyRoomListXOffset,
        atb::client::ui::LobbyRoomListYOffset,
        (atb::client::config::DefaultWindowWidth - (atb::client::ui::LobbyRoomListXOffset * 2.0f)) / 2.0f,
        atb::client::config::DefaultWindowHeight / 6.0f);
    _readyBtns[1].d2dLayoutRect = MakeLayoutRectByCorner(atb::client::ui::LobbyRoomListXOffset + ((atb::client::config::DefaultWindowWidth - (atb::client::ui::LobbyRoomListXOffset * 2.0f)) / 2.0f),
        atb::client::ui::LobbyRoomListYOffset,
        (atb::client::config::DefaultWindowWidth - (atb::client::ui::LobbyRoomListXOffset * 2.0f)) / 2.0f,
        atb::client::config::DefaultWindowHeight / 6.0f);

    _readyBtns[2].d2dLayoutRect = MakeLayoutRectByCorner(atb::client::ui::LobbyRoomListXOffset,
        atb::client::ui::LobbyRoomListYOffset + atb::client::config::DefaultWindowHeight / 6.0f,
        (atb::client::config::DefaultWindowWidth - (atb::client::ui::LobbyRoomListXOffset * 2.0f)) / 2.0f,
        atb::client::config::DefaultWindowHeight / 6.0f);

    _readyBtns[3].d2dLayoutRect = MakeLayoutRectByCorner(
        atb::client::ui::LobbyRoomListXOffset + (atb::client::config::DefaultWindowWidth - (atb::client::ui::LobbyRoomListXOffset * 2.0f)) / 2.0f,
        atb::client::ui::LobbyRoomListYOffset + atb::client::config::DefaultWindowHeight / 6.0f,
        (atb::client::config::DefaultWindowWidth - (atb::client::ui::LobbyRoomListXOffset * 2.0f)) / 2.0f,
        atb::client::config::DefaultWindowHeight / 6.0f);

    for (int i = 0; i < 4; ++i)
    {
        _userCardBtns[i].d2dLayoutRect = _readyBtns[i].d2dLayoutRect;
        _userCardBtns[i].m_hide = true;
    }

    //로비에서 출력할 방 리스트 영역
    for (int i = 0; i < LobbyRoomSlotCount; ++i)
    {
        _roomListLayout[i] = MakeLayoutRectByCorner(atb::client::ui::LobbyRoomListXOffset
            , atb::client::ui::LobbyRoomListYOffset + (atb::client::config::DefaultWindowHeight / 2.0f * ((float)i / LobbyRoomSlotCount)),
            atb::client::config::DefaultWindowWidth - (atb::client::ui::LobbyRoomListXOffset + atb::client::ui::LobbyRoomListRightOffset), atb::client::config::DefaultWindowHeight / 2.0f * (1.0f / LobbyRoomSlotCount));
        _roomListTextBlocks[i].m_d2dLayoutRect = _roomListLayout[i];
        _roomListTextBlocks[i].m_pdwFormat = _pLobbySceneTextFormat;
        _roomListTextBlocks[i].m_pstrText =  L"ROOMNUM:   MEMBER:   0/4";
    }

    // 인게임 비트맵 로드
    // 캐릭터 프로필
    _charProfileBtns[0].resource = LoadPngFromFile(L"UI/Char_UI_1.png"); // Boss
    _charProfileBtns[1].resource = LoadPngFromFile(L"UI/Char_UI_2.png"); // Yellow
    _charProfileBtns[2].resource = LoadPngFromFile(L"UI/Char_UI_3.png"); // Mask
    _charProfileBtns[3].resource = LoadPngFromFile(L"UI/Char_UI_5.png"); // Goggle
	for (int i = 0; i < PLAYERNUM; ++i)
	{
		_charProfileBtns[i].d2dLayoutRect = MakeLayoutRectByCorner(atb::client::config::DefaultWindowWidth * 0.01f, atb::client::config::DefaultWindowHeight * 0.1f* i, atb::client::config::DefaultWindowWidth * 0.1f, atb::client::config::DefaultWindowHeight * 0.1f);
	}

	// 상태 --> 동적으로 변하는 것이므로 그때 그때 위치를 업데이트하기로 한다. 일단 비트맵 리소스만 로드한다.
    _charStatusBitmaps[0] = LoadPngFromFile(L"UI/Normal.png");
    _charStatusBitmaps[1] = LoadPngFromFile(L"UI/Danger.png");
    _charStatusBitmaps[2] = LoadPngFromFile(L"UI/Dead.png");

    // HP
    _HpBitmap = LoadPngFromFile(L"UI/HP.png");
    for (int i = 0; i < HealthIconCount; ++i)
    {
        m_HPUi[i].resource = _HpBitmap;
        m_HPUi[i].d2dLayoutRect = MakeLayoutRectByCorner(atb::client::ui::ProfileOffsetX + atb::client::ui::LargeProfileWidth + atb::client::ui::StatusWidth * i,
            atb::client::ui::LargeProfileOffsetY + atb::client::ui::LargeProfileWidth / 4.0, atb::client::ui::StatusWidth, atb::client::ui::StatusHeight);
    }
    // 크로스 헤드
    _crossHeadIcon.resource = LoadPngFromFile(L"UI/crossHair.png");
    _crossHeadIcon.d2dLayoutRect = MakeLayoutRect(atb::client::ui::CenterX, atb::client::ui::CenterY,10.f,10.f);
    _crossHeadIcon.m_hide = false;

    // 피격 이펙트
    for (int i = 0; i < _attackedUICount; i++)
    {
        _attackedEffectOpacity[i] = 0.5f;
    }
    _attackedEffects[0].resource = LoadPngFromFile(L"UI/blood_base.png");
    _attackedEffects[0].d2dLayoutRect = MakeLayoutRectByCorner(0, 0, atb::client::config::DefaultWindowWidth, atb::client::config::DefaultWindowHeight);
    _attackedEffects[0].m_hide = false;

    _attackedEffects[1].resource = LoadPngFromFile(L"UI/blood_outline.png");
    _attackedEffects[1].d2dLayoutRect = MakeLayoutRectByCorner(0, 0, atb::client::config::DefaultWindowWidth, atb::client::config::DefaultWindowHeight);
    _attackedEffects[1].m_hide = false;

    _attackedEffects[2].resource = LoadPngFromFile(L"UI/blood_frame.png");
    _attackedEffects[2].d2dLayoutRect = MakeLayoutRectByCorner(0, 0, atb::client::config::DefaultWindowWidth, atb::client::config::DefaultWindowHeight);
    _attackedEffects[2].m_hide = false;

    _attackedEffects[3].resource = LoadPngFromFile(L"UI/bullet_hole_glass.png");
    _attackedEffects[3].d2dLayoutRect = MakeLayoutRect(3*atb::client::config::DefaultWindowWidth / 4, 3*atb::client::config::DefaultWindowHeight / 4,atb::client::config::DefaultWindowWidth/2, atb::client::config::DefaultWindowHeight/2);
    _attackedEffects[3].m_hide = false;

    _attackedEffects[4].resource = LoadPngFromFile(L"UI/bullet_hole_glass.png");
    _attackedEffects[4].d2dLayoutRect = MakeLayoutRect( atb::client::config::DefaultWindowWidth / 4, atb::client::config::DefaultWindowHeight / 4, atb::client::config::DefaultWindowWidth / 3, atb::client::config::DefaultWindowHeight / 3);
    _attackedEffects[4].m_hide = false;




    // 발전기 게이지
    for (int i = 0; i < 21; ++i)
    {
        std::wstring filename;
        filename = L"UI/Generator_Gauge_";
        WCHAR num[3];
        _itow_s(i + 1, num, 10);
        num[2] = L'\0';
        filename.append(num);
        filename.append(L".png");
        _generateBtns[i].resource = LoadPngFromFile(filename.c_str());
        _generateBtns[i].d2dLayoutRect = MakeLayoutRect(atb::client::config::DefaultWindowWidth / 2.0, atb::client::config::DefaultWindowHeight * 0.8, atb::client::config::DefaultWindowWidth / 2, atb::client::config::DefaultWindowHeight / 4.f);
        _generateBtns[i].m_hide = true;
    }
    _generateBtns[21].resource = LoadPngFromFile(L"UI/F.png");
    _generateBtns[21].d2dLayoutRect = MakeLayoutRect(atb::client::config::DefaultWindowWidth / 2.0, atb::client::config::DefaultWindowHeight * 0.9, atb::client::config::DefaultWindowWidth * 0.1f, atb::client::config::DefaultWindowHeight * 0.1f);
    _generateBtns[21].m_hide = true;

    // 살리기 아이콘
    _rescueIcon.resource = LoadPngFromFile(L"UI/Rescue.png");
    _rescueIcon.d2dLayoutRect = _generateBtns[21].d2dLayoutRect;
    _rescueIcon.m_hide = true;

    m_RescueGuage.m_hide = true;
    m_RescueGuage.d2dLayoutRect[0] = MakeLayoutRect(atb::client::ui::CenterX, atb::client::ui::CenterY, atb::client::ui::MaxRescueGaugeWidth,50);
    m_RescueGuage.d2dLayoutRect[1] = MakeLayoutRect(atb::client::ui::CenterX, atb::client::ui::CenterY, atb::client::ui::MaxRescueGaugeWidth,50);
    //
}

void UIManager::Render2D(int32 curScene, int32 localPlayerIndex)
{
    DrawOtherSceneBackGround(curScene);

    DrawOtherSceneUI(curScene, 0);
    DrawOtherSceneUI(curScene, 1);
    DrawOtherSceneUITextBlock(curScene);

    DrawGameSceneUI(curScene, localPlayerIndex);
}
