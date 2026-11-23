#include "pch.h"
#include "AFirstPersonCamera.h"

#include "ACamera.h"
#include "ACameraManager.h"
#include "AGTAStyleCameraHelper.h"
#include "ABikePhysicsCameraHelper.h"
#include "AOptions.h"
#include "Render/ARenderer.h"
#include "Render/AShadowManager.h"
#include "Animation/AAnimatableObject.h"
#include "Animation/AAnimatableObjectManager.h"
#include "Animation/AAnimatableObjectType.h"
#include "Animation/AModel.h"
#include "Units/AUnit.h"
#include "Units/AAnimatedUnit.h"
#include "Units/AUnitPlayer.h"
#include "Units/AUnitCarChaseCar.h"
#include "Units/AUnitProxy.h"
#include "Units/AModelSwapComponent.h"
#include "UnitMovement/Steering/ASteer.h"
#include "UnitMovement/Steering/ABikeSteer.h"
#include "UnitMovement/Steering/ARacingCarSteer.h"
#include "Physics/ACollisionObject.h"
#include "Physics/APhysicsBike.h"
#include "Physics/APhysicsCar.h"
#include "Physics/AVehicleManager.h"
#include "Terrain/ATerrainInterface.h"

#include <cmath>

#ifdef TOSHI_SKU_WINDOWS
#  include <windows.h>
#endif

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

TPSTRING8_DECLARE( base_bone );
TPSTRING8_DECLARE( body_loc );
TPSTRING8_DECLARE( c_head );
TPSTRING8_DECLARE( cb_rlocator );
TPSTRING8_DECLARE( sunglasses1 );

// Defined in AGTAStyleCameraHelper.cpp
TBOOL SweepSphere( const TVector4& a_rcStart, const TVector4& a_rcDirection, TFLOAT a_fRadius, TFLOAT a_fLength, TFLOAT* a_pOutFraction, TVector4* a_pOutNormal, TINT a_iFlags, TUINT a_uiCollisionMask );

