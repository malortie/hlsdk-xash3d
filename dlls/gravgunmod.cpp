#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "gravgunmod.h"
#include "player.h"
#include "coop_util.h"


cvar_t cvar_allow_gravgun = { "mp_allow_gravgun","1", FCVAR_SERVER };
cvar_t cvar_allow_ar2 = { "mp_allow_ar2","0", FCVAR_SERVER };
cvar_t cvar_ar2_mp5 = { "mp_ar2_mp5","0", FCVAR_SERVER };
cvar_t cvar_ar2_balls = { "mp_ar2_balls","0", FCVAR_SERVER };
cvar_t cvar_ar2_bullets = { "mp_ar2_bullets","0", FCVAR_SERVER };
cvar_t cvar_wresptime = { "mp_wresptime","20", FCVAR_SERVER };
cvar_t cvar_iresptime = { "mp_iresptime","30", FCVAR_SERVER };
cvar_t cvar_gibtime = { "mp_gibtime","250", FCVAR_SERVER };
cvar_t cvar_hgibcount = { "mp_hgibcount","12", FCVAR_SERVER };
cvar_t cvar_agibcount = { "mp_agibcount","8", FCVAR_SERVER };
cvar_t mp_gravgun_players = { "mp_gravgun_players", "0", FCVAR_SERVER };

cvar_t mp_fixhornetbug = { "mp_fixhornetbug", "0", FCVAR_SERVER };
cvar_t mp_checkentities = { "mp_checkentities", "0", FCVAR_SERVER };
cvar_t mp_touchmenu = { "mp_touchmenu", "1", FCVAR_SERVER };
cvar_t mp_touchname = { "mp_touchname", "", FCVAR_SERVER };
cvar_t mp_touchcommand = { "mp_touchcommand", "", FCVAR_SERVER };

void Ent_RunGC_f( void );

void GGM_RegisterCVars( void )
{
	CVAR_REGISTER( &cvar_allow_ar2 );
	CVAR_REGISTER( &cvar_allow_gravgun );
	CVAR_REGISTER( &cvar_ar2_mp5 );
	CVAR_REGISTER( &cvar_ar2_bullets );
	CVAR_REGISTER( &cvar_ar2_balls );
	CVAR_REGISTER( &cvar_wresptime );
	CVAR_REGISTER( &cvar_iresptime );
	CVAR_REGISTER( &cvar_gibtime );
	CVAR_REGISTER( &cvar_hgibcount );
	CVAR_REGISTER( &cvar_agibcount );
	CVAR_REGISTER( &mp_gravgun_players );
	CVAR_REGISTER( &mp_fixhornetbug );
	CVAR_REGISTER( &mp_checkentities );
	CVAR_REGISTER( &mp_touchmenu );
	CVAR_REGISTER( &mp_touchname );
	CVAR_REGISTER( &mp_touchcommand );
	g_engfuncs.pfnAddServerCommand( "ent_rungc", Ent_RunGC_f );
}

