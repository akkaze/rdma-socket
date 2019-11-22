#include "rdma_channel.h"
#include "rdma_poller.h"
#include "logging.h"
#include "utils.h"
#include <string>
#include <iostream>
#include <thread>
#include <mutex>
#include <tuple>
#include <regex>
using namespace dmlc;
int main(int argc, char const *argv[])
{
    int N = 100;
    int M = 100;
    RdmaPoller::Get()->Listen(9996);
    std::vector<std::thread> threads(N);
    std::mutex lock;
    for (int n = 0; n < N; n++) {
        threads[n] = std::thread([&, n] {
            RdmaChannel* client_channel = new RdmaChannel;
            lock.lock();
            RdmaChannel* server_channel = RdmaPoller::Get()->Accept();
            lock.unlock();
            int client_idx = -1;
            for(int i = 0; i < M; i ++) {
                std::string str = SPrintf("Hello there %4d  %4d", n, i);
                std::string str1 = SPrintf("Hello from echo %4d  %4d", n, i);
                char* c_str = new char[str.size()];
                const char* c_str1 = str1.c_str();
                auto wc1 = server_channel->IRecv(c_str, str.size());
                auto wc2 = server_channel->ISend(reinterpret_cast<const void*>(
                        str1.c_str()), str1.size());
                wc1.Wait();
                wc2.Wait();
                std::string str_ = c_str;
                auto idx = ParseIndex(str_);
                if (i == 0) client_idx = std::get<0>(idx);
                LOG(INFO) << std::get<0>(idx) << '\t' << std::get<1>(idx);
                CHECK_EQ(client_idx, std::get<0>(idx)) << client_idx << '\t' << std::get<0>(idx);
                CHECK_EQ(i, std::get<1>(idx)) << i << '\t' << std::get<1>(idx);
                delete[] c_str;
            }
            //delete server_channel;
        });
    }
    for (int n = 0; n < N; n++) {
        threads[n].join();
    }
    return 0;
}
