#include "pch.h"
#include "AWorldOverlayRenderScope.h"
#include "Render/ARenderer.h"
#include "VR/AVRManager.h"

#include <Render/TBackend.h>
#include <Render/TCameraObject.h>

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

AWorldOverlayRenderScope::AWorldOverlayRenderScope( TViewport& a_rViewport )
    : m_rViewport( a_rViewport )
{
	auto pRender = TRenderInterfaceHAL::Interface();
	m_pOldContext = pRender->GetCurrentContext();
	auto pContext = m_rViewport.GetRenderContext();
	m_pOldCamera = pContext->GetCameraObject();
	m_oOldViewport = pContext->GetViewportParameters();
	m_oOldProjection = pContext->GetProjectionParams();

	// Order tables use the current context's projection when they are flushed.
	pRender->FlushShaders();
	AVRManager& rVR = AVRManager::GetSingleton();
	const TBOOL bEyeActive = rVR.GetCurrentEye() < rVR.GetViewCount();
	if ( bEyeActive )
	{
		// These viewports are otherwise sized to the desktop at construction.
		m_rViewport.SetX( m_pOldContext->GetX() );
		m_rViewport.SetY( m_pOldContext->GetY() );
		m_rViewport.SetWidth( m_pOldContext->GetWidth() );
		m_rViewport.SetHeight( m_pOldContext->GetHeight() );
	}

	pRender->SetCurrentRenderContext( pContext );
	m_rViewport.Begin();
	TCameraObject* pCamera = ARenderer::GetSingleton()->GetCameraObject();
	pContext->SetCameraObject( pCamera );
	pCamera->Render();
	// Render() installs the legacy symmetric projection. Replace it AFTER rendering
	// the eye camera, including the cached projection consumed by DX11 shaders.
	rVR.ApplyEyeProjection( pContext );
	pRender->GetTransforms().Push( pContext->GetWorldViewMatrix() );
}

AWorldOverlayRenderScope::~AWorldOverlayRenderScope()
{
	auto pRender = TRenderInterfaceHAL::Interface();
	// End flushes the order tables while the overlay's context is still current.
	m_rViewport.End();
	pRender->FlushShaders();
	pRender->GetTransforms().Pop();
	auto pContext = m_rViewport.GetRenderContext();
	pContext->SetCameraObject( m_pOldCamera );
	pContext->SetViewportParameters( m_oOldViewport );
	pContext->SetProjectionParams( m_oOldProjection );
	pContext->Update();
	pRender->SetCurrentRenderContext( m_pOldContext );

	// Rebind the parent's GPU viewport without beginning a second logical pass or
	// clearing the depth/colour buffers. Later markers must use the eye viewport too.
	TViewport* pParent = ARenderer::GetSingleton()->GetMainViewport();
	TBOOL bBackgroundClear = pParent->AllowBackgroundClear( TFALSE );
	TBOOL bDepthClear = pParent->AllowDepthClear( TFALSE );
	pParent->BeginSKU();
	pParent->AllowBackgroundClear( bBackgroundClear );
	pParent->AllowDepthClear( bDepthClear );
}