void Ent_RunGC( bool common, bool enttools, const char *userid )
{
	int i, count = 0, removed = 0;
	edict_t *ent = g_engfuncs.pfnPEntityOfEntIndex( gpGlobals->maxClients + 5 );

	ALERT( at_warning, "Running garbage collector\n" );

	for( i = gpGlobals->maxClients; i < gpGlobals->maxEntities; i++, ent++ )
	{
		const char *classname = STRING( ent->v.classname );

		if( ent->free )
			continue;

		if( !classname || !ent->v.classname || !classname[0] )
			continue;

		count++;

		if( ent->v.flags & FL_KILLME )
			continue;

		if( common )
		{
			if( !strcmp( classname, "gib" ) )
			{
				ent->v.flags |= FL_KILLME;
				removed++;
				continue;
			}

			if( !strncmp( classname, "monster_", 8 ) && ent->v.health <= 0 || ent->v.deadflag != DEAD_NO )
			{
				ent->v.flags |= FL_KILLME;
				removed++;
				continue;
			}
		}
		if( !enttools )
		{
			if( strncmp( classname, "monster_", 8 ) || strncmp( classname, "weapon_", 7 ) || strncmp( classname, "ammo_", 5 ) )
				continue;
		}

		if( !ent->v.owner && ent->v.spawnflags & SF_NORESPAWN )
		{
			ent->v.flags |= FL_KILLME;
			removed++;
			continue;
		}

		CBaseEntity *entity = CBaseEntity::Instance( ent );

		if( !entity )
		{
			ent->v.flags |= FL_KILLME;
			removed++;
			continue;
		}

		if( enttools && entity->enttools_data.enttools )
		{
			if( !userid || !strcmp( userid, entity->enttools_data.ownerid ) )
			{
				ent->v.flags |= FL_KILLME;
				removed++;
				continue;
			}
		}

		else if( common && !entity->IsInWorld() )
		{
			ent->v.flags |= FL_KILLME;
			removed++;
			continue;
		}
	}

	ALERT( at_notice, "Total %d entities, %d cleaned\n", count, removed );

}

void Ent_RunGC_f()
{
	int enttools = atoi(CMD_ARGV(1));
	Ent_RunGC( !enttools, enttools, NULL );
}

int Ent_CheckEntitySpawn( edict_t *pent )
{

	if( mp_checkentities.value )
	{
		int index = ENTINDEX( pent );
		static unsigned int counter, lastgc;
		counter++;


		if( gpGlobals->maxEntities - index < 10 )
		{
			ALERT( at_error, "REFUSING CREATING ENTITY %s\n", STRING( pent->v.classname ) );
			Ent_RunGC( true, true, NULL );
			return 1;
		}

		if( gpGlobals->maxEntities - index < 100 )
		{
			if( !strncmp( STRING(pent->v.classname), "env_", 4) )
				return 1;

			if( !strcmp( STRING(pent->v.classname), "gib" ) )
				return 1;


			Ent_RunGC( true, false, NULL );

			return 0;
		}

		if( index > gpGlobals->maxEntities / 2 && counter - lastgc > 64 )
		{
			lastgc = counter;
			Ent_RunGC( true, false, NULL );
			return 0;
		}
	}

	return 0;
}


void GGM_ClientPutinServer(edict_t *pEntity, CBasePlayer *pPlayer)
{
	if( mp_touchmenu.value && pPlayer->gravgunmod_data.m_state == STATE_UNINITIALIZED )
		g_engfuncs.pfnQueryClientCvarValue2( pEntity, "touch_enable", 111 );

	pPlayer->gravgunmod_data.m_state = STATE_CONNECTED;

	const char *uid = GETPLAYERAUTHID( pPlayer->edict() );
	if( !uid || strstr(uid, "PENDING") )
		uid = g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "ip" );

	strncpy( pPlayer->gravgunmod_data.uid, uid, 32 );
	pPlayer->gravgunmod_data.uid[32] = 0;
	pPlayer->gravgunmod_data.m_flEntTime = 0;
	pPlayer->gravgunmod_data.m_flEntScope = 0;
	pPlayer->gravgunmod_data.menu.pPlayer = pPlayer;
	pPlayer->gravgunmod_data.menu.Clear();
}

void GGM_ClientFirstSpawn(CBasePlayer *pPlayer)
{
	// AGHL-like spectator
	if( mp_spectator.value )
	{
		pPlayer->RemoveAllItems( TRUE );
		UTIL_BecomeSpectator( pPlayer );
	}

}

edict_t *GGM_PlayerByID( const char *id )
{
	for( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *plr = UTIL_PlayerByIndex( i );

		if( plr && plr->IsPlayer() )
		{
			CBasePlayer *player = (CBasePlayer *) plr;

			if( !strcmp( player->gravgunmod_data.uid, id ) )
				return player->edict();
		}
	}

	return NULL;
}

