#include "pch.h"
#include "AVROptionsState.h"
#include "VR/AVRSettings.h"
#include "GUI/T2GUIFontManager.h"
#include "HUD/AHudManager.h"
#include "HUD/AMenuNavHudElement.h"
#include "Sound/ASoundManager.h"
#include "SoundBank/ui.h"

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
	std::swprintf( m_awszLabels[ ROW_CONTROLS ], 96, L"Control Layout: %ls", settings.IsModernControls() ? L"Modern" : L"Classic" );
	if ( settings.GetSnapTurnDegrees() == 0 )
		std::swprintf( m_awszLabels[ ROW_SNAP_TURN ], 96, L"Snap Turning: Disabled" );
	else
		std::swprintf( m_awszLabels[ ROW_SNAP_TURN ], 96, L"Snap Turning: %d degrees", settings.GetSnapTurnDegrees() );
	std::swprintf( m_awszLabels[ ROW_MOVEMENT ], 96, L"Movement: %ls", settings.IsHeadRelativeMovement() ? L"Head Relative" : L"Fixed" );
	std::swprintf( m_awszLabels[ ROW_UI_PLACEMENT ], 96, L"UI Position: %ls", settings.IsUIFollowingHead() ? L"Head Relative" : L"Fixed" );
	std::swprintf( m_awszLabels[ ROW_VIBRATION ], 96, L"Vibration: %ls", settings.IsVibrationEnabled() ? L"On" : L"Off" );
	std::swprintf( m_awszLabels[ ROW_RESOLUTION ], 96, L"Resolution: %d%% (restart)", settings.GetResolutionPercent() );
	if ( settings.GetMSAASamples() == 1 )
		std::swprintf( m_awszLabels[ ROW_MSAA ], 96, L"MSAA: Off (restart)" );
	else
		std::swprintf( m_awszLabels[ ROW_MSAA ], 96, L"MSAA: %dx (restart)", settings.GetMSAASamples() );
	std::swprintf( m_awszLabels[ ROW_DEBUG_HUD ], 96, L"Debug HUD: %ls", settings.IsDebugHUDEnabled() ? L"On" : L"Off" );
	std::swprintf( m_awszLabels[ ROW_DEFAULTS ], 96, L"Restore Defaults" );
	for ( TINT i = 0; i < ROW_COUNT; ++i )
		m_aButtons[ i ].GetTextBox().SetText( m_awszLabels[ i ] );
}

