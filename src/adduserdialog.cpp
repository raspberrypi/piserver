#include "adduserdialog.h"
#include "csv/csv.h"
#include <set>
#include <algorithm>

using namespace std;

AddUserDialog::AddUserDialog(PiServer *ps, Gtk::Window *parent)
    : AbstractAddUser(ps), _ps(ps), _parentWindow(parent)
{
    auto builder = Gtk::Builder::create_from_resource("/data/adduserdialog.glade");
    setupAddUserFields(builder);
    builder->get_widget("adduserdialog", _dialog);
    builder->get_widget("okbutton", _okbutton);
    builder->get_widget("grid", _grid);
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

/* Import CSV file and show result in normal adduser dialog, so user can check before adding */
bool AddUserDialog::importCSV(const string &filename)
{
    try
    {
        io::CSVReader<2, io::trim_chars<' '>, io::double_quote_escape<',','\"'> > csv(filename);
        map<string,string> users;
        set<string> notwellformed, duplicates, alreadyexists;
        string user, password;

        csv.set_header("user", "password");
        while (csv.read_row(user, password))
        {
            /* Normalize user names to lower case */
            std::transform(user.begin(), user.end(), user.begin(), ::tolower);
            /* Replace any spaces with dots */
            std::replace(user.begin(), user.end(), ' ', '.');

            if (users.count(user))
            {
                duplicates.insert(user);
            }
            else
            {
                if (!_ps->isUsernameValid(user))
                    notwellformed.insert(user);
                else if (!_ps->isUsernameAvailableLocally(user)
                         || !_ps->isUsernameAvailable(user))
                    alreadyexists.insert(user);
                else
                    users.insert(make_pair(user, password));
            }
        }

        if (!notwellformed.empty() || !alreadyexists.empty() || !duplicates.empty())
        {
            string errmsg;

            if (!notwellformed.empty())
            {
                errmsg += _("The following user names contain invalid characters: ");
                for (auto user : notwellformed)
                    errmsg += "'"+user+"' ";

                errmsg += _("\nOnly the following characters are allowed: a-z 0-9 . - _\n");
            }
            if (!alreadyexists.empty())
            {
                errmsg += _("The following user names already exist on the system or this computer: ");
                for (auto user : alreadyexists)
                    errmsg += "'"+user+"' ";
                errmsg += "\n";
            }
            if (!duplicates.empty())
            {
                errmsg += _("The following user names appear multiple times in the CSV file: ");
                for (auto user : duplicates)
                    errmsg += "'"+user+"' ";
            }

            Gtk::MessageDialog d(errmsg);
            d.run();
        }

        if (!users.empty())
        {
            /* Replace existing input boxes with dynamically created ones */

            for (int i=0; i<_userEntry.size(); i++)
                _grid->remove_row(1);
            _userEntry.clear();
            _passEntry.clear();

            int nr = 1;
            for (auto &kv : users)
            {
                auto label = Gtk::manage(new Gtk::Label( to_string(nr)+")" ));
                _grid->attach(*label, 0, nr, 1, 1);
                auto uentry = Gtk::manage(new Gtk::Entry() );
                uentry->set_text(kv.first);
                _grid->attach(*uentry, 1, nr, 1, 1);
                _userEntry.push_back(uentry);
                uentry->signal_changed().connect(sigc::mem_fun(this, &AddUserDialog::setAddUserOkButton));
                auto pentry = Gtk::manage(new Gtk::Entry() );
                pentry->set_text(kv.second);
                pentry->set_visibility(false);
                _grid->attach(*pentry, 2, nr, 1, 1);
                _passEntry.push_back(pentry);
                pentry->signal_changed().connect(sigc::mem_fun(this, &AddUserDialog::setAddUserOkButton));
                nr++;
            }
            _grid->show_all();
            setAddUserOkButton();

            return true;
        }
    }
    catch (const exception &e)
    {
        Gtk::MessageDialog d(e.what());
        d.run();
    }

    return false;
}
