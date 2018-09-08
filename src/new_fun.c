#include <sys/types.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include "mud.h"
#include "sha256.h"

extern void remove_member args( ( CHAR_DATA *ch ) );
bool str_ssh	args( ( const char *astr, const char *bstr ) );
void transStatRemove args ((CHAR_DATA *ch));

/* Abbreviate powerlevels */
char *abbNumLD(long double number)
{
	static char abbNumber[MAX_STRING_LENGTH];
	long double tempNumber = number;

	abbNumber[0] = '\0';

	if (number < 100000)
		sprintf(abbNumber, "%s", num_punct_ld(number));
	else if (number < 1000000) // 123.4k
	{
		tempNumber /= 1000;
		sprintf(abbNumber, "%.1Lfk", tempNumber );
	}
	else if (number < 10000000) // 1.234m
	{
		tempNumber /= 1000000;
		sprintf(abbNumber, "%.3Lfm", tempNumber );
	}
	else if (number < 100000000) //12.34m
	{
		tempNumber /= 1000000;
		sprintf(abbNumber, "%.2Lfm", tempNumber );
	}
	else if (number < 1000000000) //123.4m
	{
		tempNumber /= 1000000;
		sprintf(abbNumber, "%.1Lfm", tempNumber );
	}
	else if (number < 10000000000ULL)
	{
		tempNumber /= 1000000000;
		sprintf(abbNumber, "%.3Lfb", tempNumber );
	}
	else if (number < 100000000000ULL)
	{
		tempNumber /= 1000000000;
		sprintf(abbNumber, "%.2Lfb", tempNumber );
	}
	else if (number < 1000000000000ULL)
	{
		tempNumber /= 1000000000;
		sprintf(abbNumber, "%.1Lfb", tempNumber );
	}
	else if (number < 10000000000000ULL)
	{
		tempNumber /= 1000000000000ULL;
		sprintf(abbNumber, "%.3Lft", tempNumber );
	}
	else if (number < 100000000000000ULL)
	{
		tempNumber /= 1000000000000ULL;
		sprintf(abbNumber, "%.2Lft", tempNumber );
	}
	else if (number < 1000000000000000ULL)
	{
		tempNumber /= 1000000000000ULL;
		sprintf(abbNumber, "%.1Lft", tempNumber );
	}
	else if (number < 10000000000000000ULL)
        {
                tempNumber /= 1000000000000000ULL;
                sprintf(abbNumber, "%.3Lfq", tempNumber );
        }
        else if (number < 100000000000000000ULL)
        {
                tempNumber /= 1000000000000000ULL;
                sprintf(abbNumber, "%.2Lfq", tempNumber );
        }
        else 
        {
                tempNumber /= 1000000000000000ULL;
                sprintf(abbNumber, "%.1Lfq", tempNumber );
	}


	return abbNumber;
}

bool is_splitformed(CHAR_DATA *ch)
{
	if (xIS_SET(ch->affected_by, AFF_SPLIT_FORM)
		|| xIS_SET(ch->affected_by, AFF_TRI_FORM)
		|| xIS_SET(ch->affected_by, AFF_MULTI_FORM)
		|| xIS_SET(ch->affected_by, AFF_BIOJR))
		return true;

	return false;
}


double weightedtraining(CHAR_DATA *ch)
{
    OBJ_DATA *obj;
    int iWear;
	double weightinc = 0;

	if (IS_NPC(ch))
		return 0;

    for ( iWear = 0; iWear < MAX_WEAR; iWear++ )
    {
	for ( obj = ch->first_carrying; obj; obj = obj->next_content )
	   if ( obj->wear_loc == iWear )
	   {
			if (obj->item_type == ITEM_ARMOR && obj->value[3] > 0)
				weightinc += obj->value[3];
	   }
    }

	return weightinc;
}

double weightedClothingPlMod(CHAR_DATA *ch)
{
    OBJ_DATA *obj;
    int iWear;
	double plMod = 0;
	
	return 0;

	if (IS_NPC(ch))
		return 0;

    for ( iWear = 0; iWear < MAX_WEAR; iWear++ )
    {
	for ( obj = ch->first_carrying; obj; obj = obj->next_content )
	   if ( obj->wear_loc == iWear )
	   {
			if (obj->item_type == ITEM_ARMOR && obj->value[3] > 0)
				plMod += obj->value[3];
	   }
    }

	plMod = plMod/10000;
	return URANGE( 0, plMod, 20 );
}

void update_plHiscore( CHAR_DATA *ch )
{
	struct tm *time;
	struct tm *creationTime;
	int tyear = 0;
	int tmon = 0;
	int ctyear = 0;
	int ctmon = 0;

	time = localtime(&current_time);

	tyear = time->tm_year;
	tmon = time->tm_mon;

	creationTime = localtime(&ch->pcdata->creation_date);
	ctyear = creationTime->tm_year;
	ctmon = creationTime->tm_mon;

	adjust_hiscore_ld( "powerlevel", ch, ch->exp );

	if (is_saiyan(ch))
		adjust_hiscore_ld( "plsaiyan", ch, ch->exp );
	if (is_human(ch))
		adjust_hiscore_ld( "plhuman", ch, ch->exp );
	if (is_hb(ch))
		adjust_hiscore_ld( "plhalfbreed", ch, ch->exp );
	if (is_namek(ch))
		adjust_hiscore_ld( "plnamek", ch, ch->exp );
	if (is_android(ch) || is_superandroid(ch) )
		adjust_hiscore_ld( "plandroid", ch, ch->exp );
	if (is_icer(ch))
		adjust_hiscore_ld( "plicer", ch, ch->exp );
	if (is_bio(ch))
		adjust_hiscore_ld( "plbio-android", ch, ch->exp );
	if (is_kaio(ch))
                adjust_hiscore_ld( "plkaio", ch, ch->exp );
	if (is_demon(ch))
                adjust_hiscore_ld( "pldemon", ch, ch->exp );

	if (ctyear == tyear
		&& ctmon == tmon)
	{
		switch (tmon)
		{
			default:
			case 0:
				adjust_hiscore_ld( "januaryladder", ch, ch->exp );
				break;
			case 1:
				adjust_hiscore_ld( "februaryladder", ch, ch->exp );
				break;
			case 2:
				adjust_hiscore_ld( "marchladder", ch, ch->exp );
				break;
			case 3:
				adjust_hiscore_ld( "aprilladder", ch, ch->exp );
				break;
			case 4:
				adjust_hiscore_ld( "mayladder", ch, ch->exp );
				break;
			case 5:
				adjust_hiscore_ld( "juneladder", ch, ch->exp );
				break;
			case 6:
				adjust_hiscore_ld( "julyladder", ch, ch->exp );
				break;
			case 7:
				adjust_hiscore_ld( "augustladder", ch, ch->exp );
				break;
			case 8:
				adjust_hiscore_ld( "septemberladder", ch, ch->exp );
				break;
			case 9:
				adjust_hiscore_ld( "octoberladder", ch, ch->exp );
				break;
			case 10:
				adjust_hiscore_ld( "novemberladder", ch, ch->exp );
				break;
			case 11:
				adjust_hiscore_ld( "decemberladder", ch, ch->exp );
				break;
		}
	}


	return;
}

