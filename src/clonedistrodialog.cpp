#include "clonedistrodialog.h"
#include "distribution.h"
#include "piserver.h"
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>

using namespace std;

CloneDistroDialog::CloneDistroDialog(PiServer *ps, Distribution *olddistro, Gtk::Window *parent)
    : _ps(ps), _olddistro(olddistro)
{
    auto builder = Gtk::Builder::create_from_resource("/data/clonedistrodialog.glade");
    Gtk::Label *distrolabel;
    builder->get_widget("clonedistrodialog", _dialog);
    builder->get_widget("okbutton", _okButton);
    builder->get_widget("nameentry", _nameEntry);
    builder->get_widget("distrolabel", distrolabel);
    distrolabel->set_text(olddistro->name());

    _nameEntry->signal_changed().connect(sigc::mem_fun(this, &CloneDistroDialog::setOkSensitive));
    if (parent)
        _dialog->set_transient_for(*parent);
}

CloneDistroDialog::~CloneDistroDialog()
{
    delete _dialog;
}

bool CloneDistroDialog::exec()
{
    while (_dialog->run() == Gtk::RESPONSE_OK)
    {
        try
        {
            string name = _nameEntry->get_text();


            if (_ps->getDistributions()->count(name))
                throw runtime_error(Glib::ustring::compose(
                                        _("There already exists another OS with the name '%1'"), name));

            Distribution *newdistro = new Distribution(name, "");
            string oldpath = _olddistro->distroPath();
            string newpath = newdistro->distroPath();

            if (::access(oldpath.c_str(), F_OK) == -1)
                throw runtime_error(Glib::ustring::compose(
                                        _("Source folder '%1' does not exists"), oldpath));
            if (::access(newpath.c_str(), F_OK) == 0)
                throw runtime_error(Glib::ustring::compose(
                                        _("There is already a folder '%1' on disk"), newpath));

            /* FIXME: proper file copy code with disk space check and progress indication
               instead of just calling "cp" may be better */

            const gchar *cmd[] = {"cp", "-a", "--one-file-system", oldpath.c_str(), newpath.c_str(), NULL};
            GError *error = NULL;

            if (!g_spawn_async(NULL, (gchar **) cmd, NULL,
                               (GSpawnFlags) (G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH), NULL, NULL, &_pid, &error))
                throw runtime_error(error->message);

            _progressDialog = new Gtk::MessageDialog(*_dialog, _("Copying files"),
                                                     false, Gtk::MESSAGE_OTHER, Gtk::BUTTONS_NONE, false);
            sigc::connection cwatch = Glib::signal_child_watch().connect(sigc::mem_fun(this, &CloneDistroDialog::onCopyFinished), _pid);
            _dialog->get_window()->set_cursor(Gdk::Cursor::create(Gdk::WATCH));
            int result = _progressDialog->run();
            _ps->addDistribution(newdistro);
            _dialog->get_window()->set_cursor();
            cwatch.disconnect();
            g_spawn_close_pid(_pid);
            delete _progressDialog;
            _progressDialog = NULL;

            if (result != 0)
            {
                const char *msg = _("Error occured while copying files");

                if (result == Gtk::RESPONSE_DELETE_EVENT)
                {
                    ::kill(_pid, SIGTERM);
                    msg = _("Copying files cancelled");
                }

                Gtk::MessageDialog d(msg);
                d.set_transient_for(*_dialog);
                d.run();

                /* FIXME: should we delete partially copied files? */
            }

            return true;
        }
        catch (const runtime_error &r)
        {
            Gtk::MessageDialog d(r.what());
            d.set_transient_for(*_dialog);
            d.run();
        }
    }

    return false;
}

bool CloneDistroDialog::isNameValid(const std::string &name)
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

void CloneDistroDialog::setOkSensitive()
{
    bool ok = _nameEntry->get_text_length()
            && isNameValid(_nameEntry->get_text() );
    _okButton->set_sensitive(ok);
}

void CloneDistroDialog::onCopyFinished(GPid pid, int status)
{
    if (pid == _pid && _progressDialog)
        _progressDialog->response(status);
}
