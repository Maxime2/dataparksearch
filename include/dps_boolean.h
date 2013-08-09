/* Copyright (C) 2003-2007 Datapark corp. All rights reserved.
   Copyright (C) 2000-2002 Lavtech.com corp. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA 
*/

#ifndef DPS_BOOLEAN_H
#define DPS_BOOLEAN_H

/* Boolean search constants and types */
enum {
  DPS_MAXSTACK	         = 128,
  DPS_STACK_LEFT	 = 1,
  DPS_STACK_RIGHT	 = 2,
  DPS_STACK_PHRASE_LEFT  = 3,
  DPS_STACK_PHRASE_RIGHT = 4,
  DPS_STACK_BOT	         = 0,
  DPS_STACK_OR	         = 5,
  DPS_STACK_AND	         = 6,
  DPS_STACK_NEAR         = 7,
  DPS_STACK_ANYWORD      = 8,
  DPS_STACK_NOT	         = 9,
  DPS_STACK_ERR          = -8,
  DPS_STACK_WORD	 = 200,
  DPS_STACK_WORD_NOT     = 256
};

extern int DpsCalcBoolItems(DPS_AGENT *Agent, DPS_RESULT *Res);
extern void DpsWWLBoolItems(DPS_RESULT *Res);
extern DPS_BOOLSTACK *DpsBoolStackInit(DPS_BOOLSTACK *s);
extern void DpsBoolStackFree(DPS_BOOLSTACK *s);
extern int DpsAddStackItem(DPS_AGENT *query, DPS_RESULT *Res, DPS_PREPARE_STATE *state, char *word, dpsunicode_t *uword);
extern void DpsStackItemFree(DPS_STACK_ITEM *item);

#endif
