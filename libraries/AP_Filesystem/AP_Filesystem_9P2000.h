/*
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <AP_Networking/AP_Networking_Config.h>

#if AP_NETWORKING_FILESYSTEM_ENABLED

#include "AP_Filesystem_backend.h"
#include <AP_Networking/AP_Networking.h>

class AP_Filesystem_9P2000 : public AP_Filesystem_Backend
{
public:

    // functions that closely match the equivalent posix calls
    int open(const char *fname, int flags, bool allow_absolute_paths = false) override;
    int close(int fd) override;
    int32_t read(int fd, void *buf, uint32_t count) override;
    int32_t write(int fd, const void *buf, uint32_t count) override;
    // int fsync(int fd) override; not supported
    int32_t lseek(int fd, int32_t offset, int whence) override;
    int stat(const char *pathname, struct stat *stbuf) override;
    int unlink(const char *pathname) override;
    int mkdir(const char *pathname) override;
    void *opendir(const char *pathname) override;
    struct dirent *readdir(void *dirp) override;
    int closedir(void *dirp) override;
    int rename(const char *oldpath, const char *newpath) override;

    // No way to get disk details in 9P2000
    // return free disk space in bytes, -1 on error
    //int64_t disk_free(const char *path) override;
    //int64_t disk_space(const char *path) override;

    // set modification time on a file
    bool set_mtime(const char *filename, const uint32_t mtime_sec) override;

private:

    // Open a given file ID with flags
    bool open_fileId(NineP2000& fs, const uint32_t fileId, int flags);

    // Wait for response, blocking
    bool wait_for_tag(NineP2000& fs, const uint16_t tag) const;

    // Get the file id for a given name
    uint32_t get_file_id(NineP2000& fs, const char *name, const NineP2000::walkType type) const;

    // Create a file or directory with the given name
    bool create_file(NineP2000& fs, const char *fname, bool is_dir);

    // only allow up to 4 files at a time
    static constexpr uint8_t max_open_file = 4;
    static constexpr uint8_t max_open_dir = 4;
    struct rfile {
        uint32_t fileId;
        uint32_t size;
        uint32_t ofs;
    } file[max_open_file];

    // allow up to 4 directory opens
    struct rdir {
        char *path;
        uint32_t fileId;
        uint32_t ofs;
        struct dirent de;
    } dir[max_open_dir];
};

#endif  // AP_NETWORKING_FILESYSTEM_ENABLED
