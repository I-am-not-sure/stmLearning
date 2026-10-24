#include "attitude_math.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/* 用四元数产生独立的理想姿态/向量，验证被测模块的矩阵换算。 */
typedef struct { double w, x, y, z; } Q;
static const double PI = 3.14159265358979323846;
static const float bias[3] = {-0.66f, 0.42f, 0.16f};

static Q product(Q a, Q b)
{
    Q result = {a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z,
        a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,
        a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
        a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w};
    return result;
}

static Q axis(int which, double degrees)
{
    double half = degrees * PI / 360.0;
    Q result = {cos(half), 0, 0, 0};
    if (which == 0) result.x = sin(half);
    if (which == 1) result.y = sin(half);
    if (which == 2) result.z = sin(half);
    return result;
}

static Q inverse(Q q)
{
    Q result = {q.w, -q.x, -q.y, -q.z};
    return result;
}

static void rotate(Q q, const float input[3], float output[3])
{
    Q v = {0, input[0], input[1], input[2]};
    Q r = product(product(q, v), inverse(q));
    output[0] = (float)r.x; output[1] = (float)r.y; output[2] = (float)r.z;
}

static Q mount(double x, double y, double z)
{
    return product(product(axis(2,z), axis(1,y)), axis(0,x));
}

static AttitudeSample sample(double front, double side, double yaw,
    Q installation, double initial_heading, const float body_rate[3])
{
    Q body = product(product(axis(2,yaw), axis(0,front)), axis(1,side));
    Q orientation = product(product(axis(2,initial_heading), body), installation);
    AttitudeSample result;
    result.angles_deg[0] = (float)(atan2(2*(orientation.w*orientation.x+orientation.y*orientation.z),
        1-2*(orientation.x*orientation.x+orientation.y*orientation.y))*180/PI);
    result.angles_deg[1] = (float)(asin(2*(orientation.w*orientation.y-orientation.z*orientation.x))*180/PI);
    result.angles_deg[2] = (float)(atan2(2*(orientation.w*orientation.z+orientation.x*orientation.y),
        1-2*(orientation.y*orientation.y+orientation.z*orientation.z))*180/PI);
    const float up[3] = {0,0,1};
    rotate(inverse(orientation), up, result.acc_g);
    rotate(inverse(installation), body_rate, result.gyro_deg_s);
    for (int i=0; i<3; ++i) result.gyro_deg_s[i] += bias[i];
    return result;
}

static void near(float actual, float expected, float tolerance)
{
    if (fabsf(actual - expected) > tolerance)
    {
        fprintf(stderr, "actual %.7f expected %.7f tolerance %.7f\n", actual, expected, tolerance);
        assert(0);
    }
}

static void calibrate(AttitudeMath *ctx, Q installation, float heading,
                      float installation_yaw, uint32_t first_tick)
{
    const float stopped[3] = {0,0,0};
    AttitudeSample raw = sample(0,0,0,installation,heading,stopped);
    AttitudeBody output = {0};
    AttitudeMath_Init(ctx, installation_yaw);
    for (uint32_t i=0; i<10; ++i)
    {
        AttitudeMathResult status = AttitudeMath_Process(ctx,&raw,first_tick+i*100U,&output);
        assert(status == (i==9?ATTITUDE_MATH_READY:ATTITUDE_MATH_CALIBRATING));
    }
    assert(ctx->ready && ctx->calibration_count==10);
    near(output.front_deg,0,0.001f);
    near(output.side_deg,0,0.001f);
    near(output.yaw_deg,0,0.001f);
    for(int i=0;i<3;++i) near(ctx->gyro_bias_deg_s[i],bias[i],0.00001f);
}

static void test_known_twenty_degree_case(void)
{
    AttitudeMath ctx;
    Q installation = mount(0,-10,0);
    calibrate(&ctx,installation,0,0,0);
    AttitudeSample raw = {{20.283559f,-9.391286f,-3.451178f},
                          {-0.66f,0.42f,0.16f},{0,0,1}};
    AttitudeBody body;
    assert(AttitudeMath_Process(&ctx,&raw,1000,&body)==ATTITUDE_MATH_READY);
    near(body.front_deg,20,0.001f);
    near(body.side_deg,0,0.001f);
    near(body.yaw_deg,0,0.001f);
}

