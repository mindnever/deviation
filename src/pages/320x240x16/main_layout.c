/*
 This project is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 Deviation is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with Deviation.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "common.h"
#include "pages.h"
#include "icons.h"
#include "gui/gui.h"
#include "config/model.h"
#include "telemetry.h"

#define gui (&gui_objs.u.mainlayout)
#define pc Model.pagecfg2


static void draw_elements();
static const char *boxlabel_cb(guiObject_t *obj, const void *data);
static const char *newelem_cb(guiObject_t *obj, int dir, void *data);
static void add_dlg_cb(guiObject_t *obj, const void *data);
static void cfg_cb(guiObject_t *obj, const void *data);
static void newelem_press_cb(guiObject_t *obj, const void *data);
static const char *xpos_cb(guiObject_t *obj, int dir, void *data);
static const char *ypos_cb(guiObject_t *obj, int dir, void *data);
static void notify_cb(guiObject_t *obj);
static void move_elem();
static void select_for_move(guiLabel_t *obj);
static u8 _action_cb(u32 button, u8 flags, void *data);
static const char *dlgbut_str_cb(guiObject_t *obj, const void *data);
static void dlgbut_cb(struct guiObject *obj, const void *data);


struct buttonAction action;
u8 cfg_elem_type;

#define HEADER_Y 32

#include "../common/_main_layout.c"

void PAGE_MainLayoutInit(int page)
{
     (void)page;
    PAGE_SetModal(0);
    BUTTON_RegisterCallback(&action,
          CHAN_ButtonMask(BUT_ENTER)
          | CHAN_ButtonMask(BUT_EXIT)
          | CHAN_ButtonMask(BUT_LEFT)
          | CHAN_ButtonMask(BUT_LEFT)
          | CHAN_ButtonMask(BUT_RIGHT)
          | CHAN_ButtonMask(BUT_UP)
          | CHAN_ButtonMask(BUT_DOWN),
          BUTTON_PRESS | BUTTON_LONGPRESS | BUTTON_PRIORITY, _action_cb, NULL);
     if (Model.mixer_mode == MIXER_STANDARD)
         PAGE_ShowHeader_ExitOnly(NULL, MODELMENU_Show);
     else
         PAGE_ShowHeader(NULL);
    long_press = 0;
    newelem = 0;
    selected_x = 0;
    const u16 color[5] = {
        RGB888_to_RGB565(0xaa, 0x44, 0x44),
        RGB888_to_RGB565(0x44, 0xaa, 0x44),
        RGB888_to_RGB565(0x44, 0x44, 0xaa),
        RGB888_to_RGB565(0x44, 0x44, 0x44),
        RGB888_to_RGB565(0x33, 0x33, 0x33),
        };
    for (int i = 0 ; i < 5; i++)
        gui->desc[i] = (struct LabelDesc){
            .font = 0,
            .font_color = 0xffff,
            .fill_color = color[i],
            .outline_color = 0,
            .style = LABEL_FILL};
    gui->desc[1].font = TINY_FONT.font; //Special case for trims
    GUI_CreateIcon(&gui->newelem, 32, 0, &icons[ICON_PLUS], add_dlg_cb, NULL);
    GUI_CreateIcon(&gui->editelem, 64, 0, &icons[ICON_OPTIONS], cfg_cb, NULL);
    GUI_SetHidden((guiObject_t *)&gui->editelem, 1);
    //GUI_CreateTextSelect(&gui->newelem, 36, 12, TEXTSELECT_96, newelem_press_cb, newelem_cb, NULL);
    GUI_CreateLabel(&gui->xlbl, 80+18, 10, NULL, TITLE_FONT, "X");
    GUI_CreateTextSelect(&gui->x, 88+18, 10, TEXTSELECT_64, NULL, xpos_cb, NULL);
    GUI_CreateLabel(&gui->ylbl, 164+16, 10, NULL, TITLE_FONT, "Y");
    GUI_CreateTextSelect(&gui->y, 172+16, 10, TEXTSELECT_64, NULL, ypos_cb, NULL);

    GUI_SelectionNotify(notify_cb);
    draw_elements();
}
void PAGE_MainLayoutEvent()
{
}
void PAGE_MainLayoutExit()
{
    GUI_SelectionNotify(NULL);
    BUTTON_UnregisterCallback(&action);
}
void PAGE_MainLayoutRestoreDialog(int idx)
{
    GUI_RemoveAllObjects();
    PAGE_MainLayoutInit(0);
    selected_for_move = &gui->elem[idx];
    show_config();
}

void set_selected_for_move(guiLabel_t * obj)
{
    selected_for_move = obj;
    int state = obj ? 1 : 0;
    GUI_SetHidden((guiObject_t *)&gui->editelem, !state);
    GUI_TextSelectEnable(&gui->x, state);
    GUI_TextSelectEnable(&gui->y, state);
}
const char *xpos_cb(guiObject_t *obj, int dir, void *data)
{
    (void)obj;
    (void)data;
    if (selected_for_move) {
        int x = GUI_TextSelectHelper(selected_x, 0, LCD_WIDTH-selected_w, dir, 1, 10, NULL);
        if (x != selected_x) {
            selected_x = x;
            move_elem();
        }
    }
    sprintf(tmp, "%d", selected_x);
    return tmp;
}
const char *ypos_cb(guiObject_t *obj, int dir, void *data)
{
    (void)obj;
    (void)data;
    if (selected_for_move) {
        int y = GUI_TextSelectHelper(selected_y, HEADER_Y, LCD_HEIGHT-selected_h, dir, 1, 10, NULL);
        if (y != selected_y) {
            selected_y = y;
            move_elem();
        }
    }
    sprintf(tmp, "%d", selected_y);
    return tmp;
}

void select_for_move(guiLabel_t *obj)
{
    GUI_SetSelected((guiObject_t *)obj);
    notify_cb((guiObject_t *)obj);
    if (selected_for_move == obj)
        return;
    if (selected_for_move) {
        selected_for_move->desc.font_color ^= 0xffff;
        selected_for_move->desc.fill_color ^= 0xffff;
        GUI_Redraw((guiObject_t *)selected_for_move);
    }
    set_selected_for_move(obj);
    selected_for_move->desc.font_color ^= 0xffff;
    selected_for_move->desc.fill_color ^= 0xffff;
}

void newelem_press_cb(guiObject_t *obj, const void *data)
{
    (void)obj;
    (void)data;
    int i = create_element();
    if (i >= 0) {
        draw_elements();
        select_for_move(&gui->elem[i]);
    }
}

static void dialog_ok_cb(u8 state, void * data)
{
    (void)state;
    (void)data;
    guiObject_t *obj = (guiObject_t *)selected_for_move;
    draw_elements();
    if(obj && OBJ_IS_USED(obj))
        select_for_move((guiLabel_t *)obj);
}

#define ADD_DIALOG_W 268
#define ADD_DIALOG_H 130
static void add_dlg_cb(guiObject_t *obj, const void *data)
{
    (void)obj;
    (void)data;
    GUI_CreateDialog(&gui->dialog,
        (LCD_WIDTH - ADD_DIALOG_W) / 2,
        (LCD_HEIGHT - HEADER_Y - ADD_DIALOG_H) / 2 + HEADER_Y,
        ADD_DIALOG_W,
        ADD_DIALOG_H,
        _tr("Page Config"), NULL, dialog_ok_cb, dtCancel, "");
    GUI_CreateLabel(&gui->dlglbl[0],
        (LCD_WIDTH - ADD_DIALOG_W) / 2 + 10,
        (LCD_HEIGHT - HEADER_Y - ADD_DIALOG_H) / 2 + HEADER_Y + 30,
        NULL, DEFAULT_FONT, _tr("Type"));
    GUI_CreateTextSelect(&gui->dlgts[0],
        (LCD_WIDTH - ADD_DIALOG_W) / 2 + ADD_DIALOG_W - 10 - 128 - 68,
        (LCD_HEIGHT - HEADER_Y - ADD_DIALOG_H) / 2 + HEADER_Y + 30,
        TEXTSELECT_128, NULL, newelem_cb, NULL);
    GUI_CreateButton(&gui->dlgbut[0],
        (LCD_WIDTH - ADD_DIALOG_W) / 2 + ADD_DIALOG_W - 10 - 64,
        (LCD_HEIGHT - HEADER_Y - ADD_DIALOG_H) / 2 + HEADER_Y + 30,
        BUTTON_64x16, add_dlgbut_str_cb, 0, newelem_press_cb, (void *)1L);

    GUI_CreateLabel(&gui->dlglbl[1],
        (LCD_WIDTH - ADD_DIALOG_W) / 2 + 10,
        (LCD_HEIGHT - HEADER_Y - ADD_DIALOG_H) / 2 + HEADER_Y + 70,
        NULL, DEFAULT_FONT, _tr("Template"));
    GUI_CreateButton(&gui->dlgbut[1],
        (LCD_WIDTH - ADD_DIALOG_W) / 2 + ADD_DIALOG_W - 10 - 112 - 68,
        (LCD_HEIGHT - HEADER_Y - ADD_DIALOG_H) / 2 + HEADER_Y + 70,
        BUTTON_96x16, add_dlgbut_str_cb, 0, add_dlgbut_cb, (void *)0L);
    GUI_SetSelected((guiObject_t *)&gui->dlgbut[0]);
}

static guiObject_t *getobj_cb(int relrow, int col, void *data)
{
    (void)data;
    if (OBJ_IS_USED(&gui->dlgbut2[0]))
        col = (3 + col) % 3;
    else 
        col = (2 + col) % 2;
    if (col == 0)
        return (guiObject_t *)&gui->dlgts[relrow];
    if (col == 1 && OBJ_IS_USED(&gui->dlgbut2[0])) 
        return (guiObject_t *)&gui->dlgbut2[relrow];
    return (guiObject_t *)&gui->dlgbut[relrow];
}

const char *dlgbut_str_cb(guiObject_t *obj, const void *data)
{
    (void)data;
    return ((guiButton_t *)obj >= gui->dlgbut2 && (guiButton_t *)obj < gui->dlgbut2 + LAYDLG_NUM_ITEMS) ? _tr("Edit") : _tr("Delete");
}

static void toggle_press_cb(guiObject_t *obj, const void *data)
{
    PAGE_MainLayoutExit();
    TGLICO_Select(obj, data);
}

#define LAYDLG_X_SPACE 10
#define LAYDLG_X (LCD_WIDTH - LAYDLG_MIN_WIDTH) / 2
#define LAYDLG_Y (32 + LAYDLG_Y_SPACE)
#define LAYDLG_MIN_WIDTH (2*LAYDLG_X_SPACE + 15 + 100 + 64 +20) //space + # + spinbox + button + scrollbar
#define LAYDLG_SCROLLABLE_X LAYDLG_X_SPACE
static int row_cb(int absrow, int relrow, int y, void *data)
{
    int type = (long)data;
    long elemidx = elem_rel_to_abs(type, absrow);
    int X = LAYDLG_X + LAYDLG_SCROLLABLE_X - (type == ELEM_TOGGLE ? 68/2 : 0);
    int del_x = X + 15 + 100;
    int num_objs = 2;
    if (type == ELEM_MODELICO) {
        GUI_CreateLabelBox(&gui->dlglbl[relrow], X, y, 115, LAYDLG_TEXT_HEIGHT, &DEFAULT_FONT, NULL, NULL, _tr("Model"));
    } else {
        if (type == ELEM_TOGGLE) {
            GUI_CreateButton(&gui->dlgbut2[relrow], del_x, y, BUTTON_64x16, dlgbut_str_cb, 0, toggle_press_cb, (void *)elemidx);
            del_x = X + 15 + 168;
            num_objs++;
        }
        GUI_CreateLabelBox(&gui->dlglbl[relrow], X, y, 10, 16, &DEFAULT_FONT, label_cb, NULL, (void *)(long)(absrow));
        GUI_CreateTextSelect(&gui->dlgts[relrow], X + 15, y, TEXTSELECT_96, NULL, dlgts_cb, (void *)elemidx);
    }
    GUI_CreateButton(&gui->dlgbut[relrow], del_x, y, BUTTON_64x16, dlgbut_str_cb, 0, dlgbut_cb, (void *)elemidx);
    return num_objs;
}

static void cfg_cb(guiObject_t *obj, const void *data)
{
    (void)obj;
    (void)data;
    show_config();
}

static void show_config()
{
    int count = 0;
    int row_idx = 0;
    long type;
    if (OBJ_IS_USED(&gui->dialog))
        GUI_RemoveHierObjects((guiObject_t *)&gui->dialog);
    if(selected_for_move) {
        int selected_idx = guielem_idx((guiObject_t *)selected_for_move);
        type = ELEM_TYPE(pc.elem[selected_idx]);
        row_idx = elem_abs_to_rel(selected_idx);
        count = elem_get_count(type);
    }
    if (! count) {
        dialog_ok_cb(1, NULL);
        return;
    }
    int x = LAYDLG_X - (type == ELEM_TOGGLE ? 68/2 : 0);
    int width = LAYDLG_MIN_WIDTH + (type == ELEM_TOGGLE ? 64 : 0);;
    GUI_CreateDialog(&gui->dialog,
         x, LAYDLG_Y,
         width, LAYDLG_HEIGHT,
         _tr("Page Config"), NULL, dialog_ok_cb, dtOk, "");
    GUI_CreateScrollable(&gui->scrollable,
         x + LAYDLG_SCROLLABLE_X, LAYDLG_Y + LAYDLG_SCROLLABLE_Y,
         width - 2 * LAYDLG_SCROLLABLE_X, LAYDLG_HEIGHT - 2 * LAYDLG_SCROLLABLE_Y,
         LAYDLG_TEXT_HEIGHT, count, row_cb, getobj_cb, NULL, (void *)type);
    GUI_SetSelected(GUI_ShowScrollableRowCol(&gui->scrollable, row_idx, 0));
}
    
static u8 _action_cb(u32 button, u8 flags, void *data)
{
    (void)data;

    if(! GUI_GetSelected() || ! selected_for_move || GUI_IsModal())
        return 0;
    if(CHAN_ButtonIsPressed(button, BUT_EXIT)) {
        selected_for_move->desc.font_color ^= 0xffff;
        selected_for_move->desc.fill_color ^= 0xffff;
        GUI_Redraw((guiObject_t *)selected_for_move);
        set_selected_for_move(NULL);
        return 1;
    }
    if(CHAN_ButtonIsPressed(button, BUT_ENTER)) {
        show_config();
        return 1;
    }
    if(CHAN_ButtonIsPressed(button, BUT_LEFT)) {
        xpos_cb(NULL, (flags & BUTTON_LONGPRESS) ? -2 : -1, (void *)selected_for_move->cb_data);
        return 1;
    }
    if(CHAN_ButtonIsPressed(button, BUT_RIGHT)) {
        xpos_cb(NULL, (flags & BUTTON_LONGPRESS) ? 2 : 1, (void *)selected_for_move->cb_data);
        return 1;
    }
    if(CHAN_ButtonIsPressed(button, BUT_UP)) {
        ypos_cb(NULL, (flags & BUTTON_LONGPRESS) ? -2 : -1, (void *)selected_for_move->cb_data);
        return 1;
    }
    if(CHAN_ButtonIsPressed(button, BUT_DOWN)) {
        ypos_cb(NULL, (flags & BUTTON_LONGPRESS) ? 2 : 1, (void *)selected_for_move->cb_data);
        return 1;
    }
    return 0;
}