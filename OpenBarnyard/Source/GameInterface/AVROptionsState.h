#pragma once
#include "ABYardMenuState.h"
#include "AMessagePopupState.h"

// Native AGUI2 page for port-only settings.
class AVROptionsState : public ABYardMenuState
{
public:
	TDECLARE_CLASS( AVROptionsState, ABYardMenuState );

	~AVROptionsState();
	virtual TBOOL OnUpdate( TFLOAT a_fDeltaTime ) OVERRIDE;
	virtual TBOOL ProcessInput( const Toshi::TInputInterface::InputEvent* a_pEvent ) OVERRIDE;
	virtual void  OnInsertion() OVERRIDE;
	virtual void  OnRemoval() OVERRIDE;
	virtual void  OnActivate() OVERRIDE;
	virtual TBOOL ProcessCommand( AInputCommand a_eCommand, const Toshi::TInputInterface::InputEvent* a_pEvent, TBOOL& a_rStopEvents ) OVERRIDE;
	virtual void  OnMenuItemActivated( AGUI2MenuItem* a_pItem ) OVERRIDE;
	virtual void  OnMenuClose() OVERRIDE;

private:
	enum ROW
	{
		ROW_CONTROLS,
		ROW_SNAP_TURN,
		ROW_TOGGLE_SPRINT,
		ROW_MOVEMENT,
		ROW_UI_PLACEMENT,
		ROW_VIBRATION,
		ROW_RESOLUTION,
		ROW_MSAA,
		ROW_DEBUG_HUD,
		ROW_DEFAULTS,
		ROW_COUNT
	};
	static constexpr TINT VISIBLE_ROWS = 5;
	void                   RefreshLabels();
	void                   ShowRows();
	void                   FocusRow( TINT a_iRow );
	void                   UpdateScrollIndicators();
	void                   Adjust( TINT a_iRow, TINT a_iDirection );
	static TBOOL __stdcall OnResetResponse( AVROptionsState* a_pState, AMessagePopupState* a_pPopup, AMessagePopupState::ResponseEvent* a_pEvent );

	TBOOL m_bOriginalModernControls  = TTRUE;
	TBOOL m_bOriginalHeadRelativeMovement = TTRUE;
	TBOOL m_bOriginalUIFollowsHead = TTRUE;
	TBOOL m_bOriginalToggleSprint = TFALSE;
	TINT  m_iFirstVisible = 0;
	TBOOL m_bOriginalVibration     = TTRUE;
	TINT  m_iOriginalSnapTurnDegrees = 0;
	TINT  m_iOriginalResolutionPercent = 100;
	TINT  m_iOriginalMSAASamples       = 2;
	TBOOL m_bOriginalDebugHUD = TFALSE;
	TBOOL m_bConfirmed               = TFALSE;

	TBOOL m_bHasScrollArrows = TFALSE;
	Toshi::T2GUIRectangle m_aScrollArrows[ 2 ];
	AGUI2Button                                                                              m_aButtons[ ROW_COUNT ];
	TWCHAR                                                                                   m_awszLabels[ ROW_COUNT ][ 96 ];
	Toshi::TListener<AMessagePopupState, AMessagePopupState::ResponseEvent, AVROptionsState> m_oResetListener;
};
