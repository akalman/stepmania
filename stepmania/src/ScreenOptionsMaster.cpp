#include "global.h"

#include "ScreenOptionsMaster.h"
#include "RageException.h"
#include "RageUtil.h"
#include "RageLog.h"
#include "ThemeManager.h"
#include "GameState.h"
#include "ScreenManager.h"
#include "NoteSkinManager.h"
#include "Course.h"
#include "Steps.h"
#include "Style.h"
#include "song.h"
#include "SongManager.h"
#include "Character.h"
#include "PrefsManager.h"
#include "ScreenOptionsMasterPrefs.h"
#include "GameSoundManager.h"
#include "StepMania.h"
#include "RageSoundManager.h"
#include "ProfileManager.h"
#include "StepsUtil.h"
#include "LuaManager.h"
#include "GameManager.h"
#include "Foreach.h"

#define LINE_NAMES				THEME->GetMetric (m_sName,"LineNames")
#define OPTION_MENU_FLAGS		THEME->GetMetric (m_sName,"OptionMenuFlags")
#define LINE(sLineName)			THEME->GetMetric (m_sName,ssprintf("Line%s",sLineName.c_str()))

#define ENTRY(s)				THEME->GetMetric ("ScreenOptionsMaster",s)
#define ENTRY_NAME(s)			THEME->GetMetric ("OptionNames", s)
#define ENTRY_MODE(s,i)			THEME->GetMetric ("ScreenOptionsMaster",ssprintf("%s,%i",(s).c_str(),(i+1)))
#define ENTRY_DEFAULT(s)		THEME->GetMetric ("ScreenOptionsMaster",(s) + "Default")
#define NEXT_SCREEN				THEME->GetMetric (m_sName,"NextScreen")
#define PREV_SCREEN				THEME->GetMetric (m_sName,"PrevScreen")

/* Add the list named "ListName" to the given row/handler. */
void ScreenOptionsMaster::SetList( OptionRowDefinition &row, OptionRowHandler &hand, CString ListName )
{
	hand.type = ROW_LIST;
	hand.m_bUseModNameForIcon = true;

	row.name = ListName;
	if( !ListName.CompareNoCase("noteskins") )
	{
		hand.Default.Init(); /* none */
		row.bOneChoiceForAllPlayers = false;

		CStringArray arraySkinNames;
		NOTESKIN->GetNoteSkinNames( arraySkinNames );
		for( unsigned skin=0; skin<arraySkinNames.size(); skin++ )
		{
			arraySkinNames[skin].MakeUpper();

			GameCommand mc;
			mc.m_sModifiers = arraySkinNames[skin];
			hand.ListEntries.push_back( mc );
			row.choices.push_back( arraySkinNames[skin] );
		}
		return;
	}

	hand.Default.Load( -1, ParseCommands(ENTRY_DEFAULT(ListName)) );

	/* Parse the basic configuration metric. */
	CStringArray asParts;
	split( ENTRY(ListName), ",", asParts );
	if( asParts.size() < 1 )
		RageException::Throw( "Parse error in ScreenOptionsMasterEntries::ListName%s", ListName.c_str() );

	row.bOneChoiceForAllPlayers = false;
	const int NumCols = atoi( asParts[0] );
	for( unsigned i=0; i<asParts.size(); i++ )
	{
		if( asParts[i].CompareNoCase("together") == 0 )
			row.bOneChoiceForAllPlayers = true;
		if( asParts[i].CompareNoCase("SelectMultiple") == 0 )
			row.selectType = SELECT_MULTIPLE;
		if( asParts[i].CompareNoCase("SelectNone") == 0 )
			row.selectType = SELECT_NONE;
		if( asParts[i].CompareNoCase("ShowOneInRow") == 0 )
			row.layoutType = LAYOUT_SHOW_ONE_IN_ROW;
	}

	for( int col = 0; col < NumCols; ++col )
	{
		GameCommand mc;
		mc.Load( 0, ParseCommands(ENTRY_MODE(ListName, col)) );
		if( mc.m_sName == "" )
			RageException::Throw( "List \"%s\", col %i has no name", ListName.c_str(), col );

		if( !mc.IsPlayable() )
			continue;

		hand.ListEntries.push_back( mc );

		CString sName = mc.m_sName;
		CString sChoice = ENTRY_NAME(mc.m_sName);
		row.choices.push_back( sChoice );
	}
}