char *get_pkColor( CHAR_DATA *ch )
{
	if( is_kaio(ch) && ch->kairank > 0 )
	{
	  if( ch->kairank == 1 )
	    return ("&g");
          if( ch->kairank == 2 )
            return ("&G");
          if( ch->kairank == 3 )
            return ("&B");
          if( ch->kairank == 4 )
            return ("&Y");
          if( ch->kairank == 5 )
            return ("&W");
          if( ch->kairank == 6 )
            return ("&R");
	}
	if( is_demon(ch) && ch->demonrank > 0 )
        {
	    if( ch->demonrank == 1 )
	      return ("&O");
	    if( ch->demonrank == 2 )
              return ("&r");
	    if( ch->demonrank == 3 )
              return ("&R");
	}
	if (xIS_SET(ch->act, PLR_PK1))
		return ("&Y");
	if (xIS_SET(ch->act, PLR_PK2) || IS_HC(ch))
		return ("&R");

	return ("&G");
}

bool can_pk( CHAR_DATA *ch )
{
	if (xIS_SET(ch->act, PLR_PK1))
		return true;
	if (xIS_SET(ch->act, PLR_PK2))
		return true;
	if (xIS_SET(ch->act, PLR_WAR1))
                return true;
        if (xIS_SET(ch->act, PLR_WAR2))
                return true;
	if (IS_HC(ch))
		return true;

	return false;
}

// Chaged this to a function for expantion later on
int get_damroll( CHAR_DATA *ch)
{
	int damroll = 0;
	
	if (xIS_SET((ch)->affected_by, AFF_ENERGYFIST)) {
		damroll = get_curr_int(ch) / 40;
	}
		
	else if (xIS_SET((ch)->affected_by, AFF_HYBRIDSTYLE)) {
		damroll = (get_curr_str(ch) / 25) + (get_curr_int(ch) / 50);
	}
	else if (xIS_SET((ch)->affected_by, AFF_BRUISERSTYLE)) {
		damroll = get_curr_str(ch) / 18;
	}
	else {
		damroll = get_curr_str(ch) / 20;
	}
	if (ch->mental_state > 5 && ch->mental_state < 15)
		damroll++;

	return damroll;
}

// For finding the defence bonus based on str
int get_strDef( CHAR_DATA *victim )
{
	int strDef = 0;

	strDef = 0;

	if (victim->mental_state > 5 && victim->mental_state < 15)
	  strDef++;
	if (victim->skillGsn > 0)
	  strDef/= 2;

	return strDef;
}

// For finding the defence bonus based on con
int get_conDef( CHAR_DATA *victim )
{
	int conDef = 0;

	conDef = 0;

	if (victim->mental_state > 5 && victim->mental_state < 15)
	  conDef++;

	if(victim->skillGsn > 0)
	  conDef /= 2;
	
	return conDef;
}

int get_armor( CHAR_DATA *ch )
{
	int armor = 0;

	if (!IS_NPC(ch))
		armor = ((ch->perm_con / 3) + (ch->perm_str / 6));

    if (armor > 8000)
    	armor = 8000;
    return armor;
}

int get_maxarmor( CHAR_DATA *ch )
{
	int armor = 0;

	if (!IS_NPC(ch))
		armor = ((ch->perm_con / 3) + (ch->perm_str / 6));

    if (armor > 8000)
    	armor = 8000;

    return armor;
}

//  Hidden armor stat = evil.  --Saiyr
int get_hidden_armor( CHAR_DATA *ch )
{
    int armor;
    int maxarmor;
    float percentage;
    int hiddenarmor;

    if( IS_NPC( ch ) )
	return ch->armor;
    else
    {
	armor = get_armor( ch );
	maxarmor = get_maxarmor( ch );
	percentage = armor / maxarmor;
	hiddenarmor = maxarmor > race_table[ch->race]->max_armor ? percentage * race_table[ch->race]->max_armor : armor;
    }

    return hiddenarmor;
}

double get_attmod( CHAR_DATA *ch, CHAR_DATA *victim )
{
	double attmod = 0.000;
	attmod = ( (double) (ch)->pl / (victim)->pl);
	return attmod;
}
long double get_lattmod( CHAR_DATA *ch, CHAR_DATA *victim )
{
	long double attmod = 0.000;

	if ( !IS_NPC(ch) && !IS_NPC(victim) )
    	attmod = ( (long double) (ch)->pl / (victim)->pl);
	if ( !IS_NPC(ch) && IS_NPC(victim) )
    	attmod = ( (long double) (ch)->pl / (victim)->exp);
	if ( IS_NPC(ch) && !IS_NPC(victim) )
    	attmod = ( (long double) (ch)->exp / (victim)->pl);
	if ( IS_NPC(ch) && IS_NPC(victim) )
    	attmod = ( (long double) (ch)->exp / (victim)->exp);

	return attmod;
}

OBJ_DATA *has_scouter( CHAR_DATA *ch )
{
    OBJ_DATA *obj;
	int iWear;

    for ( iWear = 0; iWear < MAX_WEAR; iWear++ )
    {
	for ( obj = ch->first_carrying; obj; obj = obj->next_content )
	   if ( obj->wear_loc == iWear )
	   {
		if ( obj->item_type == ITEM_SCOUTER )
		    return obj;
	   }
    }

    return NULL;
}

OBJ_DATA *has_dragonradar( CHAR_DATA *ch )
{
    OBJ_DATA *obj;
        int iWear;

    for ( iWear = 0; iWear < MAX_WEAR; iWear++ )
    {
        for ( obj = ch->first_carrying; obj; obj = obj->next_content )
           if ( obj->wear_loc == WEAR_HOLD )
           {
                if ( obj->item_type == ITEM_DRAGONRADAR )
                    return obj;
           }
    }
    return NULL;
}

int get_true_rank( CHAR_DATA *ch )
{

	if (IS_NPC(ch))
		return 0;

	if (ch->exp < 5000)
		return 1;
	else if (ch->exp < 100000)
		return 2;
	else if (ch->exp < 1000000)
		return 3;
	else if (ch->exp < 10000000)
		return 4;
	else if (ch->exp < 100000000)
		return 5;
	else if (ch->exp < 1000000000)
		return 6;
	else if (ch->exp < 10000000000ULL)
		return 7;
	else if (ch->exp < 50000000000ULL)
		return 8;
	else if (ch->exp < 100000000000ULL)
		return 9;
	else if (ch->exp < 300000000000ULL)
		return 10;
	else if (ch->exp < 600000000000ULL)
		return 11;
	else if (ch->exp < 1000000000000ULL)
		return 12;
	else if (ch->exp < 10000000000000ULL)
		return 13;
	else if (ch->exp < 50000000000000ULL)
                return 14;
	else if (ch->exp < 100000000000000ULL)
                return 15;
	else
		return 16;
	return 0;
}

