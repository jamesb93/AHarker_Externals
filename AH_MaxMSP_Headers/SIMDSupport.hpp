
#ifndef SIMDSUPPORT
#define SIMDSUPPORT

#include <cmath>
#include <emmintrin.h>
#include <immintrin.h>
#include <stdint.h>

/*
// Load / Store / Unpack
 
 #define F32_VEC_MOVE_LO                    _mm_movelh_ps
 #define F32_VEC_MOVE_HI                    _mm_movehl_ps
 #define F64_VEC_UNPACK_LO                  _mm_unpacklo_pd
 #define F64_VEC_UNPACK_HI                  _mm_unpackhi_pd
 #define F64_VEC_STORE_HI                   _mm_storeh_pd
 #define F64_VEC_STORE_LO                   _mm_storel_pd
 
 // Conversions
 
 #define F32_VEC_FROM_I32                   _mm_cvtepi32_ps
 #define I32_VEC_FROM_F32_ROUND             _mm_cvtps_epi32
 #define I32_VEC_FROM_F32_TRUNC             _mm_cvttps_epi32
 
 #define F64_VEC_FROM_F32                   _mm_cvtps_pd
 #define F32_VEC_FROM_F64                   _mm_cvtpd_ps
 
 #define F64_VEC_FROM_I32                   _mm_cvtepi32_pd
 #define I32_VEC_FROM_F64_ROUND             _mm_cvtpd_epi32
 #define I32_VEC_FROM_F64_TRUNC             _mm_cvttpd_epi32
 
 // Shuffles
 
 #define I32_VEC_SHUFFLE_OP                 _mm_shuffle_epi32
*/

#ifdef __APPLE__

#include <cpuid.h>

template <class T> T *allocate_aligned(size_t size)
{
    return static_cast<T *>(malloc(size * sizeof(T)));
}

template <class T> void deallocate_aligned(T *ptr)
{
    free(ptr);
}

#elif defined(__linux__)

#include <stdlib.h>

template <class T> T *allocate_aligned(size_t size)
{
    void *mem;
    posix_memalign(&mem, 16, size * sizeof(T));
    return static_cast<T *>(mem);
}

template <class T> void deallocate_aligned(T *ptr)
{
    free(ptr);
}

#else

#include <malloc.h>

template <class T> T *allocate_aligned(size_t size)
{
    return static_cast<T *>(_aligned_malloc(size * sizeof(T), 16));
}

template <class T> void deallocate_aligned(T *ptr)
{
    _aligned_free(ptr);
}

#endif


#include <algorithm>
#include <functional>

#define SIMD_COMPILER_SUPPORT_SCALAR 0
#define SIMD_COMPILER_SUPPORT_SSE128 1
#define SIMD_COMPILER_SUPPORT_AVX256 2
#define SIMD_COMPILER_SUPPORT_AVX512 3

template<class T> struct SIMDLimits
{
    static const int max_size = 1;
    static const int byte_width = sizeof(T);
};

#if defined(__AVX512F__)
#define SIMD_COMPILER_SUPPORT_LEVEL SIMD_COMPILER_SUPPORT_AVX512

template<> struct SIMDLimits<double>
{
    static const int max_size = 8;
    static const int byte_width = 64;
};

template<> struct SIMDLimits<float>
{
    static const int max_size = 16;
    static const int byte_width = 64;
};

#elif defined(__AVX__)
#define SIMD_COMPILER_SUPPORT_LEVEL SIMD_COMPILER_SUPPORT_AVX256

template<> struct SIMDLimits<double>
{
    static const int max_size = 4;
    static const int byte_width = 32;
};

template<> struct SIMDLimits<float>
{
    static const int max_size = 8;
    static const int byte_width = 32;
};

#elif defined(__SSE__)
#define SIMD_COMPILER_SUPPORT_LEVEL SIMD_COMPILER_SUPPORT_SSE128

template<> struct SIMDLimits<double>
{
    static const int max_size = 2;
    static const int byte_width = 16;
};

template<> struct SIMDLimits<float>
{
    static const int max_size = 4;
    static const int byte_width = 16;
};

#else
#define SIMD_COMPILER_SUPPORT_LEVEL SIMD_COMPILER_SUPPORT_SCALAR
#endif

// Select Functionality for all types

template <class T> T select(const T& a, const T& b, const T& mask)
{
    return (b & mask) | and_not(mask, a);
}

// Data Type Definitions

// ******************** A Vector of Given Size (Made of Vectors / Scalars) ******************** //

template <int final_size, class T> struct SizedVector
{
    typedef SizedVector SV;
    typedef typename T::scalar_type scalar_type;
    static const int size = final_size;
    static const int array_size = final_size / T::size;
    
    SizedVector() {}
    SizedVector(const typename T::scalar_type& a)
    {
        for (int i = 0; i < array_size; i++)
            mData[i] = a;
    }
    SizedVector(const SizedVector *ptr) { *this = *ptr; }
    SizedVector(const typename T::scalar_type *array) { *this = *reinterpret_cast<const SizedVector *>(array); }
    
    // This template allows a static loop
    
    template <int First, int Last>
    struct static_for
    {
        template <typename Fn>
        void operator()(SizedVector &result, const SizedVector &a, const SizedVector &b, Fn const& fn) const
        {
            if (First < Last)
            {
                result.mData[First] = fn(a.mData[First], b.mData[First]);
                static_for<First + 1, Last>()(result, a, b, fn);
            }
        }
    };
    
    // This specialisation avoids infinite recursion
    
    template <int N>
    struct static_for<N, N>
    {
        template <typename Fn>
        void operator()(SV &result, const SV &a, const SV &b, Fn const& fn) const {}
    };
    
    template <typename Op> friend SizedVector op(const SV& a, const SV& b, Op op)
    {
        SV result;
        
        static_for<0, array_size>()(result, a, b, op);
        
        return result;
    }
    
    friend SV operator + (const SV& a, const SV& b) { return op(a, b, std::plus<T>()); }
    friend SV operator - (const SV& a, const SV& b) { return op(a, b, std::minus<T>()); }
    friend SV operator * (const SV& a, const SV& b) { return op(a, b, std::multiplies<T>()); }
    friend SV operator / (const SV& a, const SV& b) { return op(a, b, std::divides<T>()); }
    
    SV& operator += (const SV& b)   { return (*this = *this + b); }
    SV& operator -= (const SV& b)   { return (*this = *this - b); }
    SV& operator *= (const SV& b)   { return (*this = *this * b); }
    SV& operator /= (const SV& b)   { return (*this = *this / b); }
    
    //friend SV sqrt(const SV& a) { return sqrt(a.mVal); }
    
    friend SV min(const SV& a, const SV& b) { return op(a, b, std::min<T>()); }
    friend SV max(const SV& a, const SV& b) { return op(a, b, std::max<T>()); }
    //friend SV sel(const SV& a, const SV& b) { return op(a, b, std::sel<T>()); }

    //friend SV and_not(const SV& a, const SV& b) { return (~a.mVal) &  b.mVal; }
    //friend SV operator & (const SV& a, const SV& b) { return a.mVal & b.mVal; }
    //friend SV operator | (const SV& a, const SV& b) { return a.mVal | b.mVal; }
    //friend SV operator ^ (const SV& a, const SV& b) { return a.mVal ^ b.mVal; }
    
    friend SV operator == (const SV& a, const SV& b) { return op(a, b, std::equal_to<T>()); }
    friend SV operator != (const SV& a, const SV& b) { return op(a, b, std::not_equal_to<T>()); }
    friend SV operator > (const SV& a, const SV& b) { return op(a, b, std::greater<T>());; }
    friend SV operator < (const SV& a, const SV& b) { return op(a, b, std::less<T>()); }
    friend SV operator >= (const SV& a, const SV& b) { return op(a, b, std::greater_equal<T>()); }
    friend SV operator <= (const SV& a, const SV& b) { return op(a, b, std::less_equal<T>()); }
    
    T mData[array_size];
};

