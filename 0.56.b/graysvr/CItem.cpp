//
// CItem.cpp
// Copyright Menace Software (www.menasoft.com).
//

#include "graysvr.h"	// predef header.

/*
	If you add a new trigger here, be sure to also and ALWAYS add a corresponding
	character trigger (even if it does not make sense)! CTRIG_item* will be automatically
	called when one of these triggers fired, depending on their index. So NEVER unalign the two lists!
*/
LPCTSTR const CItem::sm_szTrigName[ITRIG_QTY+1] =	// static
{
	"@AAAUNUSED",
	"@AfterClick",
	"@Buy",
	"@Click",
	"@ClientTooltip",	// Sending tooltip to a client
	"@ContextMenuRequest",
	"@ContextMenuSelect",
	"@Create",
	"@DAMAGE",				// I have been damaged in some way
#ifdef _ALPHASPHERE
	"@DAMAGEGIVEN",
#endif
	"@DCLICK",				// I have been dclicked.
	"@Destroy",				//+I am nearly destroyed
	"@DropOn_Char",			// I have been dropped on this char
	"@DropOn_Ground",		// I have been dropped on the ground here
	"@DropOn_Item",			// I have been dropped on this item
	"@DropOn_Self",			// An item has been dropped here
	"@EQUIP",		// I have been unequipped
    "@EQUIPTEST",
	"@FIRE",
	"@PICKUP_GROUND",	// I was picked up off the ground.
	"@PICKUP_PACK",	// picked up from inside some container.
	"@PICKUP_SELF", // picked up from here
	"@PICKUP_STACK",	// was picked up from a stack
	"@SELL",
	"@SpellEffect",		// cast some spell on me.
	"@STEP",			// I have been walked on.
	"@TARGON_CANCEL",
	"@TARGON_CHAR",
	"@TARGON_GROUND",
	"@TARGON_ITEM",	// I am being combined with an item
	"@TIMER",		// My timer has expired.
	"@ToolTip",
	"@UNEQUIP",		// i have been unequipped (or try to unequip)
	NULL,
};

/////////////////////////////////////////////////////////////////
// -CItem

CItem::CItem( ITEMID_TYPE id, CItemBase * pItemDef ) : CObjBase( true )
{
	ASSERT( pItemDef );

	g_Serv.StatInc(SERV_STAT_ITEMS);
	m_Attr = 0;
	m_amount = 1;
	m_containedGridIndex = 0;

	m_itNormal.m_more1 = 0;
	m_itNormal.m_more2 = 0;
	m_itNormal.m_morep.ZeroPoint();
	m_itWeapon.m_dmgcold = 0;
	m_itWeapon.m_dmgenergy = 0;
	m_itWeapon.m_dmgfire = 0;
	m_itWeapon.m_dmgpoison = 0;

	SetBase( pItemDef );
	SetDispID( id );

	g_World.m_uidLastNewItem = GetUID();	// for script access.
	ASSERT( IsDisconnected());
}

void CItem::Delete()
{
	if ( !IsSetEF(EF_Minimize_Triggers) )
	{
		//We can forbid the deletion in here with no pain
		if (CItem::OnTrigger(ITRIG_DESTROY,&g_Serv) == TRIGRET_RET_TRUE)
			return;
	}

	CObjBase::Delete();
}

CItem::~CItem()
{
	DeletePrepare();	// Must remove early because virtuals will fail in child destructor.
	if ( ! g_Serv.IsLoading())
	switch ( m_type )
	{
		case IT_SPAWN_CHAR:
			Spawn_KillChildren();
			break;
		case IT_FIGURINE:
		case IT_EQ_HORSE:
			{	// remove the ridden or linked char.
				CChar * pHorse = m_itFigurine.m_UID.CharFind();
				if ( pHorse && pHorse->IsDisconnected() && ! pHorse->m_pPlayer )
				{
					pHorse->m_atRidden.m_FigurineUID.InitUID();
					pHorse->Delete();
				}
			}
			break;
	}
	g_Serv.StatDec(SERV_STAT_ITEMS);
}

CItem * CItem::CreateBase( ITEMID_TYPE id )	// static
{
	ADDTOCALLSTACK("CItem::CreateBase");
	// All CItem creates come thru here.
	// NOTE: This is a free floating item.
	//  If not put on ground or in container after this = Memory leak !

	ITEMID_TYPE idErrorMsg = ITEMID_NOTHING;
	CItemBase * pItemDef = CItemBase::FindItemBase( id );
	if ( pItemDef == NULL )
	{
		idErrorMsg = id;
		id = (ITEMID_TYPE) g_Cfg.ResourceGetIndexType( RES_ITEMDEF, "DEFAULTITEM" );
		if ( id <= 0 )
		{
			id = ITEMID_GOLD_C1;
		}
		pItemDef = CItemBase::FindItemBase( id );
		ASSERT(pItemDef);
	}

	CItem * pItem;
	switch ( pItemDef->GetType() )
	{
		case IT_MAP:
		case IT_MAP_BLANK:
			pItem = new CItemMap( id, pItemDef );
			break;
		case IT_COMM_CRYSTAL:
			pItem = new CItemCommCrystal( id, pItemDef );
			break;
		case IT_GAME_BOARD:
		case IT_BBOARD:
		case IT_TRASH_CAN:
		case IT_CONTAINER_LOCKED:
		case IT_CONTAINER:
		case IT_EQ_TRADE_WINDOW:
		case IT_EQ_VENDOR_BOX:
		case IT_EQ_BANK_BOX:
		case IT_KEYRING:
		case IT_SHIP_HOLD_LOCK:
		case IT_SHIP_HOLD:
			pItem = new CItemContainer( id, pItemDef );
			break;
		case IT_CORPSE:
			pItem = new CItemCorpse( id, pItemDef );
			break;
		case IT_MESSAGE:
		case IT_BOOK:
			// A message for a bboard or book text.
			pItem = new CItemMessage( id, pItemDef );
			break;
		case IT_STONE_GUILD:
		case IT_STONE_TOWN:
			pItem = new CItemStone( id, pItemDef );
			break;
		case IT_MULTI:
			pItem = new CItemMulti( id, pItemDef );
			break;
		case IT_MULTI_CUSTOM:
			pItem = new CItemMultiCustom( id, pItemDef );
			break;
		case IT_SHIP:
			pItem = new CItemShip( id, pItemDef );
			break;
		case IT_EQ_MEMORY_OBJ:
			pItem = new CItemMemory( id, pItemDef );
			break;
		case IT_EQ_SCRIPT:	// pure script.with TAG(s) support
		case IT_SCRIPT:
			pItem = new CItemScript( id, pItemDef );
			break;
		default:
			if ( pItemDef->GetMakeValue(0))
				pItem = new CItemVendable( id, pItemDef );
			else
				pItem = new CItem( id, pItemDef );
			break;
	}

	ASSERT( pItem );
	if ( idErrorMsg && idErrorMsg != -1 )
	{
		DEBUG_ERR(( "CreateBase invalid item 0%x\n", idErrorMsg ));
	}
	return( pItem );
}

CItem * CItem::CreateDupeItem( const CItem * pItem, CChar * pSrc, bool fSetNew )	// static
{
	ADDTOCALLSTACK("CItem::CreateDupeItem");
	// Dupe this item.
	if ( pItem == NULL )
		return( NULL );
	CItem * pItemNew = CreateBase( pItem->GetID());
	ASSERT(pItemNew);
	pItemNew->DupeCopy( pItem );

	if ( pSrc )
		pSrc->m_Act_Targ = pItemNew->GetUID();

	if ( fSetNew )
		g_World.m_uidNew = pItemNew->GetUID();

	return( pItemNew );
}

CItem * CItem::CreateScript( ITEMID_TYPE id, CChar * pSrc ) // static
{
	ADDTOCALLSTACK("CItem::CreateScript");
	// Create item from the script id.

	CItem * pItem = CreateBase( id );
	ASSERT( pItem );

	// default stuff for the item.
	switch ( pItem->GetType())
	{
		case IT_CONTAINER_LOCKED:
			{
				// At this level it just means to add a key for it.
				CItemContainer * pCont = dynamic_cast <CItemContainer *> ( pItem );
				ASSERT(pCont);
				pCont->MakeKey();
			}
			break;
		case IT_CORPSE:
			{
				//	initialize TAG.BLOOD as the amount of blood inside
				CItemBase	*pItemDef = pItem->Item_GetDef();
				int iBlood = 0;
				if ( pItemDef )
				{
					iBlood = pItemDef->m_TagDefs.GetKeyNum("MAXBLOOD", true);
				}
				if ( !iBlood )
					iBlood = 5;
				else if ( iBlood < 0 )
					iBlood = 0;
				pItem->m_TagDefs.SetNum("BLOOD", iBlood, true);
			}
			break;
	}

	// call the ON=@Create trigger
	CItemBase * pItemDef = pItem->Item_GetDef();
	ASSERT( pItemDef );

	if ( pItemDef->HasTrigger(ITRIG_Create) )
	{
		CResourceLock s;
		if ( pItemDef->ResourceLock(s))
		{
			if ( pSrc )
				pItem->OnTriggerScript( s, sm_szTrigName[ITRIG_Create], pSrc );
			else
				pItem->OnTriggerScript( s, sm_szTrigName[ITRIG_Create], &g_Serv );
		}
	}

	if ( pSrc && pSrc->IsClient() )
	{
		CScriptTriggerArgs	args;
		args.m_pO1 = pItem;
		pSrc->OnTrigger("@ItemCreate", pSrc, &args);
	}

	return( pItem );
}

CItem * CItem::CreateHeader( TCHAR * pArg, CObjBase * pCont, bool fDupeCheck, CChar * pSrc )
{
	ADDTOCALLSTACK("CItem::CreateHeader");
	// Just read info on a single item carryed by a CChar.
	// ITEM=#id,#amount,R#chance

	RESOURCE_ID rid = g_Cfg.ResourceGetID( RES_ITEMDEF, (LPCTSTR&)pArg );
	if ( ! rid.IsValidUID())
		return( NULL );
	if ( rid.GetResType() != RES_ITEMDEF && rid.GetResType() != RES_TEMPLATE )
		return( NULL );
	if ( rid.GetResIndex() == 0 )
		return( NULL );

	int amount = 1;
	if ( Str_Parse( pArg, &pArg ))
	{
		if ( pArg[0] != 'R' )
		{
			amount = Exp_GetVal( pArg );
			Str_Parse( pArg, &pArg );
		}
		if ( pArg[0] == 'R' )
		{
			// 1 in x chance of creating this.
			if ( Calc_GetRandVal( ATOI( pArg+1 )))
				return( NULL );	// don't create it
		}
	}

	if ( amount == 0 )
		return( NULL );

	ITEMID_TYPE id = (ITEMID_TYPE) rid.GetResIndex();

	if ( fDupeCheck && rid.GetResType() == RES_ITEMDEF && pCont )
	{
		// Check if they already have the item ? In the case of a regen.
		// This is just to keep NEWBIE items from being duped.
		CContainer * pContBase = dynamic_cast <CContainer *> ( pCont );
		ASSERT(pContBase);
		if ( pContBase->ContentFind( rid ))
		{
			// We already have this.
			return( NULL );
		}
	}

	CItem * pItem = CItem::CreateTemplate( id, (rid.GetResType() == RES_ITEMDEF? NULL:pCont), pSrc );
	if ( pItem != NULL )
	{
		// Is the item movable ?
		if ( ! pItem->IsMovableType() && pCont && pCont->IsItem())
		{
			DEBUG_ERR(( "Script Error: 0%x item is not movable type, cont=0%x\n", id, (DWORD)(pCont->GetUID()) ));
			pItem->Delete();
			return( NULL );
		}

		if ( amount != 1 )
		{
			pItem->SetAmount( amount );
		}

		// Items should have their container set after their amount to
		// avoid stacking issues.
		if ( pCont && rid.GetResType() == RES_ITEMDEF )
		{
			CContainer * pContBase = dynamic_cast <CContainer *> ( pCont );
			ASSERT(pContBase);
			pContBase->ContentAdd(pItem);
		}
	}
	return( pItem );
}

LPCTSTR const CItem::sm_szTemplateTable[ITC_QTY+1] =
{
	"BUY",
	"CONTAINER",
	"FULLINTERP",
	"ITEM",
	"ITEMNEWBIE",
	"NEWBIESWAP",
	"SELL",
	NULL,
};

CItem * CItem::CreateTemplate( ITEMID_TYPE id, CObjBase * pCont, CChar * pSrc )	// static
{
	ADDTOCALLSTACK("CItem::CreateTemplate");
	// Create an item or a template.
	// A template is a collection of items. (not a single item)
	// But we can create (in this case) only the container item.
	// and return that.
	// ARGS:
	//  pCont = container item or char to put the new template or item in..
	// NOTE:
	//  This can possibly be reentrant under the current scheme.

	if ( id < ITEMID_TEMPLATE )
	{
		CItem * pItem = CreateScript( id, pSrc );
		if ( pCont )
		{
			CContainer * pContBase = dynamic_cast <CContainer *> ( pCont );
			ASSERT(pContBase);
			pContBase->ContentAdd(pItem);
		}
		return( pItem );
	}

	CResourceLock s;
	if ( ! g_Cfg.ResourceLock( s, RESOURCE_ID( RES_TEMPLATE, id )))
		return NULL;

	return( ReadTemplate( s, pCont ));
}

CItem * CItem::ReadTemplate( CResourceLock & s, CObjBase * pCont ) // static
{
	ADDTOCALLSTACK("CItem::ReadTemplate");
	CChar * pVendor = NULL;
	CItemContainer * pVendorSell = NULL;
	CItemContainer * pVendorBuy = NULL;
	if ( pCont != NULL )
	{
		pVendor = dynamic_cast <CChar*>( pCont->GetTopLevelObj());
		if ( pVendor != NULL && pVendor->NPC_IsVendor())
		{
			pVendorSell = pVendor->GetBank( LAYER_VENDOR_STOCK );
			pVendorBuy = pVendor->GetBank( LAYER_VENDOR_BUYS );
		}
	}

	bool fItemAttrib = false;
	CItem * pNewTopCont = NULL;
	CItem * pItem = NULL;
	while ( s.ReadKeyParse())
	{
		if ( s.IsKeyHead( "ON", 2 ))
			break;

		int index = FindTableSorted( s.GetKey(), sm_szTemplateTable, COUNTOF( sm_szTemplateTable )-1 );
		switch (index)
		{
			case ITC_BUY: // "BUY"
			case ITC_SELL: // "SELL"
				fItemAttrib = false;
				if (pVendorBuy != NULL)
				{
					pItem = CItem::CreateHeader( s.GetArgRaw(), (index==ITC_SELL)?pVendorSell:pVendorBuy, false );
					if ( pItem == NULL )
					{
						continue;
					}
					if ( pItem->IsItemInContainer())
					{
						fItemAttrib = true;
						pItem->SetContainedLayer( pItem->GetAmount());	// set the Restock amount.
					}
				}
				continue;
	
			case ITC_CONTAINER:
				fItemAttrib = false;
				{
					pItem = CItem::CreateHeader( s.GetArgRaw(), pCont, false, pVendor );
					if ( pItem == NULL )
					{
						continue;
					}
					pCont = dynamic_cast <CItemContainer *> ( pItem );
					if ( pCont == NULL )
					{
						DEBUG_ERR(( "CreateTemplate CContainer %s is not a container\n", pItem->GetResourceName() ));
					}
					else
					{
						fItemAttrib = true;
						if ( ! pNewTopCont )
							pNewTopCont = pItem;
					}
				}
				continue;
	
			case ITC_ITEM:
			case ITC_ITEMNEWBIE:
				fItemAttrib = false;
				if ( pCont == NULL && pItem != NULL )
				{
					// Don't create anymore items til we have some place to put them !
					continue;
				}
				pItem = CItem::CreateHeader( s.GetArgRaw(), pCont, false, pVendor );
				if ( pItem != NULL )
				{
					fItemAttrib = true;
				}
				continue;
		}

		if ( pItem != NULL && fItemAttrib )
		{
			pItem->r_LoadVal( s );
		}
	}

	if ( pNewTopCont )
	{
		return pNewTopCont;
	}
	return( pItem );
}

bool CItem::IsTopLevelMultiLocked() const
{
	ADDTOCALLSTACK("CItem::IsTopLevelMultiLocked");
	// Is this item locked to the structure ?
	ASSERT( IsTopLevel());
	if ( ! m_uidLink.IsValidUID())	// linked to something.
		return( false );
	if ( IsType(IT_KEY))	// keys cannot be locked down. 
		return( false );
	const CRegionBase * pArea = GetTopPoint().GetRegion( REGION_TYPE_MULTI );
	if ( pArea == NULL )
		return( false );
	if ( pArea->GetResourceID() == m_uidLink )
		return( true );
	return( false );
}

bool CItem::IsMovableType() const
{
	ADDTOCALLSTACK("CItem::IsMovableType");
	if ( IsAttr( ATTR_MOVE_ALWAYS ))	// this overrides other flags.
		return( true );
	if ( IsAttr( ATTR_MOVE_NEVER | ATTR_STATIC | ATTR_INVIS ))
		return( false );
	const CItemBase * pItemDef = Item_GetDef();
	ASSERT(pItemDef);
	if ( ! pItemDef->IsMovableType())
		return( false );
	return( true );
}

bool CItem::IsMovable() const
{
	ADDTOCALLSTACK("CItem::IsMovable");
	if ( ! IsTopLevel() && ! IsAttr( ATTR_MOVE_NEVER ))
	{
		// If it's in my pack (event though not normally movable) thats ok.
		return( true );
	}
	return( IsMovableType());
}

int CItem::IsWeird() const
{
	ADDTOCALLSTACK("CItem::IsWeird");
	// Does item i have a "link to reality"?
	// (Is the container it is in still there)
	// RETURN: 0 = success ok
	int iResultCode = CObjBase::IsWeird();
	if ( iResultCode )
	{
		return( iResultCode );
	}
	CItemBase * pItemDef = Item_GetDef();
	if ( pItemDef == NULL )
	{
		iResultCode = 0x2102;
		return( iResultCode );
	}
	if ( pItemDef->GetID() == 0 )
	{
		iResultCode = 0x2103;
		return( iResultCode );
	}
	if ( IsDisconnected() )	// Should be connected to the world.
	{
		iResultCode = 0x2104;
		return( iResultCode );
	}
	if ( IsTopLevel())
	{
		// no container = must be at valid point.
		if ( ! GetTopPoint().IsValidPoint())
		{
			iResultCode = 0x2105;
			return( iResultCode );
		}
		return( 0 );
	}

	// The container must be valid.
	CObjBase * ptCont = GetContainer(); 
	return( ( ptCont == NULL ) ? 0x2106 : ptCont->IsWeird() );
}

CItem * CItem::SetType(IT_TYPE type)
{
	ADDTOCALLSTACK("CItem::SetType");
	m_type = type;
	return this;
}

