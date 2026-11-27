#include "pch.h"
#include "AControllerBindings.h"
#include "VR/AVRSettings.h"
#include <Input/TInputDeviceController.h>

#include <array>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

namespace
{
using Pad                  = TInputDeviceController;
using Doodad               = TInputDevice::Doodad;
constexpr Doodad BUTTONS[] = {
	Pad::BUTTON_CONTEXT_3, Pad::BUTTON_CONTEXT_2, Pad::BUTTON_CONTEXT_4, Pad::BUTTON_CONTEXT_1,
	Pad::BUTTON_SHOULDER_LEFT_1, Pad::BUTTON_SHOULDER_RIGHT_1,
	Pad::BUTTON_SHOULDER_LEFT_2, Pad::BUTTON_SHOULDER_RIGHT_2,
	Pad::BUTTON_STICK_LEFT, Pad::BUTTON_STICK_RIGHT
};
constexpr TINT COUNT = TARRAYSIZE( BUTTONS );

TINT ButtonIndex( Doodad a_iButton )
{
	for ( TINT i = 0; i < COUNT; ++i )
		if ( BUTTONS[ i ] == a_iButton ) return i;
	return -1;
}

TBOOL UsesPrimaryActionButton( const char* a_szMapName )
{
	constexpr const char* maps[] = {
		"ABarnYardPoolMiniGame", "ACookingMiniGame", "ATapperMiniGameState",
		"AWhackARaccMiniGameState", "AGopherShopGame", "AHoneyCollectingGame"
	};
	for ( const char* map : maps )
		if ( std::strcmp( a_szMapName, map ) == 0 ) return TTRUE;
	return TFALSE;
}

struct Preference
{
	AInputCommand command;
	Doodad        button;
};
// Ordered by priority where a game's map shares a physical button between
// state-specific commands. Remap whole groups, preserving those aliases and
// every displaced action rather than overwriting bindings or dropping commands.
constexpr Preference PREFERENCES[] = {
	{ AInputCommand_Jump, Pad::BUTTON_CONTEXT_3 },
	{ AInputCommand_Accelerator, Pad::BUTTON_SHOULDER_RIGHT_2 },
	{ AInputCommand_Brake, Pad::BUTTON_SHOULDER_LEFT_2 },
	{ AInputCommand_Shoot, Pad::BUTTON_SHOULDER_RIGHT_2 },
	// Switch holds the secondary (aimed squirt) launcher until released.
	// ZLock is target lock, not the action that enters squirt aim.
	{ AInputCommand_Switch, Pad::BUTTON_SHOULDER_LEFT_2 },
	{ AInputCommand_Inventory, Pad::BUTTON_CONTEXT_1 },
	{ AInputCommand_Interact, Pad::BUTTON_CONTEXT_2 },
	{ AInputCommand_ZLock, Pad::BUTTON_SHOULDER_LEFT_1 },
	{ AInputCommand_Use, Pad::BUTTON_CONTEXT_4 },
	{ AInputCommand_Dodge, Pad::BUTTON_CONTEXT_2 },
	{ AInputCommand_ShootAlt, Pad::BUTTON_SHOULDER_RIGHT_1 },
	{ AInputCommand_Reload, Pad::BUTTON_CONTEXT_4 },
	{ AInputCommand_Speed, Pad::BUTTON_STICK_LEFT },
	{ AInputCommand_SuperSpeed, Pad::BUTTON_STICK_LEFT },
	{ AInputCommand_Bell, Pad::BUTTON_CONTEXT_1 },
	{ AInputCommand_Horn, Pad::BUTTON_CONTEXT_1 },
	{ AInputCommand_Leave, Pad::BUTTON_CONTEXT_2 },
	// Trick1 is the bike's existing sprint/boost command, not an airborne trick.
	// Trick2 has no bike handler and needs no Modern preference.
	{ AInputCommand_Trick1, Pad::BUTTON_STICK_LEFT },
	{ AInputCommand_Trick3, Pad::BUTTON_SHOULDER_LEFT_1 },
	{ AInputCommand_Trick4, Pad::BUTTON_SHOULDER_RIGHT_1 },
	{ AInputCommand_SelectPrevious, Pad::BUTTON_SHOULDER_LEFT_1 },
	{ AInputCommand_SelectNext, Pad::BUTTON_SHOULDER_RIGHT_1 },
	{ AInputCommand_Look, Pad::BUTTON_STICK_RIGHT },
	{ AInputCommand_Quad, Pad::BUTTON_SHOULDER_LEFT_1 },
	{ AInputCommand_SideStep, Pad::BUTTON_SHOULDER_LEFT_2 }
};

using Entry = std::pair<Doodad, AInputMap::CommandBinding>;
std::unordered_map<const AInputMap*, std::unique_ptr<AControllerBindings::Map>> s_maps;
struct DeviceState
{
	TBOOL                    initialised = TFALSE;
	TBOOL                    modern      = TTRUE;
	std::array<TBOOL, COUNT> blocked{};
	std::array<TBOOL, COUNT> releaseLayout{};
};
std::unordered_map<const TInputDevice*, DeviceState> s_devices;
} // namespace

