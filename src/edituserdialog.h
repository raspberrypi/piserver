#ifndef EDITUSERDIALOG_H
#define EDITUSERDIALOG_H

#include "piserver.h"
#include <gtkmm.h>

class EditUserDialog
{
public:
    EditUserDialog(PiServer *ps, const std::string &user, Gtk::Window *parent = NULL);
    virtual ~EditUserDialog();
    bool exec();

protected:
    PiServer *_ps;
    Gtk::Window *_parentWindow;
    std::string _user;
    Gtk::Dialog *_dialog;
    Gtk::Entry *_passEntry, *_pass2Entry;
    Gtk::Button *_okButton;
    Gtk::CheckButton *_showPassCheck;

    void setOkButton();
    void on_showPassToggled();
};

#endif // EDITUSERDIALOG_H
