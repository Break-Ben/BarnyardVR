#include "pch.h"
#include "AGUI2TextBox.h"
#include "Input/AInputPrompts.h"
#include <unordered_map>

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

namespace
{
// External bookkeeping preserves the inherited binary class layout used by mods.
// Entries are removed on destruction/recreation, never weak pointers to output text.
struct PromptLayout
{
	TBOOL   fit      = TFALSE;
	TUINT32 revision = 0;
};
auto& PromptLayouts()
{
	// Some text boxes are globals destroyed after GUI shutdown. Keep the empty
	// registry object alive until process exit; each box still erases its entry.
	static auto* layouts = new std::unordered_map<const AGUI2TextBox*, PromptLayout>;
	return *layouts;
}
} // namespace

// $Barnyard: FUNCTION 006c3620
AGUI2TextBox::AGUI2TextBox()
{
	m_bRenderShadow  = TFALSE;
	m_wszText        = TNULL;
	m_pFont          = TNULL;
	m_bFixedHeight   = TFALSE;
	m_fShadowOffsetX = 5.0f;
	m_fShadowOffsetY = 5.0f;
	m_uiShadowColour = 0x80000000;
	m_fScale         = 1.0f;
	m_eTextAlign     = T2GUIFont::TextAlign_Center;
}

// $Barnyard: FUNCTION 006c3660
// $Barnyard: FUNCTION 00666f60
AGUI2TextBox::~AGUI2TextBox()
{
	PromptLayouts().erase( this );
}

// $Barnyard: FUNCTION 006c3670
// $BarnyardWiiJP: FUNCTION 804a24ac
void AGUI2TextBox::Create( T2GUIFont* a_pFont, TFLOAT a_fWidth )
{
	PromptLayouts().erase( this );
	m_pFont                = a_pFont;
	T2GUIElement::m_fWidth = a_fWidth;
	m_bDimensionsDirty     = TTRUE;
	m_bFixedHeight         = TFALSE;
}

void AGUI2TextBox::SetText( const TWCHAR* a_wszText )
{
	PromptLayouts().erase( this );
	m_wszText          = a_wszText;
	m_bDimensionsDirty = TTRUE;
}

void AGUI2TextBox::SetTextAndFitWidth( const TWCHAR* text )
{
	SetText( text );
	m_bFixedHeight = TFALSE;
	m_fWidth       = m_pFont->GetTextWidth( text ) * m_fScale;
	if ( AInputPrompts::ContainsToken( text ) ) PromptLayouts()[ this ] = { TTRUE, 0 };
}

// $Barnyard: FUNCTION 006c3780
void AGUI2TextBox::RenderText( void* a_fnCallback )
{
	if ( m_pFont && m_wszText )
	{
		TFLOAT fWidth;
		TFLOAT fHeight;
		GetDimensions( fWidth, fHeight );

		TFLOAT fPosX = -fWidth * 0.5f;
		TFLOAT fPosY = -fHeight * 0.5f;

		if ( m_bRenderShadow )
		{
			m_pFont->DrawTextWrapped(
			    m_wszText,
			    fPosX + m_fShadowOffsetX,
			    fPosY + m_fShadowOffsetY,
			    fWidth,
			    fHeight,
			    m_uiShadowColour,
			    m_fScale,
			    m_eTextAlign,
			    a_fnCallback
			);
		}

		m_pFont->DrawTextWrapped(
		    m_wszText,
		    fPosX,
		    fPosY,
		    fWidth,
		    fHeight,
		    T2GUIElement::m_uiColour,
		    m_fScale,
		    m_eTextAlign,
		    a_fnCallback
		);
	}
}

// $Barnyard: FUNCTION 006c3760
// $BarnyardWiiJP: FUNCTION 804a2620
TFLOAT AGUI2TextBox::GetLineHeight()
{
	if ( m_pFont )
		return m_pFont->GetLineHeight();

	return 0.0f;
}

void AGUI2TextBox::SetScale( TFLOAT a_fScale )
{
	m_fScale           = a_fScale;
	m_bDimensionsDirty = TTRUE;
}

void AGUI2TextBox::SetShadow( TBOOL a_bEnabled, TUINT32 a_uiColour )
{
	m_bRenderShadow  = a_bEnabled;
	m_uiShadowColour = a_uiColour;
}

void AGUI2TextBox::SetShadowEnabled( TBOOL a_bEnabled )
{
	m_bRenderShadow = a_bEnabled;
}

