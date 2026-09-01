#include "pch.h"
#include "UIManager.h"
#include "SceneId.h"

#include "DXSampleHelper.h"

#include <d2d1_1.h>
#include <wincodec.h>
//
//const float MAX_RESCUE_GUAGE = FRAME_BUFFER_WIDTH / 4.0f;
//
//const float PROFILE_UI_OFFSET_X = FRAME_BUFFER_WIDTH * 0.01;
//const float PROFILE_UI_OFFSET_Y = FRAME_BUFFER_HEIGHT * 0.1;
//
//const float PROFILE_UI_WIDTH = FRAME_BUFFER_WIDTH * 0.1;
//const float PROFILE_UI_HEIGHT = FRAME_BUFFER_HEIGHT * 0.1;
//
//const float BIG_PROFILE_UI_OFFSET_Y = PROFILE_UI_OFFSET_Y * 7;
//const float BIG_PROFILE_UI_WIDTH = FRAME_BUFFER_WIDTH * 0.2;
//const float BIG_PROFILE_UI_HEIGHT = FRAME_BUFFER_HEIGHT * 0.2;
//
//const float STATUS_UI_WIDTH = PROFILE_UI_WIDTH * 0.8;
//const float STATUS_UI_HEIGHT = PROFILE_UI_HEIGHT * 0.8;

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
    : m_pd2dDeviceContext(d2dContext), m_pd2dWriteFactory(writeFactory)
{
    if (!m_pd2dDeviceContext || !m_pd2dWriteFactory)
        throw std::invalid_argument("UIManager requires initialized D2D services");
    m_fWidth = static_cast<float>(nWidth);
    m_fHeight = static_cast<float>(nHeight);
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
    for (std::size_t index = 0; index < std::size(m_LoginResult); ++index)
        m_LoginResult[index].m_hide = index != visibleIndex;
}

std::optional<UIManager::LoginFeedback> UIManager::TickLoginFeedback(const float elapsedSeconds) noexcept
{
    for (std::size_t index = 0; index < std::size(m_LoginResult); ++index)
    {
        auto& feedback = m_LoginResult[index];
        if (feedback.m_hide) continue;

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
        rect = &m_IDPWTextBlocks[0].m_d2dLayoutRect;
        break;
    case UiHitTarget::TitlePassword:
        rect = &m_IDPWTextBlocks[1].m_d2dLayoutRect;
        break;
    case UiHitTarget::TitleLogin:
        rect = &m_TitleButtons[0].d2dLayoutRect;
        break;
    case UiHitTarget::TitleRegister:
        rect = &m_TitleButtons[2].d2dLayoutRect;
        break;
    case UiHitTarget::TitleQuit:
        rect = &m_TitleButtons[1].d2dLayoutRect;
        break;
    case UiHitTarget::LobbyEnter:
        rect = &m_LobbyButtons[0].d2dLayoutRect;
        break;
    case UiHitTarget::LobbyCreate:
        rect = &m_LobbyButtons[1].d2dLayoutRect;
        break;
    case UiHitTarget::LobbyLogout:
        rect = &m_LobbyButtons[2].d2dLayoutRect;
        break;
    case UiHitTarget::RoomReady:
        rect = &m_RoomButtons[0].d2dLayoutRect;
        break;
    case UiHitTarget::RoomLeave:
        rect = &m_RoomButtons[1].d2dLayoutRect;
        break;
    }
    return rect && ContainsPoint(*rect, point);
}

bool UIManager::TrySelectLobbyRoomSlot(const int32 slot, const POINT& point) noexcept
{
    if (slot < 0 || slot >= LobbyRoomSlotCount) return false;
    if (!ContainsPoint(m_RoomListLayout[slot], point)) return false;
    m_selectedLayout = slot;
    return true;
}

void UIManager::AppendCredential(const int32 field, const wchar_t character)
{
    auto& text = m_IDPWTextBlocks[field == 1 ? 1 : 0].m_pstrText;
    if (text.length() <= 10) text.push_back(character);
}

void UIManager::BackspaceCredential(const int32 field)
{
    auto& text = m_IDPWTextBlocks[field == 1 ? 1 : 0].m_pstrText;
    if (!text.empty()) text.erase(text.length() - 1, 1);
}

const std::wstring& UIManager::CredentialText(const int32 field) const noexcept
{
    return m_IDPWTextBlocks[field == 1 ? 1 : 0].m_pstrText;
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
    ThrowIfFailed(m_pd2dDeviceContext->CreateBitmapFromWicBitmap(
        converter.Get(),
        nullptr,
        &bitmap));

    return bitmap.Detach();
}


void UIManager::UpdateRoomTextBlocks(UINT nIndex,const WCHAR* pstrUIText, const D2D1_RECT_F& pd2dLayoutRect, bool hide)
{
    m_RoomListTextBlock[nIndex].m_pstrText.erase();
    m_RoomListTextBlock[nIndex].m_pstrText.append(pstrUIText);

    m_RoomListTextBlock[nIndex].m_hide = hide;
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
    m_roomUiSnapshot = snapshot;
}

void UIManager::UpdateResultSceneUI(const ResultUiSnapshot& snapshot)
{
    m_resultUiSnapshot = snapshot;
}