int CItem::FixWeirdness()
{
	ADDTOCALLSTACK("CItem::FixWeirdness");
	// Check for weirdness and fix it if possible.
	// RETURN: false = i can't fix this.

	if ( IsType(IT_EQ_MEMORY_OBJ) && ! IsValidUID())
	{
		SetUID( UID_CLEAR, true );	// some cases we don't get our UID because we are created during load.
	}

	int iResultCode = CObjBase::IsWeird();
	if ( iResultCode )
	{
		return( iResultCode );
	}

	CItemBase * pItemDef = Item_GetDef();
	ASSERT(pItemDef);

	CChar * pChar;
	if ( IsItemEquipped())
	{
		pChar = dynamic_cast <CChar*>( GetParent());
		if ( ! pChar )
		{
			iResultCode = 0x2202;
			return( iResultCode );
		}
	}
	else pChar = NULL;

	// Make sure the link is valid.
	if ( m_uidLink.IsValidUID())
	{
		// Make sure all my memories are valid !
		if ( m_uidLink == GetUID() || m_uidLink.ObjFind() == NULL )
		{
			if ( m_type == IT_EQ_MEMORY_OBJ )
			{
				return( false );	// get rid of it.	(this is not an ERROR per se)
			}
			if ( IsAttr(ATTR_STOLEN))
			{
				// The item has been laundered.
				m_uidLink.InitUID();
			}
			else
			{
				DEBUG_ERR(( "'%s' Bad Link to 0%lx\n", (LPCTSTR) GetName(), (DWORD) m_uidLink ));
				m_uidLink.InitUID();
				iResultCode = 0x2205;
				return( iResultCode );	// get rid of it.
			}
		}
	}

	// Check Amount - Only stackable items should set amount ?

	if ( GetAmount() != 1 &&
		! IsType( IT_SPAWN_CHAR ) &&
		! IsType( IT_SPAWN_ITEM ) &&
		! IsStackableException())
	{
		// This should only exist in vendors restock boxes & spawns.
		if ( ! GetAmount())
		{
			SetAmount( 1 );
		}
		// stacks of these should not exist.
		else if ( ! pItemDef->IsStackableType() &&
			! IsType( IT_CORPSE ) &&
			! IsType( IT_EQ_MEMORY_OBJ ))
		{
			SetAmount( 1 );
		}
	}

	// Check Attributes

	//	allow bless/curse for memory items only, and of course deny both blessed+cursed
	if ( !IsType(IT_EQ_MEMORY_OBJ) )
		ClrAttr(ATTR_CURSED|ATTR_CURSED2|ATTR_BLESSED|ATTR_BLESSED2);
	else if ( IsAttr(ATTR_CURSED|ATTR_CURSED2) && IsAttr(ATTR_BLESSED|ATTR_BLESSED2) )
		ClrAttr( ATTR_CURSED|ATTR_CURSED2|ATTR_BLESSED|ATTR_BLESSED2 );

	if ( IsMovableType() )
	{
		if ( pItemDef->IsType(IT_WATER) || pItemDef->Can(CAN_I_WATER) )
			SetAttr(ATTR_MOVE_NEVER);
	}

	switch ( GetID())
	{
		case ITEMID_SPELLBOOK2:	// weird client bug with these.
			SetDispID( ITEMID_SPELLBOOK );
			break;
	
		case ITEMID_DEATHSHROUD:	// be on a dead person only.
		case ITEMID_GM_ROBE:
			{
				CChar * pChar = dynamic_cast<CChar*>( GetTopLevelObj());
				if ( pChar == NULL )
				{
					iResultCode = 0x2206;
					return( iResultCode );	// get rid of it.
				}
				if ( GetID() == ITEMID_DEATHSHROUD )
				{
					if ( IsAttr( ATTR_MAGIC ) && IsAttr( ATTR_NEWBIE ))
						break;	// special
					if ( ! pChar->IsStatFlag( STATF_DEAD ))
					{
						iResultCode = 0x2207;
						return( iResultCode );	// get rid of it.
					}
				}
				else
				{
					// Only GM/counsel can have robe.
					// Not a GM til read *ACCT.SCP
					if ( pChar->GetPrivLevel() < PLEVEL_Counsel )
					{
						iResultCode = 0x2208;
						return( iResultCode );	// get rid of it.
					}
				}
			}
			break;
	
		case ITEMID_VENDOR_BOX:
			if ( ! IsItemEquipped())
			{
				SetID(ITEMID_BACKPACK);
			}
			else if ( GetEquipLayer() == LAYER_BANKBOX )
			{
				SetID(ITEMID_BANK_BOX);
			}
			else
			{
				SetType(IT_EQ_VENDOR_BOX);
			}
			break;
	
		case ITEMID_BANK_BOX:
			// These should only be bank boxes and that is it !
			if ( ! IsItemEquipped())
			{
				SetID(ITEMID_BACKPACK);
			}
			else if ( GetEquipLayer() != LAYER_BANKBOX )
			{
				SetID(ITEMID_VENDOR_BOX);
			}
			else
			{
				SetType( IT_EQ_BANK_BOX );
			}
			break;
	
		case ITEMID_MEMORY:
			// Memory or other flag in my head.
			// Should not exist except equipped.
			if ( ! IsItemEquipped())
			{
				iResultCode = 0x2222;
				return( iResultCode );	// get rid of it.
			}
			break;
	}

	switch ( GetType() )
	{
		case IT_EQ_TRADE_WINDOW:
			// Should not exist except equipped.
			if ( ! IsItemEquipped() ||
				GetEquipLayer() != LAYER_NONE ||
				pChar->m_pPlayer == NULL )
			{
				iResultCode = 0x2220;
				return( iResultCode );	// get rid of it.
			}
			break;
	
		case IT_EQ_CLIENT_LINGER:
			// Should not exist except equipped.
			if ( ! IsItemEquipped() ||
				GetEquipLayer() != LAYER_FLAG_ClientLinger ||
				pChar->m_pPlayer == NULL )
			{
				iResultCode = 0x2221;
				return( iResultCode );	// get rid of it.
			}
			break;
	
		case IT_EQ_MEMORY_OBJ:
			{
				// Should not exist except equipped.
				CItemMemory * pItemTemp = dynamic_cast <CItemMemory*>(this);
				if ( pItemTemp == NULL )
				{
					iResultCode = 0x2222;
					return( iResultCode );	// get rid of it.
				}
			}
			break;
	
		case IT_EQ_HORSE:
			// These should only exist eqipped.
			if ( ! IsItemEquipped() || GetEquipLayer() != LAYER_HORSE )
			{
				iResultCode = 0x2226;
				return( iResultCode );	// get rid of it.
			}
			SetTimeout( 10*TICK_PER_SEC );
			break;
	
		case IT_HAIR:
		case IT_BEARD:	// 62 = just for grouping purposes.
			// Hair should only be on person or equipped. (can get lost in pack)
			// Hair should only be on person or on corpse.
			if ( ! IsItemEquipped())
			{
				CItemContainer * pCont = dynamic_cast <CItemContainer*> (GetParent());
				if ( pCont == NULL ||
					( pCont->GetID() != ITEMID_CORPSE && pCont->GetID() != ITEMID_VENDOR_BOX ))
				{
					iResultCode = 0x2227;
					return( iResultCode );	// get rid of it.
				}
			}
			else
			{
				SetAttr( ATTR_MOVE_NEVER );
				if ( GetEquipLayer() != LAYER_HAIR && GetEquipLayer() != LAYER_BEARD )
				{
					iResultCode = 0x2228;
					return( iResultCode );	// get rid of it.
				}
			}
			break;
	
		case IT_GAME_PIECE:
			// This should only be in a game.
			if ( ! IsItemInContainer())
			{
				iResultCode = 0x2229;
				return( iResultCode );	// get rid of it.
			}
			break;
	
		case IT_KEY:
			// blank unlinked keys.
			if ( m_itKey.m_lockUID && ! IsValidUID())
			{
				DEBUG_ERR(( "Key '%s' has bad link to 0%lx, �blanked out\n", (LPCTSTR) GetName(), (DWORD) m_itKey.m_lockUID ));
				m_itKey.m_lockUID.ClearUID();
			}
			break;
	
		case IT_SPAWN_CHAR:
		case IT_SPAWN_ITEM:
			Spawn_FixDef();
			Spawn_SetTrackID();
			break;
	
		case IT_CONTAINER_LOCKED:
		case IT_SHIP_HOLD_LOCK:
		case IT_DOOR_LOCKED:
			// Doors and containers must have a lock complexity set.
			if ( ! m_itContainer.m_lock_complexity )
			{
				m_itContainer.m_lock_complexity = 500 + Calc_GetRandVal( 600 );
			}
			break;
	
		case IT_POTION:
			if ( m_itPotion.m_skillquality == 0 ) // store bought ?
			{
				m_itPotion.m_skillquality = Calc_GetRandVal(950);
			}
			break;
		case IT_MAP_BLANK:
			if ( m_itNormal.m_more1 || m_itNormal.m_more2 )
			{
				SetType( IT_MAP );
			}
			break;
	
		default:
			if ( GetType() > IT_QTY )
			{
				if ( GetType() < IT_TRIGGER )
				{
					m_type = pItemDef->GetType();
				}
			}
	}

	if ( IsItemEquipped())
	{
		ASSERT( pChar );

		switch ( GetEquipLayer())
		{
			case LAYER_NONE:
				// Only Trade windows should be equipped this way..
				if ( m_type != IT_EQ_TRADE_WINDOW )
				{
					iResultCode = 0x2230;
					return( iResultCode );	// get rid of it.
				}
				break;
			case LAYER_SPECIAL:
				switch ( GetType())
				{
				case IT_EQ_MEMORY_OBJ:
				case IT_EQ_SCRIPT:	// pure script.
					break;
				default:
					iResultCode = 0x2231;
					return( iResultCode );	// get rid of it.
				}
				break;
			case LAYER_VENDOR_STOCK:
			case LAYER_VENDOR_EXTRA:
			case LAYER_VENDOR_BUYS:
				if ( pChar->m_pPlayer )	// players never need carry these,
					return( false );
				SetAttr(ATTR_MOVE_NEVER);
				break;
	
			case LAYER_PACK:
			case LAYER_BANKBOX:
				SetAttr(ATTR_MOVE_NEVER);
				break;
	
			case LAYER_HORSE:
				if ( m_type != IT_EQ_HORSE )
				{
					iResultCode = 0x2233;
					return( iResultCode );	// get rid of it.
				}
				SetAttr(ATTR_MOVE_NEVER);
				pChar->StatFlag_Set( STATF_OnHorse );
				break;
			case LAYER_FLAG_ClientLinger:
				if ( m_type != IT_EQ_CLIENT_LINGER )
				{
					iResultCode = 0x2234;
					return( iResultCode );	// get rid of it.
				}
				break;
	
			case LAYER_FLAG_Murders:
				if ( ! pChar->m_pPlayer ||
					pChar->m_pPlayer->m_wMurders <= 0 )
				{
					iResultCode = 0x2235;
					return( iResultCode );	// get rid of it.
				}
				break;
		}
	}

	else if ( IsTopLevel())
	{
		if ( IsAttr(ATTR_DECAY) && ! IsTimerSet())
		{
			iResultCode = 0x2236;
			return( iResultCode );	// get rid of it.
		}

		// unreasonably long for a top level item ?
		if ( GetTimerAdjusted() > 90*24*60*60 )
			SetTimeout(60*60);
	}

	// is m_BaseDef just set bad ?
	return( IsWeird());
}

CItem * CItem::UnStackSplit( int amount, CChar * pCharSrc )
{
	ADDTOCALLSTACK("CItem::UnStackSplit");
	// Set this item to have this amount.
	// leave another pile behind.
	// can be 0 size if on vendor.
	// ARGS:
	//  amount = the amount that i want to set this pile to
	// RETURN: 
	//  The newly created item.

	if ( amount >= GetAmount() )
		return( NULL );

	ASSERT( amount <= GetAmount());
	CItem * pItemNew = CreateDupeItem( this );
	pItemNew->SetAmount( GetAmount() - amount );
	SetAmountUpdate( amount );

	if ( ! pItemNew->MoveNearObj( this ))
	{
		if ( pCharSrc )
		{
			pCharSrc->ItemBounce( pItemNew );
		}
		else
		{
			// No place to put this item !
			pItemNew->Delete();
		}
	}

	return( pItemNew );
}

bool CItem::IsSameType( const CObjBase * pObj ) const
{
	ADDTOCALLSTACK("CItem::IsSameType");
	const CItem * pItem = dynamic_cast <const CItem*> ( pObj );
	if ( pItem == NULL )
		return( false );

	if ( GetID() != pItem->GetID() )
		return( false );
	if ( GetHue() != pItem->GetHue())
		return( false );
	if ( m_type != pItem->m_type )
		return( false );
	if ( m_uidLink != pItem->m_uidLink )
		return( false );

	if ( m_itNormal.m_more1 != pItem->m_itNormal.m_more1 )
		return( false );
	if ( m_itNormal.m_more2 != pItem->m_itNormal.m_more2 )
		return( false );
	if ( m_itNormal.m_morep != pItem->m_itNormal.m_morep )
		return( false );

	return( true );
}

bool CItem::IsStackableException() const
{
	ADDTOCALLSTACK("CItem::IsStackableException");
	// IS this normally unstackable type item now stackable ?
	// NOTE: This means the m_amount can be = 0

	if ( IsTopLevel() && IsAttr( ATTR_INVIS ))
		return( true );	// resource tracker.

	// In a vendors box ?
	const CItemContainer * pCont = dynamic_cast <const CItemContainer*>( GetParent());
	if ( pCont == NULL )
		return( false );
	if ( ! pCont->IsType(IT_EQ_VENDOR_BOX) && ! pCont->IsAttr(ATTR_MAGIC))
		return( false );
	return( true );
}

bool CItem::IsStackable( const CItem * pItem ) const
{
	ADDTOCALLSTACK("CItem::IsStackable");
	CItemBase * pItemDef = Item_GetDef();
	ASSERT(pItemDef);

	if ( ! pItemDef->IsStackableType())
	{
		// Vendor boxes can stack normally un-stackable stuff.
		if ( ! pItem->IsStackableException())
			return( false );
	}

	// try object rotations ex. left and right hand hides ?
	if ( !IsSameType(pItem) )
		return false;

	// total should not add up to > 64K !!!
	if ( pItem->GetAmount() > ( USHRT_MAX - GetAmount() - 1))
		return false;

	return true;
}

bool CItem::Stack( CItem * pItem )
{
	ADDTOCALLSTACK("CItem::Stack");
	// RETURN:
	//  true = the item got stacked. (it is gone)
	//  false = the item will not stack. (do somethjing else with it)
	//

	if ( pItem == this )
		return( true );
	if ( !IsStackable(pItem) )
		return false;

	// Lost newbie status.
	if ( IsAttr( ATTR_NEWBIE ) != pItem->IsAttr( ATTR_NEWBIE )) return false;
	else if ( IsAttr( ATTR_MOVE_NEVER ) != pItem->IsAttr( ATTR_MOVE_NEVER )) return false;
	else if ( IsAttr( ATTR_STATIC ) != pItem->IsAttr( ATTR_STATIC )) return false;

	SetAmount( pItem->GetAmount() + GetAmount());
	pItem->Delete();
	return( true );
}

int CItem::GetDecayTime() const
{
	ADDTOCALLSTACK("CItem::GetDecayTime");
	// Return time in seconds that it will take to decay this item.
	// -1 = never decays.

	switch ( GetType())
	{
	case IT_FOLIAGE:
	case IT_CROPS: // Crops "decay" as they grow
		return g_World.GetTimeDiff(g_World.GetNextNewMoon( ( GetTopPoint().m_map == 1 ) ? false : true ) +
			Calc_GetRandVal(20) * g_Cfg.m_iGameMinuteLength);
	case IT_MULTI:
	case IT_SHIP:
	case IT_MULTI_CUSTOM:
		// very long decay updated as people use it.
		return( 14*24*60*60*TICK_PER_SEC );	// days to rot a structure.
	case IT_TRASH_CAN:
		// Empties every n seconds.
		return( 15*TICK_PER_SEC );
	}

	if ( IsAttr(ATTR_CAN_DECAY|ATTR_STATIC|ATTR_MOVE_NEVER) || !IsMovableType() )
		return -1;

	if ( IsAttr(ATTR_MAGIC) )			//	magics destroyed later
		return ( 4*g_Cfg.m_iDecay_Item );
	if ( IsAttr(ATTR_NEWBIE) )			//	but newbie faster
		return ( g_Cfg.m_iDecay_Item / 2 );

	return g_Cfg.m_iDecay_Item;
}

void CItem::SetTimeout( int iDelay )
{
	ADDTOCALLSTACK("CItem::SetTimeout");
	// PURPOSE:
	//  Set delay in TICK_PER_SEC of a sec.
	//  -1 = never.
	// NOTE:
	//  It may be a decay timer or it might be a trigger timer

	CObjBase::SetTimeout( iDelay );

	// Items on the ground must be put in sector list correctly.
	if ( !IsTopLevel() )
		return;

	CSector * pSector = GetTopSector();
	if ( !pSector )
		return;

	CItemsList::sm_fNotAMove = true;
	pSector->MoveItemToSector( this, iDelay >= 0 );
	CItemsList::sm_fNotAMove = false;
	SetContainerFlags(0);
}

void CItem::SetDecayTime( int iTime )
{
	ADDTOCALLSTACK("CItem::SetDecayTime");
	// iTime = 0 = set default. (TICK_PER_SEC of a sec)
	// -1 = set none. (clear it)

	if ( IsTimerSet() && ! IsAttr(ATTR_DECAY))
	{
		return;	// already a timer here. let it expire on it's own
	}
	if ( ! iTime )
	{
		if ( IsTopLevel())
		{
			iTime = GetDecayTime();
		}
		else
		{
			iTime = -1;
		}
	}
	SetTimeout( iTime );
	if ( iTime != -1 )
	{
		SetAttr(ATTR_DECAY);
	}
	else
	{
		ClrAttr(ATTR_DECAY);
	}
}

SOUND_TYPE CItem::GetDropSound( const CObjBase * pObjOn ) const
{
	ADDTOCALLSTACK("CItem::GetDropSound");
	// Get a special drop sound for the item.
	CItemBase * pItemDef = Item_GetDef();
	ASSERT(pItemDef);
	SOUND_TYPE iSnd = NULL;

	switch ( pItemDef->GetType())
	{
	case IT_COIN:
	case IT_GOLD:
		// depends on amount.
		switch ( GetAmount())
		{
		case 1: iSnd = 0x035; break;
		case 2: iSnd = 0x032; break;
		case 3:
		case 4:	iSnd = 0x036; break;
		default: iSnd = 0x037;
		}
		break;
	case IT_GEM:
		iSnd = (( GetID() > ITEMID_GEMS ) ? 0x034 : 0x032 );  // Small vs Large Gems
		break;
	case IT_INGOT:  // Any Ingot
		if ( pObjOn == NULL )
		{
			iSnd = 0x033;
		}
		break;
	}

	CVarDefCont * pTagStorage = NULL; 
	pTagStorage = GetKey("OVERRIDE.DROPSOUND", true);
	if ( pTagStorage )
	{
		if ( pTagStorage->GetValNum() )
		{
			iSnd = pTagStorage->GetValNum();
		}
	}

	// normal drop sound for what dropped in/on.
	if ( iSnd == NULL )
		return( pObjOn ? 0x057 : 0x042 );
	else
		return ( iSnd );
}

bool CItem::MoveTo( CPointMap pt ) // Put item on the ground here.
{
	ADDTOCALLSTACK("CItem::MoveTo");
	// Move this item to it's point in the world. (ground/top level)
	// NOTE: Do NOT do the decay timer here.

	if ( ! pt.IsValidPoint())
		return false;

	CSector * pSector = pt.GetSector();
	ASSERT( pSector );
	pSector->MoveItemToSector( this, IsTimerSet());

	// Is this area too complex ?
	if ( ! g_Serv.IsLoading())
	{
		int iCount = pSector->GetItemComplexity();
		if ( iCount > g_Cfg.m_iMaxSectorComplexity )
		{
			DEBUG_ERR(( "Warning: %d items at %s,too complex!\n", iCount, pt.WriteUsed()));
		}
	}

	SetTopPoint( pt );
	ASSERT( IsTopLevel());	// on the ground.

	Update();
	return( true );
}

bool CItem::MoveToCheck( const CPointMap & pt, CChar * pCharMover )
{
	ADDTOCALLSTACK("CItem::MoveToCheck");
	// Make noise and try to pile it and such.

	CPointMap	ptNewPlace;
	if ( !pt.IsValidPoint() )
	{
		if ( pCharMover )
			ptNewPlace = pCharMover->GetTopPoint();
		else
			ptNewPlace.ValidatePoint();
	}
	else ptNewPlace  = pt;

	int iItemCount = 0;

	// Look for pileable ? count how many items are here.
	CItem * pItem = NULL;
	CWorldSearch AreaItems(ptNewPlace);
	DWORD	dMyZ = ptNewPlace.m_z;
	while (true)
	{
		pItem = AreaItems.GetItem();
		if ( pItem == NULL )
			break;

		iItemCount ++;

		// can't stack from a distance
		if ( ptNewPlace.GetDistZ(pItem->GetTopPoint()) <= 1 )
		{
			// ITRIG_STACKON
			if ( Stack( pItem ))
			{
				dMyZ = 100;
				break;
			}
		}

		if ( pItem->GetTopPoint().m_z > dMyZ ) dMyZ = pItem->GetTopPoint().m_z + 1;
	}
	// one floor. needs some configuration on that
	if ( dMyZ - ptNewPlace.m_z < 12 ) ptNewPlace.m_z = dMyZ;

	// Set the decay timer for this if not in a house or such.
	int iDecayTime = GetDecayTime();
	if ( iDecayTime > 0 )
	{
		// In a place where it will decay ?
		const CRegionBase * pRegion = ptNewPlace.GetRegion(REGION_TYPE_MULTI|REGION_TYPE_AREA|REGION_TYPE_ROOM);
		if ( pRegion != NULL && pRegion->IsFlag( REGION_FLAG_NODECAY ))	// No decay here in my house/boat
			iDecayTime = -1;
	}

	// MoveToDecay(ptNewPlace, iDecayTime);

	if ( iItemCount > g_Cfg.m_iMaxItemComplexity )
	{
		Speak("Too many items here!");
		if ( iItemCount > g_Cfg.m_iMaxItemComplexity + g_Cfg.m_iMaxItemComplexity/2 )
		{
			Speak("The ground collapses!");
			Delete();
		}
		// attempt to reject the move.
		return false;
	}

	// iItemCount - ITRIG_DROPON_ITEM

	CObjBase * pOldCont = this->GetContainer();

	TRIGRET_TYPE ttResult = TRIGRET_RET_DEFAULT;

	CScriptTriggerArgs args;
	args.m_s1 = ptNewPlace.WriteUsed();
	args.m_s1_raw = args.m_s1;
	args.m_iN1 = iDecayTime;
	ttResult = OnTrigger(ITRIG_DROPON_GROUND, pCharMover, &args);

	iDecayTime = args.m_iN1;	// ARGN1 = Decay time for the dropped item (in ticks)

	if (( IsDeleted() ) || ( pOldCont != this->GetContainer()))
		return false;

	if (ttResult != TRIGRET_RET_TRUE)
	{
		// when a player (not npcs or gms) drops an item on water it is deleted (not static/unmovable items)
		if ( (IsAttr(ATTR_MOVE_NEVER|ATTR_STATIC) == false) &&
			((pCharMover != NULL) && (pCharMover->m_pPlayer != NULL && pCharMover->IsPriv(PRIV_GM) == false)) )
		{
			const CUOMapMeter * pMeter = g_World.GetMapMeter(ptNewPlace);
			if ( pMeter && (pMeter->m_z == ptNewPlace.m_z) )
			{
				if ( IT_WATER == g_World.GetTerrainItemType( pMeter->m_wTerrainIndex ) )
				{
					Speak("*Blub*");
					Delete();
					return( false );
				}
				else if ( pMeter->m_wTerrainIndex == 0x0244 )
				{
					Delete();
					return( false );
				}
			}
		}
	}

	MoveToDecay(ptNewPlace, iDecayTime);
	Sound(GetDropSound(NULL));

	return true;
}