int get_rank_number( CHAR_DATA *ch )
{

	if (IS_NPC(ch))
		return -1;

	if (ch->exp < 100000)
		return 1;
	else
		return 2;
	return -1;
}

char *get_rank( CHAR_DATA *ch)
{

	if (IS_NPC(ch))
		return ("NPC");

        if (ch->exp < 100000)
	    return ("Fighter in Training");
	else 
	    return ("Deadly Fighter");
}

char *get_rank_color( CHAR_DATA *ch)
{

	if (IS_NPC(ch))
		return ("&rNPC");

	if (IS_HC(ch))
		return ("&rUnknown");
	else if (ch->exp < 100000)
		return ("&PFighter in Training");
	else
		return ("&cDeadly Fighter");
	
	return ("&rBUG: NOTIFY GOKU");
}

char *color_align( CHAR_DATA *ch)
{
	if (IS_HC(ch))
		return ("&z");
	else if (IS_GOOD(ch))
		return ("&C");
	else if (IS_NEUTRAL(ch))
		return ("&w");
	else
		return ("&R");
}

char *color_clan( CHAR_DATA *ch)
{

	if (IS_NPC(ch))
		return ("");
	if (!ch->pcdata)
		return ("");
	if (!ch->pcdata->clan)
		return ("");

	if (ch->pcdata->clan->alignment == CLANALIGN_GOOD)
		return ("&C");
	else if (ch->pcdata->clan->alignment == CLANALIGN_NEUTRAL)
		return ("&w");
	else if (ch->pcdata->clan->alignment == CLANALIGN_EVIL)
		return ("&R");

	return ("");
}

char *get_build( CHAR_DATA *ch )
{
	if (IS_NPC(ch))
		return ("");

	return (build_type[ch->pcdata->build]);
}

char *get_haircolor( CHAR_DATA *ch )
{
	if (IS_NPC(ch))
		return ("");

	return (hair_color[ch->pcdata->haircolor]);
}

char *get_eyes( CHAR_DATA *ch )
{
	if (IS_NPC(ch))
		return ("");

	return (eye_color[ch->pcdata->eyes]);
}

char *get_complexion( CHAR_DATA *ch )
{
	if (IS_NPC(ch))
		return ("");

	return (complexion[ch->pcdata->complexion]);
}

char *get_secondary_color( CHAR_DATA *ch )
{
	if (IS_NPC(ch))
		return ("");

	return (secondary_color[ch->pcdata->secondarycolor]);
}

int get_hairlen( CHAR_DATA *ch )
{
	if (IS_NPC(ch))
		return 0;

	return (ch->pcdata->hairlen);
}

char *get_hairstyle( CHAR_DATA *ch )
{
	if (IS_NPC(ch))
		return ("");

	return (hair_style[ch->pcdata->hairstyle]);
}

char *heshe( CHAR_DATA *ch, bool cap )
{
	if (cap)
	{
	if (ch->sex == SEX_MALE)
		return ("He");
	if (ch->sex == SEX_FEMALE)
		return ("She");
	else
		return ("It");
	}
	else
	{
	if (ch->sex == SEX_MALE)
		return ("he");
	if (ch->sex == SEX_FEMALE)
		return ("she");
	else
		return ("it");
	}
}

char *himher( CHAR_DATA *ch, bool cap )
{
	if (cap)
	{
	if (ch->sex == SEX_MALE)
		return ("Him");
	if (ch->sex == SEX_FEMALE)
		return ("Her");
	else
		return ("It");
	}
	else
	{
	if (ch->sex == SEX_MALE)
		return ("him");
	if (ch->sex == SEX_FEMALE)
		return ("her");
	else
		return ("it");
	}
}

char *hisher( CHAR_DATA *ch, bool cap )
{
	if (cap)
	{
	if (ch->sex == SEX_MALE)
		return ("His");
	if (ch->sex == SEX_FEMALE)
		return ("Her");
	else
		return ("Its");
	}
	else
	{
	if (ch->sex == SEX_MALE)
		return ("his");
	if (ch->sex == SEX_FEMALE)
		return ("her");
	else
		return ("its");
	}
}

sh_int get_newage( CHAR_DATA *ch )
{
	if (IS_NPC(ch))
	    return 4 + ( ch->played + (current_time - ch->logon) ) / 7200;
	else
		return (ch->pcdata->age);
}

void kaioken_drain(CHAR_DATA *ch)
{
	char buf[MAX_INPUT_LENGTH];
    int drain;
	int kaioken;
	long double los = 0;

	if (ch->position <= POS_SLEEPING)
	{
		act( AT_RED, "The powers of the Kaioken slowly fade away as you lose conciseness.", ch, NULL, NULL, TO_CHAR );
		act( AT_RED, "The powers of $n's Kaioken attack slowly fade as $e loses conciseness.", ch, NULL, NULL, TO_NOTVICT );

		xREMOVE_BIT((ch)->affected_by, AFF_KAIOKEN);
		if (xIS_SET((ch)->affected_by, AFF_HEART))
			xREMOVE_BIT(ch->affected_by, AFF_HEART);
		ch->pl = ch->exp;
                transStatRemove( ch );
		heart_calc(ch, "");
		return;
	}

	drain = number_range( 6, 8 );

	kaioken = (ch->pl / ch->exp);
	drain *= kaioken;

	if( !str_cmp(get_race(ch),"kaio") )
	  drain /= 3;

	if (ch->mana - drain < 0) {
		drain = drain - ch->mana;
		ch->mana = 0;
	}
	else
		ch->mana -= drain;

	if (drain != 0 && ch->mana == 0) {
		if (ch->hit - drain > 0)
			ch->hit -= drain;
		else if (ch->hit - drain < 0) {
			ch->hit -= drain;
			update_pos(ch);
			if ( ch->position == POS_DEAD) {
				act( AT_RED, "Your body explodes under the strain of using the Kaioken.", ch, NULL, NULL, TO_CHAR );
				act( AT_RED, "$n explodes under the strain of using the Kaioken.", ch, NULL, NULL, TO_NOTVICT );
				sprintf( buf, "%s explodes under the strain of using the Kaioken", ch->name );
				do_info(ch, buf);
				if ( !IS_NPC(ch) )
					los = ch->exp * 0.085;
					gain_exp( ch, 0 - los);
				raw_kill(ch, ch);
			}
		}
	}
	return;
}

int dam_armor_recalc( CHAR_DATA *ch, int dam )
{
	double dam_to_lf = 0;
	int armorValue = get_armor( ch );

	if (armorValue <= 0)
		return dam;

        if( armorValue > 8000 )
        {
          armorValue = 8000;
        }

	dam_to_lf = dam - ((double) armorValue / 10000 * dam);

	dam_to_lf = floor(dam_to_lf);

	if (dam_to_lf > 999999999)
		dam_to_lf = 999999999;

	return (int)dam_to_lf;
}

