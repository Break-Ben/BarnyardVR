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

void AVRSettings::SetDummyEnabled( TBOOL a_bEnabled )
{
	if ( m_bDummyEnabled != a_bEnabled )
	{
		m_bDummyEnabled = a_bEnabled;
		m_bDirty        = TTRUE;
	}
}

void AVRSettings::SetDummyLevel( TINT a_iLevel )
{
	if ( a_iLevel < DUMMY_LEVEL_MIN ) a_iLevel = DUMMY_LEVEL_MIN;
	if ( a_iLevel > DUMMY_LEVEL_MAX ) a_iLevel = DUMMY_LEVEL_MAX;
	if ( m_iDummyLevel != a_iLevel )
	{
		m_iDummyLevel = a_iLevel;
		m_bDirty      = TTRUE;
	}
}

void AVRSettings::RestoreDefaults()
{
	SetDummyEnabled( TFALSE );
	SetDummyLevel( DUMMY_LEVEL_DEFAULT );
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

		if ( key == "DummyEnabled" && ( number == 0 || number == 1 ) )
			SetDummyEnabled( number != 0 );
		else if ( key == "DummyLevel" && number >= DUMMY_LEVEL_MIN && number <= DUMMY_LEVEL_MAX )
			SetDummyLevel( TINT( number ) );
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
	fprintf( pFile, "# BarnyardVR settings. Dummy options have no gameplay effect.\n" );
	fprintf( pFile, "DummyEnabled=%d\nDummyLevel=%d\n", m_bDummyEnabled ? 1 : 0, m_iDummyLevel );
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
