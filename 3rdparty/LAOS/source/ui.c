/*
 * Copyright (C) 2012-2013 Gil Mendes
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/**
 * @file
 * @brief		User interface functions.
 *
 * @todo		Destroy windows.
 */

#include <lib/ctype.h>
#include <lib/string.h>
#include <lib/utility.h>

#include <assert.h>
#include <memory.h>
#include <time.h>
#include <ui.h>

struct ui_choice;
struct ui_textbox;

/** Details of a line in a text view. */
typedef struct ui_textview_line {
	const char *ptr;		/**< Pointer to the line. */
	size_t len;			/**< Length of the line. */
} ui_textview_line_t;

/** Structure containing a text view window. */
typedef struct ui_textview {
	ui_window_t header;		/**< Window header. */

	const char *buf;		/**< Buffer containing text. */
	size_t size;			/**< Size of the buffer. */

	/** Array containing details of each line. */
	struct {
		size_t start;		/**< Start of the line. */
		size_t len;		/**< Length of the line. */
	} *lines;

	size_t count;			/**< Number of lines. */
	size_t offset;			/**< Current offset. */
} ui_textview_t;

/** Structure containing a list window. */
typedef struct ui_list {
	ui_window_t header;		/**< Window header. */
	bool exitable;			/**< Whether the menu can be exited. */
	ui_entry_t **entries;		/**< Array of entries. */
	size_t count;			/**< Number of entries. */
	size_t offset;			/**< Offset of first entry displayed. */
	size_t selected;		/**< Index of selected entry. */
} ui_list_t;

/** Structure containing a link. */
typedef struct ui_link {
	ui_entry_t header;		/**< Entry header. */
	ui_window_t *window;		/**< Window that this links to. */
} ui_link_t;

/** Structure containing a checkbox. */
typedef struct ui_checkbox {
	ui_entry_t header;		/**< Entry header. */
	const char *label;		/**< Label for the checkbox. */
	value_t *value;			/**< Value of the checkbox. */
} ui_checkbox_t;

/** Structure containing a textbox. */
typedef struct ui_textbox {
	ui_entry_t header;		/**< Entry header. */
	const char *label;		/**< Label for the textbox. */
	value_t *value;			/**< Value of the textbox. */
} ui_textbox_t;

/** Structure containing a chooser. */
typedef struct ui_chooser {
	ui_entry_t header;		/**< Entry header. */
	const char *label;		/**< Label for the choice. */
	struct ui_choice *selected;	/**< Selected item. */
	value_t *value;			/**< Value to update. */
	ui_window_t *list;		/**< List implementing the chooser. */
} ui_chooser_t;

/** Structure containing an choice. */
typedef struct ui_choice {
	ui_entry_t header;		/**< Entry header. */
	ui_chooser_t *chooser;		/**< Chooser that the entry is for. */
	const char *label;		/**< Label for the choice. */
	value_t value;			/**< Value of the choice. */
} ui_choice_t;

/** State for the textbox editor. */
static ui_window_t *textbox_edit_window = NULL;
static char textbox_edit_buf[1024];
static size_t textbox_edit_len = 0;
static size_t textbox_edit_offset = 0;
static bool textbox_edit_update = false;

/** Size of the content region. */
#define UI_CONTENT_WIDTH	((size_t)main_console->width - 2)
#define UI_CONTENT_HEIGHT	((size_t)main_console->height - 4)

/** Set the region to the title region.
 * @param clear		Whether to clear the region. */
static inline void set_title_region(bool clear) {
	draw_region_t r = { 0, 0, main_console->width, 1, false };
	main_console->set_region(&r);
	if(clear)
		main_console->clear(0, 0, r.width, r.height);
}

/** Set the region to the help region.
 * @param clear		Whether to clear the region. */
static inline void set_help_region(bool clear) {
	draw_region_t r = { 0, main_console->height - 1, main_console->width, 1, false };
	main_console->set_region(&r);
	if(clear)
		main_console->clear(0, 0, r.width, r.height);
}

/** Set the region to the content region.
 * @param clear		Whether to clear the region. */