namespace
{

enum VIEW_MODE
{
	VIEW_NONE,
	VIEW_ON_FOOT,
	VIEW_AIM,
	VIEW_BIKE,
	VIEW_CAR,
	VIEW_FLIGHT,
};

enum HIDE_MODE
{
	HIDE_NONE,
	HIDE_PLAYER,
	HIDE_RIDER,
	HIDE_CAR_DRIVER,
	HIDE_SIMPLE,
};

// Ceiling clearance check flags (ignores units, foliage, bushes and camera-ignored geometry)
constexpr TINT  OVERHANG_IGNORE_FLAGS = ACollisionObject::TYPE_IGNORED | ACollisionObject::TYPE_BUSH | ACollisionObject::TYPE_PLAYER | ACollisionObject::TYPE_FOLIAGE | ACollisionObject::TYPE_TRUNK | ACollisionObject::TYPE_UNIT;
constexpr TUINT OVERHANG_MASK         = 0x90000;

// Distance from the first-person eye within which the owning unit is hidden during transitions
constexpr TFLOAT HIDE_ENTER_DISTANCE = 0.65f;
constexpr TFLOAT HIDE_EXIT_DISTANCE  = 0.55f;

// Bike cockpit offset in vehicle space (+X right, +Y down, +Z forward)
constexpr TVector4 BIKE_EYE_OFFSET = TVector4( 0.0f, -1.68f, -0.20f, 1.0f );

// Fallback Joy Ride driver eye offsets in vehicle body space (+X right, +Y down, +Z forward)
constexpr TVector4 CAR_TRUCK_DEFAULT_EYE_OFFSET = TVector4( -0.42f, -1.56f, 0.05f, 1.0f );
constexpr TVector4 CAR_SEDAN_DEFAULT_EYE_OFFSET = TVector4( -0.42f, -1.44f, -0.05f, 1.0f );

// Vehicle freelook limits and auto-return timing
constexpr TFLOAT VEHICLE_MAX_YAW_DEG      = 115.0f;
constexpr TFLOAT VEHICLE_MIN_PITCH_DEG    = -55.0f;
constexpr TFLOAT VEHICLE_MAX_PITCH_DEG    = 45.0f;
constexpr TFLOAT VEHICLE_RECENTER_DELAY   = 1.25f;
constexpr TFLOAT VEHICLE_RECENTER_SPEED   = 3.5f;
constexpr TFLOAT VEHICLE_STICK_YAW_RATE   = 150.0f;
constexpr TFLOAT VEHICLE_STICK_PITCH_RATE = 110.0f;

// Aim camera sensitivity and turn rates
constexpr TFLOAT AIM_MOUSE_SCALE      = 0.13f;
constexpr TFLOAT AIM_STICK_YAW_RATE   = 135.0f;
constexpr TFLOAT AIM_STICK_PITCH_RATE = 105.0f;

// Active view state for SyncBeforeRender
VIEW_MODE                 s_eViewMode      = VIEW_NONE;
AGTAStyleCameraHelper*    s_pOnFootHelper  = TNULL;
ABikePhysicsCameraHelper* s_pVehicleHelper = TNULL;
TRef<AUnitProxy>          s_pActiveUnit;
TVector4                  s_vOnFootForward   = TVector4( 0.0f, 0.0f, 1.0f, 1.0f );
TFLOAT                    s_fOnFootEyeHeight = AFirstPersonCamera::DEFAULT_EYE_HEIGHT;
TBOOL                     s_bOnFootOverride  = TFALSE;
TVector4                  s_vOnFootOverride  = TVector4::VEC_ZERO;
TBOOL                     s_bCarLookBack     = TFALSE;
TVector4                  s_vFlightEyePos    = TVector4::VEC_ZERO;
TBOOL                     s_bFlightThisFrame = TFALSE;

// Cached rest-pose driver eye offset for the active Joy Ride car (in vehicle body space)
AUnitCarChaseCar* s_pCachedCar          = TNULL;
TBOOL             s_bCarEyeOffsetCached = TFALSE;
TVector4          s_vCarEyeOffset       = CAR_TRUCK_DEFAULT_EYE_OFFSET;

// Vehicle freelook state (degrees)
TFLOAT   s_fVehicleLookYaw      = 0.0f;
TFLOAT   s_fVehicleSnapYaw      = 0.0f;
TFLOAT   s_fVehicleLookPitch    = 0.0f;
TVector2 s_vVehicleStickLook    = TVector2::VEC_ZERO;
TVector2 s_vVehicleKeyLook      = TVector2::VEC_ZERO;
TFLOAT   s_fVehicleLookIdleTime = 10.0f;

// Aim camera state
TBOOL            s_bAimActive    = TFALSE;
TRef<AUnitProxy> s_pAimUnit;
TVector4         s_vAimFlatDir   = TVector4( 0.0f, 0.0f, 1.0f, 1.0f );
TFLOAT           s_fAimPitchDeg  = 0.0f;
TVector2         s_vAimMouseTurn = TVector2::VEC_ZERO; // degrees accumulated this frame
TVector2         s_vAimRateTurn  = TVector2::VEC_ZERO; // normalised stick/key input
TVector2         s_vAimDpadTurn  = TVector2::VEC_ZERO; // held D-pad look buttons
TVector4         s_vAimForward   = TVector4( 0.0f, 0.0f, 1.0f, 1.0f );

// Hidden unit tracking (restored automatically whenever the view leaves first-person)
TRef<AUnitProxy> s_pHiddenUnit;
HIDE_MODE        s_eHiddenMode    = HIDE_NONE;
TVector4         s_vLastHiddenEye = TVector4::VEC_ZERO;

// Saved scene near clip plane
TBOOL  s_bNearClipOverridden = TFALSE;
TFLOAT s_fSavedNearClip      = 0.5f;

AUnit* GetHelperTargetUnit( ACameraHelper* a_pHelper )
{
	if ( !a_pHelper )
		return TNULL;

	AUnitProxy* pProxy = a_pHelper->GetTarget();
	return pProxy ? pProxy->GetUnit() : TNULL;
}

void NormaliseFlatSafe( const TVector4& a_rcIn, TVector4& a_rOut )
{
	TFLOAT fMagSq = a_rcIn.x * a_rcIn.x + a_rcIn.z * a_rcIn.z;

	if ( fMagSq > 0.000001f )
	{
		TFLOAT fInv = 1.0f / sqrtf( fMagSq );
		a_rOut.Set( a_rcIn.x * fInv, 0.0f, a_rcIn.z * fInv, 1.0f );
	}
	else
	{
		a_rOut.Set( 0.0f, 0.0f, 1.0f, 1.0f );
	}
}

void SweepCeilingClearance( const TVector4& a_rcSafeStart, TVector4& a_rDesiredEye )
{
	if ( !ATerrainInterface::IsSingletonCreated() )
		return;

	TVector4 vDelta(
	    a_rDesiredEye.x - a_rcSafeStart.x,
	    a_rDesiredEye.y - a_rcSafeStart.y,
	    a_rDesiredEye.z - a_rcSafeStart.z,
	    1.0f
	);

	TFLOAT fLengthSq = vDelta.MagnitudeSq();

	if ( fLengthSq <= 0.0001f )
		return;

	TFLOAT fLength = sqrtf( fLengthSq );
	vDelta.Divide( fLength );

	TFLOAT   fFreeDist = fLength;
	TVector4 vNormal   = TVector4::VEC_POSY;

	if ( SweepSphere( a_rcSafeStart, vDelta, 0.14f, 0.0f, &fFreeDist, &vNormal, OVERHANG_IGNORE_FLAGS, OVERHANG_MASK ) )
	{
		TFLOAT fClamped = TMath::Max( 0.0f, fFreeDist - 0.05f );
		a_rDesiredEye.Set(
		    a_rcSafeStart.x + vDelta.x * fClamped,
		    a_rcSafeStart.y + vDelta.y * fClamped,
		    a_rcSafeStart.z + vDelta.z * fClamped,
		    1.0f
		);
	}
}

void ResetAnimatableOpacityRecursive( AAnimatableObject* a_pObject )
{
	if ( !a_pObject )
		return;

	if ( AModelInstanceRef pModelInstance = a_pObject->GetModelInstance() )
	{
		pModelInstance->DisableDrawWithDefaultMaterials();
		pModelInstance->SetOpacity( 1.0f );
		pModelInstance->ResetOpacity();
	}

	T2_FOREACH( a_pObject->GetAttachedObjects(), it )
	{
		ResetAnimatableOpacityRecursive( it.Get() );
	}
}

void HideAnimatableRecursive( AAnimatableObject* a_pObject, TFLOAT a_fOpacity )
{
	if ( !a_pObject )
		return;

	if ( AModelInstanceRef pModelInstance = a_pObject->GetModelInstance() )
	{
		pModelInstance->DisableDrawWithDefaultMaterials();
		pModelInstance->SetOpacity( a_fOpacity );
	}

	T2_FOREACH( a_pObject->GetAttachedObjects(), it )
	{
		HideAnimatableRecursive( it.Get(), a_fOpacity );
	}
}

void ApplyUnitHidden( AUnit* a_pUnit, HIDE_MODE a_eMode, const TVector4& a_rcEyePos )
{
	if ( !a_pUnit )
		return;

	// Restore the previously hidden unit if the target or hide mode switched
	AUnit* pPrevUnit = s_pHiddenUnit ? s_pHiddenUnit->GetUnit() : TNULL;

	if ( pPrevUnit && ( pPrevUnit != a_pUnit || s_eHiddenMode != a_eMode ) )
	{
		if ( s_eHiddenMode == HIDE_CAR_DRIVER )
		{
			if ( pPrevUnit->IsA( &TGetClass( AUnitCarChaseCar ) ) )
				ResetAnimatableOpacityRecursive( TSTATICCAST( AUnitCarChaseCar, pPrevUnit )->GetDriver() );
		}
		else if ( pPrevUnit->IsA( &TGetClass( AAnimatedUnit ) ) )
		{
			AAnimatedUnit* pAnimUnit = TSTATICCAST( AAnimatedUnit, pPrevUnit );
			ResetAnimatableOpacityRecursive( pAnimUnit->GetAnimatableObject() );

			if ( AModelSwapComponent* pSwap = TSTATICCAST( AModelSwapComponent, pAnimUnit->FindComponentExact( &TGetClass( AModelSwapComponent ) ) ) )
				ResetAnimatableOpacityRecursive( pSwap->GetSwappedModel() );
		}
	}

	s_pHiddenUnit    = a_pUnit->GetSelfProxy();
	s_eHiddenMode    = a_eMode;
	s_vLastHiddenEye = a_rcEyePos;

	if ( a_eMode == HIDE_CAR_DRIVER )
	{
		if ( a_pUnit->IsA( &TGetClass( AUnitCarChaseCar ) ) )
			HideAnimatableRecursive( TSTATICCAST( AUnitCarChaseCar, a_pUnit )->GetDriver(), 0.0f );

		return;
	}

	if ( !a_pUnit->IsA( &TGetClass( AAnimatedUnit ) ) )
		return;

	AAnimatedUnit*     pAnimUnit = TSTATICCAST( AAnimatedUnit, a_pUnit );
	AAnimatableObject* pAnimObj  = pAnimUnit->GetAnimatableObject();

	if ( !pAnimObj )
		return;

	AModelInstanceRef pModelInstance = pAnimObj->GetModelInstance();

	if ( !pModelInstance )
		return;

	// Hide the character's own mesh while keeping its skeleton updates and shadow intact
	pModelInstance->DisableDrawWithDefaultMaterials();
	pModelInstance->SetOpacity( 0.0f );

	// Also hide any active swapped model (e.g. landing/get-up model swap)
	if ( AModelSwapComponent* pSwap = TSTATICCAST( AModelSwapComponent, pAnimUnit->FindComponentExact( &TGetClass( AModelSwapComponent ) ) ) )
		HideAnimatableRecursive( pSwap->GetSwappedModel(), 0.0f );

	if ( a_eMode == HIDE_SIMPLE )
	{
		T2_FOREACH( pAnimObj->GetAttachedObjects(), it )
		{
			HideAnimatableRecursive( it.Get(), 0.0f );
		}
		return;
	}

	const TINT iBikeBone      = pModelInstance->GetBoneID( TPS8( body_loc ) );
	const TINT iRightHandBone = ( a_eMode == HIDE_PLAYER ) ? pModelInstance->GetBoneID( TPS8( cb_rlocator ) ) : -1;

	TBOOL    bHandBonePosValid = TFALSE;
	TVector4 vHandBonePos      = TVector4::VEC_ZERO;

	T2_FOREACH( pAnimObj->GetAttachedObjects(), it )
	{
		AAnimatableObject* pAttached   = it.Get();
		const TINT         iParentBone = pAttached->GetAttachmentInfo().iParentBone;

		// Keep the bike mesh visible beneath the rider so handlebars and front wheel show in first-person
		if ( iBikeBone != -1 && iParentBone == iBikeBone )
		{
			ResetAnimatableOpacityRecursive( pAttached );
			continue;
		}

		// Keep carried items held in the right hand visible unless they swing right in front of the eyes
		if ( iRightHandBone != -1 && iParentBone == iRightHandBone )
		{
			AAnimatableObjectType* pType        = pAttached->GetObjectType();
			const TBOOL            bIsEyewear   = pType && pType->GetName() == TPS8( sunglasses1 );

			if ( !bIsEyewear )
			{
				if ( !bHandBonePosValid )
				{
					TMatrix44 matHand;
					pModelInstance->GetBoneTransform( pModelInstance->GetBoneIndex( TPS8( cb_rlocator ) ), matHand );
					vHandBonePos      = matHand.GetTranslation();
					bHandBonePosValid = TTRUE;
				}

				TFLOAT fDist = sqrtf( TVector4::DistanceSq( a_rcEyePos, vHandBonePos ) );

				if ( fDist >= 0.60f )
				{
					ResetAnimatableOpacityRecursive( pAttached );
					continue;
				}

				if ( fDist > 0.40f )
				{
					HideAnimatableRecursive( pAttached, ( fDist - 0.40f ) / 0.20f );
					continue;
				}
			}
		}

		// Hide head attachments (sunglasses, Pip, hats) and any other body attachments
		HideAnimatableRecursive( pAttached, 0.0f );
	}
}

void RestoreHiddenUnitImmediate()
{
	if ( s_pHiddenUnit )
	{
		if ( AUnit* pUnit = s_pHiddenUnit->GetUnit() )
		{
			if ( s_eHiddenMode == HIDE_CAR_DRIVER )
			{
				if ( pUnit->IsA( &TGetClass( AUnitCarChaseCar ) ) )
					ResetAnimatableOpacityRecursive( TSTATICCAST( AUnitCarChaseCar, pUnit )->GetDriver() );
			}
			else if ( pUnit->IsA( &TGetClass( AAnimatedUnit ) ) )
			{
				AAnimatedUnit* pAnimUnit = TSTATICCAST( AAnimatedUnit, pUnit );
				ResetAnimatableOpacityRecursive( pAnimUnit->GetAnimatableObject() );

				if ( AModelSwapComponent* pSwap = TSTATICCAST( AModelSwapComponent, pAnimUnit->FindComponentExact( &TGetClass( AModelSwapComponent ) ) ) )
					ResetAnimatableOpacityRecursive( pSwap->GetSwappedModel() );
			}
		}

		s_pHiddenUnit = TNULL;
	}

	s_eHiddenMode = HIDE_NONE;
}

void ApplyFirstPersonNearClip()
{
	s_bNearClipOverridden = TTRUE;
}

void RestoreNearClip()
{
	s_bNearClipOverridden = TFALSE;
}

void SyncInterpolatedVehiclesBeforeRender()
{
	TBOOL bSyncedBikes = TFALSE;
	TBOOL bSyncedCars  = TFALSE;

	if ( AAnimatableObjectManager::IsSingletonCreated() )
		bSyncedBikes = AAnimatableObjectManager::GetSingleton()->SyncInterpolatedBikeTransforms();

	bSyncedCars = AUnitCarChaseCar::SyncAllInterpolatedVisuals();

	if ( ( bSyncedBikes || bSyncedCars ) && AShadowManager::IsSingletonCreated() )
		AShadowManager::GetSingleton()->Update();
}

struct ScopedVehicleVisualSync
{
	~ScopedVehicleVisualSync()
	{
		SyncInterpolatedVehiclesBeforeRender();
	}
};

} // namespace