void do_info( CHAR_DATA *ch, char *argument )
{
	char buf[MAX_INPUT_LENGTH];
	CHAR_DATA *och;
	DESCRIPTOR_DATA *d;

	sprintf(buf, "&W[&YInfo&W] &w::&B%s&w::\r\n", argument);

	sysdata.outBytesFlag = LOGBOUTINFOCHANNEL;
	for (d = first_descriptor; d; d = d->next)
	{
		if(d->connected == CON_PLAYING)
		{
			och = d->character;
			/*if (och == ch)
				continue;*/
			if(!xIS_SET(och->deaf, CHANNEL_INFO))
			{
				och->desc->psuppress_channel++;
				send_to_char(buf, och);
			}
		}
	}
	sysdata.outBytesFlag = LOGBOUTNORM;
	do_ainfo(ch, argument);
	return;
}

void echo_to_clan( CLAN_DATA *clan, char *argument )
{
	char buf[MAX_INPUT_LENGTH];
	CHAR_DATA *och;
	DESCRIPTOR_DATA *d;

	sprintf(buf, "&W[&YClanInfo&W] &w::&c%s&w::\r\n", argument);

	sysdata.outBytesFlag = LOGBOUTINFOCHANNEL;
	for (d = first_descriptor; d; d = d->next)
	{
		if(d->connected == CON_PLAYING)
		{
			och = d->character;
			if (!och->pcdata->clan)
				continue;
			if (och->pcdata->clan != clan)
				continue;
			och->desc->psuppress_channel++;
			send_to_char(buf, och);
		}
	}
	sysdata.outBytesFlag = LOGBOUTNORM;
	return;
}

void do_ainfo( CHAR_DATA *ch, char *argument )
{
	char buf[MAX_INPUT_LENGTH];
	CHAR_DATA *och;
	DESCRIPTOR_DATA *d;

	if (ch)
	{
	if (IS_IMMORTAL(ch))
		sprintf(buf, "&W[&YAInfo&W] &w::&Y%s&w::\r\n", argument);
	else
		sprintf(buf, "&W[&YAInfo&W] &w::&B%s&w::\r\n", argument);
	}
	else
		sprintf(buf, "&W[&YAInfo&W] &w::&R%s&w::\r\n", argument);

	sysdata.outBytesFlag = LOGBOUTINFOCHANNEL;
	for (d = first_descriptor; d; d = d->next)
	{
		if(d->connected == CON_PLAYING)
		{
			och = d->character;
			if (och == ch)
				continue;
			if(!xIS_SET(och->deaf, CHANNEL_AINFO) && IS_IMMORTAL(och)
				&& och->level >= ch->level)
			{
				och->desc->psuppress_channel++;
				send_to_char(buf, och);
			}
		}
	}
	sysdata.outBytesFlag = LOGBOUTNORM;
	return;
}

bool str_ssh( const char *astr, const char *bstr )
{
    int sstr1;
    int sstr2;
    int ichar;
	int ibstr = 0;
	int punct = 0;
    if ( astr[0] == '\0' )
		return false;
    if ( bstr[0] == '\0' )
		return false;

    sstr1 = strlen(astr);
    sstr2 = strlen(bstr);

    for ( ichar = 0; ichar <= sstr1; ichar++ )
	{
		if ((astr[ichar] == '&' || astr[ichar] == '}' || astr[ichar] == '^')
			&& LOWER(astr[ichar+1]) != LOWER(bstr[ibstr]))
		{
			ichar++;
			continue;
		}
/*
		if (isspace(astr[ichar]) || isdigit(astr[ichar]))
			continue;
*/
		if (astr[ichar] == astr[ichar-1])
			continue;
		if (ispunct(astr[ichar]) && ibstr > 0)
		{
			punct++;
			ichar++;
			ibstr++;
		}
		if (LOWER(astr[ichar]) != LOWER(bstr[ibstr]))
			ibstr = 0;
		if (LOWER(astr[ichar]) == LOWER(bstr[ibstr]))
			ibstr++;
		if (ibstr >= sstr2 && punct < sstr2)
			return true;
	}

	return false;

}

void find_absorb_data( CHAR_DATA *ch, CHAR_DATA *victim)
{
	int sn = 0;
	int i = 0;
	bool stop = false;
	bool snFound = false;

	if (IS_NPC(ch) || IS_NPC(victim))
		return;
/*
 * moved the sn != gsn checks to the while loop so that it can
 * continue looping if it finds these numbers instead of just
 * breaking the loop -Goku
 */
	while(!stop)
	{
		switch (number_range(1,2))
		{
		case 1:
			sn = number_range(gsn_first_skill, gsn_first_ability - 1);
			if (victim->pcdata->learned[sn] > 0
				&& ch->pcdata->learned[sn] < skill_table[sn]->skill_adept[ch->class]
				&& ch->pl >= skill_table[sn]->skill_level[ch->class]
				&& skill_table[sn]->skill_adept[ch->class] > 0
				&& skill_table[sn]->skill_level[ch->class] > 0
				&& sn != gsn_semiperfect && sn != gsn_perfect
				&& sn != gsn_ultraperfect && sn != gsn_rescue)
				stop = true;
				snFound = true;
			break;
		case 2:
			sn = number_range(gsn_first_ability, gsn_first_weapon - 1);
			if (victim->pcdata->learned[sn] > 0
				&& ch->pcdata->learned[sn] < skill_table[sn]->skill_adept[ch->class]
				&& ch->pl >= skill_table[sn]->skill_level[ch->class]
				&& skill_table[sn]->skill_adept[ch->class] > 0
				&& skill_table[sn]->skill_level[ch->class] > 0
				&& sn != gsn_semiperfect && sn != gsn_perfect
				&& sn != gsn_ultraperfect && sn != gsn_rescue)
				stop = true;
				snFound = true;
			break;
		}
		i++;
		if (i > 100)
			stop = true;
	}

	if (snFound)
	{
		ch->pcdata->absorb_sn = sn;
		ch->pcdata->absorb_learn = victim->pcdata->learned[sn] / 2;
	}
	else
	{
		ch->pcdata->absorb_sn = 0;
		ch->pcdata->absorb_learn = 0;
	}

	return;
}