static void test_full_mount_and_combined_motion(void)
{
    const float stopped[3] = {0,0,0};
    const float cases[][3] = {{20,0,0},{0,15,0},{0,0,90},{-25,-18,-120},{20,15,60}};
    const float offsets[] = {0,23,-37};
    for(unsigned j=0;j<sizeof(offsets)/sizeof(offsets[0]);++j)
    {
        Q installation=mount(0.9,-10.4,offsets[j]);
        AttitudeMath ctx;calibrate(&ctx,installation,31,offsets[j],0);
        for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);++i)
        {
            AttitudeSample raw=sample(cases[i][0],cases[i][1],cases[i][2],installation,31,stopped);
            AttitudeBody body;
            assert(AttitudeMath_Process(&ctx,&raw,1100+i*100,&body)==ATTITUDE_MATH_READY);
            near(body.front_deg,cases[i][0],0.002f);
            near(body.side_deg,cases[i][1],0.002f);
            near(body.yaw_deg,cases[i][2],0.002f);
        }
        const float moving[3]={1.2f,-0.7f,0.5f};
        AttitudeSample raw=sample(20,0,0,installation,31,moving);
        AttitudeBody body;
        assert(AttitudeMath_Process(&ctx,&raw,3000,&body)==ATTITUDE_MATH_READY);
        /* RATE只做坐标旋转，保留原始读数中的bias，不扣初始平均值。 */
        float body_bias[3];rotate(installation,bias,body_bias);
        for(int i=0;i<3;++i)near(body.gyro_deg_s[i],moving[i]+body_bias[i],0.00001f);
        near(body.acc_g[0],0,0.00001f);
        near(body.acc_g[1],(float)sin(20*PI/180),0.00001f);
        near(body.acc_g[2],(float)cos(20*PI/180),0.00001f);
    }
}

/* 零安装角时RATE应与本帧原始GYRO相等，包括第10次刚完成标定的输出。
 * 第10帧故意与前9帧不同，同时验证初始均值仍统计、但不参与扣除。 */
static void test_rate_keeps_initial_average(void)
{
    const float stopped[3]={0,0,0};
    const float last_gyro[3]={3.25f,-1.5f,0.75f};
    const float moving[3]={5.0f,-3.0f,8.0f};
    Q installation=mount(0,0,0);
    AttitudeSample raw=sample(0,0,0,installation,0,stopped);
    AttitudeMath ctx;AttitudeMath_Init(&ctx,0);
    AttitudeBody body;memset(&body,0x5a,sizeof(body));
    AttitudeBody sentinel=body;
    for(uint32_t i=0;i<9;++i)
    {
        assert(AttitudeMath_Process(&ctx,&raw,i*100,&body)==ATTITUDE_MATH_CALIBRATING);
        assert(memcmp(&body,&sentinel,sizeof(body))==0);
    }
    memcpy(raw.gyro_deg_s,last_gyro,sizeof(last_gyro));
    assert(AttitudeMath_Process(&ctx,&raw,900,&body)==ATTITUDE_MATH_READY);
    assert(ctx.ready && ctx.calibration_count==10);
    float initial_average[3];
    for(int i=0;i<3;++i)
    {
        initial_average[i]=(9.0f*bias[i]+last_gyro[i])/10.0f;
        near(ctx.gyro_bias_deg_s[i],initial_average[i],0.00001f);
        near(body.gyro_deg_s[i],last_gyro[i],0.00001f);
    }
    raw=sample(0,0,0,installation,0,moving);
    assert(AttitudeMath_Process(&ctx,&raw,1000,&body)==ATTITUDE_MATH_READY);
    for(int i=0;i<3;++i)
    {
        near(body.gyro_deg_s[i],raw.gyro_deg_s[i],0.00001f);
        near(ctx.gyro_bias_deg_s[i],initial_average[i],0.00001f);
    }
}

static void test_spacing_average_and_clock_wrap(void)
{
    const float stopped[3]={0,0,0};
    AttitudeSample raw=sample(0,0,0,mount(0.9,-10.4,0),2,stopped);
    AttitudeMath ctx;AttitudeMath_Init(&ctx,0);
    AttitudeBody output;memset(&output,0x5a,sizeof(output));
    AttitudeBody sentinel=output;
    assert(AttitudeMath_Process(&ctx,&raw,0,&output)==ATTITUDE_MATH_CALIBRATING);
    for(uint32_t t=1;t<100;++t)AttitudeMath_Process(&ctx,&raw,t,&output);
    assert(ctx.calibration_count==1);
    assert(memcmp(&output,&sentinel,sizeof(output))==0);
    for(uint32_t i=1;i<10;++i)
    {
        raw.angles_deg[0]=0.9f+(i%2?0.1f:-0.1f);
        raw.gyro_deg_s[0]=bias[0]+(i%2?0.02f:-0.02f);
        AttitudeMath_Process(&ctx,&raw,i*100,&output);
    }
    near(ctx.reference_angles_deg[0],0.91f,0.0001f);
    near(ctx.gyro_bias_deg_s[0],bias[0]+0.002f,0.00001f);
    assert(ctx.ready);
    calibrate(&ctx,mount(0.9,-10.4,0),2,0,UINT32_MAX-450U);
}

