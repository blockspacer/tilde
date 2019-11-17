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
#include <cstring>

#include "tilde/filebuffer.h"
#include "tilde/filestate.h"
#include "tilde/log.h"
#include "tilde/main.h"
#include "tilde/openfiles.h"
#include "tilde/option.h"

load_process_t::load_process_t(const callback_t &cb)
    : stepped_process_t(cb),
      state(SELECT_FILE),
      bom_state(UNKNOWN),
      file(nullptr),
      wrapper(nullptr),
      encoding("UTF-8"),
      fd(-1),
      buffer_used(true) {
  set_up_connections();
}

load_process_t::load_process_t(const callback_t &cb, const char *name, const char *_encoding,
                               bool missing_ok)
    : stepped_process_t(cb),
      state(missing_ok ? INITIAL_MISSING_OK : INITIAL),
      bom_state(UNKNOWN),
      file(new file_buffer_t(name, _encoding == nullptr ? "UTF-8" : _encoding)),
      wrapper(nullptr),
      encoding(_encoding == nullptr ? "UTF-8" : _encoding),
      fd(-1),
      buffer_used(true) {
  set_up_connections();
}

void load_process_t::set_up_connections() {
  connections.push_back(continue_abort_dialog->connect_activate([this] { run(); }, 0));
  connections.push_back(continue_abort_dialog->connect_activate([this] { abort(); }, 1));
  connections.push_back(continue_abort_dialog->connect_closed([this] { abort(); }));

  connections.push_back(preserve_bom_dialog->connect_activate([this] { preserve_bom(); }, 0));
  connections.push_back(preserve_bom_dialog->connect_activate([this] { remove_bom(); }, 1));
  connections.push_back(preserve_bom_dialog->connect_closed([this] { preserve_bom(); }));

  connections.push_back(
      open_file_dialog->connect_file_selected(bind_front(&load_process_t::file_selected, this)));
  connections.push_back(open_file_dialog->connect_closed([this] { abort(); }));

  connections.push_back(
      encoding_dialog->connect_activate(bind_front(&load_process_t::encoding_selected, this)));
}

void load_process_t::abort() {
  delete file;
  file = nullptr;
  stepped_process_t::abort();
}

bool load_process_t::step() {
  std::string message;
  rw_result_t rw_result;

  if (state == SELECT_FILE) {
    open_file_dialog->reset();
    open_file_dialog->show();
    encoding_dialog->set_encoding(encoding.c_str());
    return false;
  }

  switch ((rw_result = file->load(this))) {
    case rw_result_t::SUCCESS:
      result = true;
      break;
    case rw_result_t::ERRNO_ERROR:
      printf_into(&message, "Could not load file '%s': %s", file->get_name().c_str(),
                  strerror(rw_result.get_errno_error()));
      error_dialog->set_message(message);
      error_dialog->show();
      result = true;
      break;
    case rw_result_t::CONVERSION_OPEN_ERROR:
      printf_into(&message, "Could not find a converter for selected encoding: %s",
                  transcript_strerror(rw_result.get_transcript_error()));
      delete file;
      file = nullptr;
      error_dialog->set_message(message);
      error_dialog->show();
      break;
    case rw_result_t::CONVERSION_ERROR:
      printf_into(&message, "Could not load file in encoding %s: %s", file->get_encoding(),
                  transcript_strerror(rw_result.get_transcript_error()));
      delete file;
      file = nullptr;
      error_dialog->set_message(message);
      error_dialog->show();
      break;
    case rw_result_t::CONVERSION_IMPRECISE:
      printf_into(&message, "Conversion from encoding %s is irreversible", file->get_encoding());
      continue_abort_dialog->set_message(message);
      continue_abort_dialog->show();
      return false;
    case rw_result_t::CONVERSION_ILLEGAL:
      printf_into(&message, "Conversion from encoding %s encountered illegal characters",
                  file->get_encoding());
      continue_abort_dialog->set_message(message);
      continue_abort_dialog->show();
      return false;
    case rw_result_t::CONVERSION_TRUNCATED:
      printf_into(&message, "File appears to be truncated");
      result = true;
      error_dialog->set_message(message);
      error_dialog->show();
      break;
    case rw_result_t::BOM_FOUND:
      preserve_bom_dialog->show();
      return false;
    default:
      PANIC();
  }
  return true;
}

