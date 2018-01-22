#ifndef DOWNLOADEXTRACTTHREAD_H
#define DOWNLOADEXTRACTTHREAD_H

#include "downloadthread.h"
#include <deque>
#include <condition_variable>

class DownloadExtractThread : public DownloadThread
{
public:
    /*
     * Constructor
     *
     * - url: URL to download
     * - localfolder: Folder to extract archive to
     */
    explicit DownloadExtractThread(const std::string &url, const std::string &localfolder);

    virtual ~DownloadExtractThread();
    virtual void cancelDownload();
    void setPostInstallScript(const std::string &path);
    void setLdapConfig(const std::string &config);

protected:
    std::string _folder, _postInstallScript, _ldapConfig;
    std::thread *_extractThread;
    std::deque<std::string> _queue;
    static const int MAX_QUEUE_SIZE;
    std::mutex _queueMutex;
    std::condition_variable _cv;

    std::string _popQueue();
    void _pushQueue(const char *data, size_t len);
    void _cancelExtract();
    virtual size_t _writeData(const char *buf, size_t len);
    virtual void _onDownloadSuccess();
    virtual void _onDownloadError(const std::string &msg);
    virtual void extractRun();

    ssize_t _on_read(struct archive *a, const void **buff);
    int _on_close(struct archive *a);

    static ssize_t _archive_read(struct archive *a, void *client_data, const void **buff);
    static int _archive_close(struct archive *a, void *client_data);
};

#endif // DOWNLOADEXTRACTTHREAD_H
