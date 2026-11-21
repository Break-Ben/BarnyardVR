#pragma once
#include <Input/TInputInterface.h>

// Shared joystick-only snap detection. Does not alter event data or input mappings.
namespace AVRSnapTurn
{
// True means joystick yaw was handled (including neutral/held-stick frames).
// Output is a signed step in degrees, zero until the next deflection.
TBOOL FilterStickYaw( const Toshi::TInputInterface::InputEvent* a_pEvent, TFLOAT& a_rfDegrees );

// Pending steps for legacy helpers without spare fields; keeps their binary layouts intact.
void   QueueYaw( const void* a_pOwner, TFLOAT a_fDegrees );
TFLOAT ConsumeYaw( const void* a_pOwner );
} // namespace AVRSnapTurn
