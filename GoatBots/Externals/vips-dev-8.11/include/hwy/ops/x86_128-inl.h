// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// 128-bit vectors and SSE4 instructions, plus some AVX2 and AVX512-VL
// operations when compiling for those targets.
// External include guard in highway.h - see comment there.

#include <emmintrin.h>
#if HWY_TARGET == HWY_SSSE3
#include <tmmintrin.h>  // SSSE3
#else
#include <smmintrin.h>  // SSE4
#include <wmmintrin.h>  // CLMUL
#endif
#include <stddef.h>
#include <stdint.h>

#include "hwy/base.h"
#include "hwy/ops/shared-inl.h"

// Clang 3.9 generates VINSERTF128 instead of the desired VBROADCASTF128,
// which would free up port5. However, inline assembly isn't supported on
// MSVC, results in incorrect output on GCC 8.3, and raises "invalid output size
// for constraint" errors on Clang (https://gcc.godbolt.org/z/-Jt_-F), hence we
// disable it.
#ifndef HWY_LOADDUP_ASM
#define HWY_LOADDUP_ASM 0
#endif

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

template <typename T>
using Full128 = Simd<T, 16 / sizeof(T)>;

namespace detail {

template <typename T>
struct Raw128 {
  using type = __m128i;
};
template <>
struct Raw128<float> {
  using type = __m128;
};
template <>
struct Raw128<double> {
  using type = __m128d;
};

}  // namespace detail

template <typename T, size_t N = 16 / sizeof(T)>
class Vec128 {
  using Raw = typename detail::Raw128<T>::type;

 public:
  // Compound assignment. Only usable if there is a corresponding non-member
  // binary operator overload. For example, only f32 and f64 support division.
  HWY_INLINE Vec128& operator*=(const Vec128 other) {
    return *this = (*this * other);
  }
  HWY_INLINE Vec128& operator/=(const Vec128 other) {
    return *this = (*this / other);
  }
  HWY_INLINE Vec128& operator+=(const Vec128 other) {
    return *this = (*this + other);
  }
  HWY_INLINE Vec128& operator-=(const Vec128 other) {
    return *this = (*this - other);
  }
  HWY_INLINE Vec128& operator&=(const Vec128 other) {
    return *this = (*this & other);
  }
  HWY_INLINE Vec128& operator|=(const Vec128 other) {
    return *this = (*this | other);
  }
  HWY_INLINE Vec128& operator^=(const Vec128 other) {
    return *this = (*this ^ other);
  }

  Raw raw;
};

// Forward-declare for use by DeduceD, see below.
template <typename T>
class Vec256;
template <typename T>
class Vec512;

// FF..FF or 0.
template <typename T, size_t N = 16 / sizeof(T)>
struct Mask128 {
  typename detail::Raw128<T>::type raw;
};

namespace detail {

// Deduce Simd<T, N> from Vec*<T, N> (pointers because Vec256/512 may be
// incomplete types at this point; this is simpler than avoiding multiple
// definitions of DFromV via #if)
struct DeduceD {
  template <typename T, size_t N>
  Simd<T, N> operator()(const Vec128<T, N>*) const {
    return Simd<T, N>();
  }
  template <typename T>
  Simd<T, 32 / sizeof(T)> operator()(const Vec256<T>*) const {
    return Simd<T, 32 / sizeof(T)>();
  }
  template <typename T>
  Simd<T, 64 / sizeof(T)> operator()(const Vec512<T>*) const {
    return Simd<T, 64 / sizeof(T)>();
  }
};

}  // namespace detail

template <class V>
using DFromV = decltype(detail::DeduceD()(static_cast<V*>(nullptr)));

// ------------------------------ BitCast

namespace detail {

HWY_INLINE __m128i BitCastToInteger(__m128i v) { return v; }
HWY_INLINE __m128i BitCastToInteger(__m128 v) { return _mm_castps_si128(v); }
HWY_INLINE __m128i BitCastToInteger(__m128d v) { return _mm_castpd_si128(v); }

template <typename T, size_t N>
HWY_INLINE Vec128<uint8_t, N * sizeof(T)> BitCastToByte(Vec128<T, N> v) {
  return Vec128<uint8_t, N * sizeof(T)>{BitCastToInteger(v.raw)};
}

// Cannot rely on function overloading because return types differ.
template <typename T>
struct BitCastFromInteger128 {
  HWY_INLINE __m128i operator()(__m128i v) { return v; }
};
template <>
struct BitCastFromInteger128<float> {
  HWY_INLINE __m128 operator()(__m128i v) { return _mm_castsi128_ps(v); }
};
template <>
struct BitCastFromInteger128<double> {
  HWY_INLINE __m128d operator()(__m128i v) { return _mm_castsi128_pd(v); }
};

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> BitCastFromByte(Simd<T, N> /* tag */,
                                        Vec128<uint8_t, N * sizeof(T)> v) {
  return Vec128<T, N>{BitCastFromInteger128<T>()(v.raw)};
}

}  // namespace detail

template <typename T, size_t N, typename FromT>
HWY_API Vec128<T, N> BitCast(Simd<T, N> d,
                             Vec128<FromT, N * sizeof(T) / sizeof(FromT)> v) {
  return detail::BitCastFromByte(d, detail::BitCastToByte(v));
}

// ------------------------------ Set

// Returns an all-zero vector/part.
template <typename T, size_t N, HWY_IF_LE128(T, N)>
HWY_API Vec128<T, N> Zero(Simd<T, N> /* tag */) {
  return Vec128<T, N>{_mm_setzero_si128()};
}
template <size_t N, HWY_IF_LE128(float, N)>
HWY_API Vec128<float, N> Zero(Simd<float, N> /* tag */) {
  return Vec128<float, N>{_mm_setzero_ps()};
}
template <size_t N, HWY_IF_LE128(double, N)>
HWY_API Vec128<double, N> Zero(Simd<double, N> /* tag */) {
  return Vec128<double, N>{_mm_setzero_pd()};
}

// Returns a vector/part with all lanes set to "t".
template <size_t N, HWY_IF_LE128(uint8_t, N)>
HWY_API Vec128<uint8_t, N> Set(Simd<uint8_t, N> /* tag */, const uint8_t t) {
  return Vec128<uint8_t, N>{_mm_set1_epi8(static_cast<char>(t))};  // NOLINT
}
template <size_t N, HWY_IF_LE128(uint16_t, N)>
HWY_API Vec128<uint16_t, N> Set(Simd<uint16_t, N> /* tag */, const uint16_t t) {
  return Vec128<uint16_t, N>{_mm_set1_epi16(static_cast<short>(t))};  // NOLINT
}
template <size_t N, HWY_IF_LE128(uint32_t, N)>
HWY_API Vec128<uint32_t, N> Set(Simd<uint32_t, N> /* tag */, const uint32_t t) {
  return Vec128<uint32_t, N>{_mm_set1_epi32(static_cast<int>(t))};
}
template <size_t N, HWY_IF_LE128(uint64_t, N)>
HWY_API Vec128<uint64_t, N> Set(Simd<uint64_t, N> /* tag */, const uint64_t t) {
  return Vec128<uint64_t, N>{
      _mm_set1_epi64x(static_cast<long long>(t))};  // NOLINT
}
template <size_t N, HWY_IF_LE128(int8_t, N)>
HWY_API Vec128<int8_t, N> Set(Simd<int8_t, N> /* tag */, const int8_t t) {
  return Vec128<int8_t, N>{_mm_set1_epi8(static_cast<char>(t))};  // NOLINT
}
template <size_t N, HWY_IF_LE128(int16_t, N)>
HWY_API Vec128<int16_t, N> Set(Simd<int16_t, N> /* tag */, const int16_t t) {
  return Vec128<int16_t, N>{_mm_set1_epi16(static_cast<short>(t))};  // NOLINT
}
template <size_t N, HWY_IF_LE128(int32_t, N)>
HWY_API Vec128<int32_t, N> Set(Simd<int32_t, N> /* tag */, const int32_t t) {
  return Vec128<int32_t, N>{_mm_set1_epi32(t)};
}
template <size_t N, HWY_IF_LE128(int64_t, N)>
HWY_API Vec128<int64_t, N> Set(Simd<int64_t, N> /* tag */, const int64_t t) {
  return Vec128<int64_t, N>{
      _mm_set1_epi64x(static_cast<long long>(t))};  // NOLINT
}
template <size_t N, HWY_IF_LE128(float, N)>
HWY_API Vec128<float, N> Set(Simd<float, N> /* tag */, const float t) {
  return Vec128<float, N>{_mm_set1_ps(t)};
}
template <size_t N, HWY_IF_LE128(double, N)>
HWY_API Vec128<double, N> Set(Simd<double, N> /* tag */, const double t) {
  return Vec128<double, N>{_mm_set1_pd(t)};
}

HWY_DIAGNOSTICS(push)
HWY_DIAGNOSTICS_OFF(disable : 4700, ignored "-Wuninitialized")

// Returns a vector with uninitialized elements.
template <typename T, size_t N, HWY_IF_LE128(T, N)>
HWY_API Vec128<T, N> Undefined(Simd<T, N> /* tag */) {
  // Available on Clang 6.0, GCC 6.2, ICC 16.03, MSVC 19.14. All but ICC
  // generate an XOR instruction.
  return Vec128<T, N>{_mm_undefined_si128()};
}
template <size_t N, HWY_IF_LE128(float, N)>
HWY_API Vec128<float, N> Undefined(Simd<float, N> /* tag */) {
  return Vec128<float, N>{_mm_undefined_ps()};
}
template <size_t N, HWY_IF_LE128(double, N)>
HWY_API Vec128<double, N> Undefined(Simd<double, N> /* tag */) {
  return Vec128<double, N>{_mm_undefined_pd()};
}

HWY_DIAGNOSTICS(pop)

// ------------------------------ GetLane

// Gets the single value stored in a vector/part.
template <size_t N>
HWY_API uint8_t GetLane(const Vec128<uint8_t, N> v) {
  return _mm_cvtsi128_si32(v.raw) & 0xFF;
}
template <size_t N>
HWY_API int8_t GetLane(const Vec128<int8_t, N> v) {
  return _mm_cvtsi128_si32(v.raw) & 0xFF;
}
template <size_t N>
HWY_API uint16_t GetLane(const Vec128<uint16_t, N> v) {
  return _mm_cvtsi128_si32(v.raw) & 0xFFFF;
}
template <size_t N>
HWY_API int16_t GetLane(const Vec128<int16_t, N> v) {
  return _mm_cvtsi128_si32(v.raw) & 0xFFFF;
}
template <size_t N>
HWY_API uint32_t GetLane(const Vec128<uint32_t, N> v) {
  return _mm_cvtsi128_si32(v.raw);
}
template <size_t N>
HWY_API int32_t GetLane(const Vec128<int32_t, N> v) {
  return _mm_cvtsi128_si32(v.raw);
}
template <size_t N>
HWY_API float GetLane(const Vec128<float, N> v) {
  return _mm_cvtss_f32(v.raw);
}
template <size_t N>
HWY_API uint64_t GetLane(const Vec128<uint64_t, N> v) {
#if HWY_ARCH_X86_32
  alignas(16) uint64_t lanes[2];
  Store(v, Simd<uint64_t, N>(), lanes);
  return lanes[0];
#else
  return _mm_cvtsi128_si64(v.raw);
#endif
}
template <size_t N>
HWY_API int64_t GetLane(const Vec128<int64_t, N> v) {
#if HWY_ARCH_X86_32
  alignas(16) int64_t lanes[2];
  Store(v, Simd<int64_t, N>(), lanes);
  return lanes[0];
#else
  return _mm_cvtsi128_si64(v.raw);
#endif
}
template <size_t N>
HWY_API double GetLane(const Vec128<double, N> v) {
  return _mm_cvtsd_f64(v.raw);
}

// ================================================== LOGICAL

// ------------------------------ And

