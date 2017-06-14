#ifndef ADDHOSTWINDOW_H
#define ADDHOSTWINDOW_H

#include "piserver.h"
#include "abstractaddhost.h"
#include <gtkmm.h>

class AddHostDialog : private AbstractAddHost
{
public:
    AddHostDialog(PiServer *ps, Gtk::Window *parent = NULL);
    virtual ~AddHostDialog();
    bool exec();

protected:
    PiServer *_ps;
    Gtk::Dialog *_dialog;
    Gtk::Button *_okbutton;
    Gtk::ComboBox *_oscombo;
    Gtk::Entry *_descriptionentry;
    Glib::RefPtr<Gtk::ListStore> _distrostore;

    virtual void setAddHostOkButton();
};

#endif // ADDHOSTWINDOW_H
