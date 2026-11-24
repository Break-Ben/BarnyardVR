#include "pch.h"
#include "Platform/Windows/Input/TInputDeviceController_OpenXR.h"
#include <Platform/Windows/Input/TInputDeviceController_Win.h>
#include "VR/AVRManager.h"

#include <algorithm>
#include <cmath>

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

TDEFINE_CLASS( TInputOpenXRDeviceController );

namespace
{
static TFLOAT ClampAxis( TFLOAT a_fValue )
{
	if ( !std::isfinite( a_fValue ) )
		return 0.0f;

	return std::max( -1.0f, std::min( a_fValue, 1.0f ) );
}

// Convert to the signed XInput range before mapping to DirectInput axes.
static TINT ToXInputAxis( TFLOAT a_fValue )
{
	const TINT iValue = TINT( ClampAxis( a_fValue ) * TInputDXDeviceController::STICK_CENTRE );
	return std::max( -TInputDXDeviceController::STICK_CENTRE, std::min( iValue, TInputDXDeviceController::STICK_CENTRE - 1 ) );
}

static TBOOL IsButtonDoodad( TInputDevice::Doodad a_iDoodad )
{
	return TInputDeviceController::BUTTON_DPAD_UP <= a_iDoodad && a_iDoodad <= TInputDeviceController::BUTTON_SHOULDER_RIGHT_2;
}

static TUINT32 GetButtonMask( TInputDevice::Doodad a_iDoodad )
{
	return IsButtonDoodad( a_iDoodad ) ? 1u << ( a_iDoodad - TInputDeviceController::BUTTON_DPAD_UP ) : 0;
}
} // namespace

TInputOpenXRDeviceController::TInputOpenXRDeviceController()
    : m_uiButtonMask( 0 )
    , m_uiOldButtonMask( 0 )
    , m_afSticks {}
    , m_aiSticks { TInputDXDeviceController::STICK_CENTRE, TInputDXDeviceController::STICK_CENTRE - 1,
                   TInputDXDeviceController::STICK_CENTRE, TInputDXDeviceController::STICK_CENTRE - 1 }
    , m_bPolled( TFALSE )
{
}

TBOOL TInputOpenXRDeviceController::Acquire()
{
	SetAcquired( TTRUE );
	return TTRUE;
}

TBOOL TInputOpenXRDeviceController::Unacquire()
{
	m_oPauseButtons.Reset();
	SetAcquired( TFALSE );
	m_bPolled = TFALSE;
	return TTRUE;
}

void TInputOpenXRDeviceController::Release()
{
	m_oPauseButtons.Reset();
	m_bPolled = TFALSE;
}

void TInputOpenXRDeviceController::Update( TFLOAT a_fDeltaTime )
{
	m_uiOldButtonMask = m_uiButtonMask;

	AVRManager::ControllerInputState oState;
	if ( !AVRManager::GetSingleton().GetControllerInputState( oState ) )
	{
		oState = {};
		m_oPauseButtons.Reset();
	}

	m_uiButtonMask = m_oPauseButtons.Update( oState.uiButtonMask, a_fDeltaTime );

	// OpenXR and XInput report Y positive-up. Convert both sticks just like
	// XInput's GetDXAxis so events and direct axis queries match a gamepad.
	const TINT iCenter = TInputDXDeviceController::STICK_CENTRE;
	m_aiSticks[ 0 ] = ToXInputAxis( oState.fLeftStickX ) + iCenter;
	m_aiSticks[ 1 ] = iCenter - 1 - ToXInputAxis( oState.fLeftStickY );
	m_aiSticks[ 2 ] = ToXInputAxis( oState.fRightStickX ) + iCenter;
	m_aiSticks[ 3 ] = iCenter - 1 - ToXInputAxis( oState.fRightStickY );

	// Use the same per-axis deadzone and saturation as Windows gamepads.
	for ( TINT i = 0; i < TARRAYSIZE( m_afSticks ); i++ )
		m_afSticks[ i ] = TInputDXDeviceController::NormaliseStick( m_aiSticks[ i ] );

	UpdateVirtualButtons();
	m_bPolled = TTRUE;
}