// ******************** Basic Data Type Defintions ******************** //

template <class T> struct Scalar
{
    static const int size = 1;
    typedef T scalar_type;
    
    Scalar() {}
    Scalar(T a) : mVal(a) {}
    Scalar(const T* a) { mVal = *a; }
    template <class U> Scalar(const Scalar<U> a) { mVal = static_cast<T>(a.mVal); }
    
    void store(T *a) { *a = mVal; }
    
    friend Scalar operator + (const Scalar& a, const Scalar& b) { return a.mVal + b.mVal; }
    friend Scalar operator - (const Scalar& a, const Scalar& b) { return a.mVal - b.mVal; }
    friend Scalar operator * (const Scalar& a, const Scalar& b) { return a.mVal * b.mVal; }
    friend Scalar operator / (const Scalar& a, const Scalar& b) { return a.mVal / b.mVal; }

    Scalar& operator += (const Scalar& b)   { return (*this = *this + b); }
    Scalar& operator -= (const Scalar& b)   { return (*this = *this - b); }
    Scalar& operator *= (const Scalar& b)   { return (*this = *this * b); }
    Scalar& operator /= (const Scalar& b)   { return (*this = *this / b); }
    
    friend Scalar sqrt(const Scalar& a) { return sqrt(a.mVal); }
  
    friend Scalar min(const Scalar& a, const Scalar& b) { return std::min(a.mVal, b.mVal); }
    friend Scalar max(const Scalar& a, const Scalar& b) { return std::max(a.mVal, b.mVal); }
    friend Scalar sel(const Scalar& a, const Scalar& b, const Scalar& c) { return c ? b : a; }

    friend Scalar and_not(const Scalar& a, const Scalar& b) { return (~a.mVal) &  b.mVal; }
    friend Scalar operator & (const Scalar& a, const Scalar& b) { return a.mVal & b.mVal; }
    friend Scalar operator | (const Scalar& a, const Scalar& b) { return a.mVal | b.mVal; }
    friend Scalar operator ^ (const Scalar& a, const Scalar& b) { return a.mVal ^ b.mVal; }
    
    friend Scalar operator == (const Scalar& a, const Scalar& b) { return a.mVal == b.mVal; }
    friend Scalar operator != (const Scalar& a, const Scalar& b) { return a.mVal != b.mVal; }
    friend Scalar operator > (const Scalar& a, const Scalar& b) { return a.mVal > b.mVal; }
    friend Scalar operator < (const Scalar& a, const Scalar& b) { return a.mVal < b.mVal; }
    friend Scalar operator >= (const Scalar& a, const Scalar& b) { return a.mVal >= b.mVal; }
    friend Scalar operator <= (const Scalar& a, const Scalar& b) { return a.mVal <= b.mVal; }

    T mVal;
};

template <class T, class U, int vec_size> struct SIMDVector
{
    static const int size = vec_size;
    typedef T scalar_type;
    
    SIMDVector() {}
    SIMDVector(U a) : mVal(a) {}
    
    U mVal;
};

template <class T, int vec_size> struct SIMDType {};

template<>
struct SIMDType<double, 1>
{
    static const int size = 1;
    typedef float scalar_type;
    
    SIMDType() {}
    SIMDType(double a) : mVal(a) {}
    SIMDType(const double* a) { mVal = *a; }
    
    void store(double *a) { *a = mVal; }

    friend SIMDType operator + (const SIMDType& a, const SIMDType& b) { return a.mVal + b.mVal; }
    friend SIMDType operator - (const SIMDType& a, const SIMDType& b) { return a.mVal - b.mVal; }
    friend SIMDType operator * (const SIMDType& a, const SIMDType& b) { return a.mVal * b.mVal; }
    friend SIMDType operator / (const SIMDType& a, const SIMDType& b) { return a.mVal / b.mVal; }
    
    SIMDType& operator += (const SIMDType& b)   { return (*this = *this + b); }
    SIMDType& operator -= (const SIMDType& b)   { return (*this = *this - b); }
    SIMDType& operator *= (const SIMDType& b)   { return (*this = *this * b); }
    SIMDType& operator /= (const SIMDType& b)   { return (*this = *this / b); }
    
    friend SIMDType sqrt(const SIMDType& a) { return sqrt(a.mVal); }
    
    friend SIMDType round(const SIMDType& a) { return round(a.mVal); }
    friend SIMDType trunc(const SIMDType& a) { return trunc(a.mVal); }
    
