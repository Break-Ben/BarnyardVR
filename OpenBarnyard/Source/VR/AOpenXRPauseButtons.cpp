#include "pch.h"
#include "AOpenXRPauseButtons.h"
#include "AVRManager.h"
#include <algorithm>
#include <cmath>

#include <Core/TMemoryDebugOn.h>

void AOpenXRPauseButtons::Reset()
{
	m_uiPending = m_uiDelivered = 0;
	m_fWait                     = 0.0f;
	m_bChord                    = TFALSE;
}

TUINT32 AOpenXRPauseButtons::Update( TUINT32 a_uiButtons, TFLOAT a_fDeltaTime )
{
	constexpr TUINT32 clicks = AVRManager::CONTROLLER_BUTTON_STICK_LEFT | AVRManager::CONTROLLER_BUTTON_STICK_RIGHT;
	const TUINT32     down   = a_uiButtons & clicks;
	TUINT32           result = a_uiButtons & ~clicks;
	// The application-accessible menu button is normally on the LEFT controller
	// (Touch/WMR/Vive), not the runtime-reserved right system button.
	if ( result & AVRManager::CONTROLLER_BUTTON_SELECT )
	{
		result &= ~AVRManager::CONTROLLER_BUTTON_SELECT;
		result |= AVRManager::CONTROLLER_BUTTON_START;
	}
	if ( m_bChord )
	{
		// Swallow both clicks until fully released; never leak a click on exit.
		if ( down == 0 ) Reset();
		else result |= AVRManager::CONTROLLER_BUTTON_START;
		return result;
	}
	if ( down == clicks )
	{
		m_bChord    = TTRUE;
		m_uiPending = m_uiDelivered = 0;
		result |= AVRManager::CONTROLLER_BUTTON_START;
		return result;
	}
	m_uiDelivered &= down;
	if ( m_uiPending && !( down & m_uiPending ) )
	{
		// A quick standalone click still gets one down/up pair.
		result |= m_uiPending;
		m_uiPending = 0;
		m_fWait     = 0.0f;
	}
	if ( down && !m_uiDelivered && !m_uiPending )
	{
		m_uiPending = down;
		m_fWait     = 0.0f;
	}
	if ( m_uiPending )
	{
		if ( std::isfinite( a_fDeltaTime ) ) m_fWait += std::clamp( a_fDeltaTime, 0.0f, 0.1f );
		if ( m_fWait >= 0.2f )
		{
			m_uiDelivered = m_uiPending;
			m_uiPending   = 0;
		}
	}
	return result | m_uiDelivered;
}
