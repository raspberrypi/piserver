#ifndef EXPORTDISTRODIALOG_H
#define EXPORTDISTRODIALOG_H

#include <gtkmm.h>
#include <string>

class PiServer;
class Distribution;

class ExportDistroDialog
{
public:
    ExportDistroDialog(PiServer *ps, Distribution *olddistro, Gtk::Window *parent);
    virtual ~ExportDistroDialog();
    bool exec();

protected:
    PiServer *_ps;
    Distribution *_distro;
    Gtk::Dialog *_progressDialog;
    GPid _pid;
    Gtk::Window *_parent;

    void onTarFinished(GPid pid, int status);
};

#endif // EXPORTDISTRODIALOG_H