TBOOL AFirstPersonCamera::ShouldOverrideNearClip()
{
	return s_bNearClipOverridden && IsEnabled();
}

TBOOL AFirstPersonCamera::IsEnabled()
{
	return AOptions::IsFirstPersonCameraEnabled();
}

TFLOAT AFirstPersonCamera::GetDefaultFOV()
{
	static TFLOAT s_fFOVRad = 0.0f;

	if ( s_fFOVRad == 0.0f )
	{
		TFLOAT fDegrees = DEFAULT_FOV_DEG;

		if ( const TCHAR* szCmdLine = GetCommandLineA() )
		{
			if ( const TCHAR* szArg = strstr( szCmdLine, "-fpfov " ) )
			{
				TFLOAT fParsed = TFLOAT( atof( szArg + 7 ) );

				if ( fParsed >= 50.0f && fParsed <= 115.0f )
					fDegrees = fParsed;
			}
		}

		s_fFOVRad = TMath::DegToRad( fDegrees );
	}

	return s_fFOVRad;
}

TFLOAT AFirstPersonCamera::GetAimFOV()
{
	return GetDefaultFOV() * ( 60.0f / DEFAULT_FOV_DEG );
}

TFLOAT AFirstPersonCamera::GetVehicleFOV()
{
	return TMath::Min( GetDefaultFOV() * ( 80.0f / DEFAULT_FOV_DEG ), TMath::DegToRad( 110.0f ) );
}

TFLOAT AFirstPersonCamera::GetVehicleBoostFOV()
{
	return TMath::Min( TMath::Max( TMath::HALF_PI, GetVehicleFOV() + TMath::DegToRad( 10.0f ) ), TMath::DegToRad( 120.0f ) );
}

TBOOL AFirstPersonCamera::IsOnFootPlayerTarget( const AUnit* a_pUnit )
{
	return IsEnabled() && a_pUnit && a_pUnit->IsA( &TGetClass( AUnitPlayer ) );
}

TBOOL AFirstPersonCamera::IsUnitInOnFootFirstPerson( const AUnit* a_pUnit )
{
	if ( !IsOnFootPlayerTarget( a_pUnit ) || !ACameraManager::IsSingletonCreated() )
		return TFALSE;

	ACameraManager* pCamMgr = ACameraManager::GetSingleton();
	ACameraHelper*  pHelper = pCamMgr->GetCurrentCameraHelper();

	if ( !pHelper || !pHelper->IsA( &TGetClass( AGTAStyleCameraHelper ) ) )
		return TFALSE;

	return GetHelperTargetUnit( pHelper ) == a_pUnit;
}

TBOOL AFirstPersonCamera::IsUnitInFirstPerson( const AUnit* a_pUnit )
{
	if ( !IsOnFootPlayerTarget( a_pUnit ) )
		return TFALSE;

	if ( s_bAimActive && s_pAimUnit && s_pAimUnit->GetUnit() == a_pUnit )
		return TTRUE;

	return IsUnitInOnFootFirstPerson( a_pUnit );
}

void AFirstPersonCamera::BuildForwardDirection( const TVector4& a_rcFlatForward, TFLOAT a_fPitchDeg, TVector4& a_rOutForward )
{
	TVector4 vFlat;
	NormaliseFlatSafe( a_rcFlatForward, vFlat );

	TMath::Clip( a_fPitchDeg, -MAX_PITCH_DEG, MAX_PITCH_DEG );

	TFLOAT fSinPitch;
	TFLOAT fCosPitch;
	TMath::SinCos( TMath::DegToRad( a_fPitchDeg ), fSinPitch, fCosPitch );

	a_rOutForward.Set(
	    vFlat.x * fCosPitch,
	    fSinPitch,
	    vFlat.z * fCosPitch,
	    1.0f
	);
	a_rOutForward.Normalise();
}

