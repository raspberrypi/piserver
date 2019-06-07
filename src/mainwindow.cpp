#include "mainwindow.h"
#include "distribution.h"
#include "host.h"
#include "addhostdialog.h"
#include "edithostdialog.h"
#include "adduserdialog.h"
#include "edituserdialog.h"
#include "downloadthread.h"
#include "adddistrodialog.h"
#include "clonedistrodialog.h"
#include "dhcpclient.h"
#include "exportdistrodialog.h"
#include "addfolderdialog.h"
#include <arpa/inet.h>
#include <sys/types.h>
#include <dirent.h>
#include <pwd.h>
#include <iostream>
#include <regex>

using namespace std;
using nlohmann::json;

MainWindow::MainWindow(Glib::RefPtr<Gtk::Application> app, PiServer *ps)
    : _app(app), _ps(ps), _dt(NULL)
{
    auto builder = Gtk::Builder::create_from_resource("/data/piserver.glade");
    builder->get_widget("maindialog", _window);
    builder->get_widget("notebook", _notebook);
    builder->get_widget("distrotree", _distrotree);
    builder->get_widget("hosttree", _hosttree);
    builder->get_widget("usertree", _usertree);
    builder->get_widget("folderstree", _folderstree);
    builder->get_widget("usersearchentry", _usersearchentry);
    builder->get_widget("ifacecombo", _ifacecombo);
    builder->get_widget("startipentry", _startipentry);
    builder->get_widget("endipentry", _endipentry);
    builder->get_widget("netmaskentry", _netmaskentry);
    builder->get_widget("gatewayentry", _gatewayentry);
    builder->get_widget("proxydhcpradio", _proxydhcpradio);
    builder->get_widget("standaloneradio", _standaloneradio);
    builder->get_widget("dhcpserverframe", _dhcpserverframe);
    builder->get_widget("savesettingsbutton", _savesettingsbutton);
    builder->get_widget("softwarelabel", _softwarelabel);

    _distrostore = Glib::RefPtr<Gtk::ListStore>::cast_dynamic( builder->get_object("osstore") );
    _userstore = Glib::RefPtr<Gtk::ListStore>::cast_dynamic( builder->get_object("userstore") );
    _hoststore = Glib::RefPtr<Gtk::ListStore>::cast_dynamic( builder->get_object("pistore") );
    _ethstore = Glib::RefPtr<Gtk::ListStore>::cast_dynamic( builder->get_object("ethstore") );
    _folderstore = Glib::RefPtr<Gtk::ListStore>::cast_dynamic( builder->get_object("folderstore") );

    _usersearchentry->signal_changed().connect( sigc::mem_fun(this, &MainWindow::_reloadUsers) );
    _hosttree->signal_row_activated().connect( sigc::mem_fun(this, &MainWindow::on_hosttree_activated) );
    _distrotree->signal_row_activated().connect( sigc::mem_fun(this, &MainWindow::on_distrotree_activated) );
    _usertree->signal_row_activated().connect( sigc::mem_fun(this, &MainWindow::on_usertree_activated) );
    _folderstree->signal_row_activated().connect( sigc::mem_fun(this, &MainWindow::on_foldertree_activated) );

    Gtk::ToolButton *button;
    builder->get_widget("adduserbutton", button);
    if (_ps->externalServer())
        button->set_sensitive(false);
    else
        button->signal_clicked().connect( sigc::mem_fun(this, &MainWindow::on_adduser_clicked) );
    builder->get_widget("addhostbutton", button);
    button->signal_clicked().connect( sigc::mem_fun(this, &MainWindow::on_addhost_clicked) );
    builder->get_widget("addosbutton", button);
    button->signal_clicked().connect( sigc::mem_fun(this, &MainWindow::on_addos_clicked) );
    builder->get_widget("addfolderbutton", button);
    button->signal_clicked().connect( sigc::mem_fun(this, &MainWindow::on_addfolder_clicked) );
    builder->get_widget("importusersbutton", button);
    if (_ps->externalServer())
        button->set_sensitive(false);
    else
        button->signal_clicked().connect( sigc::mem_fun(this, &MainWindow::on_importusers_clicked) );
    builder->get_widget("edituserbutton", _edituserbutton);
    _edituserbutton->signal_clicked().connect( sigc::mem_fun(this, &MainWindow::on_edituser_clicked) );
    builder->get_widget("removeuserbutton", _deluserbutton);
    _deluserbutton->signal_clicked().connect( sigc::mem_fun(this, &MainWindow::on_deluser_clicked) );
    builder->get_widget("edithostbutton", _edithostbutton);
    _edithostbutton->signal_clicked().connect( sigc::mem_fun(this, &MainWindow::on_edithost_clicked) );
    builder->get_widget("removehostbutton", _delhostbutton);
    _delhostbutton->signal_clicked().connect( sigc::mem_fun(this, &MainWindow::on_delhost_clicked) );
    builder->get_widget("upgradeosbutton", _upgradeosbutton);
    _upgradeosbutton->signal_clicked().connect( sigc::mem_fun(this, &MainWindow::on_upgradeos_clicked) );
    builder->get_widget("cloneosbutton", _cloneosbutton);
    _cloneosbutton->signal_clicked().connect( sigc::mem_fun(this, &MainWindow::on_cloneos_clicked) );
    builder->get_widget("exportosbutton", _exportosbutton);
    _exportosbutton->signal_clicked().connect( sigc::mem_fun(this, &MainWindow::on_exportos_clicked) );
    builder->get_widget("removeosbutton", _delosbutton);
    _delosbutton->signal_clicked().connect( sigc::mem_fun(this, &MainWindow::on_delos_clicked) );
    builder->get_widget("shellbutton", _shellbutton);
    _shellbutton->signal_clicked().connect( sigc::mem_fun(this, &MainWindow::on_shell_clicked) );
    builder->get_widget("removefolderbutton", _delfolderbutton);
    _delfolderbutton->signal_clicked().connect( sigc::mem_fun(this, &MainWindow::on_delfolder_clicked) );
    builder->get_widget("openfolderbutton", _openfolderbutton);
    _openfolderbutton->signal_clicked().connect( sigc::mem_fun(this, &MainWindow::on_openfolder_clicked) );

    _ethstore->clear();
    map<string,string> ifaces = _ps->getInterfaces();

    for (auto kv : ifaces)
    {
        /* exclude loopback interface */
        if (kv.first == "lo")
            continue;

        auto row = _ethstore->append();
        row->set_value(0, kv.first);
        row->set_value(1, kv.first+" ("+kv.second+")");
    }

    string ifaceSetting = _ps->getSetting("interface");
    if (!ifaceSetting.empty())
        _ifacecombo->set_active_id(ifaceSetting);
    string startIpSuggestion, endIpSuggestion;
    string currentIP = _ps->currentIP();
    if (!currentIP.empty())
    {
        string ip3 = currentIP.substr(0, currentIP.rfind('.')+1);
        startIpSuggestion = ip3+"50";
        endIpSuggestion = ip3+"150";
    }

    _startipentry->set_text( _ps->getSetting("startIP", startIpSuggestion) );
    _endipentry->set_text( _ps->getSetting("endIP", endIpSuggestion) );
    _netmaskentry->set_text( _ps->getSetting("netmask", "255.255.255.0") );
    _gatewayentry->set_text( _ps->getSetting("gateway") );
    _standaloneradio->set_active( _ps->getSetting("standaloneDhcpServer", 0) );

    _startipentry->signal_changed().connect(sigc::mem_fun(this, &MainWindow::_setSettingsSensitive));
    _endipentry->signal_changed().connect(sigc::mem_fun(this, &MainWindow::_setSettingsSensitive));
    _netmaskentry->signal_changed().connect(sigc::mem_fun(this, &MainWindow::_setSettingsSensitive));
    _gatewayentry->signal_changed().connect(sigc::mem_fun(this, &MainWindow::_setSettingsSensitive));
    _ifacecombo->signal_changed().connect(sigc::mem_fun(this, &MainWindow::_setSettingsSensitive));
    _standaloneradio->signal_clicked().connect(sigc::mem_fun(this, &MainWindow::_setSettingsSensitive));
    _savesettingsbutton->signal_clicked().connect( sigc::mem_fun(this, &MainWindow::on_savesettings_clicked));
    _setSettingsSensitive();

    _reloadDistro();
    _reloadHosts();
    _reloadUsers();
    _reloadFolders();
    _usersearchentry->grab_focus();

    _dt = new DownloadThread(PISERVER_REPO_URL);
    //_dt->setCacheFile(PISERVER_REPO_CACHEFILE);
    _dt->signalSuccess().connect( sigc::mem_fun(this, &MainWindow::onDownloadSuccessful) );
    _dt->signalError().connect( sigc::mem_fun(this, &MainWindow::onDownloadFailed) );
    _dt->start();
}

