#pragma once

#include <Render/TViewport.h>

// A depth-separated pass for geometry that still belongs to the world (not the
// GUI). Keeps the active eye's viewport and projection in VR, and drains queued
// draws before restoring the parent context. The viewport's clear policy is kept.
class AWorldOverlayRenderScope
{
public:
	AWorldOverlayRenderScope( Toshi::TViewport& a_rViewport );
	~AWorldOverlayRenderScope();

	AWorldOverlayRenderScope( const AWorldOverlayRenderScope& ) = delete;
	AWorldOverlayRenderScope& operator=( const AWorldOverlayRenderScope& ) = delete;

private:
	Toshi::TViewport& m_rViewport;
	Toshi::TRenderContext* m_pOldContext;
	Toshi::TCameraObject* m_pOldCamera;
	Toshi::TRenderContext::VIEWPORTPARAMS m_oOldViewport;
	Toshi::TRenderContext::PROJECTIONPARAMS m_oOldProjection;
};
