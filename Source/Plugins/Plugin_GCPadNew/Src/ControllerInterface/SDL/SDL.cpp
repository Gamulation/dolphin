#include "../ControllerInterface.h"

#ifdef CIFACE_USE_SDL

#include "SDL.h"

#ifdef _WIN32
	#pragma comment(lib, "SDL.1.3.lib")
#endif

// temp for debuggin
//#include <fstream>

namespace ciface
{
namespace SDL
{

void Init( std::vector<ControllerInterface::Device*>& devices )
{	
	if ( SDL_Init( SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC ) >= 0 )
    {
		// joysticks
		for( int i = 0; i < SDL_NumJoysticks(); ++i )
		{
			SDL_Joystick* dev = SDL_JoystickOpen( i );
			if ( dev )
			{
				Joystick* js = new Joystick( dev, i );
				// only add if it has some inputs/outputs
				if ( js->Inputs().size() || js->Outputs().size() )
					devices.push_back( js );
				else
					delete js;
			}
		}
    }
}

Joystick::Joystick( SDL_Joystick* const joystick, const unsigned int index ) : m_joystick(joystick), m_index(index)
{
	// get buttons
	for ( int i = 0; i < SDL_JoystickNumButtons( m_joystick ); ++i )
	{
		inputs.push_back( new Button( i ) );
	}
	
	// get hats
	for ( int i = 0; i < SDL_JoystickNumHats( m_joystick ); ++i )
	{
		// each hat gets 4 input instances associated with it, (up down left right)
		for ( unsigned int d = 0; d < 4; ++d )
			inputs.push_back( new Hat( i, d ) );
	}

	// get axes
	for ( int i = 0; i < SDL_JoystickNumAxes( m_joystick ); ++i )
	{
		// each axis gets a negative and a positive input instance associated with it
		inputs.push_back( new Axis( i, -32768 ) );
		inputs.push_back( new Axis( i, 32767 ) );
	}
	m_haptic = SDL_HapticOpenFromJoystick( m_joystick );
	// try to get supported ff effects
	if ( m_haptic  )
	{

		//SDL_HapticSetGain( m_haptic, 1000 );
		//SDL_HapticSetAutocenter( m_haptic, 0 );

		const unsigned int supported_effects = SDL_HapticQuery( m_haptic ); // use this later

		// constant effect
		if ( supported_effects & SDL_HAPTIC_CONSTANT )
		{
			outputs.push_back( new ConstantEffect( m_state_out.size() ) );
			m_state_out.push_back( EffectIDState() );
		}

		// ramp effect
		if ( supported_effects & SDL_HAPTIC_RAMP )
		{
			outputs.push_back( new RampEffect( m_state_out.size() ) );
			m_state_out.push_back( EffectIDState() );
		}
	}

}

Joystick::~Joystick()
{
	if ( m_haptic )
	{	
		// stop/destroy all effects
		//SDL_HapticStopAll( m_haptic ); // ControllerInterface handles this
		std::vector<EffectIDState>::iterator i = m_state_out.begin(),
			e = m_state_out.end();
		for ( ; i!=e; ++i )
			if ( i->id != -1 )
				SDL_HapticDestroyEffect( m_haptic, i->id );
		// close haptic first
		SDL_HapticClose( m_haptic );
	}

	// close joystick
	SDL_JoystickClose( m_joystick );
}

//std::string Joystick::Effect::GetName() const
//{
//	return haptic_named_effects[m_index].name;
//}

std::string Joystick::ConstantEffect::GetName() const
{
	return "Constant";
}

std::string Joystick::RampEffect::GetName() const
{
	return "Ramp";
}

void Joystick::ConstantEffect::SetState( const ControlState state, Joystick::EffectIDState* const effect )
{
	if ( state )
	{
		//debuggin here ...
		//memset( &effect->effect, 0, sizeof(effect->effect) );
		effect->effect.type = SDL_HAPTIC_CONSTANT;
		effect->effect.constant.length = SDL_HAPTIC_INFINITY;
		//effect->effect.constant.attack_length = 3000;
		//effect->effect.constant.direction.type = SDL_HAPTIC_CARTESIAN;
		//effect->effect
		//effect->effect.constant.button = 0xFFFFFFFF;
	}
	else
		effect->effect.type = 0;

	Sint16 old = effect->effect.constant.level;
	effect->effect.constant.level = state * 0x7FFF;
	if ( old != effect->effect.constant.level )
		effect->changed = true;
}

void Joystick::RampEffect::SetState( const ControlState state, Joystick::EffectIDState* const effect )
{
	if ( state )
	{
		effect->effect.type = SDL_HAPTIC_RAMP;
		effect->effect.ramp.length = SDL_HAPTIC_INFINITY;
	}
	else
		effect->effect.type = 0;
	
	Sint16 old = effect->effect.ramp.start;
	effect->effect.ramp.start = state * 0x7FFF;
	if ( old != effect->effect.ramp.start )
		effect->changed = true;
}

ControlState Joystick::GetInputState(const ControllerInterface::Device::Input* input)
{
	return ((Input*)input)->GetState( m_joystick );
}

void Joystick::SetOutputState(const ControllerInterface::Device::Output* output, const ControlState state)
{
	((Output*)output)->SetState( state, &m_state_out[ ((Output*)output)->m_index ] );
}

bool Joystick::UpdateInput()
{
	SDL_JoystickUpdate();		// each joystick is doin this, o well
	
	return true;
}

bool Joystick::UpdateOutput()
{
	std::vector<EffectIDState>::iterator i = m_state_out.begin(),
		e = m_state_out.end();
	for ( ; i!=e; ++i )
		if ( i->changed )	// if SetState was called on this output
		{
			if ( -1 == i->id )	// effect isn't currently uploaded
			{
				if ( i->effect.type )		// if outputstate is >0  this would be true
					if ( (i->id = SDL_HapticNewEffect( m_haptic, &i->effect )) > -1 )	// upload the effect
					{
						//std::ofstream file( "SDLgood.txt" );

						/*if ( 0 == */SDL_HapticRunEffect( m_haptic, i->id, 1 );//)	// run the effect
						//	file << "all good";
						//else
						//	file << "not good";
						//file.close();

					}
					else
					{
						// DEBUG

						//std::ofstream file( "SDLerror.txt" );
						//file << SDL_GetError();
						//file.close();

					}
			}
			else	// effect is already uploaded
			{
				if ( i->effect.type )	// if ouputstate >0
					SDL_HapticUpdateEffect( m_haptic, i->id, &i->effect );	// update the effect
				else
				{
					SDL_HapticStopEffect( m_haptic, i->id );	// else, stop and remove the effect
					SDL_HapticDestroyEffect( m_haptic, i->id );
					i->id = -1;	// mark it as not uploaded
				}
			}

			i->changed = false;
		}
	return true;
}

std::string Joystick::GetName() const
{
	return SDL_JoystickName( m_index );
}

std::string Joystick::GetSource() const
{
	return "SDL";
}

int Joystick::GetId() const
{
	return m_index;
}

std::string Joystick::Button::GetName() const
{
	std::ostringstream ss;
	ss << "Button " << m_index;
	return ss.str();
}

std::string Joystick::Axis::GetName() const
{
	std::ostringstream ss;
	ss << "Axis " << m_index << ( m_range>0 ? '+' : '-' );
	return ss.str();
}

std::string Joystick::Hat::GetName() const
{
	std::ostringstream ss;
	ss << "Hat " << m_index << ' ' << "NESW"[m_direction];
	return ss.str();
}

ControlState Joystick::Button::GetState( SDL_Joystick* const js ) const
{
	return SDL_JoystickGetButton( js, m_index );
}

ControlState Joystick::Axis::GetState( SDL_Joystick* const js ) const
{
	return std::max( 0.0f, ControlState(SDL_JoystickGetAxis( js, m_index )) / m_range );
}

ControlState Joystick::Hat::GetState( SDL_Joystick* const js ) const
{
	return (SDL_JoystickGetHat( js, m_index ) & ( 1 << m_direction )) > 0;
}



}
}

#endif
