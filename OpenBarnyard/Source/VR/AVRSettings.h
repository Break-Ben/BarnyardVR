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
	TBOOL                        IsModernControls() const { return m_bModernControls; }
	void                         SetModernControls( TBOOL a_bModern );

	TBOOL IsDebugHUDEnabled() const { return m_bDebugHUD; }
	void SetDebugHUDEnabled( TBOOL a_bEnabled );

	// Graphics settings take effect on the next application start.
	inline static constexpr TINT RESOLUTION_MIN  = 50;
	inline static constexpr TINT RESOLUTION_MAX  = 200;
	inline static constexpr TINT RESOLUTION_STEP = 10;
	inline static constexpr TINT MSAA_SAMPLES[]  = { 1, 2, 4, 8 };
	TINT                         GetResolutionPercent() const { return m_iResolutionPercent; }
	void                         SetResolutionPercent( TINT a_iPercent );
	TINT                         GetMSAASamples() const { return m_iMSAASamples; }
	void                         SetMSAASamples( TINT a_iSamples );
	void                         RestoreDefaults();

	// Save only when dirty. A failed save leaves the values dirty for retry.
	TBOOL Save();

private:
	AVRSettings();
	AVRSettings( const AVRSettings& )            = delete;
	AVRSettings& operator=( const AVRSettings& ) = delete;
	void         Load();

	TBOOL m_bModernControls  = TTRUE;
	TINT  m_iSnapTurnDegrees = 0;
	TINT  m_iResolutionPercent = 100;
	TINT  m_iMSAASamples       = 2;
	TBOOL m_bDebugHUD = TFALSE;
	TBOOL m_bDirty           = TFALSE;
};
