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
#include <GUI/T2GUIRectangle.h>
#include <GUI/T2GUIFontManager.h>
#include <GUI/T2GUIRenderer.h>
#include <Render/TRenderInterface.h>
#include <Render/TTextureFactory.h>
#include <File/TFile.h>
#include <algorithm>
#include <cmath>
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
	TBOOL         stickGlyph = TFALSE; // Explicit stick artwork, also for ordinary gamepads.
};
std::vector<Token> s_tokens;
T2GUIMaterial*     s_material        = TNULL;
TTexture*          s_texture         = TNULL;
T2GUIMaterial*     s_gamepadStickMaterial = TNULL;
TTexture*          s_gamepadStickTexture  = TNULL;
T2GUIFont*         s_smallPromptFont = TNULL;
T2GUIFont*         s_menuGlyphFont   = TNULL;
TBOOL              s_initialised     = TFALSE;
TBOOL              s_openXRActive    = TFALSE;

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
	auto prompt = vrControl ? AVRManager::GetSingleton().GetControllerPrompt( doodad ) : AVRControllerPrompt{};
	const TBOOL gamepadStick = token.stickGlyph && AInputHandler::GetSingleton()->IsControllerUsed() && !s_openXRActive;
	if ( gamepadStick ) prompt.icon = AVRControlIcon::LeftStick;
	if ( ( vrControl && s_material ) || ( gamepadStick && s_gamepadStickMaterial ) )
	{
		const TINT column = TINT( prompt.icon );
		const TINT row    = TINT( prompt.family );
		// Match capital-letter ink, not the oversized native GameCube A icon.
		const auto* reference = font->GetNativeCharData( L'B' );
		glyph.width = glyph.height = reference ? reference->ui8CharHeight : font->GetFontDef()->uiLineHeight;
		glyph.yOffset              = reference ? reference->iYOffset : 0.0f;
		if ( gamepadStick )
		{
			glyph.material = s_gamepadStickMaterial;
			glyph.uv1 = { 0.5f / CELL_SIZE, 0.5f / CELL_SIZE };
			glyph.uv2 = { ( CELL_SIZE - 0.5f ) / CELL_SIZE, ( CELL_SIZE - 0.5f ) / CELL_SIZE };
			return TTRUE;
		}
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
	if ( vrControl || gamepadStick ) name = ControlLabel( prompt.icon );
	glyph.text = name ? name : L"?";
	// Rekord18 is used by interaction/instruction prompts; the same Xbox
	// icons render correctly in the menu's Rekord26 page. Borrow that native
	// glyph (not its text font) and scale to the destination line height.
	// Leave menu rendering, keyboard text, other pads and OpenXR untouched.
	if ( !vrControl && AInputHandler::GetSingleton()->IsControllerUsed() &&
	     AInputHandler::GetSingleton()->IsControllerXInput() && font == s_smallPromptFont &&
	     s_menuGlyphFont && name && name[ 0 ] >= 0x0F00 && name[ 0 ] <= 0x0FFF && !name[ 1 ] &&
	     s_menuGlyphFont->GetNativeCharData( name[ 0 ] ) && s_menuGlyphFont->GetFontDef()->uiLineHeight )
	{
		glyph.textFont  = s_menuGlyphFont;
		glyph.textScale = TFLOAT( font->GetFontDef()->uiLineHeight ) / s_menuGlyphFont->GetFontDef()->uiLineHeight;
		glyph.width     = std::clamp( std::ceil( s_menuGlyphFont->GetTextWidth( name ) * glyph.textScale - font->GetFontDef()->iLetterSpacing ), 0.0f, 255.0f );
		return TTRUE;
	}
	glyph.width = std::clamp( std::ceil( const_cast<T2GUIFont*>( font )->GetTextWidth( glyph.text ) - font->GetFontDef()->iLetterSpacing ), 0.0f, 255.0f );
	return TTRUE;
}
} // namespace

TBOOL AInputPrompts::SetOpenXRRectangle( T2GUIRectangle& rectangle, const TPString8& mapName, AInputCommand command )
{
	if ( !s_openXRActive || !s_material || !AInputHandler::GetSingleton()->IsControllerUsed() ) return TFALSE;
	auto* map = AInputMapManager::GetSingleton()->FindMap( mapName );
	if ( !map ) return TFALSE;
	const auto doodad = map->GetCommandDoodad( command, TFALSE );
	if ( doodad < Pad::BUTTON_DPAD_UP || doodad > Pad::VIRTSTK_DPAD ) return TFALSE;
	const auto prompt = AVRManager::GetSingleton().GetControllerPrompt( doodad );
	const TFLOAT width = TFLOAT( CELL_SIZE * TINT( AVRControlIcon::Count ) );
	const TFLOAT height = TFLOAT( CELL_SIZE * FAMILY_COUNT );
	const TFLOAT x = TFLOAT( TINT( prompt.icon ) * CELL_SIZE );
	const TFLOAT y = TFLOAT( TINT( prompt.family ) * CELL_SIZE );
	const TFLOAT u1 = ( x + 0.5f ) / width, u2 = ( x + CELL_SIZE - 0.5f ) / width;
	const TFLOAT v1 = ( y + 0.5f ) / height, v2 = ( y + CELL_SIZE - 0.5f ) / height;
	rectangle.SetMaterial( s_material );
	rectangle.SetUVs( { u1, v1 }, { u2, v1 }, { u2, v2 }, { u1, v2 } );
	return TTRUE;
}