void bio_absorb(CHAR_DATA *ch, CHAR_DATA *victim)
{
	int xp_gain_post = 0;
	char buf1[MAX_STRING_LENGTH];
    long double cPL = ch->exp;
    long double vPL = victim->exp;
    bool canAbsorb = false;

    if(IS_NPC(ch))
    	return;

    if( !IS_NPC( ch )
     && ch->pl > ch->exp )
      cPL = ch->pl;

    if( !IS_NPC( victim )
     && victim->pl > victim->exp )
      vPL = victim->pl;

    if( ( cPL / vPL ) > 5 )
    {
      canAbsorb = false;
    }
    else
    {
      canAbsorb = true;
    }

	act( AT_HIT, "You stab $N in the chest with your tail and suck out $S life force.", ch, NULL, victim, TO_CHAR );
	act( AT_HIT, "$n stabs $N in the chest with $s tail and sucks out $S life force.", ch, NULL, victim, TO_ROOM );

	if (number_range(1, 100) < 75 && ch->pcdata->absorb_learn > 0
        && ( canAbsorb ) && !IS_NPC( victim) )
	{
	pager_printf(ch, "You surge with new found power as you learn %s\n\r", skill_table[ch->pcdata->absorb_sn]->name);
	ch->pcdata->learned[ch->pcdata->absorb_sn] = UMIN( GET_ADEPT(ch,ch->pcdata->absorb_sn), ch->pcdata->learned[ch->pcdata->absorb_sn] + ch->pcdata->absorb_learn );
	}

	xp_gain_post = UMIN( ch->pcdata->absorb_pl * ( race_table[ch->race]->exp_multiplier/100.0), 2147483646);

	if (xp_gain_post != 1) {
		sprintf( buf1, "Your power level increases by %s points.", num_punct(xp_gain_post) );
	    act( AT_HIT, buf1, ch, NULL, victim, TO_CHAR );
	}
	else {
		sprintf( buf1, "Your power level increases by %s point.", num_punct(xp_gain_post) );
	   	act( AT_HIT, buf1, ch, NULL, victim, TO_CHAR );
	}

	gain_exp(ch, ch->pcdata->absorb_pl);

	ch->pcdata->absorb_pl = 0;
	ch->pcdata->absorb_sn = 0;
	ch->pcdata->absorb_learn = 0;
	victim->hit = -20;
    update_pos( victim );

	evolveCheck(ch, victim, false);

	return;
}

void evolveCheck(CHAR_DATA *ch, CHAR_DATA *victim,  bool death)
{
	int evolveChance = 0;
    long double cPL, vPL;

    if(IS_NPC(ch))
    	return;

	if (ch->exp > skill_table[gsn_semiperfect]->skill_level[ch->class]
		&& ch->pcdata->learned[gsn_semiperfect] == 0)
	{
		if (!IS_NPC(victim))
		{
                        if( ch->exp > ch->pl )
                          cPL = ch->exp;
                        else
                          cPL = ch->pl;
                        if( victim->exp > victim->pl )
                          vPL = victim->exp;
                        else
                          vPL = victim->pl;
                        if( ( cPL / vPL ) > 5 )
                          return;
			if (!death)
				ch->rage++;
			evolveChance = number_range(1, 100);
			if (ch->rage * 5 > evolveChance)
			{
				pager_printf(ch, "&YNew power boils within you as you feel it's time to evolve.\n\r", skill_table[ch->pcdata->absorb_sn]->name);
				ch->pcdata->learned[gsn_semiperfect] = 100;
				ch->rage = 0;
			}
		}
	}

	if (ch->exp > skill_table[gsn_perfect]->skill_level[ch->class]
		&& ch->pcdata->learned[gsn_perfect] == 0)
	{
		if (!IS_NPC(victim))
		{
                        if( ch->exp > ch->pl )
                          cPL = ch->exp;
                        else
                          cPL = ch->pl;
                        if( victim->exp > victim->pl )
                          vPL = victim->exp;
                        else
                          vPL = victim->pl;
                        if( ( cPL / vPL ) > 5 )
                          return;
			if (!death)
				ch->rage++;
			evolveChance = number_range(1, 100);
			if (ch->rage * 5 > evolveChance)
			{
				pager_printf(ch, "&YNew power boils within you as you feel it's time to evolve.\n\r", skill_table[ch->pcdata->absorb_sn]->name);
				ch->pcdata->learned[gsn_perfect] = 100;
				ch->rage = 0;
			}
		}
	}

	if (ch->exp > skill_table[gsn_ultraperfect]->skill_level[ch->class]
		&& ch->pcdata->learned[gsn_ultraperfect] == 0)
	{
		if (!IS_NPC(victim))
		{
                        if( ch->exp > ch->pl )
                          cPL = ch->exp;
                        else
                          cPL = ch->pl;
                        if( victim->exp > victim->pl )
                          vPL = victim->exp;
                        else
                          vPL = victim->pl;
                        if( ( cPL / vPL ) > 5 )
                          return;
			if (!death)
				ch->rage++;
			evolveChance = number_range(1, 100);
			if (ch->rage * 5 > evolveChance)
			{
				pager_printf(ch, "&YNew power boils within you as you feel it's time to evolve.\n\r", skill_table[ch->pcdata->absorb_sn]->name);
				ch->pcdata->learned[gsn_ultraperfect] = 100;
				ch->rage = 0;
			}
		}
	}
}

void update_absorb(CHAR_DATA *ch, CHAR_DATA *victim)
{
	int i = 0;
	int race_absorb = 0;

    if(IS_NPC(ch))
    	return;

	if (IS_NPC(victim))
	{
		ch->pcdata->absorb_mob++;
		return;
	}

	if (victim->pl >= ch->pl)
	{
		ch->pcdata->absorb_pc++;
		ch->pcdata->absorb_race[victim->race]++;
	}
	else
	{
		ch->pcdata->absorb_pc++;
		ch->pcdata->absorb_race[victim->race]++;
	}

	for(i=0;i<=10;i++)
	{
		if (ch->pcdata->absorb_race[i] > race_absorb)
		{
			race_absorb = ch->pcdata->absorb_race[i];
			ch->pcdata->absorb_pl_mod = i;
		}
	}

	return;
}

int chargeDamMult(CHAR_DATA *ch, int dam)
{
	if (ch->charge > 0)
		dam *= ch->charge;
	return dam;
}

bool gTrainFlee(CHAR_DATA *ch)
{
    ROOM_INDEX_DATA *was_in;
    ROOM_INDEX_DATA *now_in;
    int attempt;
    sh_int door;
    EXIT_DATA *pexit;

    was_in = ch->in_room;
    for ( attempt = 0; attempt < 8; attempt++ )
    {
	door = number_door( );
	if (( pexit = get_exit( was_in, door ) ) == NULL
	||    !pexit->to_room
	||   IS_SET( pexit->exit_info, EX_NOFLEE )
	|| ( IS_SET( pexit->exit_info, EX_CLOSED )
	&&  !IS_AFFECTED( ch, AFF_PASS_DOOR ) )
	|| ( IS_NPC( ch )
	&&   xIS_SET( pexit->to_room->room_flags, ROOM_NO_MOB ) ) )
	    continue;
	move_char( ch, pexit, 0 );
	if ( ( now_in = ch->in_room ) == was_in )
	    continue;
	ch->in_room = was_in;
	act( AT_FLEE, "$n tried to hard and got kicked out of the room!", ch, NULL, NULL, TO_ROOM );
	ch->in_room = now_in;
	act( AT_FLEE, "$n glances around for signs of pursuit.", ch, NULL, NULL, TO_ROOM );
    act( AT_FLEE, "You flee head over heels from combat!", ch, NULL, NULL, TO_CHAR );

	return true;
    }

	/* didn't get knocked out*/
    return false;
}