AControllerBindings::Map& AControllerBindings::Resolve( AInputMap& a_rOwner, Map& a_rOriginal, TBOOL a_bModern )
{
	if ( !a_bModern ) return a_rOriginal;
	const auto found = s_maps.find( &a_rOwner );
	if ( found != s_maps.end() ) return *found->second;

	std::vector<Entry> entries;
	for ( auto it = a_rOriginal.Begin(); it != a_rOriginal.End(); ++it )
		entries.emplace_back( it->GetFirst(), it->GetSecond() );

	const char* mapName = a_rOwner.GetName().GetString();
	if ( std::strcmp( mapName, "VRInventory" ) == 0 )
	{
		auto effective = std::make_unique<Map>( AMemory::GetAllocator( AMemory::POOL_Misc ) );
		for ( const auto& entry : entries )
		{
			// Preserve navigation, keyboard and mouse exactly. Replace gameplay
			// action buttons rather than letting Jump/Interact aliases leak in.
			if ( ButtonIndex( entry.first ) < 0 ) effective->Insert( entry.first, entry.second );
		}
		effective->Insert( Pad::BUTTON_CONTEXT_3, TUINT32( AInputCommand_Interact ) | ( 1u << 16 ) );
		effective->Insert( Pad::BUTTON_CONTEXT_1, TUINT32( AInputCommand_Inventory ) | ( 1u << 16 ) );
		effective->Insert( Pad::BUTTON_CONTEXT_2, TUINT32( AInputCommand_Inventory ) | ( 2u << 16 ) );
		auto* result = effective.get();
		s_maps.emplace( &a_rOwner, std::move( effective ) );
		return *result;
	}

	// Leave/SelectPrevious/etc. also appear in ordinary menus. Do not apply
	// gameplay preferences to those maps just because the command name matches.
	TBOOL gameplay = TFALSE;
	for ( const auto& entry : entries )
	{
		switch ( AInputMap::GetBindingCommand( entry.second ) )
		{
			case AInputCommand_Jump:
			case AInputCommand_Accelerator:
			case AInputCommand_Shoot:
			case AInputCommand_Interact:
			case AInputCommand_Use:
			case AInputCommand_Dodge:
				gameplay = TTRUE;
				break;
			default: break;
		}
	}
	if ( std::strcmp( mapName, "VRMap" ) == 0 ) gameplay = TFALSE;
	std::array<Doodad, COUNT> destinations;
	std::array<TBOOL, COUNT>  assigned{};
	std::array<TBOOL, COUNT>  occupied{};
	for ( TINT i = 0; i < COUNT; ++i ) destinations[ i ] = BUTTONS[ i ];

	// UI maps use Quest A to accept and B to go back. Keep these separate
	// from gameplay, where B is Interact rather than Cancel.
	std::vector<Preference> preferences;
	// The map has its own copy of the dialog bindings; keep its zoom triggers
	// fixed before applying the usual menu accept/cancel preferences.
	if ( std::strcmp( mapName, "VRMap" ) == 0 )
	{
		preferences.push_back( { AInputCommand_ZoomIn, Pad::BUTTON_SHOULDER_LEFT_2 } );
		preferences.push_back( { AInputCommand_ZoomOut, Pad::BUTTON_SHOULDER_RIGHT_2 } );
	}
	// Car chase calls its pedals Forward/BackwardWithUp, unlike the bike.
	// Do not reinterpret these directional commands in tapper/whack/QTE maps.
	if ( std::strcmp( mapName, "ACarChaseMiniGameState" ) == 0 )
	{
		preferences.push_back( { AInputCommand_ForwardWithUp, Pad::BUTTON_SHOULDER_RIGHT_2 } );
		preferences.push_back( { AInputCommand_BackwardWithUp, Pad::BUTTON_SHOULDER_LEFT_2 } );
		preferences.push_back( { AInputCommand_Shoot, Pad::BUTTON_CONTEXT_3 } ); // handbrake
		preferences.push_back( { AInputCommand_Trick1, Pad::BUTTON_CONTEXT_2 } ); // look back
	}
	// Simple activities use A for their primary action, not the
	// combat trigger layout. These overrides precede the generic preferences.
	if ( std::strcmp( mapName, "AHoneyCollectingGame" ) == 0 )
	{
		preferences.push_back( { AInputCommand_Jump, Pad::BUTTON_CONTEXT_4 } ); // swat
		preferences.push_back( { AInputCommand_Shoot, Pad::BUTTON_CONTEXT_3 } );
	}
	else if ( std::strcmp( mapName, "AGateCrashMiniGameState" ) == 0 || std::strcmp( mapName, "AGateCrashMinigameState" ) == 0 )
		preferences.push_back( { AInputCommand_Shoot, Pad::BUTTON_CONTEXT_2 } ); // swing
	else if ( std::strcmp( mapName, "AGolfMinigameState" ) == 0 )
	{
		preferences.push_back( { AInputCommand_Shoot, Pad::BUTTON_CONTEXT_3 } );
		preferences.push_back( { AInputCommand_ZoomOut, Pad::BUTTON_SHOULDER_LEFT_2 } );
		preferences.push_back( { AInputCommand_ZoomIn, Pad::BUTTON_SHOULDER_RIGHT_2 } );
	}
	else if ( std::strcmp( mapName, "ADartMiniGame" ) == 0 )
		preferences.push_back( { AInputCommand_Interact, Pad::BUTTON_CONTEXT_3 } );
	else if ( UsesPrimaryActionButton( mapName ) )
		preferences.push_back( { AInputCommand_Shoot, Pad::BUTTON_CONTEXT_3 } );

	if ( gameplay )
		preferences.insert( preferences.end(), std::begin( PREFERENCES ), std::end( PREFERENCES ) );
	else
	{
		preferences.push_back( { AInputCommand_OK, Pad::BUTTON_CONTEXT_3 } );
		preferences.push_back( { AInputCommand_Cancel, Pad::BUTTON_CONTEXT_2 } );
		preferences.push_back( { AInputCommand_Back, Pad::BUTTON_CONTEXT_2 } );
		preferences.push_back( { AInputCommand_Yes, Pad::BUTTON_CONTEXT_3 } );
		preferences.push_back( { AInputCommand_No, Pad::BUTTON_CONTEXT_2 } );
	}
	for ( const auto& preference : preferences )
	{
		const TINT target = ButtonIndex( preference.button );
		if ( occupied[ target ] ) continue;
		TINT    source = -1;
		TUINT16 rank   = TUINT16( -1 );
		for ( const auto& entry : entries )
		{
			const TINT index = ButtonIndex( entry.first );
			if ( index >= 0 && !assigned[ index ] && AInputMap::GetBindingCommand( entry.second ) == preference.command &&
			     AInputMap::GetBindingIndex( entry.second ) < rank )
			{
				source = index;
				rank   = AInputMap::GetBindingIndex( entry.second );
			}
		}
		if ( source < 0 ) continue;
		for ( TINT displaced = 0; displaced < COUNT; ++displaced )
		{
			if ( destinations[ displaced ] == preference.button )
			{
				std::swap( destinations[ source ], destinations[ displaced ] );
				break;
			}
		}
		assigned[ source ] = TTRUE;
		occupied[ target ] = TTRUE;
	}

	auto effective = std::make_unique<Map>( AMemory::GetAllocator( AMemory::POOL_Misc ) );
	for ( const auto& entry : entries )
	{
		const TINT index = ButtonIndex( entry.first );
		effective->Insert( index >= 0 ? destinations[ index ] : entry.first, entry.second );
	}
	auto* result = effective.get();
	s_maps.emplace( &a_rOwner, std::move( effective ) );
	return *result;
}

