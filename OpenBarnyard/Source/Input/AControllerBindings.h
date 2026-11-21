#pragma once
#include "AInputMap.h"

// A single effective map is shared by dispatch and reverse (prompt) lookup.
// Only controller action buttons move: axes, navigation, mouse and keys stay native.
namespace AControllerBindings
{
using Map = Toshi::T2Map<Toshi::TInputDevice::Doodad, AInputMap::CommandBinding>;
Map& Resolve( AInputMap& a_rOwner, Map& a_rOriginal, TBOOL a_bModern );
void Invalidate( const AInputMap* a_pOwner );
Map* ForEvent( AInputMap& a_rMap, const Toshi::TInputInterface::InputEvent* a_pEvent );
} // namespace AControllerBindings
