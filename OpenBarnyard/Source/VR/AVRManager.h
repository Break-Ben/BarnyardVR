#pragma once
#include "AVRPromptTypes.h"

#include <Toshi/Typedefs.h>
#include <Math/TMatrix44.h>

namespace Toshi
{
class TCameraObject;
class TRenderContext;
class TRenderInterface;
class TViewport;
}

// Owns OpenXR session/frame state and keeps all OpenXR/Win32 graphics types private to
// AVRManager.cpp. This header is intentionally safe for mods and the WebAssembly build.
class AVRManager
{
public:
	struct ControllerInputState
	{
		TFLOAT fLeftStickX = 0.0f;
		TFLOAT fLeftStickY = 0.0f;
		TFLOAT fRightStickX = 0.0f;
		TFLOAT fRightStickY = 0.0f;
		TUINT32 uiButtonMask = 0;
	};

	enum ControllerButton : TUINT32
	{
		CONTROLLER_BUTTON_DPAD_UP          = 1u << 0,
		CONTROLLER_BUTTON_DPAD_RIGHT       = 1u << 1,
		CONTROLLER_BUTTON_DPAD_DOWN        = 1u << 2,
		CONTROLLER_BUTTON_DPAD_LEFT        = 1u << 3,
		CONTROLLER_BUTTON_SELECT           = 1u << 4,
		CONTROLLER_BUTTON_START            = 1u << 5,
		CONTROLLER_BUTTON_STICK_LEFT       = 1u << 6,
		CONTROLLER_BUTTON_STICK_RIGHT      = 1u << 7,
		CONTROLLER_BUTTON_CONTEXT_1        = 1u << 8,
		CONTROLLER_BUTTON_CONTEXT_2        = 1u << 9,
		CONTROLLER_BUTTON_CONTEXT_3        = 1u << 10,
		CONTROLLER_BUTTON_CONTEXT_4        = 1u << 11,
		CONTROLLER_BUTTON_SHOULDER_LEFT_1  = 1u << 12,
		CONTROLLER_BUTTON_SHOULDER_LEFT_2  = 1u << 13,
		CONTROLLER_BUTTON_SHOULDER_RIGHT_1 = 1u << 14,
		CONTROLLER_BUTTON_SHOULDER_RIGHT_2 = 1u << 15,
	};

public:
	static AVRManager& GetSingleton();

	// The OpenXR action set is created with the instance and attached to every session.
	// The game input device reads this frame's action state through the usual Toshi
	// controller interface.
	TUINT32 GetControllerPromptRevision() const;
	AVRControllerPrompt GetControllerPrompt( TINT a_iDoodad ) const;
	TBOOL HasControllerInput() const;
	TBOOL GetControllerInputState( ControllerInputState& a_rState ) const;
	void  SetControllerVibration( TFLOAT a_fAmplitude );

	// Prepare is called before the DX11 device is created so the runtime's required
	// adapter LUID can be used during device selection.
	TBOOL Prepare( Toshi::TRenderInterface* a_pRenderer );
	TBOOL Initialize( Toshi::TRenderInterface* a_pRenderer );
	void  Shutdown();

	TBOOL IsActive() const;
	TBOOL ShouldExit() const;
	TBOOL BeginFrame();
	TBOOL ShouldRender() const;
	TUINT32 GetViewCount() const;
	TUINT32 GetCurrentEye() const;

	// Center-head pose for gameplay, independent of eye rendering. Uses the same
	// recentering, coordinate conversion and yaw-only rig as the rendered views.
	TBOOL GetHeadWorldMatrix( const Toshi::TMatrix44& a_rcRigCamera, Toshi::TMatrix44& a_rOutHead ) const;

	// Align the rig's forward and fixed UI with the current physical heading.
	TBOOL AlignEntryHeading();
	// Rebase for a bike handoff while retaining the current world-facing view.
	TBOOL AlignBikeEntryHeading( const Toshi::TMatrix44& a_rcRigCamera, Toshi::TVector4& a_rOutRigForward );

	// The camera matrix is the ordinary, gravity-locked game camera pose for this frame.
	TBOOL BeginEye(
	    TUINT32 a_uiEye,
	    const Toshi::TMatrix44& a_rcCenterCamera,
	    Toshi::TCameraObject* a_pCamera,
	    Toshi::TViewport* a_pWorldViewport,
	    Toshi::TViewport* a_pHALViewport1,
	    Toshi::TViewport* a_pHALViewport2,
	    TBOOL a_bWorldRendered
	);
	void ApplyEyeProjection( Toshi::TRenderContext* a_pContext );
	void EndEye();

	// The quad layer is a single transparent 2D render target, composed over both eyes.
	TBOOL BeginUILayer();
	// Draw within the GUI viewport, after normal UI; also available in release builds.
	void  RenderDebugHUD();
	void  EndUILayer();
	void  EndFrame();

	void OnDeviceLost();
	void OnDeviceFound();
	void OnDisplayChanged();

public:
	// Public only so translation-unit-local OpenXR resource helpers can name the PImpl type.
	// No OpenXR/platform types are exposed by the definition.
	struct Impl;

private:
	AVRManager();
	~AVRManager();
	AVRManager( const AVRManager& )            = delete;
	AVRManager& operator=( const AVRManager& ) = delete;

	Impl* m_pImpl;
};
