/*
 * testdiscovery.h
 *
 * Copyright (C) 2006 by Universitaet Stuttgart (VIS). Alle Rechte vorbehalten.
 */

#include "testdiscovery.h"
#include "vislib/ClusterDiscoveryService.h"
#include "vislib/SystemInformation.h"
#include "vislib/Trace.h"

#include <iostream>

#include "testhelper.h"


class MyListener : public vislib::net::ClusterDiscoveryListener {
    
    virtual void OnNodeFound(const vislib::net::ClusterDiscoveryService& src, 
            const vislib::net::SocketAddress& addr);
};


/*
 * MyListener::OnNodeFound
 */
void MyListener::OnNodeFound(const vislib::net::ClusterDiscoveryService& src, 
        const vislib::net::SocketAddress& addr) {
    std::cout << addr.ToStringA() << " was found for \"" << src.GetName() 
        << "\"" << std::endl;
}


void TestClusterDiscoveryService(void) {
    using namespace vislib;
    using namespace vislib::net;
    using namespace vislib::sys;

    UINT oldLevel = Trace::GetInstance().GetLevel();
    Trace::GetInstance().SetLevel(Trace::LEVEL_ERROR);

    MyListener myListener;

    // TODO: MIDL_uhyper_crowbar
    Socket::Startup();

    ClusterDiscoveryService cds("testnet", 
        28181, 
        IPAddress("129.69.215.255"), 
        SocketAddress(SocketAddress::FAMILY_INET, IPAddress(SystemInformation::ComputerNameA()), 12345));
    cds.AddListener(&myListener);
    cds.Start();
    Thread::Sleep(60 * 1000);
    cds.Stop();

    Trace::GetInstance().SetLevel(oldLevel);
    Socket::Cleanup();
}
