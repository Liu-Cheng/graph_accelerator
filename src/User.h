#ifndef __USER_H__
#define __USER_H__

// We need this file for defining types used in both Request.h and common.h
// This is used to identify the memory access port
enum PortType{
    INSPECT_DEPTH_READ = 0,
    EXPAND_RPAO_READ,
    EXPAND_CIAO_READ,
    EXPAND_RPAI_READ,
    EXPAND_CIAI_READ,
    EXPAND_DEPTH_READ,
    EXPAND_DEPTH_WRITE,
};

#endif
