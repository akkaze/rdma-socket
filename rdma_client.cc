#include "rdma_channel.h"
#include "rdma_poller.h"
#include "logging.h"
#include "utils.h"
#include <string>
#include <iostream>
#include <thread>
#include <mutex>
using namespace dmlc;
int main(int argc, char const *argv[])
{
    int N = 100;
    int M = 100;
    std::vector<std::thread> threads(N);
    std::mutex lock;
    for (int n = 0; n < N; n++) {
        threads[n] = std::thread([&, n] {
            RdmaChannel* client_channel = new RdmaChannel;
            lock.lock();
            client_channel->Connect("127.0.0.1", 9996);
            lock.unlock();
            int server_idx = -1;
            for(int i = 0; i < M; i ++) {
                std::string str = SPrintf("Hello there %4d %4d", n, i);
                std::string str1 = SPrintf("Hello from echo %4d  %4d", n, i);
                const char* c_str = str.c_str();
                char* c_str1 = new char[str1.size()];
                auto wc1 = client_channel->ISend(
                    reinterpret_cast<const void *>(c_str), str.size());
                auto wc2 = client_channel->IRecv(c_str1, str1.size());
                wc1.Wait();
                wc2.Wait();
                std::string str1_ = c_str1;
                auto idx = ParseIndex(str1);
                if (i == 0) server_idx = std::get<0>(idx);
                LOG(INFO) << std::get<0>(idx) << '\t' << std::get<1>(idx);
                CHECK_EQ(server_idx, std::get<0>(idx)) << server_idx << '\t' << std::get<0>(idx);
                CHECK_EQ(i, std::get<1>(idx)) << i << '\t' << std::get<1>(idx);
                delete[] c_str1;
            }
            //delete client_channel;
        LOG(INFO) << std::to_string(n) << " finished";
        });
    }
    for (int n = 0; n < N; n++) {
        threads[n].join();
    }
    return 0;
}
