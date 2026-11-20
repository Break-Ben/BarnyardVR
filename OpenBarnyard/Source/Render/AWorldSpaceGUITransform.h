#pragma once
#include <Math/TMatrix44.h>

// Preserve the real eye-space plane for X/Y/W, but use legacy GUI Z only for
// ordering in the private mask buffer. Tiny world-space offsets collapse after
// perspective projection (especially far away) and also shift mask edges.
inline Toshi::TMatrix44 BuildWorldGUIClipTransform( const Toshi::TMatrix44& a_rcFrame, const Toshi::TMatrix44& a_rcLocal, const Toshi::TMatrix44& a_rcProjection )
{
	using namespace Toshi;
	TMatrix44 plane = a_rcFrame;
	plane.SetBasisVector4( BASISVECTOR_FORWARD, TVector4( 0.0f, 0.0f, 0.0f, 0.0f ) );
	TMatrix44 view, clip;
	view.Multiply( plane, a_rcLocal );
	clip.Multiply( a_rcProjection, view );
	// clip Z = 0.5 * clip W + 0.25 * GUI Z. Equal local points have identical
	// X/Y/W in every layer, and mask Z=0.05 remains strictly before fill Z=0.1.
	clip.m_f13 = 0.5f * clip.m_f14;
	clip.m_f23 = 0.5f * clip.m_f24;
	clip.m_f33 = 0.25f;
	clip.m_f43 = 0.5f * clip.m_f44;
	return clip;
}
