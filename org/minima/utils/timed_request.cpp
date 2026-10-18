#include "org/minima/utils/timed_request.hpp"

#include <chrono>

#include "org/minima/objects/base/mini_data.hpp"

namespace org {
namespace minima {
namespace utils {

// RequestUnit implementations

TimedRequest::RequestUnit::RequestUnit(const org::minima::objects::base::MiniData& zData)
    : mTimeMilli(0), mDataID(nullptr) {
    auto now = std::chrono::system_clock::now();
    auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    mTimeMilli = static_cast<std::int64_t>(ms);

    // Store a copy of the MiniData
    mDataID = std::make_unique<org::minima::objects::base::MiniData>(zData);
}

TimedRequest::RequestUnit::~RequestUnit() = default;
TimedRequest::RequestUnit::RequestUnit(RequestUnit&&) noexcept = default;
TimedRequest::RequestUnit& TimedRequest::RequestUnit::operator=(RequestUnit&&) noexcept = default;

// TimedRequest implementations

TimedRequest::TimedRequest() : mPreviousRequests() {}

TimedRequest::~TimedRequest() = default;
TimedRequest::TimedRequest(TimedRequest&&) noexcept = default;
TimedRequest& TimedRequest::operator=(TimedRequest&&) noexcept = default;

bool TimedRequest::sendNow(const org::minima::objects::base::MiniData& zDataID) {
    // Have we recently sent this request..
    for (const auto& requnit : mPreviousRequests) {
        if (requnit.mDataID && requnit.mDataID->isEqual(zDataID)) {
            return false;
        }
    }
    return true;
}

} // namespace utils
} // namespace minima
} // namespace org