    friend SIMDType min(const SIMDType& a, const SIMDType& b) { return std::min(a.mVal, b.mVal); }
    friend SIMDType max(const SIMDType& a, const SIMDType& b) { return std::max(a.mVal, b.mVal); }
    friend SIMDType sel(const SIMDType& a, const SIMDType& b, const SIMDType& c) { return c.mVal ? b.mVal : a.mVal; }

    friend SIMDType operator == (const SIMDType& a, const SIMDType& b) { return a.mVal == b.mVal; }
    friend SIMDType operator != (const SIMDType& a, const SIMDType& b) { return a.mVal != b.mVal; }
    friend SIMDType operator > (const SIMDType& a, const SIMDType& b) { return a.mVal > b.mVal; }
    friend SIMDType operator < (const SIMDType& a, const SIMDType& b) { return a.mVal < b.mVal; }
    friend SIMDType operator >= (const SIMDType& a, const SIMDType& b) { return a.mVal >= b.mVal; }
    friend SIMDType operator <= (const SIMDType& a, const SIMDType& b) { return a.mVal <= b.mVal; }
    
    double mVal;
};

template<>
struct SIMDType<float, 1>
{
    static const int size = 1;
    typedef float scalar_type;
    
    SIMDType() {}
    SIMDType(float a) : mVal(a) {}
    SIMDType(const float* a) { mVal = *a; }
    
    void store(float *a) { *a = mVal; }
    
    friend SIMDType operator + (const SIMDType& a, const SIMDType& b) { return a.mVal + b.mVal; }
    friend SIMDType operator - (const SIMDType& a, const SIMDType& b) { return a.mVal - b.mVal; }
    friend SIMDType operator * (const SIMDType& a, const SIMDType& b) { return a.mVal * b.mVal; }
    friend SIMDType operator / (const SIMDType& a, const SIMDType& b) { return a.mVal / b.mVal; }
    
    SIMDType& operator += (const SIMDType& b)   { return (*this = *this + b); }
    SIMDType& operator -= (const SIMDType& b)   { return (*this = *this - b); }
    SIMDType& operator *= (const SIMDType& b)   { return (*this = *this * b); }
    SIMDType& operator /= (const SIMDType& b)   { return (*this = *this / b); }
    
    friend SIMDType sqrt(const SIMDType& a) { return sqrtf(a.mVal); }
    
    friend SIMDType round(const SIMDType& a) { return roundf(a.mVal); }
    friend SIMDType trunc(const SIMDType& a) { return truncf(a.mVal); }
    
    friend SIMDType min(const SIMDType& a, const SIMDType& b) { return std::min(a.mVal, b.mVal); }
    friend SIMDType max(const SIMDType& a, const SIMDType& b) { return std::max(a.mVal, b.mVal); }
    friend SIMDType sel(const SIMDType& a, const SIMDType& b, const SIMDType& c) { return c.mVal ? b.mVal : a.mVal; }
    
    friend SIMDType operator == (const SIMDType& a, const SIMDType& b) { return a.mVal == b.mVal; }
    friend SIMDType operator != (const SIMDType& a, const SIMDType& b) { return a.mVal != b.mVal; }
    friend SIMDType operator > (const SIMDType& a, const SIMDType& b) { return a.mVal > b.mVal; }
    friend SIMDType operator < (const SIMDType& a, const SIMDType& b) { return a.mVal < b.mVal; }
    friend SIMDType operator >= (const SIMDType& a, const SIMDType& b) { return a.mVal >= b.mVal; }
    friend SIMDType operator <= (const SIMDType& a, const SIMDType& b) { return a.mVal <= b.mVal; }
    
    operator SIMDType<double, 1>() { return static_cast<double>(mVal); }
    
    float mVal;
};

template<>
struct SIMDType<float, 2>
{
    static const int size = 1;
    typedef float scalar_type;
    
    SIMDType() {}
    
    SIMDType(float a)
    {
        mVals[0] = a;
        mVals[1] = a;
    }
    
    SIMDType(const float* a)
    {
        mVals[0] = a[0];
        mVals[1] = a[1];
    }
    
    void store(float *a)
    {
        a[0] = mVals[0];
        a[1] = mVals[1];
    }
    
    // No Ops for now..
    
    /*
    friend SIMDType operator + (const SIMDType& a, const SIMDType& b) { return a.mVal + b.mVal; }
    friend SIMDType operator - (const SIMDType& a, const SIMDType& b) { return a.mVal - b.mVal; }
    friend SIMDType operator * (const SIMDType& a, const SIMDType& b) { return a.mVal * b.mVal; }
    friend SIMDType operator / (const SIMDType& a, const SIMDType& b) { return a.mVal / b.mVal; }
    
    SIMDType& operator += (const SIMDType& b)   { return (*this = *this + b); }
    SIMDType& operator -= (const SIMDType& b)   { return (*this = *this - b); }
    SIMDType& operator *= (const SIMDType& b)   { return (*this = *this * b); }
    SIMDType& operator /= (const SIMDType& b)   { return (*this = *this / b); }
     
    friend SIMDType sqrt(const SIMDType& a) { return sqrt(a.mVal); }
    
    friend SIMDType min(const SIMDType& a, const SIMDType& b) { return std::min(a.mVal, b.mVal); }
    friend SIMDType max(const SIMDType& a, const SIMDType& b) { return std::max(a.mVal, b.mVal); }
    friend SIMDType sel(const SIMDType& a, const SIMDType& b, const SIMDType& c) { return c.mVal ? b.mVal : a.mVal; }
    
    friend SIMDType operator == (const SIMDType& a, const SIMDType& b) { return a.mVal == b.mVal; }
    friend SIMDType operator != (const SIMDType& a, const SIMDType& b) { return a.mVal != b.mVal; }
    friend SIMDType operator > (const SIMDType& a, const SIMDType& b) { return a.mVal > b.mVal; }
    friend SIMDType operator < (const SIMDType& a, const SIMDType& b) { return a.mVal < b.mVal; }
    friend SIMDType operator >= (const SIMDType& a, const SIMDType& b) { return a.mVal >= b.mVal; }
    friend SIMDType operator <= (const SIMDType& a, const SIMDType& b) { return a.mVal <= b.mVal; }
    */
    float mVals[2];
};

