#ifndef DEPENDENCIESINSTALLTHREAD_H
#define DEPENDENCIESINSTALLTHREAD_H

#include <string>
#include <thread>
#include <map>
#include <glibmm.h>
#include "piserver.h"

class DependenciesInstallThread
{
public:
    DependenciesInstallThread(PiServer *ps);
    virtual ~DependenciesInstallThread();

    /*
     * Thread start function
     */
    void start();

    /*
     * Thread safe signals
     * Creator of the DownloadThread object must have a glib main loop
     */
    Glib::Dispatcher &signalSuccess();
    Glib::Dispatcher &signalFailure();
    std::string error();

protected:
    virtual void run();
    void _execCheckResult(const std::string &cmd);
    void _preseed(const std::map<std::string,std::string> &values);

    PiServer *_ps;
    std::string _error;
    std::thread *_thread;
    Glib::Dispatcher _signalSucess, _signalFailure;
};

#endif // DEPENDENCIESINSTALLTHREAD_H
