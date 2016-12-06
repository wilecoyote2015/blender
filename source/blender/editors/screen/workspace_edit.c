/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/screen/workspace_edit.c
 *  \ingroup edscr
 */

#include "BKE_context.h"
#include "BKE_main.h"
#include "BKE_library.h"
#include "BKE_screen.h"
#include "BKE_workspace.h"

#include "BLI_listbase.h"

#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "ED_screen.h"

#include "WM_api.h"
#include "WM_types.h"

#include "screen_intern.h"


/** \name Workspace API
 *
 * \brief API for managing workspaces and their data.
 * \{ */


/**
 * \brief Change the active workspace.
 *
 * Operator call, WM + Window + screen already existed before
 * Pretty similar to #ED_screen_set since changing workspace also changes screen.
 *
 * \warning Do NOT call in area/region queues!
 * \returns success.
 */
bool ED_workspace_change(bContext *C, wmWindow *win, WorkSpace *ws_new)
{
	Main *bmain = CTX_data_main(C);
	bScreen *screen_old = BKE_workspace_active_screen_get(win->workspace);
	bScreen *screen_new = BKE_workspace_active_screen_get(ws_new);

	if (!(screen_new = screen_set_ensure_valid(bmain, win, screen_new))) {
		return false;
	}

	if (screen_old != screen_new) {
		screen_set_prepare(C, win, screen_new, screen_old);
		win->workspace = ws_new;
		screen_set_refresh(bmain, C, win, screen_old->scene != screen_new->scene);
	}
	BLI_assert(CTX_wm_workspace(C) == ws_new);

	return true;
}

/**
 * Duplicate a workspace including its layouts.
 */
WorkSpace *ED_workspace_duplicate(WorkSpace *workspace_old, Main *bmain, wmWindow *win)
{
	WorkSpaceLayout *layout_active_old = BKE_workspace_active_layout_get(workspace_old);
	WorkSpace *worspace_new = BKE_libblock_alloc(bmain, ID_WS, workspace_old->id.name + 2);

	for (WorkSpaceLayout *layout_old = workspace_old->layouts.first; layout_old; layout_old = layout_old->next) {
		WorkSpaceLayout *layout_new = ED_workspace_layout_duplicate(worspace_new, layout_old, win);

		if (layout_active_old == layout_old) {
			bScreen *screen_new = BKE_workspace_layout_screen_get(layout_new);

			screen_new_activate_refresh(win, screen_new);
			BKE_workspace_active_layout_set(worspace_new, layout_new);
		}
	}

	return worspace_new;
}

/**
 * \return if succeeded.
 */
bool ED_workspace_delete(Main *bmain, bContext *C, wmWindow *win, WorkSpace *ws)
{
	if (BLI_listbase_is_single(&bmain->workspaces)) {
		return false;
	}

	if (win->workspace == ws) {
		WorkSpace *fallback_ws = ws->id.prev ? ws->id.prev : ws->id.next;
		ED_workspace_change(C, win, fallback_ws);
	}
	BKE_libblock_free(bmain, &ws->id);

	return true;
}

/** \} Workspace API */


/** \name Workspace Operators
 *
 * \{ */

static int workspace_new_exec(bContext *C, wmOperator *UNUSED(op))
{
	Main *bmain = CTX_data_main(C);
	wmWindow *win = CTX_wm_window(C);
	WorkSpace *workspace_old = win->workspace;
	WorkSpace *workspace;

	workspace = ED_workspace_duplicate(workspace_old, bmain, win);
	WM_event_add_notifier(C, NC_WORKSPACE | ND_WORKSPACE_SET, workspace);

	return OPERATOR_FINISHED;
}

static void WORKSPACE_OT_workspace_new(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "New Workspace";
	ot->description = "Add a new workspace";
	ot->idname = "WORKSPACE_OT_workspace_new";

	/* api callbacks */
	ot->exec = workspace_new_exec;
	ot->poll = WM_operator_winactive;
}

static int workspace_delete_exec(bContext *C, wmOperator *UNUSED(op))
{
	Main *bmain = CTX_data_main(C);
	wmWindow *win = CTX_wm_window(C);

	ED_workspace_delete(bmain, C, win, win->workspace);

	return OPERATOR_FINISHED;
}

static void WORKSPACE_OT_workspace_delete(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Delete Workspace";
	ot->description = "Delete the active workspace";
	ot->idname = "WORKSPACE_OT_workspace_delete";

	/* api callbacks */
	ot->exec = workspace_delete_exec;
}

void ED_operatortypes_workspace(void)
{
	WM_operatortype_append(WORKSPACE_OT_workspace_new);
	WM_operatortype_append(WORKSPACE_OT_workspace_delete);
}

/** \} Workspace Operators */
