#include "pch.h"
#include "VR/AVRManager.h"

#if !defined( TOSHI_SKU_WASM ) && defined( TOSHI_SKU_WINDOWS ) && defined( TRENDERINTERFACE_DX11 )

#include <Render/TBackend.h>
#include <Render/TCameraObject.h>
#include <Render/TRenderContext.h>
#include <Render/TViewport.h>
#include <Toshi/TApplication.h>
#include <Toshi/TString8.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>

#  define XR_USE_GRAPHICS_API_D3D11
#  include <d3d11_1.h>
#  include <dxgi1_2.h>
#define XR_USE_PLATFORM_WIN32
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

namespace
{
constexpr TUINT32 kEyeCount = 2;
constexpr TFLOAT  kUiQuadWidthMeters = 1.95f;
constexpr TFLOAT  kUiWorldDistance = 1.75f;
constexpr TFLOAT  kUiMenuDistance = 2.1f;
constexpr TFLOAT  kGripButtonThreshold = 0.5f;
constexpr TFLOAT  kTriggerButtonThreshold = 0.12f;

static bool IsCommandLineOption( const char* a_szCommandLine, const char* a_szOption )
{
	if ( !a_szCommandLine || !a_szOption )
		return false;

	const size_t uiLength = std::strlen( a_szOption );
	for ( const char* pFound = std::strstr( a_szCommandLine, a_szOption ); pFound; pFound = std::strstr( pFound + 1, a_szOption ) )
	{
		const bool bLeftBoundary  = pFound == a_szCommandLine || pFound[ -1 ] == ' ' || pFound[ -1 ] == '\t' || pFound[ -1 ] == '\r' || pFound[ -1 ] == '\n';
		const char cRightBoundary = pFound[ uiLength ];
		const bool bRightBoundary = cRightBoundary == '\0' || cRightBoundary == ' ' || cRightBoundary == '\t' || cRightBoundary == '\r' || cRightBoundary == '\n';
		if ( bLeftBoundary && bRightBoundary )
			return true;
	}

	return false;
}

static TFLOAT ParseVRScale( const char* a_szCommandLine )
{
	if ( !a_szCommandLine )
		return 1.0f;

	const char* pOption = std::strstr( a_szCommandLine, "-vrscale" );
	if ( !pOption )
		return 1.0f;
	pOption += std::strlen( "-vrscale" );
	if ( *pOption && *pOption != ' ' && *pOption != '\t' )
		return 1.0f;

	while ( *pOption == ' ' || *pOption == '\t' )
		++pOption;

	char*  pEnd   = TNULL;
	TFLOAT fScale = std::strtof( pOption, &pEnd );
	if ( pEnd == pOption || !std::isfinite( fScale ) || fScale <= 0.0f )
		return 1.0f;

	return std::max( 0.25f, std::min( fScale, 2.0f ) );
}

static bool IsExtensionAvailable( const std::vector<XrExtensionProperties>& a_rcExtensions, const char* a_szName )
{
	for ( const XrExtensionProperties& rExtension : a_rcExtensions )
	{
		if ( std::strcmp( rExtension.extensionName, a_szName ) == 0 )
			return true;
	}
	return false;
}

template <class T>
static bool GetOpenXRProc( XrInstance a_hInstance, const char* a_szName, T& a_rProc )
{
	PFN_xrVoidFunction pFunction = TNULL;
	if ( XR_FAILED( xrGetInstanceProcAddr( a_hInstance, a_szName, &pFunction ) ) || !pFunction )
		return false;
	a_rProc = reinterpret_cast<T>( pFunction );
	return true;
}

static XrQuaternionf NormaliseQuaternion( XrQuaternionf a_Quaternion )
{
	const TFLOAT fLengthSq = a_Quaternion.x * a_Quaternion.x + a_Quaternion.y * a_Quaternion.y + a_Quaternion.z * a_Quaternion.z + a_Quaternion.w * a_Quaternion.w;
	if ( fLengthSq < 1.0e-8f || !std::isfinite( fLengthSq ) )
		return { 0.0f, 0.0f, 0.0f, 1.0f };

	const TFLOAT fInvLength = 1.0f / std::sqrt( fLengthSq );
	a_Quaternion.x *= fInvLength;
	a_Quaternion.y *= fInvLength;
	a_Quaternion.z *= fInvLength;
	a_Quaternion.w *= fInvLength;
	return a_Quaternion;
}

static XrVector3f RotateVector( const XrQuaternionf& a_rcQuaternion, const XrVector3f& a_rcVector )
{
	// v' = v + 2w(q.xyz x v) + q.xyz x (2(q.xyz x v))
	const TFLOAT fTx = 2.0f * ( a_rcQuaternion.y * a_rcVector.z - a_rcQuaternion.z * a_rcVector.y );
	const TFLOAT fTy = 2.0f * ( a_rcQuaternion.z * a_rcVector.x - a_rcQuaternion.x * a_rcVector.z );
	const TFLOAT fTz = 2.0f * ( a_rcQuaternion.x * a_rcVector.y - a_rcQuaternion.y * a_rcVector.x );
	return {
	    a_rcVector.x + a_rcQuaternion.w * fTx + ( a_rcQuaternion.y * fTz - a_rcQuaternion.z * fTy ),
	    a_rcVector.y + a_rcQuaternion.w * fTy + ( a_rcQuaternion.z * fTx - a_rcQuaternion.x * fTz ),
	    a_rcVector.z + a_rcQuaternion.w * fTz + ( a_rcQuaternion.x * fTy - a_rcQuaternion.y * fTx ),
	};
}

static XrQuaternionf MultiplyQuaternion( const XrQuaternionf& a_rcLeft, const XrQuaternionf& a_rcRight )
{
	return {
	    a_rcLeft.w * a_rcRight.x + a_rcLeft.x * a_rcRight.w + a_rcLeft.y * a_rcRight.z - a_rcLeft.z * a_rcRight.y,
	    a_rcLeft.w * a_rcRight.y - a_rcLeft.x * a_rcRight.z + a_rcLeft.y * a_rcRight.w + a_rcLeft.z * a_rcRight.x,
	    a_rcLeft.w * a_rcRight.z + a_rcLeft.x * a_rcRight.y - a_rcLeft.y * a_rcRight.x + a_rcLeft.z * a_rcRight.w,
	    a_rcLeft.w * a_rcRight.w - a_rcLeft.x * a_rcRight.x - a_rcLeft.y * a_rcRight.y - a_rcLeft.z * a_rcRight.z,
	};
}

static bool IsValidFov( const XrFovf& a_rcFov )
{
	const TFLOAT fTanLeft  = std::tan( a_rcFov.angleLeft );
	const TFLOAT fTanRight = std::tan( a_rcFov.angleRight );
	const TFLOAT fTanDown  = std::tan( a_rcFov.angleDown );
	const TFLOAT fTanUp    = std::tan( a_rcFov.angleUp );
	return std::isfinite( fTanLeft ) && std::isfinite( fTanRight ) && std::isfinite( fTanDown ) && std::isfinite( fTanUp ) &&
	    fTanRight - fTanLeft > 1.0e-4f && fTanUp - fTanDown > 1.0e-4f;
}

static Toshi::TMatrix44 MakeEyeWorldMatrix( const Toshi::TMatrix44& a_rcRigCamera, const XrPosef& a_rcEyePose, const XrVector3f& a_rcNeutralPosition, const XrQuaternionf& a_rcReferenceYaw )
{
	XrQuaternionf oYawInverse = { -a_rcReferenceYaw.x, -a_rcReferenceYaw.y, -a_rcReferenceYaw.z, a_rcReferenceYaw.w };
	XrVector3f    vRelativeXR = {
	    a_rcEyePose.position.x - a_rcNeutralPosition.x,
	    a_rcEyePose.position.y - a_rcNeutralPosition.y,
	    a_rcEyePose.position.z - a_rcNeutralPosition.z,
	};
	vRelativeXR = RotateVector( oYawInverse, vRelativeXR );

	const XrQuaternionf oEyeRotation = MultiplyQuaternion( oYawInverse, NormaliseQuaternion( a_rcEyePose.orientation ) );
	const Toshi::TQuaternion oBarnyardRotation( oEyeRotation.x, -oEyeRotation.y, -oEyeRotation.z, oEyeRotation.w );

	Toshi::TMatrix44 oEyeLocal;
	oEyeLocal.SetFromQuaternion( oBarnyardRotation );
	oEyeLocal.AsBasisVector4( Toshi::BASISVECTOR_TRANSLATION ) = Toshi::TVector4( vRelativeXR.x, -vRelativeXR.y, -vRelativeXR.z, 1.0f );

	Toshi::TVector4 vCameraPosition = a_rcRigCamera.AsBasisVector4( Toshi::BASISVECTOR_TRANSLATION );
	Toshi::TVector4 vForward        = a_rcRigCamera.AsBasisVector4( Toshi::BASISVECTOR_FORWARD );
	TFLOAT           fForwardLength = std::sqrt( vForward.x * vForward.x + vForward.z * vForward.z );

	if ( fForwardLength < 1.0e-4f )
	{
		const Toshi::TVector4& rcCameraUp = a_rcRigCamera.AsBasisVector4( Toshi::BASISVECTOR_UP );
		vForward.x = -rcCameraUp.x;
		vForward.z = -rcCameraUp.z;
		fForwardLength = std::sqrt( vForward.x * vForward.x + vForward.z * vForward.z );
	}

	if ( fForwardLength < 1.0e-4f )
	{
		vForward.x = 0.0f;
		vForward.z = 1.0f;
		fForwardLength = 1.0f;
	}

	vForward.x /= fForwardLength;
	vForward.z /= fForwardLength;

	Toshi::TMatrix44 oRig;
	oRig.Identity();
	oRig.AsBasisVector4( Toshi::BASISVECTOR_RIGHT )       = Toshi::TVector4( vForward.z, 0.0f, -vForward.x, 0.0f );
	oRig.AsBasisVector4( Toshi::BASISVECTOR_UP )          = Toshi::TVector4( 0.0f, 1.0f, 0.0f, 0.0f ); // Barnyard's positive Y points down
	oRig.AsBasisVector4( Toshi::BASISVECTOR_FORWARD )    = Toshi::TVector4( vForward.x, 0.0f, vForward.z, 0.0f );
	oRig.AsBasisVector4( Toshi::BASISVECTOR_TRANSLATION ) = vCameraPosition;

	Toshi::TMatrix44 oEyeWorld;
	oEyeWorld.Multiply( oRig, oEyeLocal );
	return oEyeWorld;
}

static bool IsDXGIAdapterAvailable( const LUID& a_rcLuid )
{
	IDXGIFactory1* pFactory = TNULL;
	if ( FAILED( CreateDXGIFactory1( __uuidof( IDXGIFactory1 ), (void**)&pFactory ) ) )
		return false;

	bool bFound = false;
	for ( UINT uiAdapter = 0;; uiAdapter++ )
	{
		IDXGIAdapter1* pAdapter = TNULL;
		const HRESULT hEnumResult = pFactory->EnumAdapters1( uiAdapter, &pAdapter );
		if ( hEnumResult == DXGI_ERROR_NOT_FOUND )
			break;
		if ( FAILED( hEnumResult ) || !pAdapter )
			break;

		DXGI_ADAPTER_DESC1 oDesc = {};
		if ( SUCCEEDED( pAdapter->GetDesc1( &oDesc ) ) &&
		     oDesc.AdapterLuid.LowPart == a_rcLuid.LowPart && oDesc.AdapterLuid.HighPart == a_rcLuid.HighPart )
		{
			bFound = true;
			pAdapter->Release();
			break;
		}
		pAdapter->Release();
	}

	pFactory->Release();
	return bFound;
}

static DXGI_FORMAT ChooseDXGIFormat( const std::vector<int64_t>& a_rcFormats )
{
	const DXGI_FORMAT aPreferred[] = {
	    DXGI_FORMAT_R8G8B8A8_UNORM,
	    DXGI_FORMAT_B8G8R8A8_UNORM,
	    DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
	    DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
	};

	for ( DXGI_FORMAT ePreferred : aPreferred )
	{
		if ( std::find( a_rcFormats.begin(), a_rcFormats.end(), int64_t( ePreferred ) ) != a_rcFormats.end() )
			return ePreferred;
	}

	// The DX11 swapchain path supports these standard four-byte UNORM/sRGB formats.
	return DXGI_FORMAT_UNKNOWN;
}

} // namespace

struct AVRManager::Impl
{

	struct Target
	{
		XrSwapchain hSwapchain = XR_NULL_HANDLE;
		TUINT32 uiWidth = 0;
		TUINT32 uiHeight = 0;
		TUINT32 uiImageIndex = 0;
		TBOOL bAcquired = TFALSE;
		std::vector<XrSwapchainImageD3D11KHR> aD3D11Images;
		std::vector<ID3D11RenderTargetView*> aRenderTargetViews;
		std::vector<ID3D11ShaderResourceView*> aShaderResourceViews;
		// Legacy game shaders produce display-encoded RGB. Render into this UNORM
		// surface first, then convert to linear when copying to the OpenXR image.
		ID3D11Texture2D* pColorRenderTexture = TNULL;
		ID3D11RenderTargetView* pColorRenderTargetView = TNULL;
		ID3D11ShaderResourceView* pColorShaderResourceView = TNULL;
		Toshi::TRenderD3DInterface::DEPTHTARGET oRendererDepth = {};
	};

	struct ViewportSnapshot
	{
		Toshi::TViewport* pViewport = TNULL;
		TFLOAT fWidth = 0.0f;
		TFLOAT fHeight = 0.0f;
	};

	Toshi::TRenderInterface* pRenderer = TNULL;
	TBOOL bPrepared = TFALSE;
	TBOOL bGraphicsReady = TFALSE;
	TBOOL bSessionRunning = TFALSE;
	TBOOL bFrameBegun = TFALSE;
	TBOOL bShouldRender = TFALSE;
	TBOOL bViewsValid = TFALSE;
	TBOOL bProjectionReady = TFALSE;
	TBOOL bWorldRendered = TFALSE;
	TBOOL bUILayerReady = TFALSE;
	TBOOL bUILayerActive = TFALSE;
	TBOOL bEyeActive = TFALSE;
	TBOOL bHasNeutralPose = TFALSE;
	TBOOL bRecenterPending = TFALSE;
	TBOOL bExitRequested = TFALSE;
	TBOOL bRuntimeLost = TFALSE;
	TBOOL bDeviceLost = TFALSE;
	TBOOL bColorCorrectionAvailable = TFALSE;
	TBOOL bHasMirrorFrame = TFALSE;
	TUINT32 uiCompletedEyeMask = 0;
	TUINT32 uiActiveEye = std::numeric_limits<TUINT32>::max();
	TFLOAT fVRScale = 1.0f;
	TFLOAT fQuadHeightMeters = 1.0f;