void AControllerBindings::Invalidate( const AInputMap* a_pOwner )
{
	s_maps.erase( a_pOwner );
}

AControllerBindings::Map* AControllerBindings::ForEvent( AInputMap& a_rMap, const TInputInterface::InputEvent* a_pEvent )
{
	const TBOOL modern = AVRSettings::GetSingleton().IsModernControls();
	auto*       device = a_pEvent->GetSource();
	if ( !device->IsA( &TGetClass( TInputDeviceController ) ) ) return &a_rMap.GetCommandMap( TFALSE );
	auto& state = s_devices[ device ];
	if ( state.initialised && state.modern != modern )
	{
		// Do not reinterpret a held trigger/button when Confirm changes layouts.
		// Its eventual release uses the old layout, preserving key-up semantics.
		for ( TINT i = 0; i < COUNT; ++i )
		{
			const TBOOL current      = a_pEvent->GetDoodad() == BUTTONS[ i ];
			const TBOOL newlyPressed = current && a_pEvent->GetEventType() == TInputInterface::EVENT_TYPE_GONE_DOWN;
			const TBOOL releasing    = current && a_pEvent->GetEventType() == TInputInterface::EVENT_TYPE_GONE_UP;
			if ( !state.blocked[ i ] && !newlyPressed && ( device->IsDown( BUTTONS[ i ] ) || releasing ) )
			{
				state.blocked[ i ]       = TTRUE;
				state.releaseLayout[ i ] = state.modern;
			}
		}
	}
	state.initialised = TTRUE;
	state.modern      = modern;
	const TINT index  = ButtonIndex( a_pEvent->GetDoodad() );
	if ( index >= 0 && state.blocked[ index ] )
	{
		if ( a_pEvent->GetEventType() != TInputInterface::EVENT_TYPE_GONE_UP ) return TNULL;
		state.blocked[ index ] = TFALSE;
		return &a_rMap.GetCommandMap( state.releaseLayout[ index ] );
	}
	return &a_rMap.GetCommandMap( modern );
}
