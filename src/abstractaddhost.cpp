#include "abstractaddhost.h"
#include "distribution.h"

using namespace std;

AbstractAddHost::AbstractAddHost(PiServer *ps)
    : _ps(ps)
{
}

AbstractAddHost::~AbstractAddHost()
{
}

void AbstractAddHost::setupAddHostFields(Glib::RefPtr<Gtk::Builder> builder)
{
    builder->get_widget("hosttree", _hosttree);
    _hoststore = Glib::RefPtr<Gtk::ListStore>::cast_dynamic( builder->get_object("hoststore") );
    _hoststore->clear();

    _watcher.signal_macDetected().connect( sigc::mem_fun(this, &AbstractAddHost::on_macDetected) );
    dynamic_cast<Gtk::CellRendererToggle *>(_hosttree->get_column_cell_renderer(0))->signal_toggled().connect(
                sigc::mem_fun(this, &AbstractAddHost::on_hostToggled) );
    _stpdetect.signal_detected().connect( sigc::mem_fun(this, &AbstractAddHost::on_stpDetected) );

    try
    {
        _watcher.startListening();
    }
    catch (std::exception)
    {
        Gtk::MessageDialog d(_("Error listening for DHCP traffic.\n"
                               "Uninstall any other DHCP servers running on this computer."));
        d.run();
    }

    try
    {
        std::string ifname = _ps->getSetting("interface");
        if (ifname.empty())
        {
            map<string,string> ifaces = _ps->getInterfaces();
            for (auto kv : ifaces)
            {
                if (kv.first != "lo")
                {
                    ifname = kv.first;
                    break;
                }
            }
        }

        if (!ifname.empty())
        {
            _stpdetect.startListening(ifname);
        }
    }
    catch (const std::exception &e)
    {
        Gtk::MessageDialog d( e.what() );
    }
}

void AbstractAddHost::on_macDetected(std::string mac)
{
    if (!_ps->getHosts()->count(mac))
    {
        auto row = _hoststore->append();
        row->set_value(0, true);
        row->set_value(1, mac);
        setAddHostOkButton();
    }
}

bool AbstractAddHost::addHostFieldsOk()
{
    int selected = 0;

    for (auto &row : _hoststore->children() )
    {
        bool checked;
        row->get_value(0, checked);
        if (checked)
            selected++;
    }

    return selected > 0;
}

void AbstractAddHost::on_hostToggled(const Glib::ustring& path)
{
    auto iter = _hoststore->get_iter(path);
    bool checked;
    iter->get_value(0, checked);
    iter->set_value(0, !checked);
    setAddHostOkButton();
}

void AbstractAddHost::addHosts(Distribution *distro, const string &description)
{
    set<string> macs;

    for (auto &row : _hoststore->children() )
    {
        bool checked;
        string mac;

        row->get_value(0, checked);
        if (checked)
        {
            row->get_value(1, mac);
            macs.insert(mac);
        }
    }

    _ps->addHosts(macs, distro, description);
}

void AbstractAddHost::on_stpDetected(std::string info)
{
    Gtk::MessageDialog d(Glib::ustring::compose(_(
                           "STP seems to be enabled on your switch, which\n"
                           "can conflict with network booting.\n"
                           "Ask your network administrator to either disable it completely\n"
                           "on ports connecting Pi, or change it to the portfast/rapid variant.\n\n"
                           "(%1)"), info));
    d.run();
}