TINT TInputOpenXRDeviceController::ProcessEvents( EventEmitter& a_rEmitter, TFLOAT a_fDeltaTime )
{
	if ( !m_bPolled )
		return 0;

	TINT iNumProcessed = ProcessRepeats( a_rEmitter, a_fDeltaTime );
	iNumProcessed += ProcessVirtualButtons( a_rEmitter, a_fDeltaTime );

	// Match the physical button transition order of the Windows gamepad.
	for ( TINT i = 0; i < TInputDXDeviceController::NUM_DX_BUTTONS; i++ )
	{
		const Doodad iDoodad = TInputDXDeviceController::TranslateDXButtonToDoodad( i );
		const TBOOL  bIsDown = ( m_uiButtonMask & GetButtonMask( iDoodad ) ) != 0;
		const TBOOL  bWasDown = ( m_uiOldButtonMask & GetButtonMask( iDoodad ) ) != 0;

		if ( bIsDown && !bWasDown )
		{
			a_rEmitter.Throw( TInputInterface::InputEvent( this, iDoodad, TInputInterface::EVENT_TYPE_GONE_DOWN ) );
			iNumProcessed++;
		}
		else if ( !bIsDown && bWasDown )
		{
			a_rEmitter.Throw( TInputInterface::InputEvent( this, iDoodad, TInputInterface::EVENT_TYPE_GONE_UP ) );
			iNumProcessed++;
		}
	}

	a_rEmitter.Throw( TInputInterface::InputEvent( this, STICK_LEFT, TInputInterface::EVENT_TYPE_VIRTUAL, m_afSticks[ 0 ], m_afSticks[ 1 ] ) );
	a_rEmitter.Throw( TInputInterface::InputEvent( this, STICK_RIGHT, TInputInterface::EVENT_TYPE_VIRTUAL, m_afSticks[ 2 ], m_afSticks[ 3 ] ) );

	return iNumProcessed + 2;
}

TBOOL TInputOpenXRDeviceController::IsDown( Doodad a_iDoodad ) const
{
	if ( IsButtonDoodad( a_iDoodad ) )
		return ( m_uiButtonMask & GetButtonMask( a_iDoodad ) ) != 0;

	return TInputDeviceController::IsDown( a_iDoodad );
}

TBOOL TInputOpenXRDeviceController::WasDown( Doodad a_iDoodad ) const
{
	if ( IsButtonDoodad( a_iDoodad ) )
		return ( m_uiOldButtonMask & GetButtonMask( a_iDoodad ) ) != 0;

	return TInputDeviceController::WasDown( a_iDoodad );
}

TINT TInputOpenXRDeviceController::GetAxisInt( Doodad a_iDoodad, TINT a_iAxis ) const
{
	const TINT iResult = GetAxisInt2( a_iDoodad, a_iAxis );
	if ( iResult <= TInputDXDeviceController::STICK_DEADZONE && iResult >= -TInputDXDeviceController::STICK_DEADZONE )
		return 0;
	return iResult;
}

TINT TInputOpenXRDeviceController::GetAxisInt2( Doodad a_iDoodad, TINT a_iAxis ) const
{
	if ( a_iAxis < 0 || a_iAxis > 1 || ( a_iDoodad != STICK_LEFT && a_iDoodad != STICK_RIGHT ) )
		return 0;

	const TINT iIndex = ( a_iDoodad == STICK_LEFT ? 0 : 2 ) + a_iAxis;
	TINT iResult = m_aiSticks[ iIndex ] - TInputDXDeviceController::STICK_CENTRE;
	TMath::Clip( iResult, -TInputDXDeviceController::STICK_SATURATE, TInputDXDeviceController::STICK_SATURATE );
	return iResult;
}

TFLOAT TInputOpenXRDeviceController::GetAxisFloat( Doodad a_iDoodad, TINT a_iAxis ) const
{
	return GetAxisInt( a_iDoodad, a_iAxis ) * ( 1.0f / TInputDXDeviceController::STICK_SATURATE );
}

TFLOAT TInputOpenXRDeviceController::GetAxisFloat2( Doodad a_iDoodad, TINT a_iAxis ) const
{
	return GetAxisInt2( a_iDoodad, a_iAxis ) * ( 1.0f / TInputDXDeviceController::STICK_SATURATE );
}