void UIManager::ReleaseResources()
{
    auto release = [](auto*& resource)
    {
        if (resource) resource->Release();
        resource = nullptr;
    };

    for (auto& item : m_backGround) release(item.resource);
    for (auto& item : m_LoginResult) release(item.resource);
    for (auto& item : m_TitleButtons) release(item.resource);
    for (auto& item : m_LobbyButtons) release(item.resource);
    for (auto& item : m_RoomButtons) release(item.resource);
    for (auto& item : m_ReadyBitmaps) release(item.resource);
    for (auto& item : m_ReadyCard) release(item.resource);
    for (auto& item : m_CharProfile) release(item.resource);
    for (auto& item : m_CharStatusBitmaps) release(item);
    release(m_HpBitmap);
    release(m_CharCrossHead.resource);
    for (auto& item : m_AttackedEffect) release(item.resource);
    for (auto& item : m_GenerateUIButtons) release(item.resource);
    release(m_RescueIcon.resource);

    for (auto& item : m_IDPWTextBlocks) release(item.m_pd2dTextBrush);
    release(m_TitleTextFormat);
    release(m_LobbyTextFormat);
    release(redBrush);
    release(grayBrush);
    release(blackBrush);
    release(whiteBrush);
    release(greenBrush);

    m_pd2dDeviceContext = nullptr;
    m_pd2dWriteFactory = nullptr;
}

void UIManager::DrawOtherSceneBackGround(int32 Scene)
{
    switch (Scene)
    {
    case atb::SceneIndex(atb::SceneId::Title):
    case atb::SceneIndex(atb::SceneId::Lobby):
    case atb::SceneIndex(atb::SceneId::Room):
        m_pd2dDeviceContext->DrawBitmap(m_backGround[Scene].resource, D2D1_RECT_F{ 0,0,m_fWidth,m_fHeight });
        break;
    case atb::SceneIndex(atb::SceneId::InGame):
        break;
    case atb::SceneIndex(atb::SceneId::Result):
        if (m_resultUiSnapshot.bossWon)
        {
            m_pd2dDeviceContext->DrawBitmap(m_backGround[3].resource, D2D1_RECT_F{ 0,0,m_fWidth,m_fHeight });
        }
        else m_pd2dDeviceContext->DrawBitmap(m_backGround[4].resource, D2D1_RECT_F{ 0,0,m_fWidth,m_fHeight });

        break;
    }
}

void UIManager::DrawOtherSceneUI(int32 Scene,int32 idx)
{
    if (Scene == atb::SceneIndex(atb::SceneId::Title)) // 타이틀 씬
    {
        m_pd2dDeviceContext->DrawBitmap(m_TitleButtons[0].resource, m_TitleButtons[0].d2dLayoutRect);
        m_pd2dDeviceContext->DrawBitmap(m_TitleButtons[1].resource, m_TitleButtons[1].d2dLayoutRect);
        m_pd2dDeviceContext->DrawBitmap(m_TitleButtons[2].resource, m_TitleButtons[2].d2dLayoutRect);


        for (int i = 0; i < 3; ++i) if(!m_LoginResult[i].m_hide) m_pd2dDeviceContext->DrawBitmap(m_LoginResult[i].resource, m_LoginResult[i].d2dLayoutRect, m_LoginResult[i].animTime);
    }
    else if (Scene == atb::SceneIndex(atb::SceneId::Lobby)) // 로비 씬
    {
        m_pd2dDeviceContext->DrawBitmap(m_LobbyButtons[0].resource, m_LobbyButtons[0].d2dLayoutRect);
        m_pd2dDeviceContext->DrawBitmap(m_LobbyButtons[1].resource, m_LobbyButtons[1].d2dLayoutRect);
        m_pd2dDeviceContext->DrawBitmap(m_LobbyButtons[2].resource, m_LobbyButtons[2].d2dLayoutRect);
    }
    else if (Scene == atb::SceneIndex(atb::SceneId::Room)) // 게임 룸 씬
    {
        m_pd2dDeviceContext->DrawBitmap(m_RoomButtons[0].resource, m_RoomButtons[0].d2dLayoutRect);
        m_pd2dDeviceContext->DrawBitmap(m_RoomButtons[1].resource, m_RoomButtons[1].d2dLayoutRect);

        for (std::size_t index = 0; index < m_roomUiSnapshot.members.size(); ++index)
        {
            const auto& member = m_roomUiSnapshot.members[index];
            if (member.occupied)
                m_pd2dDeviceContext->DrawBitmap(
                    m_ReadyCard[index].resource, m_ReadyCard[index].d2dLayoutRect);

            m_pd2dDeviceContext->DrawRectangle(
                m_ReadyBitmaps[index].d2dLayoutRect, blackBrush, 6.0f);
            if (member.ready)
                m_pd2dDeviceContext->DrawBitmap(
                    m_ReadyBitmaps[index].resource, m_ReadyBitmaps[index].d2dLayoutRect);
        }
    }
}