void ScreenOptionsMaster::SetLua( OptionRowDefinition &row, OptionRowHandler &hand, const CString &sLuaFunction )
{
	hand.type = ROW_LUA;
	hand.m_bUseModNameForIcon = true;

	/* Run the Lua expression.  It should return a table. */
	hand.m_LuaTable.SetFromExpression( sLuaFunction );

	if( hand.m_LuaTable.GetLuaType() != LUA_TTABLE )
		RageException::Throw( "Result of \"%s\" is not a table", sLuaFunction.c_str() );

	{
		hand.m_LuaTable.PushSelf( LUA->L );

		lua_pushstring( LUA->L, "Name" );
		lua_gettable( LUA->L, -2 );
		const char *pStr = lua_tostring( LUA->L, -1 );
		if( pStr == NULL )
			RageException::Throw( "\"%s\" \"Name\" entry is not a string", sLuaFunction.c_str() );
		row.name = pStr;
		lua_pop( LUA->L, 1 );


		lua_pushstring( LUA->L, "OneChoiceForAllPlayers" );
		lua_gettable( LUA->L, -2 );
		row.bOneChoiceForAllPlayers = !!lua_toboolean( LUA->L, -1 );
		lua_pop( LUA->L, 1 );


		lua_pushstring( LUA->L, "LayoutType" );
		lua_gettable( LUA->L, -2 );
		pStr = lua_tostring( LUA->L, -1 );
		if( pStr == NULL )
			RageException::Throw( "\"%s\" \"LayoutType\" entry is not a string", sLuaFunction.c_str() );
		row.layoutType = StringToLayoutType( pStr );
		ASSERT( row.layoutType != LAYOUT_INVALID );
		lua_pop( LUA->L, 1 );

		lua_pushstring( LUA->L, "SelectType" );
		lua_gettable( LUA->L, -2 );
		pStr = lua_tostring( LUA->L, -1 );
		if( pStr == NULL )
			RageException::Throw( "\"%s\" \"SelectType\" entry is not a string", sLuaFunction.c_str() );
		row.selectType = StringToSelectType( pStr );
		ASSERT( row.selectType != SELECT_INVALID );
		lua_pop( LUA->L, 1 );

		/* Iterate over the "Choices" table. */
		lua_pushstring( LUA->L, "Choices" );
		lua_gettable( LUA->L, -2 );
		if( !lua_istable( LUA->L, -1 ) )
			RageException::Throw( "\"%s\" \"Choices\" is not a table", sLuaFunction.c_str() );

		lua_pushnil( LUA->L );
		while( lua_next(LUA->L, -2) != 0 )
		{
			/* `key' is at index -2 and `value' at index -1 */
			const char *pValue = lua_tostring( LUA->L, -1 );
			if( pValue == NULL )
				RageException::Throw( "\"%s\" Column entry is not a string", sLuaFunction.c_str() );
			LOG->Trace( "'%s'", pValue);

			row.choices.push_back( pValue );

			lua_pop( LUA->L, 1 );  /* removes `value'; keeps `key' for next iteration */
		}

		lua_pop( LUA->L, 1 ); /* pop choices table */

		lua_pop( LUA->L, 1 ); /* pop main table */
		ASSERT( lua_gettop(LUA->L) == 0 );
	}
}


