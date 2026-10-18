#pragma once

#include <cstdint>
#include <memory>
#include <vector>

// Forward declaration for project dependency (do not include header here)
namespace org { namespace minima { namespace objects { namespace base {
class MiniData;
}}}}

namespace org {
namespace minima {
namespace utils {

class TimedRequest {
public:
    class RequestUnit {
    public:
        std::int64_t mTimeMilli;
        std::unique_ptr<org::minima::objects::base::MiniData> mDataID;

        explicit RequestUnit(const org::minima::objects::base::MiniData& zData);

        // Needed because of unique_ptr to forward-declared type
        ~RequestUnit();
        RequestUnit(RequestUnit&&) noexcept;
        RequestUnit& operator=(RequestUnit&&) noexcept;

        // No copying
        RequestUnit(const RequestUnit&) = delete;
        RequestUnit& operator=(const RequestUnit&) = delete;
    };

    // Public for parity with Java code
    std::vector<RequestUnit> mPreviousRequests;

    TimedRequest();

    ~TimedRequest();
    TimedRequest(TimedRequest&&) noexcept;
    TimedRequest& operator=(TimedRequest&&) noexcept;

    TimedRequest(const TimedRequest&) = delete;
    TimedRequest& operator=(const TimedRequest&) = delete;

    bool sendNow(const org::minima::objects::base::MiniData& zDataID);
};

} // namespace utils
} // namespace minima
} // namespace org