MainWindow::~MainWindow()
{
    delete _window;
    if (_dt)
    {
        _dt->cancelDownload();
        delete _dt;
    }
}

void MainWindow::exec()
{
    _app->run(*_window);
}

void MainWindow::_reloadDistro()
{
    _distrostore->clear();
    _upgradeosbutton->set_sensitive(false);
    _cloneosbutton->set_sensitive(false);
    _exportosbutton->set_sensitive(false);
    _delosbutton->set_sensitive(false);
    _shellbutton->set_sensitive(false);
    auto distros = _ps->getDistributions();

    for (auto &kv : *distros)
    {
        Distribution *d = kv.second;
        auto row = _distrostore->append();
        row->set_value(0, d->name());
        row->set_value(1, d->version());
        row->set_value(2, d->latestVersion());
    }
}

void MainWindow::_reloadHosts()
{
    _hoststore->clear();
    _edithostbutton->set_sensitive(false);
    _delhostbutton->set_sensitive(false);
    auto hosts = _ps->getHosts();

    for (auto &kv : *hosts)
    {
        Host *h = kv.second;
        auto row = _hoststore->append();
        row->set_value(0, h->mac());
        row->set_value(1, h->description());
        if (h->distro())
        {
            row->set_value(2, h->distro()->name());
        }
    }
}

