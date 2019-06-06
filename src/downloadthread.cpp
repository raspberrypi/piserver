#include "downloadthread.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <utime.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

using namespace std;

string DownloadThread::_proxy;
int DownloadThread::_curlCount = 0;

DownloadThread::DownloadThread(const string &url, const string &localfilename) :
    _thread(NULL), _lastDlTotal(0), _lastDlNow(0), _url(url), _cancelled(false), _successful(false),
    _lastModified(0), _serverTime(0), _filename(localfilename), _file(NULL),
    _startOffset(0), _lastFailureTime(0)
{
    if (!_curlCount)
        curl_global_init(CURL_GLOBAL_DEFAULT);
    _curlCount++;

    if (!localfilename.empty())
        _file = new ofstream(localfilename, ios_base::out | ios_base::binary | ios_base::trunc);
}

DownloadThread::~DownloadThread()
{
    if (_thread)
    {
        _thread->join();
        delete _thread;
    }
    if (_file)
        delete _file;

    if (!--_curlCount)
        curl_global_cleanup();
}

void DownloadThread::start()
{
    _thread = new thread(&DownloadThread::run, this);
}

void DownloadThread::setProxy(const string &proxy)
{
    _proxy = proxy;
}

string DownloadThread::proxy()
{
    return _proxy;
}

void DownloadThread::setUserAgent(const string &ua)
{
    _useragent = ua;
}

/* Curl write callback function, let it call the object oriented version */
size_t DownloadThread::_curl_write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    return static_cast<DownloadThread *>(userdata)->_writeData(ptr, size * nmemb);
}

int DownloadThread::_curl_xferinfo_callback(void *userdata, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
    return (static_cast<DownloadThread *>(userdata)->_progress(dltotal, dlnow, ultotal, ulnow) == false);
}

size_t DownloadThread::_curl_header_callback( void *ptr, size_t size, size_t nmemb, void *userdata)
{
    int len = size*nmemb;
    string headerstr((char *) ptr, len);
    static_cast<DownloadThread *>(userdata)->_header(headerstr);

    return len;
}


