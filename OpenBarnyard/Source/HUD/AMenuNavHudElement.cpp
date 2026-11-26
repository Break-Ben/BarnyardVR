#include "pch.h"
#include "AMenuNavHudElement.h"
#include "GUI/AGUI2.h"
#include "Locale/ALocale.h"
#include "Locale/ALocaleManager.h"
#include "AHudManager.h"

#include <GUI/T2GUIFontManager.h>
#include <GUI/T2GUITextureSectionManager.h>

#ifdef BARNYARD_COMMUNITY_PATCH
#  include <array>
#  include <string>
#  include <unordered_map>
#endif

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

// NOTE: GetClass 00455a40, CreateObject 00455ec0, CreateObjectInPlace 00455f20
TDEFINE_CLASS( AMenuNavHudElement );

// NOTE: inlined in retail, the text box fits the text unless its width is fixed
static void SetNavText( AGUI2TextBox& a_rText, const TWCHAR* a_wszText, TBOOL a_bFixedWidth )
{
	if ( a_bFixedWidth )
		a_rText.SetText( a_wszText );
	else
		a_rText.SetTextAndFitWidth( a_wszText );
}

#ifdef BARNYARD_COMMUNITY_PATCH
namespace
{
// Keep unformatted source strings so device changes can also refresh tags that
// do not have live prompt tokens, without changing the original HUD layout.
auto& NavTextSources()
{
	static auto* sources = new std::unordered_map<const AMenuNavHudElement*, std::array<std::wstring, AMenuNavHudElement::NUM_TEXTS>>;
	return *sources;
}
}
#endif

static void FormatNavText( AMenuNavHudElement* a_pNav, TINT a_iIndex, const TWCHAR* a_wszSource )
{
#ifdef BARNYARD_COMMUNITY_PATCH
	auto& source = NavTextSources()[ a_pNav ][ a_iIndex ];
	source = a_wszSource;
	a_wszSource = source.c_str();
#endif
	ALocale::FormatInputTags( a_pNav->m_apwszTexts[ a_iIndex ], AMenuNavHudElement::MAX_LENGTH, a_wszSource );
}

// $Barnyard: FUNCTION 00455a50
AMenuNavHudElement::AMenuNavHudElement()
    : AHudElement( MASK_MENU_NAV )
{
}

// $Barnyard: FUNCTION 00455ac0
// $Barnyard: FUNCTION 00455f60
AMenuNavHudElement::~AMenuNavHudElement()
{
}

