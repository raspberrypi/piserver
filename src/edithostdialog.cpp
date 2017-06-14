#include "edithostdialog.h"
#include "host.h"
#include "distribution.h"
#include <iostream>

using namespace std;

EditHostDialog::EditHostDialog(PiServer *ps, const std::string &mac, Gtk::Window *parent)
    : _ps(ps), _mac(mac)
{
    auto builder = Gtk::Builder::create_from_resource("/data/edithostdialog.glade");
    builder->get_widget("edithostdialog", _dialog);
    builder->get_widget("maclabel", _maclabel);
    builder->get_widget("oscombo", _oscombo);
    builder->get_widget("descriptionentry", _descriptionentry);
    _distrostore = Glib::RefPtr<Gtk::ListStore>::cast_dynamic( builder->get_object("distrostore") );
    if (parent)
        _dialog->set_transient_for(*parent);

    Host *h = ps->getHost(mac);
    _maclabel->set_text(mac);
    _descriptionentry->set_text(h->description());
    _distrostore->clear();
    auto distros = _ps->getDistributions();

    for (auto &kv : *distros)
    {
        auto row = _distrostore->append();
        row->set_value(0, kv.second->name());

        if (kv.second == h->distro())
        {
            _oscombo->set_active(row);
        }
    }

    if (!h->distro() && !distros->empty())
        _oscombo->set_active(0);

    _descriptionentry->grab_focus();
}

EditHostDialog::~EditHostDialog()
{
    delete _dialog;
}

bool EditHostDialog::exec()
{
    if (_dialog->run() == Gtk::RESPONSE_OK)
    {
        string distroname;
        string description = _descriptionentry->get_text();
        _oscombo->get_active()->get_value(0, distroname);
        Distribution *distro = _ps->getDistributions()->at(distroname);

        Host *h = _ps->getHost(_mac);
        if (!h)
            return false; /* Host doesn't exist anymore */

        h->setDescription(description);
        h->setDistro(distro);
        _ps->updateHost(h);

        return true;
    }

    return false;
}
