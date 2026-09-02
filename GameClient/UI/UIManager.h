#pragma once

#include "GameUiSnapshot.h"

#include <optional>

// UI매니저 개요
// 게임 프레임워크 시작 시, 각 화면에 필요한 모든 UI 이미지를 로드한다. WCI 컨버터를 활용
// D2D를 활용해서 이미지를 그린다.
// LobbyButton

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

    static constexpr int32 LobbyRoomSlotCount =
        static_cast<int32>(atb::client::ui::LobbyRoomSlotCount);
    static constexpr int32 HealthIconCount = 3;
    static constexpr float FullOpacity = 0.8f;

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
    float                           wndWidth = 0.0f;
    float                           wndHeight = 0.0f;

    // D2DRenderer가 소유하며 UIManager보다 오래 산다.
    ID2D1DeviceContext2*            _pd2dDeviceContext = nullptr;
    IDWriteFactory*                 _pd2dWriteFactory = nullptr;

    // TextBlock
    UINT                           _textBlockCount = 0;
    IDWriteTextFormat*             _pTitleTextFormat = nullptr;
    IDWriteTextFormat*             _pLobbySceneTextFormat = nullptr;



    // 배경 레이어 비트맵들
    UIBackGround _backGroundBitmaps[5];

    // 타이틀 전용
    ANIMButton _loginPopUpBtns[3];

    // 버튼 비트맵들
    UIButton _titleSceneBtns[3];

    // 로비 전용
    UIButton _lobbySceneBtns[3];
    UITextBlock _roomListTextBlocks[5];

    int32 _lastRoomPage = 0;
    int32 _selectedRow = -1;
    D2D1_RECT_F _roomListLayout[5];

    // 방 전용
    UIButton _roomBtns[2];

    // 인 게임
    InGameUI _rescueIcon; // 살리기 UI
    InGameUI _crossHeadIcon; // 조준선 UI

    int _attackedUICount = 5; // 피격 UI 텍스쳐 수
    InGameUI _attackedEffects[5]; // 피격 UI 텍스쳐

    float _attackedEffectOpacity[5]; // 피격 UI 투명도
    float _crossHeadOpacity = 0.8f; // 조준선 투명도


    // 인 게임 전용
    UIButton                   _generateBtns[23];
    UIButton                   _charProfileBtns[4]; // 다른 캐릭터 초상화 표시

    ID2D1Bitmap*               _charStatusBitmaps[3] = {}; // 비트맵 리소스를 가져와서 공유한다.
    ID2D1Bitmap*               _HpBitmap = nullptr;

    D2D_RECT_F                 m_myProfileLayout; // 자기 캐릭터 레이아웃

    InGameUI                   m_CharStatus[3]; // 캐릭터 상태
    InGameUI                   m_HPUi[HealthIconCount];    // 캐릭터 HP
    GuageUI                    m_RescueGuage;

    // 동적으로 바뀌는 텍스트 버튼들 Id,PW
    UITextBlock                _idpwTextBlocks[2];
    // 레디 버튼
    UIButton                   _readyBtns[4];
    UIButton                   _userCardBtns[4];


    // 결과창
    UITextBlock _resultTextBlocks[2];
    RoomUiSnapshot _roomUiSnapshot;
    ResultUiSnapshot _resultUiSnapshot;

    // 레이어 위치 출력을 위한 브러시
    ID2D1SolidColorBrush* _redBrush = nullptr; // 빨강
    ID2D1SolidColorBrush* _grayBrush = nullptr; // 회색
    ID2D1SolidColorBrush* _blackBrush = nullptr; // 회색
    ID2D1SolidColorBrush* _whiteBrush = nullptr;
    ID2D1SolidColorBrush* _greenBrush = nullptr;
};