bool CItem::MoveNearObj( const CObjBaseTemplate * pObj, int iSteps, WORD wCan )
{
	ADDTOCALLSTACK("CItem::MoveNearObj");
	// Put in the same container as another item.
	ASSERT(pObj);
	CItemContainer * pPack = (dynamic_cast <CItemContainer*> (pObj->GetParent()));
	if ( pPack != NULL )
	{
		// Put in same container (make sure they don't get re-combined)
		pPack->ContentAdd( this, pObj->GetContainedPoint());
		return( true );
	}
	else 
	{
		// Equipped or on the ground so put on ground nearby.
		return CObjBase::MoveNearObj( pObj, iSteps, wCan );
	}
}

LPCTSTR CItem::GetName() const
{
	ADDTOCALLSTACK("CItem::GetName");
	// allow some items to go unnamed (just use type name).

	CItemBase * pItemDef = Item_GetDef();
	ASSERT(pItemDef);

	LPCTSTR pszNameBase;
	if ( IsIndividualName())
	{
		pszNameBase = CObjBase::GetName();
	}
	else if ( pItemDef->HasTypeName())
	{

		pszNameBase	= NULL;
		if ( IsSetOF( OF_Items_AutoName ) )
		{
			if ( IsType( IT_SCROLL ) || IsType( IT_POTION ) )
			{
				if ( RES_GET_INDEX(m_itPotion.m_Type) != SPELL_Explosion )
				{
					const CSpellDef * pSpell	= g_Cfg.GetSpellDef( (SPELL_TYPE) m_itSpell.m_spell );
					if ( pSpell )
						pszNameBase	= pSpell->GetName();
				}
			}
		}
		if ( !pszNameBase )			// Just use it's type name instead.
			pszNameBase = pItemDef->GetTypeName();
	}
	else
	{
		// Get the name of the link.
		const CObjBase * pObj = m_uidLink.ObjFind();
		if ( pObj && pObj != this )
		{
			return( pObj->GetName());
		}
		return "unnamed";
	}

	// Watch for names that should be pluralized.
	// Get rid of the % in the names.
	return( CItemBase::GetNamePluralize( pszNameBase, m_amount != 1 && ! IsType(IT_CORPSE) ));
}

LPCTSTR CItem::GetNameFull( bool fIdentified ) const
{
	ADDTOCALLSTACK("CItem::GetNameFull");
	// Should be LPCTSTR
	// Get a full descriptive name. Prefixing and postfixing.

	int len = 0;
	TCHAR * pTemp = Str_GetTemp();

	LPCTSTR pszTitle = NULL;
	LPCTSTR pszName = GetName();

	CItemBase * pItemDef = Item_GetDef();
	ASSERT(pItemDef);

	bool fSingular = (GetAmount()==1 || IsType(IT_CORPSE));
	if (fSingular) // m_corpse_DispID is m_amount
	{
		if ( ! IsIndividualName())
		{
			len += strcpylen( pTemp+len, pItemDef->GetArticleAndSpace());
		}
	}
	else
	{
		pszTitle = "%i ";
		len += sprintf( pTemp+len, pszTitle, GetAmount());
	}

	if ( fIdentified && IsAttr(ATTR_CURSED|ATTR_CURSED2|ATTR_BLESSED|ATTR_BLESSED2|ATTR_MAGIC))
	{
		bool fTitleSet = false;
		switch ( m_Attr & ( ATTR_CURSED|ATTR_CURSED2|ATTR_BLESSED|ATTR_BLESSED2))
		{
			case (ATTR_CURSED|ATTR_CURSED2):
				pszTitle = g_Cfg.GetDefaultMsg( DEFMSG_ITEMTITLE_UNHOLY );
				fTitleSet = true;
				break;
			case ATTR_CURSED2:
				pszTitle = g_Cfg.GetDefaultMsg( DEFMSG_ITEMTITLE_DAMNED );
				fTitleSet = true;
				break;
			case ATTR_CURSED:
				pszTitle = g_Cfg.GetDefaultMsg( DEFMSG_ITEMTITLE_CURSED );
				fTitleSet = true;
				break;
			case (ATTR_BLESSED|ATTR_BLESSED2):
				pszTitle = g_Cfg.GetDefaultMsg( DEFMSG_ITEMTITLE_HOLY );
				fTitleSet = true;
				break;
			case ATTR_BLESSED2:
				pszTitle = g_Cfg.GetDefaultMsg( DEFMSG_ITEMTITLE_SACRED );
				fTitleSet = true;
				break;
			case ATTR_BLESSED:
				pszTitle = g_Cfg.GetDefaultMsg( DEFMSG_ITEMTITLE_BLESSED );
				fTitleSet = true;
				break;
		}
		if ( fTitleSet )
		{
			if ( fSingular && !IsSetOF(OF_NoPrefix) ) 
			{
				len = strcpylen( pTemp, Str_GetArticleAndSpace(pszTitle));
			}
			len += strcpylen( pTemp+len, pszTitle );
		}

		if ( IsAttr(ATTR_MAGIC))
		{
			if ( m_itWeapon.m_spelllevel && IsTypeArmorWeapon() && ! IsType(IT_WAND))
			{
				// A weapon, (Not a wand)
				if ( ! pszTitle )
				{
					if ( !IsSetOF(OF_NoPrefix) )
						pszTitle = "a ";
					else
						pszTitle = "";

					len = strcpylen( pTemp, pszTitle );
				}
				len += sprintf( pTemp+len, "%c%d ", ( m_itWeapon.m_spelllevel<0 ) ? '-':'+', abs( g_Cfg.GetSpellEffect( SPELL_Enchant, m_itWeapon.m_spelllevel )));
			}
			else
			{
				// Don't put "magic" in front of "magic key"
				if ( strnicmp( pszName, "MAGIC", 5 ))
				{
					if ( ! pszTitle )
					{
						if ( !IsSetOF(OF_NoPrefix) )
						{
							pszTitle = "a ";
						} else
						{
							pszTitle = "";
						}
						len = strcpylen( pTemp, pszTitle );
					}
					len += strcpylen( pTemp+len, g_Cfg.GetDefaultMsg( DEFMSG_ITEMTITLE_MAGIC ) );
				}
			}
		}
	}

	// Prefix the name
	switch ( m_type )
	{
		case IT_STONE_GUILD:
			len += strcpylen( pTemp+len, g_Cfg.GetDefaultMsg( DEFMSG_ITEMTITLE_GUILDSTONE_FOR ) );
			break;
		case IT_STONE_TOWN:
			len += strcpylen( pTemp+len, g_Cfg.GetDefaultMsg( DEFMSG_ITEMTITLE_TOWN_OF ) );
			break;
		case IT_EQ_MEMORY_OBJ:
			len += strcpylen( pTemp+len, g_Cfg.GetDefaultMsg( DEFMSG_ITEMTITLE_MEMORY_OF ) );
			break;
		case IT_SPAWN_CHAR:
			if ( ! IsIndividualName())
				len += strcpylen( pTemp+len, g_Cfg.GetDefaultMsg( DEFMSG_ITEMTITLE_SPAWN ) );
			break;
		case IT_KEY:
			if ( ! m_itKey.m_lockUID.IsValidUID())
			{
				len += strcpylen( pTemp+len, g_Cfg.GetDefaultMsg( DEFMSG_ITEMTITLE_BLANK ) );
			}
			break;
		case IT_RUNE:
			if ( ! m_itRune.m_pntMark.IsCharValid())
			{
				len += strcpylen( pTemp+len, g_Cfg.GetDefaultMsg( DEFMSG_ITEMTITLE_BLANK ) );
			}
			else if ( ! m_itRune.m_Strength )
			{
				len += strcpylen( pTemp+len, g_Cfg.GetDefaultMsg( DEFMSG_ITEMTITLE_FADED ) );
			}
			break;
		case IT_TELEPAD:
			if ( ! m_itTelepad.m_pntMark.IsValidPoint())
			{
				len += strcpylen( pTemp+len, g_Cfg.GetDefaultMsg( DEFMSG_ITEMTITLE_BLANK ) );
			}
			break;
	}

	len += strcpylen( pTemp+len, pszName );

	// Suffix the name.

	if ( fIdentified && IsAttr(ATTR_MAGIC) && IsTypeArmorWeapon())	// wand is also a weapon.
	{
		SPELL_TYPE spell = (SPELL_TYPE) RES_GET_INDEX( m_itWeapon.m_spell );
		if ( spell )
		{
			const CSpellDef * pSpellDef = g_Cfg.GetSpellDef( spell );
			if ( pSpellDef )
			{
				len += sprintf( pTemp+len, " of %s", pSpellDef->GetName());
				if (m_itWeapon.m_spellcharges)
				{
					len += sprintf( pTemp+len, " (%d %s)", m_itWeapon.m_spellcharges, g_Cfg.GetDefaultMsg( DEFMSG_ITEMTITLE_CHARGES ) );
				}
			}
		}
	}

	switch ( m_type )
	{
		case IT_LOOM:
			if ( m_itLoom.m_ClothQty )
			{
				ITEMID_TYPE AmmoID = (ITEMID_TYPE) RES_GET_INDEX( m_itLoom.m_ClothID );
				const CItemBase * pAmmoDef = CItemBase::FindItemBase(AmmoID);
				if ( pAmmoDef )
				{
					len += sprintf( pTemp+len, " (%d %ss)", m_itLoom.m_ClothQty, pAmmoDef->GetName());
				}
			}
			break;

		case IT_ARCHERY_BUTTE:
			if ( m_itArcheryButte.m_AmmoCount )
			{
				ITEMID_TYPE AmmoID = (ITEMID_TYPE) RES_GET_INDEX( m_itArcheryButte.m_AmmoType );
				const CItemBase * pAmmoDef = CItemBase::FindItemBase(AmmoID);
				if ( pAmmoDef )
				{
					len += sprintf( pTemp+len, " %d %ss", m_itArcheryButte.m_AmmoCount, pAmmoDef->GetName());
				}
			}
			break;

		case IT_STONE_TOWN:
			{
				const CItemStone * pStone = dynamic_cast <const CItemStone*>(this);
				ASSERT(pStone);
				len += sprintf( pTemp+len, " (pop:%d)", pStone->GetCount());
			}
			break;

		case IT_MEAT_RAW:
		case IT_LEATHER:
		case IT_HIDE:
		case IT_FEATHER:
		case IT_FUR:
		case IT_WOOL:
		case IT_BLOOD:
		case IT_BONE:
			if ( fIdentified )
			{
				CREID_TYPE id = (CREID_TYPE) RES_GET_INDEX( m_itSkin.m_creid );
				if ( id)
				{
					CCharBase * pCharDef = CCharBase::FindCharBase( id );
					if ( pCharDef)
					{
						len += sprintf( pTemp+len, " (%s)", pCharDef->GetTradeName());
					}
				}
			}
			break;

		case IT_LIGHT_LIT:
		case IT_LIGHT_OUT:
			// how many charges ?
			if ( m_itLight.m_charges != USHRT_MAX )
			{
				CItem * pLightItem = const_cast<CItem *>(this);
				if ( pLightItem && pLightItem->Light_GetOverride(pItemDef) )
				{
					len += sprintf( pTemp+len, " (%d %s)", m_itLight.m_charges, g_Cfg.GetDefaultMsg( DEFMSG_ITEMTITLE_CHARGES ) );
				}
			}
			break;
	}

	
	if ( IsAttr(ATTR_STOLEN))
	{
		// Who is it stolen from ?
		CChar * pChar = m_uidLink.CharFind();
		if ( pChar )
		{
			len += sprintf( pTemp+len, " (%s %s)", g_Cfg.GetDefaultMsg( DEFMSG_ITEMTITLE_STOLEN_FROM ), pChar->GetName());
		}
		else
		{
			len += sprintf( pTemp+len, " (%s)", g_Cfg.GetDefaultMsg( DEFMSG_ITEMTITLE_STOLEN ) );
		}
	}

	return( pTemp );
}

bool CItem::SetName( LPCTSTR pszName )
{
	ADDTOCALLSTACK("CItem::SetName");
	// Can't be a dupe name with type name ?
	ASSERT(pszName);
	CItemBase * pItemDef = Item_GetDef();
	ASSERT(pItemDef);

	LPCTSTR pszTypeName = pItemDef->GetTypeName();
	if ( ! strnicmp( pszName, "a ", 2 ))
	{
		if ( ! strcmpi( pszTypeName, pszName+2 ))
			pszName = "";
	}
	else if ( ! strnicmp( pszName, "an ", 3 ))
	{
		if ( ! strcmpi( pszTypeName, pszName+3 ))
			pszName = "";
	}
	return SetNamePool( pszName );
}

bool CItem::SetBase( CItemBase * pItemDef )
{
	ADDTOCALLSTACK("CItem::SetBase");
	// Total change of type. (possibly dangerous)

	CItemBase * pItemOldDef = Item_GetDef();

	if ( pItemDef == NULL || pItemDef == pItemOldDef )
		return false;

	// This could be a weight change for my parent !!!
	CContainer * pParentCont = dynamic_cast <CContainer*> (GetParent());
	int iWeightOld = 0;

	if ( pItemOldDef)
	{
		if ( pParentCont )
		{
			iWeightOld = GetWeight();
		}
	}

	m_BaseRef.SetRef(pItemDef);

	if (pParentCont)
	{
		ASSERT( IsItemEquipped() || IsItemInContainer());
		pParentCont->OnWeightChange( GetWeight() - iWeightOld );
	}

	m_type = pItemDef->GetType();	// might change the type.
	return( true );
}

bool CItem::SetBaseID( ITEMID_TYPE id )
{
	ADDTOCALLSTACK("CItem::SetBaseID");
	// Converting the type of an existing item is possibly risky.
	// Creating invalid objects of any sort can crash clients.
	// User created items can be overwritten if we do this twice !
	CItemBase * pItemDef = CItemBase::FindItemBase( id );
	if ( pItemDef == NULL )
	{
		DEBUG_ERR(( "SetBaseID 0%x invalid item uid=0%x\n",
		id, (DWORD) GetUID()));
		return false;
	}
	SetBase( pItemDef );	// set new m_type etc
	return( true );
}

bool CItem::SetID( ITEMID_TYPE id )
{
	ADDTOCALLSTACK("CItem::SetID");
	if ( ! IsSameDispID( id ))
	{
		if ( ! SetBaseID( id ))
			return( false );
	}
	SetDispID( id );
	return( true );
}

bool CItem::SetDispID( ITEMID_TYPE id )
{
	ADDTOCALLSTACK("CItem::SetDispID");
	// Just change what this item looks like.
	// do not change it's basic type.

	if ( id == GetDispID())
		return true;	// no need to do this again or overwrite user created item types.

	if ( CItemBase::IsValidDispID(id) && id < ITEMID_MULTI )
	{
		m_wDispIndex = id;
	}
	else
	{
		CItemBase * pItemDef = Item_GetDef();
		ASSERT(pItemDef);
		m_wDispIndex = pItemDef->GetDispID();
		ASSERT( CItemBase::IsValidDispID((ITEMID_TYPE)m_wDispIndex));
	}
	return( true );
}

void CItem::SetAmount( int amount )
{
	ADDTOCALLSTACK("CItem::SetAmount");
	// propagate the weight change.
	// Setting to 0 might be legal if we are deleteing it ?

	int oldamount = GetAmount();
	if ( oldamount == amount )
		return;

	m_amount = amount;

	// sometimes the diff graphics for the types are not in the client.
	if ( IsType(IT_ORE))
	{
		static const ITEMID_TYPE sm_Item_Ore[] =
		{
			ITEMID_ORE_1, ITEMID_ORE_1, ITEMID_ORE_2, ITEMID_ORE_3
		};
		SetDispID( ( GetAmount() >= COUNTOF(sm_Item_Ore)) ? ITEMID_ORE_4 : sm_Item_Ore[GetAmount()] );
	}

	CContainer * pParentCont = dynamic_cast <CContainer*> (GetParent());
	if (pParentCont)
	{
		ASSERT( IsItemEquipped() || IsItemInContainer());
		CItemBase * pItemDef = Item_GetDef();
		ASSERT(pItemDef);
		pParentCont->OnWeightChange(( amount - oldamount ) * pItemDef->GetWeight());
	}
}

void CItem::SetAmountUpdate( int amount )
{
	ADDTOCALLSTACK("CItem::SetAmountUpdate");
	int oldamount = GetAmount();
	SetAmount( amount );
	if ( oldamount < 5 || amount < 5 )	// beyond this make no visual diff.
	{
		// Strange client thing. You have to remove before it will update this.
		RemoveFromView();
	}
	Update();
}

inline bool CItem::CallPersonalTrigger(TCHAR * pArgs, CTextConsole * pSrc, TRIGRET_TYPE & trResult)
{
	TCHAR * ppCmdTrigger[3];
	int iResultArgs = Str_ParseCmds(pArgs, ppCmdTrigger, COUNTOF(ppCmdTrigger), ",");
	
	if ( iResultArgs > 0 )
	{
		LPCTSTR callTrigger = ppCmdTrigger[0];
		int iTriggerArgType = 0;
		CScriptTriggerArgs csTriggerArgs;

		if ( iResultArgs == 3 )
		{
			iTriggerArgType = ATOI(ppCmdTrigger[1]);

			if ( iTriggerArgType == 1 ) // 3 ARGNs
			{
				int Arg_piCmd[3];
				iResultArgs = Str_ParseCmds(ppCmdTrigger[2], Arg_piCmd, COUNTOF(Arg_piCmd), ",");

				if ( iResultArgs == 3 )
				{
					csTriggerArgs.m_iN3 = Arg_piCmd[2];
				}

				if ( iResultArgs >= 2 )
				{
					csTriggerArgs.m_iN2 = Arg_piCmd[1];
				}

				if ( iResultArgs >= 1 )
				{
					csTriggerArgs.m_iN1 = Arg_piCmd[0];
				}
			}
			else if ( iTriggerArgType == 2 ) // ARGS
			{
				csTriggerArgs.m_s1 = ppCmdTrigger[2];
				csTriggerArgs.m_s1_raw = ppCmdTrigger[2];
			}
			else if ( iTriggerArgType == 3 ) // ARGO
			{
				CGrayUID guTriggerArg(Exp_GetVal(ppCmdTrigger[2]));
				CObjBase * pTriggerArgObj = guTriggerArg.ObjFind();
				if ( pTriggerArgObj )
				{
					csTriggerArgs.m_pO1 = pTriggerArgObj;
				}
			}
		}

		trResult = OnTrigger(callTrigger, pSrc, &csTriggerArgs);
		return true;
	}

	return false;
}

void CItem::WriteUOX( CScript & s, int index )
{
	ADDTOCALLSTACK("CItem::WriteUOX");
	s.Printf( "SECTION WORLDITEM %d\n", index );
	s.Printf( "{\n" );
	s.Printf( "SERIAL %d\n", (DWORD) GetUID());
	s.Printf( "NAME %s\n", GetName());
	s.Printf( "ID %d\n", GetDispID());
	s.Printf( "X %d\n", GetTopPoint().m_x );
	s.Printf( "Y %d\n", GetTopPoint().m_y );
	s.Printf( "Z %d\n", GetTopZ());
	s.Printf( "CONT %d\n", -1 );
	s.Printf( "TYPE %d\n", m_type );
	s.Printf( "AMOUNT %d\n", m_amount );
	s.Printf( "COLOR %d\n", GetHue());
	//ATT 5
	//VALUE 1
	s.Printf( "}\n\n" );
}

