#pragma once

#include <atomic>
#include "./rdma_channel.h"


class RdmaPoller
{
public:
    RdmaPoller() {
        InitContext();
        this->listen_fd_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        this->set_ready(false);
        this->set_finished(false);
        poll_thread = utils::make_unique<std::thread>(
                      [this] { PollForever(); });
    }
    static RdmaPoller* Get() {
        static RdmaPoller poller;
        return &poller;
    }
    ~RdmaPoller() {
        this->set_finished(true);
        close(this->listen_fd_);
        poll_thread->join();
    }

    void PollForever();

    int Listen(int32_t tcp_port, const size_t& backlog = 32);
    RdmaChannel* Accept();
    int ib_port() const {
        return ib_port_;
    }
    ibv_context* context() const {
        return context_;
    }
    ibv_cq* completion_queue() const {
        return completion_queue_;
    }
    ibv_pd* protection_domain() const {
        return protection_domain_;
    }
    bool ready() {
        return ready_.load(std::memory_order_acquire);
    }
    void set_ready(const bool& ready) {
        ready_.store(ready, std::memory_order_release);
    }
    bool finished() {
        return finished_.load(std::memory_order_acquire);
    }
    void set_finished(const bool& finished) {
        finished_.store(finished, std::memory_order_release);
    }
    ibv_gid gid() {
        return gid_;
    }
    int sgid_idx() {
        return sgid_idx_;
    }
    uint32_t snp() {
        return snp_;
    }
    uint64_t iid() {
        return iid_;
    }
    uint64_t max_num_queue_entries() {
        return dev_attr_.max_cqe;
    }
protected:
    void InitContext();
    void ExitContext();
    void InitQueuePair();
    void SetQueuePairRTR();
    void SetQueuePairRTS();
private:
    int32_t listen_fd_;
    uint32_t timeout_;
    ibv_context* context_;
    ibv_cq* completion_queue_;
    ibv_pd* protection_domain_;
    ibv_comp_channel* comp_channel_;
    ibv_device* dev_;
    ibv_device_attr dev_attr_;
    int ib_port_;
    std::atomic<bool> ready_;
    std::atomic<bool> finished_;
    std::unique_ptr<std::thread> poll_thread;
    ibv_gid gid_;
    int sgid_idx_;
    int gid_idx_;
    uint32_t snp_;
    uint64_t iid_;
};