void MainWindow::_reloadUsers()
{
    _userstore->clear();
    _edituserbutton->set_sensitive(false);
    _deluserbutton->set_sensitive(false);

    try
    {
        auto users = _ps->searchUsernames(_usersearchentry->get_text());

        for (const auto &kv : users)
        {
            const User &u = kv.second;
            auto row = _userstore->append();
            row->set_value(0, u.name());
            row->set_value(1, u.description());
            row->set_value(2, u.lastlogin());
            row->set_value(3, u.dn());
        }
    }
    catch (exception &e)
    {
        Gtk::MessageDialog d(e.what());
        d.set_transient_for(*_window);
        d.run();
    }
}

void MainWindow::on_adduser_clicked()
{
    AddUserDialog d(_ps, _window);
    if ( d.exec() )
        _reloadUsers();
}

void MainWindow::on_importusers_clicked()
{
    Gtk::FileChooserDialog fd(*_window, _("Please choose a .CSV file to import"));
    auto filter = Gtk::FileFilter::create();
    filter->set_name("CSV files");
    filter->add_mime_type("text/csv");
    fd.add_filter(filter);
    fd.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
    fd.add_button(Gtk::Stock::OPEN, Gtk::RESPONSE_OK);

    if (fd.run() != Gtk::RESPONSE_OK)
        return;

    fd.hide();
    AddUserDialog d(_ps, _window);
    if ( d.importCSV(fd.get_filename()) && d.exec() )
        _reloadUsers();
}

void MainWindow::on_edituser_clicked()
{
    string dn, user, desc;
    auto iter = _usertree->get_selection()->get_selected();
    iter->get_value(0, user);
    iter->get_value(1, desc);
    iter->get_value(3, dn);
    if (!user.empty())
    {
        EditUserDialog d(_ps, dn, user, desc, _window);
        if (d.exec())
        {
            iter->set_value(1, d.description());
        }
    }
}

void MainWindow::on_deluser_clicked()
{
    string dn, user;
    auto iter = _usertree->get_selection()->get_selected();
    iter->get_value(0, user);
    iter->get_value(3, dn);
    if (!user.empty())
    {
        Gtk::MessageDialog d(_("Are you sure you want to delete this user and the files in their home directory?"),
                             false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO);
        d.set_transient_for(*_window);
        if (d.run() == Gtk::RESPONSE_YES)
        {
            try
            {
                _ps->deleteUser(dn, user);
            }
            catch (exception &e)
            {
                Gtk::MessageDialog ed(e.what());
                ed.set_transient_for(*_window);
                ed.run();
            }
            _reloadUsers();
        }
    }
}

