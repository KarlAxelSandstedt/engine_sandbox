/*
==========================================================================
    Copyright (C) 2025,2026 Axel Sandstedt 

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
==========================================================================
*/

#ifndef __UI_LOCAL_H__
#define __UI_LOCAL_H__

#include "ui_public.h"
#include "cmd.h"

/* UI CMDS */
void 	timeline_drag(void);
void	ui_text_input_mode_enable(void);
void	ui_text_input_mode_disable(void);
void 	ui_text_input_flush(void);
void 	ui_text_op(void);

void 		ui_popup_build(void);
extern u32	cmd_ui_popup_build;

/* internal */
struct ui_text_input *text_edit_stub_ptr(void);

#endif
