#include "TouchAffine.h"

#include <assert.h>
#include <math.h>
#include <stddef.h>

using watchinput::TouchAffineError;
using watchinput::TouchAffinePoint;
using watchinput::TouchAffineResult;

static bool near(float actual,float expected,float tolerance=0.001f) {
    return fabsf(actual-expected)<=tolerance;
}

template<size_t N>
static TouchAffineResult solve(const TouchAffinePoint (&points)[N]) {
    TouchAffineResult result{};
    assert(watchinput::solveTouchAffine(points,N,&result));
    return result;
}

static TouchAffinePoint mapped(float x,float y,const float c[6]) {
    return {x,y,c[0]*x+c[1]*y+c[2],c[3]*x+c[4]*y+c[5]};
}

static void expectCoefficients(const TouchAffineResult& result,const float expected[6],float tolerance=0.001f) {
    for(size_t i=0;i<6;++i) assert(near(result.coefficients[i],expected[i],tolerance));
    assert(result.trainingError.rms<=tolerance);
    assert(result.trainingError.maximum<=tolerance);
}

int main() {
    {
        const float expected[6]={1,0,0,0,1,0};
        TouchAffinePoint points[]={mapped(0,0,expected),mapped(467,0,expected),
                                   mapped(0,467,expected)};
        expectCoefficients(solve(points),expected);
    }
    {
        const float expected[6]={1,0,17,0,1,-23};
        TouchAffinePoint points[]={mapped(15,21,expected),mapped(451,18,expected),
                                   mapped(24,446,expected),mapped(438,452,expected)};
        auto result=solve(points);
        expectCoefficients(result,expected);
        TouchAffinePoint holdout[]={mapped(233,233,expected),mapped(91,379,expected)};
        TouchAffineError error{};
        assert(watchinput::touchAffineError(result.coefficients,holdout,2,&error));
        assert(error.rms<0.001f&&error.maximum<0.001f);
    }
    {
        const float expected[6]={1.08f,0,4.5f,0,0.92f,12.25f};
        TouchAffinePoint points[]={mapped(0,0,expected),mapped(467,2,expected),
                                   mapped(3,465,expected),mapped(464,467,expected),
                                   mapped(205,311,expected)};
        expectCoefficients(solve(points),expected);
    }
    {
        // Rotation, shear and translation, trained away from the holdout points.
        const float expected[6]={0.25f,-0.95f,421.0f,1.05f,0.18f,-16.0f};
        TouchAffinePoint points[]={mapped(12,34,expected),mapped(440,19,expected),
                                   mapped(29,453,expected),mapped(421,444,expected),
                                   mapped(171,288,expected)};
        auto result=solve(points);
        expectCoefficients(result,expected,0.002f);
        TouchAffinePoint holdout[]={mapped(233,233,expected),mapped(0,467,expected),
                                    mapped(467,0,expected),mapped(107,359,expected)};
        TouchAffineError error{};
        assert(watchinput::touchAffineError(result.coefficients,holdout,4,&error));
        assert(error.rms<0.002f&&error.maximum<0.003f);
    }
    {
        // Realistic pixel range with fractional coefficients stays sub-pixel.
        const float expected[6]={0.9987f,0.0124f,-3.75f,-0.0081f,1.0042f,6.125f};
        TouchAffinePoint points[]={mapped(1,2,expected),mapped(466,3,expected),
                                   mapped(2,466,expected),mapped(466,466,expected),
                                   mapped(77,312,expected),mapped(389,144,expected)};
        auto result=solve(points);
        TouchAffinePoint holdout[]={mapped(233,233,expected),mapped(350,420,expected)};
        TouchAffineError error{};
        assert(watchinput::touchAffineError(result.coefficients,holdout,2,&error));
        assert(error.maximum<0.001f);
    }
    {
        // An overdetermined calibration remains close under bounded measurement noise.
        const float expected[6]={1.012f,-0.018f,3.5f,0.009f,0.991f,-4.25f};
        TouchAffinePoint points[]={mapped(8,12,expected),mapped(455,7,expected),
                                   mapped(4,459,expected),mapped(462,452,expected),
                                   mapped(112,318,expected),mapped(371,155,expected),
                                   mapped(228,241,expected),mapped(61,189,expected)};
        const float noise[][2]={{0.25f,-0.20f},{-0.30f,0.15f},{0.10f,0.30f},{-0.15f,-0.25f},
                                {0.35f,0.05f},{-0.20f,-0.10f},{0.05f,0.25f},{-0.10f,-0.15f}};
        for(size_t i=0;i<sizeof(points)/sizeof(points[0]);++i) {
            points[i].targetX+=noise[i][0];
            points[i].targetY+=noise[i][1];
        }
        auto result=solve(points);
        assert(near(result.coefficients[0],expected[0],0.002f));
        assert(near(result.coefficients[1],expected[1],0.002f));
        assert(near(result.coefficients[2],expected[2],0.25f));
        assert(near(result.coefficients[3],expected[3],0.002f));
        assert(near(result.coefficients[4],expected[4],0.002f));
        assert(near(result.coefficients[5],expected[5],0.25f));
        assert(result.trainingError.rms<0.4f&&result.trainingError.maximum<0.5f);
    }
    {
        // Holdout evaluation reports a different geometry and never refits coefficients.
        const float expected[6]={0.98f,0.03f,7,-0.02f,1.01f,-5};
        TouchAffinePoint training[]={mapped(0,0,expected),mapped(467,0,expected),
                                     mapped(0,467,expected),mapped(467,467,expected)};
        auto result=solve(training);
        float before[6];
        for(size_t i=0;i<6;++i) before[i]=result.coefficients[i];
        TouchAffinePoint holdout[]={mapped(233,233,expected),mapped(120,350,expected)};
        for(auto& point:holdout) { point.targetX+=8; point.targetY-=6; }
        TouchAffineError error{};
        assert(watchinput::touchAffineError(result.coefficients,holdout,2,&error));
        assert(near(error.rms,10)&&near(error.maximum,10));
        for(size_t i=0;i<6;++i) assert(result.coefficients[i]==before[i]);
    }
    {
        float identity[6]={1,0,0,0,1,0};
        TouchAffinePoint points[]={{10,20,13,24},{100,200,103,204}};
        TouchAffineError error{};
        assert(watchinput::touchAffineError(identity,points,2,&error));
        assert(near(error.rms,5)&&near(error.maximum,5));
    }
    {
        TouchAffineResult sentinel{{9,8,7,6,5,4},{3,2}};
        TouchAffinePoint collinear[]={{0,0,2,3},{100,100,4,5},{200,200,6,7},{300,300,8,9}};
        assert(!watchinput::solveTouchAffine(collinear,4,&sentinel));
        assert(sentinel.coefficients[0]==9&&sentinel.trainingError.rms==3);

        TouchAffinePoint repeated[]={{42,42,1,2},{42,42,3,4},{42,42,5,6}};
        assert(!watchinput::solveTouchAffine(repeated,3,&sentinel));

        TouchAffinePoint illConditioned[]={{0,0,0,0},{467,0,467,0},
                                           {100,0.001f,100,0.001f},{350,-0.001f,350,-0.001f}};
        assert(!watchinput::solveTouchAffine(illConditioned,4,&sentinel));
    }
    {
        TouchAffineResult sentinel{{9,8,7,6,5,4},{3,2}};
        TouchAffinePoint invalid[]={{0,0,0,0},{1,0,1,0},{0,1,NAN,1}};
        assert(!watchinput::solveTouchAffine(invalid,3,&sentinel));
        assert(sentinel.coefficients[0]==9&&sentinel.trainingError.maximum==2);
        float bad[6]={1,0,0,0,INFINITY,0};
        TouchAffinePoint holdout[]={{0,0,0,0}};
        TouchAffineError error{7,8};
        assert(!watchinput::touchAffineError(bad,holdout,1,&error));
        assert(error.rms==7&&error.maximum==8);
        assert(!watchinput::solveTouchAffine(holdout,1,&sentinel));
    }
}
