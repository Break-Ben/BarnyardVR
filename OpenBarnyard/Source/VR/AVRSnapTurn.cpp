#include "pch.h"
#include "AVRSnapTurn.h"
#include "AVRManager.h"
#include "AVRSettings.h"
#include <Input/TInputDeviceController.h>

#include <cmath>
#include <unordered_map>

#include <Core/TMemoryDebugOn.h>

namespace
{
// Hysteresis avoids jitter/repeats near the activation threshold. A held stick
// never auto-repeats: return towards centre before another snap, even to reverse.
constexpr TFLOAT                                                 ACTIVATE = 0.7f;
constexpr TFLOAT                                                 RELEASE  = 0.3f;
std::unordered_map<const void*, std::unordered_map<TINT, TBOOL>> s_latched;
std::unordered_map<const void*, TFLOAT>                          s_pending;
} // namespace

TBOOL AVRSnapTurn::FilterStickYaw( const Toshi::TInputInterface::InputEvent* a_pEvent, TFLOAT& a_rfDegrees )
{
	a_rfDegrees = 0.0f;
	if ( !a_pEvent->GetSource()->IsA( &TGetClass( Toshi::TInputDeviceController ) ) )
		return TFALSE; // Never snap mouse deltas or keyboard look commands.

	const TFLOAT x       = a_pEvent->GetMagnitudeFloat( 0 );
	TBOOL&       latched = s_latched[ a_pEvent->GetSource() ][ a_pEvent->GetDoodad() ];
	const TINT   angle   = AVRSettings::GetSingleton().GetSnapTurnDegrees();
	if ( !AVRManager::GetSingleton().IsActive() || angle == 0 )
	{
		latched = std::isfinite( x ) && std::abs( x ) > RELEASE;
		return TFALSE;
	}
	if ( !std::isfinite( x ) ) return TTRUE;
	if ( std::abs( x ) <= RELEASE ) latched = TFALSE;
	else if ( !latched && std::abs( x ) >= ACTIVATE )
	{
		latched     = TTRUE;
		a_rfDegrees = x > 0.0f ? TFLOAT( angle ) : -TFLOAT( angle );
	}
	return TTRUE;
}

void AVRSnapTurn::QueueYaw( const void* a_pOwner, TFLOAT a_fDegrees )
{
	if ( a_fDegrees != 0.0f ) s_pending[ a_pOwner ] += a_fDegrees;
}

TFLOAT AVRSnapTurn::ConsumeYaw( const void* a_pOwner )
{
	const auto it = s_pending.find( a_pOwner );
	if ( it == s_pending.end() ) return 0.0f;
	const TFLOAT degrees = it->second;
	s_pending.erase( it );
	return degrees;
}