// $Barnyard: FUNCTION 00455f80
TBOOL AMenuNavHudElement::OnAttach()
{
	m_abFixedWidth[ 0 ] = TFALSE;
	m_abFixedWidth[ 1 ] = TFALSE;
	m_abFixedWidth[ 2 ] = TFALSE;

	m_apwszTexts[ 0 ] = TSTATICCAST( TWCHAR, TMalloc( MAX_LENGTH * sizeof( TWCHAR ) ) );
	m_apwszTexts[ 1 ] = TSTATICCAST( TWCHAR, TMalloc( MAX_LENGTH * sizeof( TWCHAR ) ) );
	m_apwszTexts[ 2 ] = TSTATICCAST( TWCHAR, TMalloc( MAX_LENGTH * sizeof( TWCHAR ) ) );

	FormatNavText( this, 0, ALocaleManager::Interface()->GetString( 1047 ) );
	FormatNavText( this, 1, ALocaleManager::Interface()->GetString( 1048 ) );

	m_apwszTexts[ 2 ][ 0 ] = 0;

	T2GUIFont* pFont = T2GUIFontManager::FindFont( "Rekord26" );

	// The bar at the bottom of the screen
	T2GUITextureSection* pBacking = T2GUITextureSectionManager::GetTextureSection( "Wood_Thin" );
	m_oBacking.SetDimensions( AGUI2::GetRootElement()->GetWidth(), pBacking->GetHeight() );
	m_oBacking.SetTextureSection( pBacking );
	m_oBacking.SetAttachment( T2GUIATTACHMENT_BOTTOMCENTER, T2GUIATTACHMENT_BOTTOMCENTER );
	AGUI2::GetRootElement()->AddChildTail( m_oBacking );

	m_fSpacing = m_oBacking.GetWidth() * 0.25f;

	// The text on the left...
	m_aTexts[ 0 ].Create( pFont, 0.0f );
	SetNavText( m_aTexts[ 0 ], m_apwszTexts[ 0 ], m_abFixedWidth[ 0 ] );
	m_aTexts[ 0 ].SetTransform( -m_fSpacing, 0.0f );
	m_aTexts[ 0 ].SetScale( 0.75f );

	// ...on the right...
	m_aTexts[ 1 ].Create( pFont, 0.0f );
	SetNavText( m_aTexts[ 1 ], m_apwszTexts[ 1 ], m_abFixedWidth[ 1 ] );
	m_aTexts[ 1 ].SetTransform( m_fSpacing, 0.0f );
	m_aTexts[ 1 ].SetScale( 0.75f );

	// ...and in the middle
	m_aTexts[ 2 ].Create( pFont, 0.0f );
	SetNavText( m_aTexts[ 2 ], m_apwszTexts[ 2 ], m_abFixedWidth[ 2 ] );
	m_aTexts[ 2 ].SetScale( 0.75f );
	m_aTexts[ 2 ].T2GUIElement::Hide();

	m_oBacking.AddChildTail( m_aTexts[ 0 ] );
	m_oBacking.AddChildTail( m_aTexts[ 1 ] );
	m_oBacking.AddChildTail( m_aTexts[ 2 ] );
	m_oBacking.T2GUIElement::Hide();

	return TTRUE;
}

// $Barnyard: FUNCTION 00455b30
void AMenuNavHudElement::OnDetach()
{
#ifdef BARNYARD_COMMUNITY_PATCH
	NavTextSources().erase( this );
#endif
	TFree( m_apwszTexts[ 0 ] );
	TFree( m_apwszTexts[ 1 ] );
	TFree( m_apwszTexts[ 2 ] );
}

// $Barnyard: FUNCTION 004561e0
void AMenuNavHudElement::Show()
{
	AGUI2::GetRootElement()->AddChildTail( m_oBacking );
	m_oBacking.T2GUIElement::Show();
	m_bCurrentlyVisible = TTRUE;
}

// $Barnyard: FUNCTION 00455b60
void AMenuNavHudElement::Hide()
{
	m_oBacking.T2GUIElement::Hide();
	m_bCurrentlyVisible = TFALSE;
}

// $Barnyard: FUNCTION 00455b70
void AMenuNavHudElement::SetTexts( TINT a_iLeft, TINT a_iRight, TINT a_iCentre )
{
	if ( a_iLeft != -1 )
	{
		FormatNavText( this, 0, ALocaleManager::Interface()->GetString( a_iLeft ) );
		SetNavText( m_aTexts[ 0 ], m_apwszTexts[ 0 ], m_abFixedWidth[ 0 ] );
	}

	if ( a_iRight != -1 )
	{
		FormatNavText( this, 1, ALocaleManager::Interface()->GetString( a_iRight ) );
		SetNavText( m_aTexts[ 1 ], m_apwszTexts[ 1 ], m_abFixedWidth[ 1 ] );
	}

	if ( a_iCentre != -1 )
	{
		FormatNavText( this, 2, ALocaleManager::Interface()->GetString( a_iCentre ) );
		SetNavText( m_aTexts[ 2 ], m_apwszTexts[ 2 ], m_abFixedWidth[ 2 ] );
	}

	m_aTexts[ 0 ].SetVisible( a_iLeft != -1 );
	m_aTexts[ 1 ].SetVisible( a_iRight != -1 );
	m_aTexts[ 2 ].SetVisible( a_iCentre != -1 );
}

#ifdef BARNYARD_COMMUNITY_PATCH

