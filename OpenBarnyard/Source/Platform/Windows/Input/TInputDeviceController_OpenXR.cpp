#include "pch.h"
#include "Platform/Windows/Input/TInputDeviceController_OpenXR.h"
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
constexpr TFLOAT kStickDeadzone = 7500.0f / 32000.0f;
constexpr TFLOAT kStickSaturate = 1.0f;
constexpr TINT   kStickIntScale = 32000;

static TFLOAT ClampAxis( TFLOAT a_fValue )
{
	if ( !std::isfinite( a_fValue ) )
		return 0.0f;

	return std::max( -1.0f, std::min( a_fValue, 1.0f ) );
}

static TFLOAT ApplyStickDeadzone( TFLOAT a_fValue )
{
	a_fValue = ClampAxis( a_fValue );
	const TFLOAT fMagnitude = std::fabs( a_fValue );

	if ( fMagnitude <= kStickDeadzone )
		return 0.0f;

	const TFLOAT fAdjusted = std::min( ( fMagnitude - kStickDeadzone ) / ( kStickSaturate - kStickDeadzone ), 1.0f );
	return std::copysign( fAdjusted, a_fValue );
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
	SetAcquired( TFALSE );
	m_bPolled = TFALSE;
	return TTRUE;
}

void TInputOpenXRDeviceController::Release()
{
	m_bPolled = TFALSE;
}

void TInputOpenXRDeviceController::Update( TFLOAT a_fDeltaTime )
{
	m_uiOldButtonMask = m_uiButtonMask;

	AVRManager::ControllerInputState oState;
	if ( !AVRManager::GetSingleton().GetControllerInputState( oState ) )
		oState = {};

	m_uiButtonMask = oState.uiButtonMask;

	// OpenXR reports stick Y as positive-up; the game's standard controller axis
	// convention is positive-down, and gameplay negates that axis for forward motion.
	m_afRawSticks[ 0 ] = ClampAxis( oState.fLeftStickX );
	m_afRawSticks[ 1 ] = ClampAxis( -oState.fLeftStickY );
	m_afRawSticks[ 2 ] = ClampAxis( oState.fRightStickX );
	m_afRawSticks[ 3 ] = ClampAxis( -oState.fRightStickY );

	for ( TINT i = 0; i < TARRAYSIZE( m_afSticks ); i++ )
		m_afSticks[ i ] = ApplyStickDeadzone( m_afRawSticks[ i ] );

	UpdateVirtualButtons();
	m_bPolled = TTRUE;
}

TINT TInputOpenXRDeviceController::ProcessEvents( EventEmitter& a_rEmitter, TFLOAT a_fDeltaTime )
{
	if ( !m_bPolled )
		return 0;

	TINT iNumProcessed = ProcessRepeats( a_rEmitter, a_fDeltaTime );
	iNumProcessed += ProcessVirtualButtons( a_rEmitter, a_fDeltaTime );

	for ( TINT i = 0; i < 16; i++ )
	{
		const Doodad iDoodad = BUTTON_DPAD_UP + i;
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