void load_process_t::file_selected(const std::string &name) {
  open_files_t::iterator iter;
  if ((iter = open_files.contains(name.c_str())) != open_files.end()) {
    file = *iter;
    done();
    return;
  }

  file = new file_buffer_t(name, encoding);
  state = INITIAL;
  run();
}

void load_process_t::encoding_selected(const std::string *_encoding) { encoding = *_encoding; }

file_buffer_t *load_process_t::get_file_buffer() {
  if (result) {
    return file;
  }
  return nullptr;
}

void load_process_t::cleanup() {
#ifdef DEBUG
  ASSERT(result || file == nullptr);
#endif
  delete wrapper;
  if (fd >= 0) {
    close(fd);
  }
}

void load_process_t::preserve_bom() {
  // FIXME: set encoding accordingly
  bom_state = PRESERVE_BOM;
  run();
}

void load_process_t::remove_bom() {
  bom_state = REMOVE_BOM;
  run();
}

void load_process_t::execute(const callback_t &cb) { (new load_process_t(cb))->run(); }

void load_process_t::execute(const callback_t &cb, const char *name, const char *encoding,
                             bool missing_ok) {
  (new load_process_t(cb, name, encoding, missing_ok))->run();
}

save_as_process_t::save_as_process_t(const callback_t &cb, file_buffer_t *_file,
                                     bool _allow_highlight_change)
    : stepped_process_t(cb),
      state(SELECT_FILE),
      file(_file),
      allow_highlight_change(_allow_highlight_change) {
  connections.push_back(continue_abort_dialog->connect_activate([this] { run(); }, 0));
  connections.push_back(continue_abort_dialog->connect_activate([this] { abort(); }, 1));
  connections.push_back(continue_abort_dialog->connect_closed([this] { abort(); }));

  connections.push_back(
      save_as_dialog->connect_file_selected(bind_front(&save_as_process_t::file_selected, this)));
  connections.push_back(save_as_dialog->connect_closed([this] { abort(); }));

  connections.push_back(
      encoding_dialog->connect_activate(bind_front(&save_as_process_t::encoding_selected, this)));
}