static void test_heading_boundary(void)
{
    const float stopped[3]={0,0,0};
    Q installation=mount(0,-10,0);
    AttitudeMath ctx;AttitudeMath_Init(&ctx,0);
    AttitudeBody body;
    for(uint32_t i=0;i<10;++i)
    {
        AttitudeSample raw=sample(0,0,0,installation,i%2?179.8:-179.8,stopped);
        AttitudeMath_Process(&ctx,&raw,i*100,&body);
    }
    assert(ctx.ready && fabsf(ctx.reference_angles_deg[2])>179.9f);
    AttitudeSample raw=sample(0,0,20,installation,180,stopped);
    assert(AttitudeMath_Process(&ctx,&raw,1100,&body)==ATTITUDE_MATH_READY);
    near(body.yaw_deg,20,0.002f);
}

static void test_restart_invalid_and_singular(void)
{
    const float stopped[3]={0,0,0};
    Q installation=mount(0,-10,0);
    AttitudeSample raw=sample(0,0,0,installation,0,stopped);
    AttitudeMath ctx;AttitudeMath_Init(&ctx,0);
    AttitudeBody body={0};
    for(uint32_t i=0;i<4;++i)AttitudeMath_Process(&ctx,&raw,i*100,&body);
    raw.angles_deg[0]=2;
    assert(AttitudeMath_Process(&ctx,&raw,400,&body)==ATTITUDE_MATH_CALIBRATING);
    assert(ctx.calibration_count==5 && ctx.calibration_restarts==0);
    raw=sample(0,0,0,installation,0,stopped);
    AttitudeMath_Process(&ctx,&raw,500,&body);
    AttitudeMath_DiscardPending(&ctx);
    assert(ctx.calibration_count==0 && ctx.calibration_restarts==1);
    AttitudeMath_Process(&ctx,&raw,600,&body);
    raw.angles_deg[0]=NAN;
    assert(AttitudeMath_Process(&ctx,&raw,700,&body)==ATTITUDE_MATH_INVALID);
    assert(ctx.calibration_count==0);
    calibrate(&ctx,installation,0,0,0);
    AttitudeMath_DiscardPending(&ctx);assert(ctx.ready);
    raw=sample(90,0,0,installation,0,stopped);
    AttitudeBody before=body;
    assert(AttitudeMath_Process(&ctx,&raw,1200,&body)==ATTITUDE_MATH_SINGULAR);
    assert(memcmp(&before,&body,sizeof(body))==0);
    raw.acc_g[0]=INFINITY;
    assert(AttitudeMath_Process(&ctx,&raw,1300,&body)==ATTITUDE_MATH_INVALID);
    assert(memcmp(&before,&body,sizeof(body))==0);
}

/* 重放用户触发旧版重启的六组日志，再重复前四组，验证第 10 次完成。
 * 这些读数超过旧版 3 deg/s 阈值，角度变化也超过旧版 1 deg 阈值。
 */
static void test_reported_samples_finish_without_motion_restart(void)
{
    const AttitudeSample logs[] = {
        {{-3.950f,-12.068f,149.447f},{2.747f,-3.235f,-7.019f},{0.229f,-0.079f,0.980f}},
        {{-4.449f,-11.816f,147.695f},{4.700f,1.099f,-2.869f},{0.222f,-0.084f,0.970f}},
        {{-5.054f,-12.283f,148.431f},{0.854f,2.869f,4.578f},{0.235f,-0.101f,0.973f}},
        {{-6.603f,-11.508f,150.721f},{-2.014f,-0.061f,-2.014f},{0.178f,-0.122f,0.956f}},
        {{-10.080f,-13.162f,152.161f},{-13.184f,-12.573f,2.747f},{0.264f,-0.142f,0.985f}},
        {{-2.357f,-11.118f,148.200f},{2.197f,2.930f,-4.944f},{0.203f,-0.042f,0.973f}}
    };
    AttitudeMath ctx;AttitudeMath_Init(&ctx,0);
    AttitudeBody body={0};
    for (uint32_t i=0;i<10;++i)
    {
        if (i != 0)
        {
            /* 未到 100 ms 的晃动读数既不计数，也不能清空已有累计。 */
            AttitudeMathResult skipped=AttitudeMath_Process(&ctx,&logs[4],100U*i-50U,&body);
            assert(skipped==ATTITUDE_MATH_CALIBRATING);
            assert(ctx.calibration_count==i && ctx.calibration_restarts==0);
        }
        AttitudeMathResult result=AttitudeMath_Process(&ctx,&logs[i%6],100U*i,&body);
        assert(ctx.calibration_count==i+1);
        assert(ctx.calibration_restarts==0);
        assert(result==(i==9?ATTITUDE_MATH_READY:ATTITUDE_MATH_CALIBRATING));
    }
    assert(ctx.ready);
}

int main(void)
{
    test_known_twenty_degree_case();
    test_full_mount_and_combined_motion();
    test_rate_keeps_initial_average();
    test_spacing_average_and_clock_wrap();
    test_heading_boundary();
    test_restart_invalid_and_singular();
    test_reported_samples_finish_without_motion_restart();
    puts("attitude_math: all numerical and state tests passed");
    return 0;
}