TFLOAT AFirstPersonCamera::PitchDegFromDirection( const TVector4& a_rcDirection )
{
	TFLOAT fMagSq = a_rcDirection.MagnitudeSq();

	if ( fMagSq <= 0.000001f )
		return 0.0f;

	TFLOAT fSinPitch = a_rcDirection.y / sqrtf( fMagSq );
	TMath::Clip( fSinPitch, -1.0f, 1.0f );

	TFLOAT fPitchDeg = TMath::ASin( fSinPitch ) * ( 180.0f / TMath::PI );
	TMath::Clip( fPitchDeg, -MAX_PITCH_DEG, MAX_PITCH_DEG );
	return fPitchDeg;
}

void AFirstPersonCamera::ComputePlayerEyePosition( AUnit* a_pUnit, const TVector4& a_rcBasePos, TFLOAT a_fHeightOffset, TVector4& a_rOutEyePos )
{
	TFLOAT fEyeHeight  = ( a_fHeightOffset > 0.3f ) ? a_fHeightOffset : DEFAULT_EYE_HEIGHT;
	TFLOAT fSafeHeight = TMath::Min( 1.0f, fEyeHeight * 0.6f );

	TVector4 vSafeStart( a_rcBasePos.x, a_rcBasePos.y - fSafeHeight, a_rcBasePos.z, 1.0f );
	a_rOutEyePos.Set( a_rcBasePos.x, a_rcBasePos.y - fEyeHeight, a_rcBasePos.z, 1.0f );

	SweepCeilingClearance( vSafeStart, a_rOutEyePos );
}

void AFirstPersonCamera::ComputeBikeCockpit( AUnit* a_pRider, const TMatrix44& a_rcVehicleTransform, TVector4& a_rOutEyePos, TVector4& a_rOutForward )
{
	const TVector4& rcVehiclePos = a_rcVehicleTransform.AsBasisVector4( BASISVECTOR_TRANSLATION );
	const TVector4& rcVehicleUp  = a_rcVehicleTransform.AsBasisVector4( BASISVECTOR_UP );
	const TVector4& rcVehicleFwd = a_rcVehicleTransform.AsBasisVector4( BASISVECTOR_FORWARD );

	TVector4 vSafeStart;

	// Follow the bike's natural lean and pitch while upright; fall back to world-up if tipped over
	if ( rcVehicleUp.y > 0.35f )
	{
		TMatrix44::TransformVector( a_rOutEyePos, a_rcVehicleTransform, BIKE_EYE_OFFSET );
		a_rOutEyePos.w = 1.0f;

		TMatrix44::TransformVector( vSafeStart, a_rcVehicleTransform, TVector4( BIKE_EYE_OFFSET.x, -0.95f, BIKE_EYE_OFFSET.z, 1.0f ) );
		vSafeStart.w = 1.0f;
	}
	else
	{
		a_rOutEyePos.Set( rcVehiclePos.x, rcVehiclePos.y + BIKE_EYE_OFFSET.y, rcVehiclePos.z, 1.0f );
		vSafeStart.Set( rcVehiclePos.x, rcVehiclePos.y - 0.95f, rcVehiclePos.z, 1.0f );
	}

	SweepCeilingClearance( vSafeStart, a_rOutEyePos );

	TVector4 vFlatFwd;
	NormaliseFlatSafe( rcVehicleFwd, vFlatFwd );

	const TFLOAT fYaw = s_fVehicleLookYaw + s_fVehicleSnapYaw;
	if ( fYaw != 0.0f )
		vFlatFwd.AsVector3().RotateY( TMath::DegToRad( fYaw ) );

	TFLOAT fVehiclePitch = PitchDegFromDirection( rcVehicleFwd );
	TMath::Clip( fVehiclePitch, -55.0f, 55.0f );

	// Dampen vehicle pitch slightly as the rider turns their head sideways
	TFLOAT fForwardAlignment = TMath::Cos( TMath::DegToRad( fYaw ) );
	TFLOAT fTotalPitch       = fVehiclePitch * fForwardAlignment + s_fVehicleLookPitch;

	BuildForwardDirection( vFlatFwd, fTotalPitch, a_rOutForward );
}

static TBOOL ContainsSubstringNoCase( const TCHAR* a_szHaystack, const TCHAR* a_szNeedle )
{
	if ( !a_szHaystack || !a_szNeedle || !a_szNeedle[ 0 ] )
		return TFALSE;

	for ( const TCHAR* pH = a_szHaystack; *pH; ++pH )
	{
		const TCHAR* pA = pH;
		const TCHAR* pB = a_szNeedle;

		while ( *pA && *pB )
		{
			TCHAR cA = ( *pA >= 'A' && *pA <= 'Z' ) ? TCHAR( *pA + ( 'a' - 'A' ) ) : *pA;
			TCHAR cB = ( *pB >= 'A' && *pB <= 'Z' ) ? TCHAR( *pB + ( 'a' - 'A' ) ) : *pB;

			if ( cA != cB )
				break;

			++pA;
			++pB;
		}

		if ( !*pB )
			return TTRUE;
	}

	return TFALSE;
}

static TINT FindDriverHeadBone( TSkeletonInstance* a_pDriverSkel )
{
	if ( !a_pDriverSkel )
		return -1;

	TINT iBone = a_pDriverSkel->GetBoneID( TPS8( c_head ).GetString() );

	if ( iBone != -1 )
		return iBone;

	static const TCHAR* const s_aszFallbackNames[] = {
		"cg_head",
		"root_head",
		"c_neck",
	};

	for ( TSIZE i = 0; i < TARRAYSIZE( s_aszFallbackNames ); i++ )
	{
		iBone = a_pDriverSkel->GetBoneID( s_aszFallbackNames[ i ] );

		if ( iBone != -1 )
			return iBone;
	}

	TSkeleton* pSkeleton = a_pDriverSkel->GetSkeleton();

	if ( !pSkeleton )
		return -1;

	const TINT iBoneCount = pSkeleton->GetBoneCount();
	TINT       iNeckBone  = -1;

	for ( TINT i = 0; i < iBoneCount; i++ )
	{
		const TCHAR* pszName = pSkeleton->GetBone( i )->GetName();

		if ( !pszName || !pszName[ 0 ] )
			continue;

		if ( ContainsSubstringNoCase( pszName, "head" ) &&
		     !ContainsSubstringNoCase( pszName, "light" ) &&
		     !ContainsSubstringNoCase( pszName, "horn" ) &&
		     !ContainsSubstringNoCase( pszName, "ear" ) &&
		     !ContainsSubstringNoCase( pszName, "end" ) &&
		     !ContainsSubstringNoCase( pszName, "nub" ) )
		{
			return i;
		}

		if ( iNeckBone == -1 && ContainsSubstringNoCase( pszName, "neck" ) )
			iNeckBone = i;
	}

	return iNeckBone;
}

