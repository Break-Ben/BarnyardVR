#include "ToshiPCH.h"
#include "TCameraObject.h"

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include "Core/TMemoryDebugOn.h"

TOSHI_NAMESPACE_START

// $Barnyard: FUNCTION 006cd030
TCameraObject::TCameraObject()
{
	m_eScaling = FOV_SCALING_TYPE_HORIZONTAL;
	m_eMode    = TRenderContext::CameraMode_Perspective;
	m_fNear    = 1.0f;
	m_fFar     = 100.0f;
	m_fFOV     = TMath::DegToRad( 90.0f );
	m_fCentreX = 0.5f;
	m_fCentreY = 0.5f;
	m_bEnabled = TTRUE;
}

TCameraObject::~TCameraObject()
{
}

// Use the same camera projection for off-screen/reference sizing without
// changing the active render context (which may be rendering an OpenXR eye).
TRenderContext::PROJECTIONPARAMS TCameraObject::GetProjectionParams( TFLOAT a_fWidth, TFLOAT a_fHeight ) const
{
	TRenderContext::PROJECTIONPARAMS projParams = {};
	projParams.m_Centre                         = { m_fCentreX * a_fWidth, m_fCentreY * a_fHeight };

	auto render = TRenderInterface::GetSingleton();

	if ( m_eScaling == FOV_SCALING_TYPE_HORIZONTAL )
	{
#ifdef BARNYARD_COMMUNITY_PATCH
		// Hor+
		TFLOAT fWidth = a_fWidth;

		if ( m_eMode == TRenderContext::CameraMode_Perspective )
			fWidth = TMath::Min( fWidth, a_fHeight * ( 4.0f / 3.0f ) );

		projParams.m_Proj.x = ( fWidth * 0.5f ) / tan( m_fFOV * 0.5f );
#else
		projParams.m_Proj.x = ( a_fWidth * 0.5f ) / tan( m_fFOV * 0.5f );
#endif // BARNYARD_COMMUNITY_PATCH
		projParams.m_Proj.y = projParams.m_Proj.x;

		if ( m_eMode == TRenderContext::CameraMode_Perspective )
		{
			projParams.m_Proj.y *= render->GetPixelAspectRatio();
		}
	}
	else
	{
		projParams.m_Proj.y = ( a_fHeight * 0.5f ) / tan( m_fFOV * 0.5f );
		projParams.m_Proj.x = projParams.m_Proj.y;

		if ( m_eMode == TRenderContext::CameraMode_Perspective )
		{
			projParams.m_Proj.x /= render->GetPixelAspectRatio();
		}
	}

	projParams.m_fNearClip = m_fNear;
	projParams.m_fFarClip  = m_fFar;

	return projParams;
}

// $Barnyard: FUNCTION 006cd0d0
void TCameraObject::Render()
{
	if ( m_bEnabled )
	{
		auto pRender        = TRenderInterface::GetSingleton();
		auto pRenderContext = pRender->GetCurrentContext();

		const TRenderContext::PROJECTIONPARAMS projParams = GetProjectionParams( pRenderContext->GetWidth(), pRenderContext->GetHeight() );

		pRenderContext->SetProjectionParams( projParams );

		if ( m_eMode == TRenderContext::CameraMode_Perspective || m_eMode == TRenderContext::CameraMode_Orthographic )
			pRenderContext->SetCameraMode( m_eMode );

		TMatrix44 matrix;
		m_TransformObject.GetLocalMatrixImp( matrix );
		matrix.InvertOrthogonal();

		pRenderContext->SetWorldViewMatrix( matrix );
		pRenderContext->SetModelViewMatrix( matrix );
		pRenderContext->Update();
	}
}

TOSHI_NAMESPACE_END