	XrInstance hInstance = XR_NULL_HANDLE;
	XrSystemId hSystem = XR_NULL_SYSTEM_ID;
	XrSession hSession = XR_NULL_HANDLE;
	XrActionSet hGameplayActionSet = XR_NULL_HANDLE;
	XrPath hLeftHandPath = XR_NULL_PATH;
	XrPath hRightHandPath = XR_NULL_PATH;
	XrAction hLeftStickAction = XR_NULL_HANDLE;
	XrAction hRightStickAction = XR_NULL_HANDLE;
	XrAction hLeftMenuAction = XR_NULL_HANDLE;
	XrAction hRightMenuAction = XR_NULL_HANDLE;
	XrAction hLeftStickClickAction = XR_NULL_HANDLE;
	XrAction hRightStickClickAction = XR_NULL_HANDLE;
	XrAction hContext1Action = XR_NULL_HANDLE;
	XrAction hContext2Action = XR_NULL_HANDLE;
	XrAction hContext3Action = XR_NULL_HANDLE;
	XrAction hContext4Action = XR_NULL_HANDLE;
	XrAction hLeftSqueezeAction = XR_NULL_HANDLE;
	XrAction hRightSqueezeAction = XR_NULL_HANDLE;
	XrAction hLeftTriggerAction = XR_NULL_HANDLE;
	XrAction hRightTriggerAction = XR_NULL_HANDLE;
	AVRManager::ControllerInputState oControllerInputState;
	std::vector<XrPath> aInputSourceScratch = std::vector<XrPath>( 8 );
	TBOOL bInputActionsReady = TFALSE;
	TBOOL bInputActionsAttached = TFALSE;
	TBOOL bSessionFocused = TFALSE;
	TBOOL bLoggedInputSyncFailure = TFALSE;
	XrSpace hLocalSpace = XR_NULL_HANDLE;
	XrSpace hHeadSpace = XR_NULL_HANDLE;
	XrEnvironmentBlendMode eBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
	XrFrameState oFrameState = { XR_TYPE_FRAME_STATE };
	XrViewState oViewState = { XR_TYPE_VIEW_STATE };
	std::array<XrView, kEyeCount> aLocatedViews = {};
	std::array<XrViewConfigurationView, kEyeCount> aConfigurationViews = {};
	std::array<XrCompositionLayerProjectionView, kEyeCount> aProjectionViews = {};
	XrCompositionLayerProjection oProjectionLayer = { XR_TYPE_COMPOSITION_LAYER_PROJECTION };
	XrCompositionLayerQuad oQuadLayer = { XR_TYPE_COMPOSITION_LAYER_QUAD };
	XrSwapchainSubImage oQuadSubImage = {};
	XrVector3f vNeutralPosition = { 0.0f, 0.0f, 0.0f };
	XrQuaternionf qReferenceYaw = { 0.0f, 0.0f, 0.0f, 1.0f };
	XrPosef oQuadPose = {};
	XrExtent2Df oQuadExtent = {};
	int64_t i64SwapchainFormat = 0;

	Target aEyes[ kEyeCount ];
	Target oUI;

	Toshi::TCameraObject* pActiveCamera = TNULL;
	Toshi::TRenderContext* pActiveContext = TNULL;
	Toshi::TMatrix44 oSavedCameraMatrix;
	Toshi::TRenderContext::PROJECTIONPARAMS oSavedProjection = {};
	ViewportSnapshot aSavedViewports[ 3 ];

	XrGraphicsRequirementsD3D11KHR oD3D11Requirements = { XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR };
	ID3D11Device* pD3D11Device = TNULL;
	ID3D11DeviceContext* pD3D11Context = TNULL;
	ID3D11VertexShader* pMirrorVertexShader = TNULL;
	ID3D11PixelShader* pMirrorPixelShader = TNULL;
	ID3D11PixelShader* pColorResolvePixelShader = TNULL;
	ID3D11PixelShader* pColorResolveUIPixelShader = TNULL;
	ID3D11SamplerState* pMirrorSampler = TNULL;
	ID3D11RasterizerState* pMirrorRasterizer = TNULL;
	ID3D11BlendState* pMirrorAlphaBlend = TNULL;
};

AVRManager::AVRManager()
    : m_pImpl( new Impl() )
{
}

AVRManager::~AVRManager()
{
	Shutdown();
	delete m_pImpl;
}

AVRManager& AVRManager::GetSingleton()
{
	static AVRManager s_oInstance;
	return s_oInstance;
}

static void ClearControllerInputState( AVRManager::Impl& a_rImpl )
{
	a_rImpl.oControllerInputState = {};
}

static void DestroyInputActions( AVRManager::Impl& a_rImpl )
{
	XrAction* apActions[] = {
	    &a_rImpl.hLeftStickAction,
	    &a_rImpl.hRightStickAction,
	    &a_rImpl.hLeftMenuAction,
	    &a_rImpl.hRightMenuAction,
	    &a_rImpl.hLeftStickClickAction,
	    &a_rImpl.hRightStickClickAction,
	    &a_rImpl.hContext1Action,
	    &a_rImpl.hContext2Action,
	    &a_rImpl.hContext3Action,
	    &a_rImpl.hContext4Action,
	    &a_rImpl.hLeftSqueezeAction,
	    &a_rImpl.hRightSqueezeAction,
	    &a_rImpl.hLeftTriggerAction,
	    &a_rImpl.hRightTriggerAction,
	};

	for ( XrAction* pAction : apActions )
	{
		if ( *pAction != XR_NULL_HANDLE )
		{
			xrDestroyAction( *pAction );
			*pAction = XR_NULL_HANDLE;
		}
	}

	if ( a_rImpl.hGameplayActionSet != XR_NULL_HANDLE )
	{
		xrDestroyActionSet( a_rImpl.hGameplayActionSet );
		a_rImpl.hGameplayActionSet = XR_NULL_HANDLE;
	}

	a_rImpl.hLeftHandPath = XR_NULL_PATH;
	a_rImpl.hRightHandPath = XR_NULL_PATH;
	a_rImpl.bInputActionsReady = TFALSE;
	a_rImpl.bInputActionsAttached = TFALSE;
	a_rImpl.bLoggedInputSyncFailure = TFALSE;
	ClearControllerInputState( a_rImpl );
}

static bool CreateInputAction( AVRManager::Impl& a_rImpl, XrActionType a_eType, const char* a_szName, const char* a_szLocalizedName, XrPath a_hSubactionPath, XrAction& a_rAction )
{
	XrActionCreateInfo oActionInfo = { XR_TYPE_ACTION_CREATE_INFO };
	std::strncpy( oActionInfo.actionName, a_szName, sizeof( oActionInfo.actionName ) - 1 );
	std::strncpy( oActionInfo.localizedActionName, a_szLocalizedName, sizeof( oActionInfo.localizedActionName ) - 1 );
	oActionInfo.actionType = a_eType;
	oActionInfo.countSubactionPaths = 1;
	oActionInfo.subactionPaths = &a_hSubactionPath;
	return XR_SUCCEEDED( xrCreateAction( a_rImpl.hGameplayActionSet, &oActionInfo, &a_rAction ) );
}

struct ActionBinding
{
	XrAction   hAction;
	const char* szPath;
};

static bool SuggestInteractionProfileBindings( AVRManager::Impl& a_rImpl, const char* a_szProfile, const ActionBinding* a_pBindings, size_t a_uiNumBindings )
{
	XrPath hProfilePath = XR_NULL_PATH;
	if ( XR_FAILED( xrStringToPath( a_rImpl.hInstance, a_szProfile, &hProfilePath ) ) )
		return false;

	std::vector<XrActionSuggestedBinding> aSuggestedBindings;
	aSuggestedBindings.reserve( a_uiNumBindings );
	for ( size_t i = 0; i < a_uiNumBindings; i++ )
	{
		XrPath hBindingPath = XR_NULL_PATH;
		if ( XR_SUCCEEDED( xrStringToPath( a_rImpl.hInstance, a_pBindings[ i ].szPath, &hBindingPath ) ) )
			aSuggestedBindings.push_back( { a_pBindings[ i ].hAction, hBindingPath } );
	}

	if ( aSuggestedBindings.empty() )
		return false;

	XrInteractionProfileSuggestedBinding oSuggested = { XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING };
	oSuggested.interactionProfile = hProfilePath;
	oSuggested.countSuggestedBindings = uint32_t( aSuggestedBindings.size() );
	oSuggested.suggestedBindings = aSuggestedBindings.data();
	const XrResult eResult = xrSuggestInteractionProfileBindings( a_rImpl.hInstance, &oSuggested );
	if ( XR_SUCCEEDED( eResult ) )
		return true;

	// A runtime is allowed not to support an interaction profile. The other profiles
	// remain available, so only report errors other than that normal case.
	if ( eResult != XR_ERROR_PATH_UNSUPPORTED )
		TWARN( "OpenXR: could not suggest bindings for interaction profile %s (result %d).\n", a_szProfile, int( eResult ) );

	return false;
}

static TBOOL CreateInputActions( AVRManager::Impl& a_rImpl )
{
	if ( XR_FAILED( xrStringToPath( a_rImpl.hInstance, "/user/hand/left", &a_rImpl.hLeftHandPath ) ) ||
	     XR_FAILED( xrStringToPath( a_rImpl.hInstance, "/user/hand/right", &a_rImpl.hRightHandPath ) ) )
		return TFALSE;

	XrActionSetCreateInfo oActionSetInfo = { XR_TYPE_ACTION_SET_CREATE_INFO };
	std::strncpy( oActionSetInfo.actionSetName, "barnyard_gameplay", sizeof( oActionSetInfo.actionSetName ) - 1 );
	std::strncpy( oActionSetInfo.localizedActionSetName, "Barnyard Gameplay", sizeof( oActionSetInfo.localizedActionSetName ) - 1 );
	if ( XR_FAILED( xrCreateActionSet( a_rImpl.hInstance, &oActionSetInfo, &a_rImpl.hGameplayActionSet ) ) )
		return TFALSE;

	const bool bActionsCreated =
	    CreateInputAction( a_rImpl, XR_ACTION_TYPE_VECTOR2F_INPUT, "left_stick", "Left Stick", a_rImpl.hLeftHandPath, a_rImpl.hLeftStickAction ) &&
	    CreateInputAction( a_rImpl, XR_ACTION_TYPE_VECTOR2F_INPUT, "right_stick", "Right Stick", a_rImpl.hRightHandPath, a_rImpl.hRightStickAction ) &&
	    CreateInputAction( a_rImpl, XR_ACTION_TYPE_BOOLEAN_INPUT, "left_menu", "Left Menu", a_rImpl.hLeftHandPath, a_rImpl.hLeftMenuAction ) &&
	    CreateInputAction( a_rImpl, XR_ACTION_TYPE_BOOLEAN_INPUT, "right_menu", "Right Menu", a_rImpl.hRightHandPath, a_rImpl.hRightMenuAction ) &&
	    CreateInputAction( a_rImpl, XR_ACTION_TYPE_BOOLEAN_INPUT, "left_stick_click", "Left Stick Click", a_rImpl.hLeftHandPath, a_rImpl.hLeftStickClickAction ) &&
	    CreateInputAction( a_rImpl, XR_ACTION_TYPE_BOOLEAN_INPUT, "right_stick_click", "Right Stick Click", a_rImpl.hRightHandPath, a_rImpl.hRightStickClickAction ) &&
	    CreateInputAction( a_rImpl, XR_ACTION_TYPE_BOOLEAN_INPUT, "context_1", "Top Face Button", a_rImpl.hLeftHandPath, a_rImpl.hContext1Action ) &&
	    CreateInputAction( a_rImpl, XR_ACTION_TYPE_BOOLEAN_INPUT, "context_2", "Right Face Button", a_rImpl.hRightHandPath, a_rImpl.hContext2Action ) &&
	    CreateInputAction( a_rImpl, XR_ACTION_TYPE_BOOLEAN_INPUT, "context_3", "Bottom Face Button", a_rImpl.hRightHandPath, a_rImpl.hContext3Action ) &&
	    CreateInputAction( a_rImpl, XR_ACTION_TYPE_BOOLEAN_INPUT, "context_4", "Left Face Button", a_rImpl.hLeftHandPath, a_rImpl.hContext4Action ) &&
	    CreateInputAction( a_rImpl, XR_ACTION_TYPE_FLOAT_INPUT, "left_squeeze", "Left Squeeze", a_rImpl.hLeftHandPath, a_rImpl.hLeftSqueezeAction ) &&
	    CreateInputAction( a_rImpl, XR_ACTION_TYPE_FLOAT_INPUT, "right_squeeze", "Right Squeeze", a_rImpl.hRightHandPath, a_rImpl.hRightSqueezeAction ) &&
	    CreateInputAction( a_rImpl, XR_ACTION_TYPE_FLOAT_INPUT, "left_trigger", "Left Trigger", a_rImpl.hLeftHandPath, a_rImpl.hLeftTriggerAction ) &&
	    CreateInputAction( a_rImpl, XR_ACTION_TYPE_FLOAT_INPUT, "right_trigger", "Right Trigger", a_rImpl.hRightHandPath, a_rImpl.hRightTriggerAction );

	if ( !bActionsCreated )
	{
		DestroyInputActions( a_rImpl );
		return TFALSE;
	}

	const ActionBinding aTouchBindings[] = {
	    { a_rImpl.hLeftStickAction, "/user/hand/left/input/thumbstick" },
	    { a_rImpl.hRightStickAction, "/user/hand/right/input/thumbstick" },
	    { a_rImpl.hLeftMenuAction, "/user/hand/left/input/menu/click" },
	    { a_rImpl.hLeftStickClickAction, "/user/hand/left/input/thumbstick/click" },
	    { a_rImpl.hRightStickClickAction, "/user/hand/right/input/thumbstick/click" },
	    { a_rImpl.hContext1Action, "/user/hand/left/input/y/click" },
	    { a_rImpl.hContext2Action, "/user/hand/right/input/b/click" },
	    { a_rImpl.hContext3Action, "/user/hand/right/input/a/click" },
	    { a_rImpl.hContext4Action, "/user/hand/left/input/x/click" },
	    { a_rImpl.hLeftSqueezeAction, "/user/hand/left/input/squeeze/value" },
	    { a_rImpl.hRightSqueezeAction, "/user/hand/right/input/squeeze/value" },
	    { a_rImpl.hLeftTriggerAction, "/user/hand/left/input/trigger/value" },
	    { a_rImpl.hRightTriggerAction, "/user/hand/right/input/trigger/value" },
	};
	const ActionBinding aIndexBindings[] = {
	    { a_rImpl.hLeftStickAction, "/user/hand/left/input/thumbstick" },
	    { a_rImpl.hRightStickAction, "/user/hand/right/input/thumbstick" },
	    { a_rImpl.hLeftStickClickAction, "/user/hand/left/input/thumbstick/click" },
	    { a_rImpl.hRightStickClickAction, "/user/hand/right/input/thumbstick/click" },
	    { a_rImpl.hContext1Action, "/user/hand/left/input/a/click" },
	    { a_rImpl.hContext4Action, "/user/hand/left/input/b/click" },
	    { a_rImpl.hContext2Action, "/user/hand/right/input/b/click" },
	    { a_rImpl.hContext3Action, "/user/hand/right/input/a/click" },
	    { a_rImpl.hLeftSqueezeAction, "/user/hand/left/input/squeeze/value" },
	    { a_rImpl.hRightSqueezeAction, "/user/hand/right/input/squeeze/value" },
	    { a_rImpl.hLeftTriggerAction, "/user/hand/left/input/trigger/value" },
	    { a_rImpl.hRightTriggerAction, "/user/hand/right/input/trigger/value" },
	};
	const ActionBinding aMicrosoftBindings[] = {
	    { a_rImpl.hLeftStickAction, "/user/hand/left/input/thumbstick" },
	    { a_rImpl.hRightStickAction, "/user/hand/right/input/thumbstick" },
	    { a_rImpl.hLeftMenuAction, "/user/hand/left/input/menu/click" },
	    { a_rImpl.hRightMenuAction, "/user/hand/right/input/menu/click" },
	    { a_rImpl.hLeftStickClickAction, "/user/hand/left/input/thumbstick/click" },
	    { a_rImpl.hRightStickClickAction, "/user/hand/right/input/thumbstick/click" },
	    { a_rImpl.hContext1Action, "/user/hand/left/input/y/click" },
	    { a_rImpl.hContext2Action, "/user/hand/right/input/b/click" },
	    { a_rImpl.hContext3Action, "/user/hand/right/input/a/click" },
	    { a_rImpl.hContext4Action, "/user/hand/left/input/x/click" },
	    { a_rImpl.hLeftSqueezeAction, "/user/hand/left/input/squeeze/value" },
	    { a_rImpl.hRightSqueezeAction, "/user/hand/right/input/squeeze/value" },
	    { a_rImpl.hLeftTriggerAction, "/user/hand/left/input/trigger/value" },
	    { a_rImpl.hRightTriggerAction, "/user/hand/right/input/trigger/value" },
	};
	const ActionBinding aViveBindings[] = {
	    { a_rImpl.hLeftStickAction, "/user/hand/left/input/trackpad" },
	    { a_rImpl.hRightStickAction, "/user/hand/right/input/trackpad" },
	    { a_rImpl.hLeftMenuAction, "/user/hand/left/input/menu/click" },
	    { a_rImpl.hRightMenuAction, "/user/hand/right/input/menu/click" },
	    { a_rImpl.hLeftStickClickAction, "/user/hand/left/input/trackpad/click" },
	    { a_rImpl.hRightStickClickAction, "/user/hand/right/input/trackpad/click" },
	    { a_rImpl.hLeftTriggerAction, "/user/hand/left/input/trigger/value" },
	    { a_rImpl.hRightTriggerAction, "/user/hand/right/input/trigger/value" },
	};

	TBOOL bHasBindings = TFALSE;
	bHasBindings |= SuggestInteractionProfileBindings( a_rImpl, "/interaction_profiles/oculus/touch_controller", aTouchBindings, TARRAYSIZE( aTouchBindings ) );
	bHasBindings |= SuggestInteractionProfileBindings( a_rImpl, "/interaction_profiles/meta/touch_controller_plus", aTouchBindings, TARRAYSIZE( aTouchBindings ) );
	bHasBindings |= SuggestInteractionProfileBindings( a_rImpl, "/interaction_profiles/facebook/touch_controller_pro", aTouchBindings, TARRAYSIZE( aTouchBindings ) );
	bHasBindings |= SuggestInteractionProfileBindings( a_rImpl, "/interaction_profiles/valve/index_controller", aIndexBindings, TARRAYSIZE( aIndexBindings ) );
	bHasBindings |= SuggestInteractionProfileBindings( a_rImpl, "/interaction_profiles/microsoft/motion_controller", aMicrosoftBindings, TARRAYSIZE( aMicrosoftBindings ) );
	bHasBindings |= SuggestInteractionProfileBindings( a_rImpl, "/interaction_profiles/htc/vive_controller", aViveBindings, TARRAYSIZE( aViveBindings ) );
	if ( !bHasBindings )
	{
		TWARN( "OpenXR: no supported controller interaction profile accepted Barnyard's standard bindings.\n" );
		DestroyInputActions( a_rImpl );
		return TFALSE;
	}

	a_rImpl.bInputActionsReady = TTRUE;
	return TTRUE;
}

