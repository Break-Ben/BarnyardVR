#pragma once
#include "AInputMap.h"

// Live inline prompt tokens keep already-formatted localized text device-aware.
// The token stores a map NAME (not a potentially dangling map pointer) and command.
namespace AInputPrompts
{
void          Initialise();
void          Shutdown();
TWCHAR        GetToken( const Toshi::TPString8& a_rMapName, AInputCommand a_eCommand );
const TWCHAR* GetContinueText( const TWCHAR* a_wszKeyboardText );
void          SetOpenXRActive( TBOOL a_bActive );
TBOOL         IsOpenXRActive();
TUINT32       GetRevision();
TBOOL         ContainsToken( const TWCHAR* a_wszText );
} // namespace AInputPrompts