const char *GGM_GetPlayerID( edict_t *player )
{
	CBasePlayer *plr = (CBasePlayer*)CBaseEntity::Instance( player );

	if( !plr->IsPlayer() )
		return NULL;

	return plr->gravgunmod_data.uid;
}

/*
===============================

CMD_ARGV replacement

We do not have access to engine's COM_TokenizeString,
so perform all parsing here, allowing to change CMD_ARGV result

===============================
*/

bool cmd_used;
// CMD_ARGS replacement
namespace GGM
{
static int		cmd_argc;
static char *cmd_args;
static char		*cmd_argv[80];

static char *COM_ParseFile( char *data, char *token )
{
	int	c, len;

	if( !token )
		return NULL;

	len = 0;
	token[0] = 0;

	if( !data )
		return NULL;
// skip whitespace
skipwhite:
	while(( c = ((byte)*data)) <= ' ' )
	{
		if( c == 0 )
			return NULL;	// end of file;
		data++;
	}

	// skip // comments
	if( c=='/' && data[1] == '/' )
	{
		while( *data && *data != '\n' )
			data++;
		goto skipwhite;
	}

	// handle quoted strings specially
	if( c == '\"' )
	{
		data++;
		while( 1 )
		{
			c = (byte)*data;

			// unexpected line end
			if( !c )
			{
				token[len] = 0;
				return data;
			}
			data++;

			if( c == '\\' && *data == '"' )
			{
				token[len++] = *data++;
				continue;
			}

			if( c == '\"' )
			{
				token[len] = 0;
				return data;
			}
			token[len] = c;
			len++;
		}
	}

	// parse single characters
	if( c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ',' )
	{
		token[len] = c;
		len++;
		token[len] = 0;
		return data + 1;
	}

	// parse a regular word
	do
	{
		token[len] = c;
		data++;
		len++;
		c = ((byte)*data);

		if( c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ',' )
			break;
	} while( c > 32 );

	token[len] = 0;

	return data;
}

static void Cmd_TokenizeString( const char *text )
{
	char	cmd_token[256];
	int	i;

	// clear the args from the last string
	for( i = 0; i < cmd_argc; i++ )
	{
		delete [] cmd_argv[i];
		cmd_argv[i] = 0;
	}

	cmd_argc = 0; // clear previous args
	cmd_args = NULL;

	cmd_used = true;

	if( !text ) return;

	while( 1 )
	{
		// skip whitespace up to a /n
		while( *text && ((byte)*text) <= ' ' && *text != '\r' && *text != '\n' )
			text++;

		if( *text == '\n' || *text == '\r' )
		{
			// a newline seperates commands in the buffer
			if( *text == '\r' && text[1] == '\n' )
				text++;
			text++;
			break;
		}

		if( !*text )
			return;

		if( cmd_argc == 1 )
			 cmd_args = (char*)text;
		text = COM_ParseFile( (char*)text, cmd_token );
		if( !text ) return;

		if( cmd_argc < 80 )
		{
			cmd_argv[cmd_argc] = new char[strlen(cmd_token)+1];
			strcpy(cmd_argv[cmd_argc], cmd_token);
			cmd_argc++;
		}
	}
}

static void Cmd_Reset( void )
{
	cmd_used = false;
}

}

extern "C" int CMD_ARGC()
{
	if( cmd_used )
	{
		return GGM::cmd_argc;
	}
	return g_engfuncs.pfnCmd_Argc();
}

extern "C" const char *CMD_ARGS()
{
	if( cmd_used )
	{
		if(!GGM::cmd_args)
			return "";
		return GGM::cmd_args;
	}
	return g_engfuncs.pfnCmd_Args();
}
extern "C" const char *CMD_ARGV( int i )
{
	if( cmd_used )
	{
		if( i < 0 || i >= GGM::cmd_argc|| !GGM::cmd_argv[i] )
			return "";
		return GGM::cmd_argv[i];
	}
	return g_engfuncs.pfnCmd_Argv( i );
}

