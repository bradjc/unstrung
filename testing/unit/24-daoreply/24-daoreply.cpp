#include <stdio.h>
#include <stdlib.h>
#include "iface.h"

#include "fakeiface.h"

/* refactored because test case 27 uses this */
#include "../24-daoreply/dioA.cpp"
#include "../24-daoreply/daoackA.cpp"

int main(int argc, char *argv[])
{
        rpl_debug *deb = new rpl_debug(true, stdout);
        deb->want_time_log = false;

        dioA_step(deb, "../OUTPUTS/24-node-E-out.pcap");
        daoackA_step(deb, "../INPUTS/daoack-A-ripple1.pcap", NULL);

	exit(0);
}


