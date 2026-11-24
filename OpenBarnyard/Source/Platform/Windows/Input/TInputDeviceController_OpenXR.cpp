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
constexpr TINT kStickIntScale = 32000;

static TFLOAT ClampAxis( TFLOAT a_fValue )
{
	if ( !std::isfinite( a_fValue ) )
		return 0.0f;

	return std::max( -1.0f, std::min( a_fValue, 1.0f ) );
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
    , m_afRawSticks {}
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

	// OpenXR reports stick Y as positive-up; the game's standard controller axis
	// convention is positive-down, and gameplay negates that axis for forward motion.
	m_afRawSticks[ 0 ] = ClampAxis( oState.fLeftStickX );
	m_afRawSticks[ 1 ] = ClampAxis( -oState.fLeftStickY );
	m_afRawSticks[ 2 ] = ClampAxis( oState.fRightStickX );
	m_afRawSticks[ 3 ] = ClampAxis( -oState.fRightStickY );

	// Use the same per-axis deadzone and saturation as the Windows gamepad.
	// Bike power slides require full steering, even with a little Y input.
	for ( TINT i = 0; i < TARRAYSIZE( m_afSticks ); i++ )
	{
		const TINT iAxis = TINT( m_afRawSticks[ i ] * TInputDXDeviceController::STICK_CENTRE ) + TInputDXDeviceController::STICK_CENTRE;
		m_afSticks[ i ] = TInputDXDeviceController::NormaliseStick( iAxis );
	}

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
	return TINT( GetAxisFloat( a_iDoodad, a_iAxis ) * kStickIntScale );
}

TINT TInputOpenXRDeviceController::GetAxisInt2( Doodad a_iDoodad, TINT a_iAxis ) const
{
	return TINT( GetAxisFloat2( a_iDoodad, a_iAxis ) * kStickIntScale );
}

TFLOAT TInputOpenXRDeviceController::GetAxisFloat( Doodad a_iDoodad, TINT a_iAxis ) const
{
	if ( a_iAxis < 0 || a_iAxis > 1 )
		return 0.0f;

	if ( a_iDoodad == STICK_LEFT )
		return m_afSticks[ a_iAxis ];
	if ( a_iDoodad == STICK_RIGHT )
		return m_afSticks[ 2 + a_iAxis ];

	return 0.0f;
}

void TInputOpenXRDeviceController::GetRadialStick( Doodad a_iDoodad, TFLOAT& a_rfX, TFLOAT& a_rfY ) const
{
	constexpr TFLOAT kStickDeadzone = 7500.0f / 32000.0f;
	a_rfX = GetAxisFloat2( a_iDoodad, 0 );
	a_rfY = GetAxisFloat2( a_iDoodad, 1 );

	const TFLOAT fMagnitude = std::sqrt( a_rfX * a_rfX + a_rfY * a_rfY );
	if ( fMagnitude <= kStickDeadzone )
	{
		a_rfX = 0.0f;
		a_rfY = 0.0f;
		return;
	}

	const TFLOAT fAdjustedMagnitude = std::min( ( fMagnitude - kStickDeadzone ) / ( 1.0f - kStickDeadzone ), 1.0f );
	const TFLOAT fScale = fAdjustedMagnitude / fMagnitude;
	a_rfX *= fScale;
	a_rfY *= fScale;
}

TFLOAT TInputOpenXRDeviceController::GetAxisFloat2( Doodad a_iDoodad, TINT a_iAxis ) const
{
	if ( a_iAxis < 0 || a_iAxis > 1 )
		return 0.0f;

	if ( a_iDoodad == STICK_LEFT )
		return m_afRawSticks[ a_iAxis ];
	if ( a_iDoodad == STICK_RIGHT )
		return m_afRawSticks[ 2 + a_iAxis ];

	return 0.0f;
}
