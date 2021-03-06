/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Rubber Band Library
    An audio time-stretching and pitch-shifting library.
    Copyright 2007-2014 Particular Programs Ltd.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.

    Alternatively, if you have a valid commercial licence for the
    Rubber Band Library obtained by agreement with the copyright
    holders, you may redistribute and/or modify it under the terms
    described in that licence.

    If you wish to distribute code using the Rubber Band Library
    under terms other than those of the GNU General Public License,
    you must obtain a valid commercial licence before doing so.
*/

#ifndef _RUBBERBAND_VECTOR_OPS_COMPLEX_H_
#define _RUBBERBAND_VECTOR_OPS_COMPLEX_H_

#include "VectorOps.h"


namespace RubberBand {


template<typename T>
inline void c_phasor(T *real, T *imag, T phase)
{
    //!!! IPP contains ippsSinCos_xxx in ippvm.h -- these are
    //!!! fixed-accuracy, test and compare
#if defined HAVE_VDSP
    int one = 1;
    if (sizeof(T) == sizeof(float)) {
        vvsincosf((float *)imag, (float *)real, (const float *)&phase, &one);
    } else {
        vvsincos((double *)imag, (double *)real, (const double *)&phase, &one);
    }
#elif defined LACK_SINCOS
    if (sizeof(T) == sizeof(float)) {
        *real = cosf(phase);
        *imag = sinf(phase);
    } else {
        *real = cos(phase);
        *imag = sin(phase);
    }
#elif defined __GNUC__
    if (sizeof(T) == sizeof(float)) {
        sincosf(phase, (float *)imag, (float *)real);
    } else {
        sincos(phase, (double *)imag, (double *)real);
    }
#else
    if (sizeof(T) == sizeof(float)) {
        *real = cosf(phase);
        *imag = sinf(phase);
    } else {
        *real = cos(phase);
        *imag = sin(phase);
    }
#endif
}
void
inline v_polar_to_cartesian_pommier(float *const R__ real,
                             float *const R__ imag,
                             const float *const R__ mag,
                             const float *const R__ phase,
                             const int count){
    int i = 0;

    for (int i = 0; i + 4 < count; i += 4) {
	__m128 fmag, fphase, fre, fim;
        fmag = *(__m128*)(mag+i);
        fphase = *(__m128*)(phase+i);
	sincos_ps(fphase, &fim, &fre);
        fim = _mm_mul_ps(fim, fmag);
        fre = _mm_mul_ps(fre,fmag);
        *(__m128*)(real + i ) = fre;
        *(__m128*)(imag + i ) = fim;
    }
    while (i < count) {
        float re, im;
        c_phasor(&re, &im, phase[i]);
        real[i] = re * mag[i];
        imag[i] = im * mag[i];
        ++i;
    }
}    

void
inline v_polar_interleaved_to_cartesian_inplace_pommier(float *const R__ srcdst,
                                                 const int count){
    int i;
    int idx = 0, tidx = 0;
    for (i = 0; i + 4 < count; i += 4) {
	__m128 fmag, fphase, fre, fim;
        for (int j = 0; j < 3; ++j) {
            fmag[j] = srcdst[idx++];
            fphase[j] = srcdst[idx++];
        }
	sincos_ps(fphase, &fim, &fre);
        for (int j = 0; j < 3; ++j) {
            srcdst[tidx++] = fre[j] * fmag[j];
            srcdst[tidx++] = fim[j] * fmag[j];
        }
    }
    while (i < count) {
        float real, imag;
        float mag = srcdst[idx++];
        float phase = srcdst[idx++];
        c_phasor(&real, &imag, phase);
        srcdst[tidx++] = real * mag;
        srcdst[tidx++] = imag * mag;
        ++i;
    }
}    

void
inline v_polar_to_cartesian_interleaved_pommier(float *const R__ dst,
                                         const float *const R__ mag,
                                         const float *const R__ phase,
                                         const int count){
    int i;
    for (i = 0; i + 4 <= count; i += 4) {
	__m128 fmag, fphase, fre, fim;
        fmag = *(__m128*)(mag+i);
        fphase = *(__m128*)(phase+i);
	sincos_ps(fphase, &fim, &fre);
        fim = _mm_mul_ps(fim, fmag);
        fre = _mm_mul_ps(fre,fmag);
        *(__m128*)(dst+(2*i))   = _mm_unpacklo_ps(fre,fim);
        *(__m128*)(dst+(2*i+4)) = _mm_unpackhi_ps(fre,fim);
    }
    while (i < count) {
        float real, imag;
        c_phasor(&real, &imag, phase[i]);
        dst[2*i+0] = real * mag[i];
        dst[2*i+1] = imag * mag[i];
        ++i;
    }
}
#ifdef USE_APPROXIMATE_ATAN2
template<typename T>
inline T approximate_atan2(T  imag, T  real)
{
    T atan;
    if (real == 0) {
        if (imag > 0.0) atan = (M_PI/2);
        else if (imag == 0.0) atan = 0.0;
        else atan = -(M_PI/2);
    } else {
        T  z = imag/real;
        if (std::abs(z) < 1) {
            atan = z / (1 + 0.28 * z * z);
            if (real < 0) {
                if (imag < 0) atan -= M_PI;
                else atan += M_PI;
            }
        } else {
            atan = (M_PI/2) - z / (z * z + 0.28);
            if (imag < 0) atan -= M_PI;
        }
    }
    return atan;
}
#endif
#ifdef USE_APPROXIMATE_ATAN2
template<typename T>
inline void c_magphase(T *mag, T *phase, T real, T imag)
{
    *phase = approximate_atan2(imag,real);
    *mag = std::sqrt(real * real + imag * imag);
}
#else
template<typename T>
inline void c_magphase(T *mag, T *phase, T real, T imag)
{
    *mag = std::sqrt(real * real + imag * imag);
    *phase = atan2(imag, real);
}
#endif


template<typename S, typename T> // S source, T target
void v_polar_to_cartesian(T *const R__ real,
                          T *const R__ imag,
                          const S *const R__ mag,
                          const S *const R__ phase,
                          const int count)
{
    for (int i = 0; i < count; ++i) {c_phasor<T>(real + i, imag + i, phase[i]);}
    v_multiply(real, mag, count);
    v_multiply(imag, mag, count);
}

template<typename T>
void v_polar_interleaved_to_cartesian_inplace(T *const R__ srcdst,
                                              const int count)
{
    T real, imag;
    for (int i = 0; i < count*2; i += 2) {
        c_phasor(&real, &imag, srcdst[i+1]);
        real *= srcdst[i];
        imag *= srcdst[i];
        srcdst[i] = real;
        srcdst[i+1] = imag;
    }
}

template<typename S, typename T> // S source, T target
void v_polar_to_cartesian_interleaved(T *const R__ dst,
                                      const S *const R__ mag,
                                      const S *const R__ phase,
                                      const int count)
{
    T real, imag;
    for (int i = 0; i < count; ++i) {
        c_phasor<T>(&real, &imag, phase[i]);
        real *= mag[i];
        imag *= mag[i];
        dst[i*2] = real;
        dst[i*2+1] = imag;
    }
}    

#if defined USE_POMMIER_MATHFUN
template<>
inline void v_polar_to_cartesian(float *const R__ real,
                                 float *const R__ imag,
                                 const float *const R__ mag,
                                 const float *const R__ phase,
                                 const int count)
{
    v_polar_to_cartesian_pommier(real, imag, mag, phase, count);
}
template<>
inline void v_polar_interleaved_to_cartesian_inplace(float *const R__ srcdst,
                                                     const int count)
{
    v_polar_interleaved_to_cartesian_inplace_pommier(srcdst, count);
}

template<>
inline void v_polar_to_cartesian_interleaved(float *const R__ dst,
                                             const float *const R__ mag,
                                             const float *const R__ phase,
                                             const int count)
{
    v_polar_to_cartesian_interleaved_pommier(dst, mag, phase, count);
}

#endif

template<typename S, typename T> // S source, T target
void v_cartesian_to_polar(T *const R__ mag,
                          T *const R__ phase,
                          const S *const R__ real,
                          const S *const R__ imag,
                          const int count)
{
    for (int i = 0; i < count; ++i) {
        c_magphase<T>(mag + i, phase + i, real[i], imag[i]);
    }
}

template<typename S, typename T> // S source, T target
void v_cartesian_interleaved_to_polar(T *const R__ mag,
                                      T *const R__ phase,
                                      const S *const R__ src,
                                      const int count)
{
    for (int i = 0; i < count; ++i) {
        c_magphase<T>(mag + i, phase + i, src[i*2], src[i*2+1]);
    }
}
#ifdef HAVE_VDSP
template<>
inline void v_cartesian_to_polar(float *const R__ mag,
                                 float *const R__ phase,
                                 const float *const R__ real,
                                 const float *const R__ imag,
                                 const int count)
{
    DSPSplitComplex c;
    c.realp = const_cast<float *>(real);
    c.imagp = const_cast<float *>(imag);
    vDSP_zvmags(&c, 1, phase, 1, count); // using phase as a temporary dest
    vvsqrtf(mag, phase, &count); // using phase as the source
    vvatan2f(phase, imag, real, &count);
}
template<>
inline void v_cartesian_to_polar(double *const R__ mag,
                                 double *const R__ phase,
                                 const double *const R__ real,
                                 const double *const R__ imag,
                                 const int count)
{
    // double precision, this is significantly faster than using vDSP_polar
    DSPDoubleSplitComplex c;
    c.realp = const_cast<double *>(real);
    c.imagp = const_cast<double *>(imag);
    vDSP_zvmagsD(&c, 1, phase, 1, count); // using phase as a temporary dest
    vvsqrt(mag, phase, &count); // using phase as the source
    vvatan2(phase, imag, real, &count);
}
#endif

template<typename T>
void v_cartesian_to_polar_interleaved_inplace(T *const R__ srcdst,
                                              const int count)
{
    T mag, phase;
    for (int i = 0; i < count * 2; i += 2) {
        c_magphase(&mag, &phase, srcdst[i], srcdst[i+1]);
        srcdst[i] = mag;
        srcdst[i+1] = phase;
    }
}
template<>
inline void v_cartesian_to_polar(float *const R__ mag,
                                 float *const R__ phase,
                                 const float *const R__ real,
                                 const float *const R__ imag,
                                 const int count)
{
    int i;
    for(i=0;i+4<count;i+=4){
        _approx_magphase_ps ( (v4sf*)(mag + i), (v4sf*)(phase+i),
                *(v4sf*)(real+i),*(v4sf*)(imag+i));
    }
    while(i<count){
        c_magphase(mag+i, phase+i, real[i],imag[i]);
    }
}

}

#endif