void AVROptionsState::OnInsertion()
{
	m_iOriginalSnapTurnDegrees = AVRSettings::GetSingleton().GetSnapTurnDegrees();
	m_bOriginalModernControls  = AVRSettings::GetSingleton().IsModernControls();
	m_bOriginalHeadRelativeMovement = AVRSettings::GetSingleton().IsHeadRelativeMovement();
	m_bOriginalUIFollowsHead = AVRSettings::GetSingleton().IsUIFollowingHead();
	m_bOriginalVibration     = AVRSettings::GetSingleton().IsVibrationEnabled();
	m_iOriginalResolutionPercent = AVRSettings::GetSingleton().GetResolutionPercent();
	m_iOriginalMSAASamples       = AVRSettings::GetSingleton().GetMSAASamples();
	m_bOriginalDebugHUD          = AVRSettings::GetSingleton().IsDebugHUDEnabled();
	m_bConfirmed               = TFALSE;
	RefreshLabels();
	T2GUIFont* pFont = T2GUIFontManager::FindFont( AGUI2STYLE_FONT_PRIMARY );
	for ( TINT i = 0; i < ROW_COUNT; ++i )
	{
		auto& button = m_aButtons[ i ];
		// Keep the expanded list inside the existing dialog at standard GUI sizes.
		button.Create( pFont, 440.0f, m_awszLabels[ i ], 2.5f );
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
	m_oDialogTitle.SetText( L"VR Settings" );
	BaseClass::OnInsertion();
}

void AVROptionsState::OnRemoval()
{
	m_oResetListener.Disconnect();
	// Restore on forced removal too. A successful save advances this snapshot.
	AVRSettings::GetSingleton().SetSnapTurnDegrees( m_iOriginalSnapTurnDegrees );
	AVRSettings::GetSingleton().SetModernControls( m_bOriginalModernControls );
	AVRSettings::GetSingleton().SetHeadRelativeMovement( m_bOriginalHeadRelativeMovement );
	AVRSettings::GetSingleton().SetUIFollowingHead( m_bOriginalUIFollowsHead );
	AVRSettings::GetSingleton().SetVibrationEnabled( m_bOriginalVibration );
	AVRSettings::GetSingleton().SetResolutionPercent( m_iOriginalResolutionPercent );
	AVRSettings::GetSingleton().SetMSAASamples( m_iOriginalMSAASamples );
	AVRSettings::GetSingleton().SetDebugHUDEnabled( m_bOriginalDebugHUD );
	for ( auto& button : m_aButtons ) button.RemoveSelf();
	AMenuNavHudElement::UpdateText();
	BaseClass::OnRemoval();
}

void AVROptionsState::OnActivate()
{
	BaseClass::OnActivate();
	// Same localised, device-aware Confirm/Cancel hints as the audio settings.
	AHudManager::GetSingleton()->FindElement<AMenuNavHudElement>()->SetTexts( 1050, 1051, -1 );
}

void AVROptionsState::Adjust( TINT a_iRow, TINT a_iDirection )
{
	if ( a_iRow == ROW_CONTROLS )
	{
		auto& settings = AVRSettings::GetSingleton();
		settings.SetModernControls( !settings.IsModernControls() );
		RefreshLabels();
		return;
	}
	if ( a_iRow == ROW_MOVEMENT || a_iRow == ROW_UI_PLACEMENT )
	{
		auto& settings = AVRSettings::GetSingleton();
		if ( a_iRow == ROW_MOVEMENT )
			settings.SetHeadRelativeMovement( !settings.IsHeadRelativeMovement() );
		else
			settings.SetUIFollowingHead( !settings.IsUIFollowingHead() );
		RefreshLabels();
		return;
	}
	if ( a_iRow == ROW_VIBRATION )
	{
		auto& settings = AVRSettings::GetSingleton();
		settings.SetVibrationEnabled( !settings.IsVibrationEnabled() );
		RefreshLabels();
		return;
	}
	if ( a_iRow == ROW_DEBUG_HUD )
	{
		auto& settings = AVRSettings::GetSingleton();
		settings.SetDebugHUDEnabled( !settings.IsDebugHUDEnabled() );
		RefreshLabels();
		return;
	}
	if ( a_iRow == ROW_RESOLUTION )
	{
		auto& settings = AVRSettings::GetSingleton();
		TINT  percent  = settings.GetResolutionPercent() + a_iDirection * AVRSettings::RESOLUTION_STEP;
		if ( percent < AVRSettings::RESOLUTION_MIN ) percent = AVRSettings::RESOLUTION_MAX;
		if ( percent > AVRSettings::RESOLUTION_MAX ) percent = AVRSettings::RESOLUTION_MIN;
		settings.SetResolutionPercent( percent );
		RefreshLabels();
		return;
	}
	if ( a_iRow == ROW_MSAA )
	{
		auto&          settings = AVRSettings::GetSingleton();
		constexpr TINT count    = sizeof( AVRSettings::MSAA_SAMPLES ) / sizeof( AVRSettings::MSAA_SAMPLES[ 0 ] );
		for ( TINT i = 0; i < count; ++i )
		{
			if ( AVRSettings::MSAA_SAMPLES[ i ] == settings.GetMSAASamples() )
			{
				settings.SetMSAASamples( AVRSettings::MSAA_SAMPLES[ ( i + a_iDirection + count ) % count ] );
				break;
			}
		}
		RefreshLabels();
		return;
	}
	if ( a_iRow != ROW_SNAP_TURN ) return;
	auto&          settings = AVRSettings::GetSingleton();
	constexpr TINT count    = sizeof( AVRSettings::SNAP_ANGLES ) / sizeof( AVRSettings::SNAP_ANGLES[ 0 ] );
	for ( TINT i = 0; i < count; ++i )
	{
		if ( AVRSettings::SNAP_ANGLES[ i ] == settings.GetSnapTurnDegrees() )
		{
			settings.SetSnapTurnDegrees( AVRSettings::SNAP_ANGLES[ ( i + a_iDirection + count ) % count ] );
			break;
		}
	}
	RefreshLabels();
}

TBOOL AVROptionsState::ProcessCommand( AInputCommand a_eCommand, const TInputInterface::InputEvent* a_pEvent, TBOOL& a_rStopEvents )
{
	if ( m_eMenuState == MENUSTATE_MENU_VISIBLE && !m_bIgnoreInputs && a_eCommand == AInputCommand_Cancel )
	{
		ASoundManager::GetSingleton()->PlayCue( soundbank::UI_MENUBACK );
		m_bConfirmed = TFALSE;
		m_eMenuState = MENUSTATE_MENU_DISAPPEAR;
		return TTRUE;
	}
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
	if ( row == ROW_SNAP_TURN || row == ROW_CONTROLS || row == ROW_MOVEMENT || row == ROW_UI_PLACEMENT || row == ROW_VIBRATION || row == ROW_RESOLUTION || row == ROW_MSAA || row == ROW_DEBUG_HUD )
	{
		m_bConfirmed = TTRUE;
		m_eMenuState = MENUSTATE_MENU_DISAPPEAR;
	}
	else if ( row == ROW_DEFAULTS )
	{
		auto* pPopup = new AMessagePopupState( L"Restore Defaults", L"Reset all VR settings to their defaults?", TPString8(), TFALSE, AInputCommand_Yes, AInputCommand_No, AInputCommand_Empty, AInputCommand_Empty, TFALSE, 60.0f, 0 );
		m_oResetListener.Connect( pPopup->GetResponseEmitter(), this, OnResetResponse, 0 );
		AGameStateController::GetSingleton()->PushState( pPopup );
	}
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
	if ( !m_bConfirmed )
	{
		AVRSettings::GetSingleton().SetSnapTurnDegrees( m_iOriginalSnapTurnDegrees );
		AVRSettings::GetSingleton().SetModernControls( m_bOriginalModernControls );
		AVRSettings::GetSingleton().SetHeadRelativeMovement( m_bOriginalHeadRelativeMovement );
		AVRSettings::GetSingleton().SetUIFollowingHead( m_bOriginalUIFollowsHead );
		AVRSettings::GetSingleton().SetVibrationEnabled( m_bOriginalVibration );
		AVRSettings::GetSingleton().SetResolutionPercent( m_iOriginalResolutionPercent );
		AVRSettings::GetSingleton().SetMSAASamples( m_iOriginalMSAASamples );
		AVRSettings::GetSingleton().SetDebugHUDEnabled( m_bOriginalDebugHUD );
		AGameState::Remove();
		return;
	}
	if ( AVRSettings::GetSingleton().Save() )
	{
		m_iOriginalSnapTurnDegrees = AVRSettings::GetSingleton().GetSnapTurnDegrees();
		m_bOriginalModernControls  = AVRSettings::GetSingleton().IsModernControls();
		m_bOriginalHeadRelativeMovement = AVRSettings::GetSingleton().IsHeadRelativeMovement();
		m_bOriginalUIFollowsHead = AVRSettings::GetSingleton().IsUIFollowingHead();
		m_bOriginalVibration     = AVRSettings::GetSingleton().IsVibrationEnabled();
		m_iOriginalResolutionPercent = AVRSettings::GetSingleton().GetResolutionPercent();
		m_iOriginalMSAASamples       = AVRSettings::GetSingleton().GetMSAASamples();
		m_bOriginalDebugHUD          = AVRSettings::GetSingleton().IsDebugHUDEnabled();
		AGameState::Remove();
		return;
	}
	// Keep edits for retry, but Cancel must still restore the entry snapshot.
	m_bConfirmed = TFALSE;
	OnActivate();
	auto* pPopup = new AMessagePopupState( L"VR Settings", L"Could not save VR settings. Check the save folder is writable, then try again.", TPString8(), TTRUE, AInputCommand_OK, AInputCommand_Empty, AInputCommand_Empty, AInputCommand_Empty, TFALSE, 60.0f, 0 );
	AGameStateController::GetSingleton()->PushState( pPopup );
}