bool upgrade_player(CHAR_DATA *ch)
{
	OBJ_DATA *obj;

	if (IS_NPC(ch))
		return false;

	if( ch->pcdata->sparcount > 0 && ch->pcdata->nextspartime <= 0 )
	    ch->pcdata->sparcount = 0;

        {
            for ( obj = ch->first_carrying; obj; obj = obj->next_content )
            {
                if( obj->pIndexData->item_type == ITEM_ARMOR )
                {
                    obj->value[3] = obj->pIndexData->value[3];
                }
		if( obj->item_type == ITEM_CONTAINER )
		{
		    OBJ_DATA *cobj, *cobj_next;

		    for( cobj = obj->first_content; cobj != NULL ; cobj = cobj_next )
		    {
			cobj_next = cobj->next_content;

			if( cobj->pIndexData->item_type == ITEM_ARMOR )
			{
			    cobj->value[3] = cobj->pIndexData->value[3];
			}
		    }
		}
            }
            pager_printf( ch, "Items updated.\n\r" );
        }

	if (ch->pcdata->upgradeL >= CURRENT_UPGRADE_LEVEL)
		return false;

	/*
	 * should be a loop to do each upgrade until
	 * the player is upgraded to the current level
	 */

	pager_printf(ch, "Updates found.  Starting update...\n\r");
	/* upgrade level 1 */
        if( ch->pcdata->upgradeL == 0 )
        {
	  ch->pcdata->permTstr     = ch->perm_str;
	  ch->pcdata->permTspd     = ch->perm_dex;
	  ch->pcdata->permTint     = ch->perm_int;
	  ch->pcdata->permTcon     = ch->perm_con;
	  ch->pcdata->upgradeL     = CURRENT_UPGRADE_LEVEL;
	  ch->pcdata->xTrain       = 0;
	  ch->pcdata->total_xTrain = 0;
          ch->pcdata->upgradeL     = 1;
        }

        if( ch->pcdata->upgradeL == 1 )
        {
          ch->pcdata->orignaleyes      = ch->pcdata->eyes;
          ch->pcdata->orignalhaircolor = ch->pcdata->haircolor;
          ch->pcdata->upgradeL         = 2;
        }

        if( ch->pcdata->upgradeL == 2 )
        {
          ch->pcdata->auraColorPowerUp = -1;
          ch->pcdata->upgradeL         = 3;
        }

        if( ch->pcdata->upgradeL == 3 )
        {
          pager_printf( ch, "Unauthorizing Bio.  Please see 'help bio' for"
                            " new guidelines.\n\r" );
          xREMOVE_BIT( ch->act, PLR_CAN_CHAT );
          ch->pcdata->upgradeL = 4;
        }

        if( ch->pcdata->upgradeL == 4 )
        {
          if( ch->pcdata->learned[gsn_bbk] > 0 )
          {
            pager_printf( ch, "Removing Big Bang Kamehameha from your ability list.\n\r" );
            if( ch->pcdata->learned[gsn_big_bang] < ch->pcdata->learned[gsn_bbk] )
            {
              pager_printf( ch, "Converting Big Bang Kamehameha learned to Big Bang.\n\r" );
              ch->pcdata->learned[gsn_big_bang] = ch->pcdata->learned[gsn_bbk];
            }
            ch->pcdata->learned[gsn_bbk] = 0;
          }
          ch->pcdata->upgradeL = 5;
        }

        if( ch->pcdata->upgradeL == 5 )
        {
          if( ch->pcdata->learned[gsn_preservation] > 0
           && ch->pcdata->learned[gsn_preservation] < 95 )
          {
            pager_printf( ch, "Increasing learned percent of Preservation.\n\r" );
            ch->pcdata->learned[gsn_preservation] = 95;
          }

          if( ch->pcdata->learned[gsn_self_destruct] > 0
           && ch->pcdata->learned[gsn_self_destruct] < 95 )
          {
            pager_printf( ch, "Increasing program competency of Self Destruct.\n\r" );
            ch->pcdata->learned[gsn_self_destruct] = 95;
          }
          ch->pcdata->upgradeL = 6;
        }

	if (ch->pcdata->upgradeL == 6)
	{
	OBJ_DATA *obj;
		ch->gold = 0;
		for ( obj = ch->first_carrying; obj; obj = obj->next_content )
		{
			if (obj->pIndexData->vnum == 610)
			{
				separate_obj(obj);
				extract_obj( obj );
			}
		}
		ch->pcdata->upgradeL = 7;
	}

	if (ch->pcdata->upgradeL == 7)
	{
		if( is_leader( ch ) )
		{
			ch->pcdata->clanRank = 2;
			if( !str_cmp( ch->pcdata->clan->leader1, ch->name ) )
				ch->pcdata->clanRank = 1;
		}
		ch->pcdata->upgradeL = 8;
		pager_printf( ch, "Updating clan rank.\n\r" );
	}

	if( ch->pcdata->upgradeL == 8 )
	{
	    for( obj = ch->last_carrying; obj; obj = obj->prev_content )
	    {
		if( obj->pIndexData->vnum == 50001 )
		{
		    obj->value[4] = obj->pIndexData->value[4];
		    obj->value[5] = obj->pIndexData->value[5];
		    pager_printf( ch, "Updating mecha-synth...\n\r" );
		}
	    }
	    ch->pcdata->upgradeL = 9;
	}
	if( ch->pcdata->upgradeL == 9 )
	{
	    if( ch->pcdata->clan_name && !str_cmp( ch->pcdata->clan_name, "Crimson Shadow" ) )
	    {
		strcpy( ch->pcdata->clan_name, "" );
		ch->pcdata->clan = NULL;
	    }
	    pager_printf( ch, "All Crimson Shadow members have been outcasted.\n\r" );
	    ch->pcdata->upgradeL = 10;
	}
	if (ch ->pcdata->upgradeL == 10 )
	{
		if( ch->pcdata->sparcount != 0)
			ch->pcdata->sparcount = 0;
		ch->pcdata->upgradeL = 11;
	}
	if (ch ->pcdata->upgradeL == 11 )
	{
		if( ch->pcdata->sparcount != 0)
			ch->pcdata->sparcount = 0;
		ch->pcdata->upgradeL = 12;
	}
        if (ch->pcdata->upgradeL == 12)
        {
                if( is_leader( ch ) )
                {
                        ch->pcdata->clanRank = 2;
			if( ch->sex == SEX_FEMALE )
				ch->pcdata->clan->fRank2Count += 1;
			else
				ch->pcdata->clan->mRank2Count += 1;
			ch->pcdata->clan->members += 1;
		}
		if( ch->pcdata->clan )
		{
                if( !str_cmp( ch->pcdata->clan->leader1, ch->name ) )
		{
                        ch->pcdata->clanRank = 1;
			if( ch->sex == SEX_FEMALE )
			{
				ch->pcdata->clan->fRank2Count -= 1;
				ch->pcdata->clan->fRank1Count += 1;
			}
			else
			{
				ch->pcdata->clan->mRank2Count -= 1;
				ch->pcdata->clan->mRank1Count += 1;
			}
			ch->pcdata->clan->members += 1;
                }
		}
		if( ch->pcdata->clan && !is_leader( ch ) )
		{
			ch->pcdata->clanRank = 7;
			if( ch->sex == SEX_FEMALE )
				ch->pcdata->clan->fRank7Count += 1;
			else
				ch->pcdata->clan->mRank7Count += 1;
			ch->pcdata->clan->members += 1;
		}
                ch->pcdata->upgradeL = 13;
                pager_printf( ch, "Updating clan structure.\n\r" );
        }

	if (ch->pcdata->upgradeL == 13)
	{
		char *pwdnew;

		pwdnew = sha256_crypt( ch->pcdata->pwd );
		DISPOSE( ch->pcdata->pwd );
		ch->pcdata->pwd = str_dup( pwdnew );
		pager_printf( ch, "Password encrypted.\n\r" );
		ch->pcdata->upgradeL = 14;
	}
	if (ch->pcdata->upgradeL == 14 )
	{
	  OBJ_DATA *obj, *obj_next;
	  OBJ_DATA *cobj, *cobj_next;
	  for( obj = ch->first_carrying; obj != NULL ; obj = obj_next )
	  {
	    obj_next = obj->next_content;

	    if ( obj->item_type == ITEM_CONTAINER )
	    {

	      for( cobj = obj->first_content; cobj != NULL ; cobj = cobj_next )
	      {
		cobj_next = cobj->next_content;
	        if( cobj->pIndexData->vnum == 100333 )
                {
                 if( IS_OBJ_STAT(cobj, ITEM_ANTI_NEUTRAL) )
                 {
                  pager_printf( ch, "Anti-neutral Back to the Wall found in your inventory.\n\r");
                  pager_printf( ch, "Stripping anti-neutral flag.\n\r");
                  xTOGGLE_BIT(cobj->extra_flags, ITEM_ANTI_NEUTRAL);
                 }
                }
                else if( cobj->pIndexData->vnum == 100335 )
                {
                 if( IS_OBJ_STAT(cobj, ITEM_ANTI_NEUTRAL) )
                 {
                  pager_printf( ch, "Anti-neutral Touch found in your inventory.\n\r");
                  pager_printf( ch, "Stripping anti-neutral flag.\n\r");
                  xTOGGLE_BIT(cobj->extra_flags, ITEM_ANTI_NEUTRAL);
                 }
                }
                else if( cobj->pIndexData->vnum == 100336 )
                {
                 if( IS_OBJ_STAT(cobj, ITEM_ANTI_NEUTRAL) )
                 {
                  pager_printf( ch, "Anti-neutral Matrix of Leadership found in your inventory.\n\r");
                  pager_printf( ch, "Stripping anti-neutral flag.\n\r");
                  xTOGGLE_BIT(cobj->extra_flags, ITEM_ANTI_NEUTRAL);
                 }
                }
	      }
	    }
	      if( obj->pIndexData->vnum == 100333 )
	      {
		if( IS_OBJ_STAT(obj, ITEM_ANTI_NEUTRAL) )
		{
		  pager_printf( ch, "Anti-neutral Back to the Wall found in your inventory.\n\r");
		  pager_printf( ch, "Stripping anti-neutral flag.\n\r");
		  xTOGGLE_BIT(obj->extra_flags, ITEM_ANTI_NEUTRAL);
		}
	      }
	      else if( obj->pIndexData->vnum == 100335 )
              {
                if( IS_OBJ_STAT(obj, ITEM_ANTI_NEUTRAL) )
                {
                  pager_printf( ch, "Anti-neutral Touch found in your inventory.\n\r");
                  pager_printf( ch, "Stripping anti-neutral flag.\n\r");
		  xTOGGLE_BIT(obj->extra_flags, ITEM_ANTI_NEUTRAL);
		}
	      }
              else if( obj->pIndexData->vnum == 100336 )
              {
                if( IS_OBJ_STAT(obj, ITEM_ANTI_NEUTRAL) )
                {
                  pager_printf( ch, "Anti-neutral Matrix of Leadership found in your inventory.\n\r");
                  pager_printf( ch, "Stripping anti-neutral flag.\n\r");
		  xTOGGLE_BIT(obj->extra_flags, ITEM_ANTI_NEUTRAL);
                }
              }
	  }
	  pager_printf( ch, "Snake way equipment updated successfully.\n\r");
	  ch->pcdata->upgradeL = 15;
	}
	if (ch->pcdata->upgradeL == 15 )
        {
	  ch->pcdata->auraColorPowerUp = 0;
	  pager_printf( ch, "Your aura color has been reset to the default.\n\r");
	  pager_printf( ch, "You must set your aura again using the AURA command.\n\r");
	  ch->pcdata->upgradeL = 16;
	}
        if (ch->pcdata->upgradeL == 16 )
        {
          OBJ_DATA *obj, *obj_next;
          OBJ_DATA *cobj, *cobj_next;
          for( obj = ch->first_carrying; obj != NULL ; obj = obj_next )
          {
            obj_next = obj->next_content;

            if ( obj->item_type == ITEM_CONTAINER )
            {

              for( cobj = obj->first_content; cobj != NULL ; cobj = cobj_next )
              {
                cobj_next = cobj->next_content;
                if( cobj->pIndexData->vnum == 100333 )
                {
                 if( !IS_OBJ_STAT(cobj, ITEM_ANTI_NEUTRAL) )
                 {
                  pager_printf( ch, "Back to the Wall found in your inventory.\n\r");
                  pager_printf( ch, "Re-adding anti-neutral flag.\n\r");
                  xTOGGLE_BIT(cobj->extra_flags, ITEM_ANTI_NEUTRAL);
                 }
                }
                else if( cobj->pIndexData->vnum == 100335 )
                {
                 if( !IS_OBJ_STAT(cobj, ITEM_ANTI_NEUTRAL) )
                 {
                  pager_printf( ch, "Touch found in your inventory.\n\r");
                  pager_printf( ch, "Re-adding anti-neutral flag.\n\r");
                  xTOGGLE_BIT(cobj->extra_flags, ITEM_ANTI_NEUTRAL);
                 }
                }
                else if( cobj->pIndexData->vnum == 100336 )
                {
                 if( !IS_OBJ_STAT(cobj, ITEM_ANTI_NEUTRAL) )
                 {
                  pager_printf( ch, "Matrix of Leadership found in your inventory.\n\r");
                  pager_printf( ch, "Re-adding anti-neutral flag.\n\r");
                  xTOGGLE_BIT(cobj->extra_flags, ITEM_ANTI_NEUTRAL);
                 }
                }
              }
            }
              if( obj->pIndexData->vnum == 100333 )
              {
                if( !IS_OBJ_STAT(obj, ITEM_ANTI_NEUTRAL) )
                {
                  pager_printf( ch, "Back to the Wall found in your inventory.\n\r");
                  pager_printf( ch, "Re-adding anti-neutral flag.\n\r");
                  xTOGGLE_BIT(obj->extra_flags, ITEM_ANTI_NEUTRAL);
                }
              }
              else if( obj->pIndexData->vnum == 100335 )
              {
                if( !IS_OBJ_STAT(obj, ITEM_ANTI_NEUTRAL) )
                {
                  pager_printf( ch, "Touch found in your inventory.\n\r");
                  pager_printf( ch, "Re-adding anti-neutral flag.\n\r");
                  xTOGGLE_BIT(obj->extra_flags, ITEM_ANTI_NEUTRAL);
                }
              }
              else if( obj->pIndexData->vnum == 100336 )
              {
                if( !IS_OBJ_STAT(obj, ITEM_ANTI_NEUTRAL) )
                {
                  pager_printf( ch, "Matrix of Leadership found in your inventory.\n\r");
                  pager_printf( ch, "Re-adding anti-neutral flag.\n\r");
                  xTOGGLE_BIT(obj->extra_flags, ITEM_ANTI_NEUTRAL);
                }
              }
          }
          pager_printf( ch, "Snake way equipment updated successfully.\n\r");
          ch->pcdata->upgradeL = 17;
        }

	if( ch->pcdata->upgradeL == 17 )
	{
	  if( ch->kairank > 0 )
	  {
	    pager_printf( ch, "&RKaioshin rank update: Stripping you of your rank.&D\n\r");
	    ch->kairank = 0;
	  }
	  if( ch->demonrank > 0 )
          {
            pager_printf( ch, "&RDemon rank update: Stripping you of your rank.&D\n\r");
            ch->demonrank = 0;
          }
	  pager_printf( ch, "Kaioshin and Demon ranks updated successfully.\n\r");
          ch->pcdata->upgradeL = 18;
	}

	pager_printf(ch, "Saving updated player data...\n\r");
	save_char_obj(ch);
	pager_printf(ch, "Saved.\n\r");
	return true;

}