void AFirstPersonCamera::ComputeCarCockpit( AUnitCarChaseCar* a_pCar, TBOOL a_bLookBack, TVector4& a_rOutEyePos, TVector4& a_rOutForward )
{
	// Use the car rigid-body frame directly (+X right, +Y down, +Z forward).
	// NOTE: ARacingCarSteer::GetSeatMatrix applies m_vSeatOffset and RotateX(+PI/2) into Z-up model space,
	// so its forward basis points along the car roof normal rather than the vehicle driving direction.
	TMatrix44        matBody     = TMatrix44::IDENTITY;
	ARacingCarSteer* pSteer      = a_pCar->GetRacingCarSteer();
	const TBOOL      bIsTruck    = ( a_pCar->GetCarType() == AUnitCarChaseCar::CARTYPE_PLAYER );
	TVector4         vSeatOffset = bIsTruck ? TVector4( 0.0f, -0.93f, -0.1f, 1.0f ) : TVector4( 0.0f, -0.93f, -0.2f, 1.0f );

	if ( pSteer )
	{
		pSteer->GetInterpolatedBodyTransform( matBody );
		vSeatOffset = pSteer->GetSeatOffset();
	}
	else if ( a_pCar->GetSteer() )
	{
		TVector4 vDir;
		a_pCar->GetSteer()->CopyDirection( vDir );
		matBody.LookAtDirection( vDir, TVector4::VEC_NEGY );
		matBody.GetTranslation() = *a_pCar->GetSteer()->GetPosition();
	}

	// Resolve the driver's head position in vehicle body space once per car so suspension moves the camera without idle head-bob
	if ( s_pCachedCar != a_pCar )
	{
		s_pCachedCar          = a_pCar;
		s_bCarEyeOffsetCached = TFALSE;
		s_vCarEyeOffset       = bIsTruck ? CAR_TRUCK_DEFAULT_EYE_OFFSET : CAR_SEDAN_DEFAULT_EYE_OFFSET;
	}

	if ( !s_bCarEyeOffsetCached )
	{
		AAnimatableObject* pDriver = a_pCar->GetDriver();
		AAnimatableObject* pCarObj = a_pCar->GetAnimatableObject();
		const TINT         iBase   = a_pCar->GetBaseBone();

		if ( pDriver && pCarObj && iBase != -1 )
		{
			AModelInstanceRef  pDriverInst = pDriver->GetModelInstance();
			AModelInstanceRef  pCarInst    = pCarObj->GetModelInstance();
			TSkeletonInstance* pDriverSkel = pDriverInst ? pDriverInst->GetSkeletonInstance() : TNULL;
			TSkeletonInstance* pCarSkel    = pCarInst ? pCarInst->GetSkeletonInstance() : TNULL;

			if ( pDriverSkel && pCarSkel )
			{
				pCarSkel->UpdateState( TFALSE );
				pDriverSkel->UpdateState( TFALSE );

				const TMatrix44& rcBaseMatrix = pCarSkel->GetBoneTransformCurrent( iBase );
				TINT             iHeadBone    = FindDriverHeadBone( pDriverSkel );
				TBOOL            bHaveHead    = TFALSE;
				TVector4         vModelHead   = TVector4::VEC_ZERO;

				if ( iHeadBone != -1 )
				{
					TMatrix44 matModelHead = TMatrix44::IDENTITY;
					matModelHead.Multiply( rcBaseMatrix, pDriverSkel->GetBoneTransformCurrent( iHeadBone ) );
					vModelHead = matModelHead.GetTranslation();
					bHaveHead  = TTRUE;
				}
				else if ( TSkeleton* pSkeleton = pDriverSkel->GetSkeleton() )
				{
					// Fallback for driver rigs without a named head/neck joint: pick the highest in-cab bone
					const TINT iBoneCount = pSkeleton->GetBoneCount();
					TFLOAT     fBestZ     = 0.20f;

					for ( TINT i = 0; i < iBoneCount; i++ )
					{
						TMatrix44 matBone = TMatrix44::IDENTITY;
						matBone.Multiply( rcBaseMatrix, pDriverSkel->GetBoneTransformCurrent( i ) );
						const TVector4& rcPos = matBone.GetTranslation();

						if ( rcPos.z > fBestZ && rcPos.z < 1.30f && TMath::Abs( rcPos.x ) < 0.95f && TMath::Abs( rcPos.y ) < 1.80f )
						{
							fBestZ     = rcPos.z;
							vModelHead = rcPos;
							bHaveHead  = TTRUE;
						}
					}
				}

				if ( bHaveHead )
				{
					// Convert from Z-up car model space (+X right, +Y forward, +Z up) into Y-down vehicle body space
					const TFLOAT fBodyX = vSeatOffset.x + vModelHead.x;
					const TFLOAT fBodyY = vSeatOffset.y - vModelHead.z;
					const TFLOAT fBodyZ = vSeatOffset.z + vModelHead.y;

					if ( vModelHead.z > 0.15f && vModelHead.z < 1.35f && TMath::Abs( fBodyX ) < 1.05f && TMath::Abs( fBodyZ ) < 2.2f )
					{
						// Keep the eye cleanly inside the windshield vertical window (below the cab roof header, above the dash)
						const TFLOAT fHighestEyeY = bIsTruck ? -1.78f : -1.56f;
						const TFLOAT fLowestEyeY  = bIsTruck ? -1.34f : -1.24f;

						TFLOAT fEyeX = fBodyX;
						TFLOAT fEyeY = fBodyY;
						TFLOAT fEyeZ = fBodyZ + 0.08f;

						TMath::Clip( fEyeX, -0.85f, 0.85f );
						TMath::Clip( fEyeY, fHighestEyeY, fLowestEyeY );
						TMath::Clip( fEyeZ, -1.40f, 1.00f );

						s_vCarEyeOffset.Set( fEyeX, fEyeY, fEyeZ, 1.0f );
						s_bCarEyeOffsetCached = TTRUE;
					}
				}
			}
		}
	}

	const TVector4& rcBodyUp  = matBody.AsBasisVector4( BASISVECTOR_UP );
	const TVector4& rcBodyPos = matBody.AsBasisVector4( BASISVECTOR_TRANSLATION );
	const TVector4& rcBodyFwd = matBody.AsBasisVector4( BASISVECTOR_FORWARD );

	if ( rcBodyUp.y > 0.25f )
	{
		TVector4 vRaisedEyeOffset = s_vCarEyeOffset;
		vRaisedEyeOffset.y -= 0.12f;
		TMatrix44::TransformVector( a_rOutEyePos, matBody, vRaisedEyeOffset );
		a_rOutEyePos.w = 1.0f;
	}
	else
	{
		a_rOutEyePos.Set( rcBodyPos.x, rcBodyPos.y + s_vCarEyeOffset.y - 0.12f, rcBodyPos.z, 1.0f );
	}

	TVector4 vFlatFwd;
	NormaliseFlatSafe( rcBodyFwd, vFlatFwd );

	TFLOAT fYawDeg = s_fVehicleLookYaw + s_fVehicleSnapYaw + ( a_bLookBack ? 180.0f : 0.0f );

	if ( fYawDeg != 0.0f )
		vFlatFwd.AsVector3().RotateY( TMath::DegToRad( fYawDeg ) );

	TFLOAT fCarPitch = PitchDegFromDirection( rcBodyFwd );
	TMath::Clip( fCarPitch, -50.0f, 50.0f );

	TFLOAT fTotalPitch = fCarPitch * TMath::Cos( TMath::DegToRad( fYawDeg ) ) + s_fVehicleLookPitch;
	BuildForwardDirection( vFlatFwd, fTotalPitch, a_rOutForward );
}

void AFirstPersonCamera::ResetVehicleLook()
{
	s_fVehicleLookYaw      = 0.0f;
	s_fVehicleSnapYaw      = 0.0f;
	s_fVehicleLookPitch    = 0.0f;
	s_vVehicleStickLook    = TVector2::VEC_ZERO;
	s_vVehicleKeyLook      = TVector2::VEC_ZERO;
	s_fVehicleLookIdleTime = 10.0f;
}