void CItem::r_WriteMore1( CGString & sVal )
{
	ADDTOCALLSTACK("CItem::r_WriteMore1");
	// do special processing to represent this.
	switch ( GetType())
	{
	case IT_SPAWN_CHAR:
		sVal = g_Cfg.ResourceGetName( m_itSpawnChar.m_CharID );
		return;
	case IT_SPAWN_ITEM:
		sVal = g_Cfg.ResourceGetName( m_itSpawnItem.m_ItemID );
		return;
	case IT_TREE:
	case IT_GRASS:
	case IT_ROCK:
	case IT_WATER:
		sVal = g_Cfg.ResourceGetName( m_itResource.m_rid_res );
		return;

	case IT_FRUIT:
	case IT_FOOD:
	case IT_FOOD_RAW:
	case IT_MEAT_RAW:
		sVal = g_Cfg.ResourceGetName( RESOURCE_ID( RES_ITEMDEF, m_itFood.m_cook_id ));
		return;

	case IT_TRAP:
	case IT_TRAP_ACTIVE:
	case IT_TRAP_INACTIVE:
	case IT_ANIM_ACTIVE:
	case IT_SWITCH:
	case IT_DEED:
	case IT_LOOM:
	case IT_ARCHERY_BUTTE:
	case IT_ITEM_STONE:
		sVal = g_Cfg.ResourceGetName( RESOURCE_ID( RES_ITEMDEF, m_itNormal.m_more1 ));
		return;

	case IT_FIGURINE:
	case IT_EQ_HORSE:
		sVal = g_Cfg.ResourceGetName(RESOURCE_ID(RES_CHARDEF, m_itNormal.m_more1));
		return;

	case IT_POTION:
		sVal = g_Cfg.ResourceGetName( RESOURCE_ID( RES_SPELL, m_itPotion.m_Type ));
		return;
	}

	sVal.FormatHex( m_itNormal.m_more1 );
}

void CItem::r_WriteMore2( CGString & sVal )
{
	ADDTOCALLSTACK("CItem::r_WriteMore2");
	// do special processing to represent this.
	switch ( GetType())
	{
	case IT_FRUIT:
	case IT_FOOD:
	case IT_FOOD_RAW:
	case IT_MEAT_RAW:
		sVal = g_Cfg.ResourceGetName( RESOURCE_ID( RES_CHARDEF, m_itFood.m_MeatType ));
		return;

	case IT_CROPS:
	case IT_FOLIAGE:
		sVal = g_Cfg.ResourceGetName( RESOURCE_ID( RES_ITEMDEF, m_itCrop.m_ReapFruitID ));
		return;

	case IT_LEATHER:
	case IT_HIDE:
	case IT_FEATHER:
	case IT_FUR:
	case IT_WOOL:
	case IT_BLOOD:
	case IT_BONE:
		sVal = g_Cfg.ResourceGetName( RESOURCE_ID( RES_CHARDEF, m_itNormal.m_more2 ));
		return;

	case IT_ANIM_ACTIVE:
		sVal = g_Cfg.ResourceGetName( RESOURCE_ID( RES_TYPEDEF, m_itAnim.m_PrevType ));
		return;
	}
	sVal.FormatHex( m_itNormal.m_more2 );
}

void CItem::r_Write( CScript & s )
{
	ADDTOCALLSTACK("CItem::r_Write");
	CItemBase	*pItemDef = Item_GetDef();
	if ( !pItemDef )
		return;

	s.WriteSection((IsSetEF(EF_Size_Optimise) ? "WI %s" : "WORLDITEM %s"), GetResourceName());

	CObjBase::r_Write(s);

	if ( GetDispID() != GetID())	// the item is flipped.
		s.WriteKey("DISPID", g_Cfg.ResourceGetName(RESOURCE_ID(RES_ITEMDEF, GetDispID())));
	if ( GetAmount() != 1 )
		s.WriteKeyVal("AMOUNT", GetAmount());
	if ( ! pItemDef->IsType(m_type))
		s.WriteKey("TYPE", g_Cfg.ResourceGetName(RESOURCE_ID(RES_TYPEDEF, m_type)));
	if ( m_uidLink.IsValidUID())
		s.WriteKeyHex("LINK", m_uidLink);
	if ( m_Attr )
		s.WriteKeyHex("ATTR", m_Attr);

	if ( m_itNormal.m_more1 )
	{
		CGString sVal;
		r_WriteMore1(sVal);
		s.WriteKey( "MORE1", sVal );
	}
	if ( m_itNormal.m_more2 )
	{
		CGString sVal;
		r_WriteMore2(sVal);
		s.WriteKey( "MORE2", sVal );
	}
	if ( m_itNormal.m_morep.m_x || m_itNormal.m_morep.m_y || m_itNormal.m_morep.m_z || m_itNormal.m_morep.m_map )
		s.WriteKey( "MOREP", m_itNormal.m_morep.WriteUsed());

	CObjBase * pCont = GetContainer();
	if ( pCont )
	{
		if ( pCont->IsChar() )
		{
			if ( GetEquipLayer() >= LAYER_HORSE )
				s.WriteKeyVal("LAYER", GetEquipLayer());
		}
		s.WriteKeyHex("CONT", pCont->GetUID());
		if ( pCont->IsItem() )
		{
			s.WriteKey("P", GetContainedPoint().WriteUsed());
			if ( GetContainedGridIndex() )
				s.WriteKeyVal("CONTGRID", GetContainedGridIndex());
		}
	}
	else
		s.WriteKey("P", GetTopPoint().WriteUsed());

	if ( IsTypeWeapon() )
	{
		if ( m_itWeapon.m_dmgcold )
			s.WriteKeyHex( "DMGCOLD", m_itWeapon.m_dmgcold );
		if ( m_itWeapon.m_dmgenergy )
			s.WriteKeyHex( "DMGENERGY", m_itWeapon.m_dmgenergy );
		if ( m_itWeapon.m_dmgfire )
			s.WriteKeyHex( "DMGFIRE", m_itWeapon.m_dmgfire );
		if ( m_itWeapon.m_dmgpoison )
			s.WriteKeyHex( "DMGPOISON", m_itWeapon.m_dmgpoison );
	}
}

bool CItem::LoadSetContainer( CGrayUID uid, LAYER_TYPE layer )
{
	ADDTOCALLSTACK("CItem::LoadSetContainer");
	// Set the CItem in a container of some sort.
	// Used mostly during load.
	// "CONT" IC_CONT
	// NOTE: We don't have a valid point in the container yet probably. get that later.

	CObjBase * pObjCont = uid.ObjFind();
	if ( pObjCont == NULL )
	{
		DEBUG_ERR(( "Invalid container 0%lx\n", (DWORD) uid ));
		return( false );	// not valid object.
	}

	if ( pObjCont->IsItem())
	{
		// layer is not used here of course.

		CItemContainer * pCont = dynamic_cast <CItemContainer *> (pObjCont);
		if ( pCont != NULL )
		{
			pCont->ContentAdd( this );
			return( true );
		}
	}
	else
	{
		CChar * pChar = dynamic_cast <CChar *> (pObjCont);
		if ( pChar != NULL )
		{
			// equip the item
			CItemBase * pItemDef = Item_GetDef();
			ASSERT(pItemDef);
			if ( ! layer ) layer = pItemDef->GetEquipLayer();

			pChar->LayerAdd( this, layer );
			return( true );
		}
	}

	DEBUG_ERR(( "Non container uid=0%lx,id=0%x\n", (DWORD) uid, pObjCont->GetBaseID() ));
	return( false );	// not a container.
}

enum ICR_TYPE
{
	ICR_CONT,
	ICR_LINK,
	ICR_REGION,
	ICR_QTY,
};

LPCTSTR const CItem::sm_szRefKeys[ICR_QTY+1] =
{
	"CONT",
	"LINK",
	"REGION",
	NULL,
};

bool CItem::r_GetRef( LPCTSTR & pszKey, CScriptObj * & pRef )
{
	ADDTOCALLSTACK("CItem::r_GetRef");
	int i = FindTableHeadSorted( pszKey, sm_szRefKeys, COUNTOF(sm_szRefKeys)-1 );
	if ( i >= 0 )
	{
		pszKey += strlen( sm_szRefKeys[i] );
		SKIP_SEPARATORS(pszKey);
		switch (i)
		{
			case ICR_CONT:
				if ( pszKey[-1] != '.' )
					break;
				pRef = GetContainer();
				return( true );
			case ICR_LINK:
				if ( pszKey[-1] != '.' )
					break;
				pRef = m_uidLink.ObjFind();
				return( true );
			case ICR_REGION:
				pRef = GetTopLevelObj()->GetTopPoint().GetRegion(REGION_TYPE_MULTI |REGION_TYPE_AREA);
				return( true );
		}
	}

	return( CObjBase::r_GetRef( pszKey, pRef ));
}

enum IC_TYPE
{
	#define ADD(a,b) IC_##a,
	#include "../tables/CItem_props.tbl"
	#undef ADD
	IC_QTY,
};

LPCTSTR const CItem::sm_szLoadKeys[IC_QTY+1] =
{
	#define ADD(a,b) b,
	#include "../tables/CItem_props.tbl"
	#undef ADD
	NULL,
};


bool CItem::r_WriteVal( LPCTSTR pszKey, CGString & sVal, CTextConsole * pSrc )
{
	ADDTOCALLSTACK("CItem::r_WriteVal");
	EXC_TRY("WriteVal");
	int index;
	if ( !strnicmp( CItem::sm_szLoadKeys[IC_ADDSPELL], pszKey, 8 ) )
		index	= IC_ADDSPELL;
	else if ( !strnicmp( CItem::sm_szLoadKeys[IC_TRIGGER], pszKey, 7 ) )
		index	= IC_TRIGGER;
	else
		index	= FindTableSorted( pszKey, sm_szLoadKeys, COUNTOF( sm_szLoadKeys )-1 );

	bool fDoDefault = false;

	switch ( index )
	{
		case IC_ADDSPELL:
			pszKey	+= 8;
			SKIP_SEPARATORS( pszKey );
			sVal.FormatVal( IsSpellInBook( (SPELL_TYPE) g_Cfg.ResourceGetIndexType( RES_SPELL, pszKey ) ));
			break;
		case IC_AMOUNT:
			sVal.FormatVal( GetAmount());
			break;
		case IC_ATTR:
			sVal.FormatHex( m_Attr );
			break;
		case IC_CONT:
			{
				if ( pszKey[4] == '.' )
				{
					return( CScriptObj::r_WriteVal( pszKey, sVal, pSrc ));
				}

				CObjBase * pCont = GetContainer();
				sVal.FormatHex( pCont ? ((DWORD) pCont->GetUID() ) : 0 );
			}
			break;
		case IC_CONTGRID:
			if ( !IsItemInContainer() )
				return false;
			sVal.FormatVal(GetContainedGridIndex());
			break;
		case IC_CONTP:
			{
				CObjBase * pContainer = GetContainer();
				if (( IsItem() ) && ( IsItemInContainer() ) && ( pContainer->IsValidUID() ) && ( pContainer->IsContainer() ) && ( pContainer->IsItem() ))
					sVal = GetContainedPoint().WriteUsed();
				else
					return false;
				break;
			}
		case IC_DISPID:
			sVal = g_Cfg.ResourceGetName( RESOURCE_ID( RES_ITEMDEF, GetDispID()));
			break;
		case IC_DISPIDDEC:
			{
				int iVal = GetDispID();
	 			if ( IsType(IT_COIN)) // Fix money piles
				{
					CItemBase * pItemDef = Item_GetDef();
					ASSERT(pItemDef);

					iVal = pItemDef->GetDispID();
					if ( GetAmount() < 2 )
						iVal = iVal;
					else if ( GetAmount() < 6)
						iVal = iVal + 1;
					else
						iVal = iVal + 2;
				}
				sVal.FormatVal( iVal );
			}
			break;
		case IC_DMGCOLD:
			sVal.FormatHex( IsTypeWeapon() ? m_itWeapon.m_dmgcold : 0 );
			break;
		case IC_DMGENERGY:
			sVal.FormatHex( IsTypeWeapon() ? m_itWeapon.m_dmgenergy : 0 );
			break;
		case IC_DMGFIRE:
			sVal.FormatHex( IsTypeWeapon() ? m_itWeapon.m_dmgfire : 0 );
			break;
		case IC_DMGPOISON:
			sVal.FormatHex( IsTypeWeapon() ? m_itWeapon.m_dmgpoison : 0 );
			break;
		case IC_HEIGHT:
			{
				CItemBase * pItemDef = CItemBase::FindItemBase((ITEMID_TYPE) GetDispID() );
				CItemBaseDupe * pDupeDef = CItemBaseDupe::GetDupeRef((ITEMID_TYPE) GetDispID() );
				sVal.FormatVal( pDupeDef ? pDupeDef->GetHeight() : pItemDef->GetHeight() );
			}
			break;
		case IC_HITS:
			sVal.FormatVal(LOWORD(m_itNormal.m_more1));
			break;
		case IC_HITPOINTS:
			sVal.FormatVal( IsTypeArmorWeapon() ? m_itArmor.m_Hits_Cur : 0 );
			break;
		case IC_ID:
			fDoDefault = true;
			break;
		case IC_LAYER:
			if ( IsItemEquipped())
			{
				sVal.FormatVal( GetEquipLayer() );
				break;
			}
			fDoDefault = true;
			break;
		case IC_LINK:
			if ( pszKey[4] == '.' )
			{
				return( CScriptObj::r_WriteVal( pszKey, sVal, pSrc ));
			}
			sVal.FormatHex( m_uidLink );
			break;
		case IC_MAXHITS:
			sVal.FormatVal(HIWORD(m_itNormal.m_more1));
			break;
		case IC_MORE:
			sVal.FormatVal( m_itNormal.m_more1 );
			break;
		case IC_MORE1:
			r_WriteMore1(sVal);
			break;
		case IC_MORE1h:
			sVal.FormatVal( HIWORD( m_itNormal.m_more1 ));
			break;
		case IC_MORE1l:
			sVal.FormatVal( LOWORD( m_itNormal.m_more1 ));
			break;
		case IC_MORE2:
		case IC_FRUIT:
			r_WriteMore2(sVal);
			break;
		case IC_MORE2h:
			sVal.FormatVal( HIWORD( m_itNormal.m_more2 ));
			break;
		case IC_MORE2l:
			sVal.FormatVal( LOWORD( m_itNormal.m_more2 ));
			break;
		case IC_MOREM:
			sVal.FormatVal( m_itNormal.m_morep.m_map );
			break;
		case IC_MOREP:
			sVal = m_itNormal.m_morep.WriteUsed();
			break;
		case IC_MOREX:
			sVal.FormatVal( m_itNormal.m_morep.m_x );
			break;
		case IC_MOREY:
			sVal.FormatVal( m_itNormal.m_morep.m_y );
			break;
		case IC_MOREZ:
			if ( IsTypeSpellbook() )
				sVal.FormatVal(m_itSpellbook.m_baseid);
			else
				sVal.FormatVal(m_itNormal.m_morep.m_z);
			break;
		case IC_P:
			fDoDefault = true;
			break;
		case IC_TRIGGER:
			{
				pszKey += 7;
				GETNONWHITESPACE( pszKey );

				if ( *pszKey )
				{
					TRIGRET_TYPE trReturn;
					bool bTrigReturn = CallPersonalTrigger((TCHAR*)pszKey, pSrc, trReturn);
					if ( bTrigReturn )
						sVal.FormatVal(trReturn);

					return bTrigReturn;
				}
			} return false;
		case IC_TYPE:
			sVal = g_Cfg.ResourceGetName( RESOURCE_ID( RES_TYPEDEF, m_type ));
			break;
		default:
			fDoDefault = true;
	}
	if ( fDoDefault )
		return( CObjBase::r_WriteVal( pszKey, sVal, pSrc ));
	return true;
	EXC_CATCH;

	EXC_DEBUG_START;
	EXC_ADD_KEYRET(pSrc);
	EXC_DEBUG_END;
	return false;
}

