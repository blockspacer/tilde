/* Copyright (C) 2011-2012,2018 G.P. Halkes
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 3, as
   published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "tilde/dialogs/selectbufferdialog.h"
#include "tilde/openfiles.h"

select_buffer_dialog_t::select_buffer_dialog_t(int height, int width)
    : dialog_t(height, width, _("Select Buffer")), known_version(INT_MIN) {
  list = emplace_back<list_pane_t>(true);
  list->set_size(height - 3, width - 2);
  list->set_position(1, 1);
  list->connect_activate([this] { ok_activated(); });

  button_t *ok_button = emplace_back<button_t>("_OK", true);
  button_t *cancel_button = emplace_back<button_t>("_Cancel", false);

  cancel_button->set_anchor(this,
                            T3_PARENT(T3_ANCHOR_BOTTOMRIGHT) | T3_CHILD(T3_ANCHOR_BOTTOMRIGHT));
  cancel_button->set_position(-1, -2);
  cancel_button->connect_activate([this] { close(); });
  cancel_button->connect_move_focus_left([this] { focus_previous(); });
  cancel_button->connect_move_focus_up([this] { set_child_focus(list); });
  ok_button->set_anchor(cancel_button, T3_PARENT(T3_ANCHOR_TOPLEFT) | T3_CHILD(T3_ANCHOR_TOPRIGHT));
  ok_button->set_position(0, -2);
  ok_button->connect_activate([this] { ok_activated(); });
  cancel_button->connect_move_focus_right([this] { focus_next(); });
  cancel_button->connect_move_focus_up([this] { focus_previous(); });
}

bool select_buffer_dialog_t::set_size(optint height, optint width) {
  bool result = true;

  if (!height.is_valid()) {
    height = window.get_height();
  }
  if (!width.is_valid()) {
    width = window.get_width();
  }

  result &= dialog_t::set_size(height, width);

  result &= list->set_size(height.value() - 3, width.value() - 2);
  return result;
}

void select_buffer_dialog_t::show() {
  if (open_files.get_version() != known_version) {
    known_version = open_files.get_version();
    int width = window.get_width();

    while (!list->empty()) {
      list->pop_back();
    }

    for (file_buffer_t *open_file : open_files) {
      std::unique_ptr<multi_widget_t> multi_widget(new multi_widget_t());
      multi_widget->set_size(None, width - 5);
      multi_widget->show();
      bullet_t *bullet = new bullet_t([open_file] { return open_file->get_has_window(); });
      multi_widget->push_back(wrap_unique(bullet), -1, true, false);
      const char *name = open_file->get_name().c_str();
      if (name[0] == 0) {
        name = "(Untitled)";
      }
      label_t *label = new label_t(name);
      label->set_anchor(bullet, T3_PARENT(T3_ANCHOR_TOPRIGHT) | T3_CHILD(T3_ANCHOR_TOPLEFT));
      label->set_align(label_t::ALIGN_LEFT_UNDERFLOW);
      label->set_accepts_focus(true);
      multi_widget->push_back(wrap_unique(label), 1, true, false);
      list->push_back(std::move(multi_widget));
    }
  }
  list->reset();
  dialog_t::show();
}

void select_buffer_dialog_t::ok_activated() {
  hide();
  activate(open_files[list->get_current()]);
}