#if (SIMD_COMPILER_SUPPORT_LEVEL >= SIMD_COMPILER_SUPPORT_SSE)

typedef SIMDType<double, 2> SSEDouble;
typedef SIMDType<float, 4> SSEFloat;
typedef SIMDType<int32_t, 4> SSEInt32;

template<>
struct SIMDType<double, 2> : public SIMDVector<double, __m128d, 2>
{
    SIMDType() {}
    SIMDType(const double& a) { mVal = _mm_set1_pd(a); }
    SIMDType(const double* a) { mVal = _mm_loadu_pd(a); }
    SIMDType(__m128d a) : SIMDVector(a) {}
    
    SIMDType(const SIMDType<float, 2> a)
    {
        // FIX - this might be able to be made nicer...
        
        double vals[2];
        
        vals[0] = a.mVals[0];
        vals[1] = a.mVals[1];
        
        mVal = _mm_loadu_pd(vals);
    }
    
    void store(double *a) { _mm_storeu_pd(a, mVal); }

    friend SIMDType operator + (const SIMDType &a, const SIMDType& b) { return _mm_add_pd(a.mVal, b.mVal); }
    friend SIMDType operator - (const SIMDType &a, const SIMDType& b) { return _mm_sub_pd(a.mVal, b.mVal); }
    friend SIMDType operator * (const SIMDType &a, const SIMDType& b) { return _mm_mul_pd(a.mVal, b.mVal); }
    friend SIMDType operator / (const SIMDType &a, const SIMDType& b) { return _mm_div_pd(a.mVal, b.mVal); }
    
    SIMDType& operator += (const SIMDType& b)   { return (*this = *this + b); }
    SIMDType& operator -= (const SIMDType& b)   { return (*this = *this - b); }
    SIMDType& operator *= (const SIMDType& b)   { return (*this = *this * b); }
    SIMDType& operator /= (const SIMDType& b)   { return (*this = *this / b); }
    
    friend SIMDType sqrt(const SIMDType& a) { return _mm_sqrt_pd(a.mVal); }
    
    friend SIMDType round(const SIMDType& a) { return _mm_round_pd(a.mVal, _MM_FROUND_TO_NEAREST_INT |_MM_FROUND_NO_EXC); }
    friend SIMDType trunc(const SIMDType& a) { return _mm_round_pd(a.mVal, _MM_FROUND_TO_ZERO |_MM_FROUND_NO_EXC); }
    
    friend SIMDType min(const SIMDType& a, const SIMDType& b) { return _mm_min_pd(a.mVal, b.mVal); }
    friend SIMDType max(const SIMDType& a, const SIMDType& b) { return _mm_max_pd(a.mVal, b.mVal); }
    friend SIMDType sel(const SIMDType& a, const SIMDType& b, const SIMDType& c) { return and_not(c, a) | (b & c); }
    
    friend SIMDType and_not(const SIMDType& a, const SIMDType& b) { return _mm_andnot_pd(a.mVal, b.mVal); }
    friend SIMDType operator & (const SIMDType& a, const SIMDType& b) { return _mm_and_pd(a.mVal, b.mVal); }
    friend SIMDType operator | (const SIMDType& a, const SIMDType& b) { return _mm_or_pd(a.mVal, b.mVal); }
    friend SIMDType operator ^ (const SIMDType& a, const SIMDType& b) { return _mm_xor_pd(a.mVal, b.mVal); }
    
    friend SIMDType operator == (const SIMDType& a, const SIMDType& b) { return _mm_cmpeq_pd(a.mVal, b.mVal); }
    friend SIMDType operator != (const SIMDType& a, const SIMDType& b) { return _mm_cmpneq_pd(a.mVal, b.mVal); }
    friend SIMDType operator > (const SIMDType& a, const SIMDType& b) { return _mm_cmplt_pd(a.mVal, b.mVal); }
    friend SIMDType operator < (const SIMDType& a, const SIMDType& b) { return _mm_cmpgt_pd(a.mVal, b.mVal); }
    friend SIMDType operator >= (const SIMDType& a, const SIMDType& b) { return _mm_cmple_pd(a.mVal, b.mVal); }
    friend SIMDType operator <= (const SIMDType& a, const SIMDType& b) { return _mm_cmpge_pd(a.mVal, b.mVal); }

    template <int y, int x> static SIMDType shuffle(const SIMDType& a, const SIMDType& b)
    {
        return _mm_shuffle_pd(a.mVal, b.mVal, (y<<1)|x);
    }
};

#if (SIMD_COMPILER_SUPPORT_LEVEL >= SIMD_COMPILER_SUPPORT_AVX256)

template <class T> struct AVX256Type {};

typedef SIMDType<double, 4> AVX256Double;
typedef SIMDType<float, 8> AVX256Float;
typedef SIMDType<int32_t, 8> AVX256Int32;

template<>
struct SIMDType<double, 4> : public SIMDVector<double, __m256d, 4>
{
    SIMDType() {}
    SIMDType(const double& a) { mVal = _mm256_set1_pd(a); }
    SIMDType(const double* a) { mVal = _mm256_loadu_pd(a); }
    SIMDType(__m256d a) : SIMDVector(a) {}
    
    void store(double *a) { _mm256_storeu_pd(a, mVal); }

    friend SIMDType operator + (const SIMDType &a, const SIMDType &b) { return _mm256_add_pd(a.mVal, b.mVal); }
    friend SIMDType operator - (const SIMDType &a, const SIMDType &b) { return _mm256_sub_pd(a.mVal, b.mVal); }
    friend SIMDType operator * (const SIMDType &a, const SIMDType &b) { return _mm256_mul_pd(a.mVal, b.mVal); }
    friend SIMDType operator / (const SIMDType &a, const SIMDType &b) { return _mm256_div_pd(a.mVal, b.mVal); }
    
    SIMDType& operator += (const SIMDType& b)   { return (*this = *this + b); }
    SIMDType& operator -= (const SIMDType& b)   { return (*this = *this - b); }
    SIMDType& operator *= (const SIMDType& b)   { return (*this = *this * b); }
    SIMDType& operator /= (const SIMDType& b)   { return (*this = *this / b); }

    friend SIMDType sqrt(const SIMDType& a) { return _mm256_sqrt_pd(a.mVal); }
    
