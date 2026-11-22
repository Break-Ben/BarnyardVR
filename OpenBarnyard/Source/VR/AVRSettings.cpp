#include "pch.h"
#include "AVRSettings.h"
#include "GameInterface/SaveLoadSKU.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

#include <Core/TMemoryDebugOn.h>

namespace
{
std::filesystem::path GetVRSettingsPath()
{
	return std::filesystem::path( SaveLoadSKU::GetSaveDirectory() ) / "BarnyardVR.cfg";
}

std::string Trim( const std::string& a_rcText )
{
	const auto first = a_rcText.find_first_not_of( " \t\r\n" );
	if ( first == std::string::npos )
		return {};
	return a_rcText.substr( first, a_rcText.find_last_not_of( " \t\r\n" ) - first + 1 );
}
} // namespace

AVRSettings& AVRSettings::GetSingleton()
{
	static AVRSettings settings;
	return settings;
}

AVRSettings::AVRSettings()
{
	Load();
}

void AVRSettings::SetSnapTurnDegrees( TINT a_iDegrees )
{
	for ( TINT degrees : SNAP_ANGLES )
	{
		if ( degrees == a_iDegrees && m_iSnapTurnDegrees != degrees )
		{
			m_iSnapTurnDegrees = degrees;
			m_bDirty           = TTRUE;
		}
	}
}

void AVRSettings::SetModernControls( TBOOL a_bModern )
{
	if ( m_bModernControls != a_bModern )
	{
		m_bModernControls = a_bModern;
		m_bDirty          = TTRUE;
	}
}

void AVRSettings::SetDebugHUDEnabled( TBOOL a_bEnabled )
{
	if ( m_bDebugHUD != a_bEnabled )
	{
		m_bDebugHUD = a_bEnabled;
		m_bDirty    = TTRUE;
	}
}

void AVRSettings::SetResolutionPercent( TINT a_iPercent )
{
	if ( a_iPercent >= RESOLUTION_MIN && a_iPercent <= RESOLUTION_MAX &&
	     a_iPercent % RESOLUTION_STEP == 0 && m_iResolutionPercent != a_iPercent )
	{
		m_iResolutionPercent = a_iPercent;
		m_bDirty             = TTRUE;
	}
}

void AVRSettings::SetMSAASamples( TINT a_iSamples )
{
	for ( TINT samples : MSAA_SAMPLES )
	{
		if ( samples == a_iSamples && m_iMSAASamples != samples )
		{
			m_iMSAASamples = samples;
			m_bDirty       = TTRUE;
		}
	}
}

void AVRSettings::RestoreDefaults()
{
	SetSnapTurnDegrees( 0 );
	SetModernControls( TTRUE );
	SetResolutionPercent( 100 );
	SetMSAASamples( 2 );
	SetDebugHUDEnabled( TFALSE );
}

void AVRSettings::Load()
{
	const auto path  = GetVRSettingsPath().string();
	FILE*      pFile = fopen( path.c_str(), "r" );
	if ( !pFile ) return; // First run uses defaults.

	char line[ 256 ];
	while ( fgets( line, sizeof( line ), pFile ) )
	{
		// Discard an overlong line as a whole, not as multiple settings.
		const std::string text( line );
		if ( text.find( '\n' ) == std::string::npos && !feof( pFile ) )
		{
			int c;
			while ( ( c = fgetc( pFile ) ) != '\n' && c != EOF ) {}
			continue;
		}
		const auto equals = text.find( '=' );
		const auto key    = Trim( text.substr( 0, equals ) );
		if ( key.empty() || key[ 0 ] == '#' || equals == std::string::npos ) continue;
		const auto value  = Trim( text.substr( equals + 1 ) );
		char*      end    = TNULL;
		errno             = 0;
		const long number = strtol( value.c_str(), &end, 10 );
		if ( errno == ERANGE || end == value.c_str() || *end != '\0' ) continue;

		if ( key == "ModernControls" && ( number == 0 || number == 1 ) )
			SetModernControls( number != 0 );
		else if ( key == "DebugHUD" && ( number == 0 || number == 1 ) )
			SetDebugHUDEnabled( number != 0 );
		else if ( key == "SnapTurnDegrees" && number >= 0 && number <= 90 )
			SetSnapTurnDegrees( TINT( number ) );
		else if ( key == "ResolutionPercent" && number >= RESOLUTION_MIN && number <= RESOLUTION_MAX )
			SetResolutionPercent( TINT( number ) );
		else if ( key == "MSAASamples" && number >= 1 && number <= 8 )
			SetMSAASamples( TINT( number ) );
	}
	fclose( pFile );
	m_bDirty = TFALSE;
}

TBOOL AVRSettings::Save()
{
	if ( !m_bDirty ) return TTRUE;

	const auto      path = GetVRSettingsPath();
	std::error_code error;
	std::filesystem::create_directories( path.parent_path(), error );
	if ( error )
	{
		TWARN( "Couldn't create VR settings directory: %s\n", error.message().c_str() );
		return TFALSE;
	}
	const std::string destination = path.string();
	const std::string temporary   = destination + ".tmp";
	FILE*             pFile       = fopen( temporary.c_str(), "w" );
	if ( !pFile ) return TFALSE;
	fprintf( pFile, "# BarnyardVR settings. SnapTurnDegrees: 0 (disabled), 15, 30, 45, 60, 90.\n" );
	fprintf( pFile, "SnapTurnDegrees=%d\nModernControls=%d\n", m_iSnapTurnDegrees, m_bModernControls ? 1 : 0 );
	fprintf( pFile, "# VR graphics: restart required. ResolutionPercent: 50-200 in steps of 5; 100 is the OpenXR recommendation.\n" );
	fprintf( pFile, "# The -vrscale command-line option overrides ResolutionPercent.\n" );
	fprintf( pFile, "# MSAASamples: 1 (off), 2, 4, 8. Unsupported counts fall back to a lower level.\n" );
	fprintf( pFile, "ResolutionPercent=%d\nMSAASamples=%d\n", m_iResolutionPercent, m_iMSAASamples );
	fprintf( pFile, "DebugHUD=%d\n", m_bDebugHUD ? 1 : 0 );
	TBOOL written = fflush( pFile ) == 0 && ferror( pFile ) == 0;
	if ( fclose( pFile ) != 0 ) written = TFALSE;

	// Replace only after a complete write; never truncate the previous config.
#if defined( TOSHI_SKU_WINDOWS ) && !defined( TOSHI_SKU_WASM )
	if ( written ) written = MoveFileExA( temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH ) != 0;
#else
	if ( written ) written = std::rename( temporary.c_str(), destination.c_str() ) == 0;
#endif
	if ( !written )
	{
		std::remove( temporary.c_str() );
		TWARN( "Couldn't save VR settings: %s\n", destination.c_str() );
		return TFALSE;
	}
	m_bDirty = TFALSE;
	return TTRUE;
}