void AFirstPersonCamera::AddVehicleMouseLook( TFLOAT a_fDeltaX, TFLOAT a_fDeltaY )
{
	if ( a_fDeltaX == 0.0f && a_fDeltaY == 0.0f )
		return;

	AOptions* pOptions = AOptions::GetSingleton();
	TFLOAT    fScale   = pOptions->GetSensitivity() * 0.15f;
	TFLOAT    fPitchY  = pOptions->IsMouseInverted() ? -a_fDeltaY : a_fDeltaY;

	s_fVehicleLookYaw += a_fDeltaX * fScale;
	s_fVehicleLookPitch += fPitchY * fScale;

	TMath::Clip( s_fVehicleLookYaw, -VEHICLE_MAX_YAW_DEG, VEHICLE_MAX_YAW_DEG );
	TMath::Clip( s_fVehicleLookPitch, VEHICLE_MIN_PITCH_DEG, VEHICLE_MAX_PITCH_DEG );

	s_fVehicleLookIdleTime = 0.0f;
}

// Keep snapped yaw separate from smooth freelook, so its limits/recentering
// cannot shrink a snap or glide it back. Mouse/key freelook stays unchanged.
void AFirstPersonCamera::AddVehicleYawStep( TFLOAT a_fDegrees )
{
	s_fVehicleSnapYaw = std::remainder( s_fVehicleSnapYaw + a_fDegrees, 360.0f );
}

void AFirstPersonCamera::SetVehicleStickLook( TFLOAT a_fX, TFLOAT a_fY )
{
	AOptions* pOptions  = AOptions::GetSingleton();
	s_vVehicleStickLook = TVector2( a_fX, pOptions->IsMouseInverted() ? -a_fY : a_fY );

	if ( TMath::Abs( a_fX ) > 0.05f || TMath::Abs( a_fY ) > 0.05f )
		s_fVehicleLookIdleTime = 0.0f;
}

void AFirstPersonCamera::AddVehicleKeyLook( TFLOAT a_fYaw, TFLOAT a_fPitch )
{
	s_vVehicleKeyLook.x += a_fYaw;
	s_vVehicleKeyLook.y += a_fPitch;
	s_fVehicleLookIdleTime = 0.0f;
}

void AFirstPersonCamera::UpdateVehicleLook( TFLOAT a_fDeltaTime, TFLOAT a_fVehicleSpeed )
{
	TFLOAT fYawInput    = s_vVehicleStickLook.x + s_vVehicleKeyLook.x;
	TFLOAT fPitchInput  = s_vVehicleStickLook.y + s_vVehicleKeyLook.y;
	s_vVehicleStickLook = TVector2::VEC_ZERO;
	s_vVehicleKeyLook   = TVector2::VEC_ZERO;

	TMath::Clip( fYawInput, -1.5f, 1.5f );
	TMath::Clip( fPitchInput, -1.5f, 1.5f );

	if ( TMath::Abs( fYawInput ) > 0.01f || TMath::Abs( fPitchInput ) > 0.01f )
	{
		s_fVehicleLookYaw += fYawInput * VEHICLE_STICK_YAW_RATE * a_fDeltaTime;
		s_fVehicleLookPitch += fPitchInput * VEHICLE_STICK_PITCH_RATE * a_fDeltaTime;

		TMath::Clip( s_fVehicleLookYaw, -VEHICLE_MAX_YAW_DEG, VEHICLE_MAX_YAW_DEG );
		TMath::Clip( s_fVehicleLookPitch, VEHICLE_MIN_PITCH_DEG, VEHICLE_MAX_PITCH_DEG );

		s_fVehicleLookIdleTime = 0.0f;
		return;
	}

	s_fVehicleLookIdleTime += a_fDeltaTime;

	// Gently return the rider/driver gaze to the road ahead while moving
	if ( a_fVehicleSpeed > 1.5f && s_fVehicleLookIdleTime > VEHICLE_RECENTER_DELAY )
	{
		TFLOAT fBlend       = ( a_fDeltaTime > 0.0f ) ? ( 1.0f - expf( -VEHICLE_RECENTER_SPEED * a_fDeltaTime ) ) : 0.0f;
		s_fVehicleLookYaw   = TMath::LERP( s_fVehicleLookYaw, 0.0f, fBlend );
		s_fVehicleLookPitch = TMath::LERP( s_fVehicleLookPitch, 0.0f, fBlend );

		if ( TMath::Abs( s_fVehicleLookYaw ) < 0.05f )
			s_fVehicleLookYaw = 0.0f;

		if ( TMath::Abs( s_fVehicleLookPitch ) < 0.05f )
			s_fVehicleLookPitch = 0.0f;
	}
}

void AFirstPersonCamera::EnterAimMode( AUnit* a_pOwner, const TVector4& a_rcInitialForward )
{
	s_bAimActive    = TTRUE;
	s_pAimUnit      = a_pOwner ? a_pOwner->GetSelfProxy() : TRef<AUnitProxy>( TNULL );
	s_vAimMouseTurn = TVector2::VEC_ZERO;
	s_vAimRateTurn  = TVector2::VEC_ZERO;
	s_vAimDpadTurn  = TVector2::VEC_ZERO;

	NormaliseFlatSafe( a_rcInitialForward, s_vAimFlatDir );
	s_fAimPitchDeg = PitchDegFromDirection( a_rcInitialForward );
	BuildForwardDirection( s_vAimFlatDir, s_fAimPitchDeg, s_vAimForward );
}

void AFirstPersonCamera::LeaveAimMode()
{
	s_bAimActive    = TFALSE;
	s_pAimUnit      = TNULL;
	s_vAimMouseTurn = TVector2::VEC_ZERO;
	s_vAimRateTurn  = TVector2::VEC_ZERO;
	s_vAimDpadTurn  = TVector2::VEC_ZERO;

	if ( s_eViewMode == VIEW_AIM )
		s_eViewMode = VIEW_NONE;
}

TBOOL AFirstPersonCamera::IsAimModeActive()
{
	return s_bAimActive && IsEnabled();
}

void AFirstPersonCamera::AddAimMouseInput( TFLOAT a_fDeltaX, TFLOAT a_fDeltaY, TBOOL& a_rbBreakLock )
{
	AOptions* pOptions = AOptions::GetSingleton();
	TFLOAT    fScale   = pOptions->GetSensitivity() * AIM_MOUSE_SCALE;
	TFLOAT    fPitchY  = pOptions->IsMouseInverted() ? -a_fDeltaY : a_fDeltaY;

	s_vAimMouseTurn.x += a_fDeltaX * fScale;
	s_vAimMouseTurn.y += fPitchY * fScale;

	if ( TMath::Abs( a_fDeltaX ) > 10.0f || TMath::Abs( a_fDeltaY ) > 10.0f )
		a_rbBreakLock = TTRUE;
}

void AFirstPersonCamera::AddAimYawStep( TFLOAT a_fDegrees )
{
	// This accumulator is already applied as an immediate yaw delta, not a rate.
	s_vAimMouseTurn.x += a_fDegrees;
}

void AFirstPersonCamera::AddAimRateInput( TFLOAT a_fX, TFLOAT a_fY, TBOOL& a_rbBreakLock )
{
	AOptions* pOptions = AOptions::GetSingleton();
	TFLOAT    fPitchY  = pOptions->IsMouseInverted() ? -a_fY : a_fY;

	s_vAimRateTurn.x += a_fX;
	s_vAimRateTurn.y += fPitchY;

	if ( TMath::Abs( a_fX ) > 0.35f || TMath::Abs( a_fY ) > 0.35f )
		a_rbBreakLock = TTRUE;
}

void AFirstPersonCamera::SetAimLookButtonTurn( TBOOL a_bYaw, TFLOAT a_fTurn )
{
	if ( a_bYaw )
		s_vAimDpadTurn.x = a_fTurn;
	else
		s_vAimDpadTurn.y = a_fTurn;
}