void MainWindow::on_addhost_clicked()
{
    if (_ps->getDistributions()->empty() )
    {
        /* Shouldn't happen if user made it through the first-use wizard */
        Gtk::MessageDialog d(_("Please install a Linux distribution first under 'Software'"));
        d.set_transient_for(*_window);
        d.run();
    }
    else
    {
        AddHostDialog d(_ps, _window);
        if (d.exec())
            _reloadHosts();
    }
}

void MainWindow::on_edithost_clicked()
{
    string mac;
    auto iter = _hosttree->get_selection()->get_selected();
    iter->get_value(0, mac);
    if (!mac.empty())
    {
        EditHostDialog d(_ps, mac, _window);
        if (d.exec())
            _reloadHosts();
    }
}

void MainWindow::on_delhost_clicked()
{
    string mac;
    auto iter = _hosttree->get_selection()->get_selected();
    iter->get_value(0, mac);
    if (!mac.empty())
    {
        _ps->deleteHost(_ps->getHost(mac));
        _reloadHosts();
    }
}

void MainWindow::on_addos_clicked()
{
    auto ad = new AddDistroDialog(this, _ps, _cachedDistroInfo, _window);
    ad->show();
}

void MainWindow::on_upgradeos_clicked()
{
    string distro;
    auto iter = _distrotree->get_selection()->get_selected();
    iter->get_value(0, distro);
    if (!distro.empty())
    {
        auto ad = new AddDistroDialog(this, _ps, _cachedDistroInfo, _window);
        ad->selectDistro(distro);
        ad->show();
    }
}

void MainWindow::on_delos_clicked()
{
    string distro;
    auto iter = _distrotree->get_selection()->get_selected();
    iter->get_value(0, distro);
    if (!distro.empty())
    {
        Gtk::MessageDialog d(_("Are you sure you want to delete this operating system?"),
                             false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO);
        d.set_transient_for(*_window);
        if (d.run() == Gtk::RESPONSE_YES)
        {
            try
            {
                _ps->deleteDistribution(distro);
            }
            catch (exception &e)
            {
                Gtk::MessageDialog ed(e.what());
                ed.set_transient_for(*_window);
                ed.run();
            }
            _reloadDistro();
        }
    }
}

void MainWindow::on_cloneos_clicked()
{
    string distroname;
    auto iter = _distrotree->get_selection()->get_selected();
    iter->get_value(0, distroname);
    if (!distroname.empty())
    {
        Distribution *distro = _ps->getDistributions()->at(distroname);
        CloneDistroDialog d(_ps, distro, _window);
        if (d.exec())
            _reloadDistro();
    }
}

void MainWindow::on_exportos_clicked()
{
    string distroname;
    auto iter = _distrotree->get_selection()->get_selected();
    iter->get_value(0, distroname);
    if (!distroname.empty())
    {
        Distribution *distro = _ps->getDistributions()->at(distroname);
        ExportDistroDialog d(_ps, distro, _window);
        d.exec();
    }
}

void MainWindow::on_shell_clicked()
{
    string distro;
    auto iter = _distrotree->get_selection()->get_selected();
    iter->get_value(0, distro);
    if (!distro.empty())
    {
        _ps->startChrootTerminal(distro);
    }
}

void MainWindow::on_hosttree_activated(const Gtk::TreeModel::Path &, Gtk::TreeViewColumn *)
{
    _edithostbutton->set_sensitive();
    _delhostbutton->set_sensitive();
}

void MainWindow::on_distrotree_activated(const Gtk::TreeModel::Path &path, Gtk::TreeViewColumn *)
{
    auto iter = _distrostore->get_iter(path);
    string curVersion, newVersion;
    iter->get_value(1, curVersion);
    iter->get_value(2, newVersion);
    _upgradeosbutton->set_sensitive(curVersion != newVersion && !newVersion.empty());
    _cloneosbutton->set_sensitive();
    _exportosbutton->set_sensitive();
    _delosbutton->set_sensitive();
    _shellbutton->set_sensitive();
}

