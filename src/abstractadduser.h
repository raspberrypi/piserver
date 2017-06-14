#ifndef ABSTRACTADDUSER_H
#define ABSTRACTADDUSER_H

#include <gtkmm.h>

class PiServer;

class AbstractAddUser
{
protected:
    AbstractAddUser(PiServer *ps);
    void setupAddUserFields(Glib::RefPtr<Gtk::Builder> builder);
    virtual ~AbstractAddUser();
    virtual void setAddUserOkButton() = 0;
    bool addUserFieldsOk();
    void on_showPassToggled();
    bool checkUserAvailability(bool useLDAP);
    void addUsers();

    Gtk::CheckButton *_forcePassChangeCheck, *_showPassCheck;
    std::vector<Gtk::Entry *> _userEntry;
    std::vector<Gtk::Entry *> _passEntry;

private:
    PiServer *_ps;
};

#endif // ABSTRACTADDUSER_H
