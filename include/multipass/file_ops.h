/*
 * Copyright (C) Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef MULTIPASS_FILE_OPS_H
#define MULTIPASS_FILE_OPS_H

#include "recursive_dir_iterator.h"
#include "singleton.h"

#include <QByteArray>
#include <QDir>
#include <QFileDevice>
#include <QSaveFile>
#include <QString>
#include <QTextStream>

#include <filesystem>
#include <fstream>

#define MP_FILEOPS multipass::FileOps::instance()

namespace multipass
{
namespace fs = std::filesystem;

class FileOps : public Singleton<FileOps>
{
public:
    FileOps(const Singleton<FileOps>::PrivatePass&) noexcept;

    // QDir operations
    virtual bool exists(const QDir& dir) const;
    virtual bool isReadable(const QDir& dir) const;
    virtual bool mkpath(const QDir& dir, const QString& dirName) const;
    virtual bool rmdir(QDir& dir, const QString& dirName) const;

    // QFileInfo operations
    virtual bool exists(const QFileInfo& file) const;
    virtual bool isDir(const QFileInfo& file) const;
    virtual bool isReadable(const QFileInfo& file) const;

    // QFile operations
    virtual bool exists(const QFile& file) const;
    virtual bool is_open(const QFile& file) const;
    virtual bool open(QFileDevice& file, QIODevice::OpenMode mode) const;
    virtual QFileDevice::Permissions permissions(const QFile& file) const;
    virtual qint64 read(QFile& file, char* data, qint64 maxSize) const;
    virtual QByteArray read_all(QFile& file) const;
    virtual QString read_line(QTextStream& text_stream) const;
    virtual bool remove(QFile& file) const;
    virtual bool rename(QFile& file, const QString& newName) const;
    virtual bool resize(QFile& file, qint64 sz) const;
    virtual bool seek(QFile& file, qint64 pos) const;
    virtual bool setPermissions(QFile& file, QFileDevice::Permissions permissions) const;
    virtual qint64 size(QFile& file) const;
    virtual qint64 write(QFile& file, const char* data, qint64 maxSize) const;
    virtual qint64 write(QFileDevice& file, const QByteArray& data) const;

    // QSaveFile operations
    virtual bool commit(QSaveFile& file) const;

    // std operations
    virtual void open(std::fstream& stream, const char* filename, std::ios_base::openmode mode) const;
    virtual std::unique_ptr<std::ostream> open_write(const fs::path& path,
                                                     std::ios_base::openmode mode = std::ios_base::out) const;
    virtual std::unique_ptr<std::istream> open_read(const fs::path& path,
                                                    std::ios_base::openmode mode = std::ios_base::in) const;
    virtual bool exists(const fs::path& path, std::error_code& err) const;
    virtual bool is_directory(const fs::path& path, std::error_code& err) const;
    virtual bool create_directory(const fs::path& path, std::error_code& err) const;
    virtual bool create_directories(const fs::path& path, std::error_code& err) const;
    virtual bool remove(const fs::path& path, std::error_code& err) const;
    virtual void create_symlink(const fs::path& to, const fs::path& path, std::error_code& err) const;
    virtual fs::path read_symlink(const fs::path& path, std::error_code& err) const;
    virtual void permissions(const fs::path& path, fs::perms perms, std::error_code& err) const;
    virtual fs::file_status status(const fs::path& path, std::error_code& err) const;
    virtual std::unique_ptr<RecursiveDirIterator> recursive_dir_iterator(const fs::path& path,
                                                                         std::error_code& err) const;
};
} // namespace multipass

#endif // MULTIPASS_FILE_OPS_H