/* Add a list of difficulties/edits to the given row/handler. */
void ScreenOptionsMaster::SetSteps( OptionRowDefinition &row, OptionRowHandler &hand )
{
	hand.type = ROW_LIST;
	row.name = "Steps";
	row.bOneChoiceForAllPlayers = false;

	// fill in difficulty names
	if( GAMESTATE->m_bEditing )
	{
		row.choices.push_back( "" );
		hand.ListEntries.push_back( GameCommand() );
	}
	else if( GAMESTATE->IsCourseMode() )   // playing a course
	{
		row.bOneChoiceForAllPlayers = PREFSMAN->m_bLockCourseDifficulties;

		vector<Trail*> vTrails;
		GAMESTATE->m_pCurCourse->GetTrails( vTrails, GAMESTATE->GetCurrentStyle()->m_StepsType );
		for( unsigned i=0; i<vTrails.size(); i++ )
		{
			Trail* pTrail = vTrails[i];

			CString s = CourseDifficultyToThemedString( pTrail->m_CourseDifficulty );
			row.choices.push_back( s );
			GameCommand mc;
			mc.m_pTrail = pTrail;
			hand.ListEntries.push_back( mc );
		}
	}
	else // !GAMESTATE->IsCourseMode(), playing a song
	{
		vector<Steps*> vSteps;
		GAMESTATE->m_pCurSong->GetSteps( vSteps, GAMESTATE->GetCurrentStyle()->m_StepsType );
		StepsUtil::SortNotesArrayByDifficulty( vSteps );
		for( unsigned i=0; i<vSteps.size(); i++ )
		{
			Steps* pSteps = vSteps[i];

			CString s;
			if( pSteps->GetDifficulty() == DIFFICULTY_EDIT )
				s = pSteps->GetDescription();
			else
				s = DifficultyToThemedString( pSteps->GetDifficulty() );
			s += ssprintf( " (%d)", pSteps->GetMeter() );

			row.choices.push_back( s );
			GameCommand mc;
			mc.m_pSteps = pSteps;
			mc.m_dc = pSteps->GetDifficulty();
			hand.ListEntries.push_back( mc );
		}
	}
}


/* Add the given configuration value to the given row/handler. */
void ScreenOptionsMaster::SetConf( OptionRowDefinition &row, OptionRowHandler &hand, CString param )
{
	/* Configuration values are never per-player. */
	row.bOneChoiceForAllPlayers = true;
	hand.type = ROW_CONFIG;

	ConfOption *pConfOption = ConfOption::Find( param );
	if( pConfOption == NULL )
		RageException::Throw( "Invalid Conf type \"%s\"", param.c_str() );

	pConfOption->UpdateAvailableOptions();

	hand.opt = pConfOption;
	hand.opt->MakeOptionsList( row.choices );

	row.name = hand.opt->name;
}

/* Add a list of available characters to the given row/handler. */
void ScreenOptionsMaster::SetCharacters( OptionRowDefinition &row, OptionRowHandler &hand )
{
	hand.type = ROW_LIST;
	row.bOneChoiceForAllPlayers = false;
	row.name = "Characters";
	hand.Default.m_pCharacter = GAMESTATE->GetDefaultCharacter();

	{
		row.choices.push_back( ENTRY_NAME("Off") );
		GameCommand mc;
		mc.m_pCharacter = NULL;
		hand.ListEntries.push_back( mc );
	}

	vector<Character*> apCharacters;
	GAMESTATE->GetCharacters( apCharacters );
	for( unsigned i=0; i<apCharacters.size(); i++ )
	{
		Character* pCharacter = apCharacters[i];
		CString s = pCharacter->m_sName;
		s.MakeUpper();

		row.choices.push_back( s ); 
		GameCommand mc;
		mc.m_pCharacter = pCharacter;
		hand.ListEntries.push_back( mc );
	}
}

/* Add a list of available styles to the given row/handler. */
void ScreenOptionsMaster::SetStyles( OptionRowDefinition &row, OptionRowHandler &hand )
{
	hand.type = ROW_LIST;
	row.bOneChoiceForAllPlayers = true;
	row.name = "Style";

	vector<const Style*> vStyles;
	GAMEMAN->GetStylesForGame( GAMESTATE->m_pCurGame, vStyles );
	ASSERT( vStyles.size() );
	FOREACH_CONST( const Style*, vStyles, s )
	{
		row.choices.push_back( GAMEMAN->StyleToThemedString(*s) ); 
		GameCommand mc;
		mc.m_pStyle = *s;
		hand.ListEntries.push_back( mc );
	}

	hand.Default.m_pStyle = vStyles[0];
}

/* Add a list of available song groups to the given row/handler. */
void ScreenOptionsMaster::SetGroups( OptionRowDefinition &row, OptionRowHandler &hand )
{
	hand.type = ROW_LIST;
	row.bOneChoiceForAllPlayers = true;
	row.name = "Group";
	hand.Default.m_sSongGroup = GROUP_ALL_MUSIC;

	vector<CString> vGroups;
	SONGMAN->GetGroupNames( vGroups );
	ASSERT( vGroups.size() );

	{
		row.choices.push_back( ENTRY_NAME("AllGroups") );
		GameCommand mc;
		mc.m_sSongGroup = GROUP_ALL_MUSIC;
		hand.ListEntries.push_back( mc );
	}

	FOREACH_CONST( CString, vGroups, g )
	{
		row.choices.push_back( *g ); 
		GameCommand mc;
		mc.m_sSongGroup = *g;
		hand.ListEntries.push_back( mc );
	}
}

