#ifndef INSTALLWIZARD_H
#define INSTALLWIZARD_H

#include "piserver.h"
#include "abstractaddhost.h"
#include "abstractadduser.h"
#include "abstractadddistro.h"
#include "dependenciesinstallthread.h"
#include <gtkmm.h>

class InstallWizard : public AbstractAddHost, public AbstractAddUser, public AbstractAddDistro
{
public:
    InstallWizard(Glib::RefPtr<Gtk::Application> app, PiServer *ps);
    virtual ~InstallWizard();
    void exec();

protected:
    Glib::RefPtr<Gtk::Application> _app;
    PiServer *_ps;
    Gtk::Assistant *_assistant;
    Gtk::ComboBox *_ifacecombo;
    Glib::RefPtr<Gtk::ListStore> _ethstore, _repostore;
    Gtk::Widget *_addHostPage, *_addUserPage, *_addDistroPage, *_progressPage;
    DependenciesInstallThread *_dependsThread;

    virtual void setAddHostOkButton();
    virtual void setAddUserOkButton();
    virtual void setAddDistroOkButton();
    void onClose();
    void onPagePrepare(Gtk::Widget *newPage);
    void onDependsInstallComplete();
    void onDependsInstallFailed();
    virtual void onInstallationSuccess();
    virtual void onInstallationFailed(const std::string &error);

    std::string _randomStr(int len);
};

#endif // INSTALLWIZARD_H