static void AttachInputActions( AVRManager::Impl& a_rImpl )
{
	a_rImpl.bInputActionsAttached = TFALSE;
	ClearControllerInputState( a_rImpl );
	if ( !a_rImpl.bInputActionsReady || a_rImpl.hSession == XR_NULL_HANDLE )
		return;

	XrSessionActionSetsAttachInfo oAttachInfo = { XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO };
	oAttachInfo.countActionSets = 1;
	oAttachInfo.actionSets = &a_rImpl.hGameplayActionSet;
	const XrResult eResult = xrAttachSessionActionSets( a_rImpl.hSession, &oAttachInfo );
	if ( XR_SUCCEEDED( eResult ) )
	{
		a_rImpl.bInputActionsAttached = TTRUE;
		a_rImpl.bLoggedInputSyncFailure = TFALSE;
	}
	else
	{
		TWARN( "OpenXR: could not attach the gameplay action set to the session (result %d).\n", int( eResult ) );
	}
}

static TBOOL ReadBooleanAction( AVRManager::Impl& a_rImpl, XrAction a_hAction, XrPath a_hSubactionPath )
{
	XrActionStateGetInfo oGetInfo = { XR_TYPE_ACTION_STATE_GET_INFO };
	oGetInfo.action = a_hAction;
	oGetInfo.subactionPath = a_hSubactionPath;
	XrActionStateBoolean oState = { XR_TYPE_ACTION_STATE_BOOLEAN };
	if ( XR_FAILED( xrGetActionStateBoolean( a_rImpl.hSession, &oGetInfo, &oState ) ) )
		return TFALSE;
	return oState.isActive && oState.currentState;
}

static TFLOAT ReadFloatAction( AVRManager::Impl& a_rImpl, XrAction a_hAction, XrPath a_hSubactionPath )
{
	XrActionStateGetInfo oGetInfo = { XR_TYPE_ACTION_STATE_GET_INFO };
	oGetInfo.action = a_hAction;
	oGetInfo.subactionPath = a_hSubactionPath;
	XrActionStateFloat oState = { XR_TYPE_ACTION_STATE_FLOAT };
	if ( XR_FAILED( xrGetActionStateFloat( a_rImpl.hSession, &oGetInfo, &oState ) ) || !oState.isActive || !std::isfinite( oState.currentState ) )
		return 0.0f;
	return std::max( 0.0f, std::min( oState.currentState, 1.0f ) );
}

static XrVector2f ReadVector2Action( AVRManager::Impl& a_rImpl, XrAction a_hAction, XrPath a_hSubactionPath )
{
	XrActionStateGetInfo oGetInfo = { XR_TYPE_ACTION_STATE_GET_INFO };
	oGetInfo.action = a_hAction;
	oGetInfo.subactionPath = a_hSubactionPath;
	XrActionStateVector2f oState = { XR_TYPE_ACTION_STATE_VECTOR2F };
	if ( XR_FAILED( xrGetActionStateVector2f( a_rImpl.hSession, &oGetInfo, &oState ) ) || !oState.isActive )
		return { 0.0f, 0.0f };

	return {
	    std::isfinite( oState.currentState.x ) ? std::max( -1.0f, std::min( oState.currentState.x, 1.0f ) ) : 0.0f,
	    std::isfinite( oState.currentState.y ) ? std::max( -1.0f, std::min( oState.currentState.y, 1.0f ) ) : 0.0f,
	};
}

// SteamVR can emulate Touch controllers from optical hands while continuing
// to report the Touch interaction profile. In that mode a trigger is actually
// index_pinch and the other buttons/grip can also be gestures. Do not turn
// keyboard/gamepad hand movements into gameplay or prompt-mode changes.
static TBOOL HasPinchTriggerSource( AVRManager::Impl& a_rImpl, XrAction a_hTrigger )
{
	XrBoundSourcesForActionEnumerateInfo oInfo = { XR_TYPE_BOUND_SOURCES_FOR_ACTION_ENUMERATE_INFO };
	oInfo.action = a_hTrigger;
	auto& aSources = a_rImpl.aInputSourceScratch;
	uint32_t uiCount = 0;
	XrResult eResult = xrEnumerateBoundSourcesForAction( a_rImpl.hSession, &oInfo, uint32_t( aSources.size() ), &uiCount, aSources.data() );
	if ( eResult == XR_ERROR_SIZE_INSUFFICIENT )
	{
		aSources.resize( uiCount );
		eResult = xrEnumerateBoundSourcesForAction( a_rImpl.hSession, &oInfo, uint32_t( aSources.size() ), &uiCount, aSources.data() );
	}
	// A metadata failure must not disable otherwise working physical controllers.
	if ( XR_FAILED( eResult ) ) return TFALSE;
	for ( uint32_t i = 0; i < uiCount; ++i )
	{
		char szSource[ XR_MAX_PATH_LENGTH ] = {};
		uint32_t uiLength = 0;
		if ( XR_SUCCEEDED( xrPathToString( a_rImpl.hInstance, aSources[ i ], sizeof( szSource ), &uiLength, szSource ) ) &&
		     std::strstr( szSource, "_pinch/" ) )
			return TTRUE;
	}
	return TFALSE;
}

static void SyncControllerInput( AVRManager::Impl& a_rImpl )
{
	ClearControllerInputState( a_rImpl );
	if ( !a_rImpl.bInputActionsReady || !a_rImpl.bInputActionsAttached || !a_rImpl.bSessionRunning || !a_rImpl.bSessionFocused )
		return;

	XrActiveActionSet oActiveActionSet = { a_rImpl.hGameplayActionSet, XR_NULL_PATH };
	XrActionsSyncInfo oSyncInfo = { XR_TYPE_ACTIONS_SYNC_INFO };
	oSyncInfo.countActiveActionSets = 1;
	oSyncInfo.activeActionSets = &oActiveActionSet;
	const XrResult eSyncResult = xrSyncActions( a_rImpl.hSession, &oSyncInfo );
	if ( eSyncResult == XR_SESSION_NOT_FOCUSED )
		return;
	if ( XR_FAILED( eSyncResult ) )
	{
		if ( !a_rImpl.bLoggedInputSyncFailure )
		{
			TWARN( "OpenXR: controller input synchronization failed (result %d).\n", int( eSyncResult ) );
			a_rImpl.bLoggedInputSyncFailure = TTRUE;
		}
		return;
	}
	a_rImpl.bLoggedInputSyncFailure = TFALSE;

	const XrVector2f oLeftStick = ReadVector2Action( a_rImpl, a_rImpl.hLeftStickAction, a_rImpl.hLeftHandPath );
	const XrVector2f oRightStick = ReadVector2Action( a_rImpl, a_rImpl.hRightStickAction, a_rImpl.hRightHandPath );
	a_rImpl.oControllerInputState.fLeftStickX = oLeftStick.x;
	a_rImpl.oControllerInputState.fLeftStickY = oLeftStick.y;
	a_rImpl.oControllerInputState.fRightStickX = oRightStick.x;
	a_rImpl.oControllerInputState.fRightStickY = oRightStick.y;

	auto SetButton = [&a_rImpl]( AVRManager::ControllerButton a_eButton, TBOOL a_bDown ) {
		if ( a_bDown )
			a_rImpl.oControllerInputState.uiButtonMask |= TUINT32( a_eButton );
	};
	SetButton( AVRManager::CONTROLLER_BUTTON_SELECT, ReadBooleanAction( a_rImpl, a_rImpl.hLeftMenuAction, a_rImpl.hLeftHandPath ) );
	SetButton( AVRManager::CONTROLLER_BUTTON_START, ReadBooleanAction( a_rImpl, a_rImpl.hRightMenuAction, a_rImpl.hRightHandPath ) );
	SetButton( AVRManager::CONTROLLER_BUTTON_STICK_LEFT, ReadBooleanAction( a_rImpl, a_rImpl.hLeftStickClickAction, a_rImpl.hLeftHandPath ) );
	SetButton( AVRManager::CONTROLLER_BUTTON_STICK_RIGHT, ReadBooleanAction( a_rImpl, a_rImpl.hRightStickClickAction, a_rImpl.hRightHandPath ) );
	SetButton( AVRManager::CONTROLLER_BUTTON_CONTEXT_1, ReadBooleanAction( a_rImpl, a_rImpl.hContext1Action, a_rImpl.hLeftHandPath ) );
	SetButton( AVRManager::CONTROLLER_BUTTON_CONTEXT_2, ReadBooleanAction( a_rImpl, a_rImpl.hContext2Action, a_rImpl.hRightHandPath ) );
	SetButton( AVRManager::CONTROLLER_BUTTON_CONTEXT_3, ReadBooleanAction( a_rImpl, a_rImpl.hContext3Action, a_rImpl.hRightHandPath ) );
	SetButton( AVRManager::CONTROLLER_BUTTON_CONTEXT_4, ReadBooleanAction( a_rImpl, a_rImpl.hContext4Action, a_rImpl.hLeftHandPath ) );
	SetButton( AVRManager::CONTROLLER_BUTTON_SHOULDER_LEFT_1, ReadFloatAction( a_rImpl, a_rImpl.hLeftSqueezeAction, a_rImpl.hLeftHandPath ) >= kGripButtonThreshold );
	SetButton( AVRManager::CONTROLLER_BUTTON_SHOULDER_RIGHT_1, ReadFloatAction( a_rImpl, a_rImpl.hRightSqueezeAction, a_rImpl.hRightHandPath ) >= kGripButtonThreshold );
	SetButton( AVRManager::CONTROLLER_BUTTON_SHOULDER_LEFT_2, ReadFloatAction( a_rImpl, a_rImpl.hLeftTriggerAction, a_rImpl.hLeftHandPath ) >= kTriggerButtonThreshold );
	SetButton( AVRManager::CONTROLLER_BUTTON_SHOULDER_RIGHT_2, ReadFloatAction( a_rImpl, a_rImpl.hRightTriggerAction, a_rImpl.hRightHandPath ) >= kTriggerButtonThreshold );
	// Re-query after each sync: switching between optical hands and physical
	// Touch controllers need not change the reported interaction profile. Clear
	// the whole emulated hand, not just pinch, since grip/face buttons are also
	// synthesized. The adapter emits normal releases when these bits disappear.
	const TUINT32 uiLeftHandButtons = AVRManager::CONTROLLER_BUTTON_SELECT | AVRManager::CONTROLLER_BUTTON_STICK_LEFT |
	    AVRManager::CONTROLLER_BUTTON_CONTEXT_1 | AVRManager::CONTROLLER_BUTTON_CONTEXT_4 |
	    AVRManager::CONTROLLER_BUTTON_SHOULDER_LEFT_1 | AVRManager::CONTROLLER_BUTTON_SHOULDER_LEFT_2;
	const TUINT32 uiRightHandButtons = AVRManager::CONTROLLER_BUTTON_START | AVRManager::CONTROLLER_BUTTON_STICK_RIGHT |
	    AVRManager::CONTROLLER_BUTTON_CONTEXT_2 | AVRManager::CONTROLLER_BUTTON_CONTEXT_3 |
	    AVRManager::CONTROLLER_BUTTON_SHOULDER_RIGHT_1 | AVRManager::CONTROLLER_BUTTON_SHOULDER_RIGHT_2;
	if ( HasPinchTriggerSource( a_rImpl, a_rImpl.hLeftTriggerAction ) )
	{
		a_rImpl.oControllerInputState.uiButtonMask &= ~uiLeftHandButtons;
		a_rImpl.oControllerInputState.fLeftStickX = a_rImpl.oControllerInputState.fLeftStickY = 0.0f;
	}
	if ( HasPinchTriggerSource( a_rImpl, a_rImpl.hRightTriggerAction ) )
	{
		a_rImpl.oControllerInputState.uiButtonMask &= ~uiRightHandButtons;
		a_rImpl.oControllerInputState.fRightStickX = a_rImpl.oControllerInputState.fRightStickY = 0.0f;
	}
}

