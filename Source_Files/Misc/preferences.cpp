/*

	Copyright (C) 1991-2001 and beyond by Bungie Studios, Inc.
	and the "Aleph One" developers.
 
	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	This license is contained in the file "COPYING",
	which is included with this source code; it is available online at
	http://www.gnu.org/licenses/gpl.html

Feb 5, 2002 (Br'fin (Jeremy Parsons)):
	Default to keyboard and mouse control under Carbon
	for there are no InputSprockets
*/

/*
 *  preferences.cpp - Preferences handling
 *
 
Aug 19, 2001 (Ian Rickard):
	Tweaks and changes for dithering and abstracted bit depths.
 
Aug 21, 2001 (Ian Rickard):
	R.I.P. Valkyrie

 */

#include "cseries.h"
#include "FileHandler.h"

#include "map.h"
#include "shell.h" /* For the screen_mode structure */
#include "interface.h"
#include "mysound.h"
#include "music.h"
#include "ISp_Support.h" /* BT: Added April 16, 2000 for Input Sprocket Support */

#include "preferences.h"
#include "preferences_private.h" // ZZZ: added 23 Oct 2001 for sharing of dialog item ID's with SDL.
#include "wad.h"
#include "wad_prefs.h"
#include "game_errors.h"
#include "network.h" // for _ethernet, etc.
#include "find_files.h"
#include "game_wad.h" // for set_map_file
#include "screen.h"
#include "fades.h"
#include "extensions.h"

#include "tags.h"

#include <string.h>
#include <stdlib.h>


// Global preferences data
struct graphics_preferences_data *graphics_preferences = NULL;
struct serial_number_data *serial_preferences = NULL;
struct network_preferences_data *network_preferences = NULL;
struct player_preferences_data *player_preferences = NULL;
struct input_preferences_data *input_preferences = NULL;
struct sound_manager_parameters *sound_preferences = NULL;
struct environment_preferences_data *environment_preferences = NULL;

// LP: fake portable-files stuff
#ifdef mac
inline short memory_error() {return MemError();}
#else
inline short memory_error() {return 0;}
#endif

// Prototypes
static void *get_sound_pref_data(void);
static void *get_graphics_pref_data(void);
static void default_graphics_preferences(void *prefs);
static bool validate_graphics_preferences(void *prefs);
static void default_serial_number_preferences(void *prefs);
static bool validate_serial_number_preferences(void *prefs);
static void default_network_preferences(void *prefs);
static bool validate_network_preferences(void *prefs);
static void *get_player_pref_data(void);
static void default_player_preferences(void *prefs);
static bool validate_player_preferences(void *prefs);
static void *get_input_pref_data(void);
static void default_input_preferences(void *prefs);
static bool validate_input_preferences(void *prefs);
static void *get_environment_pref_data(void);
static void default_environment_preferences(void *prefs);
static bool validate_environment_preferences(void *prefs);
static bool ethernet_active(void);
static void get_name_from_system(unsigned char *name);

// Include platform-specific file
#if defined(mac)
#include "preferences_macintosh.h"
#elif defined(SDL)
#include "preferences_sdl.h"
#endif


/*
 *  Initialize preferences (load from file or setup defaults)
 */