bool save_as_process_t::step() {
  std::string message;
  rw_result_t rw_result;

  if (state == SELECT_FILE) {
    save_as_dialog->set_from_file(file->get_name());
    save_as_dialog->show();
    encoding_dialog->set_encoding(file->get_encoding());
    return false;
  }

  switch ((rw_result = file->save(this))) {
    case rw_result_t::SUCCESS:
      result = true;
      break;
    case rw_result_t::FILE_EXISTS:
      printf_into(&message, "File '%s' already exists", name.c_str());
      continue_abort_dialog->set_message(message);
      continue_abort_dialog->show();
      return false;
    case rw_result_t::BACKUP_FAILED:
      printf_into(
          &message,
          "Could not create a backup file: %s.\n\nThis could result in data loss if Tilde is "
          "unable to complete writing the file",
          strerror(rw_result.get_errno_error()));
      continue_abort_dialog->set_message(message);
      continue_abort_dialog->show();
      return false;
    case rw_result_t::ERRNO_ERROR:
    case rw_result_t::ERRNO_ERROR_FILE_UNTOUCHED:
      printf_into(&message, "Could not save file: %s.", strerror(rw_result.get_errno_error()));
      if (rw_result == rw_result_t::ERRNO_ERROR_FILE_UNTOUCHED) {
        message.append(
            "\n\nThe original file has not been touched. Save the current buffer to another "
            "location to ensure its contents are preserved!");
      } else if (backup_saved) {
        message.append("\n\nThe original contents of the file can still be retrieved from ");
        // FIXME: the file names probably needs to be converted from some other character set.
        if (!temp_name.empty()) {
          message.append(temp_name);
        } else {
          message.append(name);
          message.append("~");
        }
        message.append(
            ".  Save the current buffer to another location to ensure its contents are preserved!");
      } else {
        message.append(
            "\n\nThe original file contents are lost. Save the current buffer to some other "
            "location to ensure its contents are preserved!");
      }
      error_dialog->set_message(message);
      error_dialog->show();
      break;
    case rw_result_t::CONVERSION_ERROR:
      if (encoding.empty()) {
        encoding = file->get_encoding();
      }
      printf_into(&message, "Could not save file in encoding %s: %s", encoding.c_str(),
                  transcript_strerror(rw_result.get_transcript_error()));
      error_dialog->set_message(message);
      error_dialog->show();
      break;
    case rw_result_t::CONVERSION_IMPRECISE:
      if (encoding.empty()) {
        encoding = file->get_encoding();
      }
      i++;
      printf_into(&message,
                  "Conversion into encoding %s is irreversible\n\nThe loaded buffer will continue "
                  "to hold the original text, but the on-disk version will differ.",
                  encoding.c_str());
      continue_abort_dialog->set_message(message);
      continue_abort_dialog->show();
      return false;
    case rw_result_t::READ_ONLY_FILE:
      printf_into(&message, "File %s is read-only", save_name);
      continue_abort_dialog->set_message(message);
      continue_abort_dialog->show();
      return false;
    case rw_result_t::MODE_RESET_FAILED:
      printf_into(
          &message,
          "The file %s was written successfully, but changing back the mode bits on the file "
          "failed: %s",
          name.c_str(), strerror(rw_result.get_errno_error()));
      error_dialog->set_message(message);
      error_dialog->show();
      break;
    case rw_result_t::RACE_ON_FILE:
      printf_into(&message,
                  "Opening file '%s' after changing the mode opened a different file. The file was "
                  "not written.",
                  name.c_str());
      break;
    default:
      printf_into(&message,
                  "An unknown error occurred during saving. The file has not been saved and may be "
                  "damaged!");
      error_dialog->set_message(message);
      error_dialog->show();
      break;
  }
  return true;
}

void save_as_process_t::file_selected(const std::string &_name) {
  name = _name;
  state = INITIAL;
  if (allow_highlight_change) {
    highlight_changed = true;
    file->set_highlight(t3_highlight_load_by_filename(name.c_str(), map_highlight, nullptr,
                                                      T3_HIGHLIGHT_UTF8, nullptr));
  }
  run();
}

void save_as_process_t::encoding_selected(const std::string *_encoding) { encoding = *_encoding; }

void save_as_process_t::cleanup() {
  if (backup_fd >= 0) {
    close(backup_fd);
  }
  if (readonly_fd >= 0) {
    if (original_mode.is_valid()) {
      fchmod(fd, original_mode.value());
      original_mode.reset();
    }
    close(readonly_fd);
  }
  if (fd >= 0) {
    if (original_mode.is_valid()) {
      fchmod(fd, original_mode.value());
    }
    close(fd);
  } else if (!temp_name.empty()) {
    // Remove the backup file (not the ~ backup, but the temporary file we may have created).
    unlink(temp_name.c_str());
  }
  if (conversion_handle) {
    transcript_close_converter(conversion_handle);
  }
}

void save_as_process_t::execute(const callback_t &cb, file_buffer_t *_file) {
  (new save_as_process_t(cb, _file))->run();
}

bool save_as_process_t::get_highlight_changed() const { return highlight_changed; }

