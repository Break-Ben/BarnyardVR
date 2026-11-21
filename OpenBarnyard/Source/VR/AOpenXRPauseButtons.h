#pragma once
#include <Toshi/Typedefs.h>

// Delays standalone stick clicks briefly to distinguish them from a pause chord.
// Kept independent of OpenXR types so the adapter logic can be checked in isolation.
class AOpenXRPauseButtons
{
public:
	TUINT32 Update( TUINT32 a_uiButtons, TFLOAT a_fDeltaTime );
	void    Reset();

private:
	TUINT32 m_uiPending   = 0;
	TUINT32 m_uiDelivered = 0;
	TFLOAT  m_fWait       = 0.0f;
	TBOOL   m_bChord      = TFALSE;
};