void UIManager::DrawOtherSceneUITextBlock(int32 Scene)
{

    if (Scene == atb::SceneIndex(atb::SceneId::Title))
    {
        m_pd2dDeviceContext->FillRectangle(m_IDPWTextBlocks[0].m_d2dLayoutRect, grayBrush);
        m_pd2dDeviceContext->FillRectangle(m_IDPWTextBlocks[1].m_d2dLayoutRect, grayBrush);

        m_pd2dDeviceContext->DrawRectangle(m_IDPWTextBlocks[0].m_d2dLayoutRect, blackBrush,4.0);
        m_pd2dDeviceContext->DrawRectangle(m_IDPWTextBlocks[1].m_d2dLayoutRect, blackBrush,4.0);

        if(!m_IDPWTextBlocks[0].m_hide) m_pd2dDeviceContext->DrawText(m_IDPWTextBlocks[0].m_pstrText.c_str(),
            (UINT)wcslen(m_IDPWTextBlocks[0].m_pstrText.c_str()), m_IDPWTextBlocks[0].m_pdwFormat,
            m_IDPWTextBlocks[0].m_d2dLayoutRect, m_IDPWTextBlocks[0].m_pd2dTextBrush);

        if (!m_IDPWTextBlocks[1].m_hide) m_pd2dDeviceContext->DrawText(m_IDPWTextBlocks[1].m_pstrText.c_str(),
            (UINT)wcslen(m_IDPWTextBlocks[1].m_pstrText.c_str()), m_IDPWTextBlocks[1].m_pdwFormat,
            m_IDPWTextBlocks[1].m_d2dLayoutRect, m_IDPWTextBlocks[1].m_pd2dTextBrush);
    }
    else if (Scene == atb::SceneIndex(atb::SceneId::Lobby))
    {
        for (int i = 0; i < LobbyRoomSlotCount; ++i)
        {

            m_pd2dDeviceContext->DrawRectangle(m_RoomListLayout[i], blackBrush, 4.0f);
            if(!m_RoomListTextBlock[i].m_hide) m_pd2dDeviceContext->DrawText(m_RoomListTextBlock[i].m_pstrText.c_str()
                , (UINT)wcslen(m_RoomListTextBlock[i].m_pstrText.c_str()), m_RoomListTextBlock[i].m_pdwFormat
                , m_RoomListTextBlock[i].m_d2dLayoutRect, blackBrush);
        }
        if (m_selectedLayout >= 0)  m_pd2dDeviceContext->DrawRectangle(m_RoomListLayout[m_selectedLayout], redBrush, 4.0f);
    }
}