void AFirstPersonCamera::UpdateAimCamera(
    AUnit*           a_pOwner,
    ACamera*         a_pCamera,
    TFLOAT           a_fDeltaTime,
    TBOOL            a_bLockedOn,
    const TVector4&  a_rcLockPoint,
    TVector2&        a_rAimCursor,
    TFLOAT           a_fScreenWidth,
    TFLOAT           a_fScreenHeight
)
{
	if ( !a_pOwner || !a_pCamera || !a_pOwner->GetSteer() )
		return;

	if ( !s_bAimActive || !s_pAimUnit || s_pAimUnit->GetUnit() != a_pOwner )
		EnterAimMode( a_pOwner, a_pCamera->GetOrientation() );

	TFLOAT fRateX = s_vAimRateTurn.x + s_vAimDpadTurn.x;
	TFLOAT fRateY = s_vAimRateTurn.y + s_vAimDpadTurn.y;
	TMath::Clip( fRateX, -2.0f, 2.0f );
	TMath::Clip( fRateY, -2.0f, 2.0f );

	TFLOAT fYawDeltaDeg   = s_vAimMouseTurn.x + fRateX * AIM_STICK_YAW_RATE * a_fDeltaTime;
	TFLOAT fPitchDeltaDeg = s_vAimMouseTurn.y + fRateY * AIM_STICK_PITCH_RATE * a_fDeltaTime;

	const TBOOL bHasManualInput = TMath::Abs( s_vAimMouseTurn.x ) > 0.0001f ||
	                              TMath::Abs( s_vAimMouseTurn.y ) > 0.0001f ||
	                              TMath::Abs( fRateX ) > 0.01f ||
	                              TMath::Abs( fRateY ) > 0.01f;

	s_vAimMouseTurn = TVector2::VEC_ZERO;
	s_vAimRateTurn  = TVector2::VEC_ZERO;

	if ( fYawDeltaDeg != 0.0f )
	{
		s_vAimFlatDir.AsVector3().RotateY( TMath::DegToRad( fYawDeltaDeg ) );
		NormaliseFlatSafe( s_vAimFlatDir, s_vAimFlatDir );
	}

	s_fAimPitchDeg += fPitchDeltaDeg;
	TMath::Clip( s_fAimPitchDeg, -MAX_PITCH_DEG, MAX_PITCH_DEG );

	TVector4 vEyePos;
	ComputePlayerEyePosition( a_pOwner, *a_pOwner->GetSteer()->GetPosition(), DEFAULT_EYE_HEIGHT, vEyePos );

	// Smoothly track locked-on milk targets when the player isn't actively turning away
	if ( a_bLockedOn && !bHasManualInput )
	{
		TVector4 vToLock(
		    a_rcLockPoint.x - vEyePos.x,
		    a_rcLockPoint.y - vEyePos.y,
		    a_rcLockPoint.z - vEyePos.z,
		    1.0f
		);

		if ( vToLock.MagnitudeSq() > 0.04f )
		{
			TVector4 vLockFlat;
			NormaliseFlatSafe( vToLock, vLockFlat );

			TFLOAT fBlend = TMath::Min( 1.0f, a_fDeltaTime * 8.0f );
			s_vAimFlatDir.Lerp4( s_vAimFlatDir, vLockFlat, fBlend );
			NormaliseFlatSafe( s_vAimFlatDir, s_vAimFlatDir );

			TFLOAT fTargetPitch = PitchDegFromDirection( vToLock );
			s_fAimPitchDeg      = TMath::LERP( s_fAimPitchDeg, fTargetPitch, fBlend );
			TMath::Clip( s_fAimPitchDeg, -MAX_PITCH_DEG, MAX_PITCH_DEG );
		}
	}
	else if ( !a_bLockedOn )
	{
		// Keep the free-aim reticle pinned at the centre of the screen
		a_rAimCursor.x = a_fScreenWidth * 0.5f;
		a_rAimCursor.y = a_fScreenHeight * 0.5f;
	}

	BuildForwardDirection( s_vAimFlatDir, s_fAimPitchDeg, s_vAimForward );

	a_pCamera->GetMatrix().GetTranslation() = vEyePos;
	a_pCamera->LookAtDirection( s_vAimForward );
	a_pCamera->SetFOV( TMath::LERPClamped( a_pCamera->GetFOV(), GetAimFOV(), a_fDeltaTime * 8.0f ) );

	s_eViewMode   = VIEW_AIM;
	s_pActiveUnit = a_pOwner->GetSelfProxy();

	ApplyFirstPersonNearClip();
	ApplyUnitHidden( a_pOwner, HIDE_PLAYER, vEyePos );
}

void AFirstPersonCamera::RegisterOnFootView(
    AGTAStyleCameraHelper* a_pHelper,
    AUnit*                 a_pPlayer,
    const TVector4&        a_rcForward,
    TFLOAT                 a_fHeightOffset,
    TBOOL                  a_bUsePosOverride,
    const TVector4&        a_rcPosOverride
)
{
	s_eViewMode        = VIEW_ON_FOOT;
	s_pOnFootHelper    = a_pHelper;
	s_pActiveUnit      = a_pPlayer ? a_pPlayer->GetSelfProxy() : TRef<AUnitProxy>( TNULL );
	s_vOnFootForward   = a_rcForward;
	s_fOnFootEyeHeight = a_fHeightOffset;
	s_bOnFootOverride  = a_bUsePosOverride;
	s_vOnFootOverride  = a_rcPosOverride;

	if ( a_pPlayer && a_pPlayer->GetSteer() )
	{
		const TVector4& rcBase = a_bUsePosOverride ? a_rcPosOverride : *a_pPlayer->GetSteer()->GetPosition();
		TVector4        vEyePos;
		ComputePlayerEyePosition( a_pPlayer, rcBase, a_fHeightOffset, vEyePos );

		ApplyFirstPersonNearClip();
		ApplyUnitHidden( a_pPlayer, HIDE_PLAYER, vEyePos );
	}
}

void AFirstPersonCamera::RegisterBikeView( ABikePhysicsCameraHelper* a_pHelper, AUnit* a_pRider )
{
	s_eViewMode      = VIEW_BIKE;
	s_pVehicleHelper = a_pHelper;
	s_pActiveUnit    = a_pRider ? a_pRider->GetSelfProxy() : TRef<AUnitProxy>( TNULL );

	if ( a_pRider && a_pRider->GetSteer() )
	{
		ApplyFirstPersonNearClip();
		ApplyUnitHidden( a_pRider, HIDE_RIDER, *a_pRider->GetSteer()->GetPosition() );
	}
}

void AFirstPersonCamera::RegisterCarView( ABikePhysicsCameraHelper* a_pHelper, AUnitCarChaseCar* a_pCar, TBOOL a_bLookBack )
{
	s_eViewMode      = VIEW_CAR;
	s_pVehicleHelper = a_pHelper;
	s_pActiveUnit    = a_pCar ? a_pCar->GetSelfProxy() : TRef<AUnitProxy>( TNULL );
	s_bCarLookBack   = a_bLookBack;

	if ( a_pCar && a_pCar->GetSteer() )
	{
		ApplyFirstPersonNearClip();
		ApplyUnitHidden( a_pCar, HIDE_CAR_DRIVER, *a_pCar->GetSteer()->GetPosition() );
	}
}