static inline void set_content_region(bool clear) {
	draw_region_t r = { 1, 2, main_console->width - 2, UI_CONTENT_HEIGHT, false };
	main_console->set_region(&r);
	if(clear)
		main_console->clear(0, 0, r.width, r.height);
}

/** Print an action (for help text).
 * @param action	Action to print. */
static void ui_action_print(ui_action_t *action) {
	if(!action->name)
		return;

	switch(action->key) {
	case CONSOLE_KEY_UP:
		kprintf("Up");
		break;
	case CONSOLE_KEY_DOWN:
		kprintf("Down");
		break;
	case CONSOLE_KEY_LEFT:
		kprintf("Left");
		break;
	case CONSOLE_KEY_RIGHT:
		kprintf("Right");
		break;
	case CONSOLE_KEY_F1:
		kprintf("F1");
		break;
	case CONSOLE_KEY_F2:
		kprintf("F2");
		break;
	case '\n':
		kprintf("Enter");
		break;
	case '\e':
		kprintf("Esc");
		break;
	default:
		kprintf("%c", action->key & 0xFF);
		break;
	}

	kprintf(" = %s  ", action->name);
}

/** Initialise a window structure.
 * @param window	Window to initialise.
 * @param type		Type of window.
 * @param title		Title of the window. */
void ui_window_init(ui_window_t *window, ui_window_type_t *type, const char *title) {
	window->type = type;
	window->title = title;
}

/** Render help text for a window.
 * @param window	Window to render help text for.
 * @param timeout	Seconds remaining. */
static void ui_window_render_help(ui_window_t *window, int seconds) {
	set_help_region(true);
	window->type->help(window);
	if(seconds > 0) {
		main_console->move_cursor(0 - ((seconds >= 10) ? 12 : 11), 0);
		kprintf("%d second(s)", seconds);
	}
	main_console->highlight(0, 0, main_console->width, 1);
}

/** Render the contents of a window.
 * @param window	Window to render.
 * @param timeout	Seconds remaining. */
static void ui_window_render(ui_window_t *window, int seconds) {
	main_console->reset();
	main_console->show_cursor(false);

	set_title_region(true);
	kprintf("%s", window->title);
	main_console->highlight(0, 0, main_console->width, 1);

	ui_window_render_help(window, seconds);

	set_content_region(true);
	window->type->render(window);

	if(window->type->place_cursor) {
		set_content_region(false);
		window->type->place_cursor(window);
		main_console->show_cursor(true);
	}
}

/** Update the window after completion of an action.
 * @param window	Window to update.
 * @param timeout	Seconds remaining. */
static void ui_window_update(ui_window_t *window, int seconds) {
	main_console->show_cursor(false);

	ui_window_render_help(window, seconds);

	if(window->type->place_cursor) {
		set_content_region(false);
		window->type->place_cursor(window);
		main_console->show_cursor(true);
	}
}

/** Display a window.
 * @param window	Window to display.
 * @param timeout	Seconds to wait before closing the window if no input.
 *			If 0, the window will not time out. */
void ui_window_display(ui_window_t *window, int timeout) {
	timeout_t us = timeout * 1000000;
	input_result_t result;
	uint16_t key;

	ui_window_render(window, timeout);

	while(true) {
		if(timeout > 0) {
			if(main_console->check_key()) {
				timeout = 0;
				continue;
			} else {
				spin(1000);
				us -= 1000;
				if(us <= 0)
					break;

				if((ROUND_UP(us, 1000000) / 1000000) < timeout) {
					timeout--;
					ui_window_update(window, timeout);
				}
			}
		} else {
			key = main_console->get_key();
			set_content_region(false);

			result = window->type->input(window, key);
			if(result == INPUT_CLOSE) {
				break;
			} else if(result == INPUT_RENDER) {
				ui_window_render(window, timeout);
			} else {
				/* Need to re-render help text each key press,
				 * for example if the action moved to a list
				 * entry with different actions. */
				ui_window_update(window, timeout);
			}
		}
	}

	main_console->reset();
}

