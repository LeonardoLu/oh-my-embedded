#pragma once

#include <math.h>
#include <stddef.h>

namespace watchinput {

struct TouchAffinePoint {
    float rawX;
    float rawY;
    float targetX;
    float targetY;
};

struct TouchAffineError {
    float rms;
    float maximum;
};

struct TouchAffineResult {
    // M5GFX order: x'=a*x+b*y+c, y'=d*x+e*y+f.
    float coefficients[6];
    TouchAffineError trainingError;
};

inline bool touchAffineError(const float coefficients[6],
                             const TouchAffinePoint* points,
                             size_t count,
                             TouchAffineError* output) {
    if (!coefficients || !points || !count || !output) return false;
    for (size_t i=0;i<6;++i) {
        if (!isfinite(coefficients[i])) return false;
    }

    double squaredError=0.0;
    double maximumSquared=0.0;
    for (size_t i=0;i<count;++i) {
        const auto& p=points[i];
        if (!isfinite(p.rawX)||!isfinite(p.rawY)||
           !isfinite(p.targetX)||!isfinite(p.targetY)) return false;
        double x=(double)coefficients[0]*p.rawX+(double)coefficients[1]*p.rawY+coefficients[2];
        double y=(double)coefficients[3]*p.rawX+(double)coefficients[4]*p.rawY+coefficients[5];
        double dx=x-p.targetX,dy=y-p.targetY;
        double e2=dx*dx+dy*dy;
        if (!isfinite(e2)) return false;
        squaredError+=e2;
        if (e2>maximumSquared) maximumSquared=e2;
    }
    TouchAffineError result{(float)sqrt(squaredError/count),(float)sqrt(maximumSquared)};
    if (!isfinite(result.rms)||!isfinite(result.maximum)) return false;
    *output=result;
    return true;
}

inline bool solveTouchAffine(const TouchAffinePoint* points,
                             size_t count,
                             TouchAffineResult* output) {
    if (!points || count<3 || !output) return false;

    double rawMeanX=0.0,rawMeanY=0.0,targetMeanX=0.0,targetMeanY=0.0;
    for (size_t i=0;i<count;++i) {
        const auto& p=points[i];
        if (!isfinite(p.rawX)||!isfinite(p.rawY)||
           !isfinite(p.targetX)||!isfinite(p.targetY)) return false;
        rawMeanX+=p.rawX;
        rawMeanY+=p.rawY;
        targetMeanX+=p.targetX;
        targetMeanY+=p.targetY;
    }
    rawMeanX/=count;
    rawMeanY/=count;
    targetMeanX/=count;
    targetMeanY/=count;

    double xx=0.0,xy=0.0,yy=0.0;
    double targetXRawX=0.0,targetXRawY=0.0;
    double targetYRawX=0.0,targetYRawY=0.0;
    for (size_t i=0;i<count;++i) {
        double x=points[i].rawX-rawMeanX;
        double y=points[i].rawY-rawMeanY;
        double u=points[i].targetX-targetMeanX;
        double v=points[i].targetY-targetMeanY;
        xx+=x*x;
        xy+=x*y;
        yy+=y*y;
        targetXRawX+=u*x;
        targetXRawY+=u*y;
        targetYRawX+=v*x;
        targetYRawY+=v*y;
    }

    double determinant=xx*yy-xy*xy;
    double trace=xx+yy;
    // The smallest covariance eigenvalue must carry meaningful 2D spread.
    // This rejects collinear and numerically one-dimensional samples.
    if (!isfinite(determinant)||!isfinite(trace)||trace<=0.0||
        determinant<=trace*trace*1e-10) return false;

    double inverse=1.0/determinant;
    double a=(targetXRawX*yy-targetXRawY*xy)*inverse;
    double b=(targetXRawY*xx-targetXRawX*xy)*inverse;
    double d=(targetYRawX*yy-targetYRawY*xy)*inverse;
    double e=(targetYRawY*xx-targetYRawX*xy)*inverse;
    double c=targetMeanX-a*rawMeanX-b*rawMeanY;
    double f=targetMeanY-d*rawMeanX-e*rawMeanY;
    double solved[6]={a,b,c,d,e,f};
    TouchAffineResult result{};
    for (size_t i=0;i<6;++i) {
        if (!isfinite(solved[i])) return false;
        result.coefficients[i]=(float)solved[i];
        if (!isfinite(result.coefficients[i])) return false;
    }
    if (!touchAffineError(result.coefficients,points,count,&result.trainingError)) return false;
    *output=result;
    return true;
}

} // namespace watchinput