bool CItem::r_LoadVal( CScript & s ) // Load an item Script
{
	ADDTOCALLSTACK("CItem::r_LoadVal");
	EXC_TRY("LoadVal");
	switch ( FindTableSorted( s.GetKey(), sm_szLoadKeys, COUNTOF( sm_szLoadKeys )-1 ))
	{
		case IC_ADDCIRCLE:
			{
				TCHAR	*ppVal[2];
				int		amount = Str_ParseCmds(s.GetArgStr(), ppVal, COUNTOF(ppVal), " ,\t");
				int		includeLower = 0;
				int		addCircle = 0;

				if ( !amount ) return false;
				if ( amount > 1 ) includeLower = ATOI(ppVal[1]);

				for ( addCircle = ATOI(ppVal[0]); addCircle; addCircle-- )
				{
					for ( int i = 1; i < 9; i++ )
					{
						AddSpellbookSpell((SPELL_TYPE)RES_GET_INDEX(((addCircle-1) * 8) + i), false);
					}
					if ( !includeLower ) break;
				}
				return true;
			}
		case IC_ADDSPELL:
			// Add this spell to the i_spellbook.
			if ( AddSpellbookSpell( (SPELL_TYPE) RES_GET_INDEX( s.GetArgVal()), false ))
				return( false );
			return( true );
		case IC_AMOUNT:
			SetAmount( s.GetArgVal());
			return true;
		case IC_ATTR:
			m_Attr = s.GetArgVal();
			return true;
		case IC_CONT:	// needs special processing.
			{
				bool normcont = LoadSetContainer(s.GetArgVal(), (LAYER_TYPE) GetUnkZ());
				if ( !normcont && ( g_Serv.m_iModeCode == SERVMODE_Loading ))
				{
					//	since the item is no longer in container, it should be deleted
					Delete();
				}
				return normcont;
			}
		case IC_CONTGRID:
			if ( !IsItemInContainer() )
				return false;
			SetContainedGridIndex(s.GetArgVal());
			return true;
		case IC_CONTP:
			{
				CPointMap pt;	// invalid point
				TCHAR *pszTemp = Str_GetTemp();
				strcpy( pszTemp, s.GetArgStr() );
				GETNONWHITESPACE( pszTemp );
				int iArgs = 0;
				if ( IsDigit( pszTemp[0] ) || pszTemp[0] == '-' )
				{
					pt.m_map = 0; pt.m_z = 0;
					TCHAR * ppVal[2];
					iArgs = Str_ParseCmds( pszTemp, ppVal, COUNTOF( ppVal ), " ,\t" );
					if ( iArgs < 2 ) 
					{
						DEBUG_ERR(( "Bad CONTP usage (not enough parameters)\n" ));
						return false;
					}
					switch ( iArgs )
					{
						default:
						case 2:
							pt.m_y = ATOI(ppVal[1]);
						case 1:
							pt.m_x = ATOI(ppVal[0]);
						case 0:
							break;
					}
				}
				CObjBase * pContainer = GetContainer();
				if (( IsItem() ) && ( IsItemInContainer() ) && ( pContainer->IsValidUID() ) && ( pContainer->IsContainer() ) && ( pContainer->IsItem() ))
				{
					CItemContainer * pCont = dynamic_cast <CItemContainer *> ( pContainer );
					pCont->ContentAdd( this, pt );
				}
				else
				{
					DEBUG_ERR(( "Bad CONTP usage (item is not in a container)\n" ));
					return false;
				}
				break;
			}
		case IC_DISPID:
		case IC_DISPIDDEC:
			return SetDispID((ITEMID_TYPE) g_Cfg.ResourceGetIndexType( RES_ITEMDEF, s.GetArgStr()));
		case IC_DMGCOLD:
			if ( IsTypeWeapon() )
				m_itWeapon.m_dmgcold = s.GetArgVal();
			else
				DEBUG_ERR(("Dmgcold set on a non-weapon object\n"));
			return true;;
		case IC_DMGENERGY:
			if ( IsTypeWeapon() )
				m_itWeapon.m_dmgenergy = s.GetArgVal();
			else
				DEBUG_ERR(("Dmgenergy set on a non-weapon object\n"));
			return true;;
		case IC_DMGFIRE:
			if ( IsTypeWeapon() )
				m_itWeapon.m_dmgfire = s.GetArgVal();
			else
				DEBUG_ERR(("Dmgfire set on a non-weapon object\n"));
			return true;;
		case IC_DMGPOISON:
			if ( IsTypeWeapon() )
				m_itWeapon.m_dmgpoison = s.GetArgVal();
			else
				DEBUG_ERR(("Dmgpoison set on a non-weapon object\n"));
			return true;;
		case IC_HITS:
			{
				int maxHits = HIWORD(m_itNormal.m_more1);
				if( maxHits == 0 ) {
					maxHits = s.GetArgVal();
				}
				m_itNormal.m_more1 = MAKEDWORD(s.GetArgVal(), maxHits);
			}
			break;
		case IC_HITPOINTS:
			if ( !IsTypeArmorWeapon() )
				DEBUG_ERR(("Item:Hitpoints assigned for non-weapon %s\n", GetResourceName()));
			else
				m_itArmor.m_Hits_Cur = m_itArmor.m_Hits_Max = s.GetArgVal();
			return true;
		case IC_ID:
			return SetID((ITEMID_TYPE) g_Cfg.ResourceGetIndexType( RES_ITEMDEF, s.GetArgStr()));
		case IC_LAYER:
			// used only during load.
			if ( ! IsDisconnected() && ! IsItemInContainer() && ! IsItemEquipped())
			{
				return( false );
			}
			SetUnkZ( s.GetArgVal()); // GetEquipLayer()
			return true;
		case IC_LINK:
			m_uidLink = s.GetArgVal();
			return true;

		case IC_FRUIT:	// m_more2
			m_itCrop.m_ReapFruitID = (ITEMID_TYPE) g_Cfg.ResourceGetIndexType( RES_ITEMDEF, s.GetArgStr());
			return true;
		case IC_MAXHITS:
			m_itNormal.m_more1 = MAKEDWORD(LOWORD(m_itNormal.m_more1), s.GetArgVal());
			break;
		case IC_MORE:
		case IC_MORE1:
			m_itNormal.m_more1 = s.GetArgVal();
			if ( !g_Serv.IsLoading( )
				&& ( IsType( IT_SPAWN_CHAR ) || IsType( IT_SPAWN_ITEM ) ) )
			{
				Spawn_FixDef();
				Spawn_SetTrackID();
				RemoveFromView();
				Update();
			}
			return true;
		case IC_MORE1h:
			m_itNormal.m_more1 = MAKEDWORD( LOWORD(m_itNormal.m_more1), s.GetArgVal());
			break;
		case IC_MORE1l:
			m_itNormal.m_more1 = MAKEDWORD( s.GetArgVal(), HIWORD(m_itNormal.m_more1));
			break;
		case IC_MORE2:
			m_itNormal.m_more2 = s.GetArgVal();
			return true;
		case IC_MORE2h:
			m_itNormal.m_more2 = MAKEDWORD( LOWORD(m_itNormal.m_more2), s.GetArgVal());
			break;
		case IC_MORE2l:
			m_itNormal.m_more2 = MAKEDWORD( s.GetArgVal(), HIWORD(m_itNormal.m_more2));
			break;
		case IC_MOREM:
			m_itNormal.m_morep.m_map = s.GetArgVal();
			break;
		case IC_MOREP:
			{
				CPointMap pt;	// invalid point
				TCHAR *pszTemp = Str_GetTemp();
				strcpy( pszTemp, s.GetArgStr() );
				GETNONWHITESPACE( pszTemp );
				int iArgs = 0;
				if ( IsDigit( pszTemp[0] ) || pszTemp[0] == '-' )
				{
					pt.m_map = 0; pt.m_z = 0;
					TCHAR * ppVal[4];
					iArgs = Str_ParseCmds( pszTemp, ppVal, COUNTOF( ppVal ), " ,\t" );
					switch ( iArgs )
					{
						default:
						case 4:	// m_map
							if ( IsDigit(ppVal[3][0]))
							{
								pt.m_map = ATOI(ppVal[3]);
							}
						case 3: // m_z
							if ( IsDigit(ppVal[2][0]) || ppVal[2][0] == '-' )
							{
								pt.m_z = ATOI(ppVal[2]);
							}
						case 2:
							pt.m_y = ATOI(ppVal[1]);
						case 1:
							pt.m_x = ATOI(ppVal[0]);
						case 0:
							break;
					}
				}
				if ( iArgs < 2 ) 
				{
					pt.InitPoint();
				}
				
				m_itNormal.m_morep = pt;
				// m_itNormal.m_morep = g_Cfg.GetRegionPoint( s.GetArgStr() );
			}
			return true;
		case IC_MOREX:
			m_itNormal.m_morep.m_x = s.GetArgVal();
			return true;
		case IC_MOREY:
			m_itNormal.m_morep.m_y = s.GetArgVal();
			return true;
		case IC_MOREZ:
			if ( IsTypeSpellbook() )
				m_itSpellbook.m_baseid = s.GetArgVal();
			else
				m_itNormal.m_morep.m_z = s.GetArgVal();
			return true;
		case IC_P:
			// Loading or import ONLY ! others use the r_Verb
			if ( ! IsDisconnected() && ! IsItemInContainer())
				return false;
			else
			{
				// Will be placed in the world later.
				CPointMap pt;
				pt.Read( s.GetArgStr());
				SetUnkPoint(pt);
			}
			return true;
		case IC_TYPE:
			SetType( (IT_TYPE) g_Cfg.ResourceGetIndexType( RES_TYPEDEF, s.GetArgStr()));
			return true;
		default:
			return( CObjBase::r_LoadVal( s ));
	}
	return true;
	EXC_CATCH;

	EXC_DEBUG_START;
	EXC_ADD_SCRIPT;
	EXC_DEBUG_END;
	return false;
}

bool CItem::r_Load( CScript & s ) // Load an item from script
{
	ADDTOCALLSTACK("CItem::r_Load");
	CScriptObj::r_Load( s );
	if ( GetContainer() == NULL )	// Place into the world.
	{
		if ( GetTopPoint().IsCharValid())
		{
			MoveTo( GetTopPoint());
		}
	}

	int iResultCode = CObjBase::IsWeird();
	if ( iResultCode )
	{
		DEBUG_ERR(( "Item 0%x Invalid, id=%s, code=0%x\n",
		(DWORD) GetUID(), GetResourceName(), iResultCode ));
		Delete();
		return( true );
	}

	return( true );
}

enum CIV_TYPE
{
	#define ADD(a,b) CIV_##a,
	#include "../tables/CItem_functions.tbl"
	#undef ADD
	CIV_QTY,
};

LPCTSTR const CItem::sm_szVerbKeys[CIV_QTY+1] =
{
	#define ADD(a,b) b,
	#include "../tables/CItem_functions.tbl"
	#undef ADD
	NULL,
};

bool CItem::r_Verb( CScript & s, CTextConsole * pSrc ) // Execute command from script
{
	ADDTOCALLSTACK("CItem::r_Verb");
	EXC_TRY("Verb");
	ASSERT(pSrc);

	int index = FindTableSorted( s.GetKey(), sm_szVerbKeys, COUNTOF( sm_szVerbKeys )-1 );
	if ( index < 0 )
	{
		return( CObjBase::r_Verb( s, pSrc ));
	}

	CChar * pCharSrc = pSrc->GetChar();

	switch ( index )
	{
		case CIV_BOUNCE:
			if ( ! pCharSrc )
				return( false );
			pCharSrc->ItemBounce( this );
			break;
		case CIV_CONSUME:
			ConsumeAmount( s.HasArgs() ? s.GetArgVal() : 1 );
			break;
		case CIV_CONTCONSUME:
			{
				if ( IsContainer() )
				{
					CContainer * pCont = dynamic_cast<CContainer*>(this);
					CResourceQtyArray Resources;
					Resources.Load( s.GetArgStr() );
					pCont->ResourceConsume( &Resources, 1, false );
				}
			}
			break;
		case CIV_DECAY:
			SetDecayTime( s.GetArgVal());
			break;
		case CIV_DROP:
			{
				CObjBaseTemplate * pObjTop = GetTopLevelObj();
				MoveToCheck( pObjTop->GetTopPoint(), pSrc->GetChar());
			}
			return( true );
		case CIV_DUPE:
			{
				int iCount = s.GetArgVal();
				if ( iCount <= 0 ) 
					iCount = 1;
				if ( !GetContainer() && ( iCount > g_Cfg.m_iMaxItemComplexity ))	// if top-level, obey the complexity
					iCount = g_Cfg.m_iMaxItemComplexity;
				while ( iCount-- )
				{
					CItem::CreateDupeItem(this, dynamic_cast<CChar *>(pSrc), true)->MoveNearObj(this, 1);
				}
			}
			break;
		case CIV_EQUIP:
			if ( ! pCharSrc )
				return( false );
			return pCharSrc->ItemEquip( this );
		case CIV_TRIGGER:
			{
				if ( s.HasArgs() )
				{
					TCHAR * ppCmdTrigger[3];
					int iResultArgs = Str_ParseCmds(s.GetArgRaw(), ppCmdTrigger, COUNTOF(ppCmdTrigger), ",");
					if ( iResultArgs > 0 )
					{
						LPCTSTR callTrigger = ppCmdTrigger[0];
						int iTriggerArgType = 0;
						CScriptTriggerArgs csTriggerArgs;

						if ( iResultArgs == 3 )
						{
							iTriggerArgType = ATOI(ppCmdTrigger[1]);

							if ( iTriggerArgType == 1 ) // 3 ARGNs
							{
								int Arg_piCmd[3];
								iResultArgs = Str_ParseCmds(ppCmdTrigger[2], Arg_piCmd, COUNTOF(Arg_piCmd), ",");

								if ( iResultArgs == 3 )
								{
									csTriggerArgs.m_iN3 = Arg_piCmd[2];
								}

								if ( iResultArgs >= 2 )
								{
									csTriggerArgs.m_iN2 = Arg_piCmd[1];
								}

								if ( iResultArgs >= 1 )
								{
									csTriggerArgs.m_iN1 = Arg_piCmd[0];
								}
							}
							else if ( iTriggerArgType == 2 ) // ARGS
							{
								csTriggerArgs.m_s1 = ppCmdTrigger[2];
								csTriggerArgs.m_s1_raw = ppCmdTrigger[2];
							}
							else if ( iTriggerArgType == 3 ) // ARGO
							{
								CGrayUID guTriggerArg(Exp_GetVal(ppCmdTrigger[2]));
								CObjBase * pTriggerArgObj = guTriggerArg.ObjFind();
								if ( pTriggerArgObj )
								{
									csTriggerArgs.m_pO1 = pTriggerArgObj;
								}
							}
						}

						OnTrigger(callTrigger, pSrc, &csTriggerArgs);
					}
				}
			} break;
		case CIV_UNEQUIP:
			if ( ! pCharSrc )
				return( false );
			RemoveSelf();
			pCharSrc->ItemBounce(this);
			break;
		case CIV_USE:
			if ( ! pCharSrc )
				return( false );
			pCharSrc->Use_Obj( this, s.HasArgs() ? s.GetArgVal() : true, true );
			break;
		default:
			return false;
	}
	return true;
	EXC_CATCH;

	EXC_DEBUG_START;
	EXC_ADD_SCRIPTSRC;
	EXC_DEBUG_END;
	return false;
}

TRIGRET_TYPE CItem::OnTrigger( LPCTSTR pszTrigName, CTextConsole * pSrc, CScriptTriggerArgs * pArgs )
{
	ADDTOCALLSTACK("CItem::OnTrigger");
	if ( !pSrc )
		pSrc = &g_Serv;

	ITRIG_TYPE iAction;
	if ( ISINTRESOURCE(pszTrigName))
	{
		iAction = (ITRIG_TYPE) GETINTRESOURCE(pszTrigName);
		pszTrigName = sm_szTrigName[iAction];
	}
	else
	{
		iAction = (ITRIG_TYPE) FindTableSorted( pszTrigName, sm_szTrigName, COUNTOF(sm_szTrigName)-1 );
	}

	ASSERT( iAction < ITRIG_QTY );
	TRIGRET_TYPE		iRet = TRIGRET_RET_DEFAULT;

	// Is there trigger code in the script file ?
	// RETURN:
	//   false = continue default process normally.
	//   true = don't allow this.  (halt further processing)
	EXC_TRY("Trigger");

	CItemBase	*pItemDef = Item_GetDef();
	ASSERT(pItemDef);

	CResourceLink	*pResourceLink;
	int				i;

	// 1) Triggers installed on character, sensitive to actions on all items
	EXC_SET("chardef");
	CChar * pChar = pSrc->GetChar();
	if ( pChar )
	{
		// Is there an [EVENT] block call here?
		if ( iAction > XTRIG_UNKNOWN )
		{
			CGrayUID uidOldAct = pChar->m_Act_Targ;
			pChar->m_Act_Targ = GetUID();
			iRet = pChar->OnTrigger( (CTRIG_TYPE)( (CTRIG_itemAfterClick-1)+iAction ),  pSrc, pArgs );
			pChar->m_Act_Targ = uidOldAct;
			if ( iRet == TRIGRET_RET_TRUE )
				return iRet;	// Block further action.
		}
	}


	//	2) EVENTS
	EXC_SET("events");
	int origEvents = m_OEvents.GetCount();
	int curEvents = origEvents;
	for ( i=0; i < curEvents; ++i )			//	2) EVENTS (could be modifyed ingame!)
	{
		CResourceLink	*pLink = m_OEvents[i];
		if ( !pLink || !pLink->HasTrigger(iAction) )
			continue;
		CResourceLock s;
		if ( !pLink->ResourceLock(s) )
			continue;

		TRIGRET_TYPE iRet = CScriptObj::OnTriggerScript(s, pszTrigName, pSrc, pArgs);
		if ( iRet != TRIGRET_RET_FALSE && iRet != TRIGRET_RET_DEFAULT )
			return iRet;

		curEvents = m_OEvents.GetCount();
		if ( curEvents < origEvents ) // the event has been deleted, modify the counter for other trigs to work
		{
			--i;
			origEvents = curEvents;
		}
	}

	// 3) TEVENTS on the item
	EXC_SET("tevents");
	for ( i=0; i < pItemDef->m_TEvents.GetCount(); ++i )
	{
		CResourceLink * pLink = pItemDef->m_TEvents[i];
		ASSERT(pLink);
		if ( !pLink->HasTrigger(iAction) )
			continue;
		CResourceLock s;
		if ( !pLink->ResourceLock(s) )
			continue;
		TRIGRET_TYPE iRet = CScriptObj::OnTriggerScript(s, pszTrigName, pSrc, pArgs);
		if ( iRet != TRIGRET_RET_FALSE && iRet != TRIGRET_RET_DEFAULT )
			return iRet;
	}


	// 4) TYPEDEF
	EXC_SET("typedef");
	{
		// It has an assigned trigger type.
		pResourceLink = dynamic_cast <CResourceLink *>( g_Cfg.ResourceGetDef( RESOURCE_ID( RES_TYPEDEF, GetType() )));
		if ( pResourceLink == NULL )
		{
			if ( pChar )
			{
				DEBUG_ERR(( "0%x '%s' has unhandled [TYPEDEF %d] for 0%x '%s'\n", (DWORD) GetUID(), GetName(), GetType(), (DWORD) pChar->GetUID(), pChar->GetName()));
			}
			else
			{
				DEBUG_ERR(( "0%x '%s' has unhandled [TYPEDEF %d]\n", (DWORD) GetUID(), GetName(), GetType() ));
			}
			m_type = Item_GetDef()->GetType();
			return( TRIGRET_RET_DEFAULT );
		}

		if ( pResourceLink->HasTrigger( iAction ))
		{
			CResourceLock s;
			if ( pResourceLink->ResourceLock(s))
			{
				iRet = CScriptObj::OnTriggerScript( s, pszTrigName, pSrc, pArgs );
				if ( iRet == TRIGRET_RET_TRUE )
				{
					return( iRet );	// Block further action.
				}
			}
		}
	}


	// Look up the trigger in the RES_ITEMDEF. (default)
	EXC_SET("itemdef");
	pResourceLink = Base_GetDef();
	ASSERT(pResourceLink);
	if ( pResourceLink->HasTrigger( iAction ))
	{
		CResourceLock s;
		if ( pResourceLink->ResourceLock(s))
		{
			iRet = CScriptObj::OnTriggerScript( s, pszTrigName, pSrc, pArgs );
		}
	}
	EXC_CATCH;

	EXC_DEBUG_START;
	g_Log.EventDebug("trigger '%s' action '%d' char '0%lx' [0%lx]\n", pszTrigName, iAction, (pSrc && pSrc->GetChar()) ? (DWORD)pSrc->GetChar()->GetUID() : 0, (DWORD)GetUID());
	EXC_DEBUG_END;
	return iRet;
}

void CItem::DupeCopy( const CItem * pItem )
{
	ADDTOCALLSTACK("CItem::DupeCopy");
	// Dupe this item.

	CObjBase::DupeCopy( pItem );

	m_wDispIndex = pItem->m_wDispIndex;
	SetBase( pItem->Item_GetDef() );
	m_type = pItem->m_type;
	m_amount = pItem->m_amount;
	m_Attr  = pItem->m_Attr;
	m_uidLink = pItem->m_uidLink;

	m_itNormal.m_more1 = pItem->m_itNormal.m_more1;
	m_itNormal.m_more2 = pItem->m_itNormal.m_more2;
	m_itNormal.m_morep = pItem->m_itNormal.m_morep;

	m_OEvents.Copy(&(pItem->m_OEvents));
}

void CItem::SetAnim( ITEMID_TYPE id, int iTime )
{
	ADDTOCALLSTACK("CItem::SetAnim");
	// Set this to an active anim that will revert to old form when done.
	// ??? use addEffect instead !!!
	m_itAnim.m_PrevID = GetID(); // save old id.
	m_itAnim.m_PrevType = m_type;
	SetDispID( id );
	m_type = IT_ANIM_ACTIVE;
	SetTimeout( iTime );
	Update();
}

void CItem::Update( const CClient * pClientExclude )
{
	ADDTOCALLSTACK("CItem::Update");
	// Send this new item to all that can see it.

	for ( CClient * pClient = g_Serv.GetClientHead(); pClient!=NULL; pClient = pClient->GetNext())
	{
		if ( pClient == pClientExclude )
			continue;
		if ( ! pClient->CanSee( this ))
			continue;

		if ( IsItemEquipped())
		{
			if ( GetEquipLayer() == LAYER_DRAGGING )
			{
				pClient->addObjectRemove( this );
				continue;
			}
			if ( GetEquipLayer() >= LAYER_SPECIAL )	// nobody cares about this stuff.
				return;
		}
		else if ( IsItemInContainer())
		{
			CItemContainer* pCont = dynamic_cast <CItemContainer*> (GetParent());
			ASSERT(pCont);
			if ( pCont->IsAttr(ATTR_INVIS))
			{
				// used to temporary build corpses.
				pClient->addObjectRemove( this );
				continue;
			}
		}
		pClient->addItem( this );
	}
}

bool CItem::IsValidLockLink( CItem * pItemLock ) const
{
	ADDTOCALLSTACK("CItem::IsValidLockLink");
	// IT_KEY
	if ( pItemLock == NULL )
	{
		return( false );
	}
	if ( pItemLock->IsTypeLockable())
	{
		return( pItemLock->m_itContainer.m_lockUID == m_itKey.m_lockUID );
	}
	if ( CItemBase::IsID_Multi( pItemLock->GetDispID()))
	{
		// Is it linked to the item it is locking. (should be)
		// We can't match the lock uid this way tho.
		// Houses may have multiple doors. and multiple locks
		return( true );
	}
	// not connected to anything i recognize.
	return( false );
}

bool CItem::IsValidLockUID() const
{
	ADDTOCALLSTACK("CItem::IsValidLockUID");
	// IT_KEY
	// Keys must:
	//  1. have m_lockUID == UID of the container.
	//  2. m_uidLink == UID of the multi.

	if ( ! m_itKey.m_lockUID.IsValidUID() )	// blank
		return( false );

	// or we are a key to a multi.
	// we can be linked to the multi.
	if ( IsValidLockLink( m_itKey.m_lockUID.ItemFind()))
		return( true );
	if ( IsValidLockLink( m_uidLink.ItemFind()))
		return( true );

	return( false );
}