/** Print a line from a text view.
 * @param view		View to render.
 * @param line		Index of line to print. */
static void ui_textview_render_line(ui_textview_t *view, size_t line) {
	size_t i;

	for(i = 0; i < view->lines[line].len; i++)
		main_console->putch(view->buf[(view->lines[line].start + i) % view->size]);

	if(view->lines[line].len < UI_CONTENT_WIDTH)
		main_console->putch('\n');
}

/** Render a textview window.
 * @param window	Window to render. */
static void ui_textview_render(ui_window_t *window) {
	ui_textview_t *view = (ui_textview_t *)window;
	size_t i;

	for(i = view->offset; i < MIN(view->offset + UI_CONTENT_HEIGHT, view->count); i++)
		ui_textview_render_line(view, i);
}

/** Write the help text for a textview window.
 * @param window	Window to write for. */
static void ui_textview_help(ui_window_t *window) {
	ui_textview_t *view = (ui_textview_t *)window;

	if(view->offset)
		kprintf("Up = Scroll Up  ");

	if((view->count - view->offset) > UI_CONTENT_HEIGHT)
		kprintf("Down = Scroll Down  ");

	kprintf("Esc = Back");
}

/** Handle input on the window.
 * @param window	Window input was performed on.
 * @param key		Key that was pressed.
 * @return		Input handling result. */
static input_result_t ui_textview_input(ui_window_t *window, uint16_t key) {
	ui_textview_t *view = (ui_textview_t *)window;
	input_result_t ret = INPUT_HANDLED;

	switch(key) {
	case CONSOLE_KEY_UP:
		if(view->offset) {
			main_console->scroll_up();
			ui_textview_render_line(view, --view->offset);
		}
		break;
	case CONSOLE_KEY_DOWN:
		if((view->count - view->offset) > UI_CONTENT_HEIGHT) {
			main_console->scroll_down();
			main_console->move_cursor(0, -1);
			ui_textview_render_line(view, view->offset++ + UI_CONTENT_HEIGHT);
		}
		break;
	case '\e':
		ret = INPUT_CLOSE;
		break;
	}

	return ret;
}

/** Text view window type. */
static ui_window_type_t ui_textview_window_type = {
	.render = ui_textview_render,
	.help = ui_textview_help,
	.input = ui_textview_input,
};

/** Add a line to a text view.
 * @param view		View to add to.
 * @param start		Start offset of the line.
 * @param len		Length of line. */
static void ui_textview_add_line(ui_textview_t *view, size_t start, size_t len) {
	/* If the line is larger than the content width, split it. */
	if(len > UI_CONTENT_WIDTH) {
		ui_textview_add_line(view, start, UI_CONTENT_WIDTH);
		ui_textview_add_line(view, (start + UI_CONTENT_WIDTH) % view->size,
			len - UI_CONTENT_WIDTH);
	} else {
		view->lines = krealloc(view->lines, sizeof(*view->lines) * (view->count + 1));
		view->lines[view->count].start = start;
		view->lines[view->count++].len = len;
	}
}

/** Create a text view window.
 * @param title		Title for the window.
 * @param buf		Circular buffer containing text to display.
 * @param size		Total size of the buffer.
 * @param start		Start character of the buffer.
 * @param length	Length of the data from the start (will wrap around).
 * @return		Pointer to created window. */
ui_window_t *ui_textview_create(const char *title, const char *buf, size_t size,
	size_t start, size_t length)
{
	ui_textview_t *view = kmalloc(sizeof(ui_textview_t));
	size_t i, line_start, line_len;

	ui_window_init(&view->header, &ui_textview_window_type, title);
	view->buf = buf;
	view->size = size;
	view->lines = NULL;
	view->offset = 0;
	view->count = 0;

	/* Store details of all the lines in the buffer. */
	line_start = start;
	line_len = 0;
	for(i = 0; i < length; i++) {
		if(view->buf[(start + i) % view->size] == '\n') {
			ui_textview_add_line(view, line_start, line_len);
			line_start = (start + i + 1) % view->size;
			line_len = 0;
		} else {
			line_len++;
		}
	}

	/* If there is still data at the end (no newline before end), add it. */
	if(line_len)
		ui_textview_add_line(view, line_start, line_len);
	
	return &view->header;
}

