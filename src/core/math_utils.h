#ifndef CORE_MATH_UTILS_H_
#define CORE_MATH_UTILS_H_

#include <cmath>

struct Vec3 {
    float x;
    float y;
    float z;
};

struct Mat4 {
    float m[16];  // Column-major layout
};

inline Vec3 Vec3Subtract(const Vec3& a, const Vec3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

inline Vec3 Vec3Add(const Vec3& a, const Vec3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

inline Vec3 Vec3Scale(const Vec3& v, float s) {
    return {v.x * s, v.y * s, v.z * s};
}

inline float Vec3Length(const Vec3& v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

inline Vec3 Vec3Normalize(const Vec3& v) {
    float len = Vec3Length(v);
    if (len > 0.0f) {
        return {v.x / len, v.y / len, v.z / len};
    }
    return {0.0f, 0.0f, 0.0f};
}

inline float Vec3Dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vec3 Vec3Cross(const Vec3& a, const Vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

inline Mat4 Mat4Identity() {
    Mat4 mat = {};
    mat.m[0] = 1.0f;
    mat.m[5] = 1.0f;
    mat.m[10] = 1.0f;
    mat.m[15] = 1.0f;
    return mat;
}

inline Mat4 Mat4Multiply(const Mat4& a, const Mat4& b) {
    Mat4 out = {};
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            float sum = 0.0f;
            for (int i = 0; i < 4; ++i) {
                sum += a.m[r + i * 4] * b.m[i + c * 4];
            }
            out.m[r + c * 4] = sum;
        }
    }
    return out;
}

inline Mat4 Mat4Perspective(float fovRad, float aspect, float nearVal, float farVal) {
    float f = 1.0f / std::tan(fovRad / 2.0f);
    Mat4 out = {};
    out.m[0] = f / aspect;
    out.m[5] = f;
    out.m[10] = farVal / (nearVal - farVal);
    out.m[11] = -1.0f;
    out.m[14] = (nearVal * farVal) / (nearVal - farVal);
    return out;
}

inline Mat4 Mat4LookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
    Vec3 f = Vec3Normalize(Vec3Subtract(center, eye));
    Vec3 s = Vec3Normalize(Vec3Cross(f, up));
    Vec3 u = Vec3Cross(s, f);

    Mat4 out = Mat4Identity();
    out.m[0] = s.x;
    out.m[4] = s.y;
    out.m[8] = s.z;

    out.m[1] = u.x;
    out.m[5] = u.y;
    out.m[9] = u.z;

    out.m[2] = -f.x;
    out.m[6] = -f.y;
    out.m[10] = -f.z;

    out.m[12] = -Vec3Dot(s, eye);
    out.m[13] = -Vec3Dot(u, eye);
    out.m[14] = Vec3Dot(f, eye);
    return out;
}

inline Mat4 Mat4RotationY(float angleRad) {
    Mat4 out = Mat4Identity();
    float c = std::cos(angleRad);
    float s = std::sin(angleRad);
    out.m[0] = c;
    out.m[2] = -s;
    out.m[8] = s;
    out.m[10] = c;
    return out;
}

inline Mat4 Mat4RotationX(float angleRad) {
    Mat4 out = Mat4Identity();
    float c = std::cos(angleRad);
    float s = std::sin(angleRad);
    out.m[5] = c;
    out.m[6] = s;
    out.m[9] = -s;
    out.m[10] = c;
    return out;
}

inline Mat4 Mat4Transpose(const Mat4& mat) {
    Mat4 out = {};
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            out.m[c + r * 4] = mat.m[r + c * 4];
        }
    }
    return out;
}

inline Mat4 Mat4Inverse(const Mat4& mat) {
    float s0 = mat.m[0] * mat.m[5] - mat.m[1] * mat.m[4];
    float s1 = mat.m[0] * mat.m[6] - mat.m[2] * mat.m[4];
    float s2 = mat.m[0] * mat.m[7] - mat.m[3] * mat.m[4];
    float s3 = mat.m[1] * mat.m[6] - mat.m[2] * mat.m[5];
    float s4 = mat.m[1] * mat.m[7] - mat.m[3] * mat.m[5];
    float s5 = mat.m[2] * mat.m[7] - mat.m[3] * mat.m[6];

    float c0 = mat.m[8] * mat.m[13] - mat.m[9] * mat.m[12];
    float c1 = mat.m[8] * mat.m[14] - mat.m[10] * mat.m[12];
    float c2 = mat.m[8] * mat.m[15] - mat.m[11] * mat.m[12];
    float c3 = mat.m[9] * mat.m[14] - mat.m[10] * mat.m[13];
    float c4 = mat.m[9] * mat.m[15] - mat.m[11] * mat.m[13];
    float c5 = mat.m[10] * mat.m[15] - mat.m[11] * mat.m[14];

    float det = s0 * c5 - s1 * c4 + s2 * c3 + s3 * c2 - s4 * c1 + s5 * c0;
    if (std::abs(det) < 1e-6f) {
        return Mat4Identity();
    }

    float invDet = 1.0f / det;
    Mat4 out = {};
    out.m[0] = (mat.m[5] * c5 - mat.m[6] * c4 + mat.m[7] * c3) * invDet;
    out.m[1] = (-mat.m[1] * c5 + mat.m[2] * c4 - mat.m[3] * c3) * invDet;
    out.m[2] = (mat.m[13] * s5 - mat.m[14] * s4 + mat.m[15] * s3) * invDet;
    out.m[3] = (-mat.m[9] * s5 + mat.m[10] * s4 - mat.m[11] * s3) * invDet;

    out.m[4] = (-mat.m[4] * c5 + mat.m[6] * c2 - mat.m[7] * c1) * invDet;
    out.m[5] = (mat.m[0] * c5 - mat.m[2] * c2 + mat.m[3] * c1) * invDet;
    out.m[6] = (-mat.m[12] * s5 + mat.m[14] * s2 - mat.m[15] * s1) * invDet;
    out.m[7] = (mat.m[8] * s5 - mat.m[10] * s2 + mat.m[11] * s1) * invDet;

    out.m[8] = (mat.m[4] * c4 - mat.m[5] * c2 + mat.m[7] * c0) * invDet;
    out.m[9] = (-mat.m[0] * c4 + mat.m[1] * c2 - mat.m[3] * c0) * invDet;
    out.m[10] = (mat.m[12] * s4 - mat.m[13] * s2 + mat.m[15] * s0) * invDet;
    out.m[11] = (-mat.m[8] * s4 + mat.m[9] * s2 - mat.m[11] * s0) * invDet;

    out.m[12] = (-mat.m[4] * c3 + mat.m[5] * c1 - mat.m[6] * c0) * invDet;
    out.m[13] = (mat.m[0] * c3 - mat.m[1] * c1 + mat.m[2] * c0) * invDet;
    out.m[14] = (-mat.m[12] * s3 + mat.m[13] * s1 - mat.m[14] * s0) * invDet;
    out.m[15] = (mat.m[8] * s3 - mat.m[9] * s1 + mat.m[10] * s0) * invDet;

    return out;
}

#endif  // CORE_MATH_UTILS_H_