void CItem::ConvertBolttoCloth()
{
	ADDTOCALLSTACK("CItem::ConvertBolttoCloth");

	//we need to check all cloth_bolt items
	bool correctID = false;
	for(int i=(int)ITEMID_CLOTH_BOLT1; i<=(int)ITEMID_CLOTH_BOLT8; i++)
		if ( IsSameDispID( (ITEMID_TYPE)i ))
			correctID=true;

	if ( !correctID )
		return;

	// Bolts of cloth are treated as 10 single pieces of cloth
	// Convert it to 10 pieces of cloth, then consume the amount we want later
	int iOutAmount = 0;
	CItemBase * pDefCloth = Item_GetDef();

	if ( pDefCloth )
	{
		for ( int i=0; i < pDefCloth->m_BaseResources.GetCount(); i++ )
		{
			RESOURCE_ID rid = pDefCloth->m_BaseResources[i].GetResourceID();
			if ( rid.GetResType() != RES_ITEMDEF )
				continue;

			const CItemBase * pBaseDef = CItemBase::FindItemBase( (ITEMID_TYPE) rid.GetResIndex());
			if ( pBaseDef == NULL )
				continue;

			if ( pBaseDef->IsType( IT_CLOTH )) // Maybe this is better: pBaseDef->IsSameDispID(ITEMID_CLOTH1)
			{
				iOutAmount += pDefCloth->m_BaseResources[i].GetResQty();
				SetName(pBaseDef->GetName());
				continue;
			}		

		}

	}


	if ( iOutAmount == 0 )
	{
		iOutAmount += 1;
	}

	SetID( ITEMID_CLOTH1 );
	SetAmountUpdate( iOutAmount * GetAmount());
}

int CItem::ConsumeAmount( int iQty, bool fTest )
{
	ADDTOCALLSTACK("CItem::ConsumeAmount");
	// Eat or drink specific item. delete it when gone.
	// return: the amount we used.
	if ( this == NULL )	// can use none if there is nothing? or can we use all?
		return iQty;

	int iQtyMax = GetAmount();
	if ( iQty < iQtyMax )
	{
		if ( ! fTest )
		{
			SetAmountUpdate( iQtyMax - iQty );
		}
		return( iQty );
	}

	if ( ! fTest )
	{
		SetAmount( 0 );	// let there be 0 amount here til decay.
		if ( ! IsTopLevel() || ! IsAttr( ATTR_INVIS ))	// don't delete resource locators.
		{
			Delete();
		}
	}

	return( iQtyMax );
}

SPELL_TYPE CItem::GetScrollSpell() const
{
	ADDTOCALLSTACK("CItem::GetScrollSpell");
	// Given a scroll type. what spell is this ?
	for (int i = SPELL_Clumsy; i < g_Cfg.m_SpellDefs.GetCount(); i++)
	{
		const CSpellDef * pSpellDef = g_Cfg.GetSpellDef( (SPELL_TYPE) i );
		if ( pSpellDef == NULL || pSpellDef->m_idScroll == ITEMID_NOTHING )
			continue;
		if ( GetID() == pSpellDef->m_idScroll )
			return((SPELL_TYPE) i );
	}
	return( SPELL_NONE );
}

bool CItem::IsSpellInBook( SPELL_TYPE spell ) const
{
	ADDTOCALLSTACK("CItem::IsSpellInBook");
	int i = spell;

	//	max 96 spells in one spellbook
	//	convert spell back to format of the book and check whatever it is in
	if ( i <= m_itSpellbook.m_baseid )
		return false;

	i -= (m_itSpellbook.m_baseid + 1);

	if ( i < 32 )
		return (m_itSpellbook.m_spells1 & (1 << i));
	else if ( i < 64 )
		return (m_itSpellbook.m_spells2 & (1 << (i-32)));
	else if ( i < 96 )
		return (m_itSpellbook.m_spells2 & (1 << (i-64)));
	else
		return false;
}

int CItem::GetSpellcountInBook()
{
	ADDTOCALLSTACK("CItem::GetSpellcountInBook");
	// -1 = can't count
	// n = number of spells

	if ( !IsTypeSpellbook() )
		return -1;

	int count = 0;
	for ( int i = SPELL_Clumsy; i < SPELL_BOOK_QTY; i++ )
	{
		if ( IsSpellInBook( (SPELL_TYPE) i ))
		{
			count++;
		}
	}

	return( count );
}

int CItem::AddSpellbookSpell( SPELL_TYPE spell, bool fUpdate )
{
	ADDTOCALLSTACK("CItem::AddSpellbookSpell");
	// Add  this scroll to the spellbook.
	// 0 = added.
	// 1 = already here.
	// 2 = not a scroll i know about.
	// 3 = not a valid spellbook

	if ( !IsTypeSpellbook() )
		return 3;
	if ( spell == SPELL_NONE )
		return 2;
	const CSpellDef * pSpellDef = g_Cfg.GetSpellDef(spell);
	if ( pSpellDef == NULL )
		return 2;

	if ( IsSpellInBook(spell) )
		return 1;

	//	add spell to a spellbook bitmask
	int i = spell;

	if ( i <= m_itSpellbook.m_baseid )
		return 3;
	i -= (m_itSpellbook.m_baseid + 1);
	if ( i < 32 )
		m_itSpellbook.m_spells1 |= (1 << i);
	else if ( i < 64 )
		m_itSpellbook.m_spells2 |= (1 << (i-32));
	else if ( i < 96 )
		m_itSpellbook.m_spells3 |= (1 << (i-64));
	else
		return 3;

	// update the spellbook
	if ( fUpdate)
	{
		CCommand cmd;
		cmd.ContAdd.m_Cmd = XCMD_ContAdd;
		cmd.ContAdd.m_UID = UID_F_ITEM + UID_O_INDEX_FREE + spell;
		cmd.ContAdd.m_id = pSpellDef->m_idScroll;
		cmd.ContAdd.m_zero7 = 0;
		cmd.ContAdd.m_amount = spell;
		cmd.ContAdd.m_x = 0x48;
		cmd.ContAdd.m_y = 0x7D;
		cmd.ContAdd.m_UIDCont = GetUID();
		cmd.ContAdd.m_wHue = HUE_DEFAULT;

		CCommand cmdNew = cmd;
		cmd.ContAddNew.m_grid = 0;
		cmd.ContAddNew.m_UIDCont = cmd.ContAdd.m_UIDCont;
		cmd.ContAddNew.m_wHue = cmd.ContAdd.m_wHue;

		for ( CClient * pClient = g_Serv.GetClientHead(); pClient!=NULL; pClient = pClient->GetNext())
		{
			if ( ! pClient->CanSee( this ))
				continue;

			if ( pClient->GetClientVersion() >= 0x0600018 || pClient->GetClientVersionReported() >= 0x0600018 || pClient->IsClientKR() )
				pClient->xSendPkt( &cmdNew, sizeof(cmd.ContAddNew) );
			else
				pClient->xSendPkt( &cmd, sizeof(cmd.ContAdd) );
		}
	}

	return 0;
}

int CItem::AddSpellbookScroll( CItem * pScroll )
{
	ADDTOCALLSTACK("CItem::AddSpellbookScroll");
	// Add  this scroll to the spellbook.
	// 0 = added.
	// 1 = already here.
	// 2 = not a scroll i know about.

	ASSERT(pScroll);
	int iRet = AddSpellbookSpell( pScroll->GetScrollSpell(), true );
	if ( iRet )
		return( iRet );
	pScroll->ConsumeAmount(1);	// we only need 1 scroll.
	return( 0 );
}

void CItem::Flip()
{
	ADDTOCALLSTACK("CItem::Flip");
	// Equivelant rotational objects.
	// These objects are all the same except they are rotated.

	if ( IsTypeLit())	// m_pDef->Can( CAN_I_LIGHT ) ?
	{
		if ( ++m_itLight.m_pattern >= LIGHT_QTY )
			m_itLight.m_pattern = 0;
		Update();
		return;
	}

	// Doors are easy.
	if ( IsType( IT_DOOR ) || IsType( IT_DOOR_LOCKED ) || IsType(IT_DOOR_OPEN))
	{
		ITEMID_TYPE id = GetDispID();
		int doordir = CItemBase::IsID_Door( id )-1;
		int iNewID = (id - doordir) + (( doordir &~DOOR_OPENED ) + 2 ) % 16; // next closed door type.
		SetDispID((ITEMID_TYPE) iNewID );
		Update();
		return;
	}

	if ( IsType( IT_CORPSE ))
	{
		m_itCorpse.m_facing_dir = GetDirTurn((DIR_TYPE) m_itCorpse.m_facing_dir, 1 );
		Update();
		return;
	}

	CItemBase * pItemDef = Item_GetDef();
	ASSERT(pItemDef);

	// Try to rotate the object.
	ITEMID_TYPE id = pItemDef->GetNextFlipID( GetDispID());
	if ( id != GetDispID())
	{
		SetDispID( id );
		Update();
	}
}

bool CItem::Use_Portculis()
{
	ADDTOCALLSTACK("CItem::Use_Portculis");
	// Set to the new z location.
	if ( ! IsTopLevel())
		return false;

	CPointMap pt = GetTopPoint();
	if ( pt.m_z == m_itPortculis.m_z1 )
		pt.m_z = m_itPortculis.m_z2;
	else
		pt.m_z = m_itPortculis.m_z1;

	if ( pt.m_z == GetTopZ())
		return false;

	MoveTo( pt );
	Update();

	SOUND_TYPE iSnd = NULL;
	CVarDefCont * pTagStorage = NULL; 
	pTagStorage = GetKey("OVERRIDE.PORTCULISSOUND", true);
	if ( pTagStorage )
	{
		if ( pTagStorage->GetValNum() )
			iSnd = pTagStorage->GetValNum();
		else
			iSnd = 0x21d;
	} else 
		iSnd = 0x21d;

	Sound( iSnd );

	return( true );
}

SOUND_TYPE CItem::Use_Music( bool fWell ) const
{
	ADDTOCALLSTACK("CItem::Use_Music");
	const CItemBase * pItemDef = Item_GetDef();
	return( fWell ? ( pItemDef->m_ttMusical.m_iSoundGood ) : ( pItemDef->m_ttMusical.m_iSoundBad ));
}

bool CItem::IsDoorOpen() const
{
	ADDTOCALLSTACK("CItem::IsDoorOpen");
	return( CItemBase::IsID_DoorOpen( GetDispID()));
}

bool CItem::Use_Door( bool fJustOpen )
{
	ADDTOCALLSTACK("CItem::Use_Door");
	// don't call this directly but call CChar::Use_Item() instead.
	// don't check to see if it is locked here
	// RETURN:
	//  true = open

	ITEMID_TYPE id = GetDispID();
	int doordir = CItemBase::IsID_Door( id )-1;
	if ( doordir < 0 || ! IsTopLevel())
		return( false );

	id = (ITEMID_TYPE) ( id - doordir );
	IT_TYPE typelock = m_type;

	bool fClosing = ( doordir & DOOR_OPENED );	// currently open
	if ( fJustOpen && fClosing )
		return( true );	// links just open

	CPointMap pt = GetTopPoint();
	switch ( doordir )
	{
		case 0:
			pt.m_x--;
			pt.m_y++;
			doordir++;
			break;
		case DOOR_OPENED:
			pt.m_x++;
			pt.m_y--;
			doordir--;
			break;
		case DOOR_RIGHTLEFT:
			pt.m_x++;
			pt.m_y++;
			doordir++;
			break;
		case (DOOR_RIGHTLEFT+DOOR_OPENED):
			pt.m_x--;
			pt.m_y--;
			doordir--;
			break;
		case DOOR_INOUT:
			pt.m_x--;
			doordir++;
			break;
		case (DOOR_INOUT+DOOR_OPENED):
			pt.m_x++;
			doordir--;
			break;
		case (DOOR_INOUT|DOOR_RIGHTLEFT):
			pt.m_x++;
			pt.m_y--;
			doordir++;
			break;
		case (DOOR_INOUT|DOOR_RIGHTLEFT|DOOR_OPENED):
			pt.m_x--;
			pt.m_y++;
			doordir--;
			break;
		case 8:
			pt.m_x++;
			pt.m_y++;
			doordir++;
			break;
		case 9:
			pt.m_x--;
			pt.m_y--;
			doordir--;
			break;
		case 10:
			pt.m_x++;
			pt.m_y--;
			doordir++;
			break;
		case 11:
			pt.m_x--;
			pt.m_y++;
			doordir--;
			break;
		case 12:
			doordir++;
			break;
		case 13:
			doordir--;
			break;
		case 14:
			pt.m_y--;
			doordir++;
			break;
		case 15:
			pt.m_y++;
			doordir--;
			break;
	}

	SetDispID((ITEMID_TYPE) ( id + doordir ));
	// SetType( typelock );	// preserve the fact that it was locked.
	MoveTo(pt);

	CVarDefCont * pTagStorage = NULL; 
	int piVal[2];
	int t_setFlag = 0;
	pTagStorage = GetKey("OVERRIDE.DOORSOUND_CLOSE", true);
	if ( pTagStorage )
	{
		piVal[0] = pTagStorage->GetValNum();
		t_setFlag = 1;
	} else
		piVal[0] = 0;
	pTagStorage = NULL;
	pTagStorage = GetKey("OVERRIDE.DOORSOUND_OPEN", true);
	if ( pTagStorage )
	{
		piVal[1] = pTagStorage->GetValNum();
		t_setFlag = 1;
	} else
		piVal[1] = 0;

	if ( t_setFlag )
	{
			Sound( fClosing ? piVal[0] : piVal[1] );
			SetTimeout( fClosing ? -1 : 60*TICK_PER_SEC );
			return( ! fClosing );
	} 

	switch ( id )
	{
		case ITEMID_DOOR_SECRET_1:
		case ITEMID_DOOR_SECRET_2:
		case ITEMID_DOOR_SECRET_3:
		case ITEMID_DOOR_SECRET_4:
		case ITEMID_DOOR_SECRET_5:
		case ITEMID_DOOR_SECRET_6:
			Sound( fClosing ? 0x002e : 0x002f );
			break;
		case ITEMID_DOOR_METAL_S:
		case ITEMID_DOOR_BARRED:
		case ITEMID_DOOR_METAL_L:
		case ITEMID_DOOR_IRONGATE_1:
		case ITEMID_DOOR_IRONGATE_2:
			Sound( fClosing ? 0x00f3 : 0x00eb );
			break;
		default:
			Sound( fClosing ? 0x00f1 : 0x00ea );
			break;
	}

	// Auto close the door in n seconds.
	SetTimeout( fClosing ? -1 : 60*TICK_PER_SEC );
	return( ! fClosing );
}

bool CItem::Armor_IsRepairable() const
{
	ADDTOCALLSTACK("CItem::Armor_IsRepairable");
	// We might want to check based on skills:
	// SKILL_BLACKSMITHING (armor)
	// SKILL_BOWERY (xbows)
	// SKILL_TAILORING (leather)
	//

	CItemBase * pItemDef = Item_GetDef();
	ASSERT(pItemDef);

	if ( pItemDef->Can( CAN_I_REPAIR ))
		return( true );

	switch ( m_type )
	{
	case IT_CLOTHING:
	case IT_ARMOR_LEATHER:
		return( false );	// Not this way anyhow.
	case IT_SHIELD:
	case IT_ARMOR:				// some type of armor. (no real action)
		// ??? Bone armor etc is not !
		break;
	case IT_WEAPON_MACE_CROOK:
	case IT_WEAPON_MACE_PICK:
	case IT_WEAPON_MACE_SMITH:	// Can be used for smithing ?
	case IT_WEAPON_MACE_STAFF:
	case IT_WEAPON_MACE_SHARP:	// war axe can be used to cut/chop trees.
	case IT_WEAPON_SWORD:
	case IT_WEAPON_FENCE:
	case IT_WEAPON_AXE:
		break;

	case IT_WEAPON_BOW:
		// wood Bows are not repairable !
		return( false );
	case IT_WEAPON_XBOW:
		return( true );
	default:
		return( false );
	}

	return( true );
}

int CItem::Armor_GetDefense() const
{
	ADDTOCALLSTACK("CItem::Armor_GetDefense");
	// Get the defensive value of the armor. plus magic modifiers.
	// Subtract for low hitpoints.
	if ( ! IsTypeArmor())
		return 0;

	CItemBase * pItemDef = Item_GetDef();
	ASSERT(pItemDef);

	int iVal = pItemDef->m_attackBase + m_ModAr;
	int iRepair = Armor_GetRepairPercent();
	iVal = IMULDIV(iVal, iRepair, 100);

	// Add any magical effect.
	if ( IsAttr(ATTR_MAGIC) )
	{
		iRepair = g_Cfg.GetSpellEffect(SPELL_Enchant, m_itArmor.m_spelllevel);
		iVal += iRepair;	// may even be a negative effect ?
		if ( iVal < 0 )
			iVal = 0;
	}
	return iVal;
}

int CItem::Weapon_GetAttack(bool bNoRandom) const
{
	ADDTOCALLSTACK("CItem::Weapon_GetAttack");
	// Get the base attack for the weapon plus magic modifiers.
	// Subtract for low hitpoints.
	if ( ! IsTypeWeapon())	// anything can act as a weapon.
		return 1;

	CItemBase * pItemDef = Item_GetDef();
	ASSERT(pItemDef);

	int iVal = pItemDef->m_attackBase + m_ModAr;
	if ( bNoRandom ) 
		iVal += pItemDef->m_attackRange;
	else
		iVal += Calc_GetRandVal(pItemDef->m_attackRange );
	iVal = IMULDIV( iVal, Armor_GetRepairPercent(), 100 );
	if ( IsAttr(ATTR_MAGIC) && ! IsType(IT_WAND))
	{
		iVal += g_Cfg.GetSpellEffect( SPELL_Enchant, m_itArmor.m_spelllevel );
		if ( iVal < 0 )
			iVal = 0;
	}
	return( iVal );
}

SKILL_TYPE CItem::Weapon_GetSkill() const
{
	ADDTOCALLSTACK("CItem::Weapon_GetSkill");
	// assuming this is a weapon. What skill does it apply to.

	CItemBase * pItemDef = Item_GetDef();
	ASSERT(pItemDef);

	int iSkillOverride = m_TagDefs.GetKeyNum("OVERRIDE_SKILL", true) - 1;
	if ( iSkillOverride == -1)
		iSkillOverride = m_TagDefs.GetKeyNum("OVERRIDE.SKILL", true) - 1;
	if ( iSkillOverride >= 0 && iSkillOverride < MAX_SKILL )
		return (SKILL_TYPE)iSkillOverride;

	if ( pItemDef->m_iSkill >= 0 && pItemDef->m_iSkill < MAX_SKILL )
		return (SKILL_TYPE)pItemDef->m_iSkill;

	switch ( pItemDef->GetType() )
	{
		case IT_WEAPON_MACE_CROOK:
		case IT_WEAPON_MACE_PICK:
		case IT_WEAPON_MACE_SMITH:	// Can be used for smithing ?
		case IT_WEAPON_MACE_STAFF:
		case IT_WEAPON_MACE_SHARP:	// war axe can be used to cut/chop trees.
			return( SKILL_MACEFIGHTING );
		case IT_WEAPON_SWORD:
		case IT_WEAPON_AXE:
			return( SKILL_SWORDSMANSHIP );
		case IT_WEAPON_FENCE:
			return( SKILL_FENCING );
		case IT_WEAPON_BOW:
		case IT_WEAPON_XBOW:
			return( SKILL_ARCHERY );
	}

	return( SKILL_WRESTLING );
}