static void DestroyMirrorPipeline( AVRManager::Impl& a_rImpl )
{
	if ( a_rImpl.pMirrorAlphaBlend )
	{
		a_rImpl.pMirrorAlphaBlend->Release();
		a_rImpl.pMirrorAlphaBlend = TNULL;
	}
	if ( a_rImpl.pMirrorRasterizer )
	{
		a_rImpl.pMirrorRasterizer->Release();
		a_rImpl.pMirrorRasterizer = TNULL;
	}
	if ( a_rImpl.pMirrorSampler )
	{
		a_rImpl.pMirrorSampler->Release();
		a_rImpl.pMirrorSampler = TNULL;
	}
	ID3D11PixelShader** apPixelShaders[] = {
	    &a_rImpl.pMirrorPixelShader,
	    &a_rImpl.pColorResolvePixelShader,
	    &a_rImpl.pColorResolveUIPixelShader,
	};
	for ( ID3D11PixelShader** ppShader : apPixelShaders )
	{
		if ( *ppShader )
		{
			( *ppShader )->Release();
			*ppShader = TNULL;
		}
	}
	if ( a_rImpl.pMirrorVertexShader )
	{
		a_rImpl.pMirrorVertexShader->Release();
		a_rImpl.pMirrorVertexShader = TNULL;
	}
	a_rImpl.bColorCorrectionAvailable = TFALSE;
}

static void DestroyTargetColorIntermediate( AVRManager::Impl::Target& a_rTarget )
{
	if ( a_rTarget.pColorShaderResourceView )
	{
		a_rTarget.pColorShaderResourceView->Release();
		a_rTarget.pColorShaderResourceView = TNULL;
	}
	if ( a_rTarget.pColorRenderTargetView )
	{
		a_rTarget.pColorRenderTargetView->Release();
		a_rTarget.pColorRenderTargetView = TNULL;
	}
	if ( a_rTarget.pColorRenderTexture )
	{
		a_rTarget.pColorRenderTexture->Release();
		a_rTarget.pColorRenderTexture = TNULL;
	}
}

static void DestroyTargetDX11( AVRManager::Impl& a_rImpl, AVRManager::Impl::Target& a_rTarget )
{
	for ( ID3D11ShaderResourceView* pSRV : a_rTarget.aShaderResourceViews )
		if ( pSRV ) pSRV->Release();
	for ( ID3D11RenderTargetView* pRTV : a_rTarget.aRenderTargetViews )
		if ( pRTV ) pRTV->Release();
	a_rTarget.aShaderResourceViews.clear();
	a_rTarget.aRenderTargetViews.clear();
	a_rTarget.aD3D11Images.clear();
	DestroyTargetColorIntermediate( a_rTarget );

	if ( a_rImpl.pRenderer && ( a_rTarget.oRendererDepth.pView || a_rTarget.oRendererDepth.pReadOnlyView || a_rTarget.oRendererDepth.pSRV ) )
	{
		Toshi::TRenderD3DInterface::Interface()->DestroyCustomDepthTarget( a_rTarget.oRendererDepth );
	}
}

static void DestroyTarget( AVRManager::Impl& a_rImpl, AVRManager::Impl::Target& a_rTarget )
{
	DestroyTargetDX11( a_rImpl, a_rTarget );
	if ( a_rTarget.hSwapchain != XR_NULL_HANDLE && a_rImpl.hSession != XR_NULL_HANDLE )
		xrDestroySwapchain( a_rTarget.hSwapchain );
	a_rTarget.hSwapchain    = XR_NULL_HANDLE;
	a_rTarget.uiWidth       = 0;
	a_rTarget.uiHeight      = 0;
	a_rTarget.uiImageIndex  = 0;
	a_rTarget.bAcquired     = TFALSE;
}