void UIManager::InitGameSceneUI(const GameUiSnapshot& snapshot)
{
    if (!snapshot.localPlayerIndex || *snapshot.localPlayerIndex >= kGameUiPlayerCount) return;
    const int32 playerIndex = static_cast<int32>(*snapshot.localPlayerIndex);

    for (auto& profile : m_CharProfile) profile.m_hide = false;
    for (auto& status : m_CharStatus) status.m_hide = false;
    m_CharProfile[playerIndex].m_hide = true;
    if (playerIndex != 0) m_CharProfile[0].m_hide = true;
    for (auto& i : m_GenerateUIButtons) i.m_hide = true;
    // 플레이어 상태 UI 출력
    switch (playerIndex)
    {
        case 0:
        {
           // 사장님은 3명 모두 출력
            for (int i = 1; i < 4; ++i)
            {
                m_CharProfile[i].d2dLayoutRect =
                    MakeLayoutRectByCorner(PROFILE_UI_OFFSET_X, PROFILE_UI_OFFSET_Y + PROFILE_UI_OFFSET_Y * (i - 1),
                        PROFILE_UI_WIDTH, PROFILE_UI_HEIGHT);
            }
            m_CharStatus[0].d2dLayoutRect =
                MakeLayoutRectByCorner(PROFILE_UI_OFFSET_X + PROFILE_UI_WIDTH, PROFILE_UI_OFFSET_Y, STATUS_UI_WIDTH, STATUS_UI_HEIGHT);
            m_CharStatus[1].d2dLayoutRect =
                MakeLayoutRectByCorner(PROFILE_UI_OFFSET_X + PROFILE_UI_WIDTH, PROFILE_UI_OFFSET_Y * 2, STATUS_UI_WIDTH, STATUS_UI_HEIGHT);
            m_CharStatus[2].d2dLayoutRect =
                MakeLayoutRectByCorner(PROFILE_UI_OFFSET_X + PROFILE_UI_WIDTH, PROFILE_UI_OFFSET_Y * 3, STATUS_UI_WIDTH, STATUS_UI_HEIGHT);
            break;
        }
        // 나머지는 자신을 제외한 2명만 출력
        case 1:
        {

            m_CharProfile[2].d2dLayoutRect =
                MakeLayoutRectByCorner(PROFILE_UI_OFFSET_X, PROFILE_UI_OFFSET_Y, PROFILE_UI_WIDTH, PROFILE_UI_HEIGHT);

            m_CharProfile[3].d2dLayoutRect =
                MakeLayoutRectByCorner(PROFILE_UI_OFFSET_X, PROFILE_UI_OFFSET_Y * 2, PROFILE_UI_WIDTH, PROFILE_UI_HEIGHT);

            // 1 2 3 --> 0 1 2
            m_CharStatus[0].m_hide = true;
            m_CharStatus[1].d2dLayoutRect =
                MakeLayoutRectByCorner(PROFILE_UI_OFFSET_X + PROFILE_UI_WIDTH, PROFILE_UI_OFFSET_Y, STATUS_UI_WIDTH, STATUS_UI_HEIGHT);
            m_CharStatus[2].d2dLayoutRect =
                MakeLayoutRectByCorner(PROFILE_UI_OFFSET_X + PROFILE_UI_WIDTH, PROFILE_UI_OFFSET_Y * 2, STATUS_UI_WIDTH, STATUS_UI_HEIGHT);
        }
        break;
        case 2:
        {

            m_CharProfile[1].d2dLayoutRect =
                MakeLayoutRectByCorner(PROFILE_UI_OFFSET_X, PROFILE_UI_OFFSET_Y, PROFILE_UI_WIDTH, PROFILE_UI_HEIGHT);
            m_CharProfile[3].d2dLayoutRect =
                MakeLayoutRectByCorner(PROFILE_UI_OFFSET_X, PROFILE_UI_OFFSET_Y * 2, PROFILE_UI_WIDTH, PROFILE_UI_HEIGHT);

            m_CharStatus[1].m_hide = true;
            m_CharStatus[0].d2dLayoutRect =
                MakeLayoutRectByCorner(PROFILE_UI_OFFSET_X + PROFILE_UI_WIDTH, PROFILE_UI_OFFSET_Y, STATUS_UI_WIDTH, STATUS_UI_HEIGHT);
            m_CharStatus[2].d2dLayoutRect =
                MakeLayoutRectByCorner(PROFILE_UI_OFFSET_X + PROFILE_UI_WIDTH, PROFILE_UI_OFFSET_Y * 2, STATUS_UI_WIDTH, STATUS_UI_HEIGHT);
        }
        break;

        case 3:
        {
            m_CharProfile[1].d2dLayoutRect =
                MakeLayoutRectByCorner(PROFILE_UI_OFFSET_X, PROFILE_UI_OFFSET_Y, PROFILE_UI_WIDTH, PROFILE_UI_HEIGHT);
            m_CharProfile[2].d2dLayoutRect =
                MakeLayoutRectByCorner(PROFILE_UI_OFFSET_X, PROFILE_UI_OFFSET_Y * 2, PROFILE_UI_WIDTH, PROFILE_UI_HEIGHT);

            m_CharStatus[2].m_hide = true;

            m_CharStatus[0].d2dLayoutRect =
                MakeLayoutRectByCorner(PROFILE_UI_OFFSET_X + PROFILE_UI_WIDTH, PROFILE_UI_OFFSET_Y, STATUS_UI_WIDTH, STATUS_UI_HEIGHT);
            m_CharStatus[1].d2dLayoutRect =
                MakeLayoutRectByCorner(PROFILE_UI_OFFSET_X + PROFILE_UI_WIDTH, PROFILE_UI_OFFSET_Y * 2, STATUS_UI_WIDTH, STATUS_UI_HEIGHT);
        }
        break;
    }

    for (int i = 0; i < 3; ++i)
    {
        m_CharStatus[i].resource =  m_CharStatusBitmaps[0]; // normal status로 시작
    }

    m_myProfileLayout = MakeLayoutRectByCorner(PROFILE_UI_OFFSET_X,BIG_PROFILE_UI_OFFSET_Y, BIG_PROFILE_UI_WIDTH, BIG_PROFILE_UI_HEIGHT);

    // 플레이어 hp 출력

}

