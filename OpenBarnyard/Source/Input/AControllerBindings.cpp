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
	{ AInputCommand_ZLock, Pad::BUTTON_SHOULDER_LEFT_2 },
	{ AInputCommand_Interact, Pad::BUTTON_CONTEXT_4 },
	{ AInputCommand_Use, Pad::BUTTON_CONTEXT_4 },
	{ AInputCommand_Dodge, Pad::BUTTON_CONTEXT_2 },
	{ AInputCommand_ShootAlt, Pad::BUTTON_SHOULDER_RIGHT_1 },
	{ AInputCommand_Reload, Pad::BUTTON_CONTEXT_4 },
	{ AInputCommand_Speed, Pad::BUTTON_STICK_LEFT },
	{ AInputCommand_SuperSpeed, Pad::BUTTON_STICK_LEFT },
	{ AInputCommand_Switch, Pad::BUTTON_CONTEXT_1 },
	{ AInputCommand_Inventory, Pad::BUTTON_CONTEXT_1 },
	{ AInputCommand_Bell, Pad::BUTTON_CONTEXT_1 },
	{ AInputCommand_Horn, Pad::BUTTON_CONTEXT_1 },
	{ AInputCommand_Leave, Pad::BUTTON_CONTEXT_2 },
	{ AInputCommand_Trick1, Pad::BUTTON_SHOULDER_LEFT_1 },
	{ AInputCommand_Trick2, Pad::BUTTON_SHOULDER_RIGHT_1 },
	{ AInputCommand_Trick3, Pad::BUTTON_CONTEXT_4 },
	{ AInputCommand_Trick4, Pad::BUTTON_CONTEXT_1 },
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
	if ( !gameplay ) return a_rOriginal;

	std::array<Doodad, COUNT> destinations;
	std::array<TBOOL, COUNT>  assigned{};
	std::array<TBOOL, COUNT>  occupied{};
	for ( TINT i = 0; i < COUNT; ++i ) destinations[ i ] = BUTTONS[ i ];

	// Dialogue, navigation and generic Context1..4/QTE maps already have native
	// face-button semantics; only meaningful gameplay commands request changes.
	std::vector<Preference> preferences;
	// Car chase calls its pedals Forward/BackwardWithUp, unlike the bike.
	// Do not reinterpret these directional commands in tapper/whack/QTE maps.
	if ( std::strcmp( a_rOwner.GetName().GetString(), "ACarChaseMiniGameState" ) == 0 )
	{
		preferences.push_back( { AInputCommand_ForwardWithUp, Pad::BUTTON_SHOULDER_RIGHT_2 } );
		preferences.push_back( { AInputCommand_BackwardWithUp, Pad::BUTTON_SHOULDER_LEFT_2 } );
		preferences.push_back( { AInputCommand_Shoot, Pad::BUTTON_CONTEXT_3 } ); // handbrake
	}
	preferences.insert( preferences.end(), std::begin( PREFERENCES ), std::end( PREFERENCES ) );
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
