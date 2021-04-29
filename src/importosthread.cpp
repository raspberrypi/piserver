#include "importosthread.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <ctype.h>
#include <filesystem>
#include <sys/mount.h>
#include "config.h"

ImportOSThread::ImportOSThread(const std::string &device, const std::string &localfolder)
    : DownloadExtractThread("", localfolder), _device(device)
{
    _lastDlTotal = 100;
}

void ImportOSThread::run()
{
    /* Step 1: mounting SD card */
    std::string bootdev, rootdev;
    std::string mntdir = PISERVER_DATADIR "/mnt";
    std::string bootmntdir = mntdir+"/boot";

    if ( isdigit(_device.back()) )
    {
        bootdev = _device+"p1";
        rootdev = _device+"p2";
    }
    else
    {
        bootdev = _device+"1";
        rootdev = _device+"2";
    }

    if (!std::filesystem::exists(bootdev))
    {
        _emitError(std::string("Boot (FAT) partition ")+bootdev+" does not exist");
        return;
    }
    if (!std::filesystem::exists(rootdev))
    {
        _emitError(std::string("Root (ext4) partition ")+rootdev+" does not exist");
        return;
    }

    if (::mkdir(mntdir.c_str(), 0755) == -1) { /* Ignore errors. Folder may already exist */ }

    if (::mount(rootdev.c_str(), mntdir.c_str(), "ext4", MS_RDONLY, "") != 0
            && ::mount(rootdev.c_str(), mntdir.c_str(), "ext4", 0, "") != 0)
    {
        _emitError(std::string("Error mounting ")+rootdev+" as ext4: "+strerror(errno) );
        return;
    }

    if (!std::filesystem::exists(bootmntdir))
    {
        ::umount(mntdir.c_str() );
        _emitError("Error: no /boot directory inside ext4 file system");
        return;
    }

    if (::mount(bootdev.c_str(), bootmntdir.c_str(), "vfat", MS_RDONLY, "") != 0
            && ::mount(bootdev.c_str(), bootmntdir.c_str(), "vfat", 0, "") != 0)
    {
        ::umount(mntdir.c_str() );
        _emitError(std::string("Error mounting ")+bootdev+" as vfat: "+strerror(errno) );
        return;
    }

    /* Step 2: copying files */
    _lastDlNow = 25;

    std::string cmd = std::string("cp -a ")+mntdir+"/* "+_folder;
    int retcode = ::system(cmd.c_str() );
    ::umount(bootmntdir.c_str() );
    ::umount(mntdir.c_str() );

    if (retcode != 0)
    {
        _emitError("Error copying files from SD card");
        return;
    }

    /* Step 3: run image conversion script */
    _lastDlNow = 50;
    cmd = std::string(PISERVER_CONVERTSCRIPT)+" "+_folder;
    if (::system(cmd.c_str() ) != 0)
    {
        _emitError("Error running convert script");
        return;
    }

    /* Step 4: normal post-install script */
    _lastDlNow = 75;

    if (!_postInstallScript.empty())
    {
        ::setenv("DISTROROOT", _folder.c_str(), 1);
        if (!_ldapConfig.empty())
            ::setenv("EXTERNAL_LDAP_CONFIG", _ldapConfig.c_str(), 1);
        if (::system(_postInstallScript.c_str()) != 0)
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

    _lastDlNow = 100;
}
