#include "pch.h"
#include "AWorldSpaceGUI.h"
#include "AWorldSpaceUI.h"
#include "GUI/AGUI2.h"
#include "VR/AVRManager.h"
#include <map>
#if defined( TRENDERINTERFACE_DX11 )
#include "Platform/DX11/AGUI2Renderer_DX11.h"
#include <Render/TBackend.h>
#endif

#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

namespace
{
struct Entry
{
	TVector4 position;
	TMatrix44 frame;
	TFLOAT scale = 0.02f;
	TBOOL fixed = TFALSE;
	TBOOL masked = TFALSE;
	TBOOL usePivot = TFALSE;
	TBOOL valid = TFALSE;
	AWorldSpaceGUI::DrawCallback draw = TNULL;
};
std::map<const T2GUIElement*, Entry>& Entries()
{
	static std::map<const T2GUIElement*, Entry> entries;
	return entries;
}
const T2GUIElement* s_pDrawing = TNULL;
TBOOL Filter( const T2GUIElement* element )
{
	return !AWorldSpaceGUI::IsActive() || !Entries().contains( element ) || element == s_pDrawing;
}
void InstallHooks()
{
	T2GUIElement::s_cbRenderFilter = Filter;
	T2GUIElement::s_cbDestroyed = AWorldSpaceGUI::Remove;
}

#if defined( TRENDERINTERFACE_DX11 )
struct ElementDraw
{
	T2GUIElement* element;
	TBOOL usePivot;
};
void DrawElement( void* data )
{
	auto& draw = *static_cast<ElementDraw*>( data );
	// Ignore legacy projected translation and screen attachment. Labels whose
	// pointer is pivoted on the target retain only that local pivot offset.
	TVector2 pivot = TVector2::VEC_ZERO;
	if ( draw.usePivot )
	{
		TFLOAT width, height;
		draw.element->GetDimensions( width, height );
		draw.element->PivotPos( pivot.x, pivot.y, width, height );
	}
	AGUI2::GetRenderer()->PushTransform( T2GUITransform(), TVector2::VEC_ZERO, pivot );
	draw.element->Render();
	AGUI2::GetRenderer()->PopTransform();
}

TBOOL BillboardFrame( const TVector4& position, TFLOAT scale, TMatrix44& frame )
{
	auto context = TRenderInterfaceHAL::Interface()->GetCurrentContext();
	TVector4 anchor, right, up;
	TMatrix44::TransformVector( anchor, context->GetWorldViewMatrix(), position );
	if ( !( anchor.z > context->GetProjectionParams().m_fNearClip ) || !( scale > 0.0f ) || !AWorldSpaceUI::GetViewFrame( position, right, up ) ) return TFALSE;
	frame = TMatrix44::IDENTITY;
	frame.SetBasisVector4( BASISVECTOR_RIGHT, TVector4( right.x * scale, right.y * scale, right.z * scale, 0 ) );
	frame.SetBasisVector4( BASISVECTOR_UP, TVector4( -up.x * scale, -up.y * scale, -up.z * scale, 0 ) );
	// GUI masks use small Z offsets; retain their ordering in a private depth
	// buffer, without writing into the world's depth buffer.
	TVector3 forward;
	forward.CrossProduct( up.AsVector3(), right.AsVector3() );
	frame.SetBasisVector4( BASISVECTOR_FORWARD, TVector4( forward.x * scale * 0.01f, forward.y * scale * 0.01f, forward.z * scale * 0.01f, 0 ) );
	frame.SetTranslation( anchor );
	return TTRUE;
}
#endif
}

TBOOL AWorldSpaceGUI::IsActive()
{
#if defined( TRENDERINTERFACE_DX11 )
	return AVRManager::GetSingleton().IsActive();
#else
	return TFALSE;
#endif
}
void AWorldSpaceGUI::SetAnchor( T2GUIElement& element, const TVector4& position, TFLOAT scale, TBOOL masked, TBOOL usePivot )
{
	InstallHooks();
	auto& entry = Entries()[ &element ];
	entry.draw = TNULL;
	entry.position = position;
	entry.scale = scale;
	entry.masked = masked;
	entry.usePivot = usePivot;
	entry.fixed = TFALSE;
	entry.valid = TTRUE;
}
void AWorldSpaceGUI::SetFrame( T2GUIElement& element, const TMatrix44& frame )
{
	InstallHooks();
	auto& entry = Entries()[ &element ];
	entry.draw = TNULL;
	entry.frame = frame;
	entry.fixed = TTRUE;
	entry.masked = TTRUE;
	entry.valid = TTRUE;
}
void AWorldSpaceGUI::Register( T2GUIElement& element, DrawCallback draw )
{
	InstallHooks();
	auto& entry = Entries()[ &element ];
	entry.draw = draw;
	entry.valid = TTRUE;
}
void AWorldSpaceGUI::Remove( const T2GUIElement* element )
{
	Entries().erase( element );
	if ( Entries().empty() )
	{
		// Do not leave callbacks into a registry during static teardown.
		if ( T2GUIElement::s_cbRenderFilter == Filter ) T2GUIElement::s_cbRenderFilter = TNULL;
		if ( T2GUIElement::s_cbDestroyed == Remove ) T2GUIElement::s_cbDestroyed = TNULL;
	}
}
void AWorldSpaceGUI::Draw( const TVector4& position, TFLOAT scale, DrawCallback draw, void* data )
{
#if defined( TRENDERINTERFACE_DX11 )
	auto& vr = AVRManager::GetSingleton();
	if ( !IsActive() || vr.GetCurrentEye() >= vr.GetViewCount() || !draw ) return;
	TMatrix44 frame;
	if ( BillboardFrame( position, scale, frame ) )
		static_cast<AGUI2RendererDX8*>( AGUI2::GetRenderer() )->RenderWorldSpace( frame, draw, data );
#else
	(void)position; (void)scale; (void)draw; (void)data;
#endif
}
void AWorldSpaceGUI::RenderAll()
{
#if defined( TRENDERINTERFACE_DX11 )
	auto& vr = AVRManager::GetSingleton();
	if ( !IsActive() || vr.GetCurrentEye() >= vr.GetViewCount() ) return;
	for ( auto& [element, entry] : Entries() )
	{
		s_pDrawing = element;
		TBOOL visible = entry.valid;
		TBOOL attached = TFALSE;
		for ( auto parent = element; parent; parent = parent->GetParent() )
		{
			// An independently anchored child may have a registered parent.
			visible = visible && parent->IsVisible();
			attached = attached || parent == AGUI2::GetRootElement();
		}
		if ( visible && attached )
		{
			if ( entry.draw ) entry.draw( const_cast<T2GUIElement*>( element ) );
			else
			{
				TMatrix44 frame;
				if ( entry.fixed )
					frame.Multiply( TRenderInterfaceHAL::Interface()->GetCurrentContext()->GetWorldViewMatrix(), entry.frame );
				if ( entry.fixed || BillboardFrame( entry.position, entry.scale, frame ) )
				{
					ElementDraw draw = { const_cast<T2GUIElement*>( element ), entry.usePivot };
					static_cast<AGUI2RendererDX8*>( AGUI2::GetRenderer() )->RenderWorldSpace( frame, DrawElement, &draw, entry.masked );
				}
			}
		}
		s_pDrawing = TNULL;
	}
#endif
}
