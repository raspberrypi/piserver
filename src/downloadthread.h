#ifndef DOWNLOADTHREAD_H
#define DOWNLOADTHREAD_H

#include <string>
#include <thread>
#include <mutex>
#include <fstream>
#include <atomic>
#include <glibmm.h>
#include <time.h>
#include <curl/curl.h>

class DownloadThread
{
public:
    /*
     * Constructor
     *
     * - url: URL to download
     * - localfilename: File name to save downloaded file as. If empty, store data in memory buffer
     */
    explicit DownloadThread(const std::string &url, const std::string &localfilename = "");

    /*
     * Destructor
     *
     * Waits until download is complete
     * If this is not desired, call cancelDownload() first
     */
    virtual ~DownloadThread();

    /*
     * Cancel download
     *
     * Async function. Returns immedeately, but can take a second before download actually stops
     */
    virtual void cancelDownload();

    /*
     * Set proxy server.
     * Specify a string like this: user:pass@proxyserver:8080/
     * Used globally, for all connections
     */
    static void setProxy(const std::string &proxy);

    /*
     * Returns proxy server used
     */
    static std::string proxy();

    /*
     * Set user-agent header string
     */
    void setUserAgent(const std::string &ua);

    /*
     * Returns true if download has been successful
     */
    bool successfull();

    /*
     * Returns the downloaded data if saved to memory buffer instead of file
     */
    std::string data();

    /*
     * Delete downloaded file
     */
    void deleteDownloadedFile();

    /*
     * Return last-modified date (if available) as unix timestamp
     * (seconds since 1970)
     */
    time_t lastModified();

    /*
     * Return current server time as unix timestamp
     */
    time_t serverTime();

    /*
     * Cache result in local file
     */
    void setCacheFile(const std::string &filename);

    /*
     * Thread safe download progress query functions
     */
    uint64_t dlNow();
    uint64_t dltotal();
    std::string lastError();

    /*
     * Thread safe signals
     * Creator of the DownloadThread object must have a glib main loop
     */
    Glib::Dispatcher &signalSuccess();
    Glib::Dispatcher &signalError();

    /*
     * Thread start function
     */
    void start();

protected:
    virtual void run();
    virtual void _onDownloadSuccess();
    virtual void _onDownloadError(const std::string &msg);
    void _emitSuccess();
    void _emitError(const std::string &msg);

    /*
     * libcurl callbacks
     */
    virtual size_t _writeData(const char *buf, size_t len);
    bool _progress(curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);
    void _header(const std::string &header);

    static size_t _curl_write_callback(char *ptr, size_t size, size_t nmemb, void *userdata);
    static int _curl_xferinfo_callback(void *userdata, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);
    static size_t _curl_header_callback( void *ptr, size_t size, size_t nmemb, void *userdata);

    std::thread *_thread;
    CURL *_c;
    curl_off_t _startOffset;
    std::atomic<std::uint64_t> _lastDlTotal, _lastDlNow;
    std::mutex _errorMutex;
    std::string _url, _useragent, _buf, _cachefile, _filename, _lastError;
    static std::string _proxy;
    static int _curlCount;
    bool _cancelled, _successful;
    time_t _lastModified, _serverTime, _lastFailureTime;
    std::ofstream *_file;
    Glib::Dispatcher _signalSucess;
    Glib::Dispatcher _signalError;
};

#endif // DOWNLOADTHREAD_H