void MainWindow::on_usertree_activated(const Gtk::TreeModel::Path &, Gtk::TreeViewColumn *)
{
    if (!_ps->externalServer())
    {
        _edituserbutton->set_sensitive();
        _deluserbutton->set_sensitive();
    }
}

void MainWindow::onDownloadSuccessful()
{
    int updates = 0;

    try
    {
        auto j = json::parse(_dt->data());
        auto oslist = j.at("os_list");

        for (auto &entry : oslist)
        {
            string name = entry.at("os_name").get<string>();
            string latestversion = entry.at("release_date").get<string>();

            if (_ps->getDistributions()->count(name))
            {
                Distribution *d = _ps->getDistributions()->at(name);
                if (d->version() != latestversion)
                    updates++;
                d->setLatestVersion(latestversion);
            }
        }

        _reloadDistro();
        _cachedDistroInfo = _dt->data();

        if (updates)
        {
            Gtk::Popover *pop = new Gtk::Popover(*_softwarelabel);
            pop->signal_button_press_event().connect( sigc::bind<Gtk::Widget *>(sigc::mem_fun(this, &MainWindow::on_popup_button), pop));
            _notebook->signal_button_press_event().connect( sigc::bind<Gtk::Widget *>(sigc::mem_fun(this, &MainWindow::on_popup_button), pop));
            pop->add_label(_("Update available"));
            pop->set_position(Gtk::POS_BOTTOM);
            pop->set_modal(false);
            pop->show();
        }
    }
    catch (exception &e)
    { }

    delete _dt;
    _dt = NULL;
}

bool MainWindow::on_popup_button(GdkEventButton*, Gtk::Widget *sender)
{
    delete sender;
    return true;
}

void MainWindow::onDownloadFailed()
{
    delete _dt;
    _dt = NULL;
}

void MainWindow::_setSettingsSensitive()
{
    _dhcpserverframe->set_sensitive( _standaloneradio->get_active() );

    bool s = _ifacecombo->get_active_row_number() != -1;
    if (s && _standaloneradio->get_active())
    {
        if (!_validIP(_startipentry->get_text())
            || !_validIP(_endipentry->get_text())
            || !_validIP(_netmaskentry->get_text())
            || (_gatewayentry->get_text_length() && !_validIP(_gatewayentry->get_text())) )
        {
            s = false;
        }
    }
    _savesettingsbutton->set_sensitive(s);
}

bool MainWindow::_validIP(const std::string &s)
{
    struct sockaddr_in sa;
    return inet_pton(AF_INET, s.c_str(), &(sa.sin_addr)) == 1;
}

void MainWindow::_savesettings()
{
    if ( _standaloneradio->get_active() )
    {
        _window->get_window()->set_cursor();
        _savesettingsbutton->set_sensitive();
        _ps->setSetting("startIP", _startipentry->get_text() );
        _ps->setSetting("endIP", _endipentry->get_text() );
        _ps->setSetting("netmask", _netmaskentry->get_text() );
        _ps->setSetting("gateway", _gatewayentry->get_text() );
    }
    _ps->setSetting("interface", _ifacecombo->get_active_id());
    _ps->setSetting("standaloneDhcpServer", _standaloneradio->get_active() );

    _ps->saveSettings();
    _ps->regenDnsmasqConf();

    Gtk::MessageDialog d(_("Settings saved"));
    d.set_transient_for(*_window);
    d.run();
}

