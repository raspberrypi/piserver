#include "adddistrodialog.h"

using namespace std;

AddDistroDialog::AddDistroDialog(MainWindow *mw, PiServer *ps, const std::string &cachedDistroInfo, Gtk::Window *parent)
    : AbstractAddDistro(ps), _mw(mw)
{
    auto builder = Gtk::Builder::create_from_resource("/data/adddistrodialog.glade");
    builder->get_widget("assistant", _assistant);
    builder->get_widget("adddistropage", _addDistroPage);
    builder->get_widget("progresspage", _progressPage);
    setupAddDistroFields(builder, cachedDistroInfo);
    _assistant->signal_cancel().connect( sigc::mem_fun(this, &AddDistroDialog::onClose) );
    _assistant->signal_close().connect( sigc::mem_fun(this, &AddDistroDialog::onClose) );
    _assistant->signal_prepare().connect( sigc::mem_fun(this, &AddDistroDialog::onPagePrepare));

    if (parent)
        _assistant->set_transient_for(*parent);
}

AddDistroDialog::~AddDistroDialog()
{
}

void AddDistroDialog::show()
{
    _assistant->show();
}

void AddDistroDialog::selectDistro(const std::string &name)
{
    for (auto &row : _repoStore->children() )
    {
        string rn;
        row->get_value(1, rn);
        if (rn == name)
        {
            Gtk::TreePath path(row);
            _distroTree->set_cursor(path);
            break;
        }
    }
}

void AddDistroDialog::onClose()
{
    _assistant->hide();
}

void AddDistroDialog::onPagePrepare(Gtk::Widget *newPage)
{
    if (newPage == _progressPage)
    {
        startInstallation();
    }
}

void AddDistroDialog::setAddDistroOkButton()
{
    _assistant->set_page_complete(*_addDistroPage,
                                  addDistroFieldsOk() );
}

void AddDistroDialog::onInstallationSuccess()
{
    _mw->_reloadDistro();
    _assistant->set_page_complete(*_progressPage, true);
    _assistant->next_page();
}

void AddDistroDialog::onInstallationFailed(const std::string &error)
{
    Gtk::MessageDialog d(error);
    d.run();
    _assistant->hide();
}
