use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Copy, Default, PartialEq, Serialize, Deserialize)]
#[cfg_attr(
    feature = "web",
    derive(tsify::Tsify),
    tsify(into_wasm_abi),
    tsify(from_wasm_abi)
)]
pub struct Vec2 {
    pub x: f32,
    pub y: f32,
}

impl Vec2 {
    pub fn new(x: f32, y: f32) -> Self {
        Self { x, y }
    }

    pub fn length(&self) -> f32 {
        (self.x * self.x + self.y * self.y).sqrt()
    }

    pub fn length_sq(&self) -> f32 {
        self.x * self.x + self.y * self.y
    }

    pub fn normalized(&self) -> Self {
        let length = self.length();
        if length > 0.0 {
            *self / length
        } else {
            Self::default()
        }
    }

    pub fn distance(&self, rhs: Self) -> f32 {
        (*self - rhs).length()
    }

    pub fn dot(&self, rhs: Self) -> f32 {
        self.x * rhs.x + self.y * rhs.y
    }

    pub fn cross(&self, rhs: Self) -> f32 {
        self.x * rhs.y - self.y * rhs.x
    }

    pub fn rotated(&self, angle_rad: f32) -> Self {
        let cos_a = angle_rad.cos();
        let sin_a = angle_rad.sin();
        Self::new(
            self.x * cos_a - self.y * sin_a,
            self.x * sin_a + self.y * cos_a,
        )
    }
}

impl std::ops::Add for Vec2 {
    type Output = Vec2;
    fn add(self, rhs: Self) -> Self::Output {
        Self::new(self.x + rhs.x, self.y + rhs.y)
    }
}

impl std::ops::Sub for Vec2 {
    type Output = Vec2;
    fn sub(self, rhs: Self) -> Self::Output {
        Self::new(self.x - rhs.x, self.y - rhs.y)
    }
}

impl std::ops::Mul<f32> for Vec2 {
    type Output = Vec2;
    fn mul(self, rhs: f32) -> Self::Output {
        Self::new(self.x * rhs, self.y * rhs)
    }
}

impl std::ops::Div<f32> for Vec2 {
    type Output = Vec2;
    fn div(self, rhs: f32) -> Self::Output {
        Self::new(self.x / rhs, self.y / rhs)
    }
}

impl std::ops::AddAssign for Vec2 {
    fn add_assign(&mut self, rhs: Self) {
        self.x += rhs.x;
        self.y += rhs.y;
    }
}

impl std::ops::SubAssign for Vec2 {
    fn sub_assign(&mut self, rhs: Self) {
        self.x -= rhs.x;
        self.y -= rhs.y;
    }
}

impl std::ops::MulAssign<f32> for Vec2 {
    fn mul_assign(&mut self, rhs: f32) {
        self.x *= rhs;
        self.y *= rhs;
    }
}

impl std::ops::DivAssign<f32> for Vec2 {
    fn div_assign(&mut self, rhs: f32) {
        self.x /= rhs;
        self.y /= rhs;
    }
}

#[derive(Debug, Clone, Copy, Default, PartialEq)]
pub struct Rect {
    pub center: Vec2,
    pub width: f32,
    pub height: f32,
}

impl Rect {
    pub fn new(center: Vec2, width: f32, height: f32) -> Self {
        Self {
            center,
            width,
            height,
        }
    }

    pub fn contains(&self, point: Vec2) -> bool {
        let half_width = self.width * 0.5;
        let half_height = self.height * 0.5;
        point.x >= self.center.x - half_width
            && point.x <= self.center.x + half_width
            && point.y >= self.center.y - half_height
            && point.y <= self.center.y + half_height
    }
}

impl std::ops::Add<Vec2> for Rect {
    type Output = Self;
    fn add(self, rhs: Vec2) -> Self::Output {
        Self::new(self.center + rhs, self.width, self.height)
    }
}

impl std::ops::Sub<Vec2> for Rect {
    type Output = Self;
    fn sub(self, rhs: Vec2) -> Self::Output {
        Self::new(self.center - rhs, self.width, self.height)
    }
}

impl std::ops::AddAssign<Vec2> for Rect {
    fn add_assign(&mut self, rhs: Vec2) {
        self.center += rhs;
    }
}

