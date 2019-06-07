#ifndef EDITUSERDIALOG_H
#define EDITUSERDIALOG_H

#include "piserver.h"
#include <gtkmm.h>

class EditUserDialog
{
public:
    EditUserDialog(PiServer *ps, const std::string &dn, const std::string &user, const std::string &desc, Gtk::Window *parent = NULL);
    virtual ~EditUserDialog();
    bool exec();
    std::string description();

protected:
    PiServer *_ps;
    Gtk::Window *_parentWindow;
    std::string _dn, _user, _oldDesc;
    Gtk::Dialog *_dialog;
    Gtk::Entry *_passEntry, *_pass2Entry, *_descEntry;
    Gtk::Button *_okButton;
    Gtk::CheckButton *_showPassCheck;

    void setOkButton();
    void on_showPassToggled();
};

#endif // EDITUSERDIALOG_H