void initialize_preferences(
	void)
{
	OSErr err;

	if(!w_open_preferences_file(getcstr(temporary, strFILENAMES, filenamePREFERENCES),
		_typecode_preferences))
	{
		/* Major memory error.. */
		alert_user(fatalError, strERRORS, outOfMemory, memory_error());
	}

	if(error_pending())
	{
		short type;
		
		char Name[256];
		memcpy(Name,temporary,256);
		
		err= get_game_error(&type);
		if (type != noErr)
			dprintf("Preferences Init Error: %d type: %d prefs name: %s", err, type, Name);
		set_game_error(systemError, noErr);
	}
		
	/* If we didn't open, we initialized.. */
	graphics_preferences= (struct graphics_preferences_data *)get_graphics_pref_data();
	player_preferences= (struct player_preferences_data *)get_player_pref_data();
	input_preferences= (struct input_preferences_data *)get_input_pref_data();
	sound_preferences= (struct sound_manager_parameters *)get_sound_pref_data();
	serial_preferences= (struct serial_number_data *)w_get_data_from_preferences(
		prefSERIAL_TAG,sizeof(struct serial_number_data),
		default_serial_number_preferences,
		validate_serial_number_preferences);
	network_preferences= (struct network_preferences_data *)w_get_data_from_preferences(
		prefNETWORK_TAG, sizeof(struct network_preferences_data),
		default_network_preferences,
		validate_network_preferences);
	environment_preferences= (struct environment_preferences_data *)get_environment_pref_data();
}


/*
 *  Write preferences to file
 */

void write_preferences(
	void)
{
	OSErr err;
	w_write_preferences_file();

	if(error_pending())
	{
		short type;
		
		err= get_game_error(&type);
		dprintf("Preferences Write Error: %d type: %d", err, type);
		set_game_error(systemError, noErr);
	}
}


/*
 *  Get prefs data from prefs file (or defaults)
 */

static void *get_graphics_pref_data(
	void)
{
	return w_get_data_from_preferences(
		prefGRAPHICS_TAG, sizeof(struct graphics_preferences_data),
		default_graphics_preferences,
		validate_graphics_preferences);
}

static void *get_player_pref_data(
	void)
{
	return w_get_data_from_preferences(
		prefPLAYER_TAG,sizeof(struct player_preferences_data),
		default_player_preferences,
		validate_player_preferences);
}

static void *get_sound_pref_data(
	void)
{
	return w_get_data_from_preferences(
		prefSOUND_TAG,sizeof(struct sound_manager_parameters),
		default_sound_manager_parameters,
		NULL);
}

static void *get_input_pref_data(
	void)
{
	return w_get_data_from_preferences(
		prefINPUT_TAG,sizeof(struct input_preferences_data),
		default_input_preferences,
		validate_input_preferences);
}

static void *get_environment_pref_data(
	void)
{
	return w_get_data_from_preferences(
		prefENVIRONMENT_TAG,sizeof(struct environment_preferences_data), 
		default_environment_preferences,
		validate_environment_preferences);
}


/*
 *  Setup default preferences
 */

static void default_graphics_preferences(
	void *prefs)
{
	struct graphics_preferences_data *preferences=(struct graphics_preferences_data *)prefs;

	preferences->screen_mode.gamma_level= DEFAULT_GAMMA_LEVEL;

#ifdef mac
	preferences->device_spec.slot= NONE;
	preferences->device_spec.flags= deviceIsColor;
	preferences->device_spec.bit_depth= 32;
	preferences->device_spec.width= 640;
	preferences->device_spec.height= 480;
	
	preferences->screen_mode.size = _100_percent;
	preferences->screen_mode.fullscreen = false;
	preferences->screen_mode.high_resolution = true;
	
	preferences->refresh_frequency = DEFAULT_MONITOR_REFRESH_FREQUENCY;
	
	if (hardware_acceleration_code(&preferences->device_spec) == _opengl_acceleration)
	{
		preferences->screen_mode.acceleration = _opengl_acceleration;
		// IR change: dithering/abstracted bit depths
		preferences->screen_mode.display_mode = kScreenDepth32;
	}
	else
	{
		preferences->screen_mode.acceleration = _no_acceleration;
<<<<<<< preferences.cpp
		// IR change: dithering/abstracted bit depths
		preferences->screen_mode.display_mode = kScreenDepth32;
=======
		preferences->screen_mode.bit_depth = 16;
>>>>>>> 1.31
	}
	
#else
	preferences->screen_mode.size = _100_percent;
	preferences->screen_mode.acceleration = _no_acceleration;
	preferences->screen_mode.high_resolution = true;
	preferences->screen_mode.fullscreen = false;
	// IR change: dithering/abstracted bit depths
	preferences->screen_mode.display_mode = kScreenDepth16;
#endif
	
	preferences->screen_mode.draw_every_other_line= false;
	
	OGL_SetDefaults(preferences->OGL_Configure);
}

