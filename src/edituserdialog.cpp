#include "edituserdialog.h"

using namespace std;

EditUserDialog::EditUserDialog(PiServer *ps, const std::string &dn, const std::string &user, const std::string &desc, Gtk::Window *parent)
    : _ps(ps), _parentWindow(parent), _dn(dn), _user(user), _oldDesc(desc)
{
    Gtk::Label *userlabel;
    auto builder = Gtk::Builder::create_from_resource("/data/edituserdialog.glade");
    builder->get_widget("edituserdialog", _dialog);
    builder->get_widget("okbutton", _okButton);
    builder->get_widget("descentry", _descEntry);
    builder->get_widget("passentry", _passEntry);
    builder->get_widget("pass2entry", _pass2Entry);
    builder->get_widget("showpasscheck", _showPassCheck);
    builder->get_widget("userlabel", userlabel);
    userlabel->set_text(user);
    _descEntry->set_text(desc);
    _passEntry->signal_changed().connect(sigc::mem_fun(this, &EditUserDialog::setOkButton));
    _pass2Entry->signal_changed().connect(sigc::mem_fun(this, &EditUserDialog::setOkButton));
    _showPassCheck->signal_toggled().connect(sigc::mem_fun(this, &EditUserDialog::on_showPassToggled));

    if (parent)
        _dialog->set_transient_for(*parent);
}

EditUserDialog::~EditUserDialog()
{
    delete _dialog;
}

bool EditUserDialog::exec()
{
    if (_dialog->run() == Gtk::RESPONSE_OK)
    {
        try
        {
            multimap<string,string> changes;
            string desc = _descEntry->get_text();

            if (desc != _oldDesc)
                changes.insert( make_pair("description", desc) );

            if (!_passEntry->get_text().empty())
                changes.insert( make_pair("password", _passEntry->get_text()) );

            if (!changes.empty())
                _ps->updateUser(_dn, changes);

            return true;
        }
        catch (std::exception &e)
        {
            Gtk::MessageDialog ed(e.what());
            ed.set_transient_for(*_parentWindow);
            ed.run();
        }
    }

    return false;
}

void EditUserDialog::setOkButton()
{
    _okButton->set_sensitive(_passEntry->get_text() == _pass2Entry->get_text());
}

void EditUserDialog::on_showPassToggled()
{
    bool showPass = _showPassCheck->get_active();

    _passEntry->set_visibility(showPass);
    _pass2Entry->set_visibility(showPass);
}

std::string EditUserDialog::description()
{
    return _descEntry->get_text();
}