/* Add a list of available difficulties to the given row/handler. */
void ScreenOptionsMaster::SetDifficulties( OptionRowDefinition &row, OptionRowHandler &hand )
{
	set<Difficulty> vDifficulties;
	GAMESTATE->GetDifficultiesToShow( vDifficulties );

	hand.type = ROW_LIST;
	row.bOneChoiceForAllPlayers = true;
	row.name = "Difficulty";
	hand.Default.m_dc = DIFFICULTY_INVALID;

	{
		row.choices.push_back( ENTRY_NAME("AllDifficulties") );
		GameCommand mc;
		mc.m_dc = DIFFICULTY_INVALID;
		hand.ListEntries.push_back( mc );
	}

	FOREACHS_CONST( Difficulty, vDifficulties, d )
	{
		CString s = DifficultyToThemedString( *d );

		row.choices.push_back( s ); 
		GameCommand mc;
		mc.m_dc = *d;
		hand.ListEntries.push_back( mc );
	}
}

REGISTER_SCREEN_CLASS( ScreenOptionsMaster );
ScreenOptionsMaster::ScreenOptionsMaster( CString sClassName ):
	ScreenOptions( sClassName )
{
	LOG->Trace("ScreenOptionsMaster::ScreenOptionsMaster(%s)", m_sName.c_str() );

	/* If this file doesn't exist, leave the music alone (eg. ScreenPlayerOptions music sample
	 * left over from ScreenSelectMusic).  If you really want to play no music, add a redir
	 * to _silent. */
	CString MusicPath = THEME->GetPathS( m_sName, "music", true );
	if( MusicPath != "" )
		SOUND->PlayMusic( MusicPath );

	CStringArray asLineNames;
	split( LINE_NAMES, ",", asLineNames );
	if( asLineNames.empty() )
		RageException::Throw( "%s::LineNames is empty.", m_sName.c_str() );


	CStringArray Flags;
	split( OPTION_MENU_FLAGS, ";", Flags, true );
	InputMode im = INPUTMODE_INDIVIDUAL;
	bool Explanations = false;
	bool bShowUnderlines = true;
	
	for( unsigned i = 0; i < Flags.size(); ++i )
	{
		Flags[i].MakeLower();

		if( Flags[i] == "together" )
			im = INPUTMODE_SHARE_CURSOR;
		if( Flags[i] == "explanations" )
			Explanations = true;
		if( Flags[i] == "forceallplayers" )
		{
			FOREACH_PlayerNumber( pn )
				GAMESTATE->m_bSideIsJoined[pn] = true;
			GAMESTATE->m_MasterPlayerNumber = PlayerNumber(0);
		}
		if( Flags[i] == "smnavigation" )
			SetNavigation( NAV_THREE_KEY_MENU );
		if( Flags[i] == "toggle" || Flags[i] == "firstchoicegoesdown" )
			SetNavigation( PREFSMAN->m_bArcadeOptionsNavigation? NAV_TOGGLE_THREE_KEY:NAV_TOGGLE_FIVE_KEY );
		if( Flags[i] == "hideunderlines" )
			bShowUnderlines = false;
	}

	m_OptionRowAlloc = new OptionRowDefinition[asLineNames.size()];
	for( unsigned i = 0; i < asLineNames.size(); ++i )
	{
		CString sLineName = asLineNames[i];

		OptionRowDefinition &row = m_OptionRowAlloc[i];
		
		CString sRowCommands = LINE(sLineName);
		
		Commands vCommands;
		ParseCommands( sRowCommands, vCommands );
		
		if( vCommands.v.size() < 1 )
			RageException::Throw( "Parse error in %s::Line%i", m_sName.c_str(), i+1 );

		OptionRowHandler hand;
		for( unsigned part = 0; part < vCommands.v.size(); ++part)
		{
			Command& command = vCommands.v[part];

			BeginHandleArgs;

			const CString &name = command.GetName();

			if( !name.CompareNoCase("list") )				SetList( row, hand, sArg(1) );
			else if( !name.CompareNoCase("lua") )			SetLua( row, hand, sArg(1) );
			else if( !name.CompareNoCase("steps") )			SetSteps( row, hand );
			else if( !name.CompareNoCase("conf") )			SetConf( row, hand, sArg(1) );
			else if( !name.CompareNoCase("characters") )	SetCharacters( row, hand );
			else if( !name.CompareNoCase("styles") )		SetStyles( row, hand );
			else if( !name.CompareNoCase("groups") )		SetGroups( row, hand );
			else if( !name.CompareNoCase("difficulties") )	SetDifficulties( row, hand );
			else
				RageException::Throw( "Unexpected type '%s' in %s::Line%i", name.c_str(), m_sName.c_str(), i );

			EndHandleArgs;
		}

		// TRICKY:  Insert a down arrow as the first choice in the row.
		if( m_OptionsNavigation == NAV_TOGGLE_THREE_KEY )
		{
			row.choices.insert( row.choices.begin(), ENTRY_NAME("NextRow") );
			hand.ListEntries.insert( hand.ListEntries.begin(), GameCommand() );
		}

		OptionRowHandlers.push_back( hand );
	}

	ASSERT( OptionRowHandlers.size() == asLineNames.size() );

	InitMenu( im, m_OptionRowAlloc, asLineNames.size(), bShowUnderlines );
}