    friend SIMDType round(const SIMDType& a) { return _mm256_round_pd(a.mVal, _MM_FROUND_TO_NEAREST_INT |_MM_FROUND_NO_EXC); }
    friend SIMDType trunc(const SIMDType& a) { return _mm256_round_pd(a.mVal, _MM_FROUND_TO_ZERO |_MM_FROUND_NO_EXC); }
    
    friend SIMDType min(const SIMDType& a, const SIMDType& b) { return _mm256_min_pd(a.mVal, b.mVal); }
    friend SIMDType max(const SIMDType& a, const SIMDType& b) { return _mm256_max_pd(a.mVal, b.mVal); }
    friend SIMDType sel(const SIMDType& a, const SIMDType& b, const SIMDType& c) { return and_not(c, a) | (b & c); }

    friend SIMDType and_not(const SIMDType& a, const SIMDType& b) { return _mm256_andnot_pd(a.mVal, b.mVal); }
    friend SIMDType operator & (const SIMDType& a, const SIMDType& b) { return _mm256_and_pd(a.mVal, b.mVal); }
    friend SIMDType operator | (const SIMDType& a, const SIMDType& b) { return _mm256_or_pd(a.mVal, b.mVal); }
    friend SIMDType operator ^ (const SIMDType& a, const SIMDType& b) { return _mm256_xor_pd(a.mVal, b.mVal); }
    
    friend SIMDType operator == (const SIMDType& a, const SIMDType& b) { return _mm256_cmp_pd(a.mVal, b.mVal, _CMP_EQ_OQ); }
    friend SIMDType operator != (const SIMDType& a, const SIMDType& b) { return _mm256_cmp_pd(a.mVal, b.mVal, _CMP_NEQ_OQ); }
    friend SIMDType operator > (const SIMDType& a, const SIMDType& b) { return _mm256_cmp_pd(a.mVal, b.mVal, _CMP_GT_OQ); }
    friend SIMDType operator < (const SIMDType& a, const SIMDType& b) { return _mm256_cmp_pd(a.mVal, b.mVal, _CMP_LT_OQ); }
    friend SIMDType operator >= (const SIMDType& a, const SIMDType& b) { return _mm256_cmp_pd(a.mVal, b.mVal, _CMP_GE_OQ); }
    friend SIMDType operator <= (const SIMDType& a, const SIMDType& b) { return _mm256_cmp_pd(a.mVal, b.mVal, _CMP_LE_OQ); }
};

#endif

template<>
struct SIMDType<float, 4> : public SIMDVector<float, __m128, 4>
{
    SIMDType() {}
    SIMDType(const float& a) { mVal = _mm_set1_ps(a); }
    SIMDType(const float* a) { mVal = _mm_loadu_ps(a); }
    SIMDType(__m128 a) : SIMDVector(a) {}
    
    void store(float *a) { _mm_storeu_ps(a, mVal); }

    friend SIMDType operator + (const SIMDType& a, const SIMDType& b) { return _mm_add_ps(a.mVal, b.mVal); }
    friend SIMDType operator - (const SIMDType& a, const SIMDType& b) { return _mm_sub_ps(a.mVal, b.mVal); }
    friend SIMDType operator * (const SIMDType& a, const SIMDType& b) { return _mm_mul_ps(a.mVal, b.mVal); }
    friend SIMDType operator / (const SIMDType& a, const SIMDType& b) { return _mm_div_ps(a.mVal, b.mVal); }

    SIMDType& operator += (const SIMDType& b)   { return (*this = *this + b); }
    SIMDType& operator -= (const SIMDType& b)   { return (*this = *this - b); }
    SIMDType& operator *= (const SIMDType& b)   { return (*this = *this * b); }
    SIMDType& operator /= (const SIMDType& b)   { return (*this = *this / b); }
    
    friend SIMDType sqrt(const SIMDType& a) { return _mm_sqrt_ps(a.mVal); }
    
    friend SIMDType round(const SIMDType& a) { return _mm_round_ps(a.mVal, _MM_FROUND_TO_NEAREST_INT |_MM_FROUND_NO_EXC); }
    friend SIMDType trunc(const SIMDType& a) { return _mm_round_ps(a.mVal, _MM_FROUND_TO_ZERO |_MM_FROUND_NO_EXC); }
    
    friend SIMDType min(const SIMDType& a, const SIMDType& b) { return _mm_min_ps(a.mVal, b.mVal); }
    friend SIMDType max(const SIMDType& a, const SIMDType& b) { return _mm_max_ps(a.mVal, b.mVal); }
    friend SIMDType sel(const SIMDType& a, const SIMDType& b, const SIMDType& c) { return and_not(c, a) | (b & c); }

    friend SIMDType and_not(const SIMDType& a, const SIMDType& b) { return _mm_andnot_ps(a.mVal, b.mVal); }
    friend SIMDType operator & (const SIMDType& a, const SIMDType& b) { return _mm_and_ps(a.mVal, b.mVal); }
    friend SIMDType operator | (const SIMDType& a, const SIMDType& b) { return _mm_or_ps(a.mVal, b.mVal); }
    friend SIMDType operator ^ (const SIMDType& a, const SIMDType& b) { return _mm_xor_ps(a.mVal, b.mVal); }

    friend SIMDType operator == (const SIMDType& a, const SIMDType& b) { return _mm_cmpeq_ps(a.mVal, b.mVal); }
    friend SIMDType operator != (const SIMDType& a, const SIMDType& b) { return _mm_cmpneq_ps(a.mVal, b.mVal); }
    friend SIMDType operator > (const SIMDType& a, const SIMDType& b) { return _mm_cmplt_ps(a.mVal, b.mVal); }
    friend SIMDType operator < (const SIMDType& a, const SIMDType& b) { return _mm_cmpgt_ps(a.mVal, b.mVal); }
    friend SIMDType operator >= (const SIMDType& a, const SIMDType& b) { return _mm_cmple_ps(a.mVal, b.mVal); }
    friend SIMDType operator <= (const SIMDType& a, const SIMDType& b) { return _mm_cmpge_ps(a.mVal, b.mVal); }