void DownloadThread::run()
{
    char errorBuf[CURL_ERROR_SIZE] = {0};
    _c = curl_easy_init();
    curl_easy_setopt(_c, CURLOPT_NOSIGNAL, 1);
    curl_easy_setopt(_c, CURLOPT_WRITEFUNCTION, &DownloadThread::_curl_write_callback);
    curl_easy_setopt(_c, CURLOPT_WRITEDATA, this);
    curl_easy_setopt(_c, CURLOPT_XFERINFOFUNCTION, &DownloadThread::_curl_xferinfo_callback);
    curl_easy_setopt(_c, CURLOPT_PROGRESSDATA, this);
    curl_easy_setopt(_c, CURLOPT_NOPROGRESS, 0);
    curl_easy_setopt(_c, CURLOPT_URL, _url.c_str());
    curl_easy_setopt(_c, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(_c, CURLOPT_MAXREDIRS, 10);
    curl_easy_setopt(_c, CURLOPT_ERRORBUFFER, errorBuf);
    curl_easy_setopt(_c, CURLOPT_FAILONERROR, 1);
    curl_easy_setopt(_c, CURLOPT_HEADERFUNCTION, &DownloadThread::_curl_header_callback);
    curl_easy_setopt(_c, CURLOPT_HEADERDATA, this);

    if (!_useragent.empty())
        curl_easy_setopt(_c, CURLOPT_USERAGENT, _useragent.c_str());
    if (!_proxy.empty())
        curl_easy_setopt(_c, CURLOPT_PROXY, _proxy.c_str());
    if (!_file && !_cachefile.empty())
    {
        struct stat st;

        if (::stat(_cachefile.c_str(), &st) == 0 && st.st_size > 0)
        {
            curl_easy_setopt(_c, CURLOPT_TIMECONDITION, CURL_TIMECOND_IFMODSINCE);
            curl_easy_setopt(_c, CURLOPT_TIMEVALUE, st.st_mtim);
        }
    }

    CURLcode ret = curl_easy_perform(_c);

    /* Deal with badly configured HTTP servers that terminate the connection quickly
       if connections stalls for some seconds while kernel commits buffers to slow SD card */
    while (ret == CURLE_PARTIAL_FILE)
    {
        time_t t = time(NULL);

        /* If last failure happened less than 5 seconds ago, something else may
           be wrong. Sleep some time to prevent hammering server */
        if (t - _lastFailureTime < 5)
            ::sleep(10);
        _lastFailureTime = t;

        _startOffset = _lastDlNow;
        curl_easy_setopt(_c, CURLOPT_RESUME_FROM_LARGE, _startOffset);

        ret = curl_easy_perform(_c);
    }

    if (_file)
        _file->close();
    curl_easy_cleanup(_c);

    switch (ret)
    {
        case CURLE_OK:
            if (!_cachefile.empty())
            {
                int code = 0;
                curl_easy_getinfo(_c, CURLINFO_RESPONSE_CODE, &code);

                if (code == 304)
                {
                    ifstream i(_cachefile, ios_base::in | ios_base::binary);
                    stringstream buffer;
                    buffer << i.rdbuf();
                    _buf = buffer.str();
                }
                else if (_lastModified)
                {
                    ofstream f(_cachefile, ios_base::out | ios_base::trunc | ios_base::binary);
                    f.write(_buf.c_str(), _buf.size());
                    f.close();

                    struct timeval tvp[2];
                    tvp[0].tv_sec = 0;
                    tvp[0].tv_usec = 0;
                    tvp[1].tv_sec = _lastModified;
                    tvp[1].tv_usec = 0;
                    utimes(_cachefile.c_str(), tvp);
                }
            }
            _successful = true;
            _onDownloadSuccess();
            break;
        case CURLE_WRITE_ERROR:
            deleteDownloadedFile();
            _onDownloadError("Error writing file to disk");
            break;
        case CURLE_ABORTED_BY_CALLBACK:
            break;
        default:
            deleteDownloadedFile();
            _onDownloadError(errorBuf);
    }
}

size_t DownloadThread::_writeData(const char *buf, size_t len)
{
    if (_file)
    {
        _file->write(buf, len);
        return _file->fail() ? 0 : len;
    }
    else
    {
        _buf.append(buf, len);
        return len;
    }
}

bool DownloadThread::_progress(curl_off_t dltotal, curl_off_t dlnow, curl_off_t /*ultotal*/, curl_off_t /*ulnow*/)
{
    _lastDlTotal = _startOffset + dltotal;
    _lastDlNow   = _startOffset + dlnow;

    return !_cancelled;
}

void DownloadThread::_header(const string &header)
{
    if (header.compare(0, 6, "Date: ") == 0)
    {
        _serverTime = curl_getdate(header.data()+6, NULL);
    }
    else if (header.compare(0, 15, "Last-Modified: ") == 0)
    {
        _lastModified = curl_getdate(header.data()+15, NULL);
    }
}

void DownloadThread::cancelDownload()
{
    _cancelled = true;
    deleteDownloadedFile();
}

string DownloadThread::data()
{
    return _buf;
}

bool DownloadThread::successfull()
{
    return _successful;
}

time_t DownloadThread::lastModified()
{
    return _lastModified;
}

time_t DownloadThread::serverTime()
{
    return _serverTime;
}

void DownloadThread::deleteDownloadedFile()
{
    if (_file)
    {
        _file->close();
        ::unlink(_filename.c_str());
    }
}

void DownloadThread::setCacheFile(const string &filename)
{
    _cachefile = filename;
}

uint64_t DownloadThread::dlNow()
{
    return _lastDlNow;
}

uint64_t DownloadThread::dltotal()
{
    return _lastDlTotal;
}

string DownloadThread::lastError()
{
    lock_guard<std::mutex> lock(_errorMutex);
    string err = _lastError;

    return err;
}

void DownloadThread::_onDownloadSuccess()
{
    _emitSuccess();
}

void DownloadThread::_onDownloadError(const std::string &msg)
{
    _emitError(msg);
}


void DownloadThread::_emitSuccess()
{
    _signalSucess.emit();
}

void DownloadThread::_emitError(const std::string &msg)
{
    lock_guard<std::mutex> lock(_errorMutex);
    _lastError = msg;
    _signalError.emit();
}

Glib::Dispatcher &DownloadThread::signalSuccess()
{
    return _signalSucess;
}

Glib::Dispatcher &DownloadThread::signalError()
{
    return _signalError;
}
