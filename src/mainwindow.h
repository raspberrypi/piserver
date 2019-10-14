#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <gtkmm.h>
#include "piserver.h"

class DownloadThread;

class MainWindow
{
public:
    MainWindow(Glib::RefPtr<Gtk::Application> app, PiServer *ps);
    virtual ~MainWindow();
    void exec();

    void _reloadDistro();
    void _reloadHosts();
    void _reloadUsers();
    void _reloadFolders();

protected:
    Glib::RefPtr<Gtk::Application> _app;
    Gtk::Window *_window;
    Gtk::Notebook *_notebook;
    Gtk::TreeView *_distrotree, *_usertree, *_hosttree, *_folderstree;
    Gtk::ToolButton *_edituserbutton, *_deluserbutton, *_edithostbutton, *_delhostbutton,
        *_upgradeosbutton, *_cloneosbutton, *_exportosbutton, *_delosbutton, *_shellbutton, *_delfolderbutton, *_openfolderbutton;
    Gtk::SearchEntry *_usersearchentry;
    Gtk::Entry *_startipentry, *_endipentry, *_netmaskentry, *_gatewayentry;
    Gtk::Button *_savesettingsbutton;
    Gtk::ComboBox *_ifacecombo;
    Gtk::RadioButton *_proxydhcpradio, *_standaloneradio;
    Gtk::Frame *_dhcpserverframe;
    Gtk::Label *_softwarelabel;
    Glib::RefPtr<Gtk::ListStore> _distrostore, _hoststore, _userstore, _ethstore, _folderstore;
    DownloadThread *_dt;
    PiServer *_ps;
    std::string _cachedDistroInfo;

    bool _validIP(const std::string &s);
    void _savesettings();

    /* Slots */
    void on_adduser_clicked();
    void on_importusers_clicked();
    void on_edituser_clicked();
    void on_deluser_clicked();
    void on_addhost_clicked();
    void on_edithost_clicked();
    void on_delhost_clicked();
    void on_addos_clicked();
    void on_upgradeos_clicked();
    void on_delos_clicked();
    void on_cloneos_clicked();
    void on_exportos_clicked();
    void on_shell_clicked();
    void on_hosttree_activated(const Gtk::TreeModel::Path &path, Gtk::TreeViewColumn *col);
    void on_distrotree_activated(const Gtk::TreeModel::Path &path, Gtk::TreeViewColumn *col);
    void on_usertree_activated(const Gtk::TreeModel::Path &path, Gtk::TreeViewColumn *col);
    void on_foldertree_activated(const Gtk::TreeModel::Path &path, Gtk::TreeViewColumn *col);
    void onDownloadSuccessful();
    void onDownloadFailed();
    void onOtherDhcpServerDetected(const std::string &ip);
    void on_popup_clicked();
    bool on_popup_button(GdkEventButton*,Gtk::Widget *sender);
    void _setSettingsSensitive();
    void on_savesettings_clicked();
    void on_addfolder_clicked();
    void on_delfolder_clicked();
    void on_openfolder_clicked();
};

#endif // MAINWINDOW_H