void AInputPrompts::Initialise()
{
	if ( s_initialised ) return;
	s_initialised     = TTRUE;
	s_smallPromptFont = T2GUIFontManager::FindFont( "Rekord18" );
	s_menuGlyphFont   = T2GUIFontManager::FindFont( "Rekord26" );
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
	// Xelu's CC0 Xbox Series left-stick glyph, used only for the dedicated
	// honey-collection prompt; all existing native gamepad glyphs stay intact.
	constexpr const char* stickPath = "Data/VR/GamepadLeftStick.dds";
	if ( auto* file = TFile::Create( stickPath, TFILEMODE_READ ) )
	{
		delete file;
		auto* factory = TRenderInterface::GetSingleton()->GetSystemResource<TTextureFactory>( SYSRESOURCE_TEXTUREFACTORY );
		s_gamepadStickTexture = factory->CreateTextureFromFile( stickPath, 0 );
		if ( s_gamepadStickTexture && s_gamepadStickTexture->GetWidth() == CELL_SIZE && s_gamepadStickTexture->GetHeight() == CELL_SIZE )
			s_gamepadStickMaterial = T2GUI::GetRenderer()->CreateMaterial( s_gamepadStickTexture );
	}
	if ( !s_material ) TWARN( "OpenXR prompt atlas unavailable; using neutral VR labels.\n" );
#endif
	T2GUIFont::ms_fnInlineGlyph = Resolve;
}

void AInputPrompts::Shutdown()
{
	T2GUIFont::ms_fnInlineGlyph = TNULL;
	if ( s_smallPromptFont ) T2GUIFontManager::ReleaseFont( s_smallPromptFont );
	if ( s_menuGlyphFont ) T2GUIFontManager::ReleaseFont( s_menuGlyphFont );
	s_smallPromptFont = s_menuGlyphFont = TNULL;
	if ( s_gamepadStickMaterial ) T2GUI::GetRenderer()->DestroyMaterial( s_gamepadStickMaterial );
	if ( s_gamepadStickTexture ) s_gamepadStickTexture->DestroyResource();
	s_gamepadStickMaterial = TNULL;
	s_gamepadStickTexture = TNULL;
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

TWCHAR AInputPrompts::GetLeftStickToken()
{
	if ( !s_initialised ) return 0;
	for ( TINT i = 0; i < TINT( s_tokens.size() ); ++i )
		if ( s_tokens[ i ].stickGlyph ) return TWCHAR( FIRST_TOKEN + i );
	if ( s_tokens.size() >= MAX_TOKENS ) return 0;
	s_tokens.push_back( { TPString8(), AInputCommand_Unknown, Pad::STICK_LEFT, TTRUE } );
	return TWCHAR( FIRST_TOKEN + s_tokens.size() - 1 );
}

AInputCommand AInputPrompts::GetTokenCommand( TWCHAR token )
{
	const TINT index = TINT( token ) - FIRST_TOKEN;
	return index >= 0 && index < TINT( s_tokens.size() ) && s_tokens[ index ].doodad == -1
	    ? s_tokens[ index ].command : AInputCommand_Unknown;
}

void AInputPrompts::SetOpenXRActive( TBOOL active ) { s_openXRActive = active; }

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
	if ( !s_openXRActive ) return facePrompt;
	const auto available = []( TINT doodad ) {
		return AVRManager::GetSingleton().GetControllerPrompt( doodad ).icon != AVRControlIcon::Unavailable;
	};
	const TBOOL hasA = available( Pad::BUTTON_CONTEXT_3 );
	const TBOOL hasB = available( Pad::BUTTON_CONTEXT_2 );
	if ( hasA && hasB ) return facePrompt;
	// A runtime may leave just one face action bound. Prefer that working
	// button rather than advertising a potentially unbound Start action.
	if ( hasA || hasB )
	{
		startPrompt[ 6 ] = hasA ? a : b;
		return startPrompt;
	}
	return available( Pad::BUTTON_START ) ? startPrompt : L"Press a button";
}