    template <int z, int y, int x, int w> static SIMDType shuffle(const SIMDType& a, const SIMDType& b)
    {
        return _mm_shuffle_ps(a.mVal, b.mVal, ((z<<6)|(y<<4)|(x<<2)|w));
    }
    
#if (SIMD_COMPILER_SUPPORT_LEVEL >= SIMD_COMPILER_SUPPORT_AVX256)
    operator AVX256Double() { return _mm256_cvtps_pd(mVal); }
#endif
    
    operator SizedVector<4, SSEDouble>()
    {
        SizedVector<4, SSEDouble> vec;
        
        vec.mData[0] = _mm_cvtps_pd(mVal);
        vec.mData[1] = _mm_cvtps_pd(_mm_movehl_ps(mVal, mVal));
        
        return vec;
    }
};

template<>
struct SIMDType<int32_t, 4> : public SIMDVector<int32_t, __m128i, 4>
{
    SIMDType() {}
    SIMDType(const int32_t& a) { mVal = _mm_set1_epi32(a); }
    SIMDType(const int32_t* a) { mVal = _mm_loadu_si128(reinterpret_cast<const __m128i *>(a)); }
    SIMDType(__m128i a) : SIMDVector(a) {}
    
    void store(int32_t *a) { _mm_storeu_si128(reinterpret_cast<__m128i *>(a), mVal); }

    friend SIMDType operator + (const SIMDType& a, const SIMDType& b) { return _mm_add_epi32(a.mVal, b.mVal); }
    friend SIMDType operator - (const SIMDType& a, const SIMDType& b) { return _mm_sub_epi32(a.mVal, b.mVal); }
    friend SIMDType operator * (const SIMDType& a, const SIMDType& b) { return _mm_mul_epi32(a.mVal, b.mVal); }
    
    SIMDType& operator += (const SIMDType& b)   { return (*this = *this + b); }
    SIMDType& operator -= (const SIMDType& b)   { return (*this = *this - b); }
    SIMDType& operator *= (const SIMDType& b)   { return (*this = *this * b); }
    
    friend SIMDType min(const SIMDType& a, const SIMDType& b) { return _mm_min_epi32(a.mVal, b.mVal); }
    friend SIMDType max(const SIMDType& a, const SIMDType& b) { return _mm_max_epi32(a.mVal, b.mVal); }

    operator SSEFloat()     { return SSEFloat( _mm_cvtepi32_ps(mVal)); }
    
#if (SIMD_COMPILER_SUPPORT_LEVEL >= SIMD_COMPILER_SUPPORT_AVX256)
    operator AVX256Double() { return _mm256_cvtepi32_pd(mVal); }
#endif
    
    operator SizedVector<4, SSEDouble>()
    {
        SizedVector<4, SSEDouble> vec;
        
        vec.mData[0] = _mm_cvtepi32_pd(mVal);
        vec.mData[1] = _mm_cvtepi32_pd(_mm_shuffle_epi32(mVal, 0xE));
        
        return vec;
    }
};

#endif

#if (SIMD_COMPILER_SUPPORT_LEVEL >= SIMD_COMPILER_SUPPORT_AVX256)

template<>
struct SIMDType<float, 8> : public SIMDVector<float, __m256, 8>
{
    SIMDType() {}
    SIMDType(const float& a) { mVal = _mm256_set1_ps(a); }
    SIMDType(const float* a) { mVal = _mm256_loadu_ps(a); }
    SIMDType(__m256 a) : SIMDVector(a) {}
    
    void store(float *a) { _mm256_storeu_ps(a, mVal); }

    friend SIMDType operator + (const SIMDType &a, const SIMDType &b) { return _mm256_add_ps(a.mVal, b.mVal); }
    friend SIMDType operator - (const SIMDType &a, const SIMDType &b) { return _mm256_sub_ps(a.mVal, b.mVal); }
    friend SIMDType operator * (const SIMDType &a, const SIMDType &b) { return _mm256_mul_ps(a.mVal, b.mVal); }
    friend SIMDType operator / (const SIMDType &a, const SIMDType &b) { return _mm256_div_ps(a.mVal, b.mVal); }
    
    SIMDType& operator += (const SIMDType& b)   { return (*this = *this + b); }
    SIMDType& operator -= (const SIMDType& b)   { return (*this = *this - b); }
    SIMDType& operator *= (const SIMDType& b)   { return (*this = *this * b); }
    SIMDType& operator /= (const SIMDType& b)   { return (*this = *this / b); }
    
    friend SIMDType sqrt(const SIMDType& a) { return _mm256_sqrt_ps(a.mVal); }

    friend SIMDType round(const SIMDType& a) { return _mm256_round_ps(a.mVal, _MM_FROUND_TO_NEAREST_INT |_MM_FROUND_NO_EXC); }
    friend SIMDType trunc(const SIMDType& a) { return _mm256_round_ps(a.mVal, _MM_FROUND_TO_ZERO |_MM_FROUND_NO_EXC); }
    
    friend SIMDType min(const SIMDType& a, const SIMDType& b) { return _mm256_min_ps(a.mVal, b.mVal); }
    friend SIMDType max(const SIMDType& a, const SIMDType& b) { return _mm256_max_ps(a.mVal, b.mVal); }
    friend SIMDType sel(const SIMDType& a, const SIMDType& b, const SIMDType& c) { return and_not(c, a) | (b & c); }

    friend SIMDType and_not(const SIMDType& a, const SIMDType& b) { return _mm256_andnot_ps(a.mVal, b.mVal); }
    friend SIMDType operator & (const SIMDType& a, const SIMDType& b) { return _mm256_and_ps(a.mVal, b.mVal); }
    friend SIMDType operator | (const SIMDType& a, const SIMDType& b) { return _mm256_or_ps(a.mVal, b.mVal); }
    friend SIMDType operator ^ (const SIMDType& a, const SIMDType& b) { return _mm256_xor_ps(a.mVal, b.mVal); }
    
