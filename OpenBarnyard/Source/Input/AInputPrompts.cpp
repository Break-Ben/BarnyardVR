#include "pch.h"
#include "AInputPrompts.h"
#include "AInputMapManager.h"
#include "AInputHandler.h"
#include "VR/AVRSettings.h"
#include "Locale/ALocaleManager.h"
#include "VR/AVRManager.h"

#include <Input/TInputDeviceController.h>
#include <GUI/T2GUI.h>
#include <GUI/T2GUIFont.h>
#include <GUI/T2GUIRenderer.h>
#include <Render/TRenderInterface.h>
#include <Render/TTextureFactory.h>
#include <File/TFile.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

namespace
{
using Pad                   = TInputDeviceController;
constexpr TINT FIRST_TOKEN  = 0xE000;
constexpr TINT MAX_TOKENS   = 2048;
constexpr TINT CELL_SIZE    = 64;
constexpr TINT FAMILY_COUNT = 5;
struct Token
{
	TPString8     map;
	AInputCommand command;
	TINT          doodad = -1; // Fixed physical controls for the title/instruction screen.
};
std::vector<Token> s_tokens;
T2GUIMaterial*     s_material     = TNULL;
TTexture*          s_texture      = TNULL;
TBOOL              s_initialised  = TFALSE;
TBOOL              s_openXRActive = TFALSE;

const TWCHAR* ControlLabel( AVRControlIcon icon )
{
	static constexpr const TWCHAR* labels[] = {
		L"A (L)", L"B (L)", L"X (L)", L"Y (L)", L"A (R)", L"B (R)", L"X (R)", L"Y (R)",
		L"Left trigger", L"Right trigger", L"Left grip", L"Right grip",
		L"Left stick", L"Right stick", L"Left stick click", L"Right stick click",
		L"Menu", L"Both stick clicks", L"Left pad", L"Right pad", L"Left pad click", L"Right pad click",
		L"D-pad", L"Up", L"Right", L"Down", L"Left", L"Unbound"
	};
	static_assert( TARRAYSIZE( labels ) == TINT( AVRControlIcon::Count ) );
	return labels[ TINT( icon ) ];
}

TBOOL Resolve( const T2GUIFont* font, TWCHAR character, T2GUIFont::InlineGlyph& glyph )
{
	const TINT index = TINT( character ) - FIRST_TOKEN;
	if ( index < 0 || index >= TINT( s_tokens.size() ) || !font->GetFontDef() ) return TFALSE;
	const auto& token     = s_tokens[ index ];
	auto*       manager   = AInputMapManager::GetSingleton();
	auto*       map       = token.doodad == -1 ? manager->FindMap( token.map ) : TNULL;
	const auto  doodad    = token.doodad != -1 ? token.doodad : map ? map->GetCommandDoodad( token.command, !AInputHandler::GetSingleton()->IsControllerUsed() ) :
	                                                                  -1;
	const TBOOL vrControl = AInputHandler::GetSingleton()->IsControllerUsed() && s_openXRActive && doodad >= Pad::BUTTON_DPAD_UP && doodad <= Pad::VIRTSTK_DPAD;
	const auto  prompt    = vrControl ? AVRManager::GetSingleton().GetControllerPrompt( doodad ) : AVRControllerPrompt{};
	if ( vrControl && s_material )
	{
		const TINT column = TINT( prompt.icon );
		const TINT row    = TINT( prompt.family );
		// Native button glyphs can extend above/below the text's line height.
		// Match their actual ink size and baseline, not just the text advance.
		const auto* reference = font->GetNativeCharData( L'\x0F0B' );
		glyph.width = glyph.height = reference ? reference->ui8CharHeight : font->GetFontDef()->uiLineHeight;
		glyph.yOffset              = reference ? reference->iYOffset : 0.0f;
		glyph.material             = s_material;
		const TFLOAT width         = TFLOAT( CELL_SIZE * TINT( AVRControlIcon::Count ) );
		const TFLOAT height        = TFLOAT( CELL_SIZE * FAMILY_COUNT );
		// Half-texel inset plus a transparent border prevent adjacent-cell bleed.
		glyph.uv1 = { ( column * CELL_SIZE + 0.5f ) / width, ( row * CELL_SIZE + 0.5f ) / height };
		glyph.uv2 = { ( ( column + 1 ) * CELL_SIZE - 0.5f ) / width, ( ( row + 1 ) * CELL_SIZE - 0.5f ) / height };
		return TTRUE;
	}

	const TWCHAR* name = TNULL;
	if ( doodad != -1 && !manager->GetDoodadCustomName( doodad, name ) )
		manager->GetDoodadLocalisedName( doodad, name );
	// Do not fall back to a PS2 icon for missing OpenXR artwork/metadata.
	if ( vrControl ) name = ControlLabel( prompt.icon );
	glyph.text  = name ? name : L"?";
	glyph.width = std::clamp( std::ceil( const_cast<T2GUIFont*>( font )->GetTextWidth( glyph.text ) - font->GetFontDef()->iLetterSpacing ), 0.0f, 255.0f );
	return TTRUE;
}
} // namespace