/** Render an entry from a list.
 * @note		Current draw region should be the content region.
 * @param entry		Entry to render.
 * @param pos		Position to render at.
 * @param selected	Whether to highlight. */
static void ui_list_render_entry(ui_entry_t *entry, size_t pos, bool selected) {
	draw_region_t region, content;

	/* Work out where to put the entry. */
	main_console->get_region(&content);
	region.x = content.x;
	region.y = content.y + pos;
	region.width = content.width;
	region.height = 1;
	region.scrollable = false;
	main_console->set_region(&region);

	/* Render the entry. */
	entry->type->render(entry);

	/* Highlight if necessary. */
	if(selected)
		main_console->highlight(0, 0, region.width, 1);

	/* Restore content region. */
	main_console->set_region(&content);
}

/** Render a list window.
 * @param window	Window to render. */
static void ui_list_render(ui_window_t *window) {
	ui_list_t *list = (ui_list_t *)window;
	size_t i;

	/* Render each entry. */
	for(i = list->offset; i < MIN(list->offset + UI_CONTENT_HEIGHT, list->count); i++)
		ui_list_render_entry(list->entries[i], i - list->offset, list->selected == i);
}

/** Write the help text for a list window.
 * @param window	Window to write for. */
static void ui_list_help(ui_window_t *window) {
	ui_list_t *list = (ui_list_t *)window;
	size_t i;

	if(list->count) {
		/* Print help for each of the selected entry's actions. */
		for(i = 0; i < list->entries[list->selected]->type->action_count; i++)
			ui_action_print(&list->entries[list->selected]->type->actions[i]);

		if(list->selected > 0)
			kprintf("Up = Scroll Up  ");
		if(list->selected < (list->count - 1))
			kprintf("Down = Scroll Down  ");
	}

	if(list->exitable)
		kprintf("Esc = Back");
}

/** Handle input on the window.
 * @param window	Window input was performed on.
 * @param key		Key that was pressed.
 * @return		Input handling result. */
static input_result_t ui_list_input(ui_window_t *window, uint16_t key) {
	ui_list_t *list = (ui_list_t *)window;
	input_result_t ret = INPUT_HANDLED;
	size_t i;

	switch(key) {
	case CONSOLE_KEY_UP:
		if(!list->selected)
			break;

		/* Un-highlight current entry. */
		main_console->highlight(0, list->selected - list->offset, UI_CONTENT_WIDTH, 1);

		/* If selected becomes less than the offset, must scroll up. */
		if(--list->selected < list->offset) {
			list->offset--;
			main_console->scroll_up();
			ui_list_render_entry(list->entries[list->selected], 0, true);
		} else {
			/* Highlight new entry. */
			main_console->highlight(0, list->selected - list->offset, UI_CONTENT_WIDTH, 1);
		}
		break;
	case CONSOLE_KEY_DOWN:
		if(list->selected >= (list->count - 1))
			break;

		/* Un-highlight current entry. */
		main_console->highlight(0, list->selected - list->offset, UI_CONTENT_WIDTH, 1);

		/* If selected is now off screen, must scroll down. */
		if(++list->selected >= list->offset + UI_CONTENT_HEIGHT) {
			list->offset++;
			main_console->scroll_down();
			ui_list_render_entry(list->entries[list->selected], UI_CONTENT_HEIGHT - 1, true);
		} else {
			/* Highlight new entry. */
			main_console->highlight(0, list->selected - list->offset, UI_CONTENT_WIDTH, 1);
		}
		break;
	case '\e':
		if(list->exitable)
			ret = INPUT_CLOSE;
		break;
	default:
		/* Handle custom actions. */
		for(i = 0; i < list->entries[list->selected]->type->action_count; i++) {
			if(key != list->entries[list->selected]->type->actions[i].key)
				continue;

			ret = list->entries[list->selected]->type->actions[i].cb(
				list->entries[list->selected]);
			if(ret == INPUT_HANDLED) {
				/* Need to re-render the entry. */
				main_console->highlight(0, list->selected - list->offset,
					UI_CONTENT_WIDTH, 1);
				ui_list_render_entry(list->entries[list->selected],
					list->selected - list->offset, true);
			}
			break;
		}
		break;
	}
	return ret;
}