save_process_t::save_process_t(const callback_t &cb, file_buffer_t *_file)
    : save_as_process_t(cb, _file, false) {
  if (!file->get_name().empty()) {
    state = INITIAL;
  }
}

void save_process_t::execute(const callback_t &cb, file_buffer_t *_file) {
  (new save_process_t(cb, _file))->run();
}

close_process_t::close_process_t(const callback_t &cb, file_buffer_t *_file)
    : save_process_t(cb, _file) {
  state = file->is_modified() ? CONFIRM_CLOSE : CLOSE;
  if (!_file->is_modified()) {
    state = CLOSE;
  }
  connections.push_back(close_confirm_dialog->connect_activate([this] { do_save(); }, 0));
  connections.push_back(close_confirm_dialog->connect_activate([this] { dont_save(); }, 1));
  connections.push_back(close_confirm_dialog->connect_activate([this] { abort(); }, 2));
  connections.push_back(close_confirm_dialog->connect_closed([this] { abort(); }));
}

bool close_process_t::step() {
  if (state < CONFIRM_CLOSE) {
    if (save_process_t::step()) {
      if (!result) {
        return true;
      }
      state = CLOSE;
    } else {
      return false;
    }
  }

  if (state == CLOSE) {
    recent_files.push_front(file);
    /* Can't delete the file_buffer_t here, because on switching buffers the
       edit_window_t will still want to do some stuff with it. Furthermore,
       the newly allocated file_buffer_t may be at the same address, causing
       further difficulties. So that has to be done in the callback :-( */
    result = true;
    return true;
  } else if (state == CONFIRM_CLOSE) {
    std::string message;
    printf_into(&message, "Save changes to '%s'",
                file->get_name().empty() ? "(Untitled)" : file->get_name().c_str());
    close_confirm_dialog->set_message(message);
    close_confirm_dialog->show();
  } else {
    PANIC();
  }
  return false;
}

void close_process_t::do_save() {
  state = file->get_name().empty() ? SELECT_FILE : INITIAL;
  run();
}

void close_process_t::dont_save() {
  state = CLOSE;
  run();
}

void close_process_t::execute(const callback_t &cb, file_buffer_t *_file) {
  (new close_process_t(cb, _file))->run();
}

const file_buffer_t *close_process_t::get_file_buffer_ptr() { return file; }

exit_process_t::exit_process_t(const callback_t &cb)
    : stepped_process_t(cb), iter(open_files.begin()) {
  connections.push_back(close_confirm_dialog->connect_activate([this] { do_save(); }, 0));
  connections.push_back(close_confirm_dialog->connect_activate([this] { dont_save(); }, 1));
  connections.push_back(close_confirm_dialog->connect_activate([this] { abort(); }, 2));
  connections.push_back(close_confirm_dialog->connect_closed([this] { abort(); }));
}

bool exit_process_t::step() {
  for (; iter != open_files.end(); iter++) {
    if ((*iter)->is_modified()) {
      std::string message;
      printf_into(&message, "Save changes to '%s'",
                  (*iter)->get_name().empty() ? "(Untitled)" : (*iter)->get_name().c_str());
      close_confirm_dialog->set_message(message);
      close_confirm_dialog->show();
      return false;
    }
  }
  return true;
}

void exit_process_t::do_save() {
  save_process_t::execute(bind_front(&exit_process_t::save_done, this), *iter);
}

void exit_process_t::dont_save() {
  iter++;
  run();
}

void exit_process_t::save_done(stepped_process_t *process) {
  if (process->get_result()) {
    iter++;
    run();
  } else {
    abort();
  }
}

void exit_process_t::execute(const callback_t &cb) {
  (new exit_process_t([cb](stepped_process_t *process) {
    lprintf("Exit process callback with result %d\n", process->get_result());
    cb(process);
    if (process->get_result()) {
      exit_main_loop(EXIT_SUCCESS);
    }
  }))->run();
}

