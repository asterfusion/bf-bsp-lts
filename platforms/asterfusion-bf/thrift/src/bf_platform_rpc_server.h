/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
/*
 * C/C++ header file for calling server start function from C code
 */

#ifndef _BF_PLATFORM_RPC_SERVER_H_
#define _BF_PLATFORM_RPC_SERVER_H_

#ifdef __cplusplus
extern "C" {
#endif

#define BF_PLATFORM_RPC_SERVER_PORT 9095

extern int start_bf_platform_rpc_server (
    void **server_cookie);

#ifdef __cplusplus
}
#endif

#endif /* _BF_PLATFORM_RPC_SERVER_H_ */