void AInputPrompts::Initialise()
{
	if ( s_initialised ) return;
	s_initialised = TTRUE;
	// DDS is copied by the existing Content/Common build/deployment step.
	// Keep native text fallback functional on other renderer targets.
#if defined( TRENDERINTERFACE_DX11 ) && defined( TOSHI_SKU_WINDOWS ) && !defined( TOSHI_SKU_WASM )
	constexpr const char* path = "Data/VR/ControllerPrompts.dds";
	if ( auto* file = TFile::Create( path, TFILEMODE_READ ) )
	{
		delete file;
		auto* factory = TRenderInterface::GetSingleton()->GetSystemResource<TTextureFactory>( SYSRESOURCE_TEXTUREFACTORY );
		s_texture     = factory->CreateTextureFromFile( path, 0 );
		if ( s_texture && s_texture->GetWidth() == CELL_SIZE * TINT( AVRControlIcon::Count ) && s_texture->GetHeight() == CELL_SIZE * FAMILY_COUNT )
			s_material = T2GUI::GetRenderer()->CreateMaterial( s_texture );
	}
	if ( !s_material ) TWARN( "OpenXR prompt atlas unavailable; using neutral VR labels.\n" );
#endif
	T2GUIFont::ms_fnInlineGlyph = Resolve;
}

void AInputPrompts::Shutdown()
{
	T2GUIFont::ms_fnInlineGlyph = TNULL;
	if ( s_material ) T2GUI::GetRenderer()->DestroyMaterial( s_material );
	if ( s_texture ) s_texture->DestroyResource();
	s_material = TNULL;
	s_texture  = TNULL;
	s_tokens.clear();
	s_initialised  = TFALSE;
	s_openXRActive = TFALSE;
}

TWCHAR AInputPrompts::GetToken( const TPString8& map, AInputCommand command )
{
	if ( !s_initialised ) return 0;
	for ( TINT i = 0; i < TINT( s_tokens.size() ); ++i )
		if ( s_tokens[ i ].map == map && s_tokens[ i ].command == command ) return TWCHAR( FIRST_TOKEN + i );
	if ( s_tokens.size() >= MAX_TOKENS ) return 0;
	s_tokens.push_back( { map, command } );
	return TWCHAR( FIRST_TOKEN + s_tokens.size() - 1 );
}

void  AInputPrompts::SetOpenXRActive( TBOOL active ) { s_openXRActive = active; }
TBOOL AInputPrompts::IsOpenXRActive() { return s_openXRActive; }

TBOOL AInputPrompts::ContainsToken( const TWCHAR* text )
{
	if ( !text ) return TFALSE;
	for ( ; *text; ++text )
		if ( *text >= FIRST_TOKEN && *text < FIRST_TOKEN + MAX_TOKENS ) return TTRUE;
	return TFALSE;
}

TUINT32 AInputPrompts::GetRevision()
{
	static TUINT32 revision        = 1;
	static TUINT32 previousProfile = 0;
	static TUINT32 previousStyle   = ~0u;
	const TUINT32  profile         = AVRManager::GetSingleton().GetControllerPromptRevision();
	auto*          input           = AInputHandler::GetSingleton();
	const TUINT32  style           = TUINT32( s_openXRActive ) | ( TUINT32( input->IsControllerUsed() ) << 1 ) |
	    ( TUINT32( input->IsControllerXInput() ) << 2 ) | ( TUINT32( AVRSettings::GetSingleton().IsModernControls() ) << 3 ) |
	    ( TUINT32( g_pLocaleManager->GetLanguage() ) << 4 );
	if ( previousProfile != profile || previousStyle != style )
	{
		previousProfile = profile;
		previousStyle   = style;
		++revision;
	}
	return revision;
}

const TWCHAR* AInputPrompts::GetContinueText( const TWCHAR* keyboardText )
{
	if ( !s_initialised || !AInputHandler::GetSingleton()->IsControllerUsed() ) return keyboardText;
	auto buttonToken = []( TINT doodad ) -> TWCHAR {
		for ( TINT i = 0; i < TINT( s_tokens.size() ); ++i )
			if ( s_tokens[ i ].doodad == doodad ) return TWCHAR( FIRST_TOKEN + i );
		if ( s_tokens.size() >= MAX_TOKENS ) return 0;
		s_tokens.push_back( { TPString8(), AInputCommand_Unknown, doodad } );
		return TWCHAR( FIRST_TOKEN + s_tokens.size() - 1 );
	};
	const TWCHAR a     = buttonToken( Pad::BUTTON_CONTEXT_3 ); // virtual bottom face button
	const TWCHAR b     = buttonToken( Pad::BUTTON_CONTEXT_2 ); // virtual right face button
	const TWCHAR start = buttonToken( Pad::BUTTON_START );
	if ( !a || !b || !start ) return L"Press a button";
	static TWCHAR facePrompt[]  = L"Press ? or ?";
	static TWCHAR startPrompt[] = L"Press ?";
	facePrompt[ 6 ]             = a;
	facePrompt[ 11 ]            = b;
	startPrompt[ 6 ]            = start;
	if ( s_openXRActive &&
	     ( AVRManager::GetSingleton().GetControllerPrompt( Pad::BUTTON_CONTEXT_3 ).icon == AVRControlIcon::Unavailable ||
	       AVRManager::GetSingleton().GetControllerPrompt( Pad::BUTTON_CONTEXT_2 ).icon == AVRControlIcon::Unavailable ) )
		return startPrompt;
	return facePrompt;
}