LPCTSTR CItem::Use_SpyGlass( CChar * pUser ) const
{
	ADDTOCALLSTACK("CItem::Use_SpyGlass");
	// IT_SPY_GLASS
	// Assume we are in water now ?

	CPointMap ptCoords = pUser->GetTopPoint();

#define BASE_SIGHT 26 // 32 (UO_MAP_VIEW_RADAR) is the edge of the radar circle (for the most part)
	float rWeather = (float) ptCoords.GetSector()->GetWeather();
	float rLight = (float) ptCoords.GetSector()->GetLight();
	CGString sSearch;
	TCHAR	*pResult = Str_GetTemp();

	// Weather bonus
	double rWeatherSight = rWeather == 0.0 ? (0.25 * BASE_SIGHT) : 0.0;
	// Light level bonus
	double rLightSight = (1.0 - (rLight / 25.0)) * BASE_SIGHT * 0.25;
	int iVisibility = (int) (BASE_SIGHT + rWeatherSight + rLightSight);

	// Check for the nearest land, only check every 4th square for speed
	const CUOMapMeter * pMeter = g_World.GetMapMeter( ptCoords ); // Are we at sea?
	if ( !pMeter )
		return pResult;
	switch ( pMeter->m_wTerrainIndex )
	{
	case TERRAIN_WATER1:
	case TERRAIN_WATER2:
	case TERRAIN_WATER3:
	case TERRAIN_WATER4:
	case TERRAIN_WATER5:
	case TERRAIN_WATER6:
		{
			// Look for land if at sea
			CPointMap ptLand;
			for (int x = ptCoords.m_x - iVisibility; x <= ptCoords.m_x + iVisibility; x=x+2)
				for (int y = ptCoords.m_y - iVisibility; y <= ptCoords.m_y + iVisibility; y=y+2)
			{
				CPointMap ptCur(x,y,ptCoords.m_z);
				pMeter = g_World.GetMapMeter( ptCur );
				if ( !pMeter )
					return pResult;
				switch ( pMeter->m_wTerrainIndex )
				{
				case TERRAIN_WATER1:
				case TERRAIN_WATER2:
				case TERRAIN_WATER3:
				case TERRAIN_WATER4:
				case TERRAIN_WATER5:
				case TERRAIN_WATER6:
					break;
				default:
					if (ptCoords.GetDist(ptCur) < ptCoords.GetDist(ptLand))
						ptLand = ptCur;
				}
				break;
			}

			if ( ptLand.IsValidPoint())
				sSearch.Format( "%s %s. ", g_Cfg.GetDefaultMsg(DEFMSG_USE_SPYGLASS_LAND), (LPCTSTR) CPointBase::sm_szDirs[ ptCoords.GetDir(ptLand) ] );
			else if (rLight > 3)
				sSearch = g_Cfg.GetDefaultMsg(DEFMSG_USE_SPYGLASS_DARK);
			else if (rWeather != 0)
				sSearch = g_Cfg.GetDefaultMsg(DEFMSG_USE_SPYGLASS_WEATHER);
			else
				sSearch = g_Cfg.GetDefaultMsg(DEFMSG_USE_SPYGLASS_NO_LAND);
			strcpy( pResult, sSearch );
			break;
		}
	default:
		pResult[0] = '\0';
	}

	// Check for interesting items, like boats, carpets, etc.., ignore our stuff
	CItem * pItemSighted = NULL;
	CItem * pBoatSighted = NULL;
	int iItemSighted = 0;
	int iBoatSighted = 0;
	CWorldSearch ItemsArea( ptCoords, iVisibility );
	while (true)
	{
		CItem * pItem = ItemsArea.GetItem();
		if ( pItem == NULL )
			break;
		if ( pItem == this )
			continue;

		int iDist = ptCoords.GetDist(pItem->GetTopPoint());
		if ( iDist > iVisibility ) // See if it's beyond the "horizon"
			continue;
		if ( iDist <= 8 ) // spyglasses are fuzzy up close.
			continue;

		// Skip items linked to a ship or multi
		if ( pItem->m_uidLink.IsValidUID() )
		{
			CItem * pItemLink = pItem->m_uidLink.ItemFind();
			if (( pItemLink ) && ( pItemLink->IsTypeMulti() ))
					continue;
		}

		// Track boats separately from other items
		if ( iDist <= UO_MAP_VIEW_RADAR && // if it's visible in the radar window as a boat, report it
			pItem->m_type == IT_SHIP )
		{
			iBoatSighted ++; // Keep a tally of how many we see
			if (!pBoatSighted || iDist < ptCoords.GetDist(pBoatSighted->GetTopPoint())) // Only find closer items to us
			{
				pBoatSighted = pItem;
			}
		}
		else
		{
			iItemSighted ++; // Keep a tally of how much we see
			if (!pItemSighted || iDist < ptCoords.GetDist(pItemSighted->GetTopPoint())) // Only find the closest item to us, give boats a preference
			{
				pItemSighted = pItem;
			}
		}
	}
	if (iBoatSighted) // Report boat sightings
	{
		DIR_TYPE dir = ptCoords.GetDir(pBoatSighted->GetTopPoint());
		if (iBoatSighted == 1)
			sSearch.Format(g_Cfg.GetDefaultMsg(DEFMSG_SHIP_SEEN_SHIP_SINGLE), (LPCTSTR) pBoatSighted->GetName(), CPointBase::sm_szDirs[dir] );
		else
			sSearch.Format(g_Cfg.GetDefaultMsg(DEFMSG_SHIP_SEEN_SHIP_MANY), (LPCTSTR) CPointBase::sm_szDirs[dir] );
		strcat( pResult, sSearch);
	}

	if (iItemSighted) // Report item sightings, also boats beyond the boat visibility range in the radar screen
	{
		int iDist = ptCoords.GetDist(pItemSighted->GetTopPoint());
		DIR_TYPE dir = ptCoords.GetDir(pItemSighted->GetTopPoint());
		if (iItemSighted == 1)
		{
			if ( iDist > UO_MAP_VIEW_RADAR) // if beyond ship visibility in the radar window, don't be specific
				sSearch.Format(g_Cfg.GetDefaultMsg(DEFMSG_SHIP_SEEN_STH_DIR), (LPCTSTR) CPointBase::sm_szDirs[ dir ] );
			else
				sSearch.Format(g_Cfg.GetDefaultMsg(DEFMSG_SHIP_SEEN_ITEM_DIR), (LPCTSTR) pItemSighted->GetNameFull(false), (LPCTSTR) CPointBase::sm_szDirs[ dir ] );
		}
		else
		{
			if ( iDist > UO_MAP_VIEW_RADAR) // if beyond ship visibility in the radar window, don't be specific
				sSearch.Format(g_Cfg.GetDefaultMsg(DEFMSG_SHIP_SEEN_ITEM_DIR_MANY), CPointBase::sm_szDirs[ dir ] );
			else
				sSearch.Format(g_Cfg.GetDefaultMsg(DEFMSG_SHIP_SEEN_SPECIAL_DIR), (LPCTSTR) pItemSighted->GetNameFull(false), (LPCTSTR) CPointBase::sm_szDirs[ dir ] );
		}
		strcat( pResult, sSearch);
	}

	// Check for creatures
	CChar * pCharSighted = NULL;
	int iCharSighted = 0;
	CWorldSearch AreaChar( ptCoords, iVisibility );
	while (true)
	{
		CChar * pChar = AreaChar.GetChar();
		if ( pChar == NULL )
			break;
		if ( pChar == pUser )
			continue;
		if ( pChar->m_pArea->IsFlag(REGION_FLAG_SHIP))
			continue; // skip creatures on ships, etc.
		int iDist = ptCoords.GetDist(pChar->GetTopPoint());
		if ( iDist > iVisibility ) // Can we see it?
			continue;
		iCharSighted ++;
		if ( !pCharSighted || iDist < ptCoords.GetDist(pCharSighted->GetTopPoint())) // Only find the closest char to us
		{
			pCharSighted = pChar;
		}
	}

	if (iCharSighted > 0) // Report creature sightings, don't be too specific (that's what tracking is for)
	{
		DIR_TYPE dir =  ptCoords.GetDir(pCharSighted->GetTopPoint());

		if (iCharSighted == 1)
			sSearch.Format(g_Cfg.GetDefaultMsg(DEFMSG_SHIP_SEEN_CREAT_SINGLE), CPointBase::sm_szDirs[dir] );
		else
			sSearch.Format(g_Cfg.GetDefaultMsg(DEFMSG_SHIP_SEEN_CREAT_MANY), CPointBase::sm_szDirs[dir] );
		strcat( pResult, sSearch);
	}
	return pResult;
}

LPCTSTR CItem::Use_Sextant( CPointMap pntCoords ) const
{
	ADDTOCALLSTACK("CItem::Use_Sextant");
	// IT_SEXTANT
	return g_Cfg.Calc_MaptoSextant(pntCoords);
}

bool CItem::Use_Light()
{
	ADDTOCALLSTACK("CItem::Use_Light");
	ASSERT( IsType(IT_LIGHT_OUT) || IsType(IT_LIGHT_LIT) );

	if ( IsType(IT_LIGHT_OUT) && IsItemInContainer())
		return( false );

	ITEMID_TYPE id = (ITEMID_TYPE) Light_GetOverride();
	if ( id == ITEMID_NOTHING )
		return( false );

	SetID( id );	// this will set the new m_typez

	if ( IsType(IT_LIGHT_LIT))
	{
		SetTimeout( 10*60*TICK_PER_SEC );
		if ( ! m_itLight.m_charges )
			m_itLight.m_charges = 20;
	}

	Sound( 0x226 );
	RemoveFromView();
	Update();
	return( true );
}

int CItem::Light_GetOverride(CItemBase * pBase)
{
	ADDTOCALLSTACK("CItem::Light_GetOverride");
	if ( !pBase )
		pBase = Item_GetDef();

	int ribReturn = pBase->m_ttEquippable.m_Light_ID.GetResIndex();

	int iBase = m_TagDefs.GetKeyNum("OVERRIDE_LIGHTID",true);
	if ( iBase )
		ribReturn = iBase;

	return( ribReturn );
}


int CItem::Use_LockPick( CChar * pCharSrc, bool fTest, bool fFail )
{
	ADDTOCALLSTACK("CItem::Use_LockPick");
	// This is the locked item.
	// Char is using a lock pick on it.
	// SKILL_LOCKPICKING
	// SKILL_MAGERY
	// RETURN:
	//  -1 not possible.
	//  0 = trivial
	//  difficulty of unlocking this (0 100)
	//

	if ( pCharSrc == NULL )
		return -1;

	if ( ! IsTypeLocked())
	{
		pCharSrc->SysMessage( g_Cfg.GetDefaultMsg( DEFMSG_UNLOCK_CANT_GENERAL ) );
		return -1;
	}

	CChar * pCharTop = dynamic_cast <CChar*>( GetTopLevelObj());
	if ( pCharTop && pCharTop != pCharSrc )
	{
		pCharSrc->SysMessage( g_Cfg.GetDefaultMsg( DEFMSG_UNLOCK_CANT_THERE ) );
		return -1;
	}

	// If we have the key allow unlock using spell.
	if ( pCharSrc->ContentFindKeyFor( this ))
	{
		if ( !fTest )
			pCharSrc->SysMessage( g_Cfg.GetDefaultMsg( DEFMSG_LOCK_HAS_KEY ) );

		return( 0 );
	}

	if ( IsType(IT_DOOR_LOCKED) && g_Cfg.m_iMagicUnlockDoor != -1 )
	{
		if ( g_Cfg.m_iMagicUnlockDoor == 0 )
		{
			pCharSrc->SysMessage( g_Cfg.GetDefaultMsg( DEFMSG_UNLOCK_DOOR_NEEDS_KEY ) );
			return -1;
		}

		// you can get flagged criminal for this action.
		if ( ! fTest )
		{
			pCharSrc->CheckCrimeSeen( SKILL_SNOOPING, NULL, this, g_Cfg.GetDefaultMsg( DEFMSG_LOCK_PICK_CRIME ) );
		}

		if ( Calc_GetRandVal( g_Cfg.m_iMagicUnlockDoor ))
		{
			return 10000;	// plain impossible.
		}
	}

	// If we don't have a key, we still have a *tiny* chance.
	// This is because players could have lockable chests too and we don't want it to be too easy
	if ( ! fTest )
	{
		if ( fFail )
		{
			if ( IsType(IT_DOOR_LOCKED))
			{
				pCharSrc->SysMessage( g_Cfg.GetDefaultMsg( DEFMSG_UNLOCK_FAIL_DOOR ) );
			}
			else
			{
				pCharSrc->SysMessage( g_Cfg.GetDefaultMsg( DEFMSG_UNLOCK_FAIL_CONT ) );
			}
			return( -1 );
		}

		// unlock it.
		pCharSrc->Use_KeyChange( this );
	}

	return( m_itContainer.m_lock_complexity / 10 );
}

void CItem::SetSwitchState()
{
	ADDTOCALLSTACK("CItem::SetSwitchState");
	if ( m_itSwitch.m_SwitchID )
	{
		ITEMID_TYPE id = m_itSwitch.m_SwitchID;
		m_itSwitch.m_SwitchID = GetDispID();
		SetDispID( id );
	}
}

void CItem::SetTrapState( IT_TYPE state, ITEMID_TYPE id, int iTimeSec )
{
	ADDTOCALLSTACK("CItem::SetTrapState");
	ASSERT( IsType(IT_TRAP) || IsType(IT_TRAP_ACTIVE) || IsType(IT_TRAP_INACTIVE) );
	ASSERT( state == IT_TRAP || state == IT_TRAP_ACTIVE || state == IT_TRAP_INACTIVE );

	if ( ! id )
	{
		id = m_itTrap.m_AnimID;
		if ( ! id )
		{
			id = (ITEMID_TYPE)( GetDispID() + 1 );
		}
	}
	if ( ! iTimeSec )
	{
		iTimeSec = 3*TICK_PER_SEC;
	}
	else if ( iTimeSec > 0 && iTimeSec < USHRT_MAX )
	{
		iTimeSec *= TICK_PER_SEC;
	}
	else
	{
		iTimeSec = -1;
	}

	if ( id != GetDispID())
	{
		m_itTrap.m_AnimID = GetDispID(); // save old id.
		SetDispID( id );
	}

	SetType( state );
	SetTimeout( iTimeSec );
	Update();
}

int CItem::Use_Trap()
{
	ADDTOCALLSTACK("CItem::Use_Trap");
	// We activated a trap somehow.
	// We might not be close to it tho.
	// RETURN:
	//   The amount of damage.

	ASSERT( m_type == IT_TRAP || m_type == IT_TRAP_ACTIVE );
	if ( m_type == IT_TRAP )
	{
		SetTrapState( IT_TRAP_ACTIVE, m_itTrap.m_AnimID, m_itTrap.m_wAnimSec );
	}

	if ( ! m_itTrap.m_Damage ) m_itTrap.m_Damage = 2;
	return( m_itTrap.m_Damage );	// base damage done.
}

bool CItem::SetMagicLock( CChar * pCharSrc, int iSkillLevel )
{
	ADDTOCALLSTACK("CItem::SetMagicLock");
	if ( pCharSrc == NULL )
		return false;

	if ( IsTypeLocked())
	{
		pCharSrc->SysMessage( g_Cfg.GetDefaultMsg( DEFMSG_LOCK_ALREADY_IS ) );
		return false;
	}
	if ( ! IsTypeLockable())
	{
		// Maybe lock items to the ground ?
		pCharSrc->SysMessage( g_Cfg.GetDefaultMsg( DEFMSG_LOCK_UNLOCKEABLE ) );
		return false;
	}
   if ( IsAttr( ATTR_OWNED ) )
	{
		pCharSrc->SysMessage( g_Cfg.GetDefaultMsg( DEFMSG_LOCK_MAY_NOT ) );
		return false;
	}
	switch ( m_type )
	{
		// Have to check for keys here or people would be
		// locking up dungeon chests.
		case IT_CONTAINER:
			if ( pCharSrc->ContentFindKeyFor( this ))
			{
				m_type=IT_CONTAINER_LOCKED;
				pCharSrc->SysMessage( g_Cfg.GetDefaultMsg( DEFMSG_LOCK_CONT_OK ) );
			}
			else
			{
				pCharSrc->SysMessage( g_Cfg.GetDefaultMsg( DEFMSG_LOCK_CONT_NO_KEY ) );
				return false;
			}
			break;

			// Have to check for keys here or people would be
			// locking all the doors in towns
		case IT_DOOR:
		case IT_DOOR_OPEN:
			if ( pCharSrc->ContentFindKeyFor( this ))
			{
				m_type=IT_DOOR_LOCKED;
				pCharSrc->SysMessage( g_Cfg.GetDefaultMsg( DEFMSG_LOCK_DOOR_OK ) );
			}
			else
			{
				pCharSrc->SysMessage( g_Cfg.GetDefaultMsg( DEFMSG_LOCK_DOOR_NO_KEY ) );
				return false;
			}
			break;
		case IT_SHIP_HOLD:
			if ( pCharSrc->ContentFindKeyFor( this ))
			{
				m_type=IT_SHIP_HOLD_LOCK;
				pCharSrc->SysMessage( g_Cfg.GetDefaultMsg( DEFMSG_LOCK_HOLD_OK ) );
			}
			else
			{
				pCharSrc->SysMessage( g_Cfg.GetDefaultMsg( DEFMSG_LOCK_HOLD_NO_KEY ) );
				return false;
			}
			break;
		case IT_SHIP_SIDE:
			m_type=IT_SHIP_SIDE_LOCKED;
			pCharSrc->SysMessage( g_Cfg.GetDefaultMsg( DEFMSG_LOCK_SHIP_OK ) );
			break;
		default:
			pCharSrc->SysMessage( g_Cfg.GetDefaultMsg( DEFMSG_LOCK_CAN_NOT ) );
			return false;
		}

	return( true );
}

bool CItem::OnSpellEffect( SPELL_TYPE spell, CChar * pCharSrc, int iSkillLevel, CItem * pSourceItem )
{
	ADDTOCALLSTACK("CItem::OnSpellEffect");
	//
	// A spell is cast on this item.
	// ARGS:
	//  iSkillLevel = 0-1000 = difficulty. may be slightly larger . how advanced is this spell (might be from a wand)
	//

	CScriptTriggerArgs Args( (int) spell, iSkillLevel, pSourceItem );
	TRIGRET_TYPE iRet	= OnTrigger( ITRIG_SPELLEFFECT, pCharSrc, &Args );
	spell		= (SPELL_TYPE) Args.m_iN1;
	const CSpellDef * pSpellDef = g_Cfg.GetSpellDef( spell );

	switch ( iRet )
	{
	case TRIGRET_RET_TRUE:		return false;
	case TRIGRET_RET_FALSE:		if ( pSpellDef && pSpellDef->IsSpellType( SPELLFLAG_SCRIPTED ) ) return true;
	}
	
	if ( !IsSetEF(EF_Minimize_Triggers) )
	{
		iRet	= Spell_OnTrigger( spell, SPTRIG_EFFECT, pCharSrc, &Args );
	}
	spell		= (SPELL_TYPE) Args.m_iN1;
	iSkillLevel	= Args.m_iN2;
	pSpellDef = g_Cfg.GetSpellDef( spell );

	switch ( iRet )
	{
	case TRIGRET_RET_TRUE:		return false;
	case TRIGRET_RET_FALSE:		if ( pSpellDef && pSpellDef->IsSpellType( SPELLFLAG_SCRIPTED ) ) return true;
	}

	if ( IsType(IT_WAND) )	// try to recharge the wand.
	{
		if ( ! m_itWeapon.m_spell || RES_GET_INDEX(m_itWeapon.m_spell) == spell )
		{
			SetAttr(ATTR_MAGIC);
			if ( ! m_itWeapon.m_spell || ( pCharSrc && pCharSrc->IsPriv( PRIV_GM )))
			{
				m_itWeapon.m_spell = spell;
				m_itWeapon.m_spelllevel = iSkillLevel;
				m_itWeapon.m_spellcharges = 0;
			}

			// Spells chance of blowing up the wand is based on total energy level.

			int ispellbase = pSpellDef->m_wManaUse;
			int iChanceToExplode = Calc_GetBellCurve( m_itWeapon.m_spellcharges+3+ispellbase/10, 2 ) / 2;
			if ( ! Calc_GetRandVal(iChanceToExplode))
			{
				// Overcharge will explode !
				Emote( "overcharged wand explodes" );
				g_World.Explode( pCharSrc, GetTopLevelObj()->GetTopPoint(),
					3, 2 + (ispellbase+iSkillLevel)/20,
					DAMAGE_MAGIC | DAMAGE_GENERAL | DAMAGE_HIT_BLUNT );
				Delete();
				return false;
			}
			else
			{
				m_itWeapon.m_spellcharges++;
			}
		}
	}

	if ( pCharSrc && (!pCharSrc->IsPriv(PRIV_GM) && pCharSrc->GetRegion()->CheckAntiMagic(spell)) )
	{
		pCharSrc->SysMessage("An anti-magic field disturbs this spell.");
		return false;
	}

	WORD uDamage = 0;
	switch ( spell )
	{
	case SPELL_Dispel_Field:
	case SPELL_Dispel:
	case SPELL_Mass_Dispel:
		switch ( GetType())
		{
		case IT_CAMPFIRE:
		case IT_FIRE:
		case IT_SPELL:
			// ??? compare the strength of the spells ?
			{
			if ( IsTopLevel())
			{
				CItem * pItem = CItem::CreateScript( ITEMID_FX_SPELL_FAIL, pCharSrc );
				pItem->MoveToDecay( GetTopPoint(), 2*TICK_PER_SEC );
			}
			Delete();
			}
			break;
		}
		break;

	case SPELL_Curse:
		return false;
	case SPELL_Bless:
		return false;
	case SPELL_Incognito:
		ClrAttr(ATTR_IDENTIFIED);
		return true;
	case SPELL_Lightning:
		Effect( EFFECT_LIGHTNING, ITEMID_NOTHING, pCharSrc );
		break;
	case SPELL_Explosion:
	case SPELL_Fireball:
	case SPELL_Fire_Bolt:
	case SPELL_Fire_Field:
	case SPELL_Flame_Strike:
	case SPELL_Meteor_Swarm:
		uDamage = DAMAGE_FIRE;
		break;

	case SPELL_Magic_Lock:
		if ( ! SetMagicLock( pCharSrc, iSkillLevel ))
			return( false );
		break;

	case SPELL_Unlock:
		{
			int iDifficulty = Use_LockPick( pCharSrc, true, false );
			if ( iDifficulty < 0 )
				return( false );
			bool fSuccess = g_Cfg.Calc_SkillCheck( iSkillLevel, iDifficulty );
			Use_LockPick( pCharSrc, false, ! fSuccess );
			return fSuccess;
		}

	case SPELL_Mark:
		if ( pCharSrc == NULL )
			return false;

		if ( ! pCharSrc->IsPriv(PRIV_GM))
		{
			if ( ! IsType(IT_RUNE) && ! IsType(IT_TELEPAD) )
			{
				pCharSrc->SysMessage( "That item is not a recall rune." );
				return false;
			}
			if ( GetTopLevelObj() != pCharSrc )
			{
				// Prevent people from remarking GM teleport pads if they can't pick it up.
				pCharSrc->SysMessage( "The rune must be on your person to mark." );
				return false;
			}
		}

		m_itRune.m_pntMark = pCharSrc->GetTopPoint();
		if ( IsType(IT_RUNE) )
		{
			m_itRune.m_Strength = pSpellDef->m_Effect.GetLinear( iSkillLevel );
			SetName( pCharSrc->m_pArea->GetName() );
			ResendTooltip();
		}
		break;

	case SPELL_Resurrection:
		// Should be a corpse .
		if ( IsType( IT_CORPSE ))
		{
			CItemCorpse * pCorpse = dynamic_cast <CItemCorpse*>(this);
			ASSERT(pCorpse);

			// If the corpse is attached to a player or other disconnected person then yank them back.
			CChar * pChar = pCorpse->m_uidLink.CharFind();
			if ( pChar )
			{
				pChar->Spell_Resurrection(pCorpse);
			}
			else
			{
				// Just an NPC corpse I guess.
				pChar = CChar::CreateBasic(m_itCorpse.m_BaseID);
				if ( pChar == NULL )
					return( false );
				pChar->NPC_LoadScript(false);
				pChar->RaiseCorpse(pCorpse);
			}
		}
		break;
	}

	// ??? Potions should explode when hit (etc..)
	if ( pSpellDef->IsSpellType( SPELLFLAG_HARM ))
	{
		OnTakeDamage( 1, pCharSrc, DAMAGE_HIT_BLUNT | DAMAGE_MAGIC | uDamage );
	}

	return( true );
}