void MainWindow::on_savesettings_clicked()
{
    if ( _standaloneradio->get_active() )
    {
        // Check that there aren't any other DHCP servers active that may conflict

        string nic = _ifacecombo->get_active_id();
        auto watchcursor = Gdk::Cursor::create(Gdk::WATCH);
        _window->get_window()->set_cursor(watchcursor);
        _savesettingsbutton->set_sensitive(false);

        try
        {
            DhcpClient *dhcp = new DhcpClient();
            dhcp->signal_dhcpOffer().connect( sigc::mem_fun(this, &MainWindow::onOtherDhcpServerDetected) );
            dhcp->signal_timeout().connect( sigc::mem_fun(this, &MainWindow::_savesettings) );
            dhcp->sendDhcpDiscover(nic);
        }
        catch (const runtime_error &e)
        {
            // Errors can occur in some circumstances such as when it is unable to bind() to the DHCP client port,
            // due to another program using it
            _window->get_window()->set_cursor();
            _savesettingsbutton->set_sensitive();
            Gtk::MessageDialog d(Glib::ustring::compose(_("Error checking if other DHCP servers are active: %1"), e.what() ));
            d.set_transient_for(*_window);
            d.run();
            _savesettings();
        }
    }
    else
    {
        _savesettings();
    }
}

void MainWindow::onOtherDhcpServerDetected(const std::string &otherserverip)
{
    _window->get_window()->set_cursor();
    Gtk::MessageDialog d(Glib::ustring::compose(_(
                           "There is already another DHCP server (%1) active in your network.\n"
                           "Having more than one DHCP server hand out IP-addresses from the same range can result "
                           "in serious problems, such as that more than one client is assigned the same IP-address.\n"
                           "Please read the <a href=\"https://github.com/raspberrypi/piserver/wiki\">documentation</a> before proceeding.\n\n"
                           "Are you sure you want to activate standalone DHCP server mode?"
                           ), otherserverip), true, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO);
    d.set_transient_for(*_window);
    if (d.run() == Gtk::RESPONSE_NO)
    {
        _proxydhcpradio->set_active();
    }
    else
    {
        _savesettings();
    }
    _savesettingsbutton->set_sensitive();
}

void MainWindow::on_addfolder_clicked()
{
    AddFolderDialog d(_ps, _window);
    if ( d.exec() )
        _reloadFolders();
}

void MainWindow::on_delfolder_clicked()
{
    string folder;
    auto iter = _folderstree->get_selection()->get_selected();
    iter->get_value(0, folder);
    if (!folder.empty())
    {
        Gtk::MessageDialog d(_("Are you sure you want to delete this folder?"),
                             false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO);
        d.set_transient_for(*_window);
        if (d.run() == Gtk::RESPONSE_YES)
        {
            string path = PISERVER_SHAREDROOT "/" +folder;
            if (::rmdir(path.c_str()) == -1)
            {
                Gtk::MessageDialog ed(_("Error deleting folder. Make sure it is empty."));
                ed.set_transient_for(*_window);
                ed.run();
            }
            else
            {
                _reloadFolders();
            }
        }
    }
}

void MainWindow::on_openfolder_clicked()
{
    string folder;
    auto iter = _folderstree->get_selection()->get_selected();
    iter->get_value(0, folder);
    if (!folder.empty())
    {
        string path = PISERVER_SHAREDROOT "/" +folder;
        const gchar *cmd[] = {"/usr/bin/xdg-open", path.c_str(), NULL};
        g_spawn_async(NULL, (gchar **) cmd, NULL, G_SPAWN_DEFAULT, NULL, NULL, NULL, NULL);
    }
}

void MainWindow::on_foldertree_activated(const Gtk::TreeModel::Path &, Gtk::TreeViewColumn *)
{
    _delfolderbutton->set_sensitive();
    _openfolderbutton->set_sensitive();
}

void MainWindow::_reloadFolders()
{
    _folderstore->clear();
    _delfolderbutton->set_sensitive(false);
    _openfolderbutton->set_sensitive(false);

    struct dirent *dp;
    struct stat st;
    DIR *d = ::opendir(PISERVER_SHAREDROOT);

    if (d)
    {
        while (dp = ::readdir(d))
        {
            string name = dp->d_name;
            string path = PISERVER_SHAREDROOT "/" +name;
            string owner;

            if (name == "." || name == "..")
                continue;
            if (::stat(path.c_str(), &st) == -1)
                continue;
            if ( (st.st_mode & S_IFMT ) != S_IFDIR)
                continue;

            passwd *p = getpwuid(st.st_uid);
            if (p && p->pw_name)
                owner = p->pw_name;

            auto row = _folderstore->append();
            row->set_value(0, name);
            row->set_value(1, owner);
        }
    }
}