/** List window type. */
static ui_window_type_t ui_list_window_type = {
	.render = ui_list_render,
	.help = ui_list_help,
	.input = ui_list_input,
};

/** Create a list window.
 * @param title		Title for the window.
 * @param exitable	Whether the window can be exited.
 * @return		Pointer to created window. */
ui_window_t *ui_list_create(const char *title, bool exitable) {
	ui_list_t *list = kmalloc(sizeof(ui_list_t));

	ui_window_init(&list->header, &ui_list_window_type, title);
	list->exitable = exitable;
	list->entries = NULL;
	list->count = 0;
	list->offset = 0;
	list->selected = 0;
	return &list->header;
}

/** Insert an entry into a list window.
 * @param window	Window to insert into.
 * @param entry		Entry to insert.
 * @param selected	Whether the entry should be selected. */
void ui_list_insert(ui_window_t *window, ui_entry_t *entry, bool selected) {
	ui_list_t *list = (ui_list_t *)window;
	size_t i = list->count++;

	list->entries = krealloc(list->entries, sizeof(ui_entry_t *) * list->count);
	list->entries[i] = entry;
	if(selected) {
		list->selected = i;
		if(i >= UI_CONTENT_HEIGHT)
			list->offset = (i - UI_CONTENT_HEIGHT) + 1;
	}
}

/** Return whether a list is empty.
 * @param window	Window to check.
 * @return		Whether the list is empty. */
bool ui_list_empty(ui_window_t *window) {
	ui_list_t *list = (ui_list_t *)window;
	return (list->count == 0);
}

/** Initialise a list entry structure.
 * @param entry		Entry structure to initialise.
 * @param type		Type of the entry. */
void ui_entry_init(ui_entry_t *entry, ui_entry_type_t *type) {
	entry->type = type;
}

/** Select a link.
 * @param entry		Entry to select.
 * @return		Input handling result. */
static input_result_t ui_link_select(ui_entry_t *entry) {
	ui_link_t *link = (ui_link_t *)entry;
	ui_window_display(link->window, 0);
	return INPUT_RENDER;
}

/** Actions for a link. */
static ui_action_t ui_link_actions[] = {
	{ "Select", '\n', ui_link_select },
};

/** Render a link.
 * @param entry		Entry to render. */
static void ui_link_render(ui_entry_t *entry) {
	ui_link_t *link = (ui_link_t *)entry;

	kprintf("%s", link->window->title);
	main_console->move_cursor(-2, 0);
	kprintf("->");
}

/** Link entry type. */
static ui_entry_type_t ui_link_entry_type = {
	.actions = ui_link_actions,
	.action_count = ARRAY_SIZE(ui_link_actions),
	.render = ui_link_render,
};

/** Create an entry which opens another window.
 * @param window	Window that the entry should open.
 * @return		Pointer to entry. */
ui_entry_t *ui_link_create(ui_window_t *window) {
	ui_link_t *link = kmalloc(sizeof(ui_link_t));

	ui_entry_init(&link->header, &ui_link_entry_type);
	link->window = window;
	return &link->header;
}

/** Toggle the value of a checkbox.
 * @param entry		Entry to toggle.
 * @return		Input handling result. */
static input_result_t ui_checkbox_toggle(ui_entry_t *entry) {
	ui_checkbox_t *box = (ui_checkbox_t *)entry;

	box->value->boolean = !box->value->boolean;
	return INPUT_HANDLED;
}

/** Actions for a check box. */
static ui_action_t ui_checkbox_actions[] = {
	{ "Toggle", '\n', ui_checkbox_toggle },
	{ NULL, ' ', ui_checkbox_toggle },
};

/** Render a check box.
 * @param entry		Entry to render. */
