#include "pch.h"
#include "AVROptionsState.h"
#include "VR/AVRSettings.h"
#include "GUI/T2GUIFontManager.h"
#include "GUI/T2GUI.h"
#include "GUI/T2GUITextureSectionManager.h"
#include "GUI/AGUI2.h"
#include <Input/TInputDeviceMouse.h>
#include "HUD/AHudManager.h"
#include "HUD/AMenuNavHudElement.h"
#include "Sound/ASoundManager.h"
#include "SoundBank/ui.h"

#include <cwchar>
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

TDEFINE_CLASS( AVROptionsState );

void AVROptionsState::ScrollPlank::Render()
{
	auto* pRenderer = T2GUI::GetRenderer();
	pRenderer->SetMaterial( GetMaterial() );
	pRenderer->SetColour( GetColour() );
	if ( IsInFront() ) pRenderer->ResetZCoordinate();

	const TFLOAT fHalfWidth = GetWidth() * 0.5f;
	const TFLOAT fHalfHeight = GetHeight() * 0.5f;
	TVector2 vertices[] = {
		{ -fHalfWidth, -fHalfHeight }, { fHalfWidth, -fHalfHeight },
		{ -fHalfWidth, fHalfHeight }, { fHalfWidth, fHalfHeight }
	};
	TVector2 uv1, uv2, uv3, uv4;
	GetUVs( uv1, uv2, uv3, uv4 );
	// Rotate the complete section 90 degrees, including its natural ends.
	TVector2 uvs[] = { uv4, uv1, uv3, uv2 };
	pRenderer->RenderTriStrip( vertices, uvs, 4 );
	T2GUIElement::Render();
}

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
	std::swprintf( m_awszLabels[ ROW_TOGGLE_SPRINT ], 96, L"Toggle Sprint: %ls", settings.IsToggleSprintEnabled() ? L"On" : L"Off" );
	std::swprintf( m_awszLabels[ ROW_MOVEMENT ], 96, L"Movement: %ls", settings.IsHeadRelativeMovement() ? L"Head Relative" : L"Fixed" );
	std::swprintf( m_awszLabels[ ROW_UI_PLACEMENT ], 96, L"UI Position: %ls", settings.IsUIFollowingHead() ? L"Head Relative" : L"Fixed" );
	std::swprintf( m_awszLabels[ ROW_VIBRATION ], 96, L"Vibration: %ls", settings.IsVibrationEnabled() ? L"On" : L"Off" );
	std::swprintf( m_awszLabels[ ROW_RESOLUTION ], 96, L"Resolution: %d%% (restart)", settings.GetResolutionPercent() );
	// Preserve the restart reminder without wrapping at the game's normal font size.
	auto* pFont = T2GUIFontManager::FindFont( AGUI2STYLE_FONT_PRIMARY );
	if ( pFont && pFont->GetTextWidth( m_awszLabels[ ROW_RESOLUTION ] ) > 440.0f )
		std::swprintf( m_awszLabels[ ROW_RESOLUTION ], 96, L"Res: %d%% (restart)", settings.GetResolutionPercent() );
	if ( settings.GetMSAASamples() == 1 )
		std::swprintf( m_awszLabels[ ROW_MSAA ], 96, L"MSAA: Off (restart)" );
	else
		std::swprintf( m_awszLabels[ ROW_MSAA ], 96, L"MSAA: %dx (restart)", settings.GetMSAASamples() );
	std::swprintf( m_awszLabels[ ROW_DEBUG_HUD ], 96, L"Debug HUD: %ls", settings.IsDebugHUDEnabled() ? L"On" : L"Off" );
	std::swprintf( m_awszLabels[ ROW_DEFAULTS ], 96, L"Restore Defaults" );
	for ( TINT i = 0; i < ROW_COUNT; ++i )
		m_aButtons[ i ].GetTextBox().SetText( m_awszLabels[ i ] );
}

void AVROptionsState::ShowRows()
{
	for ( TINT i = 0; i < ROW_COUNT; ++i )
		m_aButtons[ i ].SetVisible( i >= m_iFirstVisible && i < m_iFirstVisible + VISIBLE_ROWS );
}

void AVROptionsState::UpdateScrollBar()
{
	// Inset the rail vertically and keep a gap between it and the longest label.
	const TFLOAT fMenuHeight = m_oMenu.GetHeight();
	const TFLOAT fHeight = fMenuHeight * 0.86f;
	const TFLOAT fInset = ( fMenuHeight - fHeight ) * 0.5f;
	const TINT iVisible = TMath::Min( TINT( ROW_COUNT ), VISIBLE_ROWS );
	const TFLOAT fThumbHeight = fHeight * iVisible / ROW_COUNT;
	const TINT iMaxFirst = TMath::Max( TINT( ROW_COUNT ) - VISIBLE_ROWS, 0 );
	const TFLOAT fTop = iMaxFirst > 0 ? ( fHeight - fThumbHeight ) * m_iFirstVisible / iMaxFirst : 0.0f;
	const TFLOAT fX = m_oMenu.GetWidth() * 0.5f + 52.0f;

	m_oScrollTrack.SetVisible( iMaxFirst > 0 );
	m_oScrollRail.SetVisible( iMaxFirst > 0 );
	m_oScrollThumb.SetVisible( iMaxFirst > 0 );
	// The invisible hit area remains unrotated for reliable mouse picking.
	m_oScrollTrack.SetDimensions( 36.0f, fHeight );
	m_oScrollTrack.SetTransform( fX, fInset );
	// The texture is rotated in ScrollPlank::Render, keeping the dimensions
	// and hit area in the same upright coordinate system.
	m_oScrollRail.SetDimensions( 14.0f, fHeight );
	m_oScrollRail.SetTransform( fX, fInset + fHeight * 0.5f );
	m_oScrollThumb.SetDimensions( 24.0f, fThumbHeight );
	m_oScrollThumb.SetTransform( fX, fInset + fTop + fThumbHeight * 0.5f );
}

