/*
 * Copyright (C) 2009-2013 Michael Richardson <mcr@sandelman.ca>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <http://www.fsf.org/copyleft/gpl.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <sysexits.h>
#include <time.h>
#include <sys/types.h>
#include <signal.h>

#include "iface.h"
#include "dag.h"
#include "unstrung.h"

#define OPTION_SYSLOG 0x01
#define OPTION_STDERR 0x02
#define OPTION_SLEEP  0x03

char *progname;
static struct option const longopts[] =
{
    { "help",      0, 0, '?'},
    { "interface", 0, 0, 'i'},
    { "daemon",    0, 0, 'D'},
    { "prefix",    1, NULL, 'p'},
    { "instance",  1, NULL, 'I'},
    { "instanceid",1, NULL, 'I'},
    { "syslog",    0, NULL,  OPTION_SYSLOG},
    { "stderr",    0, NULL,  OPTION_STDERR},
    { "sleep",     1, NULL,  OPTION_SLEEP},
    { "interval",  1, NULL, 'W'},
    { "dagid",     1, NULL, 'G'},
    { "dagid",     1, NULL, 'G'},
    { "rank",      1, NULL, 'R'},
    { "kill",      0, 0, 'K'},
    { "verbose",   0, 0, 'v'},
    { "timelog",   0, 0, 't'},
    { name: 0 },
};

void usage()
{
    fprintf(stderr, "Usage: %s [-?] [-i ifname]\n", progname);
    fprintf(stderr,
            "\t [-p prefix] [--prefix prefix]   announce this IPv6 prefix in the destination option\n"
            "\t [-G dagid]  [--dagid dagid]     DAGid to use to announce, string or hexn\n"
            "\t [-R rank]   [--rank rank]       Initial rank to announce with\n"
            "\t [-I num]    [--instanceid num]  Instance ID (number)\n"
            "\t [-W msec]   [--interval msec]   Number of miliseconds between DIO\n"
            "\t [--verbose] [--timelog]         Turn on logging (with --time logged)\n"
            "\t [--syslog]  [--stderr]          Log to syslog and/or stderr\n"
            "\t [--sleep=secs]                  sleep secs before trying to talk to network\n"
        );
    exit(EX_USAGE);
}

bool check_dag(unsigned char opt, dag_network *dag)
{
    if(dag==NULL) {
	fprintf(stderr, "while processing '%c'; --dagid must preceed DODAG parameters\n",
                opt);
	usage();
    }
    return true;
}

const char *pidfilename="/var/run/sunshine.pid";
void killanydaemon(void)
{
    int pid;

    FILE *pidfile = fopen(pidfilename, "r");
    if(!pidfile) {
        fprintf(stderr, "could not found any running daemon\n");
        exit(1);
    }

    if(fscanf(pidfile, "%u", &pid) != 1) {
        fprintf(stderr, "could not decipher pid file contents\n");
        exit(2);
    }
    fclose(pidfile);

    printf("Killing process %u\n", pid);

    if(kill(pid, SIGTERM) != 0) {
        perror("kill");
        exit(3);
    }

    unlink(pidfilename);

    exit(0);
}

int main(int argc, char *argv[])
{
    int c;
    char *ifname = NULL;
    bool verbose = false;
    bool bedaemon = false;

    progname = argv[0];
    bool devices_scanned = false;

    network_interface *iface = NULL;
    rpl_debug *deb = new rpl_debug(false, stderr);
    deb->set_progname(progname);

    dag_network::init_stats();
    dag_network *dag = NULL;

    tzset();
    time_t now = time(NULL);
    char *today = ctime(&now);
    deb->info("PANDORA unstrung version %u.%u (%s) starting at %s",
              PANDORA_VERSION_MAJOR, PANDORA_VERSION_MINOR,
              BUILDNUMBER, today);

    network_interface::scan_devices(deb, true);
    devices_scanned=true;

    while((c = getopt_long(argc, argv, "KDG:I:R:W:i:hp:?v", longopts, 0)) != EOF) {
	switch(c) {
	default:
	    fprintf(stderr, "Unknown option: %s\n", argv[optind-1]);
	    /* FALLTHROUGH */
	case 'h':
	case '?':
	    usage();
	    /* NORETURN */

        case 'D':
            bedaemon = true;
            break;

        case 'K':
            killanydaemon();
            exit(0);

        case 't':
            deb->want_time_log = true;
            break;

        case OPTION_STDERR:
            deb->log_file   = true;
            break;

        case OPTION_SYSLOG:
            deb->log_syslog = true;
            break;

        case OPTION_SLEEP:
            {
                unsigned int doze=atoi(optarg);
                if(doze > 0) sleep(doze);
            }
            break;

        case 'p':
        {
            ip_subnet prefix;

            err_t e = ttosubnet(optarg, strlen(optarg),
                                AF_INET6, &prefix);

	    if(e) {
		fprintf(stderr, "invalid prefix string: %s\n", optarg);
		usage();
	    }
	    check_dag(c, dag);
            dag->set_prefix(prefix);
            dag->set_grounded(true);
            dag->set_dagrank(1);
            if (!iface){
            	dag->add_all_interfaces();
            }else
            {
            	dag->addselfprefix(iface);
            }
        }
        break;

        case 'I':
	    check_dag(c, dag);
            dag->set_instanceid(atoi(optarg));
            break;

        case 'W':
	    check_dag(c, dag);
            dag->set_interval(atoi(optarg));
            break;

        case 'R':
	    check_dag(c, dag);
	    {
		int rank = atoi(optarg);
		dag->set_dagrank(rank);
		//fprintf(stderr, "setting rank for %s to %u\n", dag->get_dagName(),
		//    rank, dag->get_dagRank());
		dag->set_sequence(1);
	    }
            break;

        case 'G':
	    dag = new dag_network(optarg, deb);
	    dag->set_active();
            dag->set_interval(5000);
            break;

        case 'v':
            verbose=true;
            deb->set_verbose(stderr);
            break;

        case 'i':
            check_dag(c, dag);
        	if(dag->mPrefixSet){
        		fprintf(stderr, "interface must preceed prefix parameter\n");
        		usage();
        	}
        	iface = network_interface::find_by_name(optarg);
            if(!iface) {
                deb->log("Can not find interface %s\n", optarg);
            } else {
		deb->verbose("Setting up interface[%d] %s\n",
			     iface->get_if_index(),
			     iface->get_if_name());

                iface->set_debug(deb);
                iface->setup();
            }
            break;
	}
    }

    if(!iface) {
	deb->info("running on all interfaces\n");
	network_interface::setup_all_if();
    }

    if(dag==NULL) {
	fprintf(stderr, "must set at least one DAG to announce\n");
	usage();
    }

    /* should check for already running instance before stomping PID file */

    if(bedaemon) {
        if(daemon(0, 1)!=0) {
            perror("daemon");
            exit(5);
        }
    }

    FILE *pidfile = fopen(pidfilename, "w");
    int   mypid = getpid();
    if(pidfile) {
        fprintf(pidfile, "%u\n", mypid);
        fclose(pidfile);
    } else {
        fprintf(stderr, "Can not write pid=%u to %s: %s\n",
                mypid, pidfilename, strerror(errno));
    }

    if(dag == NULL) {
        fprintf(stderr, "must specify a DAG to manage\n");
        usage();
    }

    dag->set_debug(deb);
    dag->schedule_dio(IMMEDIATELY);

    network_interface::main_loop(stderr, deb);

    exit(0);
}


/*
 * Local Variables:
 * mode: c
 * c-basic-offset:4
 * c-style: whitesmith
 * End:
 */