static void default_serial_number_preferences(
	void *prefs)
{
	memset(prefs, 0, sizeof(struct serial_number_data));
}

static void default_network_preferences(
	void *prefs)
{
	struct network_preferences_data *preferences=(struct network_preferences_data *)prefs;

	preferences->type= _ethernet;

	preferences->allow_microphone = true;
	preferences->game_is_untimed = false;
	preferences->difficulty_level = 2;
	preferences->game_options =	_multiplayer_game | _ammo_replenishes | _weapons_replenish
		| _specials_replenish |	_monsters_replenish | _burn_items_on_death | _suicide_is_penalized 
		| _force_unique_teams | _live_network_stats;
	preferences->time_limit = 10 * TICKS_PER_SECOND * 60;
	preferences->kill_limit = 10;
	preferences->entry_point= 0;
	preferences->game_type= _game_of_kill_monsters;
}

static void default_player_preferences(
	void *preferences)
{
	struct player_preferences_data *prefs=(struct player_preferences_data *)preferences;

	obj_clear(*prefs);

#ifdef mac
	GetDateTime(&prefs->last_time_ran);
#endif
	prefs->difficulty_level= 2;
	get_name_from_system(prefs->name);
	
	// LP additions for new fields:
	
	prefs->ChaseCam.Behind = 1536;
	prefs->ChaseCam.Upward = 0;
	prefs->ChaseCam.Rightward = 0;
	prefs->ChaseCam.Flags = 0;
	
	prefs->Crosshairs.Thickness = 2;
	prefs->Crosshairs.FromCenter = 8;
	prefs->Crosshairs.Length = 16;
	prefs->Crosshairs.Color = rgb_white;
}

static void default_input_preferences(
	void *prefs)
{
	struct input_preferences_data *preferences=(struct input_preferences_data *)prefs;

#if defined(TARGET_API_MAC_CARBON)
	// JTP: No ISP, go with default option
	preferences->input_device= _mouse_yaw_pitch;
#else
  	preferences->input_device= _keyboard_or_game_pad;
#endif
	set_default_keys(preferences->keycodes, _standard_keyboard_setup);
	
	// LP addition: set up defaults for modifiers:
	// interchange run and walk, but don't interchange swim and sink.
	preferences->modifiers = _inputmod_interchange_run_walk;
}

static void default_environment_preferences(
	void *preferences)
{
	struct environment_preferences_data *prefs= (struct environment_preferences_data *)preferences;

	obj_set(*prefs, NONE);

	FileSpecifier DefaultFile;
	
	get_default_map_spec(DefaultFile);
	prefs->map_checksum= read_wad_file_checksum(DefaultFile);
#ifdef mac
	obj_copy(prefs->map_file, DefaultFile.GetSpec());
#else
	strncpy(prefs->map_file, DefaultFile.GetPath(), 256);
	prefs->map_file[255] = 0;
#endif
	
	get_default_physics_spec(DefaultFile);
	prefs->physics_checksum= read_wad_file_checksum(DefaultFile);
#ifdef mac
	obj_copy(prefs->physics_file, DefaultFile.GetSpec());
#else
	strncpy(prefs->physics_file, DefaultFile.GetPath(), 256);
	prefs->physics_file[255] = 0;
#endif
	
	get_default_shapes_spec(DefaultFile);
	
	prefs->shapes_mod_date = DefaultFile.GetDate();
#ifdef mac
	obj_copy(prefs->shapes_file, DefaultFile.GetSpec());
#else
	strncpy(prefs->shapes_file, DefaultFile.GetPath(), 256);
	prefs->shapes_file[255] = 0;
#endif

	get_default_sounds_spec(DefaultFile);
	
	prefs->sounds_mod_date = DefaultFile.GetDate();
#ifdef mac
	obj_copy(prefs->sounds_file, DefaultFile.GetSpec());
#else
	strncpy(prefs->sounds_file, DefaultFile.GetPath(), 256);
	prefs->sounds_file[255] = 0;
#endif

#ifdef SDL
	get_default_theme_spec(DefaultFile);
	strncpy(prefs->theme_dir, DefaultFile.GetPath(), 256);
	prefs->theme_dir[255] = 0;
#endif
}