void AGUI2TextBox::SetShadowColour( TUINT32 a_uiColour )
{
	m_uiShadowColour = a_uiColour;
}

void AGUI2TextBox::SetShadowColour( const Toshi::TColor& a_rColour )
{
	m_uiShadowColour = a_rColour.Value32;
}

void AGUI2TextBox::SetShadowOffset( TFLOAT a_fOffsetX, TFLOAT a_fOffsetY )
{
	m_fShadowOffsetX = m_fScale * a_fOffsetX;
	m_fShadowOffsetY = m_fScale * a_fOffsetY;
}

void AGUI2TextBox::SetTextAlign( T2GUIFont::TextAlign a_eTextAlign )
{
	m_eTextAlign = a_eTextAlign;
}

// $Barnyard: FUNCTION 006c3850
void AGUI2TextBox::Render()
{
	// NOTE: there's no font until a text is set (e.g. an AGUI2FadingText before its first
	// SetText), RenderText skips the text then

	if ( T2GUIElement::HasFlags( T2GUIElement::FLAGS_RENDER_BEFORE_CHILDREN ) )
	{
		// Render children before text
		T2GUIElement::Render();
	}

	RenderText( TNULL );

	if ( !T2GUIElement::HasFlags( T2GUIElement::FLAGS_RENDER_BEFORE_CHILDREN ) )
	{
		// Render children after text
		T2GUIElement::Render();
	}
}

// $Barnyard: FUNCTION 006c36c0
void AGUI2TextBox::GetDimensions( TFLOAT& a_rWidth, TFLOAT& a_rHeight )
{
	if ( m_pFont && AInputPrompts::ContainsToken( m_wszText ) )
	{
		auto&      layout   = PromptLayouts()[ this ];
		const auto revision = AInputPrompts::GetRevision();
		if ( layout.revision != revision )
		{
			if ( layout.fit ) m_fWidth = m_pFont->GetTextWidth( m_wszText ) * m_fScale;
			m_bDimensionsDirty = TTRUE;
			layout.revision    = revision;
		}
	}

	if ( !m_bFixedHeight && m_bDimensionsDirty )
	{
		if ( !m_wszText || !m_pFont )
		{
			T2GUIElement::m_fHeight = 0.0f;
		}
		else
		{
			T2GUIElement::m_fHeight = m_pFont->GetTextHeightWrapped( m_wszText, T2GUIElement::m_fWidth, m_fScale );
		}

		m_bDimensionsDirty = TFALSE;
	}

	a_rWidth  = T2GUIElement::m_fWidth;
	a_rHeight = T2GUIElement::m_fHeight;
}

// $Barnyard: FUNCTION 006c3720
TFLOAT AGUI2TextBox::GetWidth()
{
	TFLOAT fWidth;
	TFLOAT fHeight;
	GetDimensions( fWidth, fHeight );

	return fWidth;
}

// $Barnyard: FUNCTION 006c3740
TFLOAT AGUI2TextBox::GetHeight()
{
	TFLOAT fWidth;
	TFLOAT fHeight;
	GetDimensions( fWidth, fHeight );

	return fHeight;
}

// $Barnyard: FUNCTION 0063ad20
void AGUI2TextBox::SetWidth( TFLOAT a_fWidth )
{
	PromptLayouts().erase( this );
	T2GUIElement::m_fWidth = a_fWidth;
	m_bDimensionsDirty     = TTRUE;
}

// $Barnyard: FUNCTION 006c3690
// $BarnyardWiiJP: FUNCTION 804a24c8
void AGUI2TextBox::SetSize( TFLOAT a_fWidth, TFLOAT a_fHeight, TBOOL a_bFixedHeight )
{
	PromptLayouts().erase( this );
	m_bFixedHeight         = a_bFixedHeight;
	T2GUIElement::m_fWidth = a_fWidth;

	if ( a_bFixedHeight )
	{
		T2GUIElement::m_fHeight = a_fHeight;
		m_bDimensionsDirty      = TFALSE;
	}
	else
	{
		m_bDimensionsDirty = TTRUE;
	}
}

// $Barnyard: FUNCTION 006c3890
// $BarnyardWiiJP: FUNCTION 804a27b8
void AGUI2TextBox::SetShadowAlpha( TFLOAT a_fAlpha )
{
	TUINT8 ui8Alpha  = TUINT8( a_fAlpha * 255.0f );
	m_uiShadowColour = ( m_uiShadowColour & 0x00FFFFFF ) | ui8Alpha << 24;
}
