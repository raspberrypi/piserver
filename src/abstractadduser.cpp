#include "abstractadduser.h"
#include "piserver.h"
#include <set>

using namespace std;

AbstractAddUser::AbstractAddUser(PiServer *ps)
    : _ps(ps)
{
}

AbstractAddUser::~AbstractAddUser()
{
}

void AbstractAddUser::setupAddUserFields(Glib::RefPtr<Gtk::Builder> builder)
{
    builder->get_widget("showpasscheck", _showPassCheck);
    builder->get_widget("forcepasschangecheck", _forcePassChangeCheck);
    _showPassCheck->signal_toggled().connect(sigc::mem_fun(this, &AbstractAddUser::on_showPassToggled));

    for (int i=1; i<=5; i++)
    {
        Gtk::Entry *e;
        builder->get_widget("user"+to_string(i), e);
        _userEntry.push_back(e);
        e->signal_changed().connect(sigc::mem_fun(this, &AbstractAddUser::setAddUserOkButton));
        builder->get_widget("pass"+to_string(i), e);
        _passEntry.push_back(e);
        e->signal_changed().connect(sigc::mem_fun(this, &AbstractAddUser::setAddUserOkButton));
    }
}

bool AbstractAddUser::addUserFieldsOk()
{
    bool ok = false;

    /* Enable 'ok' button if at least one username & password combo is filled in.
       and there aren't any entries that lack a counterpart */
    for (int i=0; i < _userEntry.size(); i++)
    {
        bool userSet = _userEntry[i]->get_text_length() > 0;
        bool passSet = _passEntry[i]->get_text_length() > 0;

        if (userSet && passSet)
            ok = true;
        else if (userSet && !passSet)
        {
            ok = false;
            break;
        }
        else if (!userSet && passSet)
        {
            ok = false;
            break;
        }
    }

    return ok;
}

void AbstractAddUser::on_showPassToggled()
{
    bool showPass = _showPassCheck->get_active();

    for (auto e : _passEntry)
    {
        e->set_visibility(showPass);
    }
}

bool AbstractAddUser::checkUserAvailability(bool useLDAP)
{
    set<string> notwellformed, alreadyexists;

    for (int i=0; i < _userEntry.size(); i++)
    {
        string user = _userEntry[i]->get_text();

        if (!user.length())
            continue;

        try
        {
            if (!_ps->isUsernameValid(user))
                notwellformed.insert(user);
            else if (!_ps->isUsernameAvailableLocally(user)
                     || (useLDAP && !_ps->isUsernameAvailable(user)))
                alreadyexists.insert(user);
        }
        catch (exception &e)
        {
            /* Can error out checking availibility if it is unable to connect to LDAP */
            Gtk::MessageDialog d(e.what());
            d.run();
            return false;
        }
    }

    if (!notwellformed.empty() || !alreadyexists.empty())
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
        }

        Gtk::MessageDialog d(errmsg);
        d.run();

        return false;
    }

    return true;
}

void AbstractAddUser::addUsers()
{
    for (int i=0; i < _userEntry.size(); i++)
    {
        string user = _userEntry[i]->get_text();
        string pass = _passEntry[i]->get_text();

        if (!user.length())
            continue;

        try
        {
            _ps->addUser(user, pass, _forcePassChangeCheck->get_active());
        }
        catch (exception &e)
        {
            Gtk::MessageDialog d(e.what());
            d.run();
        }
    }

    // Flush nscd cache
    if ( ::system("nscd -i passwd") != 0 ) { }
}
