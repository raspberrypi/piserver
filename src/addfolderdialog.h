#ifndef ADDFOLDERDIALOG_H
#define ADDFOLDERDIALOG_H

#include <gtkmm.h>
#include <string>

class PiServer;

class AddFolderDialog
{
public:
    AddFolderDialog(PiServer *ps, Gtk::Window *parent);
    virtual ~AddFolderDialog();
    bool exec();

protected:
    PiServer *_ps;
    Gtk::Dialog *_dialog;
    Gtk::Button *_okButton;
    Gtk::Entry *_nameEntry, *_ownerEntry;

    bool isNameValid(const std::string &name);
    void setOkSensitive();

};

#endif // ADDFOLDERDIALOG_H