open_recent_process_t::open_recent_process_t(const callback_t &cb) : load_process_t(cb) {
  connections.push_back(open_recent_dialog->connect_file_selected(
      bind_front(&open_recent_process_t::recent_file_selected, this)));
  connections.push_back(open_recent_dialog->connect_closed([this] { abort(); }));
}

bool open_recent_process_t::step() {
  if (state == SELECT_FILE) {
    open_recent_dialog->show();
    return false;
  }
  return load_process_t::step();
}

void open_recent_process_t::recent_file_selected(recent_file_info_t *_info) {
  info = _info;
  file = new file_buffer_t(info->get_name(), info->get_encoding());
  state = INITIAL;
  run();
}

void open_recent_process_t::cleanup() {
  if (result) {
    recent_files.erase(info);
  }
}

void open_recent_process_t::execute(const callback_t &cb) {
  (new open_recent_process_t(cb))->run();
}

load_cli_file_process_t::load_cli_file_process_t(const callback_t &cb)
    : stepped_process_t(cb),
      iter(cli_option.files.begin()),
      in_load(false),
      in_step(false),
      encoding_selected(false) {}

bool load_cli_file_process_t::step() {
  if (!encoding_selected) {
    encoding_selected = true;
    if (cli_option.encoding.is_valid()) {
      if (cli_option.encoding.value().empty()) {
        encoding_dialog->set_encoding("UTF-8");
        encoding_dialog->connect_activate(
            bind_front(&load_cli_file_process_t::encoding_selection_done, this));
        encoding_dialog->connect_closed([this] { run(); });
        encoding_dialog->show();
        return false;
      } else {
        encoding = cli_option.encoding.value();
      }
    }
  }

  in_step = true;
  while (iter != cli_option.files.end()) {
    text_pos_t line = -1, pos = -1;
    std::string filename = *iter;
    if (default_option.parse_file_positions.value_or(true) &&
        !cli_option.disable_file_position_parsing) {
      attempt_file_position_parse(&filename, &line, &pos);
    }

    in_load = true;
    load_process_t::execute(bind_front(&load_cli_file_process_t::load_done, this), filename.c_str(),
                            encoding.c_str(), true);
    if (in_load) {
      in_step = false;
      return false;
    }
    open_files.back()->goto_pos(line, pos);
  }
  result = true;
  return true;
}

void load_cli_file_process_t::load_done(stepped_process_t *process) {
  (void)process;

  in_load = false;
  iter++;
  if (!in_step) {
    run();
  }
}

void load_cli_file_process_t::execute(const callback_t &cb) {
  (new load_cli_file_process_t(cb))->run();
}

void load_cli_file_process_t::encoding_selection_done(const std::string *_encoding) {
  encoding = _encoding->c_str();
  run();
}

static bool is_ascii_digit(int c) { return c >= '0' && c <= '9'; }

void load_cli_file_process_t::attempt_file_position_parse(std::string *filename, text_pos_t *line,
                                                          text_pos_t *pos) {
  size_t idx = filename->size();
  if (idx < 3) {
    return;
  }
  --idx;

  // Skip trailing colon. These will typically occur when copying error messages.
  if ((*filename)[idx] == ':') {
    --idx;
  }

  while (idx > 0 && is_ascii_digit((*filename)[idx])) {
    --idx;
  }
  if (idx == 0) {
    return;
  }
  if ((*filename)[idx] != ':') {
    return;
  }

  *line = static_cast<text_pos_t>(std::atoll(filename->c_str() + idx + 1));
  filename->erase(idx);

  --idx;
  while (idx > 0 && is_ascii_digit((*filename)[idx])) {
    --idx;
  }
  if (idx == 0 || (*filename)[idx] != ':') {
    return;
  }
  *pos = *line;
  *line = static_cast<text_pos_t>(std::atoll(filename->c_str() + idx + 1));
  filename->erase(idx);
}