template <typename T, size_t N>
HWY_API Vec128<T, N> And(Vec128<T, N> a, Vec128<T, N> b) {
  return Vec128<T, N>{_mm_and_si128(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<float, N> And(const Vec128<float, N> a,
                             const Vec128<float, N> b) {
  return Vec128<float, N>{_mm_and_ps(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> And(const Vec128<double, N> a,
                              const Vec128<double, N> b) {
  return Vec128<double, N>{_mm_and_pd(a.raw, b.raw)};
}

// ------------------------------ AndNot

// Returns ~not_mask & mask.
template <typename T, size_t N>
HWY_API Vec128<T, N> AndNot(Vec128<T, N> not_mask, Vec128<T, N> mask) {
  return Vec128<T, N>{_mm_andnot_si128(not_mask.raw, mask.raw)};
}
template <size_t N>
HWY_API Vec128<float, N> AndNot(const Vec128<float, N> not_mask,
                                const Vec128<float, N> mask) {
  return Vec128<float, N>{_mm_andnot_ps(not_mask.raw, mask.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> AndNot(const Vec128<double, N> not_mask,
                                 const Vec128<double, N> mask) {
  return Vec128<double, N>{_mm_andnot_pd(not_mask.raw, mask.raw)};
}

// ------------------------------ Or

template <typename T, size_t N>
HWY_API Vec128<T, N> Or(Vec128<T, N> a, Vec128<T, N> b) {
  return Vec128<T, N>{_mm_or_si128(a.raw, b.raw)};
}

template <size_t N>
HWY_API Vec128<float, N> Or(const Vec128<float, N> a,
                            const Vec128<float, N> b) {
  return Vec128<float, N>{_mm_or_ps(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> Or(const Vec128<double, N> a,
                             const Vec128<double, N> b) {
  return Vec128<double, N>{_mm_or_pd(a.raw, b.raw)};
}

// ------------------------------ Xor

template <typename T, size_t N>
HWY_API Vec128<T, N> Xor(Vec128<T, N> a, Vec128<T, N> b) {
  return Vec128<T, N>{_mm_xor_si128(a.raw, b.raw)};
}

template <size_t N>
HWY_API Vec128<float, N> Xor(const Vec128<float, N> a,
                             const Vec128<float, N> b) {
  return Vec128<float, N>{_mm_xor_ps(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> Xor(const Vec128<double, N> a,
                              const Vec128<double, N> b) {
  return Vec128<double, N>{_mm_xor_pd(a.raw, b.raw)};
}

// ------------------------------ Not

template <typename T, size_t N>
HWY_API Vec128<T, N> Not(const Vec128<T, N> v) {
  using TU = MakeUnsigned<T>;
#if HWY_TARGET <= HWY_AVX3
  const __m128i vu = BitCast(Simd<TU, N>(), v).raw;
  return BitCast(Simd<T, N>(),
                 Vec128<TU, N>{_mm_ternarylogic_epi32(vu, vu, vu, 0x55)});
#else
  return Xor(v, BitCast(Simd<T, N>(), Vec128<TU, N>{_mm_set1_epi32(-1)}));
#endif
}

// ------------------------------ Operator overloads (internal-only if float)

template <typename T, size_t N>
HWY_API Vec128<T, N> operator&(const Vec128<T, N> a, const Vec128<T, N> b) {
  return And(a, b);
}

template <typename T, size_t N>
HWY_API Vec128<T, N> operator|(const Vec128<T, N> a, const Vec128<T, N> b) {
  return Or(a, b);
}

template <typename T, size_t N>
HWY_API Vec128<T, N> operator^(const Vec128<T, N> a, const Vec128<T, N> b) {
  return Xor(a, b);
}

// ------------------------------ CopySign

template <typename T, size_t N>
HWY_API Vec128<T, N> CopySign(const Vec128<T, N> magn,
                              const Vec128<T, N> sign) {
  static_assert(IsFloat<T>(), "Only makes sense for floating-point");

  const Simd<T, N> d;
  const auto msb = SignBit(d);

#if HWY_TARGET <= HWY_AVX3
  const Rebind<MakeUnsigned<T>, decltype(d)> du;
  // Truth table for msb, magn, sign | bitwise msb ? sign : mag
  //                  0    0     0   |  0
  //                  0    0     1   |  0
  //                  0    1     0   |  1
  //                  0    1     1   |  1
  //                  1    0     0   |  0
  //                  1    0     1   |  1
  //                  1    1     0   |  0
  //                  1    1     1   |  1
  // The lane size does not matter because we are not using predication.
  const __m128i out = _mm_ternarylogic_epi32(
      BitCast(du, msb).raw, BitCast(du, magn).raw, BitCast(du, sign).raw, 0xAC);
  return BitCast(d, decltype(Zero(du)){out});
#else
  return Or(AndNot(msb, magn), And(msb, sign));
#endif
}

template <typename T, size_t N>
HWY_API Vec128<T, N> CopySignToAbs(const Vec128<T, N> abs,
                                   const Vec128<T, N> sign) {
#if HWY_TARGET <= HWY_AVX3
  // AVX3 can also handle abs < 0, so no extra action needed.
  return CopySign(abs, sign);
#else
  return Or(abs, And(SignBit(Simd<T, N>()), sign));
#endif
}

// ------------------------------ Mask

// Mask and Vec are the same (true = FF..FF).
template <typename T, size_t N>
HWY_API Mask128<T, N> MaskFromVec(const Vec128<T, N> v) {
  return Mask128<T, N>{v.raw};
}

template <typename T, size_t N>
HWY_API Vec128<T, N> VecFromMask(const Mask128<T, N> v) {
  return Vec128<T, N>{v.raw};
}

template <typename T, size_t N>
HWY_API Vec128<T, N> VecFromMask(const Simd<T, N> /* tag */,
                                 const Mask128<T, N> v) {
  return Vec128<T, N>{v.raw};
}

#if HWY_TARGET == HWY_SSSE3

// mask ? yes : no
template <typename T, size_t N>
HWY_API Vec128<T, N> IfThenElse(Mask128<T, N> mask, Vec128<T, N> yes,
                                Vec128<T, N> no) {
  const auto vmask = VecFromMask(Simd<T, N>(), mask);
  return Or(And(vmask, yes), AndNot(vmask, no));
}

#else  // HWY_TARGET == HWY_SSSE3

// mask ? yes : no
template <typename T, size_t N>
HWY_API Vec128<T, N> IfThenElse(Mask128<T, N> mask, Vec128<T, N> yes,
                                Vec128<T, N> no) {
  return Vec128<T, N>{_mm_blendv_epi8(no.raw, yes.raw, mask.raw)};
}
template <size_t N>
HWY_API Vec128<float, N> IfThenElse(const Mask128<float, N> mask,
                                    const Vec128<float, N> yes,
                                    const Vec128<float, N> no) {
  return Vec128<float, N>{_mm_blendv_ps(no.raw, yes.raw, mask.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> IfThenElse(const Mask128<double, N> mask,
                                     const Vec128<double, N> yes,
                                     const Vec128<double, N> no) {
  return Vec128<double, N>{_mm_blendv_pd(no.raw, yes.raw, mask.raw)};
}

#endif  // HWY_TARGET == HWY_SSSE3

// mask ? yes : 0
template <typename T, size_t N>
HWY_API Vec128<T, N> IfThenElseZero(Mask128<T, N> mask, Vec128<T, N> yes) {
  return yes & VecFromMask(Simd<T, N>(), mask);
}

// mask ? 0 : no
template <typename T, size_t N>
HWY_API Vec128<T, N> IfThenZeroElse(Mask128<T, N> mask, Vec128<T, N> no) {
  return AndNot(VecFromMask(Simd<T, N>(), mask), no);
}

// ------------------------------ Mask logical

template <typename T, size_t N>
HWY_API Mask128<T, N> Not(const Simd<T, N> d, const Mask128<T, N> m) {
  return MaskFromVec(Not(VecFromMask(d, m)));
}

template <typename T, size_t N>
HWY_API Mask128<T, N> And(const Mask128<T, N> a, Mask128<T, N> b) {
  const Simd<T, N> d;
  return MaskFromVec(And(VecFromMask(d, a), VecFromMask(d, b)));
}

template <typename T, size_t N>
HWY_API Mask128<T, N> AndNot(const Mask128<T, N> a, Mask128<T, N> b) {
  const Simd<T, N> d;
  return MaskFromVec(AndNot(VecFromMask(d, a), VecFromMask(d, b)));
}

template <typename T, size_t N>
HWY_API Mask128<T, N> Or(const Mask128<T, N> a, Mask128<T, N> b) {
  const Simd<T, N> d;
  return MaskFromVec(Or(VecFromMask(d, a), VecFromMask(d, b)));
}

template <typename T, size_t N>
HWY_API Mask128<T, N> Xor(const Mask128<T, N> a, Mask128<T, N> b) {
  const Simd<T, N> d;
  return MaskFromVec(Xor(VecFromMask(d, a), VecFromMask(d, b)));
}

// ================================================== SWIZZLE (1)

// ------------------------------ Hard-coded shuffles

// Notation: let Vec128<int32_t> have lanes 3,2,1,0 (0 is least-significant).
// Shuffle0321 rotates one lane to the right (the previous least-significant
// lane is now most-significant). These could also be implemented via
// CombineShiftRightBytes but the shuffle_abcd notation is more convenient.

// Swap 32-bit halves in 64-bit halves.
template <size_t N>
HWY_API Vec128<uint32_t, N> Shuffle2301(const Vec128<uint32_t, N> v) {
  static_assert(N == 2 || N == 4, "Does not make sense for N=1");
  return Vec128<uint32_t, N>{_mm_shuffle_epi32(v.raw, 0xB1)};
}
template <size_t N>
HWY_API Vec128<int32_t, N> Shuffle2301(const Vec128<int32_t, N> v) {
  static_assert(N == 2 || N == 4, "Does not make sense for N=1");
  return Vec128<int32_t, N>{_mm_shuffle_epi32(v.raw, 0xB1)};
}
template <size_t N>
HWY_API Vec128<float, N> Shuffle2301(const Vec128<float, N> v) {
  static_assert(N == 2 || N == 4, "Does not make sense for N=1");
  return Vec128<float, N>{_mm_shuffle_ps(v.raw, v.raw, 0xB1)};
}

// Swap 64-bit halves
HWY_API Vec128<uint32_t> Shuffle1032(const Vec128<uint32_t> v) {
  return Vec128<uint32_t>{_mm_shuffle_epi32(v.raw, 0x4E)};
}
HWY_API Vec128<int32_t> Shuffle1032(const Vec128<int32_t> v) {
  return Vec128<int32_t>{_mm_shuffle_epi32(v.raw, 0x4E)};
}
HWY_API Vec128<float> Shuffle1032(const Vec128<float> v) {
  return Vec128<float>{_mm_shuffle_ps(v.raw, v.raw, 0x4E)};
}
HWY_API Vec128<uint64_t> Shuffle01(const Vec128<uint64_t> v) {
  return Vec128<uint64_t>{_mm_shuffle_epi32(v.raw, 0x4E)};
}
HWY_API Vec128<int64_t> Shuffle01(const Vec128<int64_t> v) {
  return Vec128<int64_t>{_mm_shuffle_epi32(v.raw, 0x4E)};
}
HWY_API Vec128<double> Shuffle01(const Vec128<double> v) {
  return Vec128<double>{_mm_shuffle_pd(v.raw, v.raw, 1)};
}

// Rotate right 32 bits
HWY_API Vec128<uint32_t> Shuffle0321(const Vec128<uint32_t> v) {
  return Vec128<uint32_t>{_mm_shuffle_epi32(v.raw, 0x39)};
}
HWY_API Vec128<int32_t> Shuffle0321(const Vec128<int32_t> v) {
  return Vec128<int32_t>{_mm_shuffle_epi32(v.raw, 0x39)};
}
HWY_API Vec128<float> Shuffle0321(const Vec128<float> v) {
  return Vec128<float>{_mm_shuffle_ps(v.raw, v.raw, 0x39)};
}
// Rotate left 32 bits
HWY_API Vec128<uint32_t> Shuffle2103(const Vec128<uint32_t> v) {
  return Vec128<uint32_t>{_mm_shuffle_epi32(v.raw, 0x93)};
}
HWY_API Vec128<int32_t> Shuffle2103(const Vec128<int32_t> v) {
  return Vec128<int32_t>{_mm_shuffle_epi32(v.raw, 0x93)};
}
HWY_API Vec128<float> Shuffle2103(const Vec128<float> v) {
  return Vec128<float>{_mm_shuffle_ps(v.raw, v.raw, 0x93)};
}

// Reverse
HWY_API Vec128<uint32_t> Shuffle0123(const Vec128<uint32_t> v) {
  return Vec128<uint32_t>{_mm_shuffle_epi32(v.raw, 0x1B)};
}
HWY_API Vec128<int32_t> Shuffle0123(const Vec128<int32_t> v) {
  return Vec128<int32_t>{_mm_shuffle_epi32(v.raw, 0x1B)};
}
HWY_API Vec128<float> Shuffle0123(const Vec128<float> v) {
  return Vec128<float>{_mm_shuffle_ps(v.raw, v.raw, 0x1B)};
}

// ================================================== COMPARE

// Comparisons fill a lane with 1-bits if the condition is true, else 0.

template <typename TFrom, typename TTo, size_t N>
HWY_API Mask128<TTo, N> RebindMask(Simd<TTo, N> /*tag*/, Mask128<TFrom, N> m) {
  static_assert(sizeof(TFrom) == sizeof(TTo), "Must have same size");
  const Simd<TFrom, N> d;
  return MaskFromVec(BitCast(Simd<TTo, N>(), VecFromMask(d, m)));
}

// ------------------------------ Equality

// Unsigned
template <size_t N>
HWY_API Mask128<uint8_t, N> operator==(const Vec128<uint8_t, N> a,
                                       const Vec128<uint8_t, N> b) {
  return Mask128<uint8_t, N>{_mm_cmpeq_epi8(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<uint16_t, N> operator==(const Vec128<uint16_t, N> a,
                                        const Vec128<uint16_t, N> b) {
  return Mask128<uint16_t, N>{_mm_cmpeq_epi16(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<uint32_t, N> operator==(const Vec128<uint32_t, N> a,
                                        const Vec128<uint32_t, N> b) {
  return Mask128<uint32_t, N>{_mm_cmpeq_epi32(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<uint64_t, N> operator==(const Vec128<uint64_t, N> a,
                                        const Vec128<uint64_t, N> b) {
#if HWY_TARGET == HWY_SSSE3
  const Simd<uint32_t, N * 2> d32;
  const Simd<uint64_t, N> d64;
  const auto cmp32 = VecFromMask(d32, Eq(BitCast(d32, a), BitCast(d32, b)));
  const auto cmp64 = cmp32 & Shuffle2301(cmp32);
  return MaskFromVec(BitCast(d64, cmp64));
#else
  return Mask128<uint64_t, N>{_mm_cmpeq_epi64(a.raw, b.raw)};
#endif
}

// Signed
template <size_t N>
HWY_API Mask128<int8_t, N> operator==(const Vec128<int8_t, N> a,
                                      const Vec128<int8_t, N> b) {
  return Mask128<int8_t, N>{_mm_cmpeq_epi8(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<int16_t, N> operator==(Vec128<int16_t, N> a,
                                       Vec128<int16_t, N> b) {
  return Mask128<int16_t, N>{_mm_cmpeq_epi16(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<int32_t, N> operator==(const Vec128<int32_t, N> a,
                                       const Vec128<int32_t, N> b) {
  return Mask128<int32_t, N>{_mm_cmpeq_epi32(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<int64_t, N> operator==(const Vec128<int64_t, N> a,
                                       const Vec128<int64_t, N> b) {
  // Same as signed ==; avoid duplicating the SSSE3 version.
  const Simd<uint64_t, N> du;
  return RebindMask(Simd<int64_t, N>(), BitCast(du, a) == BitCast(du, b));
}

// Float
template <size_t N>
HWY_API Mask128<float, N> operator==(const Vec128<float, N> a,
                                     const Vec128<float, N> b) {
  return Mask128<float, N>{_mm_cmpeq_ps(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<double, N> operator==(const Vec128<double, N> a,
                                      const Vec128<double, N> b) {
  return Mask128<double, N>{_mm_cmpeq_pd(a.raw, b.raw)};
}

template <typename T, size_t N>
HWY_API Mask128<T, N> TestBit(Vec128<T, N> v, Vec128<T, N> bit) {
  static_assert(!hwy::IsFloat<T>(), "Only integer vectors supported");
  return (v & bit) == bit;
}

// ------------------------------ Strict inequality

// Signed/float <
template <size_t N>
HWY_API Mask128<int8_t, N> operator<(const Vec128<int8_t, N> a,
                                     const Vec128<int8_t, N> b) {
  return Mask128<int8_t, N>{_mm_cmpgt_epi8(b.raw, a.raw)};
}
template <size_t N>
HWY_API Mask128<int16_t, N> operator<(const Vec128<int16_t, N> a,
                                      const Vec128<int16_t, N> b) {
  return Mask128<int16_t, N>{_mm_cmpgt_epi16(b.raw, a.raw)};
}
template <size_t N>
HWY_API Mask128<int32_t, N> operator<(const Vec128<int32_t, N> a,
                                      const Vec128<int32_t, N> b) {
  return Mask128<int32_t, N>{_mm_cmpgt_epi32(b.raw, a.raw)};
}
template <size_t N>
HWY_API Mask128<float, N> operator<(const Vec128<float, N> a,
                                    const Vec128<float, N> b) {
  return Mask128<float, N>{_mm_cmplt_ps(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<double, N> operator<(const Vec128<double, N> a,
                                     const Vec128<double, N> b) {
  return Mask128<double, N>{_mm_cmplt_pd(a.raw, b.raw)};
}

// Signed/float >
template <size_t N>
HWY_API Mask128<int8_t, N> operator>(const Vec128<int8_t, N> a,
                                     const Vec128<int8_t, N> b) {
  return Mask128<int8_t, N>{_mm_cmpgt_epi8(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<int16_t, N> operator>(const Vec128<int16_t, N> a,
                                      const Vec128<int16_t, N> b) {
  return Mask128<int16_t, N>{_mm_cmpgt_epi16(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<int32_t, N> operator>(const Vec128<int32_t, N> a,
                                      const Vec128<int32_t, N> b) {
  return Mask128<int32_t, N>{_mm_cmpgt_epi32(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<float, N> operator>(const Vec128<float, N> a,
                                    const Vec128<float, N> b) {
  return Mask128<float, N>{_mm_cmpgt_ps(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<double, N> operator>(const Vec128<double, N> a,
                                     const Vec128<double, N> b) {
  return Mask128<double, N>{_mm_cmpgt_pd(a.raw, b.raw)};
}

template <size_t N>
HWY_API Mask128<int64_t, N> operator>(const Vec128<int64_t, N> a,
                                      const Vec128<int64_t, N> b) {
#if HWY_TARGET == HWY_SSSE3
  // If the upper half is less than or greater, this is the answer.
  const __m128i m_gt = _mm_cmpgt_epi32(a.raw, b.raw);

  // Otherwise, the lower half decides.
  const __m128i m_eq = _mm_cmpeq_epi32(a.raw, b.raw);
  const __m128i lo_in_hi = _mm_shuffle_epi32(m_gt, _MM_SHUFFLE(2, 2, 0, 0));
  const __m128i lo_gt = _mm_and_si128(m_eq, lo_in_hi);

  const __m128i gt = _mm_or_si128(lo_gt, m_gt);
  // Copy result in upper 32 bits to lower 32 bits.
  return Mask128<int64_t, N>{_mm_shuffle_epi32(gt, _MM_SHUFFLE(3, 3, 1, 1))};
#else
  return Mask128<int64_t, N>{_mm_cmpgt_epi64(a.raw, b.raw)};  // SSE4.2
#endif
}

template <size_t N>
HWY_API Mask128<int64_t, N> operator<(const Vec128<int64_t, N> a,
                                      const Vec128<int64_t, N> b) {
  return operator>(b, a);
}

// ------------------------------ Weak inequality

// Float <= >=
template <size_t N>
HWY_API Mask128<float, N> operator<=(const Vec128<float, N> a,
                                     const Vec128<float, N> b) {
  return Mask128<float, N>{_mm_cmple_ps(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<double, N> operator<=(const Vec128<double, N> a,
                                      const Vec128<double, N> b) {
  return Mask128<double, N>{_mm_cmple_pd(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<float, N> operator>=(const Vec128<float, N> a,
                                     const Vec128<float, N> b) {
  return Mask128<float, N>{_mm_cmpge_ps(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<double, N> operator>=(const Vec128<double, N> a,
                                      const Vec128<double, N> b) {
  return Mask128<double, N>{_mm_cmpge_pd(a.raw, b.raw)};
}

// ------------------------------ FirstN (Iota, Lt)

template <typename T, size_t N, HWY_IF_LE128(T, N)>
HWY_API Mask128<T, N> FirstN(const Simd<T, N> d, size_t num) {
  const RebindToSigned<decltype(d)> di;  // Signed comparisons are cheaper.
  return RebindMask(d, Iota(di, 0) < Set(di, static_cast<MakeSigned<T>>(num)));
}

// ================================================== MEMORY (1)

// Clang static analysis claims the memory immediately after a partial vector
// store is uninitialized, and also flags the input to partial loads (at least
// for loadl_pd) as "garbage". This is a false alarm because msan does not
// raise errors. We work around this by using CopyBytes instead of intrinsics,
// but only for the analyzer to avoid potentially bad code generation.
// Unfortunately __clang_analyzer__ was not defined for clang-tidy prior to v7.
#ifndef HWY_SAFE_PARTIAL_LOAD_STORE
#if defined(__clang_analyzer__) || \
    (HWY_COMPILER_CLANG != 0 && HWY_COMPILER_CLANG < 700)
#define HWY_SAFE_PARTIAL_LOAD_STORE 1
#else
#define HWY_SAFE_PARTIAL_LOAD_STORE 0
#endif
#endif  // HWY_SAFE_PARTIAL_LOAD_STORE

// ------------------------------ Load

template <typename T>
HWY_API Vec128<T> Load(Full128<T> /* tag */, const T* HWY_RESTRICT aligned) {
  return Vec128<T>{_mm_load_si128(reinterpret_cast<const __m128i*>(aligned))};
}
HWY_API Vec128<float> Load(Full128<float> /* tag */,
                           const float* HWY_RESTRICT aligned) {
  return Vec128<float>{_mm_load_ps(aligned)};
}
HWY_API Vec128<double> Load(Full128<double> /* tag */,
                            const double* HWY_RESTRICT aligned) {
  return Vec128<double>{_mm_load_pd(aligned)};
}

template <typename T>
HWY_API Vec128<T> LoadU(Full128<T> /* tag */, const T* HWY_RESTRICT p) {
  return Vec128<T>{_mm_loadu_si128(reinterpret_cast<const __m128i*>(p))};
}
HWY_API Vec128<float> LoadU(Full128<float> /* tag */,
                            const float* HWY_RESTRICT p) {
  return Vec128<float>{_mm_loadu_ps(p)};
}
HWY_API Vec128<double> LoadU(Full128<double> /* tag */,
                             const double* HWY_RESTRICT p) {
  return Vec128<double>{_mm_loadu_pd(p)};
}

template <typename T>
HWY_API Vec128<T, 8 / sizeof(T)> Load(Simd<T, 8 / sizeof(T)> /* tag */,
                                      const T* HWY_RESTRICT p) {
#if HWY_SAFE_PARTIAL_LOAD_STORE
  __m128i v = _mm_setzero_si128();
  CopyBytes<8>(p, &v);
  return Vec128<T, 8 / sizeof(T)>{v};
#else
  return Vec128<T, 8 / sizeof(T)>{
      _mm_loadl_epi64(reinterpret_cast<const __m128i*>(p))};
#endif
}

HWY_API Vec128<float, 2> Load(Simd<float, 2> /* tag */,
                              const float* HWY_RESTRICT p) {
#if HWY_SAFE_PARTIAL_LOAD_STORE
  __m128 v = _mm_setzero_ps();
  CopyBytes<8>(p, &v);
  return Vec128<float, 2>{v};
#else
  const __m128 hi = _mm_setzero_ps();
  return Vec128<float, 2>{_mm_loadl_pi(hi, reinterpret_cast<const __m64*>(p))};
#endif
}

HWY_API Vec128<double, 1> Load(Simd<double, 1> /* tag */,
                               const double* HWY_RESTRICT p) {
#if HWY_SAFE_PARTIAL_LOAD_STORE
  __m128d v = _mm_setzero_pd();
  CopyBytes<8>(p, &v);
  return Vec128<double, 1>{v};
#else
  return Vec128<double, 1>{_mm_load_sd(p)};
#endif
}

HWY_API Vec128<float, 1> Load(Simd<float, 1> /* tag */,
                              const float* HWY_RESTRICT p) {
#if HWY_SAFE_PARTIAL_LOAD_STORE
  __m128 v = _mm_setzero_ps();
  CopyBytes<4>(p, &v);
  return Vec128<float, 1>{v};
#else
  return Vec128<float, 1>{_mm_load_ss(p)};
#endif
}

// Any <= 32 bit except <float, 1>
template <typename T, size_t N, HWY_IF_LE32(T, N)>
HWY_API Vec128<T, N> Load(Simd<T, N> /* tag */, const T* HWY_RESTRICT p) {
  constexpr size_t kSize = sizeof(T) * N;
#if HWY_SAFE_PARTIAL_LOAD_STORE
  __m128 v = _mm_setzero_ps();
  CopyBytes<kSize>(p, &v);
  return Vec128<T, N>{v};
#else
  // TODO(janwas): load_ss?
  int32_t bits;
  CopyBytes<kSize>(p, &bits);
  return Vec128<T, N>{_mm_cvtsi32_si128(bits)};
#endif
}

// For < 128 bit, LoadU == Load.
template <typename T, size_t N, HWY_IF_LE64(T, N)>
HWY_API Vec128<T, N> LoadU(Simd<T, N> d, const T* HWY_RESTRICT p) {
  return Load(d, p);
}

// 128-bit SIMD => nothing to duplicate, same as an unaligned load.
template <typename T, size_t N, HWY_IF_LE128(T, N)>
HWY_API Vec128<T, N> LoadDup128(Simd<T, N> d, const T* HWY_RESTRICT p) {
  return LoadU(d, p);
}

// ------------------------------ Store

template <typename T>
HWY_API void Store(Vec128<T> v, Full128<T> /* tag */, T* HWY_RESTRICT aligned) {
  _mm_store_si128(reinterpret_cast<__m128i*>(aligned), v.raw);
}
HWY_API void Store(const Vec128<float> v, Full128<float> /* tag */,
                   float* HWY_RESTRICT aligned) {
  _mm_store_ps(aligned, v.raw);
}
HWY_API void Store(const Vec128<double> v, Full128<double> /* tag */,
                   double* HWY_RESTRICT aligned) {
  _mm_store_pd(aligned, v.raw);
}

template <typename T>
HWY_API void StoreU(Vec128<T> v, Full128<T> /* tag */, T* HWY_RESTRICT p) {
  _mm_storeu_si128(reinterpret_cast<__m128i*>(p), v.raw);
}
HWY_API void StoreU(const Vec128<float> v, Full128<float> /* tag */,
                    float* HWY_RESTRICT p) {
  _mm_storeu_ps(p, v.raw);
}
HWY_API void StoreU(const Vec128<double> v, Full128<double> /* tag */,
                    double* HWY_RESTRICT p) {
  _mm_storeu_pd(p, v.raw);
}

template <typename T>
HWY_API void Store(Vec128<T, 8 / sizeof(T)> v, Simd<T, 8 / sizeof(T)> /* tag */,
                   T* HWY_RESTRICT p) {
#if HWY_SAFE_PARTIAL_LOAD_STORE
  CopyBytes<8>(&v, p);
#else
  _mm_storel_epi64(reinterpret_cast<__m128i*>(p), v.raw);
#endif
}
HWY_API void Store(const Vec128<float, 2> v, Simd<float, 2> /* tag */,
                   float* HWY_RESTRICT p) {
#if HWY_SAFE_PARTIAL_LOAD_STORE
  CopyBytes<8>(&v, p);
#else
  _mm_storel_pi(reinterpret_cast<__m64*>(p), v.raw);
#endif
}
HWY_API void Store(const Vec128<double, 1> v, Simd<double, 1> /* tag */,
                   double* HWY_RESTRICT p) {
#if HWY_SAFE_PARTIAL_LOAD_STORE
  CopyBytes<8>(&v, p);
#else
  _mm_storel_pd(p, v.raw);
#endif
}

// Any <= 32 bit except <float, 1>
template <typename T, size_t N, HWY_IF_LE32(T, N)>
HWY_API void Store(Vec128<T, N> v, Simd<T, N> /* tag */, T* HWY_RESTRICT p) {
  CopyBytes<sizeof(T) * N>(&v, p);
}
HWY_API void Store(const Vec128<float, 1> v, Simd<float, 1> /* tag */,
                   float* HWY_RESTRICT p) {
#if HWY_SAFE_PARTIAL_LOAD_STORE
  CopyBytes<4>(&v, p);
#else
  _mm_store_ss(p, v.raw);
#endif
}

// For < 128 bit, StoreU == Store.
template <typename T, size_t N, HWY_IF_LE64(T, N)>
HWY_API void StoreU(const Vec128<T, N> v, Simd<T, N> d, T* HWY_RESTRICT p) {
  Store(v, d, p);
}

// ================================================== ARITHMETIC

// ------------------------------ Addition

// Unsigned
template <size_t N>
HWY_API Vec128<uint8_t, N> operator+(const Vec128<uint8_t, N> a,
                                     const Vec128<uint8_t, N> b) {
  return Vec128<uint8_t, N>{_mm_add_epi8(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint16_t, N> operator+(const Vec128<uint16_t, N> a,
                                      const Vec128<uint16_t, N> b) {
  return Vec128<uint16_t, N>{_mm_add_epi16(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint32_t, N> operator+(const Vec128<uint32_t, N> a,
                                      const Vec128<uint32_t, N> b) {
  return Vec128<uint32_t, N>{_mm_add_epi32(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint64_t, N> operator+(const Vec128<uint64_t, N> a,
                                      const Vec128<uint64_t, N> b) {
  return Vec128<uint64_t, N>{_mm_add_epi64(a.raw, b.raw)};
}

// Signed
template <size_t N>
HWY_API Vec128<int8_t, N> operator+(const Vec128<int8_t, N> a,
                                    const Vec128<int8_t, N> b) {
  return Vec128<int8_t, N>{_mm_add_epi8(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int16_t, N> operator+(const Vec128<int16_t, N> a,
                                     const Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>{_mm_add_epi16(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int32_t, N> operator+(const Vec128<int32_t, N> a,
                                     const Vec128<int32_t, N> b) {
  return Vec128<int32_t, N>{_mm_add_epi32(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int64_t, N> operator+(const Vec128<int64_t, N> a,
                                     const Vec128<int64_t, N> b) {
  return Vec128<int64_t, N>{_mm_add_epi64(a.raw, b.raw)};
}

// Float
template <size_t N>
HWY_API Vec128<float, N> operator+(const Vec128<float, N> a,
                                   const Vec128<float, N> b) {
  return Vec128<float, N>{_mm_add_ps(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> operator+(const Vec128<double, N> a,
                                    const Vec128<double, N> b) {
  return Vec128<double, N>{_mm_add_pd(a.raw, b.raw)};
}

// ------------------------------ Subtraction

// Unsigned
template <size_t N>
HWY_API Vec128<uint8_t, N> operator-(const Vec128<uint8_t, N> a,
                                     const Vec128<uint8_t, N> b) {
  return Vec128<uint8_t, N>{_mm_sub_epi8(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint16_t, N> operator-(Vec128<uint16_t, N> a,
                                      Vec128<uint16_t, N> b) {
  return Vec128<uint16_t, N>{_mm_sub_epi16(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint32_t, N> operator-(const Vec128<uint32_t, N> a,
                                      const Vec128<uint32_t, N> b) {
  return Vec128<uint32_t, N>{_mm_sub_epi32(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint64_t, N> operator-(const Vec128<uint64_t, N> a,
                                      const Vec128<uint64_t, N> b) {
  return Vec128<uint64_t, N>{_mm_sub_epi64(a.raw, b.raw)};
}

// Signed
template <size_t N>
HWY_API Vec128<int8_t, N> operator-(const Vec128<int8_t, N> a,
                                    const Vec128<int8_t, N> b) {
  return Vec128<int8_t, N>{_mm_sub_epi8(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int16_t, N> operator-(const Vec128<int16_t, N> a,
                                     const Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>{_mm_sub_epi16(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int32_t, N> operator-(const Vec128<int32_t, N> a,
                                     const Vec128<int32_t, N> b) {
  return Vec128<int32_t, N>{_mm_sub_epi32(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int64_t, N> operator-(const Vec128<int64_t, N> a,
                                     const Vec128<int64_t, N> b) {
  return Vec128<int64_t, N>{_mm_sub_epi64(a.raw, b.raw)};
}

// Float
template <size_t N>
HWY_API Vec128<float, N> operator-(const Vec128<float, N> a,
                                   const Vec128<float, N> b) {
  return Vec128<float, N>{_mm_sub_ps(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> operator-(const Vec128<double, N> a,
                                    const Vec128<double, N> b) {
  return Vec128<double, N>{_mm_sub_pd(a.raw, b.raw)};
}

// ------------------------------ Saturating addition

// Returns a + b clamped to the destination range.

// Unsigned
template <size_t N>
HWY_API Vec128<uint8_t, N> SaturatedAdd(const Vec128<uint8_t, N> a,
                                        const Vec128<uint8_t, N> b) {
  return Vec128<uint8_t, N>{_mm_adds_epu8(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint16_t, N> SaturatedAdd(const Vec128<uint16_t, N> a,
                                         const Vec128<uint16_t, N> b) {
  return Vec128<uint16_t, N>{_mm_adds_epu16(a.raw, b.raw)};
}

// Signed
template <size_t N>
HWY_API Vec128<int8_t, N> SaturatedAdd(const Vec128<int8_t, N> a,
                                       const Vec128<int8_t, N> b) {
  return Vec128<int8_t, N>{_mm_adds_epi8(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int16_t, N> SaturatedAdd(const Vec128<int16_t, N> a,
                                        const Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>{_mm_adds_epi16(a.raw, b.raw)};
}

// ------------------------------ Saturating subtraction

// Returns a - b clamped to the destination range.

// Unsigned
template <size_t N>
HWY_API Vec128<uint8_t, N> SaturatedSub(const Vec128<uint8_t, N> a,
                                        const Vec128<uint8_t, N> b) {
  return Vec128<uint8_t, N>{_mm_subs_epu8(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint16_t, N> SaturatedSub(const Vec128<uint16_t, N> a,
                                         const Vec128<uint16_t, N> b) {
  return Vec128<uint16_t, N>{_mm_subs_epu16(a.raw, b.raw)};
}

// Signed
template <size_t N>
HWY_API Vec128<int8_t, N> SaturatedSub(const Vec128<int8_t, N> a,
                                       const Vec128<int8_t, N> b) {
  return Vec128<int8_t, N>{_mm_subs_epi8(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int16_t, N> SaturatedSub(const Vec128<int16_t, N> a,
                                        const Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>{_mm_subs_epi16(a.raw, b.raw)};
}

// ------------------------------ AverageRound

// Returns (a + b + 1) / 2

// Unsigned
template <size_t N>
HWY_API Vec128<uint8_t, N> AverageRound(const Vec128<uint8_t, N> a,
                                        const Vec128<uint8_t, N> b) {
  return Vec128<uint8_t, N>{_mm_avg_epu8(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint16_t, N> AverageRound(const Vec128<uint16_t, N> a,
                                         const Vec128<uint16_t, N> b) {
  return Vec128<uint16_t, N>{_mm_avg_epu16(a.raw, b.raw)};
}

// ------------------------------ Abs

// Returns absolute value, except that LimitsMin() maps to LimitsMax() + 1.
template <size_t N>
HWY_API Vec128<int8_t, N> Abs(const Vec128<int8_t, N> v) {
#if HWY_COMPILER_MSVC
  // Workaround for incorrect codegen? (reaches breakpoint)
  const auto zero = Zero(Simd<int8_t, N>());
  return Vec128<int8_t, N>{_mm_max_epi8(v.raw, (zero - v).raw)};
#else
  return Vec128<int8_t, N>{_mm_abs_epi8(v.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<int16_t, N> Abs(const Vec128<int16_t, N> v) {
  return Vec128<int16_t, N>{_mm_abs_epi16(v.raw)};
}
template <size_t N>
HWY_API Vec128<int32_t, N> Abs(const Vec128<int32_t, N> v) {
  return Vec128<int32_t, N>{_mm_abs_epi32(v.raw)};
}
// i64 is implemented after BroadcastSignBit.
template <size_t N>
HWY_API Vec128<float, N> Abs(const Vec128<float, N> v) {
  const Vec128<int32_t, N> mask{_mm_set1_epi32(0x7FFFFFFF)};
  return v & BitCast(Simd<float, N>(), mask);
}
template <size_t N>
HWY_API Vec128<double, N> Abs(const Vec128<double, N> v) {
  const Vec128<int64_t, N> mask{_mm_set1_epi64x(0x7FFFFFFFFFFFFFFFLL)};
  return v & BitCast(Simd<double, N>(), mask);
}

// ------------------------------ Integer multiplication

template <size_t N>
HWY_API Vec128<uint16_t, N> operator*(const Vec128<uint16_t, N> a,
                                      const Vec128<uint16_t, N> b) {
  return Vec128<uint16_t, N>{_mm_mullo_epi16(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int16_t, N> operator*(const Vec128<int16_t, N> a,
                                     const Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>{_mm_mullo_epi16(a.raw, b.raw)};
}

// Returns the upper 16 bits of a * b in each lane.
template <size_t N>
HWY_API Vec128<uint16_t, N> MulHigh(const Vec128<uint16_t, N> a,
                                    const Vec128<uint16_t, N> b) {
  return Vec128<uint16_t, N>{_mm_mulhi_epu16(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int16_t, N> MulHigh(const Vec128<int16_t, N> a,
                                   const Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>{_mm_mulhi_epi16(a.raw, b.raw)};
}

// Multiplies even lanes (0, 2 ..) and places the double-wide result into
// even and the upper half into its odd neighbor lane.
template <size_t N>
HWY_API Vec128<uint64_t, (N + 1) / 2> MulEven(const Vec128<uint32_t, N> a,
                                              const Vec128<uint32_t, N> b) {
  return Vec128<uint64_t, (N + 1) / 2>{_mm_mul_epu32(a.raw, b.raw)};
}

#if HWY_TARGET == HWY_SSSE3

template <size_t N, HWY_IF_LE64(int32_t, N)>  // N=1 or 2
HWY_API Vec128<int64_t, (N + 1) / 2> MulEven(const Vec128<int32_t, N> a,
                                             const Vec128<int32_t, N> b) {
  return Set(Simd<int64_t, (N + 1) / 2>(), int64_t(GetLane(a)) * GetLane(b));
}
HWY_API Vec128<int64_t> MulEven(const Vec128<int32_t> a,
                                const Vec128<int32_t> b) {
  alignas(16) int32_t a_lanes[4];
  alignas(16) int32_t b_lanes[4];
  const Full128<int32_t> di32;
  Store(a, di32, a_lanes);
  Store(b, di32, b_lanes);
  alignas(16) int64_t mul[2];
  mul[0] = int64_t(a_lanes[0]) * b_lanes[0];
  mul[1] = int64_t(a_lanes[2]) * b_lanes[2];
  return Load(Full128<int64_t>(), mul);
}

#else  // HWY_TARGET == HWY_SSSE3

template <size_t N>
HWY_API Vec128<int64_t, (N + 1) / 2> MulEven(const Vec128<int32_t, N> a,
                                             const Vec128<int32_t, N> b) {
  return Vec128<int64_t, (N + 1) / 2>{_mm_mul_epi32(a.raw, b.raw)};
}

#endif  // HWY_TARGET == HWY_SSSE3

template <size_t N>
HWY_API Vec128<uint32_t, N> operator*(const Vec128<uint32_t, N> a,
                                      const Vec128<uint32_t, N> b) {
#if HWY_TARGET == HWY_SSSE3
  // Not as inefficient as it looks: _mm_mullo_epi32 has 10 cycle latency.
  // 64-bit right shift would also work but also needs port 5, so no benefit.
  // Notation: x=don't care, z=0.
  const __m128i a_x3x1 = _mm_shuffle_epi32(a.raw, _MM_SHUFFLE(3, 3, 1, 1));
  const auto mullo_x2x0 = MulEven(a, b);
  const __m128i b_x3x1 = _mm_shuffle_epi32(b.raw, _MM_SHUFFLE(3, 3, 1, 1));
  const auto mullo_x3x1 =
      MulEven(Vec128<uint32_t, N>{a_x3x1}, Vec128<uint32_t, N>{b_x3x1});
  // We could _mm_slli_epi64 by 32 to get 3z1z and OR with z2z0, but generating
  // the latter requires one more instruction or a constant.
  const __m128i mul_20 =
      _mm_shuffle_epi32(mullo_x2x0.raw, _MM_SHUFFLE(2, 0, 2, 0));
  const __m128i mul_31 =
      _mm_shuffle_epi32(mullo_x3x1.raw, _MM_SHUFFLE(2, 0, 2, 0));
  return Vec128<uint32_t, N>{_mm_unpacklo_epi32(mul_20, mul_31)};
#else
  return Vec128<uint32_t, N>{_mm_mullo_epi32(a.raw, b.raw)};
#endif
}

template <size_t N>
HWY_API Vec128<int32_t, N> operator*(const Vec128<int32_t, N> a,
                                     const Vec128<int32_t, N> b) {
  // Same as unsigned; avoid duplicating the SSSE3 code.
  const Simd<uint32_t, N> du;
  return BitCast(Simd<int32_t, N>(), BitCast(du, a) * BitCast(du, b));
}

// ------------------------------ ShiftLeft

template <int kBits, size_t N>
HWY_API Vec128<uint16_t, N> ShiftLeft(const Vec128<uint16_t, N> v) {
  return Vec128<uint16_t, N>{_mm_slli_epi16(v.raw, kBits)};
}

template <int kBits, size_t N>
HWY_API Vec128<uint32_t, N> ShiftLeft(const Vec128<uint32_t, N> v) {
  return Vec128<uint32_t, N>{_mm_slli_epi32(v.raw, kBits)};
}

template <int kBits, size_t N>
HWY_API Vec128<uint64_t, N> ShiftLeft(const Vec128<uint64_t, N> v) {
  return Vec128<uint64_t, N>{_mm_slli_epi64(v.raw, kBits)};
}

template <int kBits, size_t N>
HWY_API Vec128<int16_t, N> ShiftLeft(const Vec128<int16_t, N> v) {
  return Vec128<int16_t, N>{_mm_slli_epi16(v.raw, kBits)};
}
template <int kBits, size_t N>
HWY_API Vec128<int32_t, N> ShiftLeft(const Vec128<int32_t, N> v) {
  return Vec128<int32_t, N>{_mm_slli_epi32(v.raw, kBits)};
}
template <int kBits, size_t N>
HWY_API Vec128<int64_t, N> ShiftLeft(const Vec128<int64_t, N> v) {
  return Vec128<int64_t, N>{_mm_slli_epi64(v.raw, kBits)};
}

template <int kBits, typename T, size_t N, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Vec128<T, N> ShiftLeft(const Vec128<T, N> v) {
  const Simd<T, N> d8;
  // Use raw instead of BitCast to support N=1.
  const Vec128<T, N> shifted{ShiftLeft<kBits>(Vec128<MakeWide<T>>{v.raw}).raw};
  return kBits == 1
             ? (v + v)
             : (shifted & Set(d8, static_cast<T>((0xFF << kBits) & 0xFF)));
}

// ------------------------------ ShiftRight

template <int kBits, size_t N>
HWY_API Vec128<uint16_t, N> ShiftRight(const Vec128<uint16_t, N> v) {
  return Vec128<uint16_t, N>{_mm_srli_epi16(v.raw, kBits)};
}
template <int kBits, size_t N>
HWY_API Vec128<uint32_t, N> ShiftRight(const Vec128<uint32_t, N> v) {
  return Vec128<uint32_t, N>{_mm_srli_epi32(v.raw, kBits)};
}
template <int kBits, size_t N>
HWY_API Vec128<uint64_t, N> ShiftRight(const Vec128<uint64_t, N> v) {
  return Vec128<uint64_t, N>{_mm_srli_epi64(v.raw, kBits)};
}

template <int kBits, size_t N>
HWY_API Vec128<uint8_t, N> ShiftRight(const Vec128<uint8_t, N> v) {
  const Simd<uint8_t, N> d8;
  // Use raw instead of BitCast to support N=1.
  const Vec128<uint8_t, N> shifted{
      ShiftRight<kBits>(Vec128<uint16_t>{v.raw}).raw};
  return shifted & Set(d8, 0xFF >> kBits);
}

template <int kBits, size_t N>
HWY_API Vec128<int16_t, N> ShiftRight(const Vec128<int16_t, N> v) {
  return Vec128<int16_t, N>{_mm_srai_epi16(v.raw, kBits)};
}
template <int kBits, size_t N>
HWY_API Vec128<int32_t, N> ShiftRight(const Vec128<int32_t, N> v) {
  return Vec128<int32_t, N>{_mm_srai_epi32(v.raw, kBits)};
}

template <int kBits, size_t N>
HWY_API Vec128<int8_t, N> ShiftRight(const Vec128<int8_t, N> v) {
  const Simd<int8_t, N> di;
  const Simd<uint8_t, N> du;
  const auto shifted = BitCast(di, ShiftRight<kBits>(BitCast(du, v)));
  const auto shifted_sign = BitCast(di, Set(du, 0x80 >> kBits));
  return (shifted ^ shifted_sign) - shifted_sign;
}

// i64 is implemented after BroadcastSignBit.

// ------------------------------ BroadcastSignBit (ShiftRight, compare, mask)

template <size_t N>
HWY_API Vec128<int8_t, N> BroadcastSignBit(const Vec128<int8_t, N> v) {
  return VecFromMask(v < Zero(Simd<int8_t, N>()));
}

template <size_t N>
HWY_API Vec128<int16_t, N> BroadcastSignBit(const Vec128<int16_t, N> v) {
  return ShiftRight<15>(v);
}

template <size_t N>
HWY_API Vec128<int32_t, N> BroadcastSignBit(const Vec128<int32_t, N> v) {
  return ShiftRight<31>(v);
}

template <size_t N>
HWY_API Vec128<int64_t, N> BroadcastSignBit(const Vec128<int64_t, N> v) {
#if HWY_TARGET <= HWY_AVX3
  return Vec128<int64_t, N>{_mm_srai_epi64(v.raw, 63)};
#elif HWY_TARGET == HWY_AVX2 || HWY_TARGET == HWY_SSE4
  return VecFromMask(v < Zero(Simd<int64_t, N>()));
#else
  // Efficient Lt() requires SSE4.2 and BLENDVPD requires SSE4.1. 32-bit shift
  // avoids generating a zero.
  const Simd<int32_t, N * 2> d32;
  const auto sign = ShiftRight<31>(BitCast(d32, v));
  return Vec128<int64_t, N>{
      _mm_shuffle_epi32(sign.raw, _MM_SHUFFLE(3, 3, 1, 1))};
#endif
}

template <size_t N>
HWY_API Vec128<int64_t, N> Abs(const Vec128<int64_t, N> v) {
#if HWY_TARGET <= HWY_AVX3
  return Vec128<int64_t, N>{_mm_abs_epi64(v.raw)};
#else
  const auto zero = Zero(Simd<int64_t,N>());
  return IfThenElse(MaskFromVec(BroadcastSignBit(v)), zero - v, v);
#endif
}

template <int kBits, size_t N>
HWY_API Vec128<int64_t, N> ShiftRight(const Vec128<int64_t, N> v) {
#if HWY_TARGET <= HWY_AVX3
  return Vec128<int64_t, N>{_mm_srai_epi64(v.raw, kBits)};
#else
  const Simd<int64_t, N> di;
  const Simd<uint64_t, N> du;
  const auto right = BitCast(di, ShiftRight<kBits>(BitCast(du, v)));
  const auto sign = ShiftLeft<64 - kBits>(BroadcastSignBit(v));
  return right | sign;
#endif
}

// ------------------------------ ZeroIfNegative (BroadcastSignBit)
template <typename T, size_t N, HWY_IF_FLOAT(T)>
HWY_API Vec128<T, N> ZeroIfNegative(Vec128<T, N> v) {
  const Simd<T, N> d;
#if HWY_TARGET == HWY_SSSE3
  const RebindToSigned<decltype(d)> di;
  const auto mask = MaskFromVec(BitCast(d, BroadcastSignBit(BitCast(di, v))));
#else
  const auto mask = MaskFromVec(v);  // MSB is sufficient for BLENDVPS
#endif
  return IfThenElse(mask, Zero(d), v);
}

// ------------------------------ ShiftLeftSame

template <size_t N>
HWY_API Vec128<uint16_t, N> ShiftLeftSame(const Vec128<uint16_t, N> v,
                                          const int bits) {
  return Vec128<uint16_t, N>{_mm_sll_epi16(v.raw, _mm_cvtsi32_si128(bits))};
}
template <size_t N>
HWY_API Vec128<uint32_t, N> ShiftLeftSame(const Vec128<uint32_t, N> v,
                                          const int bits) {
  return Vec128<uint32_t, N>{_mm_sll_epi32(v.raw, _mm_cvtsi32_si128(bits))};
}
template <size_t N>
HWY_API Vec128<uint64_t, N> ShiftLeftSame(const Vec128<uint64_t, N> v,
                                          const int bits) {
  return Vec128<uint64_t, N>{_mm_sll_epi64(v.raw, _mm_cvtsi32_si128(bits))};
}

template <size_t N>
HWY_API Vec128<int16_t, N> ShiftLeftSame(const Vec128<int16_t, N> v,
                                         const int bits) {
  return Vec128<int16_t, N>{_mm_sll_epi16(v.raw, _mm_cvtsi32_si128(bits))};
}

template <size_t N>
HWY_API Vec128<int32_t, N> ShiftLeftSame(const Vec128<int32_t, N> v,
                                         const int bits) {
  return Vec128<int32_t, N>{_mm_sll_epi32(v.raw, _mm_cvtsi32_si128(bits))};
}

template <size_t N>
HWY_API Vec128<int64_t, N> ShiftLeftSame(const Vec128<int64_t, N> v,
                                         const int bits) {
  return Vec128<int64_t, N>{_mm_sll_epi64(v.raw, _mm_cvtsi32_si128(bits))};
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Vec128<T, N> ShiftLeftSame(const Vec128<T, N> v, const int bits) {
  const Simd<T, N> d8;
  // Use raw instead of BitCast to support N=1.
  const Vec128<T, N> shifted{
      ShiftLeftSame(Vec128<MakeWide<T>>{v.raw}, bits).raw};
  return shifted & Set(d8, (0xFF << bits) & 0xFF);
}

// ------------------------------ ShiftRightSame (BroadcastSignBit)

template <size_t N>
HWY_API Vec128<uint16_t, N> ShiftRightSame(const Vec128<uint16_t, N> v,
                                           const int bits) {
  return Vec128<uint16_t, N>{_mm_srl_epi16(v.raw, _mm_cvtsi32_si128(bits))};
}
template <size_t N>
HWY_API Vec128<uint32_t, N> ShiftRightSame(const Vec128<uint32_t, N> v,
                                           const int bits) {
  return Vec128<uint32_t, N>{_mm_srl_epi32(v.raw, _mm_cvtsi32_si128(bits))};
}
template <size_t N>
HWY_API Vec128<uint64_t, N> ShiftRightSame(const Vec128<uint64_t, N> v,
                                           const int bits) {
  return Vec128<uint64_t, N>{_mm_srl_epi64(v.raw, _mm_cvtsi32_si128(bits))};
}

template <size_t N>
HWY_API Vec128<uint8_t, N> ShiftRightSame(Vec128<uint8_t, N> v,
                                          const int bits) {
  const Simd<uint8_t, N> d8;
  // Use raw instead of BitCast to support N=1.
  const Vec128<uint8_t, N> shifted{
      ShiftRightSame(Vec128<uint16_t>{v.raw}, bits).raw};
  return shifted & Set(d8, 0xFF >> bits);
}

template <size_t N>
HWY_API Vec128<int16_t, N> ShiftRightSame(const Vec128<int16_t, N> v,
                                          const int bits) {
  return Vec128<int16_t, N>{_mm_sra_epi16(v.raw, _mm_cvtsi32_si128(bits))};
}

template <size_t N>
HWY_API Vec128<int32_t, N> ShiftRightSame(const Vec128<int32_t, N> v,
                                          const int bits) {
  return Vec128<int32_t, N>{_mm_sra_epi32(v.raw, _mm_cvtsi32_si128(bits))};
}
template <size_t N>
HWY_API Vec128<int64_t, N> ShiftRightSame(const Vec128<int64_t, N> v,
                                          const int bits) {
#if HWY_TARGET <= HWY_AVX3
  return Vec128<int64_t, N>{_mm_sra_epi64(v.raw, _mm_cvtsi32_si128(bits))};
#else
  const Simd<int64_t, N> di;
  const Simd<uint64_t, N> du;
  const auto right = BitCast(di, ShiftRightSame(BitCast(du, v), bits));
  const auto sign = ShiftLeftSame(BroadcastSignBit(v), 64 - bits);
  return right | sign;
#endif
}

template <size_t N>
HWY_API Vec128<int8_t, N> ShiftRightSame(Vec128<int8_t, N> v, const int bits) {
  const Simd<int8_t, N> di;
  const Simd<uint8_t, N> du;
  const auto shifted = BitCast(di, ShiftRightSame(BitCast(du, v), bits));
  const auto shifted_sign = BitCast(di, Set(du, 0x80 >> bits));
  return (shifted ^ shifted_sign) - shifted_sign;
}

// ------------------------------ Negate

template <typename T, size_t N, HWY_IF_FLOAT(T)>
HWY_API Vec128<T, N> Neg(const Vec128<T, N> v) {
  return Xor(v, SignBit(Simd<T, N>()));
}

template <typename T, size_t N, HWY_IF_NOT_FLOAT(T)>
HWY_API Vec128<T, N> Neg(const Vec128<T, N> v) {
  return Zero(Simd<T, N>()) - v;
}

// ------------------------------ Floating-point mul / div

template <size_t N>
HWY_API Vec128<float, N> operator*(Vec128<float, N> a, Vec128<float, N> b) {
  return Vec128<float, N>{_mm_mul_ps(a.raw, b.raw)};
}
HWY_API Vec128<float, 1> operator*(const Vec128<float, 1> a,
                                   const Vec128<float, 1> b) {
  return Vec128<float, 1>{_mm_mul_ss(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> operator*(const Vec128<double, N> a,
                                    const Vec128<double, N> b) {
  return Vec128<double, N>{_mm_mul_pd(a.raw, b.raw)};
}
HWY_API Vec128<double, 1> operator*(const Vec128<double, 1> a,
                                    const Vec128<double, 1> b) {
  return Vec128<double, 1>{_mm_mul_sd(a.raw, b.raw)};
}

template <size_t N>
HWY_API Vec128<float, N> operator/(const Vec128<float, N> a,
                                   const Vec128<float, N> b) {
  return Vec128<float, N>{_mm_div_ps(a.raw, b.raw)};
}
HWY_API Vec128<float, 1> operator/(const Vec128<float, 1> a,
                                   const Vec128<float, 1> b) {
  return Vec128<float, 1>{_mm_div_ss(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> operator/(const Vec128<double, N> a,
                                    const Vec128<double, N> b) {
  return Vec128<double, N>{_mm_div_pd(a.raw, b.raw)};
}
HWY_API Vec128<double, 1> operator/(const Vec128<double, 1> a,
                                    const Vec128<double, 1> b) {
  return Vec128<double, 1>{_mm_div_sd(a.raw, b.raw)};
}

// Approximate reciprocal
template <size_t N>
HWY_API Vec128<float, N> ApproximateReciprocal(const Vec128<float, N> v) {
  return Vec128<float, N>{_mm_rcp_ps(v.raw)};
}
HWY_API Vec128<float, 1> ApproximateReciprocal(const Vec128<float, 1> v) {
  return Vec128<float, 1>{_mm_rcp_ss(v.raw)};
}

// Absolute value of difference.
template <size_t N>
HWY_API Vec128<float, N> AbsDiff(const Vec128<float, N> a,
                                 const Vec128<float, N> b) {
  return Abs(a - b);
}

// ------------------------------ Floating-point multiply-add variants

// Returns mul * x + add
template <size_t N>
HWY_API Vec128<float, N> MulAdd(const Vec128<float, N> mul,
                                const Vec128<float, N> x,
                                const Vec128<float, N> add) {
#if HWY_TARGET == HWY_SSSE3 || HWY_TARGET == HWY_SSE4
  return mul * x + add;
#else
  return Vec128<float, N>{_mm_fmadd_ps(mul.raw, x.raw, add.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<double, N> MulAdd(const Vec128<double, N> mul,
                                 const Vec128<double, N> x,
                                 const Vec128<double, N> add) {
#if HWY_TARGET == HWY_SSSE3 || HWY_TARGET == HWY_SSE4
  return mul * x + add;
#else
  return Vec128<double, N>{_mm_fmadd_pd(mul.raw, x.raw, add.raw)};
#endif
}

// Returns add - mul * x
template <size_t N>
HWY_API Vec128<float, N> NegMulAdd(const Vec128<float, N> mul,
                                   const Vec128<float, N> x,
                                   const Vec128<float, N> add) {
#if HWY_TARGET == HWY_SSSE3 || HWY_TARGET == HWY_SSE4
  return add - mul * x;
#else
  return Vec128<float, N>{_mm_fnmadd_ps(mul.raw, x.raw, add.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<double, N> NegMulAdd(const Vec128<double, N> mul,
                                    const Vec128<double, N> x,
                                    const Vec128<double, N> add) {
#if HWY_TARGET == HWY_SSSE3 || HWY_TARGET == HWY_SSE4
  return add - mul * x;
#else
  return Vec128<double, N>{_mm_fnmadd_pd(mul.raw, x.raw, add.raw)};
#endif
}

// Returns mul * x - sub
template <size_t N>
HWY_API Vec128<float, N> MulSub(const Vec128<float, N> mul,
                                const Vec128<float, N> x,
                                const Vec128<float, N> sub) {
#if HWY_TARGET == HWY_SSSE3 || HWY_TARGET == HWY_SSE4
  return mul * x - sub;
#else
  return Vec128<float, N>{_mm_fmsub_ps(mul.raw, x.raw, sub.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<double, N> MulSub(const Vec128<double, N> mul,
                                 const Vec128<double, N> x,
                                 const Vec128<double, N> sub) {
#if HWY_TARGET == HWY_SSSE3 || HWY_TARGET == HWY_SSE4
  return mul * x - sub;
#else
  return Vec128<double, N>{_mm_fmsub_pd(mul.raw, x.raw, sub.raw)};
#endif
}

// Returns -mul * x - sub
template <size_t N>
HWY_API Vec128<float, N> NegMulSub(const Vec128<float, N> mul,
                                   const Vec128<float, N> x,
                                   const Vec128<float, N> sub) {
#if HWY_TARGET == HWY_SSSE3 || HWY_TARGET == HWY_SSE4
  return Neg(mul) * x - sub;
#else
  return Vec128<float, N>{_mm_fnmsub_ps(mul.raw, x.raw, sub.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<double, N> NegMulSub(const Vec128<double, N> mul,
                                    const Vec128<double, N> x,
                                    const Vec128<double, N> sub) {
#if HWY_TARGET == HWY_SSSE3 || HWY_TARGET == HWY_SSE4
  return Neg(mul) * x - sub;
#else
  return Vec128<double, N>{_mm_fnmsub_pd(mul.raw, x.raw, sub.raw)};
#endif
}

// ------------------------------ Floating-point square root

// Full precision square root
template <size_t N>
HWY_API Vec128<float, N> Sqrt(const Vec128<float, N> v) {
  return Vec128<float, N>{_mm_sqrt_ps(v.raw)};
}
HWY_API Vec128<float, 1> Sqrt(const Vec128<float, 1> v) {
  return Vec128<float, 1>{_mm_sqrt_ss(v.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> Sqrt(const Vec128<double, N> v) {
  return Vec128<double, N>{_mm_sqrt_pd(v.raw)};
}
HWY_API Vec128<double, 1> Sqrt(const Vec128<double, 1> v) {
  return Vec128<double, 1>{_mm_sqrt_sd(_mm_setzero_pd(), v.raw)};
}

// Approximate reciprocal square root
template <size_t N>
HWY_API Vec128<float, N> ApproximateReciprocalSqrt(const Vec128<float, N> v) {
  return Vec128<float, N>{_mm_rsqrt_ps(v.raw)};
}
HWY_API Vec128<float, 1> ApproximateReciprocalSqrt(const Vec128<float, 1> v) {
  return Vec128<float, 1>{_mm_rsqrt_ss(v.raw)};
}

// ------------------------------ Min (Gt, IfThenElse)

namespace detail {

template <typename T, size_t N>
HWY_INLINE HWY_MAYBE_UNUSED Vec128<T, N> MinU(const Vec128<T, N> a,
                                              const Vec128<T, N> b) {
  const Simd<T, N> du;
  const RebindToSigned<decltype(du)> di;
  const auto msb = Set(du, static_cast<T>(T(1) << (sizeof(T) * 8 - 1)));
  const auto gt = RebindMask(du, BitCast(di, a ^ msb) > BitCast(di, b ^ msb));
  return IfThenElse(gt, b, a);
}

}  // namespace detail

// Unsigned
template <size_t N>
HWY_API Vec128<uint8_t, N> Min(const Vec128<uint8_t, N> a,
                               const Vec128<uint8_t, N> b) {
  return Vec128<uint8_t, N>{_mm_min_epu8(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint16_t, N> Min(const Vec128<uint16_t, N> a,
                                const Vec128<uint16_t, N> b) {
#if HWY_TARGET == HWY_SSSE3
  return detail::MinU(a, b);
#else
  return Vec128<uint16_t, N>{_mm_min_epu16(a.raw, b.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<uint32_t, N> Min(const Vec128<uint32_t, N> a,
                                const Vec128<uint32_t, N> b) {
#if HWY_TARGET == HWY_SSSE3
  return detail::MinU(a, b);
#else
  return Vec128<uint32_t, N>{_mm_min_epu32(a.raw, b.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<uint64_t, N> Min(const Vec128<uint64_t, N> a,
                                const Vec128<uint64_t, N> b) {
#if HWY_TARGET <= HWY_AVX3
  return Vec128<uint64_t, N>{_mm_min_epu64(a.raw, b.raw)};
#else
  return detail::MinU(a, b);
#endif
}

// Signed
template <size_t N>
HWY_API Vec128<int8_t, N> Min(const Vec128<int8_t, N> a,
                              const Vec128<int8_t, N> b) {
#if HWY_TARGET == HWY_SSSE3
  return IfThenElse(a < b, a, b);
#else
  return Vec128<int8_t, N>{_mm_min_epi8(a.raw, b.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<int16_t, N> Min(const Vec128<int16_t, N> a,
                               const Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>{_mm_min_epi16(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int32_t, N> Min(const Vec128<int32_t, N> a,
                               const Vec128<int32_t, N> b) {
#if HWY_TARGET == HWY_SSSE3
  return IfThenElse(a < b, a, b);
#else
  return Vec128<int32_t, N>{_mm_min_epi32(a.raw, b.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<int64_t, N> Min(const Vec128<int64_t, N> a,
                               const Vec128<int64_t, N> b) {
#if HWY_TARGET <= HWY_AVX3
  return Vec128<int64_t, N>{_mm_min_epi64(a.raw, b.raw)};
#else
  return IfThenElse(a < b, a, b);
#endif
}

// Float
template <size_t N>
HWY_API Vec128<float, N> Min(const Vec128<float, N> a,
                             const Vec128<float, N> b) {
  return Vec128<float, N>{_mm_min_ps(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> Min(const Vec128<double, N> a,
                              const Vec128<double, N> b) {
  return Vec128<double, N>{_mm_min_pd(a.raw, b.raw)};
}

// ------------------------------ Max (Gt, IfThenElse)

namespace detail {
template <typename T, size_t N>
HWY_INLINE HWY_MAYBE_UNUSED Vec128<T, N> MaxU(const Vec128<T, N> a,
                                              const Vec128<T, N> b) {
  const Simd<T, N> du;
  const RebindToSigned<decltype(du)> di;
  const auto msb = Set(du, static_cast<T>(T(1) << (sizeof(T) * 8 - 1)));
  const auto gt = RebindMask(du, BitCast(di, a ^ msb) > BitCast(di, b ^ msb));
  return IfThenElse(gt, a, b);
}

}  // namespace detail

// Unsigned
template <size_t N>
HWY_API Vec128<uint8_t, N> Max(const Vec128<uint8_t, N> a,
                               const Vec128<uint8_t, N> b) {
  return Vec128<uint8_t, N>{_mm_max_epu8(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint16_t, N> Max(const Vec128<uint16_t, N> a,
                                const Vec128<uint16_t, N> b) {
#if HWY_TARGET == HWY_SSSE3
  return detail::MaxU(a, b);
#else
  return Vec128<uint16_t, N>{_mm_max_epu16(a.raw, b.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<uint32_t, N> Max(const Vec128<uint32_t, N> a,
                                const Vec128<uint32_t, N> b) {
#if HWY_TARGET == HWY_SSSE3
  return detail::MaxU(a, b);
#else
  return Vec128<uint32_t, N>{_mm_max_epu32(a.raw, b.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<uint64_t, N> Max(const Vec128<uint64_t, N> a,
                                const Vec128<uint64_t, N> b) {
#if HWY_TARGET <= HWY_AVX3
  return Vec128<uint64_t, N>{_mm_max_epu64(a.raw, b.raw)};
#else
  return detail::MaxU(a, b);
#endif
}

// Signed
template <size_t N>
HWY_API Vec128<int8_t, N> Max(const Vec128<int8_t, N> a,
                              const Vec128<int8_t, N> b) {
#if HWY_TARGET == HWY_SSSE3
  return IfThenElse(a < b, b, a);
#else
  return Vec128<int8_t, N>{_mm_max_epi8(a.raw, b.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<int16_t, N> Max(const Vec128<int16_t, N> a,
                               const Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>{_mm_max_epi16(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int32_t, N> Max(const Vec128<int32_t, N> a,
                               const Vec128<int32_t, N> b) {
#if HWY_TARGET == HWY_SSSE3
  return IfThenElse(a < b, b, a);
#else
  return Vec128<int32_t, N>{_mm_max_epi32(a.raw, b.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<int64_t, N> Max(const Vec128<int64_t, N> a,
                               const Vec128<int64_t, N> b) {
#if HWY_TARGET <= HWY_AVX3
  return Vec128<int64_t, N>{_mm_max_epi64(a.raw, b.raw)};
#else
  return IfThenElse(a < b, b, a);
#endif
}

// Float
template <size_t N>
HWY_API Vec128<float, N> Max(const Vec128<float, N> a,
                             const Vec128<float, N> b) {
  return Vec128<float, N>{_mm_max_ps(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> Max(const Vec128<double, N> a,
                              const Vec128<double, N> b) {
  return Vec128<double, N>{_mm_max_pd(a.raw, b.raw)};
}

// ================================================== MEMORY (2)

// ------------------------------ Non-temporal stores

// On clang6, we see incorrect code generated for _mm_stream_pi, so
// round even partial vectors up to 16 bytes.
template <typename T, size_t N>
HWY_API void Stream(Vec128<T, N> v, Simd<T, N> /* tag */,
                    T* HWY_RESTRICT aligned) {
  _mm_stream_si128(reinterpret_cast<__m128i*>(aligned), v.raw);
}
template <size_t N>
HWY_API void Stream(const Vec128<float, N> v, Simd<float, N> /* tag */,
                    float* HWY_RESTRICT aligned) {
  _mm_stream_ps(aligned, v.raw);
}
template <size_t N>
HWY_API void Stream(const Vec128<double, N> v, Simd<double, N> /* tag */,
                    double* HWY_RESTRICT aligned) {
  _mm_stream_pd(aligned, v.raw);
}

// ------------------------------ Scatter

// Work around warnings in the intrinsic definitions (passing -1 as a mask).
HWY_DIAGNOSTICS(push)
HWY_DIAGNOSTICS_OFF(disable : 4245 4365, ignored "-Wsign-conversion")

// Unfortunately the GCC/Clang intrinsics do not accept int64_t*.
using GatherIndex64 = long long int;  // NOLINT(google-runtime-int)
static_assert(sizeof(GatherIndex64) == 8, "Must be 64-bit type");

#if HWY_TARGET <= HWY_AVX3
namespace detail {

template <typename T, size_t N>
HWY_INLINE void ScatterOffset(hwy::SizeTag<4> /* tag */, Vec128<T, N> v,
                              Simd<T, N> /* tag */, T* HWY_RESTRICT base,
                              const Vec128<int32_t, N> offset) {
  if (N == 4) {
    _mm_i32scatter_epi32(base, offset.raw, v.raw, 1);
  } else {
    const __mmask8 mask = (1u << N) - 1;
    _mm_mask_i32scatter_epi32(base, mask, offset.raw, v.raw, 1);
  }
}
template <typename T, size_t N>
HWY_INLINE void ScatterIndex(hwy::SizeTag<4> /* tag */, Vec128<T, N> v,
                             Simd<T, N> /* tag */, T* HWY_RESTRICT base,
                             const Vec128<int32_t, N> index) {
  if (N == 4) {
    _mm_i32scatter_epi32(base, index.raw, v.raw, 4);
  } else {
    const __mmask8 mask = (1u << N) - 1;
    _mm_mask_i32scatter_epi32(base, mask, index.raw, v.raw, 4);
  }
}

template <typename T, size_t N>
HWY_INLINE void ScatterOffset(hwy::SizeTag<8> /* tag */, Vec128<T, N> v,
                              Simd<T, N> /* tag */, T* HWY_RESTRICT base,
                              const Vec128<int64_t, N> offset) {
  if (N == 2) {
    _mm_i64scatter_epi64(base, offset.raw, v.raw, 1);
  } else {
    const __mmask8 mask = (1u << N) - 1;
    _mm_mask_i64scatter_epi64(base, mask, offset.raw, v.raw, 1);
  }
}
template <typename T, size_t N>
HWY_INLINE void ScatterIndex(hwy::SizeTag<8> /* tag */, Vec128<T, N> v,
                             Simd<T, N> /* tag */, T* HWY_RESTRICT base,
                             const Vec128<int64_t, N> index) {
  if (N == 2) {
    _mm_i64scatter_epi64(base, index.raw, v.raw, 8);
  } else {
    const __mmask8 mask = (1u << N) - 1;
    _mm_mask_i64scatter_epi64(base, mask, index.raw, v.raw, 8);
  }
}

}  // namespace detail

template <typename T, size_t N, typename Offset>
HWY_API void ScatterOffset(Vec128<T, N> v, Simd<T, N> d, T* HWY_RESTRICT base,
                           const Vec128<Offset, N> offset) {
  static_assert(sizeof(T) == sizeof(Offset), "Must match for portability");
  return detail::ScatterOffset(hwy::SizeTag<sizeof(T)>(), v, d, base, offset);
}
template <typename T, size_t N, typename Index>
HWY_API void ScatterIndex(Vec128<T, N> v, Simd<T, N> d, T* HWY_RESTRICT base,
                          const Vec128<Index, N> index) {
  static_assert(sizeof(T) == sizeof(Index), "Must match for portability");
  return detail::ScatterIndex(hwy::SizeTag<sizeof(T)>(), v, d, base, index);
}

template <size_t N>
HWY_API void ScatterOffset(Vec128<float, N> v, Simd<float, N> /* tag */,
                           float* HWY_RESTRICT base,
                           const Vec128<int32_t, N> offset) {
  if (N == 4) {
    _mm_i32scatter_ps(base, offset.raw, v.raw, 1);
  } else {
    const __mmask8 mask = (1u << N) - 1;
    _mm_mask_i32scatter_ps(base, mask, offset.raw, v.raw, 1);
  }
}
template <size_t N>
HWY_API void ScatterIndex(Vec128<float, N> v, Simd<float, N> /* tag */,
                          float* HWY_RESTRICT base,
                          const Vec128<int32_t, N> index) {
  if (N == 4) {
    _mm_i32scatter_ps(base, index.raw, v.raw, 4);
  } else {
    const __mmask8 mask = (1u << N) - 1;
    _mm_mask_i32scatter_ps(base, mask, index.raw, v.raw, 4);
  }
}

template <size_t N>
HWY_API void ScatterOffset(Vec128<double, N> v, Simd<double, N> /* tag */,
                           double* HWY_RESTRICT base,
                           const Vec128<int64_t, N> offset) {
  if (N == 2) {
    _mm_i64scatter_pd(base, offset.raw, v.raw, 1);
  } else {
    const __mmask8 mask = (1u << N) - 1;
    _mm_mask_i64scatter_pd(base, mask, offset.raw, v.raw, 1);
  }
}
template <size_t N>
HWY_API void ScatterIndex(Vec128<double, N> v, Simd<double, N> /* tag */,
                          double* HWY_RESTRICT base,
                          const Vec128<int64_t, N> index) {
  if (N == 2) {
    _mm_i64scatter_pd(base, index.raw, v.raw, 8);
  } else {
    const __mmask8 mask = (1u << N) - 1;
    _mm_mask_i64scatter_pd(base, mask, index.raw, v.raw, 8);
  }
}
#else  // HWY_TARGET <= HWY_AVX3

template <typename T, size_t N, typename Offset, HWY_IF_LE128(T, N)>
HWY_API void ScatterOffset(Vec128<T, N> v, Simd<T, N> d, T* HWY_RESTRICT base,
                           const Vec128<Offset, N> offset) {
  static_assert(sizeof(T) == sizeof(Offset), "Must match for portability");

  alignas(16) T lanes[N];
  Store(v, d, lanes);

  alignas(16) Offset offset_lanes[N];
  Store(offset, Simd<Offset, N>(), offset_lanes);

  uint8_t* base_bytes = reinterpret_cast<uint8_t*>(base);
  for (size_t i = 0; i < N; ++i) {
    CopyBytes<sizeof(T)>(&lanes[i], base_bytes + offset_lanes[i]);
  }
}

template <typename T, size_t N, typename Index, HWY_IF_LE128(T, N)>
HWY_API void ScatterIndex(Vec128<T, N> v, Simd<T, N> d, T* HWY_RESTRICT base,
                          const Vec128<Index, N> index) {
  static_assert(sizeof(T) == sizeof(Index), "Must match for portability");

  alignas(16) T lanes[N];
  Store(v, d, lanes);

  alignas(16) Index index_lanes[N];
  Store(index, Simd<Index, N>(), index_lanes);

  for (size_t i = 0; i < N; ++i) {
    base[index_lanes[i]] = lanes[i];
  }
}

#endif

// ------------------------------ Gather (Load/Store)

#if HWY_TARGET == HWY_SSSE3 || HWY_TARGET == HWY_SSE4

template <typename T, size_t N, typename Offset>
HWY_API Vec128<T, N> GatherOffset(const Simd<T, N> d,
                                  const T* HWY_RESTRICT base,
                                  const Vec128<Offset, N> offset) {
  static_assert(sizeof(T) == sizeof(Offset), "Must match for portability");

  alignas(16) Offset offset_lanes[N];
  Store(offset, Simd<Offset, N>(), offset_lanes);

  alignas(16) T lanes[N];
  const uint8_t* base_bytes = reinterpret_cast<const uint8_t*>(base);
  for (size_t i = 0; i < N; ++i) {
    CopyBytes<sizeof(T)>(base_bytes + offset_lanes[i], &lanes[i]);
  }
  return Load(d, lanes);
}

template <typename T, size_t N, typename Index>
HWY_API Vec128<T, N> GatherIndex(const Simd<T, N> d, const T* HWY_RESTRICT base,
                                 const Vec128<Index, N> index) {
  static_assert(sizeof(T) == sizeof(Index), "Must match for portability");

  alignas(16) Index index_lanes[N];
  Store(index, Simd<Index, N>(), index_lanes);

  alignas(16) T lanes[N];
  for (size_t i = 0; i < N; ++i) {
    lanes[i] = base[index_lanes[i]];
  }
  return Load(d, lanes);
}

#else

namespace detail {

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> GatherOffset(hwy::SizeTag<4> /* tag */,
                                     Simd<T, N> /* d */,
                                     const T* HWY_RESTRICT base,
                                     const Vec128<int32_t, N> offset) {
  return Vec128<T, N>{_mm_i32gather_epi32(
      reinterpret_cast<const int32_t*>(base), offset.raw, 1)};
}
template <typename T, size_t N>
HWY_INLINE Vec128<T, N> GatherIndex(hwy::SizeTag<4> /* tag */,
                                    Simd<T, N> /* d */,
                                    const T* HWY_RESTRICT base,
                                    const Vec128<int32_t, N> index) {
  return Vec128<T, N>{_mm_i32gather_epi32(
      reinterpret_cast<const int32_t*>(base), index.raw, 4)};
}

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> GatherOffset(hwy::SizeTag<8> /* tag */,
                                     Simd<T, N> /* d */,
                                     const T* HWY_RESTRICT base,
                                     const Vec128<int64_t, N> offset) {
  return Vec128<T, N>{_mm_i64gather_epi64(
      reinterpret_cast<const GatherIndex64*>(base), offset.raw, 1)};
}
template <typename T, size_t N>
HWY_INLINE Vec128<T, N> GatherIndex(hwy::SizeTag<8> /* tag */,
                                    Simd<T, N> /* d */,
                                    const T* HWY_RESTRICT base,
                                    const Vec128<int64_t, N> index) {
  return Vec128<T, N>{_mm_i64gather_epi64(
      reinterpret_cast<const GatherIndex64*>(base), index.raw, 8)};
}

}  // namespace detail

template <typename T, size_t N, typename Offset>
HWY_API Vec128<T, N> GatherOffset(Simd<T, N> d, const T* HWY_RESTRICT base,
                                  const Vec128<Offset, N> offset) {
  return detail::GatherOffset(hwy::SizeTag<sizeof(T)>(), d, base, offset);
}
template <typename T, size_t N, typename Index>
HWY_API Vec128<T, N> GatherIndex(Simd<T, N> d, const T* HWY_RESTRICT base,
                                 const Vec128<Index, N> index) {
  return detail::GatherIndex(hwy::SizeTag<sizeof(T)>(), d, base, index);
}

template <size_t N>
HWY_API Vec128<float, N> GatherOffset(Simd<float, N> /* tag */,
                                      const float* HWY_RESTRICT base,
                                      const Vec128<int32_t, N> offset) {
  return Vec128<float, N>{_mm_i32gather_ps(base, offset.raw, 1)};
}
template <size_t N>
HWY_API Vec128<float, N> GatherIndex(Simd<float, N> /* tag */,
                                     const float* HWY_RESTRICT base,
                                     const Vec128<int32_t, N> index) {
  return Vec128<float, N>{_mm_i32gather_ps(base, index.raw, 4)};
}

template <size_t N>
HWY_API Vec128<double, N> GatherOffset(Simd<double, N> /* tag */,
                                       const double* HWY_RESTRICT base,
                                       const Vec128<int64_t, N> offset) {
  return Vec128<double, N>{_mm_i64gather_pd(base, offset.raw, 1)};
}
template <size_t N>
HWY_API Vec128<double, N> GatherIndex(Simd<double, N> /* tag */,
                                      const double* HWY_RESTRICT base,
                                      const Vec128<int64_t, N> index) {
  return Vec128<double, N>{_mm_i64gather_pd(base, index.raw, 8)};
}

#endif  // HWY_TARGET == HWY_SSSE3 || HWY_TARGET == HWY_SSE4

HWY_DIAGNOSTICS(pop)

// ================================================== SWIZZLE (2)

// ------------------------------ Extract half

// Returns upper/lower half of a vector.
template <typename T, size_t N>
HWY_API Vec128<T, N / 2> LowerHalf(Vec128<T, N> v) {
  return Vec128<T, N / 2>{v.raw};
}

// These copy hi into lo (smaller instruction encoding than shifts).
template <typename T>
HWY_API Vec128<T, 8 / sizeof(T)> UpperHalf(Vec128<T> v) {
  return Vec128<T, 8 / sizeof(T)>{_mm_unpackhi_epi64(v.raw, v.raw)};
}
HWY_API Vec128<float, 2> UpperHalf(Vec128<float> v) {
  return Vec128<float, 2>{_mm_movehl_ps(v.raw, v.raw)};
}
HWY_API Vec128<double, 1> UpperHalf(Vec128<double> v) {
  return Vec128<double, 1>{_mm_unpackhi_pd(v.raw, v.raw)};
}

// ------------------------------ Shift vector by constant #bytes

// 0x01..0F, kBytes = 1 => 0x02..0F00
template <int kBytes, typename T, size_t N>
HWY_API Vec128<T, N> ShiftLeftBytes(const Vec128<T, N> v) {
  static_assert(0 <= kBytes && kBytes <= 16, "Invalid kBytes");
  return Vec128<T, N>{_mm_slli_si128(v.raw, kBytes)};
}

template <int kLanes, typename T, size_t N>
HWY_API Vec128<T, N> ShiftLeftLanes(const Vec128<T, N> v) {
  const Simd<uint8_t, N * sizeof(T)> d8;
  const Simd<T, N> d;
  return BitCast(d, ShiftLeftBytes<kLanes * sizeof(T)>(BitCast(d8, v)));
}

// 0x01..0F, kBytes = 1 => 0x0001..0E
template <int kBytes, typename T, size_t N>
HWY_API Vec128<T, N> ShiftRightBytes(const Vec128<T, N> v) {
  static_assert(0 <= kBytes && kBytes <= 16, "Invalid kBytes");
  return Vec128<T, N>{_mm_srli_si128(v.raw, kBytes)};
}

template <int kLanes, typename T, size_t N>
HWY_API Vec128<T, N> ShiftRightLanes(const Vec128<T, N> v) {
  const Simd<uint8_t, N * sizeof(T)> d8;
  const Simd<T, N> d;
  return BitCast(d, ShiftRightBytes<kLanes * sizeof(T)>(BitCast(d8, v)));
}

// ------------------------------ Extract from 2x 128-bit at constant offset

// Extracts 128 bits from <hi, lo> by skipping the least-significant kBytes.
template <int kBytes, typename T>
HWY_API Vec128<T> CombineShiftRightBytes(const Vec128<T> hi,
                                         const Vec128<T> lo) {
  const Full128<uint8_t> d8;
  const Vec128<uint8_t> extracted_bytes{
      _mm_alignr_epi8(BitCast(d8, hi).raw, BitCast(d8, lo).raw, kBytes)};
  return BitCast(Full128<T>(), extracted_bytes);
}

// ------------------------------ Broadcast/splat any lane

// Unsigned
template <int kLane, size_t N>
HWY_API Vec128<uint16_t, N> Broadcast(const Vec128<uint16_t, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  if (kLane < 4) {
    const __m128i lo = _mm_shufflelo_epi16(v.raw, (0x55 * kLane) & 0xFF);
    return Vec128<uint16_t, N>{_mm_unpacklo_epi64(lo, lo)};
  } else {
    const __m128i hi = _mm_shufflehi_epi16(v.raw, (0x55 * (kLane - 4)) & 0xFF);
    return Vec128<uint16_t, N>{_mm_unpackhi_epi64(hi, hi)};
  }
}
template <int kLane, size_t N>
HWY_API Vec128<uint32_t, N> Broadcast(const Vec128<uint32_t, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec128<uint32_t, N>{_mm_shuffle_epi32(v.raw, 0x55 * kLane)};
}
template <int kLane, size_t N>
HWY_API Vec128<uint64_t, N> Broadcast(const Vec128<uint64_t, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec128<uint64_t, N>{_mm_shuffle_epi32(v.raw, kLane ? 0xEE : 0x44)};
}

// Signed
template <int kLane, size_t N>
HWY_API Vec128<int16_t, N> Broadcast(const Vec128<int16_t, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  if (kLane < 4) {
    const __m128i lo = _mm_shufflelo_epi16(v.raw, (0x55 * kLane) & 0xFF);
    return Vec128<int16_t, N>{_mm_unpacklo_epi64(lo, lo)};
  } else {
    const __m128i hi = _mm_shufflehi_epi16(v.raw, (0x55 * (kLane - 4)) & 0xFF);
    return Vec128<int16_t, N>{_mm_unpackhi_epi64(hi, hi)};
  }
}
template <int kLane, size_t N>
HWY_API Vec128<int32_t, N> Broadcast(const Vec128<int32_t, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec128<int32_t, N>{_mm_shuffle_epi32(v.raw, 0x55 * kLane)};
}
template <int kLane, size_t N>
HWY_API Vec128<int64_t, N> Broadcast(const Vec128<int64_t, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec128<int64_t, N>{_mm_shuffle_epi32(v.raw, kLane ? 0xEE : 0x44)};
}

// Float
template <int kLane, size_t N>
HWY_API Vec128<float, N> Broadcast(const Vec128<float, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec128<float, N>{_mm_shuffle_ps(v.raw, v.raw, 0x55 * kLane)};
}
template <int kLane, size_t N>
HWY_API Vec128<double, N> Broadcast(const Vec128<double, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec128<double, N>{_mm_shuffle_pd(v.raw, v.raw, 3 * kLane)};
}

// ------------------------------ Shuffle bytes with variable indices

// Returns vector of bytes[from[i]]. "from" is also interpreted as bytes, i.e.
// lane indices in [0, 16).
template <typename T, size_t N>
HWY_API Vec128<T, N> TableLookupBytes(const Vec128<T, N> bytes,
                                      const Vec128<T, N> from) {
  return Vec128<T, N>{_mm_shuffle_epi8(bytes.raw, from.raw)};
}

// For all vector widths; x86 anyway zeroes if >= 0x80.
template <class V>
HWY_API V TableLookupBytesOr0(const V bytes, const V from) {
  return TableLookupBytes(bytes, from);
}

// ------------------------------ TableLookupLanes

// Returned by SetTableIndices for use by TableLookupLanes.
template <typename T, size_t N>
struct Indices128 {
  __m128i raw;
};

template <typename T, size_t N, HWY_IF_LE128(T, N)>
HWY_API Indices128<T, N> SetTableIndices(Simd<T, N> d, const int32_t* idx) {
#if !defined(NDEBUG) || defined(ADDRESS_SANITIZER)
  for (size_t i = 0; i < N; ++i) {
    HWY_DASSERT(0 <= idx[i] && idx[i] < static_cast<int32_t>(N));
  }
#endif

  const Repartition<uint8_t, decltype(d)> d8;
  alignas(16) uint8_t control[16] = {0};
  for (size_t idx_lane = 0; idx_lane < N; ++idx_lane) {
    for (size_t idx_byte = 0; idx_byte < sizeof(T); ++idx_byte) {
      control[idx_lane * sizeof(T) + idx_byte] =
          static_cast<uint8_t>(idx[idx_lane] * sizeof(T) + idx_byte);
    }
  }
  return Indices128<T, N>{Load(d8, control).raw};
}

template <size_t N>
HWY_API Vec128<uint32_t, N> TableLookupLanes(
    const Vec128<uint32_t, N> v, const Indices128<uint32_t, N> idx) {
  return TableLookupBytes(v, Vec128<uint32_t, N>{idx.raw});
}
template <size_t N>
HWY_API Vec128<int32_t, N> TableLookupLanes(const Vec128<int32_t, N> v,
                                            const Indices128<int32_t, N> idx) {
  return TableLookupBytes(v, Vec128<int32_t, N>{idx.raw});
}
template <size_t N>
HWY_API Vec128<float, N> TableLookupLanes(const Vec128<float, N> v,
                                          const Indices128<float, N> idx) {
  const Simd<int32_t, N> di;
  const Simd<float, N> df;
  return BitCast(df,
                 TableLookupBytes(BitCast(di, v), Vec128<int32_t, N>{idx.raw}));
}

// ------------------------------ Interleave lanes

// Interleaves lanes from halves of the 128-bit blocks of "a" (which provides
// the least-significant lane) and "b". To concatenate two half-width integers
// into one, use ZipLower/Upper instead (also works with scalar).

HWY_API Vec128<uint8_t> InterleaveLower(const Vec128<uint8_t> a,
                                        const Vec128<uint8_t> b) {
  return Vec128<uint8_t>{_mm_unpacklo_epi8(a.raw, b.raw)};
}
HWY_API Vec128<uint16_t> InterleaveLower(const Vec128<uint16_t> a,
                                         const Vec128<uint16_t> b) {
  return Vec128<uint16_t>{_mm_unpacklo_epi16(a.raw, b.raw)};
}
HWY_API Vec128<uint32_t> InterleaveLower(const Vec128<uint32_t> a,
                                         const Vec128<uint32_t> b) {
  return Vec128<uint32_t>{_mm_unpacklo_epi32(a.raw, b.raw)};
}
HWY_API Vec128<uint64_t> InterleaveLower(const Vec128<uint64_t> a,
                                         const Vec128<uint64_t> b) {
  return Vec128<uint64_t>{_mm_unpacklo_epi64(a.raw, b.raw)};
}

HWY_API Vec128<int8_t> InterleaveLower(const Vec128<int8_t> a,
                                       const Vec128<int8_t> b) {
  return Vec128<int8_t>{_mm_unpacklo_epi8(a.raw, b.raw)};
}
HWY_API Vec128<int16_t> InterleaveLower(const Vec128<int16_t> a,
                                        const Vec128<int16_t> b) {
  return Vec128<int16_t>{_mm_unpacklo_epi16(a.raw, b.raw)};
}
HWY_API Vec128<int32_t> InterleaveLower(const Vec128<int32_t> a,
                                        const Vec128<int32_t> b) {
  return Vec128<int32_t>{_mm_unpacklo_epi32(a.raw, b.raw)};
}
HWY_API Vec128<int64_t> InterleaveLower(const Vec128<int64_t> a,
                                        const Vec128<int64_t> b) {
  return Vec128<int64_t>{_mm_unpacklo_epi64(a.raw, b.raw)};
}

HWY_API Vec128<float> InterleaveLower(const Vec128<float> a,
                                      const Vec128<float> b) {
  return Vec128<float>{_mm_unpacklo_ps(a.raw, b.raw)};
}
HWY_API Vec128<double> InterleaveLower(const Vec128<double> a,
                                       const Vec128<double> b) {
  return Vec128<double>{_mm_unpacklo_pd(a.raw, b.raw)};
}

HWY_API Vec128<uint8_t> InterleaveUpper(const Vec128<uint8_t> a,
                                        const Vec128<uint8_t> b) {
  return Vec128<uint8_t>{_mm_unpackhi_epi8(a.raw, b.raw)};
}
HWY_API Vec128<uint16_t> InterleaveUpper(const Vec128<uint16_t> a,
                                         const Vec128<uint16_t> b) {
  return Vec128<uint16_t>{_mm_unpackhi_epi16(a.raw, b.raw)};
}
HWY_API Vec128<uint32_t> InterleaveUpper(const Vec128<uint32_t> a,
                                         const Vec128<uint32_t> b) {
  return Vec128<uint32_t>{_mm_unpackhi_epi32(a.raw, b.raw)};
}
HWY_API Vec128<uint64_t> InterleaveUpper(const Vec128<uint64_t> a,
                                         const Vec128<uint64_t> b) {
  return Vec128<uint64_t>{_mm_unpackhi_epi64(a.raw, b.raw)};
}

HWY_API Vec128<int8_t> InterleaveUpper(const Vec128<int8_t> a,
                                       const Vec128<int8_t> b) {
  return Vec128<int8_t>{_mm_unpackhi_epi8(a.raw, b.raw)};
}
HWY_API Vec128<int16_t> InterleaveUpper(const Vec128<int16_t> a,
                                        const Vec128<int16_t> b) {
  return Vec128<int16_t>{_mm_unpackhi_epi16(a.raw, b.raw)};
}
HWY_API Vec128<int32_t> InterleaveUpper(const Vec128<int32_t> a,
                                        const Vec128<int32_t> b) {
  return Vec128<int32_t>{_mm_unpackhi_epi32(a.raw, b.raw)};
}
HWY_API Vec128<int64_t> InterleaveUpper(const Vec128<int64_t> a,
                                        const Vec128<int64_t> b) {
  return Vec128<int64_t>{_mm_unpackhi_epi64(a.raw, b.raw)};
}

HWY_API Vec128<float> InterleaveUpper(const Vec128<float> a,
                                      const Vec128<float> b) {
  return Vec128<float>{_mm_unpackhi_ps(a.raw, b.raw)};
}
HWY_API Vec128<double> InterleaveUpper(const Vec128<double> a,
                                       const Vec128<double> b) {
  return Vec128<double>{_mm_unpackhi_pd(a.raw, b.raw)};
}

// ------------------------------ Zip lanes

// Same as interleave_*, except that the return lanes are double-width integers;
// this is necessary because the single-lane scalar cannot return two values.

template <size_t N>
HWY_API Vec128<uint16_t, (N + 1) / 2> ZipLower(const Vec128<uint8_t, N> a,
                                               const Vec128<uint8_t, N> b) {
  return Vec128<uint16_t, (N + 1) / 2>{_mm_unpacklo_epi8(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint32_t, (N + 1) / 2> ZipLower(const Vec128<uint16_t, N> a,
                                               const Vec128<uint16_t, N> b) {
  return Vec128<uint32_t, (N + 1) / 2>{_mm_unpacklo_epi16(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint64_t, (N + 1) / 2> ZipLower(const Vec128<uint32_t, N> a,
                                               const Vec128<uint32_t, N> b) {
  return Vec128<uint64_t, (N + 1) / 2>{_mm_unpacklo_epi32(a.raw, b.raw)};
}

template <size_t N>
HWY_API Vec128<int16_t, (N + 1) / 2> ZipLower(const Vec128<int8_t, N> a,
                                              const Vec128<int8_t, N> b) {
  return Vec128<int16_t, (N + 1) / 2>{_mm_unpacklo_epi8(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int32_t, (N + 1) / 2> ZipLower(const Vec128<int16_t, N> a,
                                              const Vec128<int16_t, N> b) {
  return Vec128<int32_t, (N + 1) / 2>{_mm_unpacklo_epi16(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int64_t, (N + 1) / 2> ZipLower(const Vec128<int32_t, N> a,
                                              const Vec128<int32_t, N> b) {
  return Vec128<int64_t, (N + 1) / 2>{_mm_unpacklo_epi32(a.raw, b.raw)};
}

template <size_t N>
HWY_API Vec128<uint16_t, (N + 1) / 2> ZipUpper(const Vec128<uint8_t, N> a,
                                               const Vec128<uint8_t, N> b) {
  return Vec128<uint16_t, (N + 1) / 2>{_mm_unpackhi_epi8(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint32_t, (N + 1) / 2> ZipUpper(const Vec128<uint16_t, N> a,
                                               const Vec128<uint16_t, N> b) {
  return Vec128<uint32_t, (N + 1) / 2>{_mm_unpackhi_epi16(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint64_t, (N + 1) / 2> ZipUpper(const Vec128<uint32_t, N> a,
                                               const Vec128<uint32_t, N> b) {
  return Vec128<uint64_t, (N + 1) / 2>{_mm_unpackhi_epi32(a.raw, b.raw)};
}

template <size_t N>
HWY_API Vec128<int16_t, (N + 1) / 2> ZipUpper(const Vec128<int8_t, N> a,
                                              const Vec128<int8_t, N> b) {
  return Vec128<int16_t, (N + 1) / 2>{_mm_unpackhi_epi8(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int32_t, (N + 1) / 2> ZipUpper(const Vec128<int16_t, N> a,
                                              const Vec128<int16_t, N> b) {
  return Vec128<int32_t, (N + 1) / 2>{_mm_unpackhi_epi16(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int64_t, (N + 1) / 2> ZipUpper(const Vec128<int32_t, N> a,
                                              const Vec128<int32_t, N> b) {
  return Vec128<int64_t, (N + 1) / 2>{_mm_unpackhi_epi32(a.raw, b.raw)};
}

// ------------------------------ Blocks

// hiH,hiL loH,loL |-> hiL,loL (= lower halves)
template <typename T>
HWY_API Vec128<T> ConcatLowerLower(const Vec128<T> hi, const Vec128<T> lo) {
  const Full128<uint64_t> d64;
  return BitCast(Full128<T>(),
                 InterleaveLower(BitCast(d64, lo), BitCast(d64, hi)));
}

// hiH,hiL loH,loL |-> hiH,loH (= upper halves)
template <typename T>
HWY_API Vec128<T> ConcatUpperUpper(const Vec128<T> hi, const Vec128<T> lo) {
  const Full128<uint64_t> d64;
  return BitCast(Full128<T>(),
                 InterleaveUpper(BitCast(d64, lo), BitCast(d64, hi)));
}

// hiH,hiL loH,loL |-> hiL,loH (= inner halves)
template <typename T>
HWY_API Vec128<T> ConcatLowerUpper(const Vec128<T> hi, const Vec128<T> lo) {
  return CombineShiftRightBytes<8>(hi, lo);
}

// hiH,hiL loH,loL |-> hiH,loL (= outer halves)
template <typename T>
HWY_API Vec128<T> ConcatUpperLower(const Vec128<T> hi, const Vec128<T> lo) {
#if HWY_TARGET == HWY_SSSE3
  const Full128<double> dd;
  const __m128d concat = _mm_move_sd(BitCast(dd, hi).raw, BitCast(dd, lo).raw);
  return BitCast(Full128<T>(), Vec128<double>{concat});
#else
  return Vec128<T>{_mm_blend_epi16(hi.raw, lo.raw, 0x0F)};
#endif
}
HWY_API Vec128<float> ConcatUpperLower(const Vec128<float> hi,
                                       const Vec128<float> lo) {
  return Vec128<float>{_mm_shuffle_ps(lo.raw, hi.raw, _MM_SHUFFLE(3, 2, 1, 0))};
}
HWY_API Vec128<double> ConcatUpperLower(const Vec128<double> hi,
                                        const Vec128<double> lo) {
  return Vec128<double>{_mm_shuffle_pd(lo.raw, hi.raw, _MM_SHUFFLE2(1, 0))};
}

// ------------------------------ OddEven (IfThenElse)

namespace detail {

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> OddEven(hwy::SizeTag<1> /* tag */, const Vec128<T, N> a,
                                const Vec128<T, N> b) {
  const Simd<T, N> d;
  const Repartition<uint8_t, decltype(d)> d8;
  alignas(16) constexpr uint8_t mask[16] = {0xFF, 0, 0xFF, 0, 0xFF, 0, 0xFF, 0,
                                            0xFF, 0, 0xFF, 0, 0xFF, 0, 0xFF, 0};
  return IfThenElse(MaskFromVec(BitCast(d, Load(d8, mask))), b, a);
}
template <typename T, size_t N>
HWY_INLINE Vec128<T, N> OddEven(hwy::SizeTag<2> /* tag */, const Vec128<T, N> a,
                                const Vec128<T, N> b) {
#if HWY_TARGET == HWY_SSSE3
  const Simd<T, N> d;
  const Repartition<uint8_t, decltype(d)> d8;
  alignas(16) constexpr uint8_t mask[16] = {0xFF, 0xFF, 0, 0, 0xFF, 0xFF, 0, 0,
                                            0xFF, 0xFF, 0, 0, 0xFF, 0xFF, 0, 0};
  return IfThenElse(MaskFromVec(BitCast(d, Load(d8, mask))), b, a);
#else
  return Vec128<T, N>{_mm_blend_epi16(a.raw, b.raw, 0x55)};
#endif
}
template <typename T, size_t N>
HWY_INLINE Vec128<T, N> OddEven(hwy::SizeTag<4> /* tag */, const Vec128<T, N> a,
                                const Vec128<T, N> b) {
#if HWY_TARGET == HWY_SSSE3
  const __m128i odd = _mm_shuffle_epi32(a.raw, _MM_SHUFFLE(3, 1, 3, 1));
  const __m128i even = _mm_shuffle_epi32(b.raw, _MM_SHUFFLE(2, 0, 2, 0));
  return Vec128<T, N>{_mm_unpacklo_epi32(even, odd)};
#else
  return Vec128<T, N>{_mm_blend_epi16(a.raw, b.raw, 0x33)};
#endif
}
template <typename T, size_t N>
HWY_INLINE Vec128<T, N> OddEven(hwy::SizeTag<8> /* tag */, const Vec128<T, N> a,
                                const Vec128<T, N> b) {
#if HWY_TARGET == HWY_SSSE3
  const Full128<double> dd;
  const __m128d concat = _mm_move_sd(BitCast(dd, a).raw, BitCast(dd, b).raw);
  return BitCast(Full128<T>(), Vec128<double>{concat});
#else
  return Vec128<T, N>{_mm_blend_epi16(a.raw, b.raw, 0x0F)};
#endif
}

}  // namespace detail

template <typename T, size_t N>
HWY_API Vec128<T, N> OddEven(const Vec128<T, N> a, const Vec128<T, N> b) {
  return detail::OddEven(hwy::SizeTag<sizeof(T)>(), a, b);
}
template <size_t N>
HWY_API Vec128<float, N> OddEven(const Vec128<float, N> a,
                                 const Vec128<float, N> b) {
#if HWY_TARGET == HWY_SSSE3
  // SHUFPS must fill the lower half of the output from one register, so we
  // need another shuffle. Unpack avoids another immediate byte.
  const __m128 odd = _mm_shuffle_ps(a.raw, a.raw, _MM_SHUFFLE(3, 1, 3, 1));
  const __m128 even = _mm_shuffle_ps(b.raw, b.raw, _MM_SHUFFLE(2, 0, 2, 0));
  return Vec128<float, N>{_mm_unpacklo_ps(even, odd)};
#else
  return Vec128<float, N>{_mm_blend_ps(a.raw, b.raw, 5)};
#endif
}

template <size_t N>
HWY_API Vec128<double, N> OddEven(const Vec128<double, N> a,
                                  const Vec128<double, N> b) {
  return Vec128<double>{_mm_shuffle_pd(b.raw, a.raw, _MM_SHUFFLE2(1, 0))};
}

// ------------------------------ Shl (ZipLower, Mul)

// Use AVX2/3 variable shifts where available, otherwise multiply by powers of
// two from loading float exponents, which is considerably faster (according
// to LLVM-MCA) than scalar or testing bits: https://gcc.godbolt.org/z/9G7Y9v.

#if HWY_TARGET > HWY_AVX3  // AVX2 or older
namespace detail {

// Returns 2^v for use as per-lane multipliers to emulate 16-bit shifts.
template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 2)>
HWY_INLINE Vec128<MakeUnsigned<T>, N> Pow2(const Vec128<T, N> v) {
  const Simd<T, N> d;
  const Repartition<float, decltype(d)> df;
  const auto zero = Zero(d);
  // Move into exponent (this u16 will become the upper half of an f32)
  const auto exp = ShiftLeft<23 - 16>(v);
  const auto upper = exp + Set(d, 0x3F80);  // upper half of 1.0f
  // Insert 0 into lower halves for reinterpreting as binary32.
  const auto f0 = ZipLower(zero, upper);
  const auto f1 = ZipUpper(zero, upper);
  // See comment below.
  const Vec128<int32_t, N> bits0{_mm_cvtps_epi32(BitCast(df, f0).raw)};
  const Vec128<int32_t, N> bits1{_mm_cvtps_epi32(BitCast(df, f1).raw)};
  return Vec128<MakeUnsigned<T>, N>{_mm_packus_epi32(bits0.raw, bits1.raw)};
}

// Same, for 32-bit shifts.
template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 4)>
HWY_INLINE Vec128<MakeUnsigned<T>, N> Pow2(const Vec128<T, N> v) {
  const Simd<T, N> d;
  const auto exp = ShiftLeft<23>(v);
  const auto f = exp + Set(d, 0x3F800000);  // 1.0f
  // Do not use ConvertTo because we rely on the native 0x80..00 overflow
  // behavior. cvt instead of cvtt should be equivalent, but avoids test
  // failure under GCC 10.2.1.
  return Vec128<MakeUnsigned<T>, N>{_mm_cvtps_epi32(_mm_castsi128_ps(f.raw))};
}

}  // namespace detail
#endif  // HWY_TARGET > HWY_AVX3

template <size_t N>
HWY_API Vec128<uint16_t, N> operator<<(const Vec128<uint16_t, N> v,
                                       const Vec128<uint16_t, N> bits) {
#if HWY_TARGET <= HWY_AVX3
  return Vec128<uint16_t, N>{_mm_sllv_epi16(v.raw, bits.raw)};
#else
  return v * detail::Pow2(bits);
#endif
}
HWY_API Vec128<uint16_t, 1> operator<<(const Vec128<uint16_t, 1> v,
                                       const Vec128<uint16_t, 1> bits) {
  return Vec128<uint16_t, 1>{_mm_sll_epi16(v.raw, bits.raw)};
}

template <size_t N>
HWY_API Vec128<uint32_t, N> operator<<(const Vec128<uint32_t, N> v,
                                       const Vec128<uint32_t, N> bits) {
#if HWY_TARGET == HWY_SSSE3 || HWY_TARGET == HWY_SSE4
  return v * detail::Pow2(bits);
#else
  return Vec128<uint32_t, N>{_mm_sllv_epi32(v.raw, bits.raw)};
#endif
}
HWY_API Vec128<uint32_t, 1> operator<<(const Vec128<uint32_t, 1> v,
                                       const Vec128<uint32_t, 1> bits) {
  return Vec128<uint32_t, 1>{_mm_sll_epi32(v.raw, bits.raw)};
}

HWY_API Vec128<uint64_t> operator<<(const Vec128<uint64_t> v,
                                    const Vec128<uint64_t> bits) {
#if HWY_TARGET == HWY_SSSE3 || HWY_TARGET == HWY_SSE4
  // Individual shifts and combine
  const Vec128<uint64_t> out0{_mm_sll_epi64(v.raw, bits.raw)};
  const __m128i bits1 = _mm_unpackhi_epi64(bits.raw, bits.raw);
  const Vec128<uint64_t> out1{_mm_sll_epi64(v.raw, bits1)};
  return ConcatUpperLower(out1, out0);
#else
  return Vec128<uint64_t>{_mm_sllv_epi64(v.raw, bits.raw)};
#endif
}
HWY_API Vec128<uint64_t, 1> operator<<(const Vec128<uint64_t, 1> v,
                                       const Vec128<uint64_t, 1> bits) {
  return Vec128<uint64_t, 1>{_mm_sll_epi64(v.raw, bits.raw)};
}

// Signed left shift is the same as unsigned.
template <typename T, size_t N, HWY_IF_SIGNED(T)>
HWY_API Vec128<T, N> operator<<(const Vec128<T, N> v, const Vec128<T, N> bits) {
  const Simd<T, N> di;
  const Simd<MakeUnsigned<T>, N> du;
  return BitCast(di, BitCast(du, v) << BitCast(du, bits));
}

// ------------------------------ Shr (mul, mask, BroadcastSignBit)

// Use AVX2+ variable shifts except for SSSE3/SSE4 or 16-bit. There, we use
// widening multiplication by powers of two obtained by loading float exponents,
// followed by a constant right-shift. This is still faster than a scalar or
// bit-test approach: https://gcc.godbolt.org/z/9G7Y9v.

template <size_t N>
HWY_API Vec128<uint16_t, N> operator>>(const Vec128<uint16_t, N> in,
                                       const Vec128<uint16_t, N> bits) {
#if HWY_TARGET <= HWY_AVX3
  return Vec128<uint16_t, N>{_mm_srlv_epi16(in.raw, bits.raw)};
#else
  const Simd<uint16_t, N> d;
  // For bits=0, we cannot mul by 2^16, so fix the result later.
  const auto out = MulHigh(in, detail::Pow2(Set(d, 16) - bits));
  // Replace output with input where bits == 0.
  return IfThenElse(bits == Zero(d), in, out);
#endif
}
HWY_API Vec128<uint16_t, 1> operator>>(const Vec128<uint16_t, 1> in,
                                       const Vec128<uint16_t, 1> bits) {
  return Vec128<uint16_t, 1>{_mm_srl_epi16(in.raw, bits.raw)};
}

template <size_t N>
HWY_API Vec128<uint32_t, N> operator>>(const Vec128<uint32_t, N> in,
                                       const Vec128<uint32_t, N> bits) {
#if HWY_TARGET == HWY_SSSE3 || HWY_TARGET == HWY_SSE4
  // 32x32 -> 64 bit mul, then shift right by 32.
  const Simd<uint32_t, N> d32;
  // Move odd lanes into position for the second mul. Shuffle more gracefully
  // handles N=1 than repartitioning to u64 and shifting 32 bits right.
  const Vec128<uint32_t, N> in31{_mm_shuffle_epi32(in.raw, 0x31)};
  // For bits=0, we cannot mul by 2^32, so fix the result later.
  const auto mul = detail::Pow2(Set(d32, 32) - bits);
  const auto out20 = ShiftRight<32>(MulEven(in, mul));  // z 2 z 0
  const Vec128<uint32_t, N> mul31{_mm_shuffle_epi32(mul.raw, 0x31)};
  // No need to shift right, already in the correct position.
  const auto out31 = BitCast(d32, MulEven(in31, mul31));  // 3 ? 1 ?
  const Vec128<uint32_t, N> out = OddEven(out31, BitCast(d32, out20));
  // Replace output with input where bits == 0.
  return IfThenElse(bits == Zero(d32), in, out);
#else
  return Vec128<uint32_t, N>{_mm_srlv_epi32(in.raw, bits.raw)};
#endif
}
HWY_API Vec128<uint32_t, 1> operator>>(const Vec128<uint32_t, 1> in,
                                       const Vec128<uint32_t, 1> bits) {
  return Vec128<uint32_t, 1>{_mm_srl_epi32(in.raw, bits.raw)};
}

HWY_API Vec128<uint64_t> operator>>(const Vec128<uint64_t> v,
                                    const Vec128<uint64_t> bits) {
#if HWY_TARGET == HWY_SSSE3 || HWY_TARGET == HWY_SSE4
  // Individual shifts and combine
  const Vec128<uint64_t> out0{_mm_srl_epi64(v.raw, bits.raw)};
  const __m128i bits1 = _mm_unpackhi_epi64(bits.raw, bits.raw);
  const Vec128<uint64_t> out1{_mm_srl_epi64(v.raw, bits1)};
  return ConcatUpperLower(out1, out0);
#else
  return Vec128<uint64_t>{_mm_srlv_epi64(v.raw, bits.raw)};
#endif
}
HWY_API Vec128<uint64_t, 1> operator>>(const Vec128<uint64_t, 1> v,
                                       const Vec128<uint64_t, 1> bits) {
  return Vec128<uint64_t, 1>{_mm_srl_epi64(v.raw, bits.raw)};
}

#if HWY_TARGET > HWY_AVX3  // AVX2 or older
namespace detail {

// Also used in x86_256-inl.h.
template <class DI, class V>
HWY_INLINE V SignedShr(const DI di, const V v, const V count_i) {
  const RebindToUnsigned<DI> du;
  const auto count = BitCast(du, count_i);  // same type as value to shift
  // Clear sign and restore afterwards. This is preferable to shifting the MSB
  // downwards because Shr is somewhat more expensive than Shl.
  const auto sign = BroadcastSignBit(v);
  const auto abs = BitCast(du, v ^ sign);  // off by one, but fixed below
  return BitCast(di, abs >> count) ^ sign;
}

}  // namespace detail
#endif  // HWY_TARGET > HWY_AVX3

template <size_t N>
HWY_API Vec128<int16_t, N> operator>>(const Vec128<int16_t, N> v,
                                      const Vec128<int16_t, N> bits) {
#if HWY_TARGET <= HWY_AVX3
  return Vec128<int16_t, N>{_mm_srav_epi16(v.raw, bits.raw)};
#else
  return detail::SignedShr(Simd<int16_t, N>(), v, bits);
#endif
}
HWY_API Vec128<int16_t, 1> operator>>(const Vec128<int16_t, 1> v,
                                      const Vec128<int16_t, 1> bits) {
  return Vec128<int16_t, 1>{_mm_sra_epi16(v.raw, bits.raw)};
}

template <size_t N>
HWY_API Vec128<int32_t, N> operator>>(const Vec128<int32_t, N> v,
                                      const Vec128<int32_t, N> bits) {
#if HWY_TARGET <= HWY_AVX3
  return Vec128<int32_t, N>{_mm_srav_epi32(v.raw, bits.raw)};
#else
  return detail::SignedShr(Simd<int32_t, N>(), v, bits);
#endif
}
HWY_API Vec128<int32_t, 1> operator>>(const Vec128<int32_t, 1> v,
                                      const Vec128<int32_t, 1> bits) {
  return Vec128<int32_t, 1>{_mm_sra_epi32(v.raw, bits.raw)};
}

template <size_t N>
HWY_API Vec128<int64_t, N> operator>>(const Vec128<int64_t, N> v,
                                      const Vec128<int64_t, N> bits) {
#if HWY_TARGET <= HWY_AVX3
  return Vec128<int64_t, N>{_mm_srav_epi64(v.raw, bits.raw)};
#else
  return detail::SignedShr(Simd<int64_t, N>(), v, bits);
#endif
}

// ------------------------------ MulEven/Odd 64x64 (UpperHalf)

HWY_INLINE Vec128<uint64_t> MulEven(const Vec128<uint64_t> a,
                                    const Vec128<uint64_t> b) {
  alignas(16) uint64_t mul[2];
  mul[0] = Mul128(GetLane(a), GetLane(b), &mul[1]);
  return Load(Full128<uint64_t>(), mul);
}

HWY_INLINE Vec128<uint64_t> MulOdd(const Vec128<uint64_t> a,
                                   const Vec128<uint64_t> b) {
  alignas(16) uint64_t mul[2];
  mul[0] = Mul128(GetLane(UpperHalf(a)), GetLane(UpperHalf(b)), &mul[1]);
  return Load(Full128<uint64_t>(), mul);
}

// ================================================== CONVERT

// ------------------------------ Promotions (part w/ narrow lanes -> full)

// Unsigned: zero-extend.
template <size_t N>
HWY_API Vec128<uint16_t, N> PromoteTo(Simd<uint16_t, N> /* tag */,
                                      const Vec128<uint8_t, N> v) {
#if HWY_TARGET == HWY_SSSE3
  const __m128i zero = _mm_setzero_si128();
  return Vec128<uint16_t, N>{_mm_unpacklo_epi8(v.raw, zero)};
#else
  return Vec128<uint16_t, N>{_mm_cvtepu8_epi16(v.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<uint32_t, N> PromoteTo(Simd<uint32_t, N> /* tag */,
                                      const Vec128<uint16_t, N> v) {
#if HWY_TARGET == HWY_SSSE3
  return Vec128<uint32_t, N>{_mm_unpacklo_epi16(v.raw, _mm_setzero_si128())};
#else
  return Vec128<uint32_t, N>{_mm_cvtepu16_epi32(v.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<uint64_t, N> PromoteTo(Simd<uint64_t, N> /* tag */,
                                      const Vec128<uint32_t, N> v) {
#if HWY_TARGET == HWY_SSSE3
  return Vec128<uint64_t, N>{_mm_unpacklo_epi32(v.raw, _mm_setzero_si128())};
#else
  return Vec128<uint64_t, N>{_mm_cvtepu32_epi64(v.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<uint32_t, N> PromoteTo(Simd<uint32_t, N> /* tag */,
                                      const Vec128<uint8_t, N> v) {
#if HWY_TARGET == HWY_SSSE3
  const __m128i zero = _mm_setzero_si128();
  const __m128i u16 = _mm_unpacklo_epi8(v.raw, zero);
  return Vec128<uint32_t, N>{_mm_unpacklo_epi16(u16, zero)};
#else
  return Vec128<uint32_t, N>{_mm_cvtepu8_epi32(v.raw)};
#endif
}

// Unsigned to signed: same plus cast.
template <size_t N>
HWY_API Vec128<int16_t, N> PromoteTo(Simd<int16_t, N> di,
                                     const Vec128<uint8_t, N> v) {
  return BitCast(di, PromoteTo(Simd<uint16_t, N>(), v));
}
template <size_t N>
HWY_API Vec128<int32_t, N> PromoteTo(Simd<int32_t, N> di,
                                     const Vec128<uint16_t, N> v) {
  return BitCast(di, PromoteTo(Simd<uint32_t, N>(), v));
}
template <size_t N>
HWY_API Vec128<int32_t, N> PromoteTo(Simd<int32_t, N> di,
                                     const Vec128<uint8_t, N> v) {
  return BitCast(di, PromoteTo(Simd<uint32_t, N>(), v));
}

// Signed: replicate sign bit.
template <size_t N>
HWY_API Vec128<int16_t, N> PromoteTo(Simd<int16_t, N> /* tag */,
                                     const Vec128<int8_t, N> v) {
#if HWY_TARGET == HWY_SSSE3
  return ShiftRight<8>(Vec128<int16_t, N>{_mm_unpacklo_epi8(v.raw, v.raw)});
#else
  return Vec128<int16_t, N>{_mm_cvtepi8_epi16(v.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<int32_t, N> PromoteTo(Simd<int32_t, N> /* tag */,
                                     const Vec128<int16_t, N> v) {
#if HWY_TARGET == HWY_SSSE3
  return ShiftRight<16>(Vec128<int32_t, N>{_mm_unpacklo_epi16(v.raw, v.raw)});
#else
  return Vec128<int32_t, N>{_mm_cvtepi16_epi32(v.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<int64_t, N> PromoteTo(Simd<int64_t, N> /* tag */,
                                     const Vec128<int32_t, N> v) {
#if HWY_TARGET == HWY_SSSE3
  return ShiftRight<32>(Vec128<int64_t, N>{_mm_unpacklo_epi32(v.raw, v.raw)});
#else
  return Vec128<int64_t, N>{_mm_cvtepi32_epi64(v.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<int32_t, N> PromoteTo(Simd<int32_t, N> /* tag */,
                                     const Vec128<int8_t, N> v) {
#if HWY_TARGET == HWY_SSSE3
  const __m128i x2 = _mm_unpacklo_epi8(v.raw, v.raw);
  const __m128i x4 = _mm_unpacklo_epi16(x2, x2);
  return ShiftRight<24>(Vec128<int32_t, N>{x4});
#else
  return Vec128<int32_t, N>{_mm_cvtepi8_epi32(v.raw)};
#endif
}

// Workaround for origin tracking bug in Clang msan prior to 11.0
// (spurious "uninitialized memory" for TestF16 with "ORIGIN: invalid")
#if defined(MEMORY_SANITIZER) && \
    (HWY_COMPILER_CLANG != 0 && HWY_COMPILER_CLANG < 1100)
#define HWY_INLINE_F16 HWY_NOINLINE
#else
#define HWY_INLINE_F16 HWY_INLINE
#endif
template <size_t N>
HWY_INLINE_F16 Vec128<float, N> PromoteTo(Simd<float, N> /* tag */,
                                          const Vec128<float16_t, N> v) {
#if HWY_TARGET == HWY_SSSE3 || HWY_TARGET == HWY_SSE4
  const Simd<int32_t, N> di32;
  const Simd<uint32_t, N> du32;
  const Simd<float, N> df32;
  // Expand to u32 so we can shift.
  const auto bits16 = PromoteTo(du32, Vec128<uint16_t, N>{v.raw});
  const auto sign = ShiftRight<15>(bits16);
  const auto biased_exp = ShiftRight<10>(bits16) & Set(du32, 0x1F);
  const auto mantissa = bits16 & Set(du32, 0x3FF);
  const auto subnormal =
      BitCast(du32, ConvertTo(df32, BitCast(di32, mantissa)) *
                        Set(df32, 1.0f / 16384 / 1024));

  const auto biased_exp32 = biased_exp + Set(du32, 127 - 15);
  const auto mantissa32 = ShiftLeft<23 - 10>(mantissa);
  const auto normal = ShiftLeft<23>(biased_exp32) | mantissa32;
  const auto bits32 = IfThenElse(biased_exp == Zero(du32), subnormal, normal);
  return BitCast(df32, ShiftLeft<31>(sign) | bits32);
#else
  return Vec128<float, N>{_mm_cvtph_ps(v.raw)};
#endif
}

template <size_t N>
HWY_API Vec128<double, N> PromoteTo(Simd<double, N> /* tag */,
                                    const Vec128<float, N> v) {
  return Vec128<double, N>{_mm_cvtps_pd(v.raw)};
}

template <size_t N>
HWY_API Vec128<double, N> PromoteTo(Simd<double, N> /* tag */,
                                    const Vec128<int32_t, N> v) {
  return Vec128<double, N>{_mm_cvtepi32_pd(v.raw)};
}

// ------------------------------ Demotions (full -> part w/ narrow lanes)

template <size_t N>
HWY_API Vec128<uint16_t, N> DemoteTo(Simd<uint16_t, N> /* tag */,
                                     const Vec128<int32_t, N> v) {
#if HWY_TARGET == HWY_SSSE3
  const Simd<int32_t, N> di32;
  const Simd<uint16_t, N * 2> du16;
  const auto zero_if_neg = AndNot(ShiftRight<31>(v), v);
  const auto too_big = VecFromMask(di32, Gt(v, Set(di32, 0xFFFF)));
  const auto clamped = Or(zero_if_neg, too_big);
  // Lower 2 bytes from each 32-bit lane; same as return type for fewer casts.
  alignas(16) constexpr uint16_t kLower2Bytes[16] = {
      0x0100, 0x0504, 0x0908, 0x0D0C, 0x8080, 0x8080, 0x8080, 0x8080};
  const auto lo2 = Load(du16, kLower2Bytes);
  return Vec128<uint16_t, N>{TableLookupBytes(BitCast(du16, clamped), lo2).raw};
#else
  return Vec128<uint16_t, N>{_mm_packus_epi32(v.raw, v.raw)};
#endif
}

template <size_t N>
HWY_API Vec128<int16_t, N> DemoteTo(Simd<int16_t, N> /* tag */,
                                    const Vec128<int32_t, N> v) {
  return Vec128<int16_t, N>{_mm_packs_epi32(v.raw, v.raw)};
}

template <size_t N>
HWY_API Vec128<uint8_t, N> DemoteTo(Simd<uint8_t, N> /* tag */,
                                    const Vec128<int32_t, N> v) {
  const __m128i i16 = _mm_packs_epi32(v.raw, v.raw);
  return Vec128<uint8_t, N>{_mm_packus_epi16(i16, i16)};
}

template <size_t N>
HWY_API Vec128<uint8_t, N> DemoteTo(Simd<uint8_t, N> /* tag */,
                                    const Vec128<int16_t, N> v) {
  return Vec128<uint8_t, N>{_mm_packus_epi16(v.raw, v.raw)};
}

template <size_t N>
HWY_API Vec128<int8_t, N> DemoteTo(Simd<int8_t, N> /* tag */,
                                   const Vec128<int32_t, N> v) {
  const __m128i i16 = _mm_packs_epi32(v.raw, v.raw);
  return Vec128<int8_t, N>{_mm_packs_epi16(i16, i16)};
}

template <size_t N>
HWY_API Vec128<int8_t, N> DemoteTo(Simd<int8_t, N> /* tag */,
                                   const Vec128<int16_t, N> v) {
  return Vec128<int8_t, N>{_mm_packs_epi16(v.raw, v.raw)};
}

template <size_t N>
HWY_API Vec128<float16_t, N> DemoteTo(Simd<float16_t, N> /* tag */,
                                      const Vec128<float, N> v) {
#if HWY_TARGET == HWY_SSSE3 || HWY_TARGET == HWY_SSE4
  const Simd<int32_t, N> di;
  const Simd<uint32_t, N> du;
  const Simd<uint16_t, N> du16;
  const Simd<float16_t, N> df16;
  const auto bits32 = BitCast(du, v);
  const auto sign = ShiftRight<31>(bits32);
  const auto biased_exp32 = ShiftRight<23>(bits32) & Set(du, 0xFF);
  const auto mantissa32 = bits32 & Set(du, 0x7FFFFF);

  const auto k15 = Set(di, 15);
  const auto exp = Min(BitCast(di, biased_exp32) - Set(di, 127), k15);
  const auto is_tiny = exp < Set(di, -24);

  const auto is_subnormal = exp < Set(di, -14);
  const auto biased_exp16 =
      BitCast(du, IfThenZeroElse(is_subnormal, exp + k15));
  const auto sub_exp = BitCast(du, Set(di, -14) - exp);  // [1, 11)
  const auto sub_m = (Set(du, 1) << (Set(du, 10) - sub_exp)) +
                     (mantissa32 >> (Set(du, 13) + sub_exp));
  const auto mantissa16 = IfThenElse(RebindMask(du, is_subnormal), sub_m,
                                     ShiftRight<13>(mantissa32));  // <1024

  const auto sign16 = ShiftLeft<15>(sign);
  const auto normal16 = sign16 | ShiftLeft<10>(biased_exp16) | mantissa16;
  const auto bits16 = IfThenZeroElse(is_tiny, BitCast(di, normal16));
  return BitCast(df16, DemoteTo(du16, bits16));
#else
  return Vec128<float16_t, N>{_mm_cvtps_ph(v.raw, _MM_FROUND_NO_EXC)};
#endif
}

template <size_t N>
HWY_API Vec128<float, N> DemoteTo(Simd<float, N> /* tag */,
                                  const Vec128<double, N> v) {
  return Vec128<float, N>{_mm_cvtpd_ps(v.raw)};
}

namespace detail {

// For well-defined float->int demotion in all x86_*-inl.h.

template <size_t N>
HWY_INLINE auto ClampF64ToI32Max(Simd<double, N> d, decltype(Zero(d)) v)
    -> decltype(Zero(d)) {
  // The max can be exactly represented in binary64, so clamping beforehand
  // prevents x86 conversion from raising an exception and returning 80..00.
  return Min(v, Set(d, 2147483647.0));
}

// For ConvertTo float->int of same size, clamping before conversion would
// change the result because the max integer value is not exactly representable.
// Instead detect the overflow result after conversion and fix it.
template <typename TI, size_t N, class DF = Simd<MakeFloat<TI>, N>>
HWY_INLINE auto FixConversionOverflow(Simd<TI, N> di,
                                      decltype(Zero(DF())) original,
                                      decltype(Zero(di).raw) converted_raw)
    -> decltype(Zero(di)) {
  // Combinations of original and output sign:
  //   --: normal <0 or -huge_val to 80..00: OK
  //   -+: -0 to 0                         : OK
  //   +-: +huge_val to 80..00             : xor with FF..FF to get 7F..FF
  //   ++: normal >0                       : OK
  const auto converted = decltype(Zero(di)){converted_raw};
  const auto sign_wrong = AndNot(BitCast(di, original), converted);
  return BitCast(di, Xor(converted, BroadcastSignBit(sign_wrong)));
}

}  // namespace detail

template <size_t N>
HWY_API Vec128<int32_t, N> DemoteTo(Simd<int32_t, N> /* tag */,
                                    const Vec128<double, N> v) {
  const auto clamped = detail::ClampF64ToI32Max(Simd<double, N>(), v);
  return Vec128<int32_t, N>{_mm_cvttpd_epi32(clamped.raw)};
}

// For already range-limited input [0, 255].
template <size_t N>
HWY_API Vec128<uint8_t, N> U8FromU32(const Vec128<uint32_t, N> v) {
  const Simd<uint32_t, N> d32;
  const Simd<uint8_t, N * 4> d8;
  alignas(16) static constexpr uint32_t k8From32[4] = {
      0x0C080400u, 0x0C080400u, 0x0C080400u, 0x0C080400u};
  // Also replicate bytes into all 32 bit lanes for safety.
  const auto quad = TableLookupBytes(v, Load(d32, k8From32));
  return LowerHalf(LowerHalf(BitCast(d8, quad)));
}

// ------------------------------ Integer <=> fp (ShiftRight, OddEven)

template <size_t N>
HWY_API Vec128<float, N> ConvertTo(Simd<float, N> /* tag */,
                                   const Vec128<int32_t, N> v) {
  return Vec128<float, N>{_mm_cvtepi32_ps(v.raw)};
}

template <size_t N>
HWY_API Vec128<double, N> ConvertTo(Simd<double, N> dd,
                                    const Vec128<int64_t, N> v) {
#if HWY_TARGET <= HWY_AVX3
  (void)dd;
  return Vec128<double, N>{_mm_cvtepi64_pd(v.raw)};
#else
  // Based on wim's approach (https://stackoverflow.com/questions/41144668/)
  const Repartition<uint32_t, decltype(dd)> d32;
  const Repartition<uint64_t, decltype(dd)> d64;

  // Toggle MSB of lower 32-bits and insert exponent for 2^84 + 2^63
  const auto k84_63 = Set(d64, 0x4530000080000000ULL);
  const auto v_upper = BitCast(dd, ShiftRight<32>(BitCast(d64, v)) ^ k84_63);

  // Exponent is 2^52, lower 32 bits from v (=> 32-bit OddEven)
  const auto k52 = Set(d32, 0x43300000);
  const auto v_lower = BitCast(dd, OddEven(k52, BitCast(d32, v)));

  const auto k84_63_52 = BitCast(dd, Set(d64, 0x4530000080100000ULL));
  return (v_upper - k84_63_52) + v_lower;  // order matters!
#endif
}

// Truncates (rounds toward zero).
template <size_t N>
HWY_API Vec128<int32_t, N> ConvertTo(const Simd<int32_t, N> di,
                                     const Vec128<float, N> v) {
  return detail::FixConversionOverflow(di, v, _mm_cvttps_epi32(v.raw));
}

template <size_t N>
HWY_API Vec128<int64_t, N> ConvertTo(Simd<int64_t, N> di,
                                     const Vec128<double, N> v) {
#if HWY_TARGET <= HWY_AVX3 && HWY_ARCH_X86_64
  return detail::FixConversionOverflow(di, v, _mm_cvttpd_epi64(v.raw));
#elif HWY_ARCH_X86_64
  const __m128i i0 = _mm_cvtsi64_si128(_mm_cvttsd_si64(v.raw));
  const __m128i i1 = _mm_cvtsi64_si128(_mm_cvttsd_si64(UpperHalf(v).raw));
  return detail::FixConversionOverflow(di, v, _mm_unpacklo_epi64(i0, i1));
#else
  using VI = decltype(Zero(di));
  const VI k0 = Zero(di);
  const VI k1 = Set(di, 1);
  const VI k51 = Set(di, 51);

  // Exponent indicates whether the number can be represented as int64_t.
  const VI biased_exp = ShiftRight<52>(BitCast(di, v)) & Set(di, 0x7FF);
  const VI exp = biased_exp - Set(di, 0x3FF);
  const auto in_range = exp < Set(di, 63);

  // If we were to cap the exponent at 51 and add 2^52, the number would be in
  // [2^52, 2^53) and mantissa bits could be read out directly. We need to
  // round-to-0 (truncate), but changing rounding mode in MXCSR hits a
  // compiler reordering bug: https://gcc.godbolt.org/z/4hKj6c6qc . We instead
  // manually shift the mantissa into place (we already have many of the
  // inputs anyway).
  const VI shift_mnt = Max(k51 - exp, k0);
  const VI shift_int = Max(exp - k51, k0);
  const VI mantissa = BitCast(di, v) & Set(di, (1ULL << 52) - 1);
  // Include implicit 1-bit; shift by one more to ensure it's in the mantissa.
  const VI int52 = (mantissa | Set(di, 1ULL << 52)) >> (shift_mnt + k1);
  // For inputs larger than 2^52, insert zeros at the bottom.
  const VI shifted = int52 << shift_int;
  // Restore the one bit lost when shifting in the implicit 1-bit.
  const VI restored = shifted | ((mantissa & k1) << (shift_int - k1));

  // Saturate to LimitsMin (unchanged when negating below) or LimitsMax.
  const VI sign_mask = BroadcastSignBit(BitCast(di, v));
  const VI limit = Set(di, LimitsMax<int64_t>()) - sign_mask;
  const VI magnitude = IfThenElse(in_range, restored, limit);

  // If the input was negative, negate the integer (two's complement).
  return (magnitude ^ sign_mask) - sign_mask;
#endif
}
HWY_API Vec128<int64_t, 1> ConvertTo(Simd<int64_t, 1> di,
                                     const Vec128<double, 1> v) {
  // Only need to specialize for non-AVX3, 64-bit (single scalar op)
#if HWY_TARGET > HWY_AVX3 && HWY_ARCH_X86_64
  const Vec128<int64_t, 1> i0{_mm_cvtsi64_si128(_mm_cvttsd_si64(v.raw))};
  return detail::FixConversionOverflow(di, v, i0.raw);
#else
  (void)di;
  const auto full = ConvertTo(Full128<int64_t>(), Vec128<double>{v.raw});
  return Vec128<int64_t, 1>{full.raw};
#endif
}

template <size_t N>
HWY_API Vec128<int32_t, N> NearestInt(const Vec128<float, N> v) {
  const Simd<int32_t, N> di;
  return detail::FixConversionOverflow(di, v, _mm_cvtps_epi32(v.raw));
}

// ------------------------------ Floating-point rounding (ConvertTo)

#if HWY_TARGET == HWY_SSSE3

// Toward nearest integer, ties to even
template <typename T, size_t N, HWY_IF_FLOAT(T)>
HWY_API Vec128<T, N> Round(const Vec128<T, N> v) {
  // Rely on rounding after addition with a large value such that no mantissa
  // bits remain (assuming the current mode is nearest-even). We may need a
  // compiler flag for precise floating-point to prevent "optimizing" this out.
  const Simd<T, N> df;
  const auto max = Set(df, MantissaEnd<T>());
  const auto large = CopySignToAbs(max, v);
  const auto added = large + v;
  const auto rounded = added - large;
  // Keep original if NaN or the magnitude is large (already an int).
  return IfThenElse(Abs(v) < max, rounded, v);
}

namespace detail {

// Truncating to integer and converting back to float is correct except when the
// input magnitude is large, in which case the input was already an integer
// (because mantissa >> exponent is zero).
template <typename T, size_t N, HWY_IF_FLOAT(T)>
HWY_INLINE Mask128<T, N> UseInt(const Vec128<T, N> v) {
  return Abs(v) < Set(Simd<T, N>(), MantissaEnd<T>());
}

}  // namespace detail

// Toward zero, aka truncate
template <typename T, size_t N, HWY_IF_FLOAT(T)>
HWY_API Vec128<T, N> Trunc(const Vec128<T, N> v) {
  const Simd<T, N> df;
  const RebindToSigned<decltype(df)> di;

  const auto integer = ConvertTo(di, v);  // round toward 0
  const auto int_f = ConvertTo(df, integer);

  return IfThenElse(detail::UseInt(v), CopySign(int_f, v), v);
}

// Toward +infinity, aka ceiling
template <typename T, size_t N, HWY_IF_FLOAT(T)>
HWY_API Vec128<T, N> Ceil(const Vec128<T, N> v) {
  const Simd<T, N> df;
  const RebindToSigned<decltype(df)> di;

  const auto integer = ConvertTo(di, v);  // round toward 0
  const auto int_f = ConvertTo(df, integer);

  // Truncating a positive non-integer ends up smaller; if so, add 1.
  const auto neg1 = ConvertTo(df, VecFromMask(di, RebindMask(di, int_f < v)));

  return IfThenElse(detail::UseInt(v), int_f - neg1, v);
}

// Toward -infinity, aka floor
template <typename T, size_t N, HWY_IF_FLOAT(T)>
HWY_API Vec128<T, N> Floor(const Vec128<T, N> v) {
  const Simd<T, N> df;
  const RebindToSigned<decltype(df)> di;

  const auto integer = ConvertTo(di, v);  // round toward 0
  const auto int_f = ConvertTo(df, integer);

  // Truncating a negative non-integer ends up larger; if so, subtract 1.
  const auto neg1 = ConvertTo(df, VecFromMask(di, RebindMask(di, int_f > v)));

  return IfThenElse(detail::UseInt(v), int_f + neg1, v);
}

#else

// Toward nearest integer, ties to even
template <size_t N>
HWY_API Vec128<float, N> Round(const Vec128<float, N> v) {
  return Vec128<float, N>{
      _mm_round_ps(v.raw, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC)};
}
template <size_t N>
HWY_API Vec128<double, N> Round(const Vec128<double, N> v) {
  return Vec128<double, N>{
      _mm_round_pd(v.raw, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC)};
}

// Toward zero, aka truncate
template <size_t N>
HWY_API Vec128<float, N> Trunc(const Vec128<float, N> v) {
  return Vec128<float, N>{
      _mm_round_ps(v.raw, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC)};
}
template <size_t N>
HWY_API Vec128<double, N> Trunc(const Vec128<double, N> v) {
  return Vec128<double, N>{
      _mm_round_pd(v.raw, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC)};
}

// Toward +infinity, aka ceiling
template <size_t N>
HWY_API Vec128<float, N> Ceil(const Vec128<float, N> v) {
  return Vec128<float, N>{
      _mm_round_ps(v.raw, _MM_FROUND_TO_POS_INF | _MM_FROUND_NO_EXC)};
}
template <size_t N>
HWY_API Vec128<double, N> Ceil(const Vec128<double, N> v) {
  return Vec128<double, N>{
      _mm_round_pd(v.raw, _MM_FROUND_TO_POS_INF | _MM_FROUND_NO_EXC)};
}

// Toward -infinity, aka floor
template <size_t N>
HWY_API Vec128<float, N> Floor(const Vec128<float, N> v) {
  return Vec128<float, N>{
      _mm_round_ps(v.raw, _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC)};
}
template <size_t N>
HWY_API Vec128<double, N> Floor(const Vec128<double, N> v) {
  return Vec128<double, N>{
      _mm_round_pd(v.raw, _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC)};
}

#endif  // !HWY_SSSE3

// ================================================== CRYPTO

#if !defined(HWY_DISABLE_PCLMUL_AES) && HWY_TARGET != HWY_SSSE3

// Per-target flag to prevent generic_ops-inl.h from defining AESRound.
#ifdef HWY_NATIVE_AES
#undef HWY_NATIVE_AES
#else
#define HWY_NATIVE_AES
#endif

HWY_API Vec128<uint8_t> AESRound(Vec128<uint8_t> state,
                                 Vec128<uint8_t> round_key) {
  return Vec128<uint8_t>{_mm_aesenc_si128(state.raw, round_key.raw)};
}

template <size_t N, HWY_IF_LE128(uint64_t, N)>
HWY_API Vec128<uint64_t, N> CLMulLower(Vec128<uint64_t, N> a,
                                       Vec128<uint64_t, N> b) {
  return Vec128<uint64_t, N>{_mm_clmulepi64_si128(a.raw, b.raw, 0x00)};
}

template <size_t N, HWY_IF_LE128(uint64_t, N)>
HWY_API Vec128<uint64_t, N> CLMulUpper(Vec128<uint64_t, N> a,
                                       Vec128<uint64_t, N> b) {
  return Vec128<uint64_t, N>{_mm_clmulepi64_si128(a.raw, b.raw, 0x11)};
}

#endif  // !defined(HWY_DISABLE_PCLMUL_AES) && HWY_TARGET != HWY_SSSE3

// ================================================== MISC

// Returns a vector with lane i=[0, N) set to "first" + i.
template <typename T, size_t N, typename T2, HWY_IF_LE128(T, N)>
HWY_API Vec128<T, N> Iota(const Simd<T, N> d, const T2 first) {
  HWY_ALIGN T lanes[16 / sizeof(T)];
  for (size_t i = 0; i < 16 / sizeof(T); ++i) {
    lanes[i] = static_cast<T>(first + static_cast<T2>(i));
  }
  return Load(d, lanes);
}

// ------------------------------ Mask

namespace detail {

constexpr HWY_INLINE uint64_t U64FromInt(int bits) {
  return static_cast<uint64_t>(static_cast<unsigned>(bits));
}

template <typename T, size_t N>
HWY_INLINE uint64_t BitsFromMask(hwy::SizeTag<1> /*tag*/,
                                 const Mask128<T, N> mask) {
  const Simd<T, N> d;
  const auto sign_bits = BitCast(d, VecFromMask(d, mask)).raw;
  return U64FromInt(_mm_movemask_epi8(sign_bits));
}

template <typename T, size_t N>
HWY_INLINE uint64_t BitsFromMask(hwy::SizeTag<2> /*tag*/,
                                 const Mask128<T, N> mask) {
  // Remove useless lower half of each u16 while preserving the sign bit.
  const auto sign_bits = _mm_packs_epi16(mask.raw, _mm_setzero_si128());
  return U64FromInt(_mm_movemask_epi8(sign_bits));
}

template <typename T, size_t N>
HWY_INLINE uint64_t BitsFromMask(hwy::SizeTag<4> /*tag*/,
                                 const Mask128<T, N> mask) {
  const Simd<T, N> d;
  const Simd<float, N> df;
  const auto sign_bits = BitCast(df, VecFromMask(d, mask));
  return U64FromInt(_mm_movemask_ps(sign_bits.raw));
}

template <typename T, size_t N>
HWY_INLINE uint64_t BitsFromMask(hwy::SizeTag<8> /*tag*/,
                                 const Mask128<T, N> mask) {
  const Simd<T, N> d;
  const Simd<double, N> df;
  const auto sign_bits = BitCast(df, VecFromMask(d, mask));
  return U64FromInt(_mm_movemask_pd(sign_bits.raw));
}

// Returns the lowest N of the _mm_movemask* bits.
template <typename T, size_t N>
constexpr uint64_t OnlyActive(uint64_t bits) {
  return ((N * sizeof(T)) == 16) ? bits : bits & ((1ull << N) - 1);
}

template <typename T, size_t N>
HWY_INLINE uint64_t BitsFromMask(const Mask128<T, N> mask) {
  return OnlyActive<T, N>(BitsFromMask(hwy::SizeTag<sizeof(T)>(), mask));
}

}  // namespace detail

template <typename T, size_t N>
HWY_API size_t StoreMaskBits(const Simd<T, N> /* tag */,
                             const Mask128<T, N> mask, uint8_t* p) {
  const uint64_t bits = detail::BitsFromMask(mask);
  const size_t kNumBytes = (N + 7)/8;
  CopyBytes<kNumBytes>(&bits, p);
  return kNumBytes;
}

template <typename T, size_t N>
HWY_API bool AllFalse(const Mask128<T, N> mask) {
  // Cheaper than PTEST, which is 2 uop / 3L.
  return detail::BitsFromMask(mask) == 0;
}

template <typename T, size_t N>
HWY_API bool AllTrue(const Simd<T, N> /* tag */, const Mask128<T, N> mask) {
  constexpr uint64_t kAllBits =
      detail::OnlyActive<T, N>((1ull << (16 / sizeof(T))) - 1);
  return detail::BitsFromMask(mask) == kAllBits;
}

template <typename T, size_t N>
HWY_API size_t CountTrue(const Simd<T, N> /* tag */, const Mask128<T, N> mask) {
  return PopCount(detail::BitsFromMask(mask));
}

// ------------------------------ Compress

namespace detail {

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> Idx16x8FromBits(const uint64_t mask_bits) {
  HWY_DASSERT(mask_bits < 256);
  const Simd<T, N> d;
  const Rebind<uint8_t, decltype(d)> d8;
  const Simd<uint16_t, N> du;

  // compress_epi16 requires VBMI2 and there is no permutevar_epi16, so we need
  // byte indices for PSHUFB (one vector's worth for each of 256 combinations of
  // 8 mask bits). Loading them directly would require 4 KiB. We can instead
  // store lane indices and convert to byte indices (2*lane + 0..1), with the
  // doubling baked into the table. AVX2 Compress32 stores eight 4-bit lane
  // indices (total 1 KiB), broadcasts them into each 32-bit lane and shifts.
  // Here, 16-bit lanes are too narrow to hold all bits, and unpacking nibbles
  // is likely more costly than the higher cache footprint from storing bytes.
  alignas(16) constexpr uint8_t table[256 * 8] = {
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  2,  0,
      0,  0,  0,  0,  0,  0,  0,  2,  0,  0,  0,  0,  0,  0,  4,  0,  0,  0,
      0,  0,  0,  0,  0,  4,  0,  0,  0,  0,  0,  0,  2,  4,  0,  0,  0,  0,
      0,  0,  0,  2,  4,  0,  0,  0,  0,  0,  6,  0,  0,  0,  0,  0,  0,  0,
      0,  6,  0,  0,  0,  0,  0,  0,  2,  6,  0,  0,  0,  0,  0,  0,  0,  2,
      6,  0,  0,  0,  0,  0,  4,  6,  0,  0,  0,  0,  0,  0,  0,  4,  6,  0,
      0,  0,  0,  0,  2,  4,  6,  0,  0,  0,  0,  0,  0,  2,  4,  6,  0,  0,
      0,  0,  8,  0,  0,  0,  0,  0,  0,  0,  0,  8,  0,  0,  0,  0,  0,  0,
      2,  8,  0,  0,  0,  0,  0,  0,  0,  2,  8,  0,  0,  0,  0,  0,  4,  8,
      0,  0,  0,  0,  0,  0,  0,  4,  8,  0,  0,  0,  0,  0,  2,  4,  8,  0,
      0,  0,  0,  0,  0,  2,  4,  8,  0,  0,  0,  0,  6,  8,  0,  0,  0,  0,
      0,  0,  0,  6,  8,  0,  0,  0,  0,  0,  2,  6,  8,  0,  0,  0,  0,  0,
      0,  2,  6,  8,  0,  0,  0,  0,  4,  6,  8,  0,  0,  0,  0,  0,  0,  4,
      6,  8,  0,  0,  0,  0,  2,  4,  6,  8,  0,  0,  0,  0,  0,  2,  4,  6,
      8,  0,  0,  0,  10, 0,  0,  0,  0,  0,  0,  0,  0,  10, 0,  0,  0,  0,
      0,  0,  2,  10, 0,  0,  0,  0,  0,  0,  0,  2,  10, 0,  0,  0,  0,  0,
      4,  10, 0,  0,  0,  0,  0,  0,  0,  4,  10, 0,  0,  0,  0,  0,  2,  4,
      10, 0,  0,  0,  0,  0,  0,  2,  4,  10, 0,  0,  0,  0,  6,  10, 0,  0,
      0,  0,  0,  0,  0,  6,  10, 0,  0,  0,  0,  0,  2,  6,  10, 0,  0,  0,
      0,  0,  0,  2,  6,  10, 0,  0,  0,  0,  4,  6,  10, 0,  0,  0,  0,  0,
      0,  4,  6,  10, 0,  0,  0,  0,  2,  4,  6,  10, 0,  0,  0,  0,  0,  2,
      4,  6,  10, 0,  0,  0,  8,  10, 0,  0,  0,  0,  0,  0,  0,  8,  10, 0,
      0,  0,  0,  0,  2,  8,  10, 0,  0,  0,  0,  0,  0,  2,  8,  10, 0,  0,
      0,  0,  4,  8,  10, 0,  0,  0,  0,  0,  0,  4,  8,  10, 0,  0,  0,  0,
      2,  4,  8,  10, 0,  0,  0,  0,  0,  2,  4,  8,  10, 0,  0,  0,  6,  8,
      10, 0,  0,  0,  0,  0,  0,  6,  8,  10, 0,  0,  0,  0,  2,  6,  8,  10,
      0,  0,  0,  0,  0,  2,  6,  8,  10, 0,  0,  0,  4,  6,  8,  10, 0,  0,
      0,  0,  0,  4,  6,  8,  10, 0,  0,  0,  2,  4,  6,  8,  10, 0,  0,  0,
      0,  2,  4,  6,  8,  10, 0,  0,  12, 0,  0,  0,  0,  0,  0,  0,  0,  12,
      0,  0,  0,  0,  0,  0,  2,  12, 0,  0,  0,  0,  0,  0,  0,  2,  12, 0,
      0,  0,  0,  0,  4,  12, 0,  0,  0,  0,  0,  0,  0,  4,  12, 0,  0,  0,
      0,  0,  2,  4,  12, 0,  0,  0,  0,  0,  0,  2,  4,  12, 0,  0,  0,  0,
      6,  12, 0,  0,  0,  0,  0,  0,  0,  6,  12, 0,  0,  0,  0,  0,  2,  6,
      12, 0,  0,  0,  0,  0,  0,  2,  6,  12, 0,  0,  0,  0,  4,  6,  12, 0,
      0,  0,  0,  0,  0,  4,  6,  12, 0,  0,  0,  0,  2,  4,  6,  12, 0,  0,
      0,  0,  0,  2,  4,  6,  12, 0,  0,  0,  8,  12, 0,  0,  0,  0,  0,  0,
      0,  8,  12, 0,  0,  0,  0,  0,  2,  8,  12, 0,  0,  0,  0,  0,  0,  2,
      8,  12, 0,  0,  0,  0,  4,  8,  12, 0,  0,  0,  0,  0,  0,  4,  8,  12,
      0,  0,  0,  0,  2,  4,  8,  12, 0,  0,  0,  0,  0,  2,  4,  8,  12, 0,
      0,  0,  6,  8,  12, 0,  0,  0,  0,  0,  0,  6,  8,  12, 0,  0,  0,  0,
      2,  6,  8,  12, 0,  0,  0,  0,  0,  2,  6,  8,  12, 0,  0,  0,  4,  6,
      8,  12, 0,  0,  0,  0,  0,  4,  6,  8,  12, 0,  0,  0,  2,  4,  6,  8,
      12, 0,  0,  0,  0,  2,  4,  6,  8,  12, 0,  0,  10, 12, 0,  0,  0,  0,
      0,  0,  0,  10, 12, 0,  0,  0,  0,  0,  2,  10, 12, 0,  0,  0,  0,  0,
      0,  2,  10, 12, 0,  0,  0,  0,  4,  10, 12, 0,  0,  0,  0,  0,  0,  4,
      10, 12, 0,  0,  0,  0,  2,  4,  10, 12, 0,  0,  0,  0,  0,  2,  4,  10,
      12, 0,  0,  0,  6,  10, 12, 0,  0,  0,  0,  0,  0,  6,  10, 12, 0,  0,
      0,  0,  2,  6,  10, 12, 0,  0,  0,  0,  0,  2,  6,  10, 12, 0,  0,  0,
      4,  6,  10, 12, 0,  0,  0,  0,  0,  4,  6,  10, 12, 0,  0,  0,  2,  4,
      6,  10, 12, 0,  0,  0,  0,  2,  4,  6,  10, 12, 0,  0,  8,  10, 12, 0,
      0,  0,  0,  0,  0,  8,  10, 12, 0,  0,  0,  0,  2,  8,  10, 12, 0,  0,
      0,  0,  0,  2,  8,  10, 12, 0,  0,  0,  4,  8,  10, 12, 0,  0,  0,  0,
      0,  4,  8,  10, 12, 0,  0,  0,  2,  4,  8,  10, 12, 0,  0,  0,  0,  2,
      4,  8,  10, 12, 0,  0,  6,  8,  10, 12, 0,  0,  0,  0,  0,  6,  8,  10,
      12, 0,  0,  0,  2,  6,  8,  10, 12, 0,  0,  0,  0,  2,  6,  8,  10, 12,
      0,  0,  4,  6,  8,  10, 12, 0,  0,  0,  0,  4,  6,  8,  10, 12, 0,  0,
      2,  4,  6,  8,  10, 12, 0,  0,  0,  2,  4,  6,  8,  10, 12, 0,  14, 0,
      0,  0,  0,  0,  0,  0,  0,  14, 0,  0,  0,  0,  0,  0,  2,  14, 0,  0,
      0,  0,  0,  0,  0,  2,  14, 0,  0,  0,  0,  0,  4,  14, 0,  0,  0,  0,
      0,  0,  0,  4,  14, 0,  0,  0,  0,  0,  2,  4,  14, 0,  0,  0,  0,  0,
      0,  2,  4,  14, 0,  0,  0,  0,  6,  14, 0,  0,  0,  0,  0,  0,  0,  6,
      14, 0,  0,  0,  0,  0,  2,  6,  14, 0,  0,  0,  0,  0,  0,  2,  6,  14,
      0,  0,  0,  0,  4,  6,  14, 0,  0,  0,  0,  0,  0,  4,  6,  14, 0,  0,
      0,  0,  2,  4,  6,  14, 0,  0,  0,  0,  0,  2,  4,  6,  14, 0,  0,  0,
      8,  14, 0,  0,  0,  0,  0,  0,  0,  8,  14, 0,  0,  0,  0,  0,  2,  8,
      14, 0,  0,  0,  0,  0,  0,  2,  8,  14, 0,  0,  0,  0,  4,  8,  14, 0,
      0,  0,  0,  0,  0,  4,  8,  14, 0,  0,  0,  0,  2,  4,  8,  14, 0,  0,
      0,  0,  0,  2,  4,  8,  14, 0,  0,  0,  6,  8,  14, 0,  0,  0,  0,  0,
      0,  6,  8,  14, 0,  0,  0,  0,  2,  6,  8,  14, 0,  0,  0,  0,  0,  2,
      6,  8,  14, 0,  0,  0,  4,  6,  8,  14, 0,  0,  0,  0,  0,  4,  6,  8,
      14, 0,  0,  0,  2,  4,  6,  8,  14, 0,  0,  0,  0,  2,  4,  6,  8,  14,
      0,  0,  10, 14, 0,  0,  0,  0,  0,  0,  0,  10, 14, 0,  0,  0,  0,  0,
      2,  10, 14, 0,  0,  0,  0,  0,  0,  2,  10, 14, 0,  0,  0,  0,  4,  10,
      14, 0,  0,  0,  0,  0,  0,  4,  10, 14, 0,  0,  0,  0,  2,  4,  10, 14,
      0,  0,  0,  0,  0,  2,  4,  10, 14, 0,  0,  0,  6,  10, 14, 0,  0,  0,
      0,  0,  0,  6,  10, 14, 0,  0,  0,  0,  2,  6,  10, 14, 0,  0,  0,  0,
      0,  2,  6,  10, 14, 0,  0,  0,  4,  6,  10, 14, 0,  0,  0,  0,  0,  4,
      6,  10, 14, 0,  0,  0,  2,  4,  6,  10, 14, 0,  0,  0,  0,  2,  4,  6,
      10, 14, 0,  0,  8,  10, 14, 0,  0,  0,  0,  0,  0,  8,  10, 14, 0,  0,
      0,  0,  2,  8,  10, 14, 0,  0,  0,  0,  0,  2,  8,  10, 14, 0,  0,  0,
      4,  8,  10, 14, 0,  0,  0,  0,  0,  4,  8,  10, 14, 0,  0,  0,  2,  4,
      8,  10, 14, 0,  0,  0,  0,  2,  4,  8,  10, 14, 0,  0,  6,  8,  10, 14,
      0,  0,  0,  0,  0,  6,  8,  10, 14, 0,  0,  0,  2,  6,  8,  10, 14, 0,
      0,  0,  0,  2,  6,  8,  10, 14, 0,  0,  4,  6,  8,  10, 14, 0,  0,  0,
      0,  4,  6,  8,  10, 14, 0,  0,  2,  4,  6,  8,  10, 14, 0,  0,  0,  2,
      4,  6,  8,  10, 14, 0,  12, 14, 0,  0,  0,  0,  0,  0,  0,  12, 14, 0,
      0,  0,  0,  0,  2,  12, 14, 0,  0,  0,  0,  0,  0,  2,  12, 14, 0,  0,
      0,  0,  4,  12, 14, 0,  0,  0,  0,  0,  0,  4,  12, 14, 0,  0,  0,  0,
      2,  4,  12, 14, 0,  0,  0,  0,  0,  2,  4,  12, 14, 0,  0,  0,  6,  12,
      14, 0,  0,  0,  0,  0,  0,  6,  12, 14, 0,  0,  0,  0,  2,  6,  12, 14,
      0,  0,  0,  0,  0,  2,  6,  12, 14, 0,  0,  0,  4,  6,  12, 14, 0,  0,
      0,  0,  0,  4,  6,  12, 14, 0,  0,  0,  2,  4,  6,  12, 14, 0,  0,  0,
      0,  2,  4,  6,  12, 14, 0,  0,  8,  12, 14, 0,  0,  0,  0,  0,  0,  8,
      12, 14, 0,  0,  0,  0,  2,  8,  12, 14, 0,  0,  0,  0,  0,  2,  8,  12,
      14, 0,  0,  0,  4,  8,  12, 14, 0,  0,  0,  0,  0,  4,  8,  12, 14, 0,
      0,  0,  2,  4,  8,  12, 14, 0,  0,  0,  0,  2,  4,  8,  12, 14, 0,  0,
      6,  8,  12, 14, 0,  0,  0,  0,  0,  6,  8,  12, 14, 0,  0,  0,  2,  6,
      8,  12, 14, 0,  0,  0,  0,  2,  6,  8,  12, 14, 0,  0,  4,  6,  8,  12,
      14, 0,  0,  0,  0,  4,  6,  8,  12, 14, 0,  0,  2,  4,  6,  8,  12, 14,
      0,  0,  0,  2,  4,  6,  8,  12, 14, 0,  10, 12, 14, 0,  0,  0,  0,  0,
      0,  10, 12, 14, 0,  0,  0,  0,  2,  10, 12, 14, 0,  0,  0,  0,  0,  2,
      10, 12, 14, 0,  0,  0,  4,  10, 12, 14, 0,  0,  0,  0,  0,  4,  10, 12,
      14, 0,  0,  0,  2,  4,  10, 12, 14, 0,  0,  0,  0,  2,  4,  10, 12, 14,
      0,  0,  6,  10, 12, 14, 0,  0,  0,  0,  0,  6,  10, 12, 14, 0,  0,  0,
      2,  6,  10, 12, 14, 0,  0,  0,  0,  2,  6,  10, 12, 14, 0,  0,  4,  6,
      10, 12, 14, 0,  0,  0,  0,  4,  6,  10, 12, 14, 0,  0,  2,  4,  6,  10,
      12, 14, 0,  0,  0,  2,  4,  6,  10, 12, 14, 0,  8,  10, 12, 14, 0,  0,
      0,  0,  0,  8,  10, 12, 14, 0,  0,  0,  2,  8,  10, 12, 14, 0,  0,  0,
      0,  2,  8,  10, 12, 14, 0,  0,  4,  8,  10, 12, 14, 0,  0,  0,  0,  4,
      8,  10, 12, 14, 0,  0,  2,  4,  8,  10, 12, 14, 0,  0,  0,  2,  4,  8,
      10, 12, 14, 0,  6,  8,  10, 12, 14, 0,  0,  0,  0,  6,  8,  10, 12, 14,
      0,  0,  2,  6,  8,  10, 12, 14, 0,  0,  0,  2,  6,  8,  10, 12, 14, 0,
      4,  6,  8,  10, 12, 14, 0,  0,  0,  4,  6,  8,  10, 12, 14, 0,  2,  4,
      6,  8,  10, 12, 14, 0,  0,  2,  4,  6,  8,  10, 12, 14};

  const Vec128<uint8_t, 2 * N> byte_idx{Load(d8, table + mask_bits * 8).raw};
  const Vec128<uint16_t, N> pairs = ZipLower(byte_idx, byte_idx);
  return BitCast(d, pairs + Set(du, 0x0100));
}

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> Idx32x4FromBits(const uint64_t mask_bits) {
  HWY_DASSERT(mask_bits < 16);

  // There are only 4 lanes, so we can afford to load the index vector directly.
  alignas(16) constexpr uint8_t packed_array[16 * 16] = {
      0,  1,  2,  3,  0,  1,  2,  3,  0,  1,  2,  3,  0,  1,  2,  3,  //
      0,  1,  2,  3,  0,  1,  2,  3,  0,  1,  2,  3,  0,  1,  2,  3,  //
      4,  5,  6,  7,  0,  1,  2,  3,  0,  1,  2,  3,  0,  1,  2,  3,  //
      0,  1,  2,  3,  4,  5,  6,  7,  0,  1,  2,  3,  0,  1,  2,  3,  //
      8,  9,  10, 11, 0,  1,  2,  3,  0,  1,  2,  3,  0,  1,  2,  3,  //
      0,  1,  2,  3,  8,  9,  10, 11, 0,  1,  2,  3,  0,  1,  2,  3,  //
      4,  5,  6,  7,  8,  9,  10, 11, 0,  1,  2,  3,  0,  1,  2,  3,  //
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 0,  1,  2,  3,  //
      12, 13, 14, 15, 0,  1,  2,  3,  0,  1,  2,  3,  0,  1,  2,  3,  //
      0,  1,  2,  3,  12, 13, 14, 15, 0,  1,  2,  3,  0,  1,  2,  3,  //
      4,  5,  6,  7,  12, 13, 14, 15, 0,  1,  2,  3,  0,  1,  2,  3,  //
      0,  1,  2,  3,  4,  5,  6,  7,  12, 13, 14, 15, 0,  1,  2,  3,  //
      8,  9,  10, 11, 12, 13, 14, 15, 0,  1,  2,  3,  0,  1,  2,  3,  //
      0,  1,  2,  3,  8,  9,  10, 11, 12, 13, 14, 15, 0,  1,  2,  3,  //
      4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 0,  1,  2,  3,  //
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15};

  const Simd<T, N> d;
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, Load(d8, packed_array + 16 * mask_bits));
}

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> Idx64x2FromBits(const uint64_t mask_bits) {
  HWY_DASSERT(mask_bits < 4);

  // There are only 2 lanes, so we can afford to load the index vector directly.
  alignas(16) constexpr uint8_t packed_array[4 * 16] = {
      0, 1, 2,  3,  4,  5,  6,  7,  0, 1, 2,  3,  4,  5,  6,  7,  //
      0, 1, 2,  3,  4,  5,  6,  7,  0, 1, 2,  3,  4,  5,  6,  7,  //
      8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2,  3,  4,  5,  6,  7,  //
      0, 1, 2,  3,  4,  5,  6,  7,  8, 9, 10, 11, 12, 13, 14, 15};

  const Simd<T, N> d;
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, Load(d8, packed_array + 16 * mask_bits));
}

// Helper function called by both Compress and CompressStore - avoids a
// redundant BitsFromMask in the latter.

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> Compress(hwy::SizeTag<2> /*tag*/, Vec128<T, N> v,
                                 const uint64_t mask_bits) {
  const auto idx = detail::Idx16x8FromBits<T, N>(mask_bits);
  using D = Simd<T, N>;
  const RebindToSigned<D> di;
  return BitCast(D(), TableLookupBytes(BitCast(di, v), BitCast(di, idx)));
}

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> Compress(hwy::SizeTag<4> /*tag*/, Vec128<T, N> v,
                                 const uint64_t mask_bits) {
  using D = Simd<T, N>;
  using TI = MakeSigned<T>;
  const Rebind<TI, D> di;
#if HWY_TARGET <= HWY_AVX3
  return BitCast(D(), Vec128<TI, N>{_mm_maskz_compress_epi32(
                          mask_bits, BitCast(di, v).raw)});
#else
  const auto idx = detail::Idx32x4FromBits<T, N>(mask_bits);
  return BitCast(D(), TableLookupBytes(BitCast(di, v), BitCast(di, idx)));
#endif
}

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> Compress(hwy::SizeTag<8> /*tag*/, Vec128<T, N> v,
                                 const uint64_t mask_bits) {
  using D = Simd<T, N>;
  using TI = MakeSigned<T>;
  const Rebind<TI, D> di;
#if HWY_TARGET <= HWY_AVX3
  return BitCast(D(), Vec128<TI, N>{_mm_maskz_compress_epi64(
                          mask_bits, BitCast(di, v).raw)});
#else
  const auto idx = detail::Idx64x2FromBits<T, N>(mask_bits);
  return BitCast(D(), TableLookupBytes(BitCast(di, v), BitCast(di, idx)));
#endif
}

}  // namespace detail

template <typename T, size_t N>
HWY_API Vec128<T, N> Compress(Vec128<T, N> v, const Mask128<T, N> mask) {
  return detail::Compress(hwy::SizeTag<sizeof(T)>(), v,
                          detail::BitsFromMask(mask));
}

// ------------------------------ CompressStore

template <typename T, size_t N>
HWY_API size_t CompressStore(Vec128<T, N> v, const Mask128<T, N> mask,
                             Simd<T, N> d, T* HWY_RESTRICT aligned) {
  const uint64_t mask_bits = detail::BitsFromMask(mask);
  // Avoid _mm_maskmoveu_si128 (>500 cycle latency because it bypasses caches).
  Store(detail::Compress(hwy::SizeTag<sizeof(T)>(), v, mask_bits), d, aligned);
  return PopCount(mask_bits);
}

// ------------------------------ StoreInterleaved3 (CombineShiftRightBytes,
// TableLookupBytes)

// 128 bits
HWY_API void StoreInterleaved3(const Vec128<uint8_t> v0,
                               const Vec128<uint8_t> v1,
                               const Vec128<uint8_t> v2, Full128<uint8_t> d,
                               uint8_t* HWY_RESTRICT unaligned) {
  const auto k5 = Set(d, 5);
  const auto k6 = Set(d, 6);

  // Shuffle (v0,v1,v2) vector bytes to (MSB on left): r5, bgr[4:0].
  // 0x80 so lanes to be filled from other vectors are 0 for blending.
  alignas(16) static constexpr uint8_t tbl_r0[16] = {
      0, 0x80, 0x80, 1, 0x80, 0x80, 2, 0x80, 0x80,  //
      3, 0x80, 0x80, 4, 0x80, 0x80, 5};
  alignas(16) static constexpr uint8_t tbl_g0[16] = {
      0x80, 0, 0x80, 0x80, 1, 0x80,  //
      0x80, 2, 0x80, 0x80, 3, 0x80, 0x80, 4, 0x80, 0x80};
  const auto shuf_r0 = Load(d, tbl_r0);
  const auto shuf_g0 = Load(d, tbl_g0);  // cannot reuse r0 due to 5 in MSB
  const auto shuf_b0 = CombineShiftRightBytes<15>(shuf_g0, shuf_g0);
  const auto r0 = TableLookupBytes(v0, shuf_r0);  // 5..4..3..2..1..0
  const auto g0 = TableLookupBytes(v1, shuf_g0);  // ..4..3..2..1..0.
  const auto b0 = TableLookupBytes(v2, shuf_b0);  // .4..3..2..1..0..
  const auto int0 = r0 | g0 | b0;
  StoreU(int0, d, unaligned + 0 * 16);

  // Second vector: g10,r10, bgr[9:6], b5,g5
  const auto shuf_r1 = shuf_b0 + k6;  // .A..9..8..7..6..
  const auto shuf_g1 = shuf_r0 + k5;  // A..9..8..7..6..5
  const auto shuf_b1 = shuf_g0 + k5;  // ..9..8..7..6..5.
  const auto r1 = TableLookupBytes(v0, shuf_r1);
  const auto g1 = TableLookupBytes(v1, shuf_g1);
  const auto b1 = TableLookupBytes(v2, shuf_b1);
  const auto int1 = r1 | g1 | b1;
  StoreU(int1, d, unaligned + 1 * 16);

  // Third vector: bgr[15:11], b10
  const auto shuf_r2 = shuf_b1 + k6;  // ..F..E..D..C..B.
  const auto shuf_g2 = shuf_r1 + k5;  // .F..E..D..C..B..
  const auto shuf_b2 = shuf_g1 + k5;  // F..E..D..C..B..A
  const auto r2 = TableLookupBytes(v0, shuf_r2);
  const auto g2 = TableLookupBytes(v1, shuf_g2);
  const auto b2 = TableLookupBytes(v2, shuf_b2);
  const auto int2 = r2 | g2 | b2;
  StoreU(int2, d, unaligned + 2 * 16);
}

// 64 bits
HWY_API void StoreInterleaved3(const Vec128<uint8_t, 8> v0,
                               const Vec128<uint8_t, 8> v1,
                               const Vec128<uint8_t, 8> v2, Simd<uint8_t, 8> d,
                               uint8_t* HWY_RESTRICT unaligned) {
  // Use full vectors for the shuffles and first result.
  const Full128<uint8_t> d_full;
  const auto k5 = Set(d_full, 5);
  const auto k6 = Set(d_full, 6);

  const Vec128<uint8_t> full_a{v0.raw};
  const Vec128<uint8_t> full_b{v1.raw};
  const Vec128<uint8_t> full_c{v2.raw};

  // Shuffle (v0,v1,v2) vector bytes to (MSB on left): r5, bgr[4:0].
  // 0x80 so lanes to be filled from other vectors are 0 for blending.
  alignas(16) static constexpr uint8_t tbl_r0[16] = {
      0, 0x80, 0x80, 1, 0x80, 0x80, 2, 0x80, 0x80,  //
      3, 0x80, 0x80, 4, 0x80, 0x80, 5};
  alignas(16) static constexpr uint8_t tbl_g0[16] = {
      0x80, 0, 0x80, 0x80, 1, 0x80,  //
      0x80, 2, 0x80, 0x80, 3, 0x80, 0x80, 4, 0x80, 0x80};
  const auto shuf_r0 = Load(d_full, tbl_r0);
  const auto shuf_g0 = Load(d_full, tbl_g0);  // cannot reuse r0 due to 5 in MSB
  const auto shuf_b0 = CombineShiftRightBytes<15>(shuf_g0, shuf_g0);
  const auto r0 = TableLookupBytes(full_a, shuf_r0);  // 5..4..3..2..1..0
  const auto g0 = TableLookupBytes(full_b, shuf_g0);  // ..4..3..2..1..0.
  const auto b0 = TableLookupBytes(full_c, shuf_b0);  // .4..3..2..1..0..
  const auto int0 = r0 | g0 | b0;
  StoreU(int0, d_full, unaligned + 0 * 16);

  // Second (HALF) vector: bgr[7:6], b5,g5
  const auto shuf_r1 = shuf_b0 + k6;  // ..7..6..
  const auto shuf_g1 = shuf_r0 + k5;  // .7..6..5
  const auto shuf_b1 = shuf_g0 + k5;  // 7..6..5.
  const auto r1 = TableLookupBytes(full_a, shuf_r1);
  const auto g1 = TableLookupBytes(full_b, shuf_g1);
  const auto b1 = TableLookupBytes(full_c, shuf_b1);
  const decltype(Zero(d)) int1{(r1 | g1 | b1).raw};
  StoreU(int1, d, unaligned + 1 * 16);
}

// <= 32 bits
template <size_t N, HWY_IF_LE32(uint8_t, N)>
HWY_API void StoreInterleaved3(const Vec128<uint8_t, N> v0,
                               const Vec128<uint8_t, N> v1,
                               const Vec128<uint8_t, N> v2,
                               Simd<uint8_t, N> /*tag*/,
                               uint8_t* HWY_RESTRICT unaligned) {
  // Use full vectors for the shuffles and result.
  const Full128<uint8_t> d_full;

  const Vec128<uint8_t> full_a{v0.raw};
  const Vec128<uint8_t> full_b{v1.raw};
  const Vec128<uint8_t> full_c{v2.raw};

  // Shuffle (v0,v1,v2) vector bytes to bgr[3:0].
  // 0x80 so lanes to be filled from other vectors are 0 for blending.
  alignas(16) static constexpr uint8_t tbl_r0[16] = {
      0,    0x80, 0x80, 1,   0x80, 0x80, 2, 0x80, 0x80, 3, 0x80, 0x80,  //
      0x80, 0x80, 0x80, 0x80};
  const auto shuf_r0 = Load(d_full, tbl_r0);
  const auto shuf_g0 = CombineShiftRightBytes<15>(shuf_r0, shuf_r0);
  const auto shuf_b0 = CombineShiftRightBytes<14>(shuf_r0, shuf_r0);
  const auto r0 = TableLookupBytes(full_a, shuf_r0);  // ......3..2..1..0
  const auto g0 = TableLookupBytes(full_b, shuf_g0);  // .....3..2..1..0.
  const auto b0 = TableLookupBytes(full_c, shuf_b0);  // ....3..2..1..0..
  const auto int0 = r0 | g0 | b0;
  alignas(16) uint8_t buf[16];
  StoreU(int0, d_full, buf);
  CopyBytes<N * 3>(buf, unaligned);
}

// ------------------------------ StoreInterleaved4

// 128 bits
HWY_API void StoreInterleaved4(const Vec128<uint8_t> v0,
                               const Vec128<uint8_t> v1,
                               const Vec128<uint8_t> v2,
                               const Vec128<uint8_t> v3, Full128<uint8_t> d,
                               uint8_t* HWY_RESTRICT unaligned) {
  // let a,b,c,d denote v0..3.
  const auto ba0 = ZipLower(v0, v1);  // b7 a7 .. b0 a0
  const auto dc0 = ZipLower(v2, v3);  // d7 c7 .. d0 c0
  const auto ba8 = ZipUpper(v0, v1);
  const auto dc8 = ZipUpper(v2, v3);
  const auto dcba_0 = ZipLower(ba0, dc0);  // d..a3 d..a0
  const auto dcba_4 = ZipUpper(ba0, dc0);  // d..a7 d..a4
  const auto dcba_8 = ZipLower(ba8, dc8);  // d..aB d..a8
  const auto dcba_C = ZipUpper(ba8, dc8);  // d..aF d..aC
  StoreU(BitCast(d, dcba_0), d, unaligned + 0 * 16);
  StoreU(BitCast(d, dcba_4), d, unaligned + 1 * 16);
  StoreU(BitCast(d, dcba_8), d, unaligned + 2 * 16);
  StoreU(BitCast(d, dcba_C), d, unaligned + 3 * 16);
}

// 64 bits
HWY_API void StoreInterleaved4(const Vec128<uint8_t, 8> in0,
                               const Vec128<uint8_t, 8> in1,
                               const Vec128<uint8_t, 8> in2,
                               const Vec128<uint8_t, 8> in3,
                               Simd<uint8_t, 8> /*tag*/,
                               uint8_t* HWY_RESTRICT unaligned) {
  // Use full vectors to reduce the number of stores.
  const Vec128<uint8_t> v0{in0.raw};
  const Vec128<uint8_t> v1{in1.raw};
  const Vec128<uint8_t> v2{in2.raw};
  const Vec128<uint8_t> v3{in3.raw};
  // let a,b,c,d denote v0..3.
  const auto ba0 = ZipLower(v0, v1);       // b7 a7 .. b0 a0
  const auto dc0 = ZipLower(v2, v3);       // d7 c7 .. d0 c0
  const auto dcba_0 = ZipLower(ba0, dc0);  // d..a3 d..a0
  const auto dcba_4 = ZipUpper(ba0, dc0);  // d..a7 d..a4
  const Full128<uint8_t> d_full;
  StoreU(BitCast(d_full, dcba_0), d_full, unaligned + 0 * 16);
  StoreU(BitCast(d_full, dcba_4), d_full, unaligned + 1 * 16);
}

// <= 32 bits
template <size_t N, HWY_IF_LE32(uint8_t, N)>
HWY_API void StoreInterleaved4(const Vec128<uint8_t, N> in0,
                               const Vec128<uint8_t, N> in1,
                               const Vec128<uint8_t, N> in2,
                               const Vec128<uint8_t, N> in3,
                               Simd<uint8_t, N> /*tag*/,
                               uint8_t* HWY_RESTRICT unaligned) {
  // Use full vectors to reduce the number of stores.
  const Vec128<uint8_t> v0{in0.raw};
  const Vec128<uint8_t> v1{in1.raw};
  const Vec128<uint8_t> v2{in2.raw};
  const Vec128<uint8_t> v3{in3.raw};
  // let a,b,c,d denote v0..3.
  const auto ba0 = ZipLower(v0, v1);       // b3 a3 .. b0 a0
  const auto dc0 = ZipLower(v2, v3);       // d3 c3 .. d0 c0
  const auto dcba_0 = ZipLower(ba0, dc0);  // d..a3 d..a0
  alignas(16) uint8_t buf[16];
  const Full128<uint8_t> d_full;
  StoreU(BitCast(d_full, dcba_0), d_full, buf);
  CopyBytes<4 * N>(buf, unaligned);
}

// ------------------------------ Reductions

namespace detail {

// N=1 for any T: no-op
template <typename T>
HWY_INLINE Vec128<T, 1> SumOfLanes(hwy::SizeTag<sizeof(T)> /* tag */,
                                   const Vec128<T, 1> v) {
  return v;
}
template <typename T>
HWY_INLINE Vec128<T, 1> MinOfLanes(hwy::SizeTag<sizeof(T)> /* tag */,
                                   const Vec128<T, 1> v) {
  return v;
}
template <typename T>
HWY_INLINE Vec128<T, 1> MaxOfLanes(hwy::SizeTag<sizeof(T)> /* tag */,
                                   const Vec128<T, 1> v) {
  return v;
}

// u32/i32/f32:

// N=2
template <typename T>
HWY_INLINE Vec128<T, 2> SumOfLanes(hwy::SizeTag<4> /* tag */,
                                   const Vec128<T, 2> v10) {
  return v10 + Shuffle2301(v10);
}
template <typename T>
HWY_INLINE Vec128<T, 2> MinOfLanes(hwy::SizeTag<4> /* tag */,
                                   const Vec128<T, 2> v10) {
  return Min(v10, Shuffle2301(v10));
}
template <typename T>
HWY_INLINE Vec128<T, 2> MaxOfLanes(hwy::SizeTag<4> /* tag */,
                                   const Vec128<T, 2> v10) {
  return Max(v10, Shuffle2301(v10));
}

// N=4 (full)
template <typename T>
HWY_INLINE Vec128<T> SumOfLanes(hwy::SizeTag<4> /* tag */,
                                const Vec128<T> v3210) {
  const Vec128<T> v1032 = Shuffle1032(v3210);
  const Vec128<T> v31_20_31_20 = v3210 + v1032;
  const Vec128<T> v20_31_20_31 = Shuffle0321(v31_20_31_20);
  return v20_31_20_31 + v31_20_31_20;
}
template <typename T>
HWY_INLINE Vec128<T> MinOfLanes(hwy::SizeTag<4> /* tag */,
                                const Vec128<T> v3210) {
  const Vec128<T> v1032 = Shuffle1032(v3210);
  const Vec128<T> v31_20_31_20 = Min(v3210, v1032);
  const Vec128<T> v20_31_20_31 = Shuffle0321(v31_20_31_20);
  return Min(v20_31_20_31, v31_20_31_20);
}
template <typename T>
HWY_INLINE Vec128<T> MaxOfLanes(hwy::SizeTag<4> /* tag */,
                                const Vec128<T> v3210) {
  const Vec128<T> v1032 = Shuffle1032(v3210);
  const Vec128<T> v31_20_31_20 = Max(v3210, v1032);
  const Vec128<T> v20_31_20_31 = Shuffle0321(v31_20_31_20);
  return Max(v20_31_20_31, v31_20_31_20);
}

// u64/i64/f64:

// N=2 (full)
template <typename T>
HWY_INLINE Vec128<T> SumOfLanes(hwy::SizeTag<8> /* tag */,
                                const Vec128<T> v10) {
  const Vec128<T> v01 = Shuffle01(v10);
  return v10 + v01;
}
template <typename T>
HWY_INLINE Vec128<T> MinOfLanes(hwy::SizeTag<8> /* tag */,
                                const Vec128<T> v10) {
  const Vec128<T> v01 = Shuffle01(v10);
  return Min(v10, v01);
}
template <typename T>
HWY_INLINE Vec128<T> MaxOfLanes(hwy::SizeTag<8> /* tag */,
                                const Vec128<T> v10) {
  const Vec128<T> v01 = Shuffle01(v10);
  return Max(v10, v01);
}

}  // namespace detail

// Supported for u/i/f 32/64. Returns the same value in each lane.
template <typename T, size_t N>
HWY_API Vec128<T, N> SumOfLanes(const Vec128<T, N> v) {
  return detail::SumOfLanes(hwy::SizeTag<sizeof(T)>(), v);
}
template <typename T, size_t N>
HWY_API Vec128<T, N> MinOfLanes(const Vec128<T, N> v) {
  return detail::MinOfLanes(hwy::SizeTag<sizeof(T)>(), v);
}
template <typename T, size_t N>
HWY_API Vec128<T, N> MaxOfLanes(const Vec128<T, N> v) {
  return detail::MaxOfLanes(hwy::SizeTag<sizeof(T)>(), v);
}

// ================================================== DEPRECATED

template <typename T, size_t N>
HWY_API size_t StoreMaskBits(const Mask128<T, N> mask, uint8_t* p) {
  return StoreMaskBits(Simd<T, N>(), mask, p);
}

template <typename T, size_t N>
HWY_API bool AllTrue(const Mask128<T, N> mask) {
  return AllTrue(Simd<T, N>(), mask);
}

template <typename T, size_t N>
HWY_API size_t CountTrue(const Mask128<T, N> mask) {
  return CountTrue(Simd<T, N>(), mask);
}

template <typename T, size_t N>
HWY_API Mask128<T, N> Not(const Mask128<T, N> m) {
  return Not(Simd<T, N>(), m);
}

// ================================================== Operator wrapper

// These apply to all x86_*-inl.h because there are no restrictions on V.

template <class V>
HWY_API V Add(V a, V b) {
  return a + b;
}
template <class V>
HWY_API V Sub(V a, V b) {
  return a - b;
}

template <class V>
HWY_API V Mul(V a, V b) {
  return a * b;
}
template <class V>
HWY_API V Div(V a, V b) {
  return a / b;
}

template <class V>
V Shl(V a, V b) {
  return a << b;
}
template <class V>
V Shr(V a, V b) {
  return a >> b;
}

template <class V>
HWY_API auto Eq(V a, V b) -> decltype(a == b) {
  return a == b;
}
template <class V>
HWY_API auto Lt(V a, V b) -> decltype(a == b) {
  return a < b;
}

template <class V>
HWY_API auto Gt(V a, V b) -> decltype(a == b) {
  return a > b;
}
template <class V>
HWY_API auto Ge(V a, V b) -> decltype(a == b) {
  return a >= b;
}

template <class V>
HWY_API auto Le(V a, V b) -> decltype(a == b) {
  return a <= b;
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();
