#include "ldapsettingsdialog.h"
#include "piserver.h"
#include <regex>
#include <strings.h>

using namespace std;

LdapSettingsDialog::LdapSettingsDialog(PiServer *ps, Gtk::Window *parent)
    : _ps(ps)
{
    auto builder = Gtk::Builder::create_from_resource("/data/ldapsettingsdialog.glade");
    builder->get_widget("dialog", _dialog);
    builder->get_widget("dntree", _dntree);
    builder->get_widget("grouptree", _grouptree);
    _dnstore = Glib::RefPtr<Gtk::ListStore>::cast_dynamic( builder->get_object("dnstore") );
    _groupstore = Glib::RefPtr<Gtk::ListStore>::cast_dynamic( builder->get_object("groupstore") );

    string currentDN = _ps->getSetting("ldapDN");
    string currentGroup = _ps->getSetting("ldapGroup");
    set<string> DNs    = _ps->getPotentialBaseDNs();
    set<string> groups = _ps->getLdapGroups();

    for (string dn : DNs)
    {
        char *sortname = ldap_dn2dcedn(dn.c_str());
        string sortstring = sortname;
        ldap_memfree(sortname);
        auto row = _dnstore->append();
        row->set_value(0, dn);
        row->set_value(1, sortstring);
        if ( !strcasecmp(dn.c_str(), currentDN.c_str()) )
            _dntree->get_selection()->select(row);
    }
    _dnstore->set_sort_column(1, Gtk::SORT_ASCENDING);

    for (string group : groups)
    {
        auto row = _groupstore->append();
        row->set_value(0, group);
        if (group == currentGroup)
            _grouptree->get_selection()->select(row);
    }

    /* If nothing is selected already, select first row */
    if (!_dntree->get_selection()->get_selected())
    {
        auto iter = _dnstore->children().begin();
        if (iter)
        {
            _grouptree->get_selection()->select(iter);
        }
    }
    if (!_grouptree->get_selection()->get_selected())
    {
        auto iter = _groupstore->children().begin();
        if (iter)
        {
            _grouptree->get_selection()->select(iter);
        }
    }

    if (parent)
        _dialog->set_transient_for(*parent);
}

LdapSettingsDialog::~LdapSettingsDialog()
{
    delete _dialog;
}

bool LdapSettingsDialog::exec()
{
    if (_dialog->run() != Gtk::RESPONSE_OK)
        return false;

    string dn, group, ldapExtraConfig, ldapServerType;
    auto dniter    = _dntree->get_selection()->get_selected();
    auto groupiter = _grouptree->get_selection()->get_selected();

    if (!dniter || !groupiter)
        return false;

    dniter->get_value(0, dn);
    groupiter->get_value(0, group);
    ldapServerType  = _ps->getSetting("ldapServerType");
    ldapExtraConfig = _ps->getSetting("ldapExtraConfig");
    regex filterRegex("(filter passwd [^\n]+\n)");

    /* Remove existing filter setting if any */
    ldapExtraConfig = regex_replace(ldapExtraConfig, filterRegex, "");

    if (group == "[all groups allowed]")
        group = "";

    ldapExtraConfig += "filter passwd "+_ps->getLdapFilter(group)+"\n";
    _ps->setSetting("ldapExtraConfig", ldapExtraConfig);
    _ps->setSetting("ldapGroup", group);
    _ps->setSetting("ldapDN", dn);

    return true;
}