ScreenOptionsMaster::~ScreenOptionsMaster()
{
	delete [] m_OptionRowAlloc;
}

void SelectExactlyOne( int iSelection, vector<bool> &vbSelectedOut )
{
	for( int i=0; i<(int)vbSelectedOut.size(); i++ )
		vbSelectedOut[i] = i==iSelection;
}

void ScreenOptionsMaster::ImportOption( const OptionRowDefinition &row, const OptionRowHandler &hand, PlayerNumber pn, int rowno, vector<bool> &vbSelectedOut )
{
	/* Figure out which selection is the default. */
	switch( hand.type )
	{
	case ROW_LIST:
		{
			int FallbackOption = -1;
			bool UseFallbackOption = true;

			for( unsigned e = 0; e < hand.ListEntries.size(); ++e )
			{
				const GameCommand &mc = hand.ListEntries[e];

				vbSelectedOut[e] = false;

				if( mc.IsZero() )
				{
					/* The entry has no effect.  This is usually a default "none of the
					 * above" entry.  It will always return true for DescribesCurrentMode().
					 * It's only the selected choice if nothing else matches. */
					if( row.selectType != SELECT_MULTIPLE )
						FallbackOption = e;
					continue;
				}

				if( row.bOneChoiceForAllPlayers )
				{
					if( mc.DescribesCurrentModeForAllPlayers() )
					{
						UseFallbackOption = false;
						if( row.selectType != SELECT_MULTIPLE )
							SelectExactlyOne( e, vbSelectedOut );
						else
							vbSelectedOut[e] = true;
					}
				}
				else
				{
					if( mc.DescribesCurrentMode(  pn) )
					{
						UseFallbackOption = false;
						if( row.selectType != SELECT_MULTIPLE )
							SelectExactlyOne( e, vbSelectedOut );
						else
							vbSelectedOut[e] = true;
					}
				}
			}

			if( row.selectType == SELECT_ONE && 
				UseFallbackOption && 
				FallbackOption != -1 )
			{
				SelectExactlyOne( FallbackOption, vbSelectedOut );
			}

			return;
		}

	case ROW_LUA:
		{
			ASSERT( lua_gettop(LUA->L) == 0 );

			/* Evaluate the LoadSelections(self,array,pn) function, where array is a table
			 * representing vbSelectedOut. */

			/* Hack: the NextRow entry is never set, and should be transparent.  Remove
			 * it, and readd it below. */
			if( m_OptionsNavigation == NAV_TOGGLE_THREE_KEY )
				vbSelectedOut.erase( vbSelectedOut.begin() );

			/* All selections default to false. */
			for( unsigned i = 0; i < vbSelectedOut.size(); ++i )
				vbSelectedOut[i] = false;

			/* Create the vbSelectedOut table. */
			LUA->CreateTableFromArray( vbSelectedOut );
			ASSERT( lua_gettop(LUA->L) == 1 ); /* vbSelectedOut table */

			/* Get the function to call from m_LuaTable. */
			hand.m_LuaTable.PushSelf( LUA->L );
			ASSERT( lua_istable( LUA->L, -1 ) );

			lua_pushstring( LUA->L, "LoadSelections" );
			lua_gettable( LUA->L, -2 );
			if( !lua_isfunction( LUA->L, -1 ) )
				RageException::Throw( "\"%s\" \"LoadSelections\" entry is not a function", row.name.c_str() );

			/* Argument 1 (self): */
			hand.m_LuaTable.PushSelf( LUA->L );

			/* Argument 2 (vbSelectedOut): */
			lua_pushvalue( LUA->L, 1 );

			/* Argument 3 (pn): */
			LUA->PushStack( (int) pn );

			ASSERT( lua_gettop(LUA->L) == 6 ); /* vbSelectedOut, m_iLuaTable, function, self, arg, arg */

			lua_call( LUA->L, 3, 0 ); // call function with 3 arguments and 0 results
			ASSERT( lua_gettop(LUA->L) == 2 );

			lua_pop( LUA->L, 1 ); /* pop option table */

			LUA->ReadArrayFromTable( vbSelectedOut );
			if( m_OptionsNavigation == NAV_TOGGLE_THREE_KEY )
				vbSelectedOut.insert( vbSelectedOut.begin(), false );
			
			lua_pop( LUA->L, 1 ); /* pop vbSelectedOut table */

			ASSERT( lua_gettop(LUA->L) == 0 );
			return;
		}

	case ROW_CONFIG:
		{
			int iSelection = hand.opt->Get();
			SelectExactlyOne( iSelection+(m_OptionsNavigation==NAV_TOGGLE_THREE_KEY?1:0), vbSelectedOut );
			return;
		}

	default:
		ASSERT(0);
	}

	if( row.selectType != SELECT_MULTIPLE )
	{
		// The first row ("go down") should not be selected.
		ASSERT( !vbSelectedOut[0] );

		// there should be exactly one option selected
		int iNumSelected = 0;
		for( unsigned e = 1; e < hand.ListEntries.size(); ++e )
			if( vbSelectedOut[e] )
				iNumSelected++;
		ASSERT( iNumSelected == 1 );
	}
}

