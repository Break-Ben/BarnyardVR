#pragma once
#include <Toshi/Typedefs.h>

// Port-only settings. Keep these out of AOptions and the original savedata.by layout.
// Loaded on first use; consumers and menus share this one source of truth.
class AVRSettings
{
public:
	static AVRSettings& GetSingleton();

	// Zero disables snapping. Stored as degrees, never as a menu-row index.
	inline static constexpr TINT SNAP_ANGLES[] = { 0, 15, 30, 45, 60, 90 };
	TINT                         GetSnapTurnDegrees() const { return m_iSnapTurnDegrees; }
	void                         SetSnapTurnDegrees( TINT a_iDegrees );
	void                         RestoreDefaults();

	// Save only when dirty. A failed save leaves the values dirty for retry.
	TBOOL Save();

private:
	AVRSettings();
	AVRSettings( const AVRSettings& )            = delete;
	AVRSettings& operator=( const AVRSettings& ) = delete;
	void         Load();

	TINT  m_iSnapTurnDegrees = 0;
	TBOOL m_bDirty           = TFALSE;
};
