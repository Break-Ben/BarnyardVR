#include "pch.h"
#include "AVROptionsState.h"
#include "VR/AVRSettings.h"
#include "GUI/T2GUIFontManager.h"

#include <cwchar>
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

TDEFINE_CLASS( AVROptionsState );

AVROptionsState::~AVROptionsState()
{
	m_oResetListener.Disconnect();
}

void AVROptionsState::RefreshLabels()
{
	const auto& settings = AVRSettings::GetSingleton();
	std::swprintf( m_awszLabels[ ROW_TOGGLE ], 96, L"Dummy Toggle: %ls", settings.IsDummyEnabled() ? L"On" : L"Off" );
	std::swprintf( m_awszLabels[ ROW_LEVEL ], 96, L"Dummy Level: %d / %d", settings.GetDummyLevel(), AVRSettings::DUMMY_LEVEL_MAX );
	std::swprintf( m_awszLabels[ ROW_DEFAULTS ], 96, L"Restore VR Defaults" );
	std::swprintf( m_awszLabels[ ROW_BACK ], 96, L"Save and Back" );
	for ( TINT i = 0; i < ROW_COUNT; ++i )
		m_aButtons[ i ].GetTextBox().SetText( m_awszLabels[ i ] );
}

void AVROptionsState::OnInsertion()
{
	RefreshLabels();
	T2GUIFont* pFont = T2GUIFontManager::FindFont( AGUI2STYLE_FONT_PRIMARY );
	for ( TINT i = 0; i < ROW_COUNT; ++i )
	{
		auto& button = m_aButtons[ i ];
		button.Create( pFont, 440.0f, m_awszLabels[ i ], 10.0f );
		button.SetId( i );
		button.SetFocusedColour( AGUI2STYLE_COLOR_FOCUSED );
		button.SetEnabledColour( AGUI2STYLE_COLOR_SECONDARY );
		button.SetDisabledColour( AGUI2STYLE_COLOR_DISABLED );
		button.SetFocusedScale( 1.0f );
		button.OnFocusLost();
		button.SetTextShadow( TTRUE, 0xa0000000, 0.6f );
		button.SetTextShadowOffset( 5.0f );
		m_oMenu.AddMenuItem( button );
	}
	m_oMenu.SetFocusAt( m_aButtons[ 0 ] );
	m_oDialogTitle.SetText( L"VR Settings (Dummy Options)" );
	BaseClass::OnInsertion();
}

void AVROptionsState::OnRemoval()
{
	m_oResetListener.Disconnect();
	// Also cover forced removal; normal Back uses OnMenuClose and reports errors.
	if ( !AVRSettings::GetSingleton().Save() )
		TWARN( "VR settings remain unsaved after menu removal\n" );
	for ( auto& button : m_aButtons ) button.RemoveSelf();
	BaseClass::OnRemoval();
}

void AVROptionsState::Adjust( TINT a_iRow, TINT a_iDirection )
{
	auto& settings = AVRSettings::GetSingleton();
	if ( a_iRow == ROW_TOGGLE )
		settings.SetDummyEnabled( !settings.IsDummyEnabled() );
	else if ( a_iRow == ROW_LEVEL )
		settings.SetDummyLevel( settings.GetDummyLevel() + a_iDirection );
	RefreshLabels();
}

TBOOL AVROptionsState::ProcessCommand( AInputCommand a_eCommand, const TInputInterface::InputEvent* a_pEvent, TBOOL& a_rStopEvents )
{
	if ( m_eMenuState == MENUSTATE_MENU_VISIBLE && !m_bIgnoreInputs &&
	     ( a_eCommand == AInputCommand_Left || a_eCommand == AInputCommand_Right ) )
	{
		AGUI2MenuItem* pItem = m_oMenu.GetFocusedMenuItem();
		if ( pItem )
			Adjust( TSTATICCAST( AGUI2Button, pItem )->GetId(), a_eCommand == AInputCommand_Left ? -1 : 1 );
		a_pEvent->StartRepeat( 0.5f, 0.1f );
		return TTRUE;
	}
	return BaseClass::ProcessCommand( a_eCommand, a_pEvent, a_rStopEvents );
}

void AVROptionsState::OnMenuItemActivated( AGUI2MenuItem* a_pItem )
{
	const TINT row = TSTATICCAST( AGUI2Button, a_pItem )->GetId();
	if ( row == ROW_TOGGLE || row == ROW_LEVEL )
	{
		// Confirm cycles the value too, so mouse-only navigation works.
		if ( row == ROW_LEVEL && AVRSettings::GetSingleton().GetDummyLevel() == AVRSettings::DUMMY_LEVEL_MAX )
			AVRSettings::GetSingleton().SetDummyLevel( AVRSettings::DUMMY_LEVEL_MIN );
		else
			Adjust( row, 1 );
		RefreshLabels();
	}
	else if ( row == ROW_DEFAULTS )
	{
		auto* pPopup = new AMessagePopupState( L"Restore VR Defaults", L"Reset all VR settings to their defaults?", TPString8(), TFALSE, AInputCommand_No, AInputCommand_Yes, AInputCommand_Empty, AInputCommand_Empty, TFALSE, 60.0f, 0 );
		m_oResetListener.Connect( pPopup->GetResponseEmitter(), this, OnResetResponse, 0 );
		AGameStateController::GetSingleton()->PushState( pPopup );
	}
	else if ( row == ROW_BACK )
		m_eMenuState = MENUSTATE_MENU_DISAPPEAR;
}

TBOOL __stdcall AVROptionsState::OnResetResponse( AVROptionsState* a_pState, AMessagePopupState* a_pPopup, AMessagePopupState::ResponseEvent* a_pEvent )
{
	if ( a_pEvent->eButton == AInputCommand_Yes )
	{
		AVRSettings::GetSingleton().RestoreDefaults();
		a_pState->RefreshLabels();
	}
	a_pState->m_oResetListener.Disconnect();
	a_pPopup->Remove();
	return TFALSE;
}

void AVROptionsState::OnMenuClose()
{
	if ( AVRSettings::GetSingleton().Save() )
	{
		AGameState::Remove();
		return;
	}
	// Keep edited values and the page available so the user can retry saving.
	OnActivate();
	auto* pPopup = new AMessagePopupState( L"VR Settings", L"Could not save VR settings. Check the save folder is writable, then try again.", TPString8(), TTRUE, AInputCommand_OK, AInputCommand_Empty, AInputCommand_Empty, AInputCommand_Empty, TFALSE, 60.0f, 0 );
	AGameStateController::GetSingleton()->PushState( pPopup );
}
