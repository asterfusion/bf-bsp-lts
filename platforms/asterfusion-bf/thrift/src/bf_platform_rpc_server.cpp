/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
#include <iostream>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>
#include "config.h"
#include "bf_platform_rpc_server.h"
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TSimpleServer.h>
#include <thrift/server/TThreadedServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/processor/TMultiplexedProcessor.h>

using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;

#ifdef HAVE_THRIFT_STDCXX_H
#include <thrift/stdcxx.h>
namespace stdcxx = ::apache::thrift::stdcxx;
#else
namespace stdcxx = boost;
#endif

using ::stdcxx::shared_ptr;

#include <pthread.h>

// All the supported Thrift service handlers :
#include "pltfm_mgr_rpc_server.ipp"
#include "pltfm_pm_rpc_server.ipp"

static pthread_mutex_t cookie_mutex;
static pthread_cond_t cookie_cv;
static char rpc_server_listen_point[16];
static void *cookie;

/*
 * Thread wrapper for starting the server
 */

static void *bf_platform_rpc_server_thread (
    void *)
{
    std::string address = rpc_server_listen_point;
    int port = BF_PLATFORM_RPC_SERVER_PORT;

    std::shared_ptr<pltfm_pm_rpcHandler>
    pltfm_pm_rpc_handler (
        new pltfm_pm_rpcHandler());

    std::shared_ptr<pltfm_mgr_rpcHandler>
    pltfm_mgr_rpc_handler (
        new pltfm_mgr_rpcHandler());

    std::shared_ptr<TMultiplexedProcessor> processor (
        new TMultiplexedProcessor());
    processor->registerProcessor (
        "pltfm_pm_rpc",
        std::shared_ptr<TProcessor> (
            new pltfm_pm_rpcProcessor (
                pltfm_pm_rpc_handler)));
    processor->registerProcessor (
        "pltfm_mgr_rpc",
        std::shared_ptr<TProcessor> (
            new pltfm_mgr_rpcProcessor (
                pltfm_mgr_rpc_handler)));
    std::shared_ptr<TServerTransport>
    serverTransport (new TServerSocket (address, port));
    std::shared_ptr<TTransportFactory>
    transportFactory (
        new TBufferedTransportFactory());
    std::shared_ptr<TProtocolFactory>
    protocolFactory (
        new TBinaryProtocolFactory());

    TThreadedServer server (
        processor, serverTransport, transportFactory,
        protocolFactory);

    pthread_mutex_lock (&cookie_mutex);
    cookie = (void *)processor.get();
    pthread_cond_signal (&cookie_cv);
    pthread_mutex_unlock (&cookie_mutex);

    server.serve();

    return NULL;
}

int bf_platform_rpc_server_rmv (void *processor)
{
    std::cerr << "Stopping BF-PLATFORM RPC server\n";

    TMultiplexedProcessor *processor_ =
        (TMultiplexedProcessor *)processor;
    processor_->registerProcessor ("pltfm_pm_rpc",
                                   std::shared_ptr<TProcessor>());
    processor_->registerProcessor ("pltfm_mgr_rpc",
                                   std::shared_ptr<TProcessor>());

    return 0;
}

static pthread_t bf_pltfm_rpc_thread_id = 0;