void ScreenOptionsMaster::ImportOptions()
{
	for( unsigned i = 0; i < OptionRowHandlers.size(); ++i )
	{
		const OptionRowHandler &hand = OptionRowHandlers[i];
		const OptionRowDefinition &data = m_OptionRowAlloc[i];
		OptionRow &row = *m_Rows[i];

		if( data.bOneChoiceForAllPlayers )
		{
			ImportOption(data, hand, PLAYER_1, i, row.m_vbSelected[0] );
		}
		else
		{
			FOREACH_HumanPlayer( p )
			{
				ImportOption( data, hand, p, i, row.m_vbSelected[p] );
			}
		}
	}
}

/* Import only settings specific to the given player. */
void ScreenOptionsMaster::ImportOptionsForPlayer( PlayerNumber pn )
{
	if( !GAMESTATE->IsHumanPlayer(pn) )
		return;

	for( unsigned i = 0; i < OptionRowHandlers.size(); ++i )
	{
		const OptionRowHandler &hand = OptionRowHandlers[i];
		const OptionRowDefinition &data = m_OptionRowAlloc[i];
		OptionRow &row = *m_Rows[i];

		if( data.bOneChoiceForAllPlayers )
			continue;
		ImportOption( data, hand, pn, i, row.m_vbSelected[pn] );
	}
}

int GetOneSelection( const vector<bool> &vbSelected )
{
	for( unsigned i=0; i<vbSelected.size(); i++ )
		if( vbSelected[i] )
			return i;
	ASSERT(0);	// shouldn't call this if not expecting one to be selected
	return -1;
}