void UIManager::UpdateGameSceneUI(const GameUiSnapshot& snapshot)
{
    if (!snapshot.localPlayerIndex || *snapshot.localPlayerIndex >= kGameUiPlayerCount) return;
    const std::size_t playerIndex = *snapshot.localPlayerIndex;

    for (std::size_t index = 1; index < snapshot.players.size(); ++index)
    {
        if (!snapshot.players[index] || index == playerIndex) continue;

        const int health = snapshot.players[index]->health;
        m_CharStatus[index - 1].resource = health == MAX_HP
            ? m_CharStatusBitmaps[0]
            : health == 0 ? m_CharStatusBitmaps[2] : m_CharStatusBitmaps[1];
    }

    if (!snapshot.localEmployee || !snapshot.players[playerIndex]) return;
    const EmployeeUiSnapshot& employee = *snapshot.localEmployee;

    for (int index = 0; index < m_nAttackedUI; ++index)
    {
        if (employee.invincible)
        {
            constexpr float maxOpacity = 0.5f;
            constexpr float bulletHoleOpacityExtra = 1.7f;
            constexpr float baseOpacityExtra = 0.5f;
            constexpr float outlineOpacityExtra = 1.0f;
            if (index == 4)
            {
                m_AttackedEffect[index].m_hide = false;
                m_AttackedOpacity[index] = employee.uiCooldown <= 0.5f
                    ? (employee.uiCooldown / 0.5f) * maxOpacity * bulletHoleOpacityExtra
                    : ((1.0f - employee.uiCooldown) / 0.5f) * maxOpacity * bulletHoleOpacityExtra;
            }
            else if (index == 3)
            {
                if (employee.uiCooldown > 0.65f)
                {
                    m_AttackedEffect[index].m_hide = false;
                    m_AttackedOpacity[index] = (employee.uiCooldown / 0.35f) * maxOpacity * bulletHoleOpacityExtra;
                }
                else if (employee.uiCooldown >= 0.3f)
                {
                    m_AttackedEffect[index].m_hide = false;
                    m_AttackedOpacity[index] = (1.0f - employee.uiCooldown / 0.35f) * maxOpacity * bulletHoleOpacityExtra;
                }
            }
            else
            {
                m_AttackedEffect[index].m_hide = false;
            }

            if (employee.uiCooldown >= 0.0f && index < 3) m_AttackedOpacity[index] = employee.uiCooldown * maxOpacity;
            if (index == 0) m_AttackedOpacity[index] *= baseOpacityExtra;
            else if (index == 1) m_AttackedOpacity[index] *= outlineOpacityExtra;
        }
        else
        {
            m_AttackedEffect[index].m_hide = true;
        }
    }

    for (auto& hp : m_HPUi) hp.m_hide = true;
    for (int index = 0; index < snapshot.players[playerIndex]->health; ++index) m_HPUi[index].m_hide = false;

    m_GenerateUIButtons[21].m_hide = !employee.inGeneratorArea;
    if (!employee.inGeneratorArea)
        for (int index = 0; index < 21; ++index) m_GenerateUIButtons[index].m_hide = true;

    if (employee.generatorInteractionActive && employee.generatorGauge)
    {
        const int gauge = static_cast<int>(*employee.generatorGauge);
        if (gauge < 1) m_GenerateUIButtons[0].m_hide = false;
        else if (((gauge % 100) / 5) <= 19) m_GenerateUIButtons[((gauge % 100) / 5) + 1].m_hide = false;
    }
    else
    {
        for (int index = 0; index < 21; ++index) m_GenerateUIButtons[index].m_hide = true;
    }

	m_RescueIcon.m_hide = !employee.rescueTargetAvailable;
    m_RescueGuage.m_hide = true;
    if (employee.rescueGauge && *employee.rescueGauge >= 0.0f && *employee.rescueGauge <= 100.0f)
    {
        const float dx = (*employee.rescueGauge * 5.8f / MAX_RESCUE_GUAGE) * 100.0f;
        m_RescueGuage.m_hide = false;
        m_RescueGuage.d2dLayoutRect[1] = MakeLayoutRect(
            CENTER_X + (-MAX_RESCUE_GUAGE + dx) / 2.0f, CENTER_Y, dx, 50.0f);
    }
    else
    {
        m_RescueGuage.d2dLayoutRect[1] = m_RescueGuage.d2dLayoutRect[0];
    }
}

void UIManager::DrawGameSceneUI(int32 Scene, int32 localPlayerIndex)
{
    if (3 != Scene || localPlayerIndex < 0 || localPlayerIndex >= PLAYERNUM) return;
    // 고정 렌더링
    // 다른 캐릭터 초상화 , 내 캐릭터 초상화
    for (auto i : m_CharProfile)
    {
        if(!i.m_hide) m_pd2dDeviceContext->DrawBitmap(i.resource, i.d2dLayoutRect, FULL_UI_OPACITY_VALUE);
    }

    for (auto i : m_GenerateUIButtons)
        if(!i.m_hide) m_pd2dDeviceContext->DrawBitmap(i.resource, i.d2dLayoutRect, FULL_UI_OPACITY_VALUE);

    for(auto i : m_CharStatus)
        if(!i.m_hide) m_pd2dDeviceContext->DrawBitmap(i.resource, i.d2dLayoutRect, FULL_UI_OPACITY_VALUE);

    // 큰 초상화 그리기
    m_pd2dDeviceContext->DrawBitmap(m_CharProfile[localPlayerIndex].resource, m_myProfileLayout, FULL_UI_OPACITY_VALUE);
    // HP 그리기
    if (localPlayerIndex != 0)
    {
        for(auto i : m_HPUi)  if (!i.m_hide) m_pd2dDeviceContext->DrawBitmap(i.resource, i.d2dLayoutRect, FULL_UI_OPACITY_VALUE);
        for (int i = 0; i < m_nAttackedUI; i++)
        {
            if (!m_AttackedEffect[i].m_hide) m_pd2dDeviceContext->DrawBitmap(m_AttackedEffect[i].resource, m_AttackedEffect[i].d2dLayoutRect, m_AttackedOpacity[i]* FULL_UI_OPACITY_VALUE);

        }
        if (!m_RescueIcon.m_hide) m_pd2dDeviceContext->DrawBitmap(m_RescueIcon.resource, m_RescueIcon.d2dLayoutRect, FULL_UI_OPACITY_VALUE);

        if (!m_RescueGuage.m_hide)
        {
            m_pd2dDeviceContext->FillRectangle(m_RescueGuage.d2dLayoutRect[1], greenBrush);
            m_pd2dDeviceContext->DrawRectangle(m_RescueGuage.d2dLayoutRect[0], blackBrush, 5.0f);
        }
    }

    if (localPlayerIndex == 0) m_pd2dDeviceContext->DrawBitmap(m_CharCrossHead.resource, m_CharCrossHead.d2dLayoutRect,m_CrossHeadOpacity* FULL_UI_OPACITY_VALUE);

}

ID2D1SolidColorBrush* UIManager::CreateBrush(D2D1::ColorF d2dColor)
{
    ComPtr<ID2D1SolidColorBrush> brush;
    ThrowIfFailed(m_pd2dDeviceContext->CreateSolidColorBrush(d2dColor, &brush));
    return brush.Detach();
}

