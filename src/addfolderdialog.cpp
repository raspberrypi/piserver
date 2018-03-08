#include "addfolderdialog.h"
#include "piserver.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>

using namespace std;

AddFolderDialog::AddFolderDialog(PiServer *ps, Gtk::Window *parent)
    : _ps(ps)
{
    auto builder = Gtk::Builder::create_from_resource("/data/addfolderdialog.glade");
    builder->get_widget("addfolderdialog", _dialog);
    builder->get_widget("okbutton", _okButton);
    builder->get_widget("nameentry", _nameEntry);
    builder->get_widget("ownerentry", _ownerEntry);
    const char *user = getenv("SUDO_USER");
    if (!user)
        user = getenv("USER");
    if (user)
        _ownerEntry->set_text(user);

    _nameEntry->signal_changed().connect(sigc::mem_fun(this, &AddFolderDialog::setOkSensitive));
    _ownerEntry->signal_changed().connect(sigc::mem_fun(this, &AddFolderDialog::setOkSensitive));
    if (parent)
        _dialog->set_transient_for(*parent);
}

AddFolderDialog::~AddFolderDialog()
{
    delete _dialog;
}

bool AddFolderDialog::exec()
{
    while (_dialog->run() == Gtk::RESPONSE_OK)
    {
        string name  = _nameEntry->get_text();
        string owner = _ownerEntry->get_text();
        string path  = PISERVER_SHAREDROOT "/" + name;
        struct passwd *userinfo;

        try
        {
            userinfo = ::getpwnam(owner.c_str());
            if (!userinfo)
                throw runtime_error(_("Specified owner does not exist"));

            if (::mkdir(path.c_str(), 0755) == -1)
                throw runtime_error(Glib::ustring::compose(_("Error creating folder '%1': %2"), path, ::strerror(errno)));
            if (::chown(path.c_str(), userinfo->pw_uid, userinfo->pw_gid) == -1)
                throw runtime_error(Glib::ustring::compose(_("Error changing owner of folder '%1': %2"), path, ::strerror(errno)));

            return true;
        }
        catch (const runtime_error &r)
        {
            Gtk::MessageDialog d(r.what());
            d.set_parent(*_dialog);
            d.run();
        }
    }

    return false;
}

bool AddFolderDialog::isNameValid(const std::string &name)
{
    if (name[0] == '-' || name[0] == '.' || name[0] == ' ')
        return false;

    for (char c : name)
    {
        if (!isalnum(c) && c != '.' && c != '_' && c != '-' && c != ' ')
            return false;
    }

    return true;
}

void AddFolderDialog::setOkSensitive()
{
    bool ok = _nameEntry->get_text_length()
            && _ownerEntry->get_text_length()
            && isNameValid(_nameEntry->get_text() );
    _okButton->set_sensitive(ok);
}