// client.cpp
void ClientCommand( edict_t *pEntity );

// Allow say /command as alias to command
void GGM_Say( edict_t *pEntity )
{
	const char *args = CMD_ARGS();

	if( !args || strlen( args ) < 2 )
		return;

	if( args[1] != '/' )
		return;

	GGM::Cmd_TokenizeString( args + 2 );
	ClientCommand( pEntity );
	GGM::Cmd_Reset();
}


GGM_PlayerMenu &GGM_PlayerMenu::Add(const char *name, const char *command)
{
	if( m_fShow )
		return *this;

	if( m_iCount > 4 )
	{
		ALERT( at_error, "GGM_PlayerMenu::Add: Only 5 menu items supported" );
		return *this;
	}

	strncpy( m_items[m_iCount].name, name, sizeof(m_items[m_iCount].name) - 1 );
	strncpy( m_items[m_iCount].command, command, sizeof(m_items[m_iCount].command) - 1 );
	m_iCount++;
	return *this;
}
GGM_PlayerMenu &GGM_PlayerMenu::Clear( void )
{
	m_fShow = false;
	m_iCount = 0;
	return *this;
}

GGM_PlayerMenu &GGM_PlayerMenu::SetTitle( const char *title )
{
	if( m_fShow )
		return *this;
	strncpy( m_sTitle, title, sizeof(m_sTitle) - 1);
	return *this;
}
GGM_PlayerMenu &GGM_PlayerMenu::New( const char *title, bool force )
{
	if( m_fShow && !force )
		return *this;

	m_fShow = false;

	SetTitle(title);
	return Clear();
}
extern int gmsgShowMenu;
void GGM_PlayerMenu::Show()
{
	if( pPlayer->gravgunmod_data.m_fTouchMenu)
	{
		char buf[256];

		#define MENU_STR(VAR) (#VAR)
		sprintf( buf, MENU_STR(slot10\ntouch_hide _sm*\ntouch_show _sm\ntouch_addbutton "_smt" "#%s" "" 0.16 0.11 0.41 0.3 0 255 0 255 78 1.5\n), m_sTitle);

		if( pPlayer )
			CLIENT_COMMAND( pPlayer->edict(), buf);

		for( int i = 0; i < m_iCount; i++ )
		{
			sprintf( buf, MENU_STR(touch_settexture _sm%d "#%d. %s"\ntouch_show _sm%d\n), i+1, i+1, m_items[i].name, i + 1 );

			if( pPlayer )
				CLIENT_COMMAND( pPlayer->edict(), buf);
		}
	}
	else
	{
		char buf[128], *pbuf = buf;
		short int flags = 1<<9;

		pbuf += sprintf( pbuf, "^2%s:\n", m_sTitle );

		for( int i = 0; i < m_iCount; i++ )
		{
			pbuf += sprintf( pbuf, "^3%d.^7 %s\n", i+1, m_items[i].name);
			flags |= 1<<i;
		}

		MESSAGE_BEGIN( MSG_ONE, gmsgShowMenu, NULL, pPlayer->pev );
		WRITE_SHORT( flags ); // slots
		WRITE_CHAR( 255 ); // show time
		WRITE_BYTE( 0 ); // need more
		WRITE_STRING( buf );
		MESSAGE_END();
	}

	m_fShow = true;
}

bool GGM_PlayerMenu::MenuSelect( int select )
{
	m_fShow = false;

	if( select > m_iCount || select < 1 )
		return false;


	if( !m_items[select-1].command[0] )
		return true;

	GGM::Cmd_TokenizeString( m_items[select-1].command );
	ClientCommand( pPlayer->edict() );
	GGM::Cmd_Reset();

	return true;
}

bool GGM_FilterFileName( const char *name )
{
	while( name && *name )
	{
		if( *name >= 'A' && *name <= 'z' || *name >= '0' && *name <= '9' )
		{
			name++;
			continue;
		}
		return false;
	}

	return true;
}

