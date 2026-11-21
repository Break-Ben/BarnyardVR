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

static void ApplyStickDeadzone( TFLOAT& a_rfX, TFLOAT& a_rfY )
{
	const TFLOAT fMagnitude = std::sqrt( a_rfX * a_rfX + a_rfY * a_rfY );
	if ( fMagnitude <= kStickDeadzone )
	{
		a_rfX = 0.0f;
		a_rfY = 0.0f;
		return;
	}

	// Use a radial deadzone and normalize by vector length. Per-axis deadzones
	// leave a diagonal short of full magnitude even when the stick is at its rim.
	const TFLOAT fAdjustedMagnitude = std::min( ( fMagnitude - kStickDeadzone ) / ( kStickSaturate - kStickDeadzone ), 1.0f );
	const TFLOAT fScale = fAdjustedMagnitude / fMagnitude;
	a_rfX *= fScale;
	a_rfY *= fScale;
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

	static const TBOOL bTraceInput = T2String8::FindString( GetCommandLineA(), "-vrinputtrace" ) != TNULL;
	if ( bTraceInput && m_uiButtonMask != oState.uiButtonMask )
		TUtil::Log( "[vrinput-adapter] time=%lu device=%p oldMask=0x%X newMask=0x%X\n",
		    GetTickCount(), static_cast<void*>( this ), m_uiButtonMask, oState.uiButtonMask );
	m_uiButtonMask = oState.uiButtonMask;

	// OpenXR reports stick Y as positive-up; the game's standard controller axis
	// convention is positive-down, and gameplay negates that axis for forward motion.
	m_afRawSticks[ 0 ] = ClampAxis( oState.fLeftStickX );
	m_afRawSticks[ 1 ] = ClampAxis( -oState.fLeftStickY );
	m_afRawSticks[ 2 ] = ClampAxis( oState.fRightStickX );
	m_afRawSticks[ 3 ] = ClampAxis( -oState.fRightStickY );

	for ( TINT i = 0; i < TARRAYSIZE( m_afSticks ); i++ )
		m_afSticks[ i ] = m_afRawSticks[ i ];
	ApplyStickDeadzone( m_afSticks[ 0 ], m_afSticks[ 1 ] );
	ApplyStickDeadzone( m_afSticks[ 2 ], m_afSticks[ 3 ] );

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