    friend SIMDType operator == (const SIMDType& a, const SIMDType& b) { return _mm256_cmp_ps(a.mVal, b.mVal, _CMP_EQ_OQ); }
    friend SIMDType operator != (const SIMDType& a, const SIMDType& b) { return _mm256_cmp_ps(a.mVal, b.mVal, _CMP_NEQ_OQ); }
    friend SIMDType operator > (const SIMDType& a, const SIMDType& b) { return _mm256_cmp_ps(a.mVal, b.mVal, _CMP_GT_OQ); }
    friend SIMDType operator < (const SIMDType& a, const SIMDType& b) { return _mm256_cmp_ps(a.mVal, b.mVal, _CMP_LT_OQ); }
    friend SIMDType operator >= (const SIMDType& a, const SIMDType& b) { return _mm256_cmp_ps(a.mVal, b.mVal, _CMP_GE_OQ); }
    friend SIMDType operator <= (const SIMDType& a, const SIMDType& b) { return _mm256_cmp_ps(a.mVal, b.mVal, _CMP_LE_OQ); }
    
    operator SizedVector<8, AVX256Double>()
    {
        SizedVector<8, AVX256Double> vec;
        
        vec.mData[0] = _mm256_cvtps_pd(_mm256_extractf128_ps(mVal, 0));
        vec.mData[1] = _mm256_cvtps_pd(_mm256_extractf128_ps(mVal, 1));
        
        return vec;
    }
};

/* This requires AVX2
 
template<>
struct SIMDType<int32_t, 8> : public SIMDVector<int32_t, __m256i, 8>
{
    SIMDType() {}
    SIMDType(const int32_t& a) { mVal = _mm256_set1_epi32(a); }
    SIMDType(const int32_t* a) { mVal = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(a)); }
    SIMDType(__m256i a) : SIMDVector(a) {}
 
    void store(int32_t *a) { _mm256_storeu_epi32(a, mVal); }

    friend SIMDType operator + (const SIMDType& a, const SIMDType& b) { return _mm256_add_epi32(a.mVal, b.mVal); }
    friend SIMDType operator - (const SIMDType& a, const SIMDType& b) { return _mm256_sub_epi32(a.mVal, b.mVal); }
    friend SIMDType operator * (const SIMDType& a, const SIMDType& b) { return _mm256_mul_epi32(a.mVal, b.mVal); }
 
    SIMDType& operator += (const SIMDType& b)   { return (*this = *this + b); }
    SIMDType& operator -= (const SIMDType& b)   { return (*this = *this - b); }
    SIMDType& operator *= (const SIMDType& b)   { return (*this = *this * b); }
 
    friend SIMDType min(const SIMDType& a, const SIMDType& b) { return _mm256_min_epi32(a.mVal, b.mVal); }
    friend SIMDType max(const SIMDType& a, const SIMDType& b) { return _mm256_max_epi32(a.mVal, b.mVal); }

    operator AVX256Float() { return AVX256Float( _mm256_cvtepi32_ps(mVal)); }
    
    operator SizedVector<8, AVX256Double>()
    {
        SizedVector<8, AVX256Double> vec;
        
        vec.mData[0] = _mm256_cvtepi32_pd(_mm256_extractf128_si256(mVal, 0));
        vec.mData[1] = _mm256_cvtepi32_pd(_mm256_extractf128_si256(mVal, 1));
        
        return vec;
    }
};
*/

#endif

#if (SIMD_COMPILER_SUPPORT_LEVEL >= SIMD_COMPILER_SUPPORT_AVX512)

typedef SIMDType<double, 8> AVX512Double;
typedef SIMDType<float, 16> AVX512Float;

template<>
struct SIMDType<double, 8> : public SIMDVector<double, __m512d, 8>
{
    SIMDType() {}
    SIMDType(const double& a) { mVal = _mm512_set1_pd(a); }
    SIMDType(const double* a) { mVal = _mm512_loadu_pd(a); }
    SIMDType(__m512d a) : SIMDVector(a) {}
    
    void store(double *a) { _mm512_storeu_pd(a, mVal); }

    friend SIMDType operator + (const SIMDType &a, const SIMDType &b) { return _mm512_add_pd(a.mVal, b.mVal); }
    friend SIMDType operator - (const SIMDType &a, const SIMDType &b) { return _mm512_sub_pd(a.mVal, b.mVal); }
    friend SIMDType operator * (const SIMDType &a, const SIMDType &b) { return _mm512_mul_pd(a.mVal, b.mVal); }
    friend SIMDType operator / (const SIMDType &a, const SIMDType &b) { return _mm512_div_pd(a.mVal, b.mVal); }
    
    SIMDType& operator += (const SIMDType& b)   { return (*this = *this + b); }
    SIMDType& operator -= (const SIMDType& b)   { return (*this = *this - b); }
    SIMDType& operator *= (const SIMDType& b)   { return (*this = *this * b); }
    SIMDType& operator /= (const SIMDType& b)   { return (*this = *this / b); }
    
    friend SIMDType sqrt(const SIMDType& a) { return _mm512_sqrt_pd(a.mVal); }
    
    friend SIMDType round(const SIMDType& a) { return _mm512_round_pd(a.mVal, _MM_FROUND_TO_NEAREST_INT |_MM_FROUND_NO_EXC); }
    friend SIMDType trunc(const SIMDType& a) { return _mm512_round_pd(a.mVal, _MM_FROUND_TO_ZERO |_MM_FROUND_NO_EXC); }
    
    friend SIMDType min(const SIMDType& a, const SIMDType& b) { return _mm512_min_pd(a.mVal, b.mVal); }
    friend SIMDType max(const SIMDType& a, const SIMDType& b) { return _mm512_max_pd(a.mVal, b.mVal); }
    friend SIMDType sel(const SIMDType& a, const SIMDType& b, const SIMDType& c) { return and_not(c, a) | (b & c); }

    friend SIMDType and_not(const SIMDType& a, const SIMDType& b) { return _mm512_andnot_pd(a.mVal, b.mVal); }
    friend SIMDType operator & (const SIMDType& a, const SIMDType& b) { return _mm512_and_pd(a.mVal, b.mVal); }
    friend SIMDType operator | (const SIMDType& a, const SIMDType& b) { return _mm512_or_pd(a.mVal, b.mVal); }
    friend SIMDType operator ^ (const SIMDType& a, const SIMDType& b) { return _mm512_xor_pd(a.mVal, b.mVal); }
    