bool GGM_MenuCommand( CBasePlayer *player, const char *name )
{
	char buf[256] = "ggm/menus/";
	char *file, *pFile;

	if( !GGM_FilterFileName( name ) )
		return false;

	strncat( buf, name, sizeof(buf) - 1 );

	file = pFile = (char*)LOAD_FILE_FOR_ME( buf, NULL );

	if( !file )
		return false;

	pFile = GGM::COM_ParseFile( pFile, buf );

	// no title
	if( !pFile )
	{
		FREE_FILE( file );
		return false;
	}

	GGM_PlayerMenu &m = player->gravgunmod_data.menu.New( buf );

	while( pFile = GGM::COM_ParseFile( pFile, buf ) )
	{
		char cmd[256];
		if( !(pFile = GGM::COM_ParseFile( pFile, cmd )) )
			break;

		m.Add( buf, cmd );
	}

	m.Show();
	return true;
}

// half-life special math?
#define MAX_MOTD_CHUNK	  60
#define MAX_MOTD_LENGTH   1536 // (MAX_MOTD_CHUNK * 4)
extern int gmsgMOTD;

bool GGM_MOTDCommand( CBasePlayer *player, const char *name )
{
	char buf[256] = "ggm/motd/";
	char *file, *pFileList;
	int char_count = 0;

	if( !GGM_FilterFileName( name ) )
		return false;

	strncat( buf, name, sizeof(buf) - 1 );

	file = pFileList = (char*)LOAD_FILE_FOR_ME( buf, NULL );

	if( !file )
		return false;

	// Send the message of the day
	// read it chunk-by-chunk,  and send it in parts

	while( pFileList && *pFileList && char_count < MAX_MOTD_LENGTH )
	{
		char chunk[MAX_MOTD_CHUNK + 1];

		if( strlen( pFileList ) < MAX_MOTD_CHUNK )
		{
			strcpy( chunk, pFileList );
		}
		else
		{
			strncpy( chunk, pFileList, MAX_MOTD_CHUNK );
			chunk[MAX_MOTD_CHUNK] = 0;		// strncpy doesn't always append the null terminator
		}

		char_count += strlen( chunk );
		if( char_count < MAX_MOTD_LENGTH )
			pFileList = file + char_count;
		else
			*pFileList = 0;

		MESSAGE_BEGIN( MSG_ONE, gmsgMOTD, NULL, player->edict() );
			WRITE_BYTE( *pFileList ? FALSE : TRUE );	// FALSE means there is still more message to come
			WRITE_STRING( chunk );
		MESSAGE_END();
	}

	FREE_FILE(file);
	return true;
}

void GGM_InitialMenus( CBasePlayer *pPlayer )
{
	pPlayer->gravgunmod_data.touch_loading = false;

	if( !GGM_MenuCommand( pPlayer, "init" ) && mp_coop.value )
		pPlayer->gravgunmod_data.menu.New( "COOP SERVER" )
				.Add("Join coop", "joincoop")
				.Add("Spectate", "spectate")
				.Show();
}