int race_lookup( const char *name )
{
  int race;

  for( race = 0; race_table[race]->race_name[0] != '\0'; race ++ )
  {
    if( !strcasecmp( name, race_table[race]->race_name ) )
      return race;
  }

  return 0;
}

int class_lookup( const char *name )
{
  int class;

  for( class = 0; class_table[class]->who_name != NULL; class ++ )
  {
    if( !strcasecmp( name, class_table[class]->who_name ) )
      return class;
  }

  return 0;
}

void do_check_ld( CHAR_DATA *ch, char *argument )
{
  char       buf[MAX_STRING_LENGTH];
  CHAR_DATA *wch;
  int        count = 0;

  if( IS_NPC( ch ) )
  {
    send_to_char( "Sorry, not for mobiles.\n\r", ch );
    return;
  }

  sprintf( buf, "\n\rLink-Dead Player" );
  strcat(  buf, "\n\r----------------" );
  strcat(  buf, "\n\r" );
  send_to_pager( buf, ch );
  for( wch = first_char; wch; wch = wch->next )
  {
    if( !wch->desc && !IS_NPC( wch ) )
    {
      sprintf( buf, "%s", wch->name );
      strcat( buf, "\n\r" );
      send_to_pager( buf, ch );
      count++;
    }
  }
  sprintf( buf, "\n\rTotal Found: %d\n\r", count );
  send_to_pager( buf, ch );
}

