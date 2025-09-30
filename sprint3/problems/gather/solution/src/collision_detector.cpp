#include "collision_detector.h"
#include <cassert>

namespace collision_detector {

CollectionResult TryCollectPoint(geom::Point2D a, geom::Point2D b, geom::Point2D c) {
    // Проверим, что перемещение ненулевое.
    // Тут приходится использовать строгое равенство, а не приближённое,
    // пскольку при сборе заказов придётся учитывать перемещение даже на небольшое
    // расстояние.
    assert(b.x != a.x || b.y != a.y);
    const double u_x = c.x - a.x;
    const double u_y = c.y - a.y;
    const double v_x = b.x - a.x;
    const double v_y = b.y - a.y;
    const double u_dot_v = u_x * v_x + u_y * v_y;
    const double u_len2 = u_x * u_x + u_y * u_y;
    const double v_len2 = v_x * v_x + v_y * v_y;
    const double proj_ratio = u_dot_v / v_len2;
    const double sq_distance = u_len2 - (u_dot_v * u_dot_v) / v_len2;

    return CollectionResult(sq_distance, proj_ratio);
}

// В задании на разработку тестов реализовывать следующую функцию не нужно -
// она будет линковаться извне.
std::vector<GatheringEvent> FindGatherEvents(const ItemGathererProvider& provider) {
    std::vector<GatheringEvent> events;
    const size_t gatherers_count = provider.GatherersCount();
    const size_t items_count = provider.ItemsCount();
    events.reserve(gatherers_count * items_count);

    for (size_t gatherer_id = 0; gatherer_id < gatherers_count; ++gatherer_id) {
        const Gatherer gatherer = provider.GetGatherer(gatherer_id);
        if (gatherer.start_pos.x == gatherer.end_pos.x && gatherer.start_pos.y == gatherer.end_pos.y) {
            continue;
        }

        for (size_t item_id = 0; item_id < items_count; ++item_id) {
            const Item item = provider.GetItem(item_id);
            const double collect_radius = gatherer.width + item.width;
            const CollectionResult result = TryCollectPoint(gatherer.start_pos, gatherer.end_pos, item.position);
            if (result.IsCollected(collect_radius)) {
                events.push_back(GatheringEvent{item_id, gatherer_id, result.sq_distance, result.proj_ratio});
            }
        }
    }

    std::sort(events.begin(), events.end(), [](const GatheringEvent& lhs, const GatheringEvent& rhs) {
        if (lhs.time < rhs.time) {
            return true;
        }
        if (lhs.time > rhs.time) {
            return false;
        }
        if (lhs.item_id < rhs.item_id) {
            return true;
        }
        if (lhs.item_id > rhs.item_id) {
            return false;
        }
        return lhs.gatherer_id < rhs.gatherer_id;
    });

    return events;
}


}  // namespace collision_detector