#ifndef ADDUSERDIALOG_H
#define ADDUSERDIALOG_H

#include "piserver.h"
#include "abstractadduser.h"
#include <gtkmm.h>
#include <vector>

class AddUserDialog : private AbstractAddUser
{
public:
    AddUserDialog(PiServer *ps, Gtk::Window *parent);
    virtual ~AddUserDialog();
    bool exec();

protected:
    PiServer *_ps;
    Gtk::Window *_parentWindow;
    Gtk::Dialog *_dialog;
    Gtk::Button *_okbutton;

    virtual void setAddUserOkButton();
};

#endif // ADDUSERDIALOG_H