static void ui_checkbox_render(ui_entry_t *entry) {
	ui_checkbox_t *box = (ui_checkbox_t *)entry;

	kprintf("%s", box->label);
	main_console->move_cursor(-3, 0);
	kprintf("[%c]", (box->value->boolean) ? 'x' : ' ');
}

/** Check box entry type. */
static ui_entry_type_t ui_checkbox_entry_type = {
	.actions = ui_checkbox_actions,
	.action_count = ARRAY_SIZE(ui_checkbox_actions),
	.render = ui_checkbox_render,
};

/** Create a checkbox entry.
 * @param label		Label for the checkbox.
 * @param value		Value to store state in (should be VALUE_TYPE_BOOLEAN).
 * @return		Pointer to created entry. */
ui_entry_t *ui_checkbox_create(const char *label, value_t *value) {
	ui_checkbox_t *box = kmalloc(sizeof(ui_checkbox_t));

	assert(value->type == VALUE_TYPE_BOOLEAN);

	ui_entry_init(&box->header, &ui_checkbox_entry_type);
	box->label = label;
	box->value = value;
	return &box->header;
}

/** Render a text box edit window.
 * @param window	Window to render. */
static void ui_textbox_editor_render(ui_window_t *window) {
	size_t i;

	for(i = 0; i < textbox_edit_len; i++)
		main_console->putch(textbox_edit_buf[i]);
}

/** Write the help text for a text box edit window.
 * @param window	Window to write for. */
static void ui_textbox_editor_help(ui_window_t *window) {
	kprintf("Enter = Update  Esc = Cancel");
}

/** Handle input on a text box edit window.
 * @param window	Window input was performed on.
 * @param key		Key that was pressed.
 * @return		Input handling result. */
static input_result_t ui_textbox_editor_input(ui_window_t *window, uint16_t key) {
	char ch;

	switch(key) {
	case '\n':
		textbox_edit_update = true;
	case '\e':
		return INPUT_CLOSE;
	case CONSOLE_KEY_LEFT:
		if(textbox_edit_offset)
			textbox_edit_offset--;

		return INPUT_HANDLED;
	case CONSOLE_KEY_RIGHT:
		if(textbox_edit_offset < textbox_edit_len)
			textbox_edit_offset++;

		return INPUT_HANDLED;
	case CONSOLE_KEY_HOME:
		textbox_edit_offset = 0;
		return INPUT_HANDLED;
	case CONSOLE_KEY_END:
		textbox_edit_offset = textbox_edit_len;
		return INPUT_HANDLED;
	case '\b':
		if(textbox_edit_offset) {
			if(textbox_edit_offset < textbox_edit_len) {
				memmove(&textbox_edit_buf[textbox_edit_offset - 1],
					&textbox_edit_buf[textbox_edit_offset],
					textbox_edit_len - textbox_edit_offset);
			}

			textbox_edit_offset--;
			textbox_edit_len--;
		}

		return INPUT_RENDER;
	case CONSOLE_KEY_DELETE:
		if(textbox_edit_offset < textbox_edit_len) {
			textbox_edit_len--;
			if(textbox_edit_offset < textbox_edit_len) {
				memmove(&textbox_edit_buf[textbox_edit_offset],
					&textbox_edit_buf[textbox_edit_offset + 1],
					textbox_edit_len - textbox_edit_offset - 1);
			}
		}

		return INPUT_RENDER;
	default:
		/* Ignore non-printable keys. */
		if(key > 0xFF || !isprint(key))
			return INPUT_HANDLED;

		ch = key & 0xFF;
		if(textbox_edit_len < ARRAY_SIZE(textbox_edit_buf)) {
			if(textbox_edit_offset < textbox_edit_len) {
				memmove(&textbox_edit_buf[textbox_edit_offset + 1],
					&textbox_edit_buf[textbox_edit_offset],
					textbox_edit_len - textbox_edit_offset);
			}

			textbox_edit_buf[textbox_edit_offset++] = ch;
			textbox_edit_len++;
		}

		/* FIXME: I'm lazy and cba to make this update the screen. */
		return INPUT_RENDER;
	}
}