/*
 *  Validate preferences
 */

static bool validate_graphics_preferences(
	void *prefs)
{
	struct graphics_preferences_data *preferences=(struct graphics_preferences_data *)prefs;
	bool changed= false;

	// Fix bool options
	preferences->screen_mode.high_resolution = !!preferences->screen_mode.high_resolution;
	preferences->screen_mode.fullscreen = !!preferences->screen_mode.fullscreen;
	preferences->screen_mode.draw_every_other_line = !!preferences->screen_mode.draw_every_other_line;

	if(preferences->screen_mode.gamma_level<0 || preferences->screen_mode.gamma_level>=NUMBER_OF_GAMMA_LEVELS)
	{
		preferences->screen_mode.gamma_level= DEFAULT_GAMMA_LEVEL;
		changed= true;
	}

#ifdef mac
// IR change: R.I.P. Valkyrie
#if OBSOLETE
	if (preferences->screen_mode.acceleration==_valkyrie_acceleration)
	{
		if (hardware_acceleration_code(&preferences->device_spec) != _valkyrie_acceleration)
		{
			preferences->screen_mode.size= _100_percent;
			// IR change: dithering/abstracted bit depths
			preferences->screen_mode.display_mode = kScreenDepth8;
			preferences->screen_mode.high_resolution = false;
			preferences->screen_mode.acceleration = _no_acceleration;
			changed= true;
		} else {
			if(preferences->screen_mode.high_resolution)
			{
				preferences->screen_mode.high_resolution= false;
				changed= true;
			}
			
			// IR change: dithering/abstracted bit depths
			if(preferences->screen_mode.display_mode != kScreenDepth16)
			{
				preferences->screen_mode.display_mode= kScreenDepth16;
				changed= true;
			}
			
			if(preferences->screen_mode.draw_every_other_line)
			{
				preferences->screen_mode.draw_every_other_line= false;
				changed= true;
			}
		}
	}
#endif

	// IR change: dithering/abstracted bit depths
	if (preferences->screen_mode.display_mode==kScreenDepth32 && !machine_supports_32bit(&preferences->device_spec))
	{
		preferences->screen_mode.display_mode= kScreenDepthDithered32;
		changed= true;
	}

	// IR addition: dithering
	if (preferences->screen_mode.display_mode==kScreenDepthDithered32 && !machine_supports_dithered_32bit(&preferences->device_spec))
	{
		preferences->screen_mode.display_mode= kScreenDepth16;
		changed= true;
	}

	/* Don't change out of 16 bit if we are in valkyrie mode. */	
	// IR removed: R.I.P. Valkyrie
//	if (preferences->screen_mode.acceleration!=_valkyrie_acceleration && 
	// IR change: dithering/abstracted bit depths
	if (preferences->screen_mode.display_mode==kScreenDepth16 && !machine_supports_16bit(&preferences->device_spec))
	{
		preferences->screen_mode.display_mode= kScreenDepth8;
		changed= true;
	}
#else
	// OpenGL requires at least 16 bit color depth
	// IR change: dithering/abstracted bit depths
	if (preferences->screen_mode.acceleration == _opengl_acceleration && preferences->screen_mode.bit_depth == 8)
	{
		preferences->screen_mode.display_mode= kScreenDepth16;
		changed= true;
	}
#endif

	return changed;
}

static bool validate_serial_number_preferences(
	void *prefs)
{
	(void) (prefs);
	return false;
}

