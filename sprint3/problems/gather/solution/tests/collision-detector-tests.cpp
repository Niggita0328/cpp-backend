#define _USE_MATH_DEFINES
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <iomanip>

#include "../src/collision_detector.h"

namespace Catch {
    template<>
    struct StringMaker<collision_detector::GatheringEvent> {
        static std::string convert(collision_detector::GatheringEvent const& value) {
            std::ostringstream tmp;
            tmp << std::fixed << std::setprecision(10)
                << "{" << value.item_id << ", " << value.gatherer_id << ", "
                << value.sq_distance << ", " << value.time << "}";
            return tmp.str();
        }
    };
}  // namespace Catch


class GatheringEventVectorMatcher : public Catch::Matchers::MatcherBase<std::vector<collision_detector::GatheringEvent>> {
public:
    GatheringEventVectorMatcher(std::vector<collision_detector::GatheringEvent> const& comparator)
        : comparator_{ comparator } {}

    bool match(std::vector<collision_detector::GatheringEvent> const& v) const override {
        if (v.size() != comparator_.size()) {
            return false;
        }
        auto sorted_v = v;
        auto sorted_comparator = comparator_;

        auto sort_rule = [](const auto& a, const auto& b) {
            if (std::abs(a.time - b.time) > 1e-10) {
                return a.time < b.time;
            }
            if (a.item_id != b.item_id) {
                return a.item_id < b.item_id;
            }
            return a.gatherer_id < b.gatherer_id;
        };

        std::sort(sorted_v.begin(), sorted_v.end(), sort_rule);
        std::sort(sorted_comparator.begin(), sorted_comparator.end(), sort_rule);

        return std::equal(sorted_v.begin(), sorted_v.end(), sorted_comparator.begin(),
            [](const auto& a, const auto& b) {
                const double tolerance = 1e-9;
                return a.item_id == b.item_id &&
                       a.gatherer_id == b.gatherer_id &&
                       std::abs(a.sq_distance - b.sq_distance) < tolerance &&
                       std::abs(a.time - b.time) < tolerance;
            });
    }

    std::string describe() const override {
        std::ostringstream ss;
        ss << "is equivalent to " << Catch::Detail::stringify(comparator_);
        return ss.str();
    }

private:
    std::vector<collision_detector::GatheringEvent> const& comparator_;
};

inline GatheringEventVectorMatcher IsEquivalentTo(std::vector<collision_detector::GatheringEvent> const& expected) {
    return GatheringEventVectorMatcher(expected);
}


class TestProvider : public collision_detector::ItemGathererProvider {
public:
    TestProvider(std::vector<collision_detector::Item> items, std::vector<collision_detector::Gatherer> gatherers)
        : items_(std::move(items)), gatherers_(std::move(gatherers)) {}

    size_t ItemsCount() const override {
        return items_.size();
    }
    collision_detector::Item GetItem(size_t idx) const override {
        return items_.at(idx);
    }
    size_t GatherersCount() const override {
        return gatherers_.size();
    }
    collision_detector::Gatherer GetGatherer(size_t idx) const override {
        return gatherers_.at(idx);
    }

private:
    std::vector<collision_detector::Item> items_;
    std::vector<collision_detector::Gatherer> gatherers_;
};

