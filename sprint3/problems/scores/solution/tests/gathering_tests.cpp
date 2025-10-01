#define _USE_MATH_DEFINES

#include <cmath>
#include <functional>
#include <sstream>
#include <utility>
#include <algorithm>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_templated.hpp>

#include "../src/model.h"

namespace Catch {
template<>
struct StringMaker<model::GatheringEvent> {
    static std::string convert(model::GatheringEvent const& value) {
        std::ostringstream tmp;
        tmp << "(" << value.gatherer_id << value.item_id << value.sq_distance << value.time << ")";

        return tmp.str();
    }
};
}  // namespace Catch

namespace {

template <typename Range, typename Predicate>
struct EqualsRangeMatcher : Catch::Matchers::MatcherGenericBase {
    EqualsRangeMatcher(Range const& range, Predicate predicate)
        : range_{range}
        , predicate_{predicate} {
    }

    template <typename OtherRange>
    bool match(const OtherRange& other) const {
        using std::begin;
        using std::end;

        return std::equal(begin(range_), end(range_), begin(other), end(other), predicate_);
    }

    std::string describe() const override {
        return "Equals: " + Catch::rangeToString(range_);
    }

private:
    const Range& range_;
    Predicate predicate_;
};

template <typename Range, typename Predicate>
auto EqualsRange(const Range& range, Predicate predicate) {
    return EqualsRangeMatcher<Range, Predicate>{range, predicate};
}

class VectorItemGathererProvider : public model::ItemGathererProvider {
public:
    VectorItemGathererProvider(std::vector<model::Item> items,
                               std::vector<model::Gatherer> gatherers)
        : items_(std::move(items))
        , gatherers_(std::move(gatherers)) {
    }

    size_t ItemsCount() const override {
        return items_.size();
    }
    model::Item GetItem(size_t idx) const override {
        return items_.at(idx);
    }
    size_t GatherersCount() const override {
        return gatherers_.size();
    }
    model::Gatherer GetGatherer(size_t idx) const override {
        return gatherers_.at(idx);
    }

private:
    std::vector<model::Item> items_;
    std::vector<model::Gatherer> gatherers_;
};

class CompareEvents {
public:
    bool operator()(const model::GatheringEvent& l,
                    const model::GatheringEvent& r) {
        if (l.gatherer_id != r.gatherer_id || l.item_id != r.item_id) {
            return false;
        }

        static const double eps = 1e-10;

        if (std::abs(l.sq_distance - r.sq_distance) > eps) {
            return false;
        }

        if (std::abs(l.time - r.time) > eps) {
            return false;
        }
        return true;
    }
};

}  // namespace

SCENARIO("Collision detection using model::FindGatherEvents") {
    WHEN("no items") {
        VectorItemGathererProvider provider{
            {}, {{{1, 2}, {4, 2}, 5.}, {{0, 0}, {10, 10}, 5.}, {{-5, 0}, {10, 5}, 5.}}};
        THEN("No events") {
            auto events = model::FindGatherEvents(provider);
            CHECK(events.empty());
        }
    }
    WHEN("no gatherers") {
        VectorItemGathererProvider provider{
            {{{1, 2}, 5.}, {{0, 0}, 5.}, {{-5, 0}, 5.}}, {}};
        THEN("No events") {
            auto events = model::FindGatherEvents(provider);
            CHECK(events.empty());
        }
    }
    WHEN("multiple items on a way of gatherer") {
        VectorItemGathererProvider provider{{
            {{9, 0.27}, .1},
            {{8, 0.24}, .1},
            {{7, 0.21}, .1},
            {{6, 0.18}, .1},
            {{5, 0.15}, .1},
            {{4, 0.12}, .1},
            {{3, 0.09}, .1},
            {{2, 0.06}, .1},
            {{1, 0.03}, .1},
            {{0, 0.0}, .1},
            {{-1, 0}, .1},
            }, {
            {{0, 0}, {10, 0}, 0.1},
        }};
        THEN("Gathered items in right order") {
            auto events = model::FindGatherEvents(provider);
            CHECK_THAT(
                events,
                EqualsRange(std::vector{
                    model::GatheringEvent{9, 0,0.*0., 0.0},
                    model::GatheringEvent{8, 0,0.03*0.03, 0.1},
                    model::GatheringEvent{7, 0,0.06*0.06, 0.2},
                    model::GatheringEvent{6, 0,0.09*0.09, 0.3},
                    model::GatheringEvent{5, 0,0.12*0.12, 0.4},
                    model::GatheringEvent{4, 0,0.15*0.15, 0.5},
                    model::GatheringEvent{3, 0,0.18*0.18, 0.6},
                }, CompareEvents()));
        }
    }
    WHEN("multiple gatherers and one item") {
        VectorItemGathererProvider provider{{
                                                {{0, 0}, 0.},
                                            },
                                            {
                                                {{-5, 0}, {5, 0}, 1.},
                                                {{0, 1}, {0, -1}, 1.},
                                                {{-10, 10}, {101, -100}, 0.5},
                                                {{-100, 100}, {10, -10}, 0.5},
                                            }
        };
        THEN("Item gathered by faster gatherer") {
            auto events = model::FindGatherEvents(provider);
            REQUIRE_FALSE(events.empty());
            CHECK(events.front().gatherer_id == 2);
        }
    }
    WHEN("Gatherers stay put") {
        VectorItemGathererProvider provider{{
                                                {{0, 0}, 10.},
                                            },
                                            {
                                                {{-5, 0}, {-5, 0}, 1.},
                                                {{0, 0}, {0, 0}, 1.},
                                                {{-10, 10}, {-10, 10}, 100}
                                            }
        };
        THEN("No events detected") {
            auto events = model::FindGatherEvents(provider);
            CHECK(events.empty());
        }
    }
}




