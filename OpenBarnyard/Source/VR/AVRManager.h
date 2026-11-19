#pragma once

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
	static AVRManager& GetSingleton();

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
