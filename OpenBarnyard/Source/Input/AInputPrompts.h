#pragma once
#include "AInputMap.h"

namespace Toshi { class T2GUIRectangle; }

// Live inline prompt tokens keep already-formatted localized text device-aware.
// The token stores a map NAME (not a potentially dangling map pointer) and command.
namespace AInputPrompts
{
TBOOL         SetOpenXRRectangle( Toshi::T2GUIRectangle& a_rRectangle, const Toshi::TPString8& a_rMapName, AInputCommand a_eCommand );
void          Initialise();
void          Shutdown();
TWCHAR        GetToken( const Toshi::TPString8& a_rMapName, AInputCommand a_eCommand );
TWCHAR        GetLeftStickToken();
AInputCommand GetTokenCommand( TWCHAR a_wcToken );
const TWCHAR* GetContinueText( const TWCHAR* a_wszKeyboardText );
void          SetOpenXRActive( TBOOL a_bActive );
TUINT32       GetRevision();
TBOOL         ContainsToken( const TWCHAR* a_wszText );
} // namespace AInputPrompts
