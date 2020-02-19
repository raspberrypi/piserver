#include "downloadextractthread.h"
#include <iostream>
#include <archive.h>
#include <archive_entry.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>

using namespace std;

const int DownloadExtractThread::MAX_QUEUE_SIZE = 64;

DownloadExtractThread::DownloadExtractThread(const std::string &url, const std::string &localfolder)
    : DownloadThread(url), _folder(localfolder), _extractThread(NULL)
{
    if (::mkdir(localfolder.c_str(), 0755) == -1) { /* Ignore errors. Folder may already exist */ }
    if (_folder.back() != '/')
        _folder += '/';
}

DownloadExtractThread::~DownloadExtractThread()
{
    if (_extractThread)
    {
        _extractThread->join();
        delete _extractThread;
    }
}

size_t DownloadExtractThread::_writeData(const char *buf, size_t len)
{
    if (!_extractThread)
    {
        /* Extract thread is started when first data comes in */
        _extractThread = new thread(&DownloadExtractThread::extractRun, this);
    }

    _pushQueue(buf, len);

    return len;
}

void DownloadExtractThread::_onDownloadSuccess()
{
    _pushQueue("", 0);
}

void DownloadExtractThread::_onDownloadError(const std::string &msg)
{
    DownloadThread::_onDownloadError(msg);
    _cancelExtract();
}

void DownloadExtractThread::_cancelExtract()
{
    std::unique_lock<std::mutex> lock(_queueMutex);
    _queue.clear();
    _queue.push_back(string());
}

void DownloadExtractThread::cancelDownload()
{
    DownloadThread::cancelDownload();
    _cancelExtract();
}

/* Raise exception on libarchive errors */
static inline void _checkResult(int r, struct archive *a)
{
    if (r < ARCHIVE_OK)
        /* Warning */
        cerr << archive_error_string(a) << endl;
    if (r < ARCHIVE_WARN)
        /* Fatal */
        throw runtime_error(archive_error_string(a));
}

/* libarchive thread */
void DownloadExtractThread::extractRun()
{
    char cwd[PATH_MAX] = {0}, *pcwd;
    struct archive *a = archive_read_new();
    struct archive *ext = archive_write_disk_new();
    struct archive_entry *entry;
    int r, flags = ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM
            | ARCHIVE_EXTRACT_ACL | ARCHIVE_EXTRACT_FFLAGS /*| ARCHIVE_EXTRACT_XATTR*/;
    if (::getuid() == 0)
        flags |= ARCHIVE_EXTRACT_OWNER;

    pcwd = ::getcwd(cwd, sizeof(cwd));
    if (::chdir(_folder.c_str()) == -1)
    {
        DownloadThread::cancelDownload();
        _emitError(string("Error changing to directory '")+_folder+"': "+strerror(errno));
        return;
    }

    archive_read_support_filter_all(a);
    archive_read_support_format_all(a);
    archive_write_disk_set_options(ext, flags);
    archive_read_open(a, this, NULL, &DownloadExtractThread::_archive_read, &DownloadExtractThread::_archive_close);

    try
    {
        while ( (r = archive_read_next_header(a, &entry)) != ARCHIVE_EOF)
        {
          _checkResult(r, a);
          //string fullpath = _folder+archive_entry_pathname(entry);
          //archive_entry_set_pathname(entry, fullpath.c_str());
          r = archive_write_header(ext, entry);
          if (r < ARCHIVE_OK)
              cerr << archive_error_string(ext) << endl;
          else if (archive_entry_size(entry) > 0)
          {
              //checkResult(copyData(a, ext), a);
              const void *buff;
              size_t size;
              int64_t offset;

              while ( (r = archive_read_data_block(a, &buff, &size, &offset)) != ARCHIVE_EOF)
              {
                  _checkResult(r, a);
                  _checkResult(archive_write_data_block(ext, buff, size, offset), ext);
              }
          }
          _checkResult(archive_write_finish_entry(ext), ext);
        }

        if (!_postInstallScript.empty())
        {
            ::setenv("DISTROROOT", _folder.c_str(), 1);
            if (!_ldapConfig.empty())
                ::setenv("EXTERNAL_LDAP_CONFIG", _ldapConfig.c_str(), 1);
            if (system(_postInstallScript.c_str()) != 0)
            {
                _emitError("Error running post-installation script");
            }
            else
            {
                _emitSuccess();
            }
            ::unsetenv("DISTROROOT");
            if (!_ldapConfig.empty())
                ::unsetenv("EXTERNAL_LDAP_CONFIG");
        }
        else
        {
            _emitSuccess();
        }
    }
    catch (exception &e)
    {
        if (!_cancelled)
        {
            /* Fatal error */
            DownloadThread::cancelDownload();
            _emitError(string("Error extracting archive: ")+e.what());
        }
    }

    archive_read_free(a);
    archive_write_free(ext);
    if (pcwd)
    {
        if (::chdir(pcwd) == -1) { }
    }
}

ssize_t DownloadExtractThread::_on_read(struct archive *a, const void **buff)
{
    _buf = _popQueue();
    *buff = _buf.c_str();
    return _buf.size();
}

int DownloadExtractThread::_on_close(struct archive *a)
{
    return 0;
}

/* static callback functions that call object oriented equivalents */
ssize_t DownloadExtractThread::_archive_read(struct archive *a, void *client_data, const void **buff)
{
   return static_cast<DownloadExtractThread *>(client_data)->_on_read(a, buff);
}

int DownloadExtractThread::_archive_close(struct archive *a, void *client_data)
{
    return static_cast<DownloadExtractThread *>(client_data)->_on_close(a);
}


/* Synchronized queue using monitor consumer/producer pattern */
std::string DownloadExtractThread::_popQueue()
{
    std::unique_lock<std::mutex> lock(_queueMutex);

    _cv.wait(lock, [this]{
            return _queue.size() != 0;
    });

    string result = _queue.front();
    _queue.pop_front();

    if (_queue.size() == (MAX_QUEUE_SIZE-1))
    {
        lock.unlock();
        _cv.notify_one();
    }

    return result;
}

void DownloadExtractThread::_pushQueue(const char *data, size_t len)
{
    std::unique_lock<std::mutex> lock(_queueMutex);

    _cv.wait(lock, [this]{
            return _queue.size() != MAX_QUEUE_SIZE;
    });

    _queue.emplace_back(data, len);
    string s(data, len);

    if (_queue.size() == 1)
    {
        lock.unlock();
        _cv.notify_one();
    }
}

void DownloadExtractThread::setPostInstallScript(const std::string &path)
{
    _postInstallScript = path;
}

void DownloadExtractThread::setLdapConfig(const std::string &config)
{
    _ldapConfig = config;
}