void AVROptionsState::FocusRow( TINT a_iRow )
{
	if ( a_iRow < 0 || a_iRow >= ROW_COUNT ) return;
	auto* pOldFocus = m_oMenu.GetFocusedMenuItem();
	if ( pOldFocus == &m_aButtons[ a_iRow ] ) return;
	if ( pOldFocus ) OnMenuItemFocused( *pOldFocus, m_aButtons[ a_iRow ] );

	TINT iFirst = m_iFirstVisible;
	if ( a_iRow < iFirst ) iFirst = a_iRow;
	else if ( a_iRow >= iFirst + VISIBLE_ROWS ) iFirst = a_iRow - VISIBLE_ROWS + 1;
	if ( iFirst != m_iFirstVisible )
	{
		m_iFirstVisible = iFirst;
		ShowRows();
		m_oMenu.ReflowChildrenVertically();
		m_oMenu.SetMenuAlpha( m_fMenuOpacity, m_fMenuOpacity * 0.6f );
		UpdateScrollBar();
	}
	m_oMenu.SetFocusAt( m_aButtons[ a_iRow ] );
	ASoundManager::GetSingleton()->PlayCue( soundbank::UI_MENUCLICK );
}

TBOOL AVROptionsState::OnUpdate( TFLOAT a_fDeltaTime )
{
	// OnMenuClose can remove this state during the base update, so do not access
	// members after calling it. The indicator follows the menu fade by one frame.
	m_oScrollRail.SetAlpha( m_fMenuOpacity * 0.8f );
	m_oScrollThumb.SetAlpha( m_fMenuOpacity );
	return BaseClass::OnUpdate( a_fDeltaTime );
}

TBOOL AVROptionsState::ProcessInput( const TInputInterface::InputEvent* a_pEvent )
{
	if ( m_eMenuState == MENUSTATE_MENU_VISIBLE && !m_bIgnoreInputs )
	{
		const auto type = a_pEvent->GetEventType();
		const TINT doodad = a_pEvent->GetDoodad();
		// Wheel-axis motion is not cursor motion. Let the wheel buttons below
		// scroll without causing a stale mouse position to steal focus.
		if ( doodad == TInputDeviceMouse::AXIS_WHEEL && type == TInputInterface::EVENT_TYPE_MOVED )
			return TTRUE;
		if ( ( type == TInputInterface::EVENT_TYPE_GONE_DOWN || type == TInputInterface::EVENT_TYPE_REPEAT ) &&
		     ( doodad == TInputDeviceMouse::BUTTON_WHEEL_FORWARD || doodad == TInputDeviceMouse::BUTTON_WHEEL_BACKWARD ) )
		{
			auto* pFocus = m_oMenu.GetFocusedMenuItem();
			if ( pFocus ) FocusRow( TSTATICCAST( AGUI2Button, pFocus )->GetId() + ( doodad == TInputDeviceMouse::BUTTON_WHEEL_FORWARD ? -1 : 1 ) );
			return TTRUE;
		}
		if ( type == TInputInterface::EVENT_TYPE_GONE_DOWN && doodad == TInputDeviceMouse::BUTTON_1 )
		{
			T2GUITransform inverse;
			m_oScrollTrack.GetInvScreenTransform( inverse );
			TVector2 pos;
			inverse.Transform( pos, AGUI2::GetMouseCursor().GetCursorPos() );
			if ( m_oScrollTrack.IsVisible() && m_oScrollTrack.IsPointInside( pos.x, pos.y ) )
			{
				const TFLOAT fFraction = ( pos.y + m_oScrollTrack.GetHeight() * 0.5f ) / m_oScrollTrack.GetHeight();
				FocusRow( TMath::Min( TINT( fFraction * ROW_COUNT ), TINT( ROW_COUNT ) - 1 ) );
				return TTRUE;
			}
			// Recalculate hover after a scroll, even when the mouse hasn't moved.
			m_oMenu.SetMouseStateDirty();
			m_oMenu.Update( 0.0f );
		}
	}
	return BaseClass::ProcessInput( a_pEvent );
}