// $BarnyardPS2EU: FUNCTION 001766b0
void AMenuNavHudElement::SetTextStrings( const TWCHAR* a_wszLeft, const TWCHAR* a_wszRight, const TWCHAR* a_wszCentre )
{
	if ( a_wszLeft )
	{
		FormatNavText( this, 0, a_wszLeft );
		SetNavText( m_aTexts[ 0 ], m_apwszTexts[ 0 ], m_abFixedWidth[ 0 ] );
	}

	if ( a_wszRight )
	{
		FormatNavText( this, 1, a_wszRight );
		SetNavText( m_aTexts[ 1 ], m_apwszTexts[ 1 ], m_abFixedWidth[ 1 ] );
	}

	if ( a_wszCentre )
	{
		FormatNavText( this, 2, a_wszCentre );
		SetNavText( m_aTexts[ 2 ], m_apwszTexts[ 2 ], m_abFixedWidth[ 2 ] );
	}

	m_aTexts[ 0 ].SetVisible( a_wszLeft != TNULL );
	m_aTexts[ 1 ].SetVisible( a_wszRight != TNULL );
	m_aTexts[ 2 ].SetVisible( a_wszCentre != TNULL );
}

#endif // BARNYARD_COMMUNITY_PATCH

// $Barnyard: FUNCTION 00455d90
void AMenuNavHudElement::SetDefaultTexts()
{
	FormatNavText( this, 0, ALocaleManager::Interface()->GetString( 1047 ) );
	SetNavText( m_aTexts[ 0 ], m_apwszTexts[ 0 ], m_abFixedWidth[ 0 ] );

	FormatNavText( this, 1, ALocaleManager::Interface()->GetString( 1048 ) );
	SetNavText( m_aTexts[ 1 ], m_apwszTexts[ 1 ], m_abFixedWidth[ 1 ] );

	m_aTexts[ 0 ].T2GUIElement::Show();
	m_aTexts[ 1 ].T2GUIElement::Show();
	m_aTexts[ 2 ].T2GUIElement::Hide();
}

// $Barnyard: FUNCTION 00455d20
void AMenuNavHudElement::SetSpacing( TFLOAT a_fSpacing )
{
	m_aTexts[ 0 ].SetTransform( -( a_fSpacing * m_fSpacing ), 0.0f );
	m_aTexts[ 1 ].SetTransform( a_fSpacing * m_fSpacing, 0.0f );
}

// $Barnyard: FUNCTION 00455d70
void AMenuNavHudElement::SetY( TFLOAT a_fY )
{
	m_oBacking.SetTransform( m_oBacking.GetTransform().GetTranslation().x, a_fY );
}

// $Barnyard: FUNCTION 00455e70
void AMenuNavHudElement::SetTextScale( TFLOAT a_fScale )
{
	m_aTexts[ 0 ].SetScale( a_fScale );
	m_aTexts[ 1 ].SetScale( a_fScale );
	m_aTexts[ 2 ].SetScale( a_fScale );
}

// $Barnyard: FUNCTION 00455eb0
TFLOAT AMenuNavHudElement::GetTextScale()
{
	return m_aTexts[ 0 ].GetScale();
}

void AMenuNavHudElement::UpdateText()
{
	if ( !AHudManager::GetSingleton() ) return;

	if ( AMenuNavHudElement* pMenuNav = AHudManager::GetSingleton()->FindElement<AMenuNavHudElement>() )
		pMenuNav->SetTexts( 1047, 1048, -1 );
}

#ifdef BARNYARD_COMMUNITY_PATCH
void AMenuNavHudElement::RefreshDeviceText()
{
	if ( !AHudManager::GetSingleton() ) return;

	auto* pNav = AHudManager::GetSingleton()->FindElement<AMenuNavHudElement>();
	if ( !pNav ) return;

	auto it = NavTextSources().find( pNav );
	if ( it == NavTextSources().end() ) return;

	for ( TINT i = 0; i < NUM_TEXTS; i++ )
	{
		ALocale::FormatInputTags( pNav->m_apwszTexts[ i ], MAX_LENGTH, it->second[ i ].c_str() );
		SetNavText( pNav->m_aTexts[ i ], pNav->m_apwszTexts[ i ], pNav->m_abFixedWidth[ i ] );
	}
}
#endif