static bool validate_network_preferences(
	void *preferences)
{
	struct network_preferences_data *prefs=(struct network_preferences_data *)preferences;
	bool changed= false;

	// Fix bool options
	prefs->allow_microphone = !!prefs->allow_microphone;
	prefs->game_is_untimed = !!prefs->game_is_untimed;

	if(prefs->type<0||prefs->type>_ethernet)
	{
		if(ethernet_active())
		{
			prefs->type= _ethernet;
		} else {
			prefs->type= _localtalk;
		}
		changed= true;
	}
	
	if(prefs->game_is_untimed != true && prefs->game_is_untimed != false)
	{
		prefs->game_is_untimed= false;
		changed= true;
	}

	if(prefs->allow_microphone != true && prefs->allow_microphone != false)
	{
		prefs->allow_microphone= true;
		changed= true;
	}

	if(prefs->game_type<0 || prefs->game_type >= NUMBER_OF_GAME_TYPES)
	{
		prefs->game_type= _game_of_kill_monsters;
		changed= true;
	}
	
	return changed;
}

static bool validate_player_preferences(
	void *preferences)
{
	struct player_preferences_data *prefs=(struct player_preferences_data *)preferences;

	// Fix bool options
	prefs->background_music_on = !!prefs->background_music_on;

	return false;
}

static bool validate_input_preferences(
	void *prefs)
{
	(void) (prefs);
	return false;
}

static bool validate_environment_preferences(
	void *prefs)
{
	(void) (prefs);
	return false;
}


/*
 *  Load the environment
 */

/* Load the environment.. */
void load_environment_from_preferences(
	void)
{
	FileSpecifier File;
	struct environment_preferences_data *prefs= environment_preferences;

#ifdef mac
	File.SetSpec(prefs->map_file);
#else
	File = prefs->map_file;
#endif
	if (File.Exists()) {
		set_map_file(File);
	} else {
		/* Try to find the checksum */
		if(find_wad_file_that_has_checksum(File,
			_typecode_scenario, strPATHS, prefs->map_checksum))	{
			set_map_file(File);
		} else {
			set_to_default_map();
		}
	}

#ifdef mac
	File.SetSpec(prefs->physics_file);
#else
	File = prefs->physics_file;
#endif
	if (File.Exists()) {
		set_physics_file(File);
		import_definition_structures();
	} else {
		if(find_wad_file_that_has_checksum(File,
			_typecode_physics, strPATHS, prefs->physics_checksum)) {
			set_physics_file(File);
			import_definition_structures();
		} else {
			/* Didn't find it.  Don't change them.. */
		}
	}
	
#ifdef mac
	File.SetSpec(prefs->shapes_file);
#else
	File = prefs->shapes_file;
#endif
	if (File.Exists()) {
		open_shapes_file(File);
	} else {
		if(find_file_with_modification_date(File,
			_typecode_shapes, strPATHS, prefs->shapes_mod_date))
		{
			open_shapes_file(File);
		} else {
			/* What should I do? */
		}
	}

#ifdef mac
	File.SetSpec(prefs->sounds_file);
#else
	File = prefs->sounds_file;
#endif
	if (File.Exists()) {
		open_sound_file(File);
	} else {
		if(find_file_with_modification_date(File,
			_typecode_sounds, strPATHS, prefs->sounds_mod_date)) {
			open_sound_file(File);
		} else {
			/* What should I do? */
		}
	}
}


// LP addition: get these from the preferences data
ChaseCamData& GetChaseCamData() {return player_preferences->ChaseCam;}
CrosshairData& GetCrosshairData() {return player_preferences->Crosshairs;}
OGL_ConfigureData& Get_OGL_ConfigureData() {return graphics_preferences->OGL_Configure;}

// LP addition: modification of Josh Elsasser's dont-switch-weapons patch
// so as to access preferences stuff here
bool dont_switch_to_new_weapon() {
	return TEST_FLAG(input_preferences->modifiers,_inputmod_dont_switch_to_new_weapon);
}
