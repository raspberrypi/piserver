#include "installwizard.h"
#include <random>

using namespace std;

InstallWizard::InstallWizard(Glib::RefPtr<Gtk::Application> app, PiServer *ps)
    : AbstractAddHost(ps), AbstractAddUser(ps), AbstractAddDistro(ps), _app(app), _ps(ps)
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
        d.run();
    }

    setAddHostOkButton();
    setAddUserOkButton();
}

InstallWizard::~InstallWizard()
{
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

        string ldapPassword = _ps->getSetting("ldapPassword", _randomStr(32));
        _ps->setSetting("ldapPassword", ldapPassword);
        string interface;
        _ifacecombo->get_active()->get_value(0, interface);
        _ps->setSetting("interface", interface);
        _ps->saveSettings();
        _dependsThread = new DependenciesInstallThread(_ps, ldapPassword);
        _dependsThread->signalSuccess().connect( sigc::mem_fun(this, &InstallWizard::onDependsInstallComplete) );
        _dependsThread->signalFailure().connect( sigc::mem_fun(this, &InstallWizard::onDependsInstallFailed) );
        _dependsThread->start();
        //startInstallation();
    }
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
    d.run();
    _app->quit();
}
