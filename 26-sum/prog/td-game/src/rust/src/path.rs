use crate::geometry::Vec2;

/// A polyline through the grid that enemies follow from entrance to exit.
#[derive(Debug, Clone, Default)]
pub struct Path {
    pub waypoints: Vec<Vec2>,
    /// Distance along the path at waypoint i (0 at the first waypoint).
    pub cumulative: Vec<f32>,
    pub portal_pairs: Vec<(usize, usize)>,
}

impl Path {
    pub fn new(waypoints: Vec<Vec2>, portal_pairs: Vec<(usize, usize)>) -> Self {
        if waypoints.is_empty() {
            return Self::default();
        }

        let cumulative = waypoints
            .iter()
            .scan((0.0, waypoints[0]), |(acc, last), &cur| {
                *acc += cur.distance(*last);
                *last = cur;
                Some(*acc)
            })
            .collect();

        Self {
            waypoints,
            cumulative,
            portal_pairs,
        }
    }

    pub fn total_length(&self) -> f32 {
        *self.cumulative.last().unwrap_or(&0.0)
    }

    /// Position along the path at the given distance. Clamped to [0, total_length()].
    /// Empty path always return (0.0, 0.0)
    pub fn position_at(&self, distance: f32) -> Vec2 {
        if self.waypoints.is_empty() {
            return Vec2::default();
        } else if distance <= 0.0 {
            return self.waypoints[0];
        } else if distance >= self.total_length() {
            return *self.waypoints.last().unwrap();
        }

        let idx = self.cumulative.partition_point(|&cum| cum <= distance);
        let seg_len = self.cumulative[idx] - self.cumulative[idx - 1];
        let t = if seg_len > 0.0 {
            (distance - self.cumulative[idx - 1]) / seg_len
        } else {
            0.0
        };
        self.waypoints[idx - 1] + (self.waypoints[idx] - self.waypoints[idx - 1]) * t
    }

    /// Path-distance of the partner portal for a portal position, None if no matches.
    pub fn paired_portal_distance(&self, position: Vec2) -> Option<f32> {
        self.portal_pairs.iter().find_map(|(src_idx, tgt_idx)| {
            if self.waypoints[*src_idx] == position {
                Some(self.cumulative[*tgt_idx])
            } else {
                None
            }
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::test_util::assert_approx_eq;

    #[test]
    fn total_length() {
        let p = Path::new(
            vec![
                Vec2::new(0.0, 0.0),
                Vec2::new(3.0, 0.0),
                Vec2::new(3.0, 4.0),
            ],
            vec![],
        );
        assert_approx_eq!(p.total_length(), 7.0); // 3 + 4
    }

    #[test]
    fn position_at_interpolates_along_segments() {
        let p = Path::new(
            vec![
                Vec2::new(0.0, 0.0),
                Vec2::new(3.0, 0.0),
                Vec2::new(3.0, 4.0),
            ],
            vec![],
        );

        let z = p.position_at(0.0);
        assert_approx_eq!(z.x, 0.0);
        assert_approx_eq!(z.y, 0.0);

        // Midway through the first (horizontal) segment.
        let mid = p.position_at(1.5);
        assert_approx_eq!(mid.x, 1.5);
        assert_approx_eq!(mid.y, 0.0);

        // At the first corner.
        let corner = p.position_at(3.0);
        assert_approx_eq!(corner.x, 3.0);
        assert_approx_eq!(corner.y, 0.0);

        // Along the second (vertical) segment.
        let up = p.position_at(5.0); // 2 into the vertical segment
        assert_approx_eq!(up.x, 3.0);
        assert_approx_eq!(up.y, 2.0);

        // At the end.
        let end = p.position_at(7.0);
        assert_approx_eq!(end.x, 3.0);
        assert_approx_eq!(end.y, 4.0);
    }

    #[test]
    fn position_at_clamps() {
        let p = Path::new(vec![Vec2::new(0.0, 0.0), Vec2::new(2.0, 0.0)], vec![]);

        // Below zero -> start.
        assert_approx_eq!(p.position_at(-5.0).x, 0.0);
        // Above total -> end.
        assert_approx_eq!(p.position_at(100.0).x, 2.0);
    }

    #[test]
    fn with_a_single_waypoint() {
        let p = Path::new(vec![Vec2::new(1.0, 2.0)], vec![]);
        assert_approx_eq!(p.total_length(), 0.0);
        assert_approx_eq!(p.position_at(0.0).x, 1.0);
        assert_approx_eq!(p.position_at(0.0).y, 2.0);
    }

    #[test]
    fn empty_default() {
        let p = Path::default();
        assert_approx_eq!(p.total_length(), 0.0);
    }

    #[test]
    fn supports_diagonal_segments() {
        // A purely diagonal route (45°): enemies can move along non-axis-aligned paths.
        let p = Path::new(vec![Vec2::new(0.0, 0.0), Vec2::new(3.0, 3.0)], vec![]);
        assert_approx_eq!(p.total_length(), (18.0f32).sqrt()); // ~4.243

        let mid = p.position_at(p.total_length() * 0.5); // halfway along the diagonal
        assert_approx_eq!(mid.x, 1.5);
        assert_approx_eq!(mid.y, 1.5);

        // One unit of distance along the diagonal -> (1/√2, 1/√2).
        let one = p.position_at(1.0);
        let s = (0.5f32).sqrt();
        assert_approx_eq!(one.x, s);
        assert_approx_eq!(one.y, s);
    }

    #[test]
    fn portal_pairs_are_directional() {
        // Waypoints at (0,0),(1,0),(2,0) -> cumulative [0, 1, 2].
        // Portal (src=1, tgt=2): an enemy at waypoint 1 teleports to
        // waypoint 2's path distance; the target does not teleport.
        let p = Path::new(
            vec![
                Vec2::new(0.0, 0.0),
                Vec2::new(1.0, 0.0),
                Vec2::new(2.0, 0.0),
            ],
            vec![(1, 2)],
        );
        assert_eq!(p.portal_pairs, vec![(1, 2)]);
        assert_eq!(p.paired_portal_distance(Vec2::new(1.0, 0.0)), Some(2.0));
        assert_eq!(p.paired_portal_distance(Vec2::new(2.0, 0.0)), None); // target
        assert_eq!(p.paired_portal_distance(Vec2::new(0.0, 0.0)), None); // non-portal
    }
}