//  Added by Saiyr.  Used to display clan ranks on the who list.
char * get_clan_rank( CHAR_DATA *ch )
{
  if( IS_NPC( ch ) )
    return "";
  if( !ch->pcdata->clan )
    return "";
  if( ch->sex == 1 || ch->sex == 0 )
  {
    if( ch->pcdata->clanRank == 1 )
      return ch->pcdata->clan->mRank1;
    if( ch->pcdata->clanRank == 2 )
      return ch->pcdata->clan->mRank2;
    if( ch->pcdata->clanRank == 3 )
      return ch->pcdata->clan->mRank3;
    if( ch->pcdata->clanRank == 4 )
      return ch->pcdata->clan->mRank4;
    if( ch->pcdata->clanRank == 5 )
      return ch->pcdata->clan->mRank5;
    if( ch->pcdata->clanRank == 6 )
      return ch->pcdata->clan->mRank6;
    if( ch->pcdata->clanRank == 7 )
      return ch->pcdata->clan->mRank7;
  }
  else
  {
    if( ch->pcdata->clanRank == 1 )
      return ch->pcdata->clan->fRank1;
    if( ch->pcdata->clanRank == 2 )
      return ch->pcdata->clan->fRank2;
    if( ch->pcdata->clanRank == 3 )
      return ch->pcdata->clan->fRank3;
    if( ch->pcdata->clanRank == 4 )
      return ch->pcdata->clan->fRank4;
    if( ch->pcdata->clanRank == 5 )
      return ch->pcdata->clan->fRank5;
    if( ch->pcdata->clanRank == 6 )
      return ch->pcdata->clan->fRank6;
    if( ch->pcdata->clanRank == 7 )
      return ch->pcdata->clan->fRank7;
  }
  return "";
}

bool has_phrase( char *searchphrase, char *querystring )
{
    int maxpos = strlen( querystring ) - strlen( searchphrase );
    int positioncounter = 0;
    int charactercounter = 0;
    int searchlen = strlen( searchphrase );
    bool retval = true;

    if( strlen( searchphrase ) == strlen( querystring ) )
    {
	if( !strcmp( searchphrase, querystring ) )
	    return true;
	else
	    return false;
    }

    while( positioncounter <= maxpos )
    {
	for( charactercounter = 0; charactercounter < searchlen; charactercounter++ )
	{
	    if( searchphrase[charactercounter] != querystring[positioncounter+charactercounter] )
	    {
		retval = false;
		break;
	    }
	}
	if( retval == true )
	    return true;
	retval = true;
	positioncounter++;
    }
    return false;
}

void obj_cost_recalc(OBJ_INDEX_DATA *obj)
{
	int cost = 0;
	AFFECT_DATA *iaf;
	int mod = 0;

	switch (obj->item_type)
	{
		case ITEM_LIGHT:
			cost = obj->value[2] * 100;
			break;
		case ITEM_WAND:
			cost = obj->value[2] * 1000;
			break;
		case ITEM_WEAPON:
			cost = obj->value[1] * obj->value[2] * 1000;
			break;
		case ITEM_ARMOR:
			cost = obj->value[5] * 100;
			break;
		case ITEM_CONTAINER:
			cost = obj->value[0] * 20;
			break;
		case ITEM_SCOUTER:
			cost = obj->cost * 10;
			break;
	}
	for( iaf = obj->first_affect; iaf; iaf = iaf->next )
	{
		if (!iaf)
			break;
		if (iaf->location == APPLY_STR || iaf->location == APPLY_DEX
			|| iaf->location == APPLY_INT || iaf->location == APPLY_CON)
			mod += iaf->modifier * 2500;
		else if (iaf->location == APPLY_LCK)
			mod += iaf->modifier * 5000;
		else if (iaf->location == APPLY_ALLSTATS)
			mod += iaf->modifier * 10000;
		else if (iaf->location == APPLY_MANA)
			mod += iaf->modifier * 500;
		else
			mod += iaf->modifier * 1000;
	}

	obj->cost = cost+mod;

	return;
}