extern "C" {
    static void bf_pltfm_parse_rlp (const char *str,
                                    size_t l)
    {
        int i = 0;
        bool listen_point_type_ip = false;
        char *if_addr, *listen_point = NULL;
        struct ifaddrs *ifap, *ifa;

        assert (!(str == NULL));

        for (i = 0; i < (int)(l - 1); i ++) {
            if (isspace (str[i])) {
                continue;
            }
            if (isdigit (str[i]) || isalpha (str[i])) {
                listen_point = (char *)&str[i];
                if (isdigit (str[i])) {
                    /* The last character could be either EOF or LF, thus should be stripped. */
                    listen_point_type_ip = true;
                }
                listen_point[strlen(listen_point) - 1] = '\0';
                break;
            }
        }

        if (!listen_point) {
            goto default_ip;
        }

        if (listen_point_type_ip) {
            memcpy (&rpc_server_listen_point[0], listen_point, strlen(listen_point));
        } else {
            /* listen_point_type is an interface, convert interface(lo/ma1/enps0f0) to IP address. */
            if (getifaddrs (&ifap) == -1) {
                goto default_ip;
            }
            for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
                if (strcmp (ifa->ifa_name, listen_point) == 0 && ifa->ifa_addr->sa_family == AF_INET) {
                    if_addr = inet_ntoa (((struct sockaddr_in *) ifa->ifa_addr)->sin_addr);
                    memcpy (&rpc_server_listen_point[0], if_addr, strlen(if_addr));
                }
            }
            freeifaddrs (ifap);
        }
        return;

default_ip:
        /* Default to 127.0.0.1/lo */
        memcpy (&rpc_server_listen_point[0],
            BF_PLATFORM_RPC_SERVER_ADDRESS, strlen(BF_PLATFORM_RPC_SERVER_ADDRESS));
    }

    static void bf_pltfm_get_rpc_listen_point () {
        // Initialize all the sub modules
        FILE *fp = NULL;
        char entry[128] = {0};

        fp = fopen ("/etc/platform.conf", "r");
        if (!fp) {
            fp = fopen("/usr/share/sonic/platform/platform.conf", "r");
        }
        if (!fp) {
            exit (0);
        } else {
            while (fgets (entry, 128, fp)) {
                char *p;
                if (entry[0] == '#') {
                    continue;
                }

                p = strstr (entry, "rpc-listen-point:");
                if (p) {
                    /* Find Platform */
                    bf_pltfm_parse_rlp (p + 17, strlen (p + 17));
                }
            }
            fclose (fp);
        }
    }

    int bf_platform_rpc_server_start (void
                                      **server_cookie)
    {
        pthread_mutex_init (&cookie_mutex, NULL);
        pthread_cond_init (&cookie_cv, NULL);

        bf_pltfm_get_rpc_listen_point();

        std::cerr <<
                  "Starting BF-PLATFORM RPC server on : "
                  << rpc_server_listen_point << ": " << BF_PLATFORM_RPC_SERVER_PORT << std::endl;

        *server_cookie = NULL;

        /* pthread_attr_t attr;
         pthread_attr_init(&attr);
         int rc = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
         std::cerr << "attr_setdetachedstate " << rc << std::endl;*/

        int status = pthread_create (
                         &bf_pltfm_rpc_thread_id, NULL,
                         bf_platform_rpc_server_thread, NULL);

        if (status) {
            return status;
        }

        pthread_mutex_lock (&cookie_mutex);
        while (!cookie) {
            pthread_cond_wait (&cookie_cv, &cookie_mutex);
        }
        pthread_mutex_unlock (&cookie_mutex);

        *server_cookie = cookie;

        pthread_mutex_destroy (&cookie_mutex);
        pthread_cond_destroy (&cookie_cv);

        return 0;
    }

    int bf_platform_rpc_server_end (void *processor)
    {
        int ret = 0;

        bf_platform_rpc_server_rmv (processor);

        if (bf_pltfm_rpc_thread_id) {
            ret = pthread_cancel (bf_pltfm_rpc_thread_id);
            if (ret == 0) {
                pthread_join (bf_pltfm_rpc_thread_id, NULL);
            }
            bf_pltfm_rpc_thread_id = 0;
        }
        return 0;
    }

    int add_platform_to_rpc_server (void *processor)
    {
        std::cerr <<
                  "Adding Thrift service for bf-platforms to server\n";

        std::shared_ptr<pltfm_pm_rpcHandler>
        pltfm_pm_rpc_handler (
            new pltfm_pm_rpcHandler());

        std::shared_ptr<pltfm_mgr_rpcHandler>
        pltfm_mgr_rpc_handler (
            new pltfm_mgr_rpcHandler());

        TMultiplexedProcessor *processor_ =
            (TMultiplexedProcessor *)processor;

        processor_->registerProcessor (
            "pltfm_pm_rpc",
            std::shared_ptr<TProcessor> (
                new pltfm_pm_rpcProcessor (
                    pltfm_pm_rpc_handler)));
        processor_->registerProcessor (
            "pltfm_mgr_rpc",
            std::shared_ptr<TProcessor> (
                new pltfm_mgr_rpcProcessor (
                    pltfm_mgr_rpc_handler)));

        return 0;
    }

    int rmv_platform_from_rpc_server (void
                                      *processor)
    {
        std::cerr <<
                  "Removing Thrift service for bf-platforms from server\n";

        TMultiplexedProcessor *processor_ =
            (TMultiplexedProcessor *)processor;
        processor_->registerProcessor ("pltfm_pm_rpc",
                                       std::shared_ptr<TProcessor>());
        processor_->registerProcessor ("pltfm_mgr_rpc",
                                       std::shared_ptr<TProcessor>());

        return 0;
    }
}
