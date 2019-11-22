#pragma once
#include <infiniband/verbs.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include "work_request.h"
const int kNumCompQueueEntries = 100;
const uint64_t kBufSize = 1 << 10U;

struct __attribute__ ((packed)) RdmaAddr {
    uint32_t lid;
    uint32_t qpn;
    uint32_t psn;
    uint32_t rkey;
    uint64_t snp;
    uint64_t iid;
    uint64_t raddr;
};

inline void PrintAddr(RdmaAddr addr) {
    std::string addr_str;
    addr_str += std::to_string(addr.lid);
    addr_str += "\t" + std::to_string(addr.qpn);
    addr_str += "\t" + std::to_string(addr.psn);
    addr_str += "\t" + std::to_string(addr.snp);
    addr_str += "\t" + std::to_string(addr.iid);
    LOG(INFO) << addr_str;
}

class RdmaPoller;
struct RdmaChannel {
    RdmaChannel();
    RdmaChannel(RdmaPoller* poller, uint64_t buf_size);
    RdmaChannel(RdmaPoller* poller);
    ~RdmaChannel() {
        delete[] send_buf_;
        delete[] recv_buf_;
    }
    WorkCompletion ISend(const void* msg, size_t size);
    WorkCompletion IRecv(void* msg, size_t size);
    void set_peer_addr(const RdmaAddr& peer_addr) {
        peer_addr_ = peer_addr;
    }
    RdmaAddr addr() const {
        return self_addr_;
    }

    RdmaAddr peer_addr() const {
        return peer_addr_;
    }
    int Connect(const std::string& hostname, const int32_t& port);
    void AfterConnection();
protected:
    void InitRdmaContext();
    void ExitRdmaContext();
    void CreateQueuePair();
    void CreateLocalAddr();
    void InitQueuePair();
    void EnableQueuePairForSend();
    void EnableQueuePairForRecv();
    uint8_t* send_buf_;
    uint8_t* recv_buf_;
    uint64_t buf_size_;
    RdmaAddr self_addr_;
    RdmaAddr peer_addr_;
    ibv_qp* queue_pair_;
    ibv_mr* send_memory_region_;
    ibv_mr* recv_memory_region_;
    RdmaPoller* poller_;
    int sgid_idx_;
    int gid_idx_;
    int num_comp_queue_entries_;
};