SCENARIO("FindGatherEvents") {
    using namespace collision_detector;
    using geom::Point2D;

    WHEN("No items or gatherers") {
        THEN("No events are generated") {
            TestProvider provider{ {}, {} };
            CHECK(FindGatherEvents(provider).empty());
        }
        THEN("No events are generated for items but no gatherers") {
            std::vector<Item> items = { {{0, 0}, 0.5} };
            TestProvider provider{ items, {} };
            CHECK(FindGatherEvents(provider).empty());
        }
        THEN("No events are generated for gatherers but no items") {
            std::vector<Gatherer> gatherers = { {{0, 0}, {1, 1}, 0.5} };
            TestProvider provider{ {}, gatherers };
            CHECK(FindGatherEvents(provider).empty());
        }
    }

    WHEN("Gatherer doesn't move") {
        THEN("No events are generated") {
            std::vector<Item> items = { {{0, 0}, 0.5} };
            std::vector<Gatherer> gatherers = { {{0, 0}, {0, 0}, 0.5} };
            TestProvider provider{ items, gatherers };
            CHECK(FindGatherEvents(provider).empty());
        }
    }

    WHEN("A single gatherer collects a single item") {
        THEN("A single event is generated for a direct hit") {
            std::vector<Item> items = { {{10, 0}, 0.5} };
            std::vector<Gatherer> gatherers = { {{0, 0}, {20, 0}, 0.5} };
            TestProvider provider{ items, gatherers };
            auto events = FindGatherEvents(provider);
            
            std::vector<GatheringEvent> expected = { {0, 0, 0.0, 0.5} };
            CHECK_THAT(events, IsEquivalentTo(expected));
        }

        THEN("Item is collected at the start point") {
            std::vector<Item> items = { {{0, 0}, 0.1} };
            std::vector<Gatherer> gatherers = { {{0, 0}, {10, 0}, 0.1} };
            TestProvider provider{ items, gatherers };
            auto events = FindGatherEvents(provider);
            
            std::vector<GatheringEvent> expected = { {0, 0, 0.0, 0.0} };
            CHECK_THAT(events, IsEquivalentTo(expected));
        }

        THEN("Item is collected at the end point") {
            std::vector<Item> items = { {{10, 0}, 0.1} };
            std::vector<Gatherer> gatherers = { {{0, 0}, {10, 0}, 0.1} };
            TestProvider provider{ items, gatherers };
            auto events = FindGatherEvents(provider);
            
            std::vector<GatheringEvent> expected = { {0, 0, 0.0, 1.0} };
            CHECK_THAT(events, IsEquivalentTo(expected));
        }

        THEN("Item is collected at the edge of the gatherer's width") {
            std::vector<Item> items = { {{5, 0.9}, 0.4} }; // Total radius = 0.5 + 0.4 = 0.9
            std::vector<Gatherer> gatherers = { {{0, 0}, {10, 0}, 0.5} };
            TestProvider provider{ items, gatherers };
            auto events = FindGatherEvents(provider);
            
            std::vector<GatheringEvent> expected = { {0, 0, 0.9*0.9, 0.5} };
            CHECK_THAT(events, IsEquivalentTo(expected));
        }
    }

    WHEN("No collection happens") {
        THEN("Item is too far from the movement path") {
            std::vector<Item> items = { {{5, 1.1}, 0.5} };
            std::vector<Gatherer> gatherers = { {{0, 0}, {10, 0}, 0.5} }; // radius sum = 1.0
            TestProvider provider{ items, gatherers };
            CHECK(FindGatherEvents(provider).empty());
        }

        THEN("Item's projection is behind the start point") {
            std::vector<Item> items = { {{-1, 0}, 0.5} };
            std::vector<Gatherer> gatherers = { {{0, 0}, {10, 0}, 0.5} };
            TestProvider provider{ items, gatherers };
            CHECK(FindGatherEvents(provider).empty());
        }

        THEN("Item's projection is past the end point") {
            std::vector<Item> items = { {{11, 0}, 0.5} };
            std::vector<Gatherer> gatherers = { {{0, 0}, {10, 0}, 0.5} };
            TestProvider provider{ items, gatherers };
            CHECK(FindGatherEvents(provider).empty());
        }
    }

    WHEN("Multiple items and gatherers") {
        THEN("Events are correct and sorted by time") {
            std::vector<Item> items = {
                /*0*/ {{2, 0}, 0.1},    // G0, t=2/20=0.1
                /*1*/ {{18, 0}, 0.1},   // G0, t=18/20=0.9
                /*2*/ {{-6, 10}, 0.1},  // G1, t=(-6 - (-10))/20 = 4/20=0.2
                /*3*/ {{15, 15}, 0.1}   // Not collected
            };
            std::vector<Gatherer> gatherers = {
                /*0*/ {{0, 0}, {20, 0}, 0.2},   // len=20
                /*1*/ {{-10, 10}, {10, 10}, 0.2} // len=20
            };
            TestProvider provider{ items, gatherers };
            auto events = FindGatherEvents(provider);

            std::vector<GatheringEvent> expected = {
                {0, 0, 0.0, 0.1},
                {2, 1, 0.0, 0.2},
                {1, 0, 0.0, 0.9}
            };
            
            CHECK_THAT(events, IsEquivalentTo(expected));
        }
    }
    
    WHEN("Complex scenarios testing geometry") {
        THEN("Diagonal movement, direct hit") {
            std::vector<Item> items = { {{5, 5}, 0.1} };
            std::vector<Gatherer> gatherers = { {{0, 0}, {10, 10}, 0.1} };
            TestProvider provider{ items, gatherers };
            auto events = FindGatherEvents(provider);
            
            std::vector<GatheringEvent> expected = { {0, 0, 0.0, 0.5} };
            CHECK_THAT(events, IsEquivalentTo(expected));
        }

        THEN("Horizontal movement, item is collected at max radius") {
            std::vector<Item> items = { {{5, 5}, 2.0} };
            std::vector<Gatherer> gatherers = { {{0, 0}, {10, 0}, 3.0} }; // 2+3=5
            TestProvider provider{ items, gatherers };
            auto events = FindGatherEvents(provider);
            
            std::vector<GatheringEvent> expected = { {0, 0, 25.0, 0.5} };
            CHECK_THAT(events, IsEquivalentTo(expected));
        }

        THEN("Horizontal movement, item is just missed") {
            std::vector<Item> items = { {{5, 5}, 1.9} };
            std::vector<Gatherer> gatherers = { {{0, 0}, {10, 0}, 3.0} }; // 1.9+3=4.9 < 5
            TestProvider provider{ items, gatherers };
            CHECK(FindGatherEvents(provider).empty());
        }
    }
}
