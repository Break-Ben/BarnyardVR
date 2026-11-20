#include "pch.h"
#include "AWorldSpaceUI.h"
#include "Render/ASysShader/ASysShader.h"
#include "Render/ASysShader/ASysMesh.h"
#include "Render/ASysShader/ASysMaterial.h"
#include "VR/AVRManager.h"

#include <Render/TBackend.h>

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

AWorldSpaceUI::~AWorldSpaceUI()
{
	if ( m_pMesh ) m_pMesh->DestroyResource();
	if ( m_pMaterial ) m_pMaterial->Destroy();
}

void AWorldSpaceUI::Render( const TVector4& a_rcAnchor, const Sprite* a_pSprites, TINT a_iCount, T2GUITextureSection* a_pTexture, TFLOAT a_fWorldScale )
{
#if defined( TRENDERINTERFACE_DX11 )
	AVRManager& rVR = AVRManager::GetSingleton();
	if ( rVR.GetCurrentEye() >= rVR.GetViewCount() || !a_pTexture || !a_pTexture->GetMaterial() || !a_pSprites || a_iCount <= 0 || a_iCount > 6 || !( a_fWorldScale > 0.0f ) )
		return;

	auto pRender = TRenderInterfaceHAL::Interface();
	auto pContext = pRender->GetCurrentContext();
	TVector4 vAnchor;
	TMatrix44::TransformVector( vAnchor, pContext->GetWorldViewMatrix(), a_rcAnchor );
	if ( !( vAnchor.z > pContext->GetProjectionParams().m_fNearClip ) )
		return;

	// Match the sun/moon's world-up tangent frame, not the eye's XY axes.
	// Only the eye position chooses the facing direction; headset rotation must
	// not choose the ring's rotation. Positive Y is down in Barnyard.
	const TVector4& eyePosition = pContext->GetViewWorldMatrix().GetTranslation();
	TVector3 direction( a_rcAnchor.x - eyePosition.x, a_rcAnchor.y - eyePosition.y, a_rcAnchor.z - eyePosition.z );
	if ( direction.MagnitudeSq() < 1.0e-6f ) return;
	direction.Normalize();
	TVector3 up( 0.0f, -1.0f, 0.0f );
	TFLOAT dot = TVector3::DotProduct( up, direction );
	up.Set( up.x - direction.x * dot, up.y - direction.y * dot, up.z - direction.z * dot );
	if ( up.MagnitudeSq() < 1.0e-4f )
	{
		up.Set( 0.0f, 0.0f, 1.0f );
		dot = TVector3::DotProduct( up, direction );
		up.Set( up.x - direction.x * dot, up.y - direction.y * dot, up.z - direction.z * dot );
	}
	up.Normalize();
	TVector3 right;
	right.CrossProduct( direction, up );
	right.Normalize();
	up.CrossProduct( right, direction );
	up.Normalize();
	TVector4 viewRight, viewUp;
	TMatrix44::RotateVector( viewRight, pContext->GetWorldViewMatrix(), TVector4( right.x, right.y, right.z, 0.0f ) );
	TMatrix44::RotateVector( viewUp, pContext->GetWorldViewMatrix(), TVector4( up.x, up.y, up.z, 0.0f ) );

	if ( !m_pMaterial )
	{
		m_pMaterial = ASysShader::GetSingleton()->CreateMaterial( TNULL );
		if ( !m_pMaterial ) return;
		if ( !m_pMaterial->Create( ASysMaterial::BLENDMODE_WORLD_UI ) )
		{
			m_pMaterial->Destroy();
			m_pMaterial = TNULL;
			return;
		}
		m_pMaterial->SetTextureNum( 1 );
		m_pMaterial->SetFlags( TMaterial::FLAGS_NO_CULL, TTRUE );
	}
	if ( !m_pMesh )
	{
		m_pMesh = ASysShader::GetSingleton()->CreateMesh( TNULL );
		if ( !m_pMesh ) return;
		if ( !m_pMesh->CreatePools( ASysMesh::FLAGS_VERTICES_DYNAMIC_APPEND | ASysMesh::FLAGS_INDICES_DYNAMIC_APPEND, 24, 36 ) )
		{
			m_pMesh->DestroyResource();
			m_pMesh = TNULL;
			return;
		}
		m_pMesh->SetMaterial( m_pMaterial );
	}

	ASysMesh::LockBuffer lock;
	if ( !m_pMesh->Lock( lock ) ) return;
	auto pVertices = reinterpret_cast<ASysMesh::Vertex*>( lock.VertexLock.apStreams[ 0 ] );
	auto pIndices = lock.IndexLock.pBuffer;
	const TVector4& uv1 = a_pTexture->GetUVPoint1();
	const TVector4& uv2 = a_pTexture->GetUVPoint2();
	for ( TINT i = 0; i < a_iCount; ++i )
	{
		const Sprite& s = a_pSprites[ i ];
		TFLOAT sn, cs;
		TMath::SinCos( s.angle, sn, cs );
		for ( TINT k = 0; k < 4; ++k )
		{
			const TFLOAT x = ( k & 1 ? 0.5f : -0.5f ) * s.width;
			const TFLOAT y = ( k & 2 ? 0.5f : -0.5f ) * s.height;
			auto& v = pVertices[ i * 4 + k ];
			const TFLOAT dx = ( s.x + x * cs - y * sn ) * a_fWorldScale;
			const TFLOAT dy = ( s.y + x * sn + y * cs ) * a_fWorldScale;
			// Sprite design coordinates use positive Y down.
			v.vPosition.Set( vAnchor.x + viewRight.x * dx - viewUp.x * dy,
			    vAnchor.y + viewRight.y * dx - viewUp.y * dy,
			    vAnchor.z + viewRight.z * dx - viewUp.z * dy );
			v.uiColor = s.colour;
			v.vUV.Set( k & 1 ? uv2.x : uv1.x, k & 2 ? uv2.y : uv1.y );
		}
		const TINT indices[] = { 0, 1, 2, 2, 1, 3 };
		for ( TINT k = 0; k < 6; ++k ) pIndices[ i * 6 + k ] = TIndexType( i * 4 + indices[ k ] );
	}
	m_pMesh->Unlock( a_iCount * 4, a_iCount * 6 );
	m_pMaterial->SetTexture( a_pTexture->GetMaterial()->GetTextureResource() );
	const TMatrix44 oldModelView = pContext->GetModelViewMatrix();
	pRender->FlushShaders();
	pContext->SetModelViewMatrix( TMatrix44::IDENTITY );
	m_pMesh->Validate();
	m_pMesh->Render();
	// Flush before restoring the matrix or changing the atlas on the next batch.
	pRender->FlushShaders();
	pContext->SetModelViewMatrix( oldModelView );
#else
	(void)a_rcAnchor;
	(void)a_pSprites;
	(void)a_iCount;
	(void)a_pTexture;
	(void)a_fWorldScale;
#endif
}
