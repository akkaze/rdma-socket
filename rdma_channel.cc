#include "logging.h"
#include "rdma_channel.h"
#include "rdma_poller.h"
#include "rdma_utils.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace dmlc;

RdmaChannel::RdmaChannel():RdmaChannel(RdmaPoller::Get()) {
}

RdmaChannel::RdmaChannel(RdmaPoller* poller) :
            RdmaChannel(poller, kBufSize) {
}
RdmaChannel::RdmaChannel(RdmaPoller* poller, uint64_t buf_size)
                : poller_(poller), buf_size_(buf_size) {
    send_buf_ = new uint8_t[buf_size_];
    recv_buf_ = new uint8_t[buf_size_];
    num_comp_queue_entries_ = poller->max_num_queue_entries();
    InitRdmaContext();
}


void RdmaChannel::InitRdmaContext() {
    CHECK_NOTNULL(poller_->protection_domain());
    CHECK_NOTNULL(send_memory_region_ = ibv_reg_mr(poller_->protection_domain(),
                  send_buf_, buf_size_, IBV_ACCESS_LOCAL_WRITE|IBV_ACCESS_REMOTE_WRITE));
    CHECK_NOTNULL(recv_memory_region_ = ibv_reg_mr(poller_->protection_domain(),
                  recv_buf_, buf_size_, IBV_ACCESS_LOCAL_WRITE|IBV_ACCESS_REMOTE_WRITE));
    CreateQueuePair();
    CreateLocalAddr();
}
void RdmaChannel::ExitRdmaContext() {
    CHECK_EQ(ibv_destroy_qp(queue_pair_), 0);
    CHECK_EQ(ibv_dereg_mr(send_memory_region_), 0);
    CHECK_EQ(ibv_dereg_mr(recv_memory_region_), 0);
}
void RdmaChannel::AfterConnection() {
    InitQueuePair();
    EnableQueuePairForRecv();
    EnableQueuePairForSend();
}

int RdmaChannel::Connect(const std::string& hostname, const int32_t& port) {
    sockaddr_in peer_addr;
    std::memset(&peer_addr, 0, sizeof(peer_addr));
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_addr.s_addr = inet_addr(hostname.c_str());
    peer_addr.sin_port = htons(port);
    int32_t peer_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (connect(peer_fd, (struct sockaddr*)&peer_addr,
                sizeof(peer_addr)) != 0) {
        LOG(INFO) << strerror(errno);
        return 1;
    }
    CHECK_GE(peer_fd, 0);
    CHECK_EQ(send(peer_fd , (char*)&self_addr_,
             sizeof(self_addr_), 0), sizeof(self_addr_))
             << "Could not send local address to peer";
    CHECK_EQ(recv(peer_fd , (char*)&peer_addr_,
             sizeof(peer_addr_), 0), sizeof(peer_addr_))
             << "Could not receive local address to peer";
    AfterConnection();
    return 0;
}

void RdmaChannel::CreateQueuePair() {
    ibv_qp_init_attr qp_init_attr;
    memset(&qp_init_attr, 0, sizeof(qp_init_attr));
    qp_init_attr.send_cq = poller_->completion_queue();
    qp_init_attr.recv_cq = poller_->completion_queue();
    qp_init_attr.qp_type = IBV_QPT_RC;
    qp_init_attr.cap.max_send_wr = 100;
    qp_init_attr.cap.max_recv_wr = 100;
    qp_init_attr.cap.max_send_sge = 1;
    qp_init_attr.cap.max_recv_sge = 1;
    qp_init_attr.cap.max_inline_data = 0;
    queue_pair_ = ibv_create_qp(poller_->protection_domain(),
                                &qp_init_attr);
    CHECK_NOTNULL(queue_pair_);
    poller_->set_ready(true);
}

