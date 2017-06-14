#include "addhostdialog.h"
#include "distribution.h"
#include "host.h"
#include <iostream>
#include <set>

using namespace std;

AddHostDialog::AddHostDialog(PiServer *ps, Gtk::Window *parent)
    : AbstractAddHost(ps), _ps(ps)
{
    auto builder = Gtk::Builder::create_from_resource("/data/addhostdialog.glade");
    setupAddHostFields(builder);
    builder->get_widget("addhostdialog", _dialog);
    builder->get_widget("okbutton", _okbutton);
    builder->get_widget("oscombo", _oscombo);
    builder->get_widget("descriptionentry", _descriptionentry);
    _distrostore = Glib::RefPtr<Gtk::ListStore>::cast_dynamic( builder->get_object("distrostore") );
    if (parent)
        _dialog->set_transient_for(*parent);

    _distrostore->clear();
    auto distros = _ps->getDistributions();

    for (auto &kv : *distros)
    {
        auto row = _distrostore->append();
        row->set_value(0, kv.second->name());
    }
    if (!distros->empty())
        _oscombo->set_active(0);

    _descriptionentry->grab_focus();
}

AddHostDialog::~AddHostDialog()
{
    delete _dialog;
}

bool AddHostDialog::exec()
{
    if (_dialog->run() == Gtk::RESPONSE_OK)
    {
        string distroname;
        string description = _descriptionentry->get_text();
        _oscombo->get_active()->get_value(0, distroname);
        Distribution *distro = _ps->getDistributions()->at(distroname);

        addHosts(distro, description);

        return true;
    }

    return false;
}

void AddHostDialog::setAddHostOkButton()
{
    _okbutton->set_sensitive( addHostFieldsOk() );
}