/** Place the cursor for the text box edit window.
 * @param window	Window to place cursor for. */
static void ui_textbox_editor_place_cursor(ui_window_t *window) {
	draw_region_t content;
	int x, y;

	main_console->get_region(&content);
	x = textbox_edit_offset % content.width;
	y = textbox_edit_offset / content.width;
	main_console->move_cursor(x, y);
}

/** Text box editor window type. */
static ui_window_type_t ui_textbox_editor_window_type = {
	.render = ui_textbox_editor_render,
	.help = ui_textbox_editor_help,
	.input = ui_textbox_editor_input,
	.place_cursor = ui_textbox_editor_place_cursor,
};

/** Edit the value of a text box.
 * @param entry		Entry to edit.
 * @return		Input handling result. */
static input_result_t ui_textbox_edit(ui_entry_t *entry) {
	ui_textbox_t *box = (ui_textbox_t *)entry;
	size_t len;

	/* Copy the current string to the editing buffer. */
	len = strlen(box->value->string);
	memcpy(textbox_edit_buf, box->value->string, len);
	textbox_edit_len = len;
	textbox_edit_offset = len;
	textbox_edit_update = false;

	/* Display the editor. */
	ui_window_display(textbox_edit_window, 0);

	/* Copy back the new string. */
	if(textbox_edit_update) {
		if(textbox_edit_len != len) {
			kfree(box->value->string);
			box->value->string = kmalloc(textbox_edit_len + 1);
		}
		memcpy(box->value->string, textbox_edit_buf, textbox_edit_len);
		box->value->string[textbox_edit_len] = 0;
	}

	return INPUT_RENDER;
}

/** Actions for a text box. */
static ui_action_t ui_textbox_actions[] = {
	{ "Edit", '\n', ui_textbox_edit },
};

/** Render a text box.
 * @param entry		Entry to render. */
static void ui_textbox_render(ui_entry_t *entry) {
	ui_textbox_t *box = (ui_textbox_t *)entry;
	size_t len, avail, i;

	kprintf("%s", box->label);

	/* Work out the length available to put the string value in. */
	avail = UI_CONTENT_WIDTH - strlen(box->label) - 3;
	len = strlen(box->value->string);
	if(len > avail) {
		kprintf(" [");
		for(i = 0; i < avail - 3; i++)
			main_console->putch(box->value->string[i]);
		kprintf("...]");
	} else {
		main_console->move_cursor(0 - len - 2, 0);
		kprintf("[%s]", box->value->string);
	}
}

/** Text box entry type. */
static ui_entry_type_t ui_textbox_entry_type = {
	.actions = ui_textbox_actions,
	.action_count = ARRAY_SIZE(ui_textbox_actions),
	.render = ui_textbox_render,
};

/** Create a textbox entry.
 * @param label		Label for the textbox.
 * @param value		Value to store state in (should be VALUE_TYPE_STRING).
 * @return		Pointer to created entry. */
ui_entry_t *ui_textbox_create(const char *label, value_t *value) {
	ui_textbox_t *box = kmalloc(sizeof(ui_textbox_t));

	assert(value->type == VALUE_TYPE_STRING);

	ui_entry_init(&box->header, &ui_textbox_entry_type);
	box->label = label;
	box->value = value;

	/* Create the editor window if it does not exist. */
	if(!textbox_edit_window) {
		textbox_edit_window = kmalloc(sizeof(ui_window_t));
		ui_window_init(textbox_edit_window, &ui_textbox_editor_window_type, label);
	}

	return &box->header;
}

/** Create an entry appropriate to edit a value.
 * @param label		Label to give the entry.
 * @param value		Value to edit.
 * @return		Pointer to created entry. */
ui_entry_t *ui_entry_create(const char *label, value_t *value) {
	switch(value->type) {
	case VALUE_TYPE_BOOLEAN:
		return ui_checkbox_create(label, value);
	case VALUE_TYPE_STRING:
		return ui_textbox_create(label, value);
	default:
		assert(0 && "Unhandled value type");
		return NULL;
	}
}

