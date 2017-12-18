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
    bool importCSV(const std::string &filename);

protected:
    PiServer *_ps;
    Gtk::Window *_parentWindow;
    Gtk::Dialog *_dialog;
    Gtk::Button *_okbutton;
    Gtk::Grid *_grid;

    virtual void setAddUserOkButton();
};

#endif // ADDUSERDIALOG_H