impl std::ops::SubAssign<Vec2> for Rect {
    fn sub_assign(&mut self, rhs: Vec2) {
        self.center -= rhs;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    use crate::test_util::assert_approx_eq;

    #[test]
    fn vec2_addition_and_subtraction() {
        let a = Vec2::new(1.0, 2.0);
        let b = Vec2::new(3.0, 4.0);

        let s = a + b;
        assert_approx_eq!(s.x, 4.0);
        assert_approx_eq!(s.y, 6.0);

        let d = b - a;
        assert_approx_eq!(d.x, 2.0);
        assert_approx_eq!(d.y, 2.0);
    }

    #[test]
    fn vec2_scalar_multiply_and_divide() {
        let a = Vec2::new(1.0, 2.0);
        let b = Vec2::new(3.0, 4.0);

        let m = a * 3.0;
        assert_approx_eq!(m.x, 3.0);
        assert_approx_eq!(m.y, 6.0);

        let q = b / 2.0;
        assert_approx_eq!(q.x, 1.5);
        assert_approx_eq!(q.y, 2.0);
    }

    #[test]
    fn vec2_in_place_compound_assignment() {
        let mut a = Vec2::new(1.0, 2.0);
        let b = Vec2::new(3.0, 4.0);

        a += b;
        assert_approx_eq!(a.x, 4.0);
        assert_approx_eq!(a.y, 6.0);

        a -= Vec2::new(1.0, 1.0);
        assert_approx_eq!(a.x, 3.0);
        assert_approx_eq!(a.y, 5.0);

        a *= 2.0;
        assert_approx_eq!(a.x, 6.0);
        assert_approx_eq!(a.y, 10.0);
    }

    #[test]
    fn vec2_default_construct_is_zero() {
        let z = Vec2::default();
        assert_approx_eq!(z.x, 0.0);
        assert_approx_eq!(z.y, 0.0);
    }

    #[test]
    fn vec2_length_and_normalization() {
        let v = Vec2::new(3.0, 4.0);
        assert_approx_eq!(v.length(), 5.0);
        assert_approx_eq!(v.length_sq(), 25.0);

        let n = v.normalized();
        assert_approx_eq!(n.length(), 1.0);
        assert_approx_eq!(n.x, 0.6);
        assert_approx_eq!(n.y, 0.8);

        // zero vector normalizes to zero
        let z = Vec2::new(0.0, 0.0);
        let zn = z.normalized();
        assert_approx_eq!(zn.x, 0.0);
        assert_approx_eq!(zn.y, 0.0);
    }

    #[test]
    fn vec2_distance_dot_cross() {
        let a = Vec2::new(0.0, 0.0);
        let b = Vec2::new(3.0, 4.0);
        assert_approx_eq!(a.distance(b), 5.0);

        let u = Vec2::new(1.0, 0.0);
        let v = Vec2::new(0.0, 1.0);
        assert_approx_eq!(u.dot(v), 0.0);
        assert_approx_eq!(u.cross(v), 1.0); // counter-clockwise
        assert_approx_eq!(v.cross(u), -1.0); // clockwise
        assert_approx_eq!(u.dot(u), 1.0);
    }

    #[test]
    fn vec2_rotated() {
        let x = Vec2::new(1.0, 0.0);

        let up = x.rotated(std::f32::consts::PI / 2.0);
        assert_approx_eq!(up.x, 0.0, 1e-5);
        assert_approx_eq!(up.y, 1.0);

        let back = x.rotated(std::f32::consts::PI);
        assert_approx_eq!(back.x, -1.0, 1e-5);
        assert_approx_eq!(back.y, 0.0, 1e-5);

        // 360-degree rotation is identity
        let around = x.rotated(2.0 * std::f32::consts::PI);
        assert_approx_eq!(around.x, 1.0, 1e-4);
        assert_approx_eq!(around.y, 0.0, 1e-4);
    }

    #[test]
    fn rect_contains() {
        let r = Rect::new(Vec2::new(5.0, 5.0), 4.0, 2.0); // spans x[3,7], y[4,6]

        assert!(r.contains(Vec2::new(5.0, 5.0))); // center
        assert!(r.contains(Vec2::new(3.0, 4.0))); // bottom-left corner
        assert!(r.contains(Vec2::new(7.0, 6.0))); // top-right corner
        assert!(!r.contains(Vec2::new(2.9, 5.0)));
        assert!(!r.contains(Vec2::new(7.1, 5.0)));
        assert!(!r.contains(Vec2::new(5.0, 3.9)));
    }

    #[test]
    fn rect_offset() {
        let r = Rect::new(Vec2::new(0.0, 0.0), 2.0, 2.0);
        let moved = r + Vec2::new(3.0, 1.0);
        assert_approx_eq!(moved.center.x, 3.0);
        assert_approx_eq!(moved.center.y, 1.0);
        assert_approx_eq!(moved.width, 2.0);
        assert_approx_eq!(moved.height, 2.0);

        let mut r = r;
        r += Vec2::new(1.0, 1.0);
        assert_approx_eq!(r.center.x, 1.0);
        assert_approx_eq!(r.center.y, 1.0);
    }
}
