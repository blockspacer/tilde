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
#ifndef FILESTATE_H
#define FILESTATE_H
#include <cerrno>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>

#include <t3widget/signals.h>
#include <t3widget/widget.h>
#include <transcript/transcript.h>

#include "tilde/filewrapper.h"
#include "tilde/openfiles.h"
#include "tilde/util.h"

using namespace t3widget;

class file_buffer_t;

class rw_result_t {
 public:
  enum stop_reason_t {
    SUCCESS,
    FILE_EXISTS,
    READ_ONLY_FILE,
    BACKUP_FAILED,
    ERRNO_ERROR,
    ERRNO_ERROR_FILE_UNTOUCHED,
    CONVERSION_OPEN_ERROR,
    CONVERSION_IMPRECISE,
    CONVERSION_ERROR,
    CONVERSION_ILLEGAL,
    CONVERSION_TRUNCATED,
    BOM_FOUND,
    MODE_RESET_FAILED,
    INTERNAL_ERROR,
    RACE_ON_FILE,
  };

 private:
  stop_reason_t reason = SUCCESS;
  union {
    int errno_error;
    transcript_error_t transcript_error;
  };

 public:
  rw_result_t() = default;
  explicit rw_result_t(stop_reason_t _reason) : reason(_reason), errno_error(errno) {}
  rw_result_t(stop_reason_t _reason, int _errno_error)
      : reason(_reason), errno_error(_errno_error) {}
  rw_result_t(stop_reason_t _reason, transcript_error_t _transcript_error)
      : reason(_reason), transcript_error(_transcript_error) {}
  int get_errno_error() { return errno_error; }
  transcript_error_t get_transcript_error() { return transcript_error; }
  operator int() const { return static_cast<int>(reason); }
};

class load_process_t : public stepped_process_t {
  friend class file_buffer_t;

 protected:
  enum { SELECT_FILE, INITIAL, INITIAL_MISSING_OK, READING_FIRST, READING } state;

  enum {
    UNKNOWN,
    REMOVE_BOM,
    PRESERVE_BOM,
  } bom_state;

  file_buffer_t *file;
  file_read_wrapper_t *wrapper;
  std::string encoding;
  int fd;
  bool buffer_used;

  explicit load_process_t(const callback_t &cb);
  load_process_t(const callback_t &cb, const char *name, const char *_encoding, bool missing_ok);
  void abort();
  bool step() override;
  virtual void file_selected(const std::string &name);
  virtual void encoding_selected(const std::string *_encoding);
  void cleanup() override;
  void preserve_bom();
  void remove_bom();

 public:
  void set_up_connections();

  virtual file_buffer_t *get_file_buffer();
  static void execute(const callback_t &cb);
  static void execute(const callback_t &cb, const char *name, const char *encoding = nullptr,
                      bool missing_ok = false);
};

class save_as_process_t : public stepped_process_t {
  friend class file_buffer_t;

 protected:
  enum { SELECT_FILE, INITIAL, OPEN_FILE, CHANGE_MODE, CREATE_BACKUP, WRITING };
  int state = SELECT_FILE;

  file_buffer_t *file;
  std::string name;
  std::string encoding;
  bool allow_highlight_change;
  bool highlight_changed = false;

  // State for save file_buffer_t::save function
  const char *save_name = nullptr;
  std::string real_name;
  std::string temp_name;
  int fd = -1;
  int backup_fd = -1;
  int readonly_fd = -1;
  dev_t readonly_dev;
  ino_t readonly_ino;
  bool backup_saved = false;
  off_t computed_length = 0;
  optional<mode_t> original_mode;
  text_pos_t i;
  transcript_t *conversion_handle = nullptr;
  std::unique_ptr<file_write_wrapper_t> wrapper = nullptr;

  save_as_process_t(const callback_t &cb, file_buffer_t *_file,
                    bool _allow_highlight_change = true);
  bool step() override;
  virtual void file_selected(const std::string &_name);
  virtual void encoding_selected(const std::string *_encoding);
  void cleanup() override;

 public:
  static void execute(const callback_t &cb, file_buffer_t *_file);

  bool get_highlight_changed() const;
};

class save_process_t : public save_as_process_t {
 protected:
  save_process_t(const callback_t &cb, file_buffer_t *_file);

 public:
  static void execute(const callback_t &cb, file_buffer_t *_file);
};

class close_process_t : public save_process_t {
 protected:
  enum { CONFIRM_CLOSE = WRITING + 1, CLOSE };

  close_process_t(const callback_t &cb, file_buffer_t *_file);
  bool step() override;
  virtual void do_save();
  virtual void dont_save();

 public:
  static void execute(const callback_t &cb, file_buffer_t *_file);
  virtual const file_buffer_t *get_file_buffer_ptr();
};

class exit_process_t : public stepped_process_t {
 protected:
  open_files_t::iterator iter;

  explicit exit_process_t(const callback_t &cb);
  bool step() override;
  virtual void do_save();
  virtual void dont_save();
  virtual void save_done(stepped_process_t *process);

 public:
  static void execute(const callback_t &cb);
};

class open_recent_process_t : public load_process_t {
 protected:
  recent_file_info_t *info;

  explicit open_recent_process_t(const callback_t &cb);
  void cleanup() override;
  virtual void recent_file_selected(recent_file_info_t *_info);
  bool step() override;

 public:
  static void execute(const callback_t &cb);
};

class load_cli_file_process_t : public stepped_process_t {
 private:
  void attempt_file_position_parse(std::string *filename, text_pos_t *line, text_pos_t *pos);

 protected:
  std::list<std::string>::const_iterator iter;
  bool in_load, in_step, encoding_selected;
  std::string encoding;

  explicit load_cli_file_process_t(const callback_t &cb);
  bool step() override;
  virtual void load_done(stepped_process_t *process);

  void encoding_selection_done(const std::string *);
  void encoding_selection_canceled();

 public:
  static void execute(const callback_t &cb);
};

#endif