IDWriteTextFormat* UIManager::CreateTextFormat(const WCHAR* pszFontName, float fFontSize)
{
    ComPtr<IDWriteTextFormat> format;
    ThrowIfFailed(m_pd2dWriteFactory->CreateTextFormat(
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
    redBrush = CreateBrush(D2D1::ColorF::Red);
    grayBrush = CreateBrush(D2D1::ColorF::Gray);
    blackBrush = CreateBrush(D2D1::ColorF::Black);
    greenBrush = CreateBrush(D2D1::ColorF::Green);
    // 폰트
    m_TitleTextFormat = CreateTextFormat(L"맑은 고딕", 40);
    m_LobbyTextFormat = CreateTextFormat(L"맑은 고딕", 40);
    // 배경 리소스들
    m_backGround[0].resource = LoadPngFromFile(L"UI/Title.png");
    m_backGround[1].resource = LoadPngFromFile(L"UI/Lobby.png");
    m_backGround[2].resource = LoadPngFromFile(L"UI/Game.png");

    m_backGround[3].resource = LoadPngFromFile(L"UI/Boss_Result.png");
    m_backGround[4].resource = LoadPngFromFile(L"UI/Emp_Result.png");

    // 타이틀 씬에 필요한 버튼
    m_TitleButtons[0].resource      = LoadPngFromFile(L"UI/Title_Start.png");
    m_TitleButtons[1].resource      = LoadPngFromFile(L"UI/Title_Quit.png");
    m_TitleButtons[2].resource      = LoadPngFromFile(L"UI/Title_Register.png");

    m_TitleButtons[0].d2dLayoutRect = MakeLayoutRect(CENTER_X + TITLEBUTTON_X_OFFSET, CENTER_Y + TITLEBUTTON_Y_OFFSET, 200, 50);
    m_TitleButtons[1].d2dLayoutRect = MakeLayoutRect(CENTER_X , CENTER_Y + FRAME_BUFFER_HEIGHT / 2.5f, 200, 50);
    m_TitleButtons[2].d2dLayoutRect = MakeLayoutRect(CENTER_X - TITLEBUTTON_X_OFFSET, CENTER_Y + TITLEBUTTON_Y_OFFSET, 200, 50);
    // ID / PW 입력 창
    m_IDPWTextBlocks[0].m_pd2dTextBrush = CreateBrush(D2D1::ColorF::White);
    m_IDPWTextBlocks[0].m_pdwFormat = m_TitleTextFormat;
    m_IDPWTextBlocks[0].m_pstrText = L"";


    m_IDPWTextBlocks[1].m_pd2dTextBrush = CreateBrush(D2D1::ColorF::White);
    m_IDPWTextBlocks[1].m_pdwFormat = m_TitleTextFormat;
    m_IDPWTextBlocks[1].m_pstrText = L"";

    m_IDPWTextBlocks[0].m_d2dLayoutRect = MakeLayoutRect(CENTER_X, CENTER_Y + FRAME_BUFFER_HEIGHT / 4.0f,  400, FontSize);
    m_IDPWTextBlocks[1].m_d2dLayoutRect = MakeLayoutRect(CENTER_X, CENTER_Y + FRAME_BUFFER_HEIGHT / 3.0f , 400, FontSize);

    m_LoginResult[0].resource = LoadPngFromFile(L"UI/LOGIN_OK.png");
    m_LoginResult[1].resource = LoadPngFromFile(L"UI/LOGIN_FAIL.png");
    m_LoginResult[2].resource = LoadPngFromFile(L"UI/REG.png");

    for (int i = 0; i < 3; ++i)
    {
        m_LoginResult[i].d2dLayoutRect = MakeLayoutRect(CENTER_X, CENTER_Y, 300, 100);
        m_LoginResult[i].m_hide = true;
    }


    // 로비 씬에 필요한 버튼
    m_LobbyButtons[0].resource = LoadPngFromFile(L"UI/Enter_Room.png");
    m_LobbyButtons[1].resource = LoadPngFromFile(L"UI/Create_Room.png");
    m_LobbyButtons[2].resource = LoadPngFromFile(L"UI/Quit_Lobby.png");

    m_LobbyButtons[0].d2dLayoutRect = MakeLayoutRectByCorner(LOBBYBUTTON_X_OFFSET,        LOBBYBUTTON_Y_OFFSET, FRAME_BUFFER_WIDTH / 3.0f, FRAME_BUFFER_HEIGHT / 4.0);
    m_LobbyButtons[1].d2dLayoutRect = MakeLayoutRectByCorner(0,                           LOBBYBUTTON_Y_OFFSET, FRAME_BUFFER_WIDTH / 3.0f, FRAME_BUFFER_HEIGHT / 4.0);
    m_LobbyButtons[2].d2dLayoutRect = MakeLayoutRectByCorner(LOBBYBUTTON_X_OFFSET * 2.0f, LOBBYBUTTON_Y_OFFSET, FRAME_BUFFER_WIDTH / 3.0f, FRAME_BUFFER_HEIGHT / 4.0);


    m_RoomButtons[0].resource = LoadPngFromFile(L"UI/Ready_Game.png");
    m_RoomButtons[1].resource = LoadPngFromFile(L"UI/Quit_Game.png");
    m_RoomButtons[0].d2dLayoutRect = MakeLayoutRectByCorner(GAMEROOM_BUTTON_X_OFFSET, GAMEROOM_BUTTON_Y_OFFSET, FRAME_BUFFER_WIDTH / 5.0f, FRAME_BUFFER_HEIGHT / 7.0f);
    m_RoomButtons[1].d2dLayoutRect = MakeLayoutRectByCorner(GAMEROOM_BUTTON_X_OFFSET + (FRAME_BUFFER_WIDTH / 5.0f), GAMEROOM_BUTTON_Y_OFFSET, FRAME_BUFFER_WIDTH / 5.0f, FRAME_BUFFER_HEIGHT / 7.0f);

    m_ReadyBitmaps[0].resource = LoadPngFromFile(L"UI/Ready.png");
    m_ReadyBitmaps[1].resource = LoadPngFromFile(L"UI/Ready2.png");
    m_ReadyBitmaps[2].resource = LoadPngFromFile(L"UI/Ready3.png");
    m_ReadyBitmaps[3].resource = LoadPngFromFile(L"UI/Ready4.png");

    m_ReadyCard[0].resource = LoadPngFromFile(L"UI/READY_CARD1.png");
    m_ReadyCard[1].resource = LoadPngFromFile(L"UI/READY_CARD2.png");
    m_ReadyCard[2].resource = LoadPngFromFile(L"UI/READY_CARD3.png");
    m_ReadyCard[3].resource = LoadPngFromFile(L"UI/READY_CARD4.png");

    m_ReadyBitmaps[0].d2dLayoutRect = MakeLayoutRectByCorner(LOBBYROOMLIST_X_OFFSET,
        LOBBYROOMLIST_Y_OFFSET,
        (FRAME_BUFFER_WIDTH - (LOBBYROOMLIST_X_OFFSET * 2.0f)) / 2.0f,
        FRAME_BUFFER_HEIGHT / 6.0f);
    m_ReadyBitmaps[1].d2dLayoutRect = MakeLayoutRectByCorner(LOBBYROOMLIST_X_OFFSET + ((FRAME_BUFFER_WIDTH - (LOBBYROOMLIST_X_OFFSET * 2.0f)) / 2.0f),
        LOBBYROOMLIST_Y_OFFSET,
        (FRAME_BUFFER_WIDTH - (LOBBYROOMLIST_X_OFFSET * 2.0f)) / 2.0f,
        FRAME_BUFFER_HEIGHT / 6.0f);

    m_ReadyBitmaps[2].d2dLayoutRect = MakeLayoutRectByCorner(LOBBYROOMLIST_X_OFFSET,
        LOBBYROOMLIST_Y_OFFSET + FRAME_BUFFER_HEIGHT / 6.0f,
        (FRAME_BUFFER_WIDTH - (LOBBYROOMLIST_X_OFFSET * 2.0f)) / 2.0f,
        FRAME_BUFFER_HEIGHT / 6.0f);

    m_ReadyBitmaps[3].d2dLayoutRect = MakeLayoutRectByCorner(
        LOBBYROOMLIST_X_OFFSET + (FRAME_BUFFER_WIDTH - (LOBBYROOMLIST_X_OFFSET * 2.0f)) / 2.0f,
        LOBBYROOMLIST_Y_OFFSET + FRAME_BUFFER_HEIGHT / 6.0f,
        (FRAME_BUFFER_WIDTH - (LOBBYROOMLIST_X_OFFSET * 2.0f)) / 2.0f,
        FRAME_BUFFER_HEIGHT / 6.0f);

    for (int i = 0; i < 4; ++i)
    {
        m_ReadyCard[i].d2dLayoutRect = m_ReadyBitmaps[i].d2dLayoutRect;
        m_ReadyCard[i].m_hide = true;
    }

    //로비에서 출력할 방 리스트 영역
    for (int i = 0; i < LobbyRoomSlotCount; ++i)
    {
        m_RoomListLayout[i] = MakeLayoutRectByCorner(LOBBYROOMLIST_X_OFFSET
            , LOBBYROOMLIST_Y_OFFSET + (FRAME_BUFFER_HEIGHT / 2.0f * ((float)i / LobbyRoomSlotCount)),
            FRAME_BUFFER_WIDTH - (LOBBYROOMLIST_X_OFFSET + LOBBYROOMLIST_X_OFFSET2), FRAME_BUFFER_HEIGHT / 2.0f * (1.0f / LobbyRoomSlotCount));
        m_RoomListTextBlock[i].m_d2dLayoutRect = m_RoomListLayout[i];
        m_RoomListTextBlock[i].m_pdwFormat = m_LobbyTextFormat;
        m_RoomListTextBlock[i].m_pstrText =  L"ROOMNUM:   MEMBER:   0/4";
    }

    // 인게임 비트맵 로드
    // 캐릭터 프로필
    m_CharProfile[0].resource = LoadPngFromFile(L"UI/Char_UI_1.png"); // Boss
    m_CharProfile[1].resource = LoadPngFromFile(L"UI/Char_UI_2.png"); // Yellow
    m_CharProfile[2].resource = LoadPngFromFile(L"UI/Char_UI_3.png"); // Mask
    m_CharProfile[3].resource = LoadPngFromFile(L"UI/Char_UI_5.png"); // Goggle
    for (int i = 0; i < PLAYERNUM; ++i)
        m_CharProfile[i].d2dLayoutRect = MakeLayoutRectByCorner(FRAME_BUFFER_WIDTH * 0.01f, FRAME_BUFFER_HEIGHT * 0.1f* i, FRAME_BUFFER_WIDTH * 0.1f, FRAME_BUFFER_HEIGHT * 0.1f);

    // 상태 --> 동적으로 변하는 것이므로 그때 그때 위치를 업데이트하기로 한다. 일단 비트맵 리소스만 로드한다.
    m_CharStatusBitmaps[0] = LoadPngFromFile(L"UI/Normal.png");
    m_CharStatusBitmaps[1] = LoadPngFromFile(L"UI/Danger.png");
    m_CharStatusBitmaps[2] = LoadPngFromFile(L"UI/Dead.png");

    // HP
    m_HpBitmap = LoadPngFromFile(L"UI/HP.png");
    for (int i = 0; i < MAX_HP; ++i)
    {
        m_HPUi[i].resource = m_HpBitmap;
        m_HPUi[i].d2dLayoutRect = MakeLayoutRectByCorner(PROFILE_UI_OFFSET_X + BIG_PROFILE_UI_WIDTH + STATUS_UI_WIDTH * i,
            BIG_PROFILE_UI_OFFSET_Y + BIG_PROFILE_UI_WIDTH / 4.0, STATUS_UI_WIDTH, STATUS_UI_HEIGHT);
    }
    // 크로스 헤드
    m_CharCrossHead.resource = LoadPngFromFile(L"UI/crossHair.png");
    m_CharCrossHead.d2dLayoutRect = MakeLayoutRect(CENTER_X, CENTER_Y,10.f,10.f);
    m_CharCrossHead.m_hide = false;

    // 피격 이펙트
    for (int i = 0; i < m_nAttackedUI; i++)
    {
        m_AttackedOpacity[i] = 0.5f;
    }
    m_AttackedEffect[0].resource = LoadPngFromFile(L"UI/blood_base.png");
    m_AttackedEffect[0].d2dLayoutRect = MakeLayoutRectByCorner(0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT);
    m_AttackedEffect[0].m_hide = false;

    m_AttackedEffect[1].resource = LoadPngFromFile(L"UI/blood_outline.png");
    m_AttackedEffect[1].d2dLayoutRect = MakeLayoutRectByCorner(0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT);
    m_AttackedEffect[1].m_hide = false;

    m_AttackedEffect[2].resource = LoadPngFromFile(L"UI/blood_frame.png");
    m_AttackedEffect[2].d2dLayoutRect = MakeLayoutRectByCorner(0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT);
    m_AttackedEffect[2].m_hide = false;

    m_AttackedEffect[3].resource = LoadPngFromFile(L"UI/bullet_hole_glass.png");
    m_AttackedEffect[3].d2dLayoutRect = MakeLayoutRect(3*FRAME_BUFFER_WIDTH / 4, 3*FRAME_BUFFER_HEIGHT / 4,FRAME_BUFFER_WIDTH/2, FRAME_BUFFER_HEIGHT/2);
    m_AttackedEffect[3].m_hide = false;

    m_AttackedEffect[4].resource = LoadPngFromFile(L"UI/bullet_hole_glass.png");
    m_AttackedEffect[4].d2dLayoutRect = MakeLayoutRect( FRAME_BUFFER_WIDTH / 4, FRAME_BUFFER_HEIGHT / 4, FRAME_BUFFER_WIDTH / 3, FRAME_BUFFER_HEIGHT / 3);
    m_AttackedEffect[4].m_hide = false;




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
        m_GenerateUIButtons[i].resource = LoadPngFromFile(filename.c_str());
        m_GenerateUIButtons[i].d2dLayoutRect = MakeLayoutRect(FRAME_BUFFER_WIDTH / 2.0, FRAME_BUFFER_HEIGHT * 0.8, FRAME_BUFFER_WIDTH / 2, FRAME_BUFFER_HEIGHT / 4.f);
        m_GenerateUIButtons[i].m_hide = true;
    }
    m_GenerateUIButtons[21].resource = LoadPngFromFile(L"UI/F.png");
    m_GenerateUIButtons[21].d2dLayoutRect = MakeLayoutRect(FRAME_BUFFER_WIDTH / 2.0, FRAME_BUFFER_HEIGHT * 0.9, FRAME_BUFFER_WIDTH * 0.1f, FRAME_BUFFER_HEIGHT * 0.1f);
    m_GenerateUIButtons[21].m_hide = true;

    // 살리기 아이콘
    m_RescueIcon.resource = LoadPngFromFile(L"UI/Rescue.png");
    m_RescueIcon.d2dLayoutRect = m_GenerateUIButtons[21].d2dLayoutRect;
    m_RescueIcon.m_hide = true;

    m_RescueGuage.m_hide = true;
    m_RescueGuage.d2dLayoutRect[0] = MakeLayoutRect(CENTER_X, CENTER_Y, MAX_RESCUE_GUAGE,50);
    m_RescueGuage.d2dLayoutRect[1] = MakeLayoutRect(CENTER_X, CENTER_Y, MAX_RESCUE_GUAGE,50);
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