void AFirstPersonCamera::RegisterFlightView( AUnit* a_pFlightUnit, const TVector4& a_rcEyePos )
{
	s_eViewMode        = VIEW_FLIGHT;
	s_pActiveUnit      = a_pFlightUnit ? a_pFlightUnit->GetSelfProxy() : TRef<AUnitProxy>( TNULL );
	s_vFlightEyePos    = a_rcEyePos;
	s_bFlightThisFrame = TTRUE;

	if ( a_pFlightUnit )
	{
		ApplyFirstPersonNearClip();
		ApplyUnitHidden( a_pFlightUnit, HIDE_SIMPLE, a_rcEyePos );
	}
}

void AFirstPersonCamera::SyncBeforeRender( ACamera* a_pCamera )
{
	if ( !a_pCamera || !ACameraManager::IsSingletonCreated() )
		return;

	const TBOOL bFlightUpdated = s_bFlightThisFrame;
	s_bFlightThisFrame         = TFALSE;

	if ( !IsEnabled() )
	{
		s_eViewMode = VIEW_NONE;
		RestoreHiddenUnitImmediate();
		RestoreNearClip();
		return;
	}

	ACameraManager* pCamMgr        = ACameraManager::GetSingleton();
	ACameraHelper*  pCurrentHelper = pCamMgr->GetCurrentCameraHelper();
	AUnit*          pUnit          = s_pActiveUnit ? s_pActiveUnit->GetUnit() : TNULL;

	// Sync visual bike/car meshes, passengers, wheels, lights and shadows to the same interpolated step once camera is placed
	ScopedVehicleVisualSync oVehicleVisualSync;

	switch ( s_eViewMode )
	{
		case VIEW_ON_FOOT:
		{
			if ( pUnit && pUnit->GetSteer() && pCurrentHelper == s_pOnFootHelper && GetHelperTargetUnit( s_pOnFootHelper ) == pUnit )
			{
				const TVector4& rcBase = s_bOnFootOverride ? s_vOnFootOverride : *pUnit->GetSteer()->GetPosition();
				TVector4        vEyePos;
				ComputePlayerEyePosition( pUnit, rcBase, s_fOnFootEyeHeight, vEyePos );

				// Enter on-foot first-person immediately, even if the camera manager
				// is blending from a minigame, loading camera or third-person view.
				// Never render the approach to the player or briefly restore the body.

				a_pCamera->GetMatrix().GetTranslation() = vEyePos;
				a_pCamera->LookAtDirection( s_vOnFootForward );
				ApplyFirstPersonNearClip();
				ApplyUnitHidden( pUnit, HIDE_PLAYER, vEyePos );
				return;
			}
			break;
		}
		case VIEW_AIM:
		{
			// Aim mode runs with all camera helpers detached; if another helper attached (cutscene/conversation), exit cleanly
			if ( s_bAimActive && pUnit && pUnit->GetSteer() && pCurrentHelper == TNULL )
			{
				TVector4 vEyePos;
				ComputePlayerEyePosition( pUnit, *pUnit->GetSteer()->GetPosition(), DEFAULT_EYE_HEIGHT, vEyePos );

				// Guard against fixed-camera states that may have overwritten the camera position directly
				if ( TVector4::DistanceSq( a_pCamera->GetMatrix().GetTranslation(), vEyePos ) <= 4.0f )
				{
					a_pCamera->GetMatrix().GetTranslation() = vEyePos;
					a_pCamera->LookAtDirection( s_vAimForward );
					ApplyFirstPersonNearClip();
					ApplyUnitHidden( pUnit, HIDE_PLAYER, vEyePos );
					return;
				}
			}
			break;
		}
		case VIEW_BIKE:
		{
			if ( pUnit && pUnit->GetSteer() && pUnit->GetSteer()->IsA( &TGetClass( ABikeSteer ) ) &&
			     TSTATICCAST( ABikeSteer, pUnit->GetSteer() )->GetPhysics() &&
			     pCurrentHelper == s_pVehicleHelper && GetHelperTargetUnit( s_pVehicleHelper ) == pUnit )
			{
				TMatrix44 matTransform;
				AVehicleManager::GetInterpolatedBodyTransform(
				    TSTATICCAST( ABikeSteer, pUnit->GetSteer() )->GetPhysics()->GetBody(),
				    matTransform
				);
				TVector4 vEyePos;
				TVector4 vForward;
				ComputeBikeCockpit( pUnit, matTransform, vEyePos, vForward );

				// Keep the rider hidden even while blending into the cockpit;
				// restoring it until the camera gets close causes a body flash on mount.
				ApplyFirstPersonNearClip();
				ApplyUnitHidden( pUnit, HIDE_RIDER, vEyePos );

				if ( pCamMgr->m_bIsPosLerping && TVector4::DistanceSq( pCamMgr->m_vLerpPosFrom, vEyePos ) > 0.25f )
					return;

				a_pCamera->GetMatrix().GetTranslation() = vEyePos;
				a_pCamera->LookAtDirection( vForward );
				return;
			}
			break;
		}
		case VIEW_CAR:
		{
			if ( pUnit && pUnit->IsA( &TGetClass( AUnitCarChaseCar ) ) &&
			     pCurrentHelper == s_pVehicleHelper && GetHelperTargetUnit( s_pVehicleHelper ) == pUnit )
			{
				AUnitCarChaseCar* pCar = TSTATICCAST( AUnitCarChaseCar, pUnit );
				TVector4          vEyePos;
				TVector4          vForward;
				ComputeCarCockpit( pCar, s_bCarLookBack, vEyePos, vForward );

				if ( pCamMgr->m_bIsPosLerping && TVector4::DistanceSq( pCamMgr->m_vLerpPosFrom, vEyePos ) > 0.25f )
				{
					if ( TVector4::DistanceSq( a_pCamera->GetMatrix().GetTranslation(), vEyePos ) <= HIDE_ENTER_DISTANCE * HIDE_ENTER_DISTANCE )
					{
						ApplyFirstPersonNearClip();
						ApplyUnitHidden( pCar, HIDE_CAR_DRIVER, vEyePos );
					}
					else
					{
						RestoreHiddenUnitImmediate();
					}
					return;
				}

				a_pCamera->GetMatrix().GetTranslation() = vEyePos;
				a_pCamera->LookAtDirection( vForward );
				ApplyFirstPersonNearClip();
				ApplyUnitHidden( pCar, HIDE_CAR_DRIVER, vEyePos );
				return;
			}
			break;
		}
		case VIEW_FLIGHT:
		{
			if ( bFlightUpdated && pUnit && pCurrentHelper == TNULL &&
			     TVector4::DistanceSq( a_pCamera->GetMatrix().GetTranslation(), s_vFlightEyePos ) <= 1.0f )
			{
				ApplyFirstPersonNearClip();
				ApplyUnitHidden( pUnit, HIDE_SIMPLE, s_vFlightEyePos );
				return;
			}
			break;
		}
		case VIEW_NONE:
		default:
			break;
	}

	// No first-person camera is active anymore (cutscene, conversation, menu, fixed minigame, etc.)
	s_eViewMode = VIEW_NONE;

	// While smoothly pulling out from first-person to another helper, wait until the camera clears the head before showing it
	if ( s_pHiddenUnit && pCamMgr->m_bIsPosLerping &&
	     TVector4::DistanceSq( a_pCamera->GetMatrix().GetTranslation(), s_vLastHiddenEye ) < HIDE_EXIT_DISTANCE * HIDE_EXIT_DISTANCE )
	{
		return;
	}

	RestoreHiddenUnitImmediate();
	RestoreNearClip();
}

void AFirstPersonCamera::RestoreAll()
{
	s_eViewMode        = VIEW_NONE;
	s_bFlightThisFrame = TFALSE;
	RestoreHiddenUnitImmediate();
	RestoreNearClip();
}
