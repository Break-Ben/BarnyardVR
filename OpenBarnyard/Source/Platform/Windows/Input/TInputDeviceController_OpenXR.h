#pragma once

#include <Input/TInputDeviceController.h>
#include "VR/AOpenXRPauseButtons.h"

TOSHI_NAMESPACE_START

// Presents the two OpenXR hand controllers as one ordinary gamepad, so gameplay
// consumes the same standard doodads and axis events as DirectInput and XInput.
class TInputOpenXRDeviceController : public TInputDeviceController
{
public:
	TDECLARE_CLASS( TInputOpenXRDeviceController, TInputDeviceController );

public:
	TInputOpenXRDeviceController();

	virtual TBOOL  Acquire() override;
	virtual TBOOL  Unacquire() override;
	virtual void   Release() override;
	virtual void   Update( TFLOAT a_fDeltaTime ) override;
	virtual TBOOL  Flush() override { return TTRUE; }
	virtual TINT   ProcessEvents( EventEmitter& a_rEmitter, TFLOAT a_fDeltaTime ) override;
	virtual TINT   GetButtonCount() const override { return 12; }
	virtual TINT   GetAxisCount() const override { return 2; }
	virtual TBOOL  IsDown( Doodad a_iDoodad ) const override;
	virtual TINT   GetAxisInt( Doodad a_iDoodad, TINT a_iAxis ) const override;
	virtual TINT   GetAxisInt2( Doodad a_iDoodad, TINT a_iAxis ) const override;
	virtual TFLOAT GetAxisFloat( Doodad a_iDoodad, TINT a_iAxis ) const override;
	virtual TFLOAT GetAxisFloat2( Doodad a_iDoodad, TINT a_iAxis ) const override;
	virtual TBOOL  IsEnabled() const override { return TTRUE; }
	virtual TBOOL  IsForceFeedbackDevice() override;
	virtual TBOOL  WasDown( Doodad a_iDoodad ) const override;
	virtual void   SetVibration( VIBRATION_ACTUATOR a_eActuator, TFLOAT a_fValue, TBOOL a_bUnknown ) override;
	virtual TINT   GetPortIndex() override { return -1; }

private:
	AOpenXRPauseButtons m_oPauseButtons;
	TUINT32 m_uiButtonMask;
	TUINT32 m_uiOldButtonMask;
	TFLOAT  m_afSticks[ 4 ];
	TFLOAT  m_afRawSticks[ 4 ];
	TFLOAT  m_afVibration[ 2 ];
	TBOOL   m_bPolled;
};

TOSHI_NAMESPACE_END