static void ReleaseTargetImage( AVRManager::Impl& a_rImpl, AVRManager::Impl::Target& a_rTarget )
{
	if ( !a_rTarget.bAcquired || a_rTarget.hSwapchain == XR_NULL_HANDLE )
		return;

	XrSwapchainImageReleaseInfo oReleaseInfo = { XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
	xrReleaseSwapchainImage( a_rTarget.hSwapchain, &oReleaseInfo );
	a_rTarget.bAcquired = TFALSE;
}

static TBOOL AcquireTargetImage( AVRManager::Impl::Target& a_rTarget )
{
	if ( a_rTarget.hSwapchain == XR_NULL_HANDLE )
		return TFALSE;

	XrSwapchainImageAcquireInfo oAcquireInfo = { XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
	if ( XR_FAILED( xrAcquireSwapchainImage( a_rTarget.hSwapchain, &oAcquireInfo, &a_rTarget.uiImageIndex ) ) )
		return TFALSE;

	a_rTarget.bAcquired = TTRUE;
	XrSwapchainImageWaitInfo oWaitInfo = { XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
	oWaitInfo.timeout = XR_INFINITE_DURATION;
	if ( XR_FAILED( xrWaitSwapchainImage( a_rTarget.hSwapchain, &oWaitInfo ) ) )
	{
		XrSwapchainImageReleaseInfo oReleaseInfo = { XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
		xrReleaseSwapchainImage( a_rTarget.hSwapchain, &oReleaseInfo );
		a_rTarget.bAcquired = TFALSE;
		return TFALSE;
	}

	return TTRUE;
}

static TBOOL CreateDepthForTarget( AVRManager::Impl& a_rImpl, AVRManager::Impl::Target& a_rTarget )
{
	(void)a_rImpl;
	return Toshi::TRenderD3DInterface::Interface()->CreateCustomDepthTarget(
	           a_rTarget.uiWidth, a_rTarget.uiHeight, DXGI_FORMAT_D24_UNORM_S8_UINT, 1, a_rTarget.oRendererDepth )
	    ? TTRUE
	    : TFALSE;
}

static TBOOL CreateTargetColorIntermediate( AVRManager::Impl& a_rImpl, AVRManager::Impl::Target& a_rTarget )
{
	D3D11_TEXTURE2D_DESC oTextureDesc = {};
	oTextureDesc.Width                 = a_rTarget.uiWidth;
	oTextureDesc.Height                = a_rTarget.uiHeight;
	oTextureDesc.MipLevels             = 1;
	oTextureDesc.ArraySize             = 1;
	oTextureDesc.Format                = DXGI_FORMAT_R8G8B8A8_UNORM;
	oTextureDesc.SampleDesc.Count      = 1;
	oTextureDesc.Usage                 = D3D11_USAGE_DEFAULT;
	oTextureDesc.BindFlags             = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	if ( FAILED( a_rImpl.pD3D11Device->CreateTexture2D( &oTextureDesc, TNULL, &a_rTarget.pColorRenderTexture ) ) )
		return TFALSE;

	D3D11_RENDER_TARGET_VIEW_DESC oRTVDesc = {};
	oRTVDesc.Format                       = oTextureDesc.Format;
	oRTVDesc.ViewDimension                = D3D11_RTV_DIMENSION_TEXTURE2D;
	if ( FAILED( a_rImpl.pD3D11Device->CreateRenderTargetView( a_rTarget.pColorRenderTexture, &oRTVDesc, &a_rTarget.pColorRenderTargetView ) ) )
	{
		DestroyTargetColorIntermediate( a_rTarget );
		return TFALSE;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC oSRVDesc = {};
	oSRVDesc.Format                          = oTextureDesc.Format;
	oSRVDesc.ViewDimension                   = D3D11_SRV_DIMENSION_TEXTURE2D;
	oSRVDesc.Texture2D.MipLevels             = 1;
	if ( FAILED( a_rImpl.pD3D11Device->CreateShaderResourceView( a_rTarget.pColorRenderTexture, &oSRVDesc, &a_rTarget.pColorShaderResourceView ) ) )
	{
		DestroyTargetColorIntermediate( a_rTarget );
		return TFALSE;
	}

	return TTRUE;
}

static TBOOL CreateDX11TargetImages( AVRManager::Impl& a_rImpl, AVRManager::Impl::Target& a_rTarget )
{
	uint32_t uiImageCount = 0;
	if ( XR_FAILED( xrEnumerateSwapchainImages( a_rTarget.hSwapchain, 0, &uiImageCount, TNULL ) ) || uiImageCount == 0 )
		return TFALSE;

	a_rTarget.aD3D11Images.resize( uiImageCount );
	a_rTarget.aRenderTargetViews.resize( uiImageCount, TNULL );
	a_rTarget.aShaderResourceViews.resize( uiImageCount, TNULL );
	for ( auto& rImage : a_rTarget.aD3D11Images )
	{
		rImage = {};
		rImage.type = XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR;
	}

	if ( XR_FAILED( xrEnumerateSwapchainImages( a_rTarget.hSwapchain, uiImageCount, &uiImageCount, reinterpret_cast<XrSwapchainImageBaseHeader*>( a_rTarget.aD3D11Images.data() ) ) ) )
		return TFALSE;

	// OpenXR swapchain images may be typed sRGB resources; create views with the
	// exact advertised format (UNORM and sRGB views are not interchangeable here).
	const DXGI_FORMAT eViewFormat = DXGI_FORMAT( a_rImpl.i64SwapchainFormat );
	for ( uint32_t i = 0; i < uiImageCount; i++ )
	{
		D3D11_RENDER_TARGET_VIEW_DESC oRTVDesc = {};
		oRTVDesc.Format                       = eViewFormat;
		oRTVDesc.ViewDimension                = D3D11_RTV_DIMENSION_TEXTURE2D;
		if ( FAILED( a_rImpl.pD3D11Device->CreateRenderTargetView( a_rTarget.aD3D11Images[ i ].texture, &oRTVDesc, &a_rTarget.aRenderTargetViews[ i ] ) ) )
			return TFALSE;

		D3D11_SHADER_RESOURCE_VIEW_DESC oSRVDesc = {};
		oSRVDesc.Format                          = eViewFormat;
		oSRVDesc.ViewDimension                   = D3D11_SRV_DIMENSION_TEXTURE2D;
		oSRVDesc.Texture2D.MipLevels             = 1;
		if ( FAILED( a_rImpl.pD3D11Device->CreateShaderResourceView( a_rTarget.aD3D11Images[ i ].texture, &oSRVDesc, &a_rTarget.aShaderResourceViews[ i ] ) ) )
			return TFALSE;
	}

	if ( !CreateDepthForTarget( a_rImpl, a_rTarget ) )
		return TFALSE;

	return TTRUE;
}


static TBOOL CreateTarget( AVRManager::Impl& a_rImpl, AVRManager::Impl::Target& a_rTarget, TUINT32 a_uiWidth, TUINT32 a_uiHeight )
{
	a_rTarget.uiWidth  = a_uiWidth;
	a_rTarget.uiHeight = a_uiHeight;

	XrSwapchainCreateInfo oCreateInfo = { XR_TYPE_SWAPCHAIN_CREATE_INFO };
	oCreateInfo.usageFlags  = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
	oCreateInfo.format      = a_rImpl.i64SwapchainFormat;
	oCreateInfo.sampleCount = 1;
	oCreateInfo.width       = a_uiWidth;
	oCreateInfo.height      = a_uiHeight;
	oCreateInfo.faceCount   = 1;
	oCreateInfo.arraySize   = 1;
	oCreateInfo.mipCount    = 1;

	if ( XR_FAILED( xrCreateSwapchain( a_rImpl.hSession, &oCreateInfo, &a_rTarget.hSwapchain ) ) )
		return TFALSE;

	if ( !CreateDX11TargetImages( a_rImpl, a_rTarget ) )
		return TFALSE;

	if ( a_rImpl.bColorCorrectionAvailable && !CreateTargetColorIntermediate( a_rImpl, a_rTarget ) )
	{
		TWARN( "OpenXR: linear color intermediate allocation failed; using the legacy direct render path.\n" );
		a_rImpl.bColorCorrectionAvailable = TFALSE;
		DestroyTargetColorIntermediate( a_rTarget );
	}
	return TTRUE;
}

using t_D3DCompile = HRESULT ( WINAPI* )( LPCVOID, SIZE_T, LPCSTR, const D3D_SHADER_MACRO*, ID3DInclude*, LPCSTR, LPCSTR, UINT, UINT, ID3DBlob**, ID3DBlob** );
static TBOOL CreateMirrorPipeline( AVRManager::Impl& a_rImpl )
{
	if ( a_rImpl.pMirrorVertexShader && a_rImpl.pMirrorPixelShader && a_rImpl.pColorResolvePixelShader &&
	     a_rImpl.pColorResolveUIPixelShader && a_rImpl.pMirrorSampler && a_rImpl.pMirrorRasterizer && a_rImpl.pMirrorAlphaBlend )
	{
		a_rImpl.bColorCorrectionAvailable = TTRUE;
		return TTRUE;
	}

	DestroyMirrorPipeline( a_rImpl );
	HMODULE hCompiler = LoadLibraryA( "d3dcompiler_47.dll" );
	if ( !hCompiler )
		hCompiler = LoadLibraryA( "d3dcompiler_43.dll" );
	if ( !hCompiler )
		return TFALSE;

	auto pCompile = reinterpret_cast<t_D3DCompile>( GetProcAddress( hCompiler, "D3DCompile" ) );
	if ( !pCompile )
	{
		FreeLibrary( hCompiler );
		return TFALSE;
	}

	static const char s_szFullscreenShader[] = R"(
struct V { float4 p : SV_Position; float2 uv : TEXCOORD0; };
V VSMain(uint id : SV_VertexID)
{
	V o;
	float2 uv = float2((id == 2) ? 2.0 : 0.0, (id == 1) ? 2.0 : 0.0);
	o.p = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, 0.0, 1.0);
	o.uv = uv;
	return o;
}
Texture2D sourceTexture : register(t0);
SamplerState sourceSampler : register(s0);

float3 SrgbToLinear(float3 c)
{
	c = saturate(c);
	float3 low = c / 12.92;
	float3 high = pow((c + 0.055) / 1.055, 2.4);
	return float3(c.r <= 0.04045 ? low.r : high.r,
	              c.g <= 0.04045 ? low.g : high.g,
	              c.b <= 0.04045 ? low.b : high.b);
}

float4 PSCopy(V i) : SV_Target
{
	return sourceTexture.Sample(sourceSampler, i.uv);
}

float4 PSResolve(V i) : SV_Target
{
	float4 c = sourceTexture.Sample(sourceSampler, i.uv);
	return float4(SrgbToLinear(c.rgb), c.a);
}

float4 PSResolvePremultiplied(V i) : SV_Target
{
	float4 c = sourceTexture.Sample(sourceSampler, i.uv);
	float alpha = saturate(c.a);
	float3 straight = alpha > 0.000001 ? c.rgb / alpha : float3(0.0, 0.0, 0.0);
	return float4(SrgbToLinear(straight) * alpha, alpha);
}

)";

	ID3DBlob* pVS = TNULL;
	HRESULT hRes = pCompile( s_szFullscreenShader, sizeof( s_szFullscreenShader ) - 1, "AVRFullscreen", TNULL, TNULL, "VSMain", "vs_4_0", 0, 0, &pVS, TNULL );
	if ( SUCCEEDED( hRes ) )
		hRes = a_rImpl.pD3D11Device->CreateVertexShader( pVS->GetBufferPointer(), pVS->GetBufferSize(), TNULL, &a_rImpl.pMirrorVertexShader );
	if ( pVS )
	{
		pVS->Release();
		pVS = TNULL;
	}

	auto fnCreatePixelShader = [ & ]( const char* a_szEntryPoint, ID3D11PixelShader** a_ppShader ) {
		ID3DBlob* pPixelShaderBlob = TNULL;
		HRESULT hPixelShader = pCompile( s_szFullscreenShader, sizeof( s_szFullscreenShader ) - 1, "AVRFullscreen", TNULL, TNULL, a_szEntryPoint, "ps_4_0", 0, 0, &pPixelShaderBlob, TNULL );
		if ( SUCCEEDED( hPixelShader ) )
			hPixelShader = a_rImpl.pD3D11Device->CreatePixelShader( pPixelShaderBlob->GetBufferPointer(), pPixelShaderBlob->GetBufferSize(), TNULL, a_ppShader );
		if ( pPixelShaderBlob )
			pPixelShaderBlob->Release();
		return hPixelShader;
	};

	if ( SUCCEEDED( hRes ) ) hRes = fnCreatePixelShader( "PSCopy", &a_rImpl.pMirrorPixelShader );
	if ( SUCCEEDED( hRes ) ) hRes = fnCreatePixelShader( "PSResolve", &a_rImpl.pColorResolvePixelShader );
	if ( SUCCEEDED( hRes ) ) hRes = fnCreatePixelShader( "PSResolvePremultiplied", &a_rImpl.pColorResolveUIPixelShader );
	FreeLibrary( hCompiler );
	if ( FAILED( hRes ) )
	{
		DestroyMirrorPipeline( a_rImpl );
		return TFALSE;
	}

	D3D11_SAMPLER_DESC oSampler = {};
	oSampler.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	oSampler.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;
	oSampler.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;
	oSampler.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;
	oSampler.MaxLOD         = D3D11_FLOAT32_MAX;
	if ( FAILED( a_rImpl.pD3D11Device->CreateSamplerState( &oSampler, &a_rImpl.pMirrorSampler ) ) )
	{
		DestroyMirrorPipeline( a_rImpl );
		return TFALSE;
	}

	D3D11_RASTERIZER_DESC oRasterizer = {};
	oRasterizer.FillMode              = D3D11_FILL_SOLID;
	oRasterizer.CullMode              = D3D11_CULL_NONE;
	oRasterizer.DepthClipEnable       = TRUE;
	if ( FAILED( a_rImpl.pD3D11Device->CreateRasterizerState( &oRasterizer, &a_rImpl.pMirrorRasterizer ) ) )
	{
		DestroyMirrorPipeline( a_rImpl );
		return TFALSE;
	}

	D3D11_BLEND_DESC oBlend = {};
	oBlend.RenderTarget[ 0 ].BlendEnable           = TRUE;
	oBlend.RenderTarget[ 0 ].SrcBlend              = D3D11_BLEND_ONE;
	oBlend.RenderTarget[ 0 ].DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
	oBlend.RenderTarget[ 0 ].BlendOp               = D3D11_BLEND_OP_ADD;
	oBlend.RenderTarget[ 0 ].SrcBlendAlpha         = D3D11_BLEND_ONE;
	oBlend.RenderTarget[ 0 ].DestBlendAlpha        = D3D11_BLEND_INV_SRC_ALPHA;
	oBlend.RenderTarget[ 0 ].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
	oBlend.RenderTarget[ 0 ].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	if ( FAILED( a_rImpl.pD3D11Device->CreateBlendState( &oBlend, &a_rImpl.pMirrorAlphaBlend ) ) )
	{
		DestroyMirrorPipeline( a_rImpl );
		return TFALSE;
	}

	a_rImpl.bColorCorrectionAvailable = TTRUE;
	return TTRUE;
}

TBOOL AVRManager::Prepare( Toshi::TRenderInterface* a_pRenderer )
{
	Impl& rImpl = *m_pImpl;
	if ( rImpl.bPrepared )
		return TTRUE;

	rImpl.pRenderer = a_pRenderer;
	const char* szCommandLine = GetCommandLineA();
	if ( IsCommandLineOption( szCommandLine, "-novr" ) )
		return TFALSE;

	rImpl.fVRScale = ParseVRScale( szCommandLine );

	uint32_t uiExtensionCount = 0;
	if ( XR_FAILED( xrEnumerateInstanceExtensionProperties( TNULL, 0, &uiExtensionCount, TNULL ) ) || uiExtensionCount == 0 )
		return TFALSE;

	std::vector<XrExtensionProperties> aExtensions( uiExtensionCount );
	for ( auto& rExtension : aExtensions )
	{
		rExtension = {};
		rExtension.type = XR_TYPE_EXTENSION_PROPERTIES;
	}
	if ( XR_FAILED( xrEnumerateInstanceExtensionProperties( TNULL, uiExtensionCount, &uiExtensionCount, aExtensions.data() ) ) )
		return TFALSE;

	const char* szGraphicsExtension = XR_KHR_D3D11_ENABLE_EXTENSION_NAME;
	if ( !IsExtensionAvailable( aExtensions, szGraphicsExtension ) )
		return TFALSE;

	XrInstanceCreateInfo oInstanceInfo = { XR_TYPE_INSTANCE_CREATE_INFO };
	std::strncpy( oInstanceInfo.applicationInfo.applicationName, "Barnyard", XR_MAX_APPLICATION_NAME_SIZE - 1 );
	oInstanceInfo.applicationInfo.applicationVersion = 1;
	std::strncpy( oInstanceInfo.applicationInfo.engineName, "Toshi", XR_MAX_ENGINE_NAME_SIZE - 1 );
	oInstanceInfo.applicationInfo.engineVersion = 1;
	oInstanceInfo.applicationInfo.apiVersion = XR_API_VERSION_1_0;
	oInstanceInfo.enabledExtensionCount = 1;
	oInstanceInfo.enabledExtensionNames = &szGraphicsExtension;
	if ( XR_FAILED( xrCreateInstance( &oInstanceInfo, &rImpl.hInstance ) ) )
		return TFALSE;

	XrSystemGetInfo oSystemInfo = { XR_TYPE_SYSTEM_GET_INFO };
	oSystemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
	if ( XR_FAILED( xrGetSystem( rImpl.hInstance, &oSystemInfo, &rImpl.hSystem ) ) )
	{
		xrDestroyInstance( rImpl.hInstance );
		rImpl.hInstance = XR_NULL_HANDLE;
		return TFALSE;
	}

	PFN_xrGetD3D11GraphicsRequirementsKHR pGetRequirements = TNULL;
	if ( !GetOpenXRProc( rImpl.hInstance, "xrGetD3D11GraphicsRequirementsKHR", pGetRequirements ) ||
	     XR_FAILED( pGetRequirements( rImpl.hInstance, rImpl.hSystem, &rImpl.oD3D11Requirements ) ) )
	{
		xrDestroyInstance( rImpl.hInstance );
		rImpl.hInstance = XR_NULL_HANDLE;
		return TFALSE;
	}
	if ( !IsDXGIAdapterAvailable( rImpl.oD3D11Requirements.adapterLuid ) )
	{
		TWARN( "OpenXR: the HMD adapter is unavailable; continuing with flatscreen rendering.\n" );
		Shutdown();
		return TFALSE;
	}
	static_cast<Toshi::TRenderD3DInterface*>( a_pRenderer )->SetPreferredAdapterLuid( rImpl.oD3D11Requirements.adapterLuid, rImpl.oD3D11Requirements.minFeatureLevel );

	if ( !CreateInputActions( rImpl ) )
		TWARN( "OpenXR: controller actions could not be created; continuing with headset rendering only.\n" );

	rImpl.bPrepared = TTRUE;
	return TTRUE;
}

static void DestroySessionTargets( AVRManager::Impl& a_rImpl )
{
	if ( a_rImpl.bEyeActive )
	{
		Toshi::TRenderD3DInterface::Interface()->EndExternalTarget();
		a_rImpl.bEyeActive = TFALSE;
	}
	if ( a_rImpl.bUILayerActive )
	{
		Toshi::TRenderD3DInterface::Interface()->EndExternalTarget();
		a_rImpl.bUILayerActive = TFALSE;
	}

	ReleaseTargetImage( a_rImpl, a_rImpl.oUI );
	for ( AVRManager::Impl::Target& rEye : a_rImpl.aEyes )
		ReleaseTargetImage( a_rImpl, rEye );
	DestroyTarget( a_rImpl, a_rImpl.oUI );
	for ( AVRManager::Impl::Target& rEye : a_rImpl.aEyes )
		DestroyTarget( a_rImpl, rEye );

	if ( a_rImpl.hHeadSpace != XR_NULL_HANDLE )
	{
		xrDestroySpace( a_rImpl.hHeadSpace );
		a_rImpl.hHeadSpace = XR_NULL_HANDLE;
	}
	if ( a_rImpl.hLocalSpace != XR_NULL_HANDLE )
	{
		xrDestroySpace( a_rImpl.hLocalSpace );
		a_rImpl.hLocalSpace = XR_NULL_HANDLE;
	}
	if ( a_rImpl.hSession != XR_NULL_HANDLE )
	{
		// xrEndSession is only valid after the runtime reports STOPPING. Teardown
		// can also happen during device loss or app shutdown, so destroy the
		// session directly rather than issuing an invalid state-dependent call.
		xrDestroySession( a_rImpl.hSession );
		a_rImpl.hSession = XR_NULL_HANDLE;
	}
	a_rImpl.bGraphicsReady = TFALSE;
	a_rImpl.bSessionRunning = TFALSE;
	a_rImpl.bFrameBegun = TFALSE;
	a_rImpl.bShouldRender = TFALSE;
	a_rImpl.bProjectionReady = TFALSE;
	a_rImpl.bUILayerReady = TFALSE;
	a_rImpl.bHasNeutralPose = TFALSE;
	a_rImpl.bInputActionsAttached = TFALSE;
	a_rImpl.bSessionFocused = TFALSE;
	a_rImpl.bLoggedInputSyncFailure = TFALSE;
	ClearControllerInputState( a_rImpl );
}

static TBOOL CreateSessionAndTargets( AVRManager::Impl& a_rImpl )
{
	if ( !a_rImpl.bPrepared || a_rImpl.hSession != XR_NULL_HANDLE )
		return TFALSE;

	XrSessionCreateInfo oSessionInfo = { XR_TYPE_SESSION_CREATE_INFO };
	oSessionInfo.systemId = a_rImpl.hSystem;
	XrGraphicsBindingD3D11KHR oBinding = { XR_TYPE_GRAPHICS_BINDING_D3D11_KHR };
	oBinding.device = static_cast<Toshi::TRenderD3DInterface*>( a_rImpl.pRenderer )->GetDevice();
	oSessionInfo.next = &oBinding;

	if ( XR_FAILED( xrCreateSession( a_rImpl.hInstance, &oSessionInfo, &a_rImpl.hSession ) ) )
		return TFALSE;

	AttachInputActions( a_rImpl );

	XrReferenceSpaceCreateInfo oSpaceInfo = { XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
	oSpaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
	oSpaceInfo.poseInReferenceSpace.orientation.w = 1.0f;
	if ( XR_FAILED( xrCreateReferenceSpace( a_rImpl.hSession, &oSpaceInfo, &a_rImpl.hLocalSpace ) ) )
	{
		DestroySessionTargets( a_rImpl );
		return TFALSE;
	}

	oSpaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
	if ( XR_FAILED( xrCreateReferenceSpace( a_rImpl.hSession, &oSpaceInfo, &a_rImpl.hHeadSpace ) ) )
	{
		DestroySessionTargets( a_rImpl );
		return TFALSE;
	}

	uint32_t uiViewCount = 0;
	if ( XR_FAILED( xrEnumerateViewConfigurationViews( a_rImpl.hInstance, a_rImpl.hSystem, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &uiViewCount, TNULL ) ) || uiViewCount != kEyeCount )
	{
		DestroySessionTargets( a_rImpl );
		return TFALSE;
	}
	for ( auto& rViewConfig : a_rImpl.aConfigurationViews )
	{
		rViewConfig = {};
		rViewConfig.type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
	}
	if ( XR_FAILED( xrEnumerateViewConfigurationViews( a_rImpl.hInstance, a_rImpl.hSystem, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, uiViewCount, &uiViewCount, a_rImpl.aConfigurationViews.data() ) ) )
	{
		DestroySessionTargets( a_rImpl );
		return TFALSE;
	}

	uint32_t uiBlendModeCount = 0;
	if ( XR_FAILED( xrEnumerateEnvironmentBlendModes( a_rImpl.hInstance, a_rImpl.hSystem, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &uiBlendModeCount, TNULL ) ) || uiBlendModeCount == 0 )
	{
		DestroySessionTargets( a_rImpl );
		return TFALSE;
	}
	std::vector<XrEnvironmentBlendMode> aBlendModes( uiBlendModeCount );
	if ( XR_FAILED( xrEnumerateEnvironmentBlendModes( a_rImpl.hInstance, a_rImpl.hSystem, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, uiBlendModeCount, &uiBlendModeCount, aBlendModes.data() ) ) )
	{
		DestroySessionTargets( a_rImpl );
		return TFALSE;
	}
	a_rImpl.eBlendMode = aBlendModes.front();
	for ( XrEnvironmentBlendMode eMode : aBlendModes )
	{
		if ( eMode == XR_ENVIRONMENT_BLEND_MODE_OPAQUE )
			 a_rImpl.eBlendMode = eMode;
	}

	uint32_t uiFormatCount = 0;
	if ( XR_FAILED( xrEnumerateSwapchainFormats( a_rImpl.hSession, 0, &uiFormatCount, TNULL ) ) || uiFormatCount == 0 )
	{
		DestroySessionTargets( a_rImpl );
		return TFALSE;
	}
	std::vector<int64_t> aFormats( uiFormatCount );
	if ( XR_FAILED( xrEnumerateSwapchainFormats( a_rImpl.hSession, uiFormatCount, &uiFormatCount, aFormats.data() ) ) )
	{
		DestroySessionTargets( a_rImpl );
		return TFALSE;
	}
	const DXGI_FORMAT eColorFormat = ChooseDXGIFormat( aFormats );
	if ( eColorFormat == DXGI_FORMAT_UNKNOWN )
	{
		DestroySessionTargets( a_rImpl );
		return TFALSE;
	}
	a_rImpl.i64SwapchainFormat = int64_t( eColorFormat );

	if ( !CreateMirrorPipeline( a_rImpl ) )
		TWARN( "OpenXR: the linear color/mirror pipeline could not be created; using the legacy direct render path.\n" );

	for ( TUINT32 i = 0; i < kEyeCount; i++ )
	{
		const XrViewConfigurationView& rViewConfig = a_rImpl.aConfigurationViews[ i ];
		TUINT32 uiWidth  = TUINT32( std::lround( rViewConfig.recommendedImageRectWidth * a_rImpl.fVRScale ) );
		TUINT32 uiHeight = TUINT32( std::lround( rViewConfig.recommendedImageRectHeight * a_rImpl.fVRScale ) );
		uiWidth  = std::min( std::max( uiWidth, 1u ), std::max( rViewConfig.maxImageRectWidth, 1u ) );
		uiHeight = std::min( std::max( uiHeight, 1u ), std::max( rViewConfig.maxImageRectHeight, 1u ) );
		if ( !CreateTarget( a_rImpl, a_rImpl.aEyes[ i ], uiWidth, uiHeight ) )
		{
			DestroySessionTargets( a_rImpl );
			return TFALSE;
		}
	}

	TUINT32 uiUIWidth  = 1024;
	TUINT32 uiUIHeight = 768;
	if ( a_rImpl.pRenderer && a_rImpl.pRenderer->GetCurrentDisplayParams() )
	{
		uiUIWidth  = std::max( a_rImpl.pRenderer->GetCurrentDisplayParams()->uiWidth, 1u );
		uiUIHeight = std::max( a_rImpl.pRenderer->GetCurrentDisplayParams()->uiHeight, 1u );
	}
	if ( !CreateTarget( a_rImpl, a_rImpl.oUI, uiUIWidth, uiUIHeight ) )
	{
		DestroySessionTargets( a_rImpl );
		return TFALSE;
	}

	a_rImpl.fQuadHeightMeters = kUiQuadWidthMeters * TFLOAT( uiUIHeight ) / TFLOAT( uiUIWidth );
	a_rImpl.oQuadExtent = { kUiQuadWidthMeters, a_rImpl.fQuadHeightMeters };
	a_rImpl.oQuadLayer.type = XR_TYPE_COMPOSITION_LAYER_QUAD;
	a_rImpl.oQuadLayer.space = a_rImpl.hLocalSpace;
	a_rImpl.oQuadLayer.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
	a_rImpl.oQuadLayer.size = a_rImpl.oQuadExtent;
	a_rImpl.oQuadLayer.pose.orientation.w = 1.0f;

	a_rImpl.oProjectionLayer.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
	a_rImpl.oProjectionLayer.space = a_rImpl.hLocalSpace;
	a_rImpl.oProjectionLayer.viewCount = kEyeCount;
	a_rImpl.oProjectionLayer.views = a_rImpl.aProjectionViews.data();
	a_rImpl.bGraphicsReady = TTRUE;
	return TTRUE;
}

TBOOL AVRManager::Initialize( Toshi::TRenderInterface* a_pRenderer )
{
	Impl& rImpl = *m_pImpl;
	if ( !rImpl.bPrepared || rImpl.bGraphicsReady )
		return rImpl.bGraphicsReady;
	rImpl.pRenderer = a_pRenderer;

	auto* pRenderer = static_cast<Toshi::TRenderD3DInterface*>( a_pRenderer );
	rImpl.pD3D11Device  = pRenderer->GetDevice();
	rImpl.pD3D11Context = pRenderer->GetDeviceContext();

	if ( !CreateSessionAndTargets( rImpl ) )
	{
		TWARN( "OpenXR: session or swapchain creation failed; using flatscreen rendering.\n" );
		DestroySessionTargets( rImpl );
		return TFALSE;
	}
	return TTRUE;
}

static void SetVSyncAndKeepRunning( AVRManager::Impl& a_rImpl, TBOOL a_bEnabled )
{
	if ( !a_rImpl.pRenderer )
		return;
	static_cast<Toshi::TRenderD3DInterface*>( a_rImpl.pRenderer )->SetVSyncEnabled( !a_bEnabled );
#ifdef BARNYARD_COMMUNITY_PATCH
	Toshi::TMSWindow::Dev_SetKeepRunning( a_bEnabled );
#endif
}

static void MarkRuntimeLost( AVRManager::Impl& a_rImpl )
{
	a_rImpl.bRuntimeLost = TTRUE;
	a_rImpl.bSessionRunning = TFALSE;
	a_rImpl.bSessionFocused = TFALSE;
	ClearControllerInputState( a_rImpl );
	SetVSyncAndKeepRunning( a_rImpl, TFALSE );
}

static void PollEvents( AVRManager::Impl& a_rImpl )
{
	XrEventDataBuffer oEvent = { XR_TYPE_EVENT_DATA_BUFFER };
	for ( ;; )
	{
		const XrResult ePollResult = xrPollEvent( a_rImpl.hInstance, &oEvent );
		if ( ePollResult == XR_EVENT_UNAVAILABLE )
			break;
		if ( XR_FAILED( ePollResult ) )
		{
			MarkRuntimeLost( a_rImpl );
			break;
		}

		switch ( oEvent.type )
		{
			case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
			{
				const auto* pStateEvent = reinterpret_cast<const XrEventDataSessionStateChanged*>( &oEvent );
				// Session state events can remain queued while a device reset tears
				// down and recreates the session on the same OpenXR instance.
				if ( pStateEvent->session != a_rImpl.hSession )
					break;

				a_rImpl.bSessionFocused = pStateEvent->state == XR_SESSION_STATE_FOCUSED;
				if ( !a_rImpl.bSessionFocused )
					ClearControllerInputState( a_rImpl );

				switch ( pStateEvent->state )
				{
					case XR_SESSION_STATE_READY:
					{
						if ( a_rImpl.bRuntimeLost || a_rImpl.bExitRequested )
							break;

						XrSessionBeginInfo oBeginInfo = { XR_TYPE_SESSION_BEGIN_INFO };
						oBeginInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
						if ( XR_SUCCEEDED( xrBeginSession( a_rImpl.hSession, &oBeginInfo ) ) )
						{
							a_rImpl.bSessionRunning = TTRUE;
							SetVSyncAndKeepRunning( a_rImpl, TTRUE );
						}
						else
						{
							MarkRuntimeLost( a_rImpl );
						}
						break;
					}
					case XR_SESSION_STATE_STOPPING:
					{
						// xrEndSession is valid only in STOPPING, and the event remains
						// authoritative even if a preceding frame call failed.
						const XrResult eEndResult = xrEndSession( a_rImpl.hSession );
						a_rImpl.bSessionRunning = TFALSE;
						if ( XR_FAILED( eEndResult ) )
							MarkRuntimeLost( a_rImpl );
						else
							SetVSyncAndKeepRunning( a_rImpl, TFALSE );
						break;
					}
					case XR_SESSION_STATE_EXITING:
						a_rImpl.bExitRequested = TTRUE;
						a_rImpl.bSessionRunning = TFALSE;
						SetVSyncAndKeepRunning( a_rImpl, TFALSE );
						break;
					case XR_SESSION_STATE_LOSS_PENDING:
						MarkRuntimeLost( a_rImpl );
						break;
					default:
						break;
				}
				break;
			}
			case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
				MarkRuntimeLost( a_rImpl );
				break;
			case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
			{
				const auto* pSpaceEvent = reinterpret_cast<const XrEventDataReferenceSpaceChangePending*>( &oEvent );
				if ( pSpaceEvent->session == a_rImpl.hSession )
					a_rImpl.bRecenterPending = TTRUE;
				break;
			}
			default:
				break;
		}
		oEvent = { XR_TYPE_EVENT_DATA_BUFFER };
	}
}

TBOOL AVRManager::IsActive() const
{
	return m_pImpl->bGraphicsReady && m_pImpl->bSessionRunning && !m_pImpl->bRuntimeLost;
}

TBOOL AVRManager::GetHeadWorldMatrix( const Toshi::TMatrix44& a_rcRigCamera, Toshi::TMatrix44& a_rOutHead ) const
{
	const Impl& rImpl = *m_pImpl;
	if ( !IsActive() || !rImpl.bHasNeutralPose || rImpl.bRecenterPending ||
	     rImpl.hHeadSpace == XR_NULL_HANDLE || rImpl.oFrameState.predictedDisplayTime <= 0 )
		return TFALSE;

	// Simulation normally runs before BeginFrame. Locate the center head for the
	// next predicted display time rather than using a cached left/right eye pose.
	const XrTime time = rImpl.oFrameState.predictedDisplayTime +
	    ( rImpl.bFrameBegun ? 0 : rImpl.oFrameState.predictedDisplayPeriod );
	XrSpaceLocation location = { XR_TYPE_SPACE_LOCATION };
	if ( XR_FAILED( xrLocateSpace( rImpl.hHeadSpace, rImpl.hLocalSpace, time, &location ) ) ||
	     !( location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT ) ||
	     !( location.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT ) )
		return TFALSE;

	const auto& p = location.pose.position;
	const auto& q = location.pose.orientation;
	if ( !std::isfinite( p.x ) || !std::isfinite( p.y ) || !std::isfinite( p.z ) ||
	     !std::isfinite( q.x ) || !std::isfinite( q.y ) || !std::isfinite( q.z ) || !std::isfinite( q.w ) ||
	     q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w < 1.0e-6f )
		return TFALSE;

	a_rOutHead = MakeEyeWorldMatrix( a_rcRigCamera, location.pose, rImpl.vNeutralPosition, rImpl.qReferenceYaw );
	return TTRUE;
}

TBOOL AVRManager::HasControllerInput() const
{
	return m_pImpl->bInputActionsReady && m_pImpl->bInputActionsAttached;
}

TBOOL AVRManager::GetControllerInputState( ControllerInputState& a_rState ) const
{
	a_rState = {};
	if ( !HasControllerInput() )
		return TFALSE;

	a_rState = m_pImpl->oControllerInputState;
	return TTRUE;
}

TBOOL AVRManager::ShouldExit() const
{
	return m_pImpl->bExitRequested;
}

TBOOL AVRManager::BeginFrame()
{
	Impl& rImpl = *m_pImpl;
	if ( !rImpl.bGraphicsReady || rImpl.hInstance == XR_NULL_HANDLE || rImpl.hSession == XR_NULL_HANDLE || rImpl.bFrameBegun )
		return TFALSE;

	PollEvents( rImpl );
	if ( rImpl.bExitRequested || rImpl.bRuntimeLost || !rImpl.bSessionRunning )
		return TFALSE;

	XrFrameWaitInfo oWaitInfo = { XR_TYPE_FRAME_WAIT_INFO };
	rImpl.oFrameState = { XR_TYPE_FRAME_STATE };
	if ( XR_FAILED( xrWaitFrame( rImpl.hSession, &oWaitInfo, &rImpl.oFrameState ) ) )
	{
		MarkRuntimeLost( rImpl );
		return TFALSE;
	}

	XrFrameBeginInfo oBeginInfo = { XR_TYPE_FRAME_BEGIN_INFO };
	if ( XR_FAILED( xrBeginFrame( rImpl.hSession, &oBeginInfo ) ) )
	{
		MarkRuntimeLost( rImpl );
		return TFALSE;
	}
	rImpl.bFrameBegun = TTRUE;
	rImpl.bShouldRender = rImpl.oFrameState.shouldRender;
	rImpl.bViewsValid = TFALSE;
	rImpl.bProjectionReady = TFALSE;
	rImpl.bWorldRendered = TFALSE;
	rImpl.bUILayerReady = TFALSE;
	rImpl.bUILayerActive = TFALSE;
	rImpl.bEyeActive = TFALSE;
	rImpl.uiCompletedEyeMask = 0;
	rImpl.uiActiveEye = std::numeric_limits<TUINT32>::max();
	rImpl.bHasMirrorFrame = TFALSE;
	SyncControllerInput( rImpl );

	if ( !rImpl.bShouldRender )
		return TTRUE;

	for ( auto& rView : rImpl.aLocatedViews )
	{
		rView = {};
		rView.type = XR_TYPE_VIEW;
	}
	XrViewLocateInfo oLocateInfo = { XR_TYPE_VIEW_LOCATE_INFO };
	oLocateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
	oLocateInfo.displayTime = rImpl.oFrameState.predictedDisplayTime;
	oLocateInfo.space = rImpl.hLocalSpace;
	rImpl.oViewState = { XR_TYPE_VIEW_STATE };
	uint32_t uiLocatedCount = 0;
	if ( XR_FAILED( xrLocateViews( rImpl.hSession, &oLocateInfo, &rImpl.oViewState, kEyeCount, &uiLocatedCount, rImpl.aLocatedViews.data() ) ) ||
	     uiLocatedCount != kEyeCount ||
	     !( rImpl.oViewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT ) ||
	     !( rImpl.oViewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT ) )
		return TTRUE;

	for ( TUINT32 i = 0; i < kEyeCount; i++ )
	{
		rImpl.aLocatedViews[ i ].pose.orientation = NormaliseQuaternion( rImpl.aLocatedViews[ i ].pose.orientation );
		if ( !IsValidFov( rImpl.aLocatedViews[ i ].fov ) )
			return TTRUE;
	}
	rImpl.bViewsValid = TTRUE;

	if ( !rImpl.bHasNeutralPose || rImpl.bRecenterPending )
	{
		rImpl.vNeutralPosition = {
		    0.5f * ( rImpl.aLocatedViews[ 0 ].pose.position.x + rImpl.aLocatedViews[ 1 ].pose.position.x ),
		    0.5f * ( rImpl.aLocatedViews[ 0 ].pose.position.y + rImpl.aLocatedViews[ 1 ].pose.position.y ),
		    0.5f * ( rImpl.aLocatedViews[ 0 ].pose.position.z + rImpl.aLocatedViews[ 1 ].pose.position.z ),
		};
		const XrQuaternionf qHead = NormaliseQuaternion( rImpl.aLocatedViews[ 0 ].pose.orientation );
		const XrVector3f vForward = RotateVector( qHead, { 0.0f, 0.0f, -1.0f } );
		const TFLOAT fYaw = std::atan2( -vForward.x, -vForward.z );
		rImpl.qReferenceYaw = { 0.0f, std::sin( fYaw * 0.5f ), 0.0f, std::cos( fYaw * 0.5f ) };
		rImpl.bHasNeutralPose = TTRUE;
		rImpl.bRecenterPending = TFALSE;
	}

	return TTRUE;
}

TBOOL AVRManager::ShouldRender() const
{
	return m_pImpl->bFrameBegun && m_pImpl->bShouldRender && m_pImpl->bViewsValid;
}

TUINT32 AVRManager::GetViewCount() const
{
	return kEyeCount;
}

TUINT32 AVRManager::GetCurrentEye() const
{
	return m_pImpl->bEyeActive ? m_pImpl->uiActiveEye : std::numeric_limits<TUINT32>::max();
}

static TBOOL DrawD3D11Fullscreen( AVRManager::Impl& a_rImpl, ID3D11RenderTargetView* a_pRenderTarget, const D3D11_VIEWPORT& a_rcViewport, ID3D11ShaderResourceView* a_pSource, ID3D11PixelShader* a_pPixelShader, TBOOL a_bAlphaBlend )
{
	if ( !a_rImpl.pMirrorVertexShader || !a_pPixelShader || !a_pSource || !a_pRenderTarget || a_rcViewport.Width <= 0.0f || a_rcViewport.Height <= 0.0f )
		return TFALSE;

	ID3D11ShaderResourceView* pNullSRV = TNULL;

	a_rImpl.pD3D11Context->OMSetRenderTargets( 1, &a_pRenderTarget, TNULL );
	a_rImpl.pD3D11Context->RSSetViewports( 1, &a_rcViewport );
	a_rImpl.pD3D11Context->RSSetState( a_rImpl.pMirrorRasterizer );
	a_rImpl.pD3D11Context->OMSetBlendState( a_bAlphaBlend ? a_rImpl.pMirrorAlphaBlend : TNULL, TNULL, 0xFFFFFFFF );
	a_rImpl.pD3D11Context->IASetInputLayout( TNULL );
	a_rImpl.pD3D11Context->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
	a_rImpl.pD3D11Context->VSSetShader( a_rImpl.pMirrorVertexShader, TNULL, 0 );
	a_rImpl.pD3D11Context->PSSetShader( a_pPixelShader, TNULL, 0 );
	a_rImpl.pD3D11Context->PSSetShaderResources( 0, 1, &a_pSource );
	a_rImpl.pD3D11Context->PSSetSamplers( 0, 1, &a_rImpl.pMirrorSampler );
	a_rImpl.pD3D11Context->Draw( 3, 0 );
	a_rImpl.pD3D11Context->PSSetShaderResources( 0, 1, &pNullSRV );

	// The raw D3D draw changed blend/rasterizer state as well as shader bindings.
	// ResetStateCache invalidates the renderer's state-object caches; invalidating
	// only the pipeline cache leaves stale alpha blending for the next eye.
	static_cast<Toshi::TRenderD3DInterface*>( a_rImpl.pRenderer )->ResetStateCache();
	return TTRUE;
}

static TBOOL ResolveTargetToOpenXR( AVRManager::Impl& a_rImpl, AVRManager::Impl::Target& a_rTarget, TUINT32 a_uiImageIndex, TBOOL a_bPremultipliedAlpha )
{
	if ( !a_rTarget.pColorShaderResourceView || a_uiImageIndex >= a_rTarget.aRenderTargetViews.size() || a_uiImageIndex >= a_rTarget.aShaderResourceViews.size() )
		return TFALSE;

	// The content has been written using the game's display-encoded color convention.
	// Decode it once so OpenXR blending/compositing receives linear-light values.
	D3D11_VIEWPORT oViewport = {};
	oViewport.Width    = TFLOAT( a_rTarget.uiWidth );
	oViewport.Height   = TFLOAT( a_rTarget.uiHeight );
	oViewport.MinDepth = 0.0f;
	oViewport.MaxDepth = 1.0f;
	const TBOOL bResolved = DrawD3D11Fullscreen(
	    a_rImpl,
	    a_rTarget.aRenderTargetViews[ a_uiImageIndex ],
	    oViewport,
	    a_rTarget.pColorShaderResourceView,
	    a_bPremultipliedAlpha ? a_rImpl.pColorResolveUIPixelShader : a_rImpl.pColorResolvePixelShader,
	    TFALSE );
	if ( !bResolved )
		return TFALSE;

	// The swapchain image must be unbound before it is released to OpenXR.
	auto* pRenderer = static_cast<Toshi::TRenderD3DInterface*>( a_rImpl.pRenderer );
	ID3D11RenderTargetView* pBackBuffer = pRenderer->GetBackBufferRTView();
	if ( pBackBuffer )
	{
		a_rImpl.pD3D11Context->OMSetRenderTargets( 1, &pBackBuffer, pRenderer->GetDepthTarget().pView );
		D3D11_VIEWPORT oViewport = {};
		oViewport.Width    = TFLOAT( pRenderer->GetBackBufferWidth() );
		oViewport.Height   = TFLOAT( pRenderer->GetBackBufferHeight() );
		oViewport.MinDepth = 0.0f;
		oViewport.MaxDepth = 1.0f;
		a_rImpl.pD3D11Context->RSSetViewports( 1, &oViewport );
		pRenderer->InvalidateStateCache();
	}
	return TTRUE;
}

static TBOOL DrawD3D11Mirror( AVRManager::Impl& a_rImpl, AVRManager::Impl::Target& a_rSource, TUINT32 a_uiImageIndex, TBOOL a_bAlphaBlend )
{
	if ( a_uiImageIndex >= a_rSource.aShaderResourceViews.size() )
		return TFALSE;

	auto* pRenderer = static_cast<Toshi::TRenderD3DInterface*>( a_rImpl.pRenderer );
	ID3D11RenderTargetView* pRenderTarget = pRenderer->GetBackBufferRTView();
	const TUINT32 uiMirrorWidth = pRenderer->GetBackBufferWidth();
	const TUINT32 uiMirrorHeight = pRenderer->GetBackBufferHeight();
	if ( !pRenderTarget || !uiMirrorWidth || !uiMirrorHeight )
		return TFALSE;

	ID3D11ShaderResourceView* pSRV = a_rSource.aShaderResourceViews[ a_uiImageIndex ];
	if ( a_rImpl.bColorCorrectionAvailable )
		pSRV = a_rSource.pColorShaderResourceView;
	if ( !pSRV || !a_rImpl.pMirrorPixelShader )
		return TFALSE;

	// The intermediate contains the game's original display-encoded pixels; copy
	// those directly to the desktop instead of applying the linear-to-sRGB resolve.
	if ( !a_bAlphaBlend )
	{
		const FLOAT aBlack[ 4 ] = { 0.0f, 0.0f, 0.0f, 1.0f };
		a_rImpl.pD3D11Context->ClearRenderTargetView( pRenderTarget, aBlack );
	}

	const TFLOAT fSourceAspect = TFLOAT( a_rSource.uiWidth ) / TFLOAT( a_rSource.uiHeight );
	const TFLOAT fTargetAspect = TFLOAT( uiMirrorWidth ) / TFLOAT( uiMirrorHeight );
	D3D11_VIEWPORT oViewport = {};
	if ( fSourceAspect > fTargetAspect )
	{
		oViewport.Width  = TFLOAT( uiMirrorWidth );
		oViewport.Height = TFLOAT( uiMirrorWidth ) / fSourceAspect;
		oViewport.TopLeftY = ( TFLOAT( uiMirrorHeight ) - oViewport.Height ) * 0.5f;
	}
	else
	{
		oViewport.Width  = TFLOAT( uiMirrorHeight ) * fSourceAspect;
		oViewport.Height = TFLOAT( uiMirrorHeight );
		oViewport.TopLeftX = ( TFLOAT( uiMirrorWidth ) - oViewport.Width ) * 0.5f;
	}
	oViewport.MinDepth = 0.0f;
	oViewport.MaxDepth = 1.0f;
	return DrawD3D11Fullscreen( a_rImpl, pRenderTarget, oViewport, pSRV, a_rImpl.pMirrorPixelShader, a_bAlphaBlend );
}


TBOOL AVRManager::BeginEye( TUINT32 a_uiEye, const Toshi::TMatrix44& a_rcCenterCamera, Toshi::TCameraObject* a_pCamera, Toshi::TViewport* a_pWorldViewport, Toshi::TViewport* a_pHALViewport1, Toshi::TViewport* a_pHALViewport2, TBOOL a_bWorldRendered )
{
	Impl& rImpl = *m_pImpl;
	if ( !ShouldRender() || rImpl.bEyeActive || a_uiEye >= kEyeCount || !a_pCamera || !a_pWorldViewport || !a_pWorldViewport->GetRenderContext() )
		return TFALSE;

	Impl::Target& rTarget = rImpl.aEyes[ a_uiEye ];
	if ( !AcquireTargetImage( rTarget ) )
		return TFALSE;

	rImpl.pActiveCamera = a_pCamera;
	rImpl.pActiveContext = a_pWorldViewport->GetRenderContext();
	rImpl.oSavedCameraMatrix = a_pCamera->GetTransformObject().GetMatrix();
	rImpl.oSavedProjection = rImpl.pActiveContext->GetProjectionParams();
	Toshi::TViewport* apViewports[] = { a_pWorldViewport, a_pHALViewport1, a_pHALViewport2 };
	for ( TUINT32 i = 0; i < TARRAYSIZE( apViewports ); i++ )
	{
		rImpl.aSavedViewports[ i ].pViewport = apViewports[ i ];
		if ( apViewports[ i ] )
		{
			rImpl.aSavedViewports[ i ].fWidth = apViewports[ i ]->GetWidth();
			rImpl.aSavedViewports[ i ].fHeight = apViewports[ i ]->GetHeight();
			apViewports[ i ]->SetWidth( TFLOAT( rTarget.uiWidth ) );
			apViewports[ i ]->SetHeight( TFLOAT( rTarget.uiHeight ) );
		}
	}

	ID3D11Texture2D* pRenderTexture = rImpl.bColorCorrectionAvailable ? rTarget.pColorRenderTexture : rTarget.aD3D11Images[ rTarget.uiImageIndex ].texture;
	ID3D11RenderTargetView* pRenderTargetView = rImpl.bColorCorrectionAvailable ? rTarget.pColorRenderTargetView : rTarget.aRenderTargetViews[ rTarget.uiImageIndex ];
	Toshi::TRenderD3DInterface::Interface()->BeginExternalTarget(
	    pRenderTexture,
	    pRenderTargetView,
	    rTarget.oRendererDepth,
	    rTarget.uiWidth,
	    rTarget.uiHeight,
	    TFALSE
	);

	rImpl.bEyeActive = TTRUE;
	rImpl.uiActiveEye = a_uiEye;
	rImpl.bWorldRendered = rImpl.bWorldRendered || a_bWorldRendered;
	a_pCamera->GetTransformObject().SetMatrix( MakeEyeWorldMatrix( a_rcCenterCamera, rImpl.aLocatedViews[ a_uiEye ].pose, rImpl.vNeutralPosition, rImpl.qReferenceYaw ) );

	Toshi::TRenderD3DInterface::Interface()->ClearCurrentTargetRGBA( 0.0f, 0.0f, 0.0f, 1.0f, TTRUE, 1.0f );

	XrCompositionLayerProjectionView& rProjectionView = rImpl.aProjectionViews[ a_uiEye ];
	rProjectionView = {};
	rProjectionView.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
	rProjectionView.pose = rImpl.aLocatedViews[ a_uiEye ].pose;
	rProjectionView.fov = rImpl.aLocatedViews[ a_uiEye ].fov;
	rProjectionView.subImage.swapchain = rTarget.hSwapchain;
	rProjectionView.subImage.imageArrayIndex = 0;
	rProjectionView.subImage.imageRect.offset = { 0, 0 };
	rProjectionView.subImage.imageRect.extent = { LONG( rTarget.uiWidth ), LONG( rTarget.uiHeight ) };
	rImpl.bProjectionReady = TTRUE;
	return TTRUE;
}

void AVRManager::ApplyEyeProjection( Toshi::TRenderContext* a_pContext )
{
	Impl& rImpl = *m_pImpl;
	if ( !rImpl.bEyeActive || !a_pContext || rImpl.uiActiveEye >= kEyeCount )
		return;

	const TUINT32 uiEye = rImpl.uiActiveEye;
	const XrFovf& rcFov = rImpl.aLocatedViews[ uiEye ].fov;
	const TFLOAT fTanLeft   = std::tan( rcFov.angleLeft );
	const TFLOAT fTanRight  = std::tan( rcFov.angleRight );
	const TFLOAT fTanDown   = std::tan( rcFov.angleDown );
	const TFLOAT fTanUp     = std::tan( rcFov.angleUp );
	const TFLOAT fWidth     = a_pContext->GetWidth();
	const TFLOAT fHeight    = a_pContext->GetHeight();
	const TFLOAT fNear      = std::min( rImpl.pActiveCamera->GetNear(), 0.06f );
	const TFLOAT fFar       = rImpl.pActiveCamera->GetFar();

	Toshi::TRenderContext::PROJECTIONPARAMS oProjection = a_pContext->GetProjectionParams();
	oProjection.m_Proj.x = fWidth / ( fTanRight - fTanLeft );
	oProjection.m_Proj.y = fHeight / ( fTanUp - fTanDown );
	oProjection.m_Centre.x = -fWidth * fTanLeft / ( fTanRight - fTanLeft );
	oProjection.m_Centre.y = fHeight * fTanUp / ( fTanUp - fTanDown );
	oProjection.m_fNearClip = fNear;
	oProjection.m_fFarClip = fFar;
	a_pContext->SetProjectionParams( oProjection );
	// SetProjectionParams only marks the render context dirty. DX11 shaders read the
	// cached projection matrix directly, so rebuild it before the first draw of this eye.
	a_pContext->Update();
}

static void RestoreEyeState( AVRManager::Impl& a_rImpl )
{
	for ( const AVRManager::Impl::ViewportSnapshot& rSnapshot : a_rImpl.aSavedViewports )
	{
		if ( rSnapshot.pViewport )
		{
			rSnapshot.pViewport->SetWidth( rSnapshot.fWidth );
			rSnapshot.pViewport->SetHeight( rSnapshot.fHeight );
		}
	}
	if ( a_rImpl.pActiveCamera )
		a_rImpl.pActiveCamera->GetTransformObject().SetMatrix( a_rImpl.oSavedCameraMatrix );
	if ( a_rImpl.pActiveContext )
		a_rImpl.pActiveContext->SetProjectionParams( a_rImpl.oSavedProjection );
	a_rImpl.pActiveCamera = TNULL;
	a_rImpl.pActiveContext = TNULL;
}

void AVRManager::EndEye()
{
	Impl& rImpl = *m_pImpl;
	if ( !rImpl.bEyeActive )
		return;

	const TUINT32 uiEye = rImpl.uiActiveEye < kEyeCount ? rImpl.uiActiveEye : 0;
	Impl::Target& rTarget = rImpl.aEyes[ uiEye ];
	TBOOL bImageReady = !rImpl.bDeviceLost;

	Toshi::TRenderD3DInterface::Interface()->EndExternalTarget();

	if ( bImageReady && rImpl.bColorCorrectionAvailable )
		bImageReady = ResolveTargetToOpenXR( rImpl, rTarget, rTarget.uiImageIndex, TFALSE );

	if ( bImageReady && uiEye == 0 )
		rImpl.bHasMirrorFrame = DrawD3D11Mirror( rImpl, rTarget, rTarget.uiImageIndex, TFALSE );

	ReleaseTargetImage( rImpl, rTarget );
	if ( bImageReady )
		rImpl.uiCompletedEyeMask |= 1u << uiEye;
	rImpl.bEyeActive = TFALSE;
	rImpl.uiActiveEye = std::numeric_limits<TUINT32>::max();
	RestoreEyeState( rImpl );
}

TBOOL AVRManager::BeginUILayer()
{
	Impl& rImpl = *m_pImpl;
	if ( !rImpl.bFrameBegun || !rImpl.bShouldRender || !rImpl.bViewsValid || rImpl.bUILayerActive || !AcquireTargetImage( rImpl.oUI ) )
		return TFALSE;

	ID3D11Texture2D* pRenderTexture = rImpl.bColorCorrectionAvailable ? rImpl.oUI.pColorRenderTexture : rImpl.oUI.aD3D11Images[ rImpl.oUI.uiImageIndex ].texture;
	ID3D11RenderTargetView* pRenderTargetView = rImpl.bColorCorrectionAvailable ? rImpl.oUI.pColorRenderTargetView : rImpl.oUI.aRenderTargetViews[ rImpl.oUI.uiImageIndex ];
	Toshi::TRenderD3DInterface::Interface()->BeginExternalTarget(
	    pRenderTexture,
	    pRenderTargetView,
	    rImpl.oUI.oRendererDepth,
	    rImpl.oUI.uiWidth,
	    rImpl.oUI.uiHeight,
	    TTRUE
	);

	rImpl.bUILayerActive = TTRUE;
	const TFLOAT fClearAlpha = rImpl.bWorldRendered ? 0.0f : 1.0f;
	Toshi::TRenderD3DInterface::Interface()->ClearCurrentTargetRGBA( 0.0f, 0.0f, 0.0f, fClearAlpha, TTRUE, 1.0f );
	return TTRUE;
}

void AVRManager::EndUILayer()
{
	Impl& rImpl = *m_pImpl;
	if ( !rImpl.bUILayerActive )
		return;

	Toshi::TRenderD3DInterface::Interface()->EndExternalTarget();

	TBOOL bUILayerReady = !rImpl.bDeviceLost;
	if ( bUILayerReady && rImpl.bColorCorrectionAvailable )
		bUILayerReady = ResolveTargetToOpenXR( rImpl, rImpl.oUI, rImpl.oUI.uiImageIndex, TTRUE );
	if ( bUILayerReady )
	{
		if ( rImpl.bHasMirrorFrame )
			(void)DrawD3D11Mirror( rImpl, rImpl.oUI, rImpl.oUI.uiImageIndex, TTRUE );
		else
			rImpl.bHasMirrorFrame = DrawD3D11Mirror( rImpl, rImpl.oUI, rImpl.oUI.uiImageIndex, TFALSE );
	}

	rImpl.oQuadSubImage.swapchain = rImpl.oUI.hSwapchain;
	rImpl.oQuadSubImage.imageArrayIndex = 0;
	rImpl.oQuadSubImage.imageRect.offset = { 0, 0 };
	rImpl.oQuadSubImage.imageRect.extent = { LONG( rImpl.oUI.uiWidth ), LONG( rImpl.oUI.uiHeight ) };
	rImpl.oQuadLayer.subImage = rImpl.oQuadSubImage;
	rImpl.oQuadLayer.space = rImpl.hLocalSpace;
	rImpl.oQuadLayer.size = rImpl.oQuadExtent;
	if ( rImpl.bHasNeutralPose )
	{
		const TFLOAT fDistance = rImpl.bWorldRendered ? kUiWorldDistance : kUiMenuDistance;
		rImpl.oQuadPose.position = {
		    rImpl.vNeutralPosition.x - fDistance * std::sin( 2.0f * std::atan2( rImpl.qReferenceYaw.y, rImpl.qReferenceYaw.w ) ),
		    rImpl.vNeutralPosition.y,
		    rImpl.vNeutralPosition.z - fDistance * std::cos( 2.0f * std::atan2( rImpl.qReferenceYaw.y, rImpl.qReferenceYaw.w ) ),
		};
		rImpl.oQuadPose.orientation = rImpl.qReferenceYaw;
	}
	else
	{
		rImpl.oQuadPose.orientation.w = 1.0f;
		rImpl.oQuadPose.position = { 0.0f, 0.0f, -kUiMenuDistance };
	}
	rImpl.oQuadLayer.pose = rImpl.oQuadPose;
	rImpl.oQuadLayer.layerFlags = rImpl.bWorldRendered ? XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT : 0;
	rImpl.bUILayerReady = bUILayerReady;
	rImpl.bUILayerActive = TFALSE;
	ReleaseTargetImage( rImpl, rImpl.oUI );
}

void AVRManager::EndFrame()
{
	Impl& rImpl = *m_pImpl;
	if ( !rImpl.bFrameBegun )
		return;
	if ( rImpl.bEyeActive )
		EndEye();
	if ( rImpl.bUILayerActive )
		EndUILayer();

	XrCompositionLayerBaseHeader* aLayers[ 2 ] = {};
	uint32_t uiLayerCount = 0;
	if ( !rImpl.bDeviceLost && rImpl.bShouldRender && rImpl.bViewsValid && rImpl.bProjectionReady && rImpl.uiCompletedEyeMask == ( ( 1u << kEyeCount ) - 1u ) )
	{
		rImpl.oProjectionLayer.viewCount = kEyeCount;
		rImpl.oProjectionLayer.views = rImpl.aProjectionViews.data();
		aLayers[ uiLayerCount++ ] = reinterpret_cast<XrCompositionLayerBaseHeader*>( &rImpl.oProjectionLayer );
	}
	if ( !rImpl.bDeviceLost && rImpl.bShouldRender && rImpl.bUILayerReady )
		aLayers[ uiLayerCount++ ] = reinterpret_cast<XrCompositionLayerBaseHeader*>( &rImpl.oQuadLayer );

	XrFrameEndInfo oEndInfo = { XR_TYPE_FRAME_END_INFO };
	oEndInfo.displayTime = rImpl.oFrameState.predictedDisplayTime;
	oEndInfo.environmentBlendMode = rImpl.eBlendMode;
	oEndInfo.layerCount = uiLayerCount;
	oEndInfo.layers = uiLayerCount ? aLayers : TNULL;
	const XrResult eEndResult = xrEndFrame( rImpl.hSession, &oEndInfo );
	rImpl.bFrameBegun = TFALSE;
	if ( XR_FAILED( eEndResult ) && !rImpl.bDeviceLost )
		MarkRuntimeLost( rImpl );

}

void AVRManager::OnDeviceLost()
{
	Impl& rImpl = *m_pImpl;
	if ( !rImpl.bPrepared )
		return;
	rImpl.bDeviceLost = TTRUE;
	if ( rImpl.bFrameBegun )
		EndFrame();
	DestroySessionTargets( rImpl );
	DestroyMirrorPipeline( rImpl );
	SetVSyncAndKeepRunning( rImpl, TFALSE );
}

void AVRManager::OnDeviceFound()
{
	Impl& rImpl = *m_pImpl;
	if ( !rImpl.bPrepared || rImpl.bRuntimeLost || rImpl.hSession != XR_NULL_HANDLE )
		return;

	auto* pRenderer = static_cast<Toshi::TRenderD3DInterface*>( rImpl.pRenderer );
	rImpl.pD3D11Device = pRenderer->GetDevice();
	rImpl.pD3D11Context = pRenderer->GetDeviceContext();
	if ( !CreateSessionAndTargets( rImpl ) )
	{
		TWARN( "OpenXR: session recovery after device reset failed; continuing in flatscreen mode.\n" );
	}
	else
	{
		rImpl.bDeviceLost = TFALSE;
	}
}

void AVRManager::OnDisplayChanged()
{
	Impl& rImpl = *m_pImpl;
	if ( !rImpl.bGraphicsReady || rImpl.hSession == XR_NULL_HANDLE || rImpl.bFrameBegun )
		return;

	TUINT32 uiWidth = 1024;
	TUINT32 uiHeight = 768;
	if ( rImpl.pRenderer && rImpl.pRenderer->GetCurrentDisplayParams() )
	{
		uiWidth = std::max( rImpl.pRenderer->GetCurrentDisplayParams()->uiWidth, 1u );
		uiHeight = std::max( rImpl.pRenderer->GetCurrentDisplayParams()->uiHeight, 1u );
	}

	Impl::Target oNewUI;
	if ( !CreateTarget( rImpl, oNewUI, uiWidth, uiHeight ) )
		return;
	DestroyTarget( rImpl, rImpl.oUI );
	rImpl.oUI = std::move( oNewUI );
	rImpl.fQuadHeightMeters = kUiQuadWidthMeters * TFLOAT( uiHeight ) / TFLOAT( uiWidth );
	rImpl.oQuadExtent = { kUiQuadWidthMeters, rImpl.fQuadHeightMeters };
	rImpl.oQuadLayer.size = rImpl.oQuadExtent;

}

void AVRManager::Shutdown()
{
	if ( !m_pImpl )
		return;
	Impl& rImpl = *m_pImpl;
	if ( rImpl.bFrameBegun )
		EndFrame();
	DestroySessionTargets( rImpl );
	SetVSyncAndKeepRunning( rImpl, TFALSE );

	DestroyMirrorPipeline( rImpl );
	DestroyInputActions( rImpl );

	if ( rImpl.hInstance != XR_NULL_HANDLE )
	{
		xrDestroyInstance( rImpl.hInstance );
		rImpl.hInstance = XR_NULL_HANDLE;
	}
	rImpl.hSystem = XR_NULL_SYSTEM_ID;
	rImpl.pRenderer = TNULL;
	rImpl.bPrepared = TFALSE;
	rImpl.bExitRequested = TFALSE;
	rImpl.bRuntimeLost = TFALSE;
	rImpl.bDeviceLost = TFALSE;
}

#else // non-Windows, WASM, or non-DX11 renderer

struct AVRManager::Impl
{
};

AVRManager::AVRManager()
    : m_pImpl( new Impl() )
{}

AVRManager::~AVRManager()
{
	delete m_pImpl;
}

AVRManager& AVRManager::GetSingleton()
{
	static AVRManager s_oInstance;
	return s_oInstance;
}

TBOOL AVRManager::GetHeadWorldMatrix( const Toshi::TMatrix44&, Toshi::TMatrix44& ) const { return TFALSE; }
TBOOL AVRManager::Prepare( Toshi::TRenderInterface* ) { return TFALSE; }
TBOOL AVRManager::Initialize( Toshi::TRenderInterface* ) { return TFALSE; }
void AVRManager::Shutdown() {}
TBOOL AVRManager::IsActive() const { return TFALSE; }
TBOOL AVRManager::HasControllerInput() const { return TFALSE; }
TBOOL AVRManager::GetControllerInputState( ControllerInputState& a_rState ) const { a_rState = {}; return TFALSE; }
TBOOL AVRManager::ShouldExit() const { return TFALSE; }
TBOOL AVRManager::BeginFrame() { return TFALSE; }
TBOOL AVRManager::ShouldRender() const { return TFALSE; }
TUINT32 AVRManager::GetViewCount() const { return 0; }
TUINT32 AVRManager::GetCurrentEye() const { return 0xFFFFFFFFu; }
TBOOL AVRManager::BeginEye( TUINT32, const Toshi::TMatrix44&, Toshi::TCameraObject*, Toshi::TViewport*, Toshi::TViewport*, Toshi::TViewport*, TBOOL ) { return TFALSE; }
void AVRManager::ApplyEyeProjection( Toshi::TRenderContext* ) {}
void AVRManager::EndEye() {}
TBOOL AVRManager::BeginUILayer() { return TFALSE; }
void AVRManager::EndUILayer() {}
void AVRManager::EndFrame() {}
void AVRManager::OnDeviceLost() {}
void AVRManager::OnDeviceFound() {}
void AVRManager::OnDisplayChanged() {}

#endif
