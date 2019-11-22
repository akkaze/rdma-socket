#include "work_request.h"
bool WorkRequest::AddBytes(const size_t nbytes) {
    completed_bytes_ += nbytes;
    if (completed_bytes_ == size_in_bytes_) {
        //done_ = true;
        if (work_type_ == kRecv) {
            std::memcpy(ptr_, extra_data_, size_in_bytes_);
        }
        Notify();
        return true;
    }
    return false;
}