/* Returns an OPT mask. */
int ScreenOptionsMaster::ExportOption( const OptionRowDefinition &row, const OptionRowHandler &hand, PlayerNumber pn, const vector<bool> &vbSelected )
{
	/* Figure out which selection is the default. */
	switch( hand.type )
	{
	case ROW_LIST:
		{
			hand.Default.Apply( pn );
			for( unsigned i=0; i<vbSelected.size(); i++ )
				if( vbSelected[i] )
					hand.ListEntries[i].Apply( pn );
		}
		break;
	case ROW_LUA:
		{
			ASSERT( lua_gettop(LUA->L) == 0 );

			/* Evaluate SaveSelections(self,array,pn) function, where array is a table
			 * representing vbSelectedOut. */

			/* Hack: the NextRow entry is never set, and should be transparent.  Remove it. */
			vector<bool> vbSelectedCopy = vbSelected;
			if( m_OptionsNavigation == NAV_TOGGLE_THREE_KEY )
				vbSelectedCopy.erase( vbSelectedCopy.begin() );

			/* Create the vbSelectedOut table. */
			LUA->CreateTableFromArray( vbSelectedCopy );
			ASSERT( lua_gettop(LUA->L) == 1 ); /* vbSelectedOut table */

			/* Get the function to call. */
			hand.m_LuaTable.PushSelf( LUA->L );
			ASSERT( lua_istable( LUA->L, -1 ) );

			lua_pushstring( LUA->L, "SaveSelections" );
			lua_gettable( LUA->L, -2 );
			if( !lua_isfunction( LUA->L, -1 ) )
				RageException::Throw( "\"%s\" \"SaveSelections\" entry is not a function", row.name.c_str() );

			/* Argument 1 (self): */
			hand.m_LuaTable.PushSelf( LUA->L );

			/* Argument 2 (vbSelectedOut): */
			lua_pushvalue( LUA->L, 1 );

			/* Argument 3 (pn): */
			LUA->PushStack( (int) pn );

			ASSERT( lua_gettop(LUA->L) == 6 ); /* vbSelectedOut, m_iLuaTable, function, self, arg, arg */

			lua_call( LUA->L, 3, 0 ); // call function with 3 arguments and 0 results
			ASSERT( lua_gettop(LUA->L) == 2 );

			lua_pop( LUA->L, 1 ); /* pop option table */
			lua_pop( LUA->L, 1 ); /* pop vbSelected table */

			ASSERT( lua_gettop(LUA->L) == 0 );

			// XXX: allow specifying the mask
			return 0;
		}

	case ROW_CONFIG:
		{
			int sel = GetOneSelection(vbSelected) - (m_OptionsNavigation==NAV_TOGGLE_THREE_KEY?1:0);

			/* Get the original choice. */
			int Original = hand.opt->Get();

			/* Apply. */
			hand.opt->Put( sel );

			/* Get the new choice. */
			int New = hand.opt->Get();

			/* If it didn't change, don't return any side-effects. */
			if( Original == New )
				return 0;

			return hand.opt->GetEffects();
		}
		break;

	default:
		ASSERT(0);
		break;
	}
	return 0;
}

int ScreenOptionsMaster::ExportOptionForAllPlayers( int iRow )
{
	int iChangeMask = 0;
	const OptionRowHandler &hand = OptionRowHandlers[iRow];
	const OptionRowDefinition &data = m_OptionRowAlloc[iRow];
	OptionRow &row = *m_Rows[iRow];
	FOREACH_HumanPlayer( pn )
	{
		vector<bool> &vbSelected = row.m_vbSelected[pn];

		iChangeMask |= ExportOption( data, hand, pn, vbSelected );
	}
	return iChangeMask;
}

