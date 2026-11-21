#pragma once

// Port-only settings. Keep these out of AOptions and the original savedata.by layout.
// Loaded on first use; consumers and menus share this one source of truth.
class AVRSettings
{
public:
	static AVRSettings& GetSingleton();

	static constexpr TINT DUMMY_LEVEL_MIN     = 1;
	static constexpr TINT DUMMY_LEVEL_MAX     = 5;
	static constexpr TINT DUMMY_LEVEL_DEFAULT = 3;

	TBOOL IsDummyEnabled() const { return m_bDummyEnabled; }
	TINT  GetDummyLevel() const { return m_iDummyLevel; }
	void  SetDummyEnabled( TBOOL a_bEnabled );
	void  SetDummyLevel( TINT a_iLevel );
	void  RestoreDefaults();

	// Save only when dirty. A failed save leaves the values dirty for retry.
	TBOOL Save();

private:
	AVRSettings();
	AVRSettings( const AVRSettings& )            = delete;
	AVRSettings& operator=( const AVRSettings& ) = delete;
	void         Load();

	TBOOL m_bDummyEnabled = TFALSE;
	TINT  m_iDummyLevel   = DUMMY_LEVEL_DEFAULT;
	TBOOL m_bDirty        = TFALSE;
};
