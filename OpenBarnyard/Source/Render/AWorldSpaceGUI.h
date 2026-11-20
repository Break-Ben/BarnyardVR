#pragma once
#include <GUI/T2GUIElement.h>
#include <Math/TMatrix44.h>

// Opt-in GUI subtrees/text drawn in the stereo world pass. Existing GUI drawing
// (including fonts and masks) is reused, never flattened onto the HUD quad.
// Sidecars and GUI hooks preserve inherited instance layouts.
class AWorldSpaceGUI
{
public:
	using DrawCallback = void ( * )( void* );
	static TBOOL IsActive();
	static void SetAnchor( Toshi::T2GUIElement& a_rElement, const Toshi::TVector4& a_rcPosition, TFLOAT a_fScale = 0.02f, TBOOL a_bMasked = TFALSE, TBOOL a_bUsePivot = TFALSE );
	static void SetFrame( Toshi::T2GUIElement& a_rElement, const Toshi::TMatrix44& a_rcFrame );
	static void Register( Toshi::T2GUIElement& a_rElement, DrawCallback a_fnDraw );
	static void Remove( const Toshi::T2GUIElement* a_pElement );
	static void RenderAll();
	static void Draw( const Toshi::TVector4& a_rcPosition, TFLOAT a_fScale, DrawCallback a_fnDraw, void* a_pData );
};
