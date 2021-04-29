#ifndef IMPORTOSTHREAD_H
#define IMPORTOSTHREAD_H

#include "downloadextractthread.h"

class ImportOSThread : public DownloadExtractThread
{
public:
    ImportOSThread(const std::string &device, const std::string &localfolder);

protected:
    std::string _device;

    virtual void run();
};

#endif // IMPORTOSTHREAD_H
