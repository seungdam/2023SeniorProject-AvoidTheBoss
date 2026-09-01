#pragma once

#include "GameUiSnapshot.h"

#include <optional>

// UI매니저 개요
// 게임 프레임워크 시작 시, 각 화면에 필요한 모든 UI 이미지를 로드한다. WCI 컨버터를 활용
// D2D를 활용해서 이미지를 그린다.
// LobbyButton

const int32 MAX_HP = 3;

struct UITextBlock
{
    std::wstring                    m_pstrText; // 출력할 텍스처
    D2D1_RECT_F                     m_d2dLayoutRect; // 출력할 레이아웃 영역
    IDWriteTextFormat*              m_pdwFormat = NULL; // 입력 포맷
    ID2D1SolidColorBrush*           m_pd2dTextBrush= NULL; // 텍스처를 출력할 브러쉬
    bool                            m_hide = false;
};
struct UIButton
{
    D2D1_RECT_F                     d2dLayoutRect; // 출력할 레이아웃 영역
    ID2D1Bitmap*                    resource = NULL;
    bool                            m_hide = false;
};

struct ANIMButton
{
    D2D1_RECT_F d2dLayoutRect;
    ID2D1Bitmap* resource = NULL;
    bool m_hide = false;
    float animTime = 1.0;
};
struct UIBackGround
{
    D2D1_RECT_F                     d2dLayoutRect; // 출력할 레이아웃 영역
    ID2D1Bitmap*                    resource = NULL;
};

struct GuageUI
{
    bool                            m_hide;
    D2D1_RECT_F                     d2dLayoutRect[2]; // 테두리랑 다 채운 용도 하나
};

struct InGameUI
{
    D2D1_RECT_F                     d2dLayoutRect; // 출력할 레이아웃 영역
    ID2D1Bitmap*                    resource = NULL;
    bool                            m_hide = false;
};

#define FULL_UI_OPACITY_VALUE 0.8f
class UIManager
{
public:
    enum class LoginFeedback : uint8
    {
        LoginOk = 0,
        LoginFailed = 1,
        RegistrationOk = 2
    };

    enum class UiHitTarget : uint8
    {
        TitleId,
        TitlePassword,
        TitleLogin,
        TitleRegister,
        TitleQuit,
        LobbyEnter,
        LobbyCreate,
        LobbyLogout,
        RoomReady,
        RoomLeave
    };

    static constexpr int32 LobbyRoomSlotCount = static_cast<int32>(kLobbyUiRoomCount);

    UIManager(ID2D1DeviceContext2* d2dContext, IDWriteFactory* writeFactory, UINT nWidth, UINT nHeight);
    ~UIManager();

    void ShowLoginFeedback(LoginFeedback feedback) noexcept;
    [[nodiscard]] std::optional<LoginFeedback> TickLoginFeedback(float elapsedSeconds) noexcept;
    [[nodiscard]] bool HitTest(UiHitTarget target, const POINT& point) const noexcept;
    [[nodiscard]] bool TrySelectLobbyRoomSlot(int32 slot, const POINT& point) noexcept;
    void AppendCredential(int32 field, wchar_t character);
    void BackspaceCredential(int32 field);
    [[nodiscard]] const std::wstring& CredentialText(int32 field) const noexcept;

    ID2D1Bitmap1* LoadPngFromFile(const wchar_t* filePath);

    void UpdateRoomTextBlocks(UINT nIndex,const WCHAR* pstrUIText, const D2D1_RECT_F& pd2dLayoutRect, bool hide);
    void UpdateLobbySceneUI(const LobbyUiSnapshot& snapshot);
    void UpdateRoomSceneUI(const RoomUiSnapshot& snapshot);
    void UpdateResultSceneUI(const ResultUiSnapshot& snapshot);



    void Render2D(int32 curScene, int32 localPlayerIndex);
    void ReleaseResources();

    void DrawOtherSceneBackGround(int32 Scene);
    void DrawOtherSceneUI(int32 Scene, int32 idx);
    void DrawOtherSceneUITextBlock(int32 Scene);
    void InitGameSceneUI(const GameUiSnapshot& snapshot);
    void UpdateGameSceneUI(const GameUiSnapshot& snapshot);
    void DrawGameSceneUI(int32 Scene, int32 localPlayerIndex);

    ID2D1SolidColorBrush* CreateBrush(D2D1::ColorF d2dColor);
    IDWriteTextFormat* CreateTextFormat(const WCHAR* pszFontName, float fFontSize);
private:
    void InitializeResources();

    // WindowInfo
    float                           m_fWidth = 0.0f;
    float                           m_fHeight = 0.0f;

    // D2DRenderer가 소유하며 UIManager보다 오래 산다.
    ID2D1DeviceContext2*            m_pd2dDeviceContext = nullptr;
    IDWriteFactory*                 m_pd2dWriteFactory = nullptr;

    // TextBlock
    UINT                           m_nTextBlocks = 0;
    IDWriteTextFormat*             m_TitleTextFormat = nullptr;
    IDWriteTextFormat*             m_LobbyTextFormat = nullptr;



    // 배경 레이어 비트맵들
    UIBackGround m_backGround[5];

    // 타이틀 전용
    ANIMButton m_LoginResult[3];

    // 버튼 비트맵들
    UIButton m_TitleButtons[3];

    // 로비 전용
    UIButton m_LobbyButtons[3];
    UITextBlock m_RoomListTextBlock[5];

    int32 m_lastRoomPage = 0;
    int32 m_selectedLayout = -1;
    D2D1_RECT_F m_RoomListLayout[5];

    // 방 전용
    UIButton m_RoomButtons[2];

    // 인 게임
    InGameUI m_RescueIcon; // 살리기 UI
    InGameUI m_CharCrossHead; // 조준선 UI

    int m_nAttackedUI = 5; // 피격 UI 텍스쳐 수
    InGameUI m_AttackedEffect[5]; // 피격 UI 텍스쳐

    float m_AttackedOpacity[5]; // 피격 UI 투명도
    float m_CrossHeadOpacity = 0.8f; // 조준선 투명도


    // 인 게임 전용
    UIButton                   m_GenerateUIButtons[23];
    UIButton                   m_CharProfile[4]; // 다른 캐릭터 초상화 표시

    ID2D1Bitmap*               m_CharStatusBitmaps[3] = {}; // 비트맵 리소스를 가져와서 공유한다.
    ID2D1Bitmap*               m_HpBitmap = nullptr;

    D2D_RECT_F                 m_myProfileLayout; // 자기 캐릭터 레이아웃

    InGameUI                   m_CharStatus[3]; // 캐릭터 상태
    InGameUI                   m_HPUi[MAX_HP];    // 캐릭터 HP
    GuageUI                    m_RescueGuage;

    // 동적으로 바뀌는 텍스트 버튼들 Id,PW
    UITextBlock                m_IDPWTextBlocks[2];
    // 레디 버튼
    UIButton                   m_ReadyBitmaps[4];
    UIButton                   m_ReadyCard[4];


    // 결과창
    UITextBlock m_ResultTextBlock[2];
    RoomUiSnapshot m_roomUiSnapshot;
    ResultUiSnapshot m_resultUiSnapshot;

    // 레이어 위치 출력을 위한 브러시
    ID2D1SolidColorBrush* redBrush = nullptr; // 빨강
    ID2D1SolidColorBrush* grayBrush = nullptr; // 회색
    ID2D1SolidColorBrush* blackBrush = nullptr; // 회색
    ID2D1SolidColorBrush* whiteBrush = nullptr;
    ID2D1SolidColorBrush* greenBrush = nullptr;
};

