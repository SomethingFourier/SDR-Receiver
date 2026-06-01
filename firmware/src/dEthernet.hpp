#ifndef dEthernet_H
#define dEthernet_H

#include <pico/types.h>

class dEthernet {
public:
	dEthernet();

	// Public API
	void Init();

private:
	// nothing yet
	
    dEthernet(const dEthernet&);
    void operator=(const dEthernet&);
};

extern dEthernet g_Ethernet;

#endif  // dEthernet_H