void AVROptionsState::OnInsertion()
{
	m_iOriginalSnapTurnDegrees = AVRSettings::GetSingleton().GetSnapTurnDegrees();
	m_bOriginalModernControls  = AVRSettings::GetSingleton().IsModernControls();
	m_bOriginalHeadRelativeMovement = AVRSettings::GetSingleton().IsHeadRelativeMovement();
	m_bOriginalUIFollowsHead = AVRSettings::GetSingleton().IsUIFollowingHead();
	m_bOriginalVibration     = AVRSettings::GetSingleton().IsVibrationEnabled();
	m_bOriginalToggleSprint  = AVRSettings::GetSingleton().IsToggleSprintEnabled();
	m_iOriginalResolutionPercent = AVRSettings::GetSingleton().GetResolutionPercent();
	m_iOriginalMSAASamples       = AVRSettings::GetSingleton().GetMSAASamples();
	m_bOriginalDebugHUD          = AVRSettings::GetSingleton().IsDebugHUDEnabled();
	m_bConfirmed               = TFALSE;
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
		// Centre the combined text and its right-offset shadow visually.
		button.GetTextBox().SetTranslation( -2.5f, 0.0f );
		m_oMenu.AddMenuItem( button );
	}
	m_iFirstVisible = 0;
	ShowRows();
	m_oMenu.SetFocusAt( m_aButtons[ 0 ] );
	m_oDialogTitle.SetText( L"VR Settings" );
	BaseClass::OnInsertion();

	// Keep normal text size, leaving a separate gutter inside the wider frame.
	m_oMenu.SetAttachment( T2GUIATTACHMENT_MIDDLECENTER, T2GUIATTACHMENT_MIDDLECENTER );
	m_oMenu.SetTranslation( -28.0f, 0.0f );
	m_oDialog.SetDialogSize( m_oMenu.GetWidth() + 180.0f, TMath::Max( m_oMenu.GetHeight(), 128.0f ) + 128.0f );

	// Use the game's complete wooden plank for both pieces. Render the
	// lighter, wider thumb in front of the darker rail.
	auto* pWood = T2GUITextureSectionManager::GetTextureSection( "Panel_01" );
	m_oScrollRail.SetTextureSection( pWood );
	m_oScrollThumb.SetTextureSection( pWood );
	m_oScrollRail.SetColour( 0xff806b55 );
	m_oScrollThumb.SetColour( 0xffffffff );
	m_oScrollTrack.SetAttachment( T2GUIATTACHMENT_TOPCENTER, T2GUIATTACHMENT_TOPCENTER );
	m_oScrollRail.SetAttachment( T2GUIATTACHMENT_TOPCENTER, T2GUIATTACHMENT_MIDDLECENTER );
	m_oScrollThumb.SetAttachment( T2GUIATTACHMENT_TOPCENTER, T2GUIATTACHMENT_MIDDLECENTER );
	m_oScrollThumb.SetInFront();
	// AddChildTail renders older children first; the thumb must follow the rail.
	m_oMenu.AddChildTail( m_oScrollRail );
	m_oMenu.AddChildTail( m_oScrollThumb );
	m_oMenu.AddChildTail( m_oScrollTrack );
	UpdateScrollBar();
	m_oScrollRail.SetAlpha( 0.0f );
	m_oScrollThumb.SetAlpha( 0.0f );
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
	AVRSettings::GetSingleton().SetToggleSprintEnabled( m_bOriginalToggleSprint );
	AVRSettings::GetSingleton().SetResolutionPercent( m_iOriginalResolutionPercent );
	AVRSettings::GetSingleton().SetMSAASamples( m_iOriginalMSAASamples );
	AVRSettings::GetSingleton().SetDebugHUDEnabled( m_bOriginalDebugHUD );
	m_oScrollThumb.RemoveSelf();
	m_oScrollRail.RemoveSelf();
	m_oScrollTrack.RemoveSelf();
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
	if ( a_iRow == ROW_TOGGLE_SPRINT )
	{
		auto& settings = AVRSettings::GetSingleton();
		settings.SetToggleSprintEnabled( !settings.IsToggleSprintEnabled() );
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
	     ( a_eCommand == AInputCommand_Up || a_eCommand == AInputCommand_Forward ||
	       a_eCommand == AInputCommand_Down || a_eCommand == AInputCommand_Backward ) )
	{
		if ( a_pEvent->GetEventType() == TInputInterface::EVENT_TYPE_GONE_DOWN ||
		     a_pEvent->GetEventType() == TInputInterface::EVENT_TYPE_REPEAT )
		{
			auto* pFocus = m_oMenu.GetFocusedMenuItem();
			if ( pFocus ) FocusRow( TSTATICCAST( AGUI2Button, pFocus )->GetId() +
			    ( a_eCommand == AInputCommand_Up || a_eCommand == AInputCommand_Forward ? -1 : 1 ) );
			a_pEvent->StartRepeat( 0.5f, 0.1f );
		}
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
	if ( row >= 0 && row < ROW_DEFAULTS )
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
		AVRSettings::GetSingleton().SetToggleSprintEnabled( m_bOriginalToggleSprint );
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
		m_bOriginalToggleSprint  = AVRSettings::GetSingleton().IsToggleSprintEnabled();
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
