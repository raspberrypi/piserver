#ifndef ABSTRACTADDHOST_H
#define ABSTRACTADDHOST_H

#include <gtkmm.h>
#include "dhcpanalyzer.h"
#include "piserver.h"

class AbstractAddHost : public sigc::trackable
{
protected:
    AbstractAddHost(PiServer *ps);
    virtual ~AbstractAddHost();
    void setupAddHostFields(Glib::RefPtr<Gtk::Builder> builder);
    bool addHostFieldsOk();
    void addHosts(Distribution *distro, const std::string &description = "");
    virtual void setAddHostOkButton() = 0;

    /* Slots */
    void on_macDetected(std::string mac);
    void on_hostToggled(const Glib::ustring& path);

    Gtk::TreeView *_hosttree;
    Glib::RefPtr<Gtk::ListStore> _hoststore;
    DhcpAnalyzer _watcher;

private:
    PiServer *_ps;
};

#endif // ABSTRACTADDHOST_H