void ScreenOptionsMaster::ExportOptions()
{
	int ChangeMask = 0;

	CHECKPOINT;
	const unsigned row = this->GetCurrentRow();
	/* If the selection is on a LIST, and the selected LIST option sets the screen,
	 * honor it. */
	m_sNextScreen = "";

	for( unsigned i = 0; i < OptionRowHandlers.size(); ++i )
	{
		CHECKPOINT_M( ssprintf("%i/%i", i, int(OptionRowHandlers.size())) );
		
		/* If SELECT_NONE, only apply it if it's the selected option. */
		const OptionRowDefinition &data = m_OptionRowAlloc[i];
		if( data.selectType == SELECT_NONE && i != row )
			continue;

		OptionRowHandler &hand = OptionRowHandlers[i];

		if( hand.type == ROW_LIST )
		{
			const int choice = m_Rows[i]->m_iChoiceInRowWithFocus[GAMESTATE->m_MasterPlayerNumber];
			GameCommand &mc = hand.ListEntries[choice];
			if( mc.m_sScreen != "" )
			{
				/* Hack: instead of applying screen commands here, store them in
				 * m_sNextScreen and apply them after we tween out.  If we don't set
				 * m_sScreen to "", we'll load it twice (once for each player) and
				 * then again for m_sNextScreen. */
				m_sNextScreen = mc.m_sScreen;
				mc.m_sScreen = "";
			}
		}

		ExportOptionForAllPlayers( i );
	}
	CHECKPOINT;

	// NEXT_SCREEN;
	if( m_sNextScreen == "" )
		m_sNextScreen = NEXT_SCREEN;

	if( ChangeMask & OPT_APPLY_ASPECT_RATIO )
	{
		THEME->UpdateLuaGlobals();	// This needs to be done before resetting the projection matrix below
		SCREENMAN->ThemeChanged();	// recreate ScreenSystemLayer and SharedBGA
	}

	/* If the theme changes, we need to reset RageDisplay to apply new theme 
	 * window title and icon. */
	/* If the aspect ratio changes, we need to reset RageDisplay so that the 
	 * projection matrix is re-created using the new screen dimensions. */
	if( (ChangeMask & OPT_APPLY_THEME) || 
		(ChangeMask & OPT_APPLY_GRAPHICS) ||
		(ChangeMask & OPT_APPLY_ASPECT_RATIO) )
		ApplyGraphicOptions();

	if( ChangeMask & OPT_SAVE_PREFERENCES )
	{
		/* Save preferences. */
		LOG->Trace("ROW_CONFIG used; saving ...");
		PREFSMAN->SaveGlobalPrefsToDisk();
		SaveGamePrefsToDisk();
	}

	if( ChangeMask & OPT_RESET_GAME )
	{
		ResetGame();
		m_sNextScreen = "";
	}

	if( ChangeMask & OPT_APPLY_SOUND )
	{
		SOUNDMAN->SetPrefs( PREFSMAN->m_fSoundVolume );
	}
	
	if( ChangeMask & OPT_APPLY_SONG )
		SONGMAN->SetPreferences();

	CHECKPOINT;
}

void ScreenOptionsMaster::GoToNextScreen()
{
	if( GAMESTATE->m_bEditing )
		SCREENMAN->PopTopScreen();
	else if( m_sNextScreen != "" )
		SCREENMAN->SetNewScreen( m_sNextScreen );
}

void ScreenOptionsMaster::GoToPrevScreen()
{
	/* XXX: A better way to handle this would be to check if we're a pushed screen. */
	if( GAMESTATE->m_bEditing )
	{
		SCREENMAN->PopTopScreen();
		// XXX: handle different destinations based on play mode?
	}
	else
	{
		SCREENMAN->DeletePreparedScreens();
		SCREENMAN->SetNewScreen( PREV_SCREEN ); // (GAMESTATE->m_PlayMode) );
	}
}

void ScreenOptionsMaster::RefreshIcons()
{
	FOREACH_HumanPlayer( p )
	{
        for( unsigned i=0; i<m_Rows.size(); ++i )     // foreach options line
		{
			if( m_Rows[i]->m_Type == OptionRow::ROW_EXIT )
				continue;	// skip

			OptionRow &row = *m_Rows[i];
			const OptionRowDefinition &data = row.m_RowDef;

			// find first selection and whether multiple are selected
			int iFirstSelection = row.GetOneSelection( p, true );

			// set icon name
			CString sIcon;

			if( iFirstSelection == -1 )
			{
				sIcon = "Multi";
			}
			else if( iFirstSelection != -1 )
			{
				const OptionRowHandler &handler = OptionRowHandlers[i];
				switch( handler.type )
				{
				case ROW_LIST:
					sIcon = handler.m_bUseModNameForIcon ?
						handler.ListEntries[iFirstSelection].m_sModifiers :
						data.choices[iFirstSelection];
					break;
				case ROW_CONFIG:
					break;
				}
			}
			

			/* XXX: hack to not display text in the song options menu */
			if( data.bOneChoiceForAllPlayers )
				sIcon = "";

			LoadOptionIcon( p, i, sIcon );
		}
	}
}

/*
 * (c) 2003-2004 Glenn Maynard
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, and/or sell copies of the Software, and to permit persons to
 * whom the Software is furnished to do so, provided that the above
 * copyright notice(s) and this permission notice appear in all copies of
 * the Software and that both the above copyright notice(s) and this
 * permission notice appear in supporting documentation.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF
 * THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS
 * INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