    friend SIMDType operator == (const SIMDType& a, const SIMDType& b) { return _mm512_cmp_pd_mask(a.mVal, b.mVal, _CMP_EQ_OQ); }
    friend SIMDType operator != (const SIMDType& a, const SIMDType& b) { return _mm512_cmp_pd_mask(a.mVal, b.mVal, _CMP_NEQ_OQ); }
    friend SIMDType operator > (const SIMDType& a, const SIMDType& b) { return _mm512_cmp_pd_mask(a.mVal, b.mVal, _CMP_GT_OQ); }
    friend SIMDType operator < (const SIMDType& a, const SIMDType& b) { return _mm512_cmp_pd_mask(a.mVal, b.mVal, _CMP_LT_OQ); }
    friend SIMDType operator >= (const SIMDType& a, const SIMDType& b) { return _mm512_cmp_pd_mask(a.mVal, b.mVal, _CMP_GE_OQ); }
    friend SIMDType operator <= (const SIMDType& a, const SIMDType& b) { return _mm512_cmp_pd_mask(a.mVal, b.mVal, _CMP_LE_OQ); }
};

template<>
struct SIMDType<float, 16> : public SIMDVector<float, __m512, 16>
{
    SIMDType() {}
    SIMDType(const float& a) { mVal = _mm512_set1_ps(a); }
    SIMDType(const float* a) { mVal = _mm512_loadu_ps(a); }
    SIMDType(__m512 a) : SIMDVector(a) {}
    
    void store(float *a) { _mm512_storeu_ps(a, mVal); }

    friend SIMDType operator + (const SIMDType &a, const SIMDType &b) { return _mm512_add_ps(a.mVal, b.mVal); }
    friend SIMDType operator - (const SIMDType &a, const SIMDType &b) { return _mm512_sub_ps(a.mVal, b.mVal); }
    friend SIMDType operator * (const SIMDType &a, const SIMDType &b) { return _mm512_mul_ps(a.mVal, b.mVal); }
    friend SIMDType operator / (const SIMDType &a, const SIMDType &b) { return _mm512_div_ps(a.mVal, b.mVal); }
    
    SIMDType& operator += (const SIMDType& b)   { return (*this = *this + b); }
    SIMDType& operator -= (const SIMDType& b)   { return (*this = *this - b); }
    SIMDType& operator *= (const SIMDType& b)   { return (*this = *this * b); }
    SIMDType& operator /= (const SIMDType& b)   { return (*this = *this / b); }
    
    friend SIMDType sqrt(const SIMDType& a) { return _mm512_sqrt_ps(a.mVal); }
    
    friend SIMDType round(const SIMDType& a) { return _mm512_round_ps(a.mVal, _MM_FROUND_TO_NEAREST_INT |_MM_FROUND_NO_EXC); }
    friend SIMDType trunc(const SIMDType& a) { return _mm512_round_ps(a.mVal, _MM_FROUND_TO_ZERO |_MM_FROUND_NO_EXC); }
    
    friend SIMDType min(const SIMDType& a, const SIMDType& b) { return _mm512_min_ps(a.mVal, b.mVal); }
    friend SIMDType max(const SIMDType& a, const SIMDType& b) { return _mm512_max_ps(a.mVal, b.mVal); }
    friend SIMDType sel(const SIMDType& a, const SIMDType& b, const SIMDType& c) { return and_not(c, a) | (b & c); }

    friend SIMDType and_not(const SIMDType& a, const SIMDType& b) { return _mm512_andnot_ps(a.mVal, b.mVal); }
    friend SIMDType operator & (const SIMDType& a, const SIMDType& b) { return _mm512_and_ps(a.mVal, b.mVal); }
    friend SIMDType operator | (const SIMDType& a, const SIMDType& b) { return _mm512_or_ps(a.mVal, b.mVal); }
    friend SIMDType operator ^ (const SIMDType& a, const SIMDType& b) { return _mm512_xor_ps(a.mVal, b.mVal); }
    
    friend SIMDType operator == (const SIMDType& a, const SIMDType& b) { return _mm512_cmp_ps_mask(a.mVal, b.mVal, _CMP_EQ_OQ); }
    friend SIMDType operator != (const SIMDType& a, const SIMDType& b) { return _mm512_cmp_ps_mask(a.mVal, b.mVal, _CMP_NEQ_OQ); }
    friend SIMDType operator > (const SIMDType& a, const SIMDType& b) { return _mm512_cmp_ps_mask(a.mVal, b.mVal, _CMP_GT_OQ); }
    friend SIMDType operator < (const SIMDType& a, const SIMDType& b) { return _mm512_cmp_ps_mask(a.mVal, b.mVal, _CMP_LT_OQ); }
    friend SIMDType operator >= (const SIMDType& a, const SIMDType& b) { return _mm512_cmp_ps_mask(a.mVal, b.mVal, _CMP_GE_OQ); }
    friend SIMDType operator <= (const SIMDType& a, const SIMDType& b) { return _mm512_cmp_ps_mask(a.mVal, b.mVal, _CMP_LE_OQ); }
};

#endif

// abs functions

static inline SIMDType<double, 1> abs(const SIMDType<double, 1> a)
{
    const static uint64_t bit_mask_64 = 0x7FFFFFFFFFFFFFFFU;
    
    uint64_t temp = *(reinterpret_cast<const uint64_t *>(&a)) & bit_mask_64;
    return *(reinterpret_cast<double *>(&temp));
}

static inline SIMDType<float, 1> abs(const SIMDType<float, 1> a)
{
    const static uint32_t bit_mask_32 = 0x7FFFFFFFU;
    
    uint32_t temp = *(reinterpret_cast<const uint32_t *>(&a)) & bit_mask_32;
    return *(reinterpret_cast<float *>(&temp));
}

template <int N> SIMDType<double, N> abs(const SIMDType<double, N> a)
{
    const static uint64_t bit_mask_64 = 0x7FFFFFFFFFFFFFFFU;
    const double bit_mask_64d = *(reinterpret_cast<const double *>(&bit_mask_64));

    return a & SIMDType<double, N>(bit_mask_64d);
}

template <int N> SIMDType<float, N> abs(const SIMDType<float, N> a)
{
    const static uint32_t bit_mask_32 = 0x7FFFFFFFU;
    const float bit_mask_32f = *(reinterpret_cast<const double *>(&bit_mask_32));
    
    return a & SIMDType<float, N>(bit_mask_32f);
}

#endif	