void RdmaChannel::InitQueuePair() {
    ibv_qp_attr *attr = new ibv_qp_attr;
    memset(attr, 0, sizeof(*attr));

    attr->qp_state          = IBV_QPS_INIT;
    attr->pkey_index        = 0;
    attr->port_num          = poller_->ib_port();
    attr->qp_access_flags   = IBV_ACCESS_REMOTE_WRITE|IBV_ACCESS_REMOTE_READ;

    CHECK_EQ(ibv_modify_qp(queue_pair_, attr,
       IBV_QP_STATE|IBV_QP_PKEY_INDEX|IBV_QP_PORT|IBV_QP_ACCESS_FLAGS),0) 
            << "Could not modify QP to INIT, ibv_modify_qp";
    delete attr;

}
void RdmaChannel::EnableQueuePairForRecv() {
    ibv_qp_attr* attr = new ibv_qp_attr;

    memset(attr, 0, sizeof(*attr));

    attr->qp_state              = IBV_QPS_RTR;
    attr->path_mtu              = IBV_MTU_2048;
    attr->dest_qp_num           = peer_addr_.qpn;
    attr->rq_psn                = peer_addr_.psn;
    attr->max_dest_rd_atomic    = 1;
    attr->min_rnr_timer         = 12;
    attr->ah_attr.is_global     = 1;
    attr->ah_attr.dlid          = peer_addr_.lid;
    attr->ah_attr.sl            = 0;
    attr->ah_attr.src_path_bits = 0;
    attr->ah_attr.port_num      = poller_->ib_port();
    attr->ah_attr.grh.dgid.global.subnet_prefix = peer_addr_.snp;
    attr->ah_attr.grh.dgid.global.interface_id = peer_addr_.iid;
    attr->ah_attr.grh.sgid_index = sgid_idx_;
    attr->ah_attr.grh.flow_label = 0;
    attr->ah_attr.grh.hop_limit = 255;
    CHECK_EQ(ibv_modify_qp(queue_pair_, attr, IBV_QP_STATE|IBV_QP_AV|
                           IBV_QP_PATH_MTU|IBV_QP_DEST_QPN|IBV_QP_RQ_PSN|
                           IBV_QP_MAX_DEST_RD_ATOMIC|IBV_QP_MIN_RNR_TIMER), 0)
                            << "Could not modify QP to RTR state";

    delete attr;
}

void RdmaChannel::EnableQueuePairForSend() {
    ibv_qp_attr *attr = new ibv_qp_attr;
    memset(attr, 0, sizeof *attr);

    attr->qp_state              = IBV_QPS_RTS;
    attr->timeout               = 14;
    attr->retry_cnt             = 7;
    attr->rnr_retry             = 7;    /* infinite retry */
    attr->sq_psn                = self_addr_.psn;
    attr->max_rd_atomic         = 1;

    CHECK_EQ(ibv_modify_qp(queue_pair_, attr,
             IBV_QP_STATE|IBV_QP_TIMEOUT|IBV_QP_RETRY_CNT|
             IBV_QP_RNR_RETRY|IBV_QP_SQ_PSN|IBV_QP_MAX_QP_RD_ATOMIC), 0)
             << "Could not modify QP to RTS state";
}

void RdmaChannel::CreateLocalAddr() {
    ibv_port_attr attr;
    ibv_query_port(poller_->context(), poller_->ib_port(), &attr);
    self_addr_.lid = attr.lid;
    self_addr_.qpn = queue_pair_->qp_num;
    self_addr_.psn = rand() & 0xffffff;
    this->sgid_idx_ = this->poller_->sgid_idx();
    self_addr_.snp = poller_->snp();
    self_addr_.iid = poller_->iid();
}

WorkCompletion RdmaChannel::ISend(const void* sendbuf_, size_t size) {
    uint64_t req_id = WorkRequestManager::Get()->
                     NewWorkRequest(kSend, sendbuf_, size, send_buf_);
    memcpy(send_buf_, sendbuf_, size);
    ibv_sge sge_list;
    memset(&sge_list, 0, sizeof(sge_list));
    sge_list.addr      = (uint64_t)send_buf_;
    sge_list.length    = size;
    sge_list.lkey      = send_memory_region_->lkey;
    ibv_send_wr send_wr;
    send_wr.wr_id       = req_id;
    send_wr.sg_list     = &sge_list;
    send_wr.num_sge     = 1;
    //send_wr.opcode      = IBV_WR_RDMA_WRITE_WITH_IMM;
    send_wr.opcode      = IBV_WR_SEND;
    send_wr.send_flags  = IBV_SEND_SIGNALED;
    send_wr.next        = NULL;

    ibv_send_wr *bad_wr;
    CHECK_EQ(ibv_post_send(queue_pair_, &send_wr, &bad_wr), 0)
             << "ibv_post_send failed.This is bad mkey";
    WorkCompletion wc(req_id);
    return wc;
}

WorkCompletion RdmaChannel::IRecv(void* recvbuf_, size_t size) {
    uint64_t req_id = WorkRequestManager::Get()->
                     NewWorkRequest(kRecv, recvbuf_, size, recv_buf_);
    ibv_sge sge_list;
    memset(&sge_list, 0, sizeof(sge_list));
    sge_list.addr      = (uint64_t)recv_buf_;
    sge_list.length    = size;
    sge_list.lkey      = recv_memory_region_->lkey;
    ibv_recv_wr recv_wr;
    recv_wr.wr_id       = req_id;
    recv_wr.sg_list     = &sge_list;
    recv_wr.num_sge     = 1;
    recv_wr.next        = NULL;

    ibv_recv_wr* bad_wr;
    CHECK_EQ(ibv_post_recv(queue_pair_, &recv_wr, &bad_wr),0)
             << "ibv_post_send failed.This is bad mkey";
    WorkCompletion wc(req_id);
//    memcpy(msg,buf_,size);
    return wc;
}


