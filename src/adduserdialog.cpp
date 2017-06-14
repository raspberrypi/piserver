#include "adduserdialog.h"
#include <set>

using namespace std;

AddUserDialog::AddUserDialog(PiServer *ps, Gtk::Window *parent)
    : AbstractAddUser(ps), _ps(ps), _parentWindow(parent)
{
    auto builder = Gtk::Builder::create_from_resource("/data/adduserdialog.glade");
    setupAddUserFields(builder);
    builder->get_widget("adduserdialog", _dialog);
    builder->get_widget("okbutton", _okbutton);
    if (parent)
        _dialog->set_transient_for(*parent);
}

AddUserDialog::~AddUserDialog()
{
    delete _dialog;
}

bool AddUserDialog::exec()
{
    while (_dialog->run() == Gtk::RESPONSE_OK)
    {
        if (checkUserAvailability(true))
        {
            addUsers();
            return true;
        }
    }

    return false;
}

void AddUserDialog::setAddUserOkButton()
{
    _okbutton->set_sensitive( addUserFieldsOk() );
}