/** Change the value of a chooser.
 * @param entry		Entry to change.
 * @return		Input handling result. */
static input_result_t ui_chooser_change(ui_entry_t *entry) {
	ui_chooser_t *chooser = (ui_chooser_t *)entry;

	ui_window_display(chooser->list, 0);
	return INPUT_RENDER;
}

/** Actions for a chooser. */
static ui_action_t ui_chooser_actions[] = {
	{ "Change", '\n', ui_chooser_change },
};

/** Render a chooser.
 * @param entry		Entry to render. */
static void ui_chooser_render(ui_entry_t *entry) {
	ui_chooser_t *chooser = (ui_chooser_t *)entry;

	assert(chooser->selected);

	kprintf("%s", chooser->label);
	main_console->move_cursor(0 - strlen(chooser->selected->label) - 2, 0);
	kprintf("[%s]", chooser->selected->label);
}

/** Chooser entry type. */
static ui_entry_type_t ui_chooser_entry_type = {
	.actions = ui_chooser_actions,
	.action_count = ARRAY_SIZE(ui_chooser_actions),
	.render = ui_chooser_render,
};

/**
 * Create a chooser entry.
 *
 * Creates an entry that presents a list of values to choose from, and sets a
 * value to the chosen value. The value given to this function is the value
 * that should be modified by the entry. All entries added to the chooser should
 * match the type of this value. The caller should ensure that its current
 * value is a valid choice.
 *
 * @param label		Label for the entry.
 * @param value		Value to store state in.
 *
 * @return		Pointer to created entry.
 */
ui_entry_t *ui_chooser_create(const char *label, value_t *value) {
	ui_chooser_t *chooser = kmalloc(sizeof(ui_chooser_t));

	// TODO: Support other types.
	assert(value->type == VALUE_TYPE_STRING && "Only string choices supported");

	ui_entry_init(&chooser->header, &ui_chooser_entry_type);
	chooser->label = label;
	chooser->selected = NULL;
	chooser->value = value;
	chooser->list = ui_list_create(label, true);
	return &chooser->header;
}

/** Select a choice.
 * @param entry		Entry to select.
 * @return		Input handling result. */
static input_result_t ui_choice_select(ui_entry_t *entry) {
	ui_choice_t *choice = (ui_choice_t *)entry;

	choice->chooser->selected = choice;
	value_destroy(choice->chooser->value);
	value_copy(&choice->value, choice->chooser->value);
	return INPUT_CLOSE;
}

/** Actions for a choice. */
static ui_action_t ui_choice_actions[] = {
	{ "Select", '\n', ui_choice_select },
};

/** Render a choice.
 * @param entry		Entry to render. */
static void ui_choice_render(ui_entry_t *entry) {
	ui_choice_t *choice = (ui_choice_t *)entry;
	kprintf("%s", choice->label);
}

/** Chooser entry type. */
static ui_entry_type_t ui_choice_entry_type = {
	.actions = ui_choice_actions,
	.action_count = ARRAY_SIZE(ui_choice_actions),
	.render = ui_choice_render,
};

/** Insert a choice into a choice entry.
 * @param entry		Entry to insert into.
 * @param label		Label for the choice, or NULL to match value.
 * @param value		Value of the choice. */
void ui_chooser_insert(ui_entry_t *entry, const char *label, const value_t *value) {
	ui_choice_t *choice = kmalloc(sizeof(ui_choice_t));
	ui_chooser_t *chooser = (ui_chooser_t *)entry;

	ui_entry_init(&choice->header, &ui_choice_entry_type);
	value_copy(value, &choice->value);
	choice->chooser = chooser;
	choice->label = (label) ? label : choice->value.string;

	/* Check if this matches the current value and mark it as selected if
	 * it is. */
	if(!chooser->selected) {
		// FIXME: other value types.
		if(strcmp(chooser->value->string, value->string) == 0)
			chooser->selected = choice;
	}

	ui_list_insert(chooser->list, &choice->header, (chooser->selected) == choice);
}
