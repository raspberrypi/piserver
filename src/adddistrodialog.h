#ifndef ADDDISTRODIALOG_H
#define ADDDISTRODIALOG_H

#include "abstractadddistro.h"
#include "mainwindow.h"

class AddDistroDialog : public AbstractAddDistro
{
public:
    AddDistroDialog(MainWindow *mw, PiServer *ps, const std::string &cachedDistroInfo = "", Gtk::Window *parent = NULL);
    virtual ~AddDistroDialog();
    void show();
    void selectDistro(const std::string &name);

protected:
    MainWindow *_mw;
    Gtk::Assistant *_assistant;
    Gtk::Widget *_addDistroPage, *_progressPage;

    void onClose();
    void onPagePrepare(Gtk::Widget *newPage);
    void setAddDistroOkButton();
    void onInstallationSuccess();
    void onInstallationFailed(const std::string &error);
};

#endif // ADDDISTRODIALOG_H
