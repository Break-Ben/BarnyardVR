#pragma once
#include <Math/TMatrix44.h>
#include <Math/TVector2.h>
#include <Math/TVector4.h>

class ACamera;
class AGTAStyleCameraHelper;
class ABikePhysicsCameraHelper;
class AUnit;
class AUnitCarChaseCar;

//-----------------------------------------------------------------------------
// Purpose: Centralized first-person camera controller. Handles first-person
// FOV and near-clip configuration, eye/cockpit anchoring with ceiling-clearance
// checks, vehicle and aim freelook state, post-physics camera synchronization
// before rendering, and automatic hiding/restoration of the active player,
// rider, car driver or flying unit and its attachments.
//
// All state is kept here rather than in retail classes so every sizeof static
// assert and save file stays identical to the original binary.
//-----------------------------------------------------------------------------
class AFirstPersonCamera
{
public:
	// Default first-person horizontal FOV on a 4:3 reference viewport (~91 deg Hor+ on 16:9, ~60 deg vertical)
	static constexpr TFLOAT DEFAULT_FOV_DEG = 75.0f;

	// Pitch limits (degrees, negative looks up along -Y, positive looks down along +Y)
	static constexpr TFLOAT MAX_PITCH_DEG        = 80.0f;
	static constexpr TFLOAT LADDER_MAX_PITCH_DEG = 75.0f;
	static constexpr TFLOAT LADDER_MAX_YAW_DEG   = 72.0f;

	// Near clip plane while a first-person view is active
	static constexpr TFLOAT NEAR_CLIP = 0.08f;

	// Default eye height above the player's feet
	static constexpr TFLOAT DEFAULT_EYE_HEIGHT = 1.7f;

public:
	// Returns TTRUE when first-person mode is enabled in the options
	static TBOOL IsEnabled();

	// First-person FOV values (radians, 4:3 Hor+)
	static TFLOAT GetDefaultFOV();
	static TFLOAT GetAimFOV();
	static TFLOAT GetVehicleFOV();
	static TFLOAT GetVehicleBoostFOV();

	// Returns TTRUE if a_pUnit is an on-foot player controlled by the first-person camera
	static TBOOL IsOnFootPlayerTarget( const AUnit* a_pUnit );

	// Returns TTRUE if the active camera is currently showing a_pUnit in on-foot first-person
	static TBOOL IsUnitInOnFootFirstPerson( const AUnit* a_pUnit );

	// Returns TTRUE if the active camera is currently showing a_pUnit in on-foot or aim first-person
	static TBOOL IsUnitInFirstPerson( const AUnit* a_pUnit );

	// Builds a unit 3D forward vector from an XZ direction and a pitch angle in degrees
	static void BuildForwardDirection( const Toshi::TVector4& a_rcFlatForward, TFLOAT a_fPitchDeg, Toshi::TVector4& a_rOutForward );

	// Extracts the pitch angle in degrees (clamped to [-MAX_PITCH_DEG, MAX_PITCH_DEG]) from a 3D direction
	static TFLOAT PitchDegFromDirection( const Toshi::TVector4& a_rcDirection );

	// Computes the player's first-person eye position above a_rcBasePos, ducking below low ceilings/overhangs
	static void ComputePlayerEyePosition( AUnit* a_pUnit, const Toshi::TVector4& a_rcBasePos, TFLOAT a_fHeightOffset, Toshi::TVector4& a_rOutEyePos );

	// Computes the first-person eye position and look direction on a bike
	static void ComputeBikeCockpit( AUnit* a_pRider, const Toshi::TMatrix44& a_rcVehicleTransform, Toshi::TVector4& a_rOutEyePos, Toshi::TVector4& a_rOutForward );

	// Computes the first-person eye position and look direction in the Joy Ride car
	static void ComputeCarCockpit( AUnitCarChaseCar* a_pCar, TBOOL a_bLookBack, Toshi::TVector4& a_rOutEyePos, Toshi::TVector4& a_rOutForward );

	//-----------------------------------------------------------------------------
	// Vehicle freelook (bikes and Joy Ride cars)
	//-----------------------------------------------------------------------------
	static void ResetVehicleLook();
	static void AddVehicleMouseLook( TFLOAT a_fDeltaX, TFLOAT a_fDeltaY );
	static void SetVehicleStickLook( TFLOAT a_fX, TFLOAT a_fY );
	static void AddVehicleKeyLook( TFLOAT a_fYaw, TFLOAT a_fPitch );
	static void UpdateVehicleLook( TFLOAT a_fDeltaTime, TFLOAT a_fVehicleSpeed );

	//-----------------------------------------------------------------------------
	// Aim camera (AAimedSquirtProjectileLauncher on the human player)
	//-----------------------------------------------------------------------------
	static void  EnterAimMode( AUnit* a_pOwner, const Toshi::TVector4& a_rcInitialForward );
	static void  LeaveAimMode();
	static TBOOL IsAimModeActive();
	static void  AddAimMouseInput( TFLOAT a_fDeltaX, TFLOAT a_fDeltaY, TBOOL& a_rbBreakLock );
	static void  AddAimRateInput( TFLOAT a_fX, TFLOAT a_fY, TBOOL& a_rbBreakLock );
	static void  SetAimLookButtonTurn( TBOOL a_bYaw, TFLOAT a_fTurn );
	static void  UpdateAimCamera(
	     AUnit*                 a_pOwner,
	     ACamera*               a_pCamera,
	     TFLOAT                 a_fDeltaTime,
	     TBOOL                  a_bLockedOn,
	     const Toshi::TVector4& a_rcLockPoint,
	     Toshi::TVector2&       a_rAimCursor,
	     TFLOAT                 a_fScreenWidth,
	     TFLOAT                 a_fScreenHeight
	 );

	//-----------------------------------------------------------------------------
	// Active view registration and post-physics sync (called before rendering)
	//-----------------------------------------------------------------------------
	static void RegisterOnFootView(
	    AGTAStyleCameraHelper* a_pHelper,
	    AUnit*                 a_pPlayer,
	    const Toshi::TVector4& a_rcForward,
	    TFLOAT                 a_fHeightOffset,
	    TBOOL                  a_bUsePosOverride,
	    const Toshi::TVector4& a_rcPosOverride
	);
	static void RegisterBikeView( ABikePhysicsCameraHelper* a_pHelper, AUnit* a_pRider );
	static void RegisterCarView( ABikePhysicsCameraHelper* a_pHelper, AUnitCarChaseCar* a_pCar, TBOOL a_bLookBack );
	static void RegisterFlightView( AUnit* a_pFlightUnit, const Toshi::TVector4& a_rcEyePos );

	// Returns TTRUE while the active view should clamp the renderer near clip to NEAR_CLIP
	static TBOOL ShouldOverrideNearClip();

	// Synchronizes the camera with post-physics unit transforms and enforces opacity / near-clip state
	static void SyncBeforeRender( ACamera* a_pCamera );

	// Immediately restores any unit hidden for first-person and resets the near clip
	static void RestoreAll();
};
