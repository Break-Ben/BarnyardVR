#pragma once
#include "ABYardMenuState.h"
#include "AMessagePopupState.h"

// Native AGUI2 page; the dummy controls deliberately do not affect gameplay.
class AVROptionsState : public ABYardMenuState
{
public:
	TDECLARE_CLASS( AVROptionsState, ABYardMenuState );

	~AVROptionsState();
	virtual void  OnInsertion() OVERRIDE;
	virtual void  OnRemoval() OVERRIDE;
	virtual TBOOL ProcessCommand( AInputCommand a_eCommand, const Toshi::TInputInterface::InputEvent* a_pEvent, TBOOL& a_rStopEvents ) OVERRIDE;
	virtual void  OnMenuItemActivated( AGUI2MenuItem* a_pItem ) OVERRIDE;
	virtual void  OnMenuClose() OVERRIDE;

private:
	enum ROW
	{
		ROW_TOGGLE,
		ROW_LEVEL,
		ROW_DEFAULTS,
		ROW_BACK,
		ROW_COUNT
	};
	void                   RefreshLabels();
	void                   Adjust( TINT a_iRow, TINT a_iDirection );
	static TBOOL __stdcall OnResetResponse( AVROptionsState* a_pState, AMessagePopupState* a_pPopup, AMessagePopupState::ResponseEvent* a_pEvent );

	AGUI2Button                                                                              m_aButtons[ ROW_COUNT ];
	TWCHAR                                                                                   m_awszLabels[ ROW_COUNT ][ 96 ];
	Toshi::TListener<AMessagePopupState, AMessagePopupState::ResponseEvent, AVROptionsState> m_oResetListener;
};