bool GGM_TouchCommand( CBasePlayer *pPlayer, const char *pcmd )
{
	edict_t *pEntity = pPlayer->edict();

	if( FStrEq(pcmd, "tb") )
	{
		CLIENT_COMMAND( pEntity, "touch_show _sb*\n");
		return true;
	}

	if( !pPlayer->gravgunmod_data.touch_loading )
		return false;

	if( FStrEq(pcmd, "m1"))
	{
		if( mp_touchmenu.value )
			CLIENT_COMMAND( pEntity,
				MENU_STR(touch_addbutton "_sm" "*black" "" 0.15 0.1 0.4 0.72 0 0 0 128 335\ntouch_addbutton "_smt" "#" "" 0.16 0.11 0.41 0.3 0 255 0 255 79 1.5\nm2\n)
				);
	}
	else if( FStrEq(pcmd, "m2"))
	{
		if( mp_touchmenu.value )
			CLIENT_COMMAND( pEntity,
				MENU_STR(touch_addbutton "_sm1" "#" "menuselect 1;touch_hide _sm*" 0.16 0.21 0.39 0.3 255 255 255 255 335 1.5\ntouch_addbutton "_sm2" "#" "menuselect 2;touch_hide _sm*" 0.16 0.31 0.39 0.4 255 255 255 255 335 1.5\nm3\n)
				);
	}
	else if( FStrEq(pcmd, "m3"))
	{
		if( mp_touchmenu.value )
			CLIENT_COMMAND( pEntity,
				MENU_STR(touch_addbutton "_sm3" "#" "menuselect 3;touch_hide _sm*" 0.16 0.41 0.39 0.5 255 255 255 255 335 1.5\ntouch_addbutton "_sm4" "#" "menuselect 4;touch_hide _sm*" 0.16 0.51 0.39 0.6 255 255 255 255 335 1.5\nm4\n)
				);
	}
	else if( FStrEq(pcmd, "m4"))
	{
		if( mp_touchmenu.value )
			CLIENT_COMMAND( pEntity,
				MENU_STR(touch_addbutton "_sm5" "#" "menuselect 5;touch_hide _sm*" 0.16 0.61 0.39 0.7 255 255 255 255 335 1.5;wait;slot10\n)
				);
		GGM_InitialMenus( pPlayer );
	}
	else return false;
	return true;
}

extern float g_flWeaponCheat;

void DumpProps(); // prop.cpp

bool GGM_ClientCommand( CBasePlayer *pPlayer, const char *pcmd )
{

	if( FStrEq( pcmd, "menuselect" ) )
	{
		int imenu = atoi( CMD_ARGV( 1 ) );

		return pPlayer->gravgunmod_data.menu.MenuSelect(imenu);
	}
	else if( GGM_MOTDCommand( pPlayer, pcmd ) )
		return true;
	else if( GGM_MenuCommand( pPlayer, pcmd ) )
		return true;
	else if( GGM_TouchCommand( pPlayer, pcmd ) )
		return true;
	else if( FStrEq(pcmd, "dumpprops") )
	{
		if ( g_flWeaponCheat != 0.0 )
			DumpProps();
	}
	else if( FStrEq(pcmd, "client") )
	{
		char args[256] = {0};
		strncpy(args, CMD_ARGS(),254);
		strcat(args,"\n");
		CLIENT_COMMAND( pPlayer->edict(), args );
	}
	else if( COOP_ClientCommand( pPlayer->edict() ) )
		return true;
	else if( Ent_ProcessClientCommand( pPlayer->edict() ) )
		return true;

	return false;

}

void GGM_CvarValue2( const edict_t *pEnt, int requestID, const char *cvarName, const char *value )
{
	if( pEnt && requestID == 111  && FStrEq( cvarName , "touch_enable" ) && atoi( value) )
	{
		CBasePlayer *player = (CBasePlayer * ) CBaseEntity::Instance( (edict_t*)pEnt );
		player->gravgunmod_data.m_fTouchMenu = !!atof( value );

		CLIENT_COMMAND((edict_t*)pEnt, "m1\n");
		player->gravgunmod_data.touch_loading = true;

		const char *name = NULL, *command = NULL;
		if( mp_coop.value )
			name = "COOP MENU", command = "coopmenu";
		else if( mp_touchname.string[0] && mp_touchcommand.string[0] )
			name = mp_touchname.string, command = mp_touchcommand.string;
		else
			return;
		CLIENT_COMMAND( (edict_t*)pEnt,
			UTIL_VarArgs(MENU_STR(touch_addbutton "_sb" "*black" "%s;touch_hide _sb*;tb" 0 0.05 0.15 0.11 0 0 0 128 334\ntouch_addbutton "_sbt" "#%s" "" 0 0.05 0.16 0.11 255 255 127 255 78 2\n	), command, name ));


	}

}