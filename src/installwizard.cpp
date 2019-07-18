#include "installwizard.h"
#include "activediscovery.h"
#include "ldapsettingsdialog.h"
#include <random>

using namespace std;

InstallWizard::InstallWizard(Glib::RefPtr<Gtk::Application> app, PiServer *ps)
    : AbstractAddHost(ps), AbstractAddUser(ps),
      AbstractAddDistro(ps), _app(app), _ps(ps), _activeDiscovery(NULL), _prevPage(0)
{
    auto builder = Gtk::Builder::create_from_resource("/data/firstuse.glade");
    builder->get_widget("assistant", _assistant);
    builder->get_widget("addhostpage", _addHostPage);
    builder->get_widget("adddistropage", _addDistroPage);
    builder->get_widget("adduserpage", _addUserPage);
    builder->get_widget("progresspage", _progressPage);
    setupAddHostFields(builder);
    setupAddUserFields(builder);
    setupAddDistroFields(builder);
    _assistant->signal_cancel().connect( sigc::mem_fun(this, &InstallWizard::onClose) );
    _assistant->signal_close().connect( sigc::mem_fun(this, &InstallWizard::onClose) );
    _assistant->signal_prepare().connect( sigc::mem_fun(this, &InstallWizard::onPagePrepare));
    builder->get_widget("ifacecombo", _ifacecombo);
    _ethstore = Glib::RefPtr<Gtk::ListStore>::cast_dynamic( builder->get_object("ethstore") );
    _repostore = Glib::RefPtr<Gtk::ListStore>::cast_dynamic( builder->get_object("distrostore") );
    builder->get_widget("authselectpage", _authSelectPage);
    builder->get_widget("localldapradio", _localLdapRadio);
    builder->get_widget("extldapradio", _extLdapRadio);
    builder->get_widget("extldapframe", _extLdapFrame);
    builder->get_widget("ldapserverentry", _ldapServerEntry);
    builder->get_widget("domainentry", _domainEntry);
    builder->get_widget("binduserentry", _bindUserEntry);
    builder->get_widget("bindpassentry", _bindPassEntry);
    builder->get_widget("ldaptypebox", _ldapTypeBox);
    builder->get_widget("restrictbygroupradio", _restrictByGroupRadio);
    _localLdapRadio->signal_clicked().connect( sigc::mem_fun(this, &InstallWizard::authSelectionRadioChange) );
    _extLdapRadio->signal_clicked().connect( sigc::mem_fun(this, &InstallWizard::authSelectionRadioChange) );
    _ldapTypeBox->signal_changed().connect( sigc::mem_fun(this, &InstallWizard::setAuthSelectionOkButton) );
    _ldapServerEntry->signal_changed().connect( sigc::mem_fun(this, &InstallWizard::setAuthSelectionOkButton) );
    _domainEntry->signal_changed().connect( sigc::mem_fun(this, &InstallWizard::setAuthSelectionOkButton) );
    _bindUserEntry->signal_changed().connect( sigc::mem_fun(this, &InstallWizard::setAuthSelectionOkButton) );
    _bindPassEntry->signal_changed().connect( sigc::mem_fun(this, &InstallWizard::setAuthSelectionOkButton) );

    _ethstore->clear();
    map<string,string> ifaces = _ps->getInterfaces();
    bool hasIfaces = false;

    for (auto kv : ifaces)
    {
        /* exclude loopback interface */
        if (kv.first == "lo")
            continue;

        auto row = _ethstore->append();
        row->set_value(0, kv.first);
        row->set_value(1, kv.first+" ("+kv.second+")");
        hasIfaces = true;
    }

    if (hasIfaces)
        _ifacecombo->set_active(0);
    else
    {
        Gtk::MessageDialog d(_("No network interfaces with IPv4 address detected. Check your network settings."));
        d.set_transient_for(*_assistant);
        d.run();
    }

    setAddHostOkButton();
    setAddUserOkButton();
}

InstallWizard::~InstallWizard()
{
    if (_activeDiscovery)
        delete _activeDiscovery;
    delete _assistant;
}

void InstallWizard::exec()
{
    _app->run(*_assistant);
}

void InstallWizard::onClose()
{
    //_app->quit();
    _assistant->hide();
}

void InstallWizard::setAddHostOkButton()
{
    _assistant->set_page_complete(*_addHostPage,
                                  addHostFieldsOk() && _ifacecombo->get_active_row_number() != -1 );
}

void InstallWizard::setAddUserOkButton()
{
    _assistant->set_page_complete(*_addUserPage,
                                  addUserFieldsOk() );
}

void InstallWizard::setAddDistroOkButton()
{
    _assistant->set_page_complete(*_addDistroPage,
                                  addDistroFieldsOk() );
}

void InstallWizard::onPagePrepare(Gtk::Widget *newPage)
{
    if (newPage == _addDistroPage && !checkUserAvailability(false))
    {
        _assistant->previous_page();
    }

    if (newPage == _progressPage)
    {
        _progressLabel1->set_text( _("Installing LDAP, NFS and DHCP server software") );

        string interface;
        _ifacecombo->get_active()->get_value(0, interface);
        _ps->setSetting("interface", interface);
        _ps->saveSettings();
        _dependsThread = new DependenciesInstallThread(_ps);
        _dependsThread->signalSuccess().connect( sigc::mem_fun(this, &InstallWizard::onDependsInstallComplete) );
        _dependsThread->signalFailure().connect( sigc::mem_fun(this, &InstallWizard::onDependsInstallFailed) );
        _dependsThread->start();
    }

    if (newPage == _addUserPage)
    {
        if (_extLdapRadio->get_active())
        {
            if (_assistant->get_nth_page(_prevPage) == _addDistroPage)
            {
                /* User pressed 'back' button on add software page. Go back one more to skip add users page */
                _assistant->previous_page();
            }
            else
            {
                /* Try out the external LDAP server information the user entered */
                string ldapType   = _ldapTypeBox->get_active_id();
                string ldapServer = _ldapServerEntry->get_text();
                string domain     = _domainEntry->get_text();
                string ldapUser   = _bindUserEntry->get_text();
                string ldapPass   = _bindPassEntry->get_text();
                string baseDN, domainSid;

                if (ldapServer.find(':') == string::npos)
                {
                    ldapServer = "ldap://"+ldapServer;
                }

                if (domain.find('=') != string::npos)
                {
                    /* Looks like the user entered a baseDN as domain */
                    baseDN = domain;
                }
                else
                {
                    /* Convert domain entered to baseDN */
                    istringstream d(domain);
                    string domainpart;
                    while (getline(d, domainpart, '.'))
                    {
                        if (!baseDN.empty())
                            baseDN += ",";
                        baseDN += "dc="+domainpart;
                    }
                }

                if (ldapType == "ad"
                        && ldapUser.find_first_of("@\\=") == string::npos)
                {
                    /* If using AD, and the username doesn't look like like a DN,
                       and doesn't include a domain already, append the domain */
                    ldapUser += "@" + domain;
                }

                try
                {
                    /* Test if the LDAP details provided work.
                       For AD also fetch domainSid as we use that to convert user's sid to unix user id numbers */
                    domainSid = _ps->getDomainSidFromLdap(ldapServer, ldapType, baseDN, ldapUser, ldapPass);

                    if (ldapType == "ad")
                    {
                        _ps->setSetting("domainSid", domainSid);
                        _ps->setSetting("ldapExtraConfig",
                                "referrals no\n"
                                "filter passwd (&(objectCategory=person)(objectClass=user)(!(UserAccountControl:1.2.840.113556.1.4.803:=2)))\n"
                                "map passwd uid              sAMAccountName\n"
                                "map passwd homeDirectory    \"/home/$sAMAccountName\"\n"
                                "map passwd gecos            displayName\n"
                                "map passwd loginShell       \"/bin/bash\"\n"
                                "map passwd gidNumber        \""+to_string(PISERVER_GID)+"\"\n"
                                "map passwd uidNumber        objectSid:"+domainSid+"\n"
                        );
                    }
                    else
                    {
                        _ps->unsetSetting("domainSid");
                        _ps->unsetSetting("ldapExtraConfig");
                    }
                    if (!ldapUser.empty())
                    {
                        _ps->setSetting("ldapUser", ldapUser);
                        _ps->setSetting("ldapPassword", ldapPass);
                    }
                    else
                    {
                        _ps->unsetSetting("ldapUser");
                        _ps->unsetSetting("ldapPassword");
                    }
                    _ps->setSetting("ldapServerType", ldapType);
                    _ps->setSetting("ldapUri", ldapServer);
                    _ps->setSetting("ldapDN", baseDN);

                    if (_restrictByGroupRadio->get_active())
                    {
                        /* Show dialog for advanced LDAP options (restrictions) */
                        LdapSettingsDialog ls(_ps, _assistant);
                        if (!ls.exec())
                        {
                            /* Keep user on current page */
                            _assistant->previous_page();
                            return;
                        }
                    }
                    else
                    {
                        _ps->unsetSetting("ldapGroup");
                    }

                    /* External LDAP seems to work, skip "add users" page */
                    _assistant->next_page();
                }
                catch (const exception &e)
                {
                    /* No luck connecting to external LDAP. Keep user on settings page */
                    _assistant->previous_page();

                    Gtk::MessageDialog d(e.what());
                    d.set_transient_for(*_assistant);
                    d.run();
                }
            }
        }
        else
        {
            _ps->setSetting("ldapPassword", _randomStr(32));
            _ps->unsetSetting("ldapServerType");
            _ps->unsetSetting("ldapUri");
            _ps->unsetSetting("ldapDN");
            _ps->unsetSetting("domainSid");
            _ps->unsetSetting("ldapUser");
            _ps->unsetSetting("ldapGroup");
        }
    }

    _prevPage = _assistant->get_current_page();
}

std::string InstallWizard::_randomStr(int len)
{
    const char *charset = "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";
    random_device rd;
    uniform_int_distribution<int> dist(0, strlen(charset)-1);
    string result;

    for (int i = 0; i < len; i++)
        result += charset[dist(rd)];

    return result;
}

void InstallWizard::onDependsInstallComplete()
{
    delete _dependsThread;
    startInstallation();
}

void InstallWizard::onDependsInstallFailed()
{
    Gtk::MessageDialog d( _dependsThread->error() );
    d.set_transient_for(*_assistant);
    d.run();
    delete _dependsThread;
    _app->quit();
}

void InstallWizard::onInstallationSuccess()
{
    addHosts(_dist);
    _ps->setSetting("installed", true);
    _ps->saveSettings();
    addUsers();

    _assistant->set_page_complete(*_progressPage, true);
    _assistant->next_page();
}

void InstallWizard::onInstallationFailed(const std::string &error)
{
    Gtk::MessageDialog d(error);
    d.set_transient_for(*_assistant);
    d.run();
    _app->quit();
}

void InstallWizard::authSelectionRadioChange()
{
    if (_extLdapRadio->get_active())
    {
        if (!_activeDiscovery)
        {
            /* Try to auto-detect AD server */
            _activeDiscovery = new ActiveDiscovery();
            _activeDiscovery->signal_discovered().connect(sigc::mem_fun(this, &InstallWizard::adDiscovered));
            _activeDiscovery->signal_discoveryFailed().connect(sigc::mem_fun(this, &InstallWizard::adDiscoveryFailed));
            string interface;
            _ifacecombo->get_active()->get_value(0, interface);
            try
            {
                _activeDiscovery->startDiscovery(interface);
                auto watchcursor = Gdk::Cursor::create(Gdk::WATCH);
                _assistant->get_window()->set_cursor(watchcursor);
            }
            catch (...)
            {
                /* Can fail if we are unable to bind to the dhcp client port */
            }
        }

        _extLdapFrame->set_sensitive();
        _ldapServerEntry->grab_focus();
    }
    else
    {
        _extLdapFrame->set_sensitive(false);
    }
    setAuthSelectionOkButton();
}

void InstallWizard::setAuthSelectionOkButton()
{
    bool complete = false;

    if (_localLdapRadio->get_active())
    {
        complete = true;
    }
    else if (_ldapServerEntry->get_text_length() && _domainEntry->get_text_length())
    {
        if (_ldapTypeBox->get_active_id() != "ad"
                || (_bindUserEntry->get_text_length() && _bindPassEntry->get_text_length() ))
        {
            complete = true;
        }
    }

    _assistant->set_page_complete(*_authSelectPage, complete);
}

void InstallWizard::adDiscovered(const std::string &server, const std::string &domain)
{
    _assistant->get_window()->set_cursor();
    _ldapServerEntry->set_text(server);
    _domainEntry->set_text(domain);
    _bindUserEntry->grab_focus();
}

void InstallWizard::adDiscoveryFailed()
{
    _assistant->get_window()->set_cursor();
}
