#ifndef STPANALYZER_H
#define STPANALYZER_H

/**
 * Class to detect if the Spanning-Tree-Protocol
 * is enabled on an Ethernet switch (which can
 * interfere with network booting)
 *
 * Detection only works when Piserver is installed
 * bare-metal on a server directly connected to
 * the network (not under with Vmware, Virtualbox, etc.)
 */

#include <glibmm.h>
#include <string>

class StpAnalyzer
{
public:
    StpAnalyzer(int onlyReportIfForwardDelayIsAbove = 5);
    virtual ~StpAnalyzer();
    void startListening(const std::string &ifname);
    void stopListening();

    /* signals */
    inline sigc::signal<void,std::string> signal_detected()
    {
        return _signal_detected;
    }

protected:
    int _s, _minForwardDelay;
    sigc::connection _conn;
    sigc::signal<void,std::string> _signal_detected;

    /* Slots */
    bool on_packet(Glib::IOCondition cond);
};

#endif // STPANALYZER_H