int CItem::Armor_GetRepairPercent() const
{
	ADDTOCALLSTACK("CItem::Armor_GetRepairPercent");

	if ( !m_itArmor.m_Hits_Max || ( m_itArmor.m_Hits_Max < m_itArmor.m_Hits_Cur )) return( 100 );
 	return( IMULDIV( m_itArmor.m_Hits_Cur, 100, m_itArmor.m_Hits_Max ));
}

LPCTSTR CItem::Armor_GetRepairDesc() const
{
	ADDTOCALLSTACK("CItem::Armor_GetRepairDesc");
	if ( m_itArmor.m_Hits_Cur > m_itArmor.m_Hits_Max )
	{
		return g_Cfg.GetDefaultMsg( DEFMSG_ITEMSTATUS_PERFECT );
	}
	else if ( m_itArmor.m_Hits_Cur == m_itArmor.m_Hits_Max )
	{
		return g_Cfg.GetDefaultMsg( DEFMSG_ITEMSTATUS_FULL );
	}
	else if ( m_itArmor.m_Hits_Cur > m_itArmor.m_Hits_Max / 2 )
	{
		return g_Cfg.GetDefaultMsg( DEFMSG_ITEMSTATUS_SCRATCHED );
	}
	else if ( m_itArmor.m_Hits_Cur > m_itArmor.m_Hits_Max / 3 )
	{
		return g_Cfg.GetDefaultMsg( DEFMSG_ITEMSTATUS_WELLWORN );
	}
	else if ( m_itArmor.m_Hits_Cur > 3 )
	{
		return g_Cfg.GetDefaultMsg( DEFMSG_ITEMSTATUS_BADLY );
	}
	else
	{
		return g_Cfg.GetDefaultMsg( DEFMSG_ITEMSTATUS_FALL_APART );
	}
}

int CItem::OnTakeDamage( int iDmg, CChar * pSrc, DAMAGE_TYPE uType )
{
	ADDTOCALLSTACK("CItem::OnTakeDamage");
	// Break stuff.
	// Explode potions. freeze and break ? scrolls get wet ?
	//
	// pSrc = The source of the damage. May not be the person wearing the item.
	//
	// RETURN:
	//  Amount of damage done.
	//  INT_MAX = destroyed !!!
	//  -1 = invalid target ?

	if ( iDmg <= 0 )
		return( 0 );

	CScriptTriggerArgs Args( iDmg, (int) uType );
	if ( OnTrigger( ITRIG_DAMAGE, pSrc, &Args ) == TRIGRET_RET_TRUE )
	{
		return( 0 );
	}

	switch ( GetType())
	{
	case IT_CLOTHING:
		if ( ( uType & DAMAGE_FIRE ) &&
			! IsAttr( ATTR_MAGIC|ATTR_NEWBIE|ATTR_MOVE_NEVER ))
		{
			// normal cloth takes special damage from fire.
			goto forcedamage;
		}
		break;

	case IT_WEAPON_ARROW:
	case IT_WEAPON_BOLT:
		if ( iDmg == 1 )
		{
			// Miss - They will ussually survive.
			if ( Calc_GetRandVal(5))
				return( 0 );
		}
		else
		{
			// Must have hit.
			if ( ! Calc_GetRandVal(3))
				return( 1 );
		}
		Delete();
		return INT_MAX;

	case IT_POTION:
		if ( uType & ( DAMAGE_HIT_BLUNT|DAMAGE_HIT_PIERCE|DAMAGE_HIT_SLASH|DAMAGE_GOD|DAMAGE_MAGIC|DAMAGE_FIRE ))
		{
			if ( RES_GET_INDEX(m_itPotion.m_Type) == SPELL_Explosion )
			{
				g_World.Explode( pSrc, GetTopLevelObj()->GetTopPoint(), 3,
					g_Cfg.GetSpellEffect( SPELL_Explosion, m_itPotion.m_skillquality ),
					DAMAGE_GENERAL | DAMAGE_HIT_BLUNT | DAMAGE_FIRE);
				Delete();
				return( INT_MAX );
			}
		}
		return( 1 );

	case IT_WEB:
		if ( ! ( uType & (DAMAGE_FIRE|DAMAGE_HIT_BLUNT|DAMAGE_HIT_SLASH|DAMAGE_GOD)))
		{
			if ( pSrc ) pSrc->SysMessage( g_Cfg.GetDefaultMsg( DEFMSG_WEB_NOEFFECT ) );
			return( 0 );
		}

		iDmg = Calc_GetRandVal( iDmg ) + 1;
		if ( iDmg > m_itWeb.m_Hits_Cur || ( uType & DAMAGE_FIRE ))
		{
			if ( pSrc ) pSrc->SysMessage( g_Cfg.GetDefaultMsg( DEFMSG_WEB_DESTROY ) );
			if ( Calc_GetRandVal( 2 ) || ( uType & DAMAGE_FIRE ))
			{
				Delete();
				return( INT_MAX );
			}
			SetID( ITEMID_REAG_SS );
			Update();
			return( 2 );
		}

		if ( pSrc ) pSrc->SysMessage( g_Cfg.GetDefaultMsg( DEFMSG_WEB_WEAKEN ) );
		m_itWeb.m_Hits_Cur -= iDmg;
		return( 1 );
	}

	// Break armor etc..
	if ( IsTypeArmorWeapon() && ( uType & ( DAMAGE_HIT_BLUNT | DAMAGE_HIT_PIERCE | DAMAGE_HIT_SLASH | DAMAGE_GOD|DAMAGE_MAGIC|DAMAGE_FIRE )))
	{
		// Server option to slow the rate ???
		if ( Calc_GetRandVal( m_itArmor.m_Hits_Max * 16 ) > iDmg )
		{
			return 1;
		}

forcedamage:

		// describe the damage to the item.

		CChar * pChar = dynamic_cast <CChar*> ( GetTopLevelObj());

		if ( m_itArmor.m_Hits_Cur <= 1 )
		{
			m_itArmor.m_Hits_Cur = 0;
			Emote( g_Cfg.GetDefaultMsg( DEFMSG_ITEM_DMG_DESTROYED ) );
			Delete();
			return( INT_MAX );
		}

		--m_itArmor.m_Hits_Cur;

		TCHAR *pszMsg = Str_GetTemp();
		if ( pSrc )	// tell hitter they scored !
		{
			if ( pChar && pChar != pSrc )
				sprintf(pszMsg, g_Cfg.GetDefaultMsg( DEFMSG_ITEM_DMG_DAMAGE1 ), (LPCTSTR) pChar->GetName(), (LPCTSTR) GetName());
			else
				sprintf(pszMsg, g_Cfg.GetDefaultMsg( DEFMSG_ITEM_DMG_DAMAGE2 ), (LPCTSTR) GetName());
			pSrc->SysMessage(pszMsg);
		}
		if ( pChar && pChar != pSrc )
		{
			// Tell target they got damaged.
			*pszMsg = 0;
			if ( m_itArmor.m_Hits_Cur < m_itArmor.m_Hits_Max / 2 )
			{
				int iPercent = Armor_GetRepairPercent();
				if ( pChar->Skill_GetAdjusted( SKILL_ARMSLORE ) / 10 > iPercent )
					sprintf(pszMsg, g_Cfg.GetDefaultMsg( DEFMSG_ITEM_DMG_DAMAGE3 ), (LPCTSTR) GetName(), (LPCTSTR) Armor_GetRepairDesc());
			}
			if ( !*pszMsg )
				sprintf(pszMsg, g_Cfg.GetDefaultMsg( DEFMSG_ITEM_DMG_DAMAGE4 ), (LPCTSTR) GetName());
			pChar->SysMessage(pszMsg);
		}
		return( 2 );
	}

	// don't know how to calc damage for this.
	return( 0 );
}

bool CItem::OnExplosion()
{
	ADDTOCALLSTACK("CItem::OnExplosion");
	// IT_EXPLOSION
	// Async explosion.
	// RETURN: true = done. (delete the animation)

	ASSERT( IsTopLevel());
	ASSERT( m_type == IT_EXPLOSION );

	if ( ! m_itExplode.m_wFlags )
		return( true );

	CChar * pSrc = m_uidLink.CharFind();

	CWorldSearch AreaChars( GetTopPoint(), m_itExplode.m_iDist );
	while (true)
	{
		CChar * pChar = AreaChars.GetChar();
		if ( pChar == NULL )
			break;
		if ( GetTopDist3D( pChar ) > m_itExplode.m_iDist )
			continue;
		pChar->Effect( EFFECT_OBJ, ITEMID_FX_EXPLODE_1, pSrc, 9, 6 );
		pChar->OnTakeDamage( m_itExplode.m_iDamage, pSrc, m_itExplode.m_wFlags );
	}

	// Damage objects near by.
	CWorldSearch AreaItems( GetTopPoint(), m_itExplode.m_iDist );
	while (true)
	{
		CItem * pItem = AreaItems.GetItem();
		if ( pItem == NULL )
			break;
		if ( pItem == this )
			continue;
		if ( GetTopDist3D( pItem ) > m_itExplode.m_iDist )
			continue;
		pItem->OnTakeDamage( m_itExplode.m_iDamage, pSrc, m_itExplode.m_wFlags );
	}

	m_itExplode.m_wFlags = 0;
	SetDecayTime( 3 * TICK_PER_SEC );
	return( false );	// don't delete it yet.
}

bool CItem::IsResourceMatch( RESOURCE_ID_BASE rid, DWORD dwArg )
{
	ADDTOCALLSTACK("CItem::IsResourceMatch");
	// Check for all the matching special cases.
	// ARGS:
	//  dwArg = specific key or map (for typedefs)
	//          exclude item id (for itemdefs and typedefs)

	if ( GetAmount() == 0 )
		return( false );	// does not count for any matches.

	CItemBase * pItemDef = Item_GetDef();
	ASSERT(pItemDef);

	if ( rid == pItemDef->GetResourceID())
		return( true );

	RES_TYPE restype = rid.GetResType();
	int index = rid.GetResIndex();

	switch ( restype )
	{
	case RES_TYPEDEF:
		if ( ! IsType( (IT_TYPE) index ))
			return( false );

		if ( dwArg )
		{
			switch ( index )
			{
			case IT_MAP:
				// matching maps.
				if ( LOWORD(dwArg) != m_itMap.m_top ||
					HIWORD(dwArg) != m_itMap.m_left )
				{
					return( false );
				}
				break;
			case IT_KEY:
				if ( ! IsKeyLockFit( dwArg ))
					return( false );
				break;
			}

			if ( dwArg == GetID())
				return( false );
		}
		return( true );

	case RES_ITEMDEF:

		if ( dwArg == GetID())
			return( false );

		if ( GetID() == index )
			return true;

		if ( IsSetEF( EF_Item_Strict_Comparison ) )
			return false;

		switch ( index )
		{
		case ITEMID_CLOTH1:
			if ( IsType(IT_CLOTH) || IsType(IT_CLOTH_BOLT))
			{
				ConvertBolttoCloth();
				return( true );
			}
			break;
		case ITEMID_LEATHER_1:
		case ITEMID_HIDES:
			if ( IsType( IT_HIDE ) || IsType( IT_LEATHER ))
			{
				// We should be able to use leather in place of hides.
				return( true );
			}
			break;
		case ITEMID_LOG_1:
		case ITEMID_LUMBER1:
			if ( IsType(IT_LOG) || IsType(IT_LUMBER))
			{
				return( true );
			}
			break;
		case ITEMID_Arrow:
			if ( IsType(IT_WEAPON_ARROW))
			{
				return( true );
			}
			break;
		case ITEMID_XBolt:
			if ( IsType(IT_WEAPON_BOLT))
			{
				return( true );
			}
			break;
		}
		break;
	}

	return( false );
}

bool CItem::OnTick()
{
	ADDTOCALLSTACK("CItem::OnTick");
	// Timer expired. Time to do something.
	// RETURN: false = delete it.

	EXC_TRY("Tick");

	EXC_SET("timer trigger");
	SetTimeout(-1);
	TRIGRET_TYPE iRet = OnTrigger( ITRIG_TIMER, &g_Serv );
	if ( iRet == TRIGRET_RET_TRUE )
		return true;

	EXC_SET("GetType");
	IT_TYPE type = m_type;

	EXC_SET("default behaviour");
	switch ( type )
	{
		case IT_LIGHT_LIT:
			{
				// use up the charges that this has .
				EXC_SET("default behaviour::IT_LIGHT_LIT");
				if ( m_itLight.m_charges > 1 )
				{
					if ( m_itLight.m_charges == USHRT_MAX )
					{
						return true;
					}
					m_itLight.m_charges --;
					SetTimeout( 10*60*TICK_PER_SEC );
				}
				else
				{
					// Torches should just go away but lanterns etc should not.
					Emote( g_Cfg.GetDefaultMsg( DEFMSG_LIGHTSRC_BURN_OUT ) );
					CItemBase * pItemDef = Item_GetDef();
					ITEMID_TYPE id = (ITEMID_TYPE) pItemDef->m_ttEquippable.m_Light_Burnout.GetResIndex();
					if ( ! id )	// burn out and be gone.
					{
						return( false );
					}
					if ( id == GetID())
					{
						// It really has infinite charges I guess.
						m_itLight.m_charges = USHRT_MAX;
					}
					else
					{
						// Transform to the next shape.
						m_itLight.m_charges = 0;
						SetID(id);

						if ( IsAttr(ATTR_DECAY) )
							SetTimeout( GetDecayTime() );
					}
					Update();
				}
			}
			return true;

		case IT_SHIP_PLANK:
			{
				EXC_SET("default behaviour::IT_SHIP_PLANK");
				Ship_Plank( false );
			}
			return true;

		case IT_EXPLOSION:
			{
				EXC_SET("default behaviour::IT_EXPLOSION");
				if ( OnExplosion())
					break;
			}
			return true;

		case IT_TRAP_ACTIVE:
			{
				EXC_SET("default behaviour::IT_TRAP_ACTIVE");
				SetTrapState( IT_TRAP_INACTIVE, m_itTrap.m_AnimID, m_itTrap.m_wAnimSec );
			}
			return( true );

		case IT_TRAP_INACTIVE:
			{
				EXC_SET("default behaviour::IT_TRAP_INACTIVE");
				// Set inactive til someone triggers it again.
				if ( m_itTrap.m_fPeriodic )
				{
					SetTrapState( IT_TRAP_ACTIVE, m_itTrap.m_AnimID, m_itTrap.m_wResetSec );
				}
				else
				{
					SetTrapState( IT_TRAP, GetDispID(), -1 );
				}
			}
			return true;

		case IT_ANIM_ACTIVE:
			{
				// reset the anim
				EXC_SET("default behaviour::IT_ANIM_ACTIVE");
				SetDispID( m_itAnim.m_PrevID );
				m_type = m_itAnim.m_PrevType;
				SetTimeout( -1 );
				Update();
			}
			return true;

		case IT_DOOR:
		case IT_DOOR_OPEN:
		case IT_DOOR_LOCKED:	// Temporarily opened locked door.
			{
				// Doors should close.
				EXC_SET("default behaviour::IT_DOOR");
				Use_Door( false );
			}
			return true;

		case IT_WAND:
			{
				// Magic devices.
				// Will regen over time ??
				EXC_SET("default behaviour::IT_WAND");
			}
			break;

		case IT_POTION:
			{
				EXC_SET("default behaviour::IT_POTION");
				// This is a explode potion ?
				if (( RES_GET_INDEX(m_itPotion.m_Type) == SPELL_Explosion ) && m_itPotion.m_ignited )
				{
					// count down the timer. ??
					if ( m_itPotion.m_tick <= 1 )
					{
						// Set it off.
						OnTakeDamage( 1, m_uidLink.CharFind(), DAMAGE_GENERAL | DAMAGE_FIRE );
					}
					else
					{
						m_itPotion.m_tick --;
						TCHAR *pszMsg = Str_GetTemp();
						CObjBase* pObj = STATIC_CAST <CObjBase*>(GetTopLevelObj());
						ASSERT(pObj);
						pObj->Speak(ITOA(m_itPotion.m_tick, pszMsg, 10));
						SetTimeout( TICK_PER_SEC );
					}
					return true;
				}
			}
			break;

		case IT_SPAWN_CHAR:	// Spawn a creature if we are under count.
		case IT_SPAWN_ITEM:	// Spawn an item.
			{
				EXC_SET("default behaviour::IT_SPAWN");
				Spawn_OnTick( true );
			}
			return true;

		case IT_CROPS:
		case IT_FOLIAGE:
			{
				EXC_SET("default behaviour::IT_CROPS");
				if ( Plant_OnTick())
					return true;
			}
			break;

		case IT_BEE_HIVE:
			{
				EXC_SET("default behaviour::IT_BEE_HIVE");
				// Regenerate honey count
				if ( m_itBeeHive.m_honeycount < 5 )
					m_itBeeHive.m_honeycount++;
				SetTimeout( 15*60*TICK_PER_SEC );
			}
			return true;

		case IT_CAMPFIRE:
			{
				EXC_SET("default behaviour::IT_CAMPFIRE");
				if ( GetID() == ITEMID_EMBERS )
					break;
				SetID( ITEMID_EMBERS );
				SetDecayTime( 2*60*TICK_PER_SEC );
				Update();
			}
			return true;

		case IT_SIGN_GUMP:	// signs never decay
			{
				EXC_SET("default behaviour::IT_SIGN_GUMP");
			}
			return true;
	}

	EXC_SET("default behaviour2");
	if ( IsAttr(ATTR_DECAY) )
		return false;

	EXC_SET("default behaviour3");
	if ( iRet == TRIGRET_RET_FALSE )
		return false;

	EXC_SET("default behaviour4");
	DEBUG_ERR(( "Timer expired without DECAY flag '%s' (UID=%x)?\n", GetName(), (DWORD)GetUID()));
	
#ifndef _WIN32
	}
#ifndef _DEBUG
	catch ( CGrayError &e )
	{
		EXC_CATCH_EXCEPTION(&e);
		g_Log.EventError("'%s' item [0%lx] - CGrayError\n", GetName(), (DWORD)GetUID());
	}
	catch (...)
	{
		EXC_CATCH_EXCEPTION(NULL);
		g_Log.EventError("'%s' item [0%lx] - ...\n", GetName(), (DWORD)GetUID());
	}
#endif
#else
	EXC_CATCH;
	
	EXC_DEBUG_START;
	g_Log.EventDebug("'%s' item [0%lx]\n", GetName(), (DWORD)GetUID());
	//g_Log.EventError("'%s' item [0%lx]\n", GetName(), GetUID());
	EXC_DEBUG_END;
#endif

	return( true );
}

int CItem::GetAbilityFlags() const
{
	CItemBase * pItemBase = Item_GetDef();
	return pItemBase->m_Can;
}