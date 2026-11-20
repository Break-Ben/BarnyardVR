#include "pch.h"
#include "AMilkFillHudElement.h"
#include "Render/AWorldSpaceGUI.h"

#include <GUI/T2GUITextureSectionManager.h>
#include <Math/TMathInline.h>

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

// Atlas UVs normally lie on texel boundaries. With linear filtering the
// outermost samples can blend neighbouring atlas content into the glass/mask.
// Use texel centres for this world-space widget; never mutate the shared atlas
// sections/materials, and always start from canonical UVs (no repeated inset).
static void UpdateGlassTextureUVs( T2GUIRectangle& a_rRectangle, const TCHAR* a_szSection )
{
	const auto pSection = T2GUITextureSectionManager::GetTextureSection( a_szSection );
	if ( !pSection ) return;
	const auto& uv1 = pSection->GetUVPoint1();
	const auto& uv2 = pSection->GetUVPoint2();
	const TFLOAT inset = AWorldSpaceGUI::IsActive() ? 0.5f : 0.0f;
	const TFLOAT du = ( uv2.x - uv1.x ) * inset / TMath::Max( 1.0f, pSection->GetWidth() );
	const TFLOAT dv = ( uv2.y - uv1.y ) * inset / TMath::Max( 1.0f, pSection->GetHeight() );
	a_rRectangle.SetUVs( TVector2( uv1.x + du, uv1.y + dv ), TVector2( uv2.x - du, uv1.y + dv ),
	    TVector2( uv2.x - du, uv2.y - dv ), TVector2( uv1.x + du, uv2.y - dv ) );
}

// $Barnyard: FUNCTION 00456450
AMilkFillHudElement::AMilkFillHudElement()
{
}

// $Barnyard: FUNCTION 00456510
// $Barnyard: FUNCTION 00456210
AMilkFillHudElement::~AMilkFillHudElement()
{
}

// $Barnyard: FUNCTION 00456380
void AMilkFillHudElement::Tick( TFLOAT a_fDeltaTime )
{
	T2GUIElement::Tick( a_fDeltaTime );
}

// $Barnyard: FUNCTION 00456390
void AMilkFillHudElement::Render()
{
	if ( !IsVisible() )
		return;

	UpdateGlassTextureUVs( m_oMask, "Glass_Mask" );
	UpdateGlassTextureUVs( m_oMilk, "Glass_Milk" );
	UpdateGlassTextureUVs( m_oBackPlate, "Glass_Back_Plate" );
	UpdateGlassTextureUVs( m_oGlint, "Glass_Glint" );

	m_oBackground.PreRender();
	m_oBackground.Render();
	m_oBackground.PostRender();

	m_oGlass.PreRender();

	m_oBackPlate.PreRender();
	m_oBackPlate.Render();
	m_oBackPlate.PostRender();

	m_oMask.PreRender();
	m_oMask.Render();
	m_oMask.PostRender();

	m_oGlint.PreRender();
	m_oGlint.Render();
	m_oGlint.PostRender();

	m_oGlass.PostRender();

	T2GUIElement::Render();
}

// $Barnyard: FUNCTION 00456530
TBOOL AMilkFillHudElement::Create()
{
	T2GUITextureSection* pBottle = T2GUITextureSectionManager::GetTextureSection( "Milk_Bottle" );
	SetDimensions(
	    TMath::Abs( TFLOAT( pBottle->GetPoint2X() - pBottle->GetPoint1X() ) ),
	    TMath::Abs( TFLOAT( pBottle->GetPoint2Y() - pBottle->GetPoint1Y() ) )
	);

	m_oGlass.SetDimensions( GetWidth(), GetHeight() );
	m_oBackground.T2GUIElement::SetDimensions( m_oGlass.GetWidth(), m_oGlass.GetHeight() );
	m_oBackground.SetColour( 0 );

	AddChildTail( m_oGlass );
	m_oGlass.Hide();

	m_oBackPlate.SetTextureSection( *T2GUITextureSectionManager::GetTextureSection( "Glass_Back_Plate" ) );
	m_oGlass.AddChildTail( m_oBackPlate );
	m_oBackPlate.Hide();

	// NOTE: the original gets the materials from the rectangles (T2GUIRectangle::GetMaterial,
	// 006c3960), they are the materials of the texture sections set just before.
	T2GUITextureSection* pMask = T2GUITextureSectionManager::GetTextureSection( "Glass_Mask" );
	m_oMask.SetTextureSection( *pMask );
	m_oMask.Hide();
	pMask->GetMaterial()->SetBlendState( T2GUIMaterial::BLENDSTATE_MASK );
	pMask->GetMaterial()->SetName( "GlassMask" );

	T2GUITextureSection* pMilk = T2GUITextureSectionManager::GetTextureSection( "Glass_Milk" );
	m_oMilk.SetTextureSection( *pMilk );
	m_oMask.AddChildTail( m_oMilk );
	m_oMilk.SetAttachment( T2GUIATTACHMENT_BOTTOMCENTER, T2GUIATTACHMENT_BOTTOMCENTER );
	m_oMilk.SetWidth( m_oMask.GetWidth() );
	m_oMilk.SetHeight( 0.0f );
	pMilk->GetMaterial()->SetBlendState( T2GUIMaterial::BLENDSTATE_MASKED );
	pMilk->GetMaterial()->SetName( "GlassMilk" );

	m_oGlint.SetTextureSection( *T2GUITextureSectionManager::GetTextureSection( "Glass_Glint" ) );
	m_oGlass.AddChildTail( m_oGlint );
	m_oGlint.Hide();

	return TTRUE;
}

// $Barnyard: FUNCTION 004562d0
void AMilkFillHudElement::Destroy()
{
	AWorldSpaceGUI::Remove( this );
	m_oBackPlate.RemoveSelf();
	m_oMask.RemoveSelf();
	m_oMilk.RemoveSelf();
	m_oGlint.RemoveSelf();
	m_oGlass.RemoveSelf();
	RemoveSelf();
}

// $Barnyard: FUNCTION 00456340
void AMilkFillHudElement::SetFill( TFLOAT a_fValue, TFLOAT a_fMax )
{
	// Keep the original geometry; atlas-edge bleeding is handled by UVs in Render.
	m_oMilk.SetWidth( m_oMask.GetWidth() );
	m_oMilk.SetHeight( m_oBackPlate.GetHeight() * ( a_fValue / a_fMax ) * 0.9f );
}
