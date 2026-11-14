#include "../Core/Scope.hpp"
#include <cassert>
#include <thread>
#include <iostream>
#include <limits>
int main(){
    gs::Scope scope,split;
    for(float x:scope.snapshot())assert(x==0);
    float input[4096];for(unsigned i=0;i<4096;i++)input[i]=.4f*std::sin(i*.02f);
    scope.push(input,4096);
    for(unsigned i=0;i<4096;i+=32)split.push(input+i,32);
    auto a=scope.snapshot(),b=split.snapshot();assert(a==b);
    assert(a[0]>0&&a[0]<.04f); // Rising-crossing alignment.
    float peak=0;for(float x:a)peak=std::max(peak,std::abs(x));assert(peak>.39f);
    for(float &x:input)x=std::numeric_limits<float>::quiet_NaN();scope.push(input,4096);
    for(float x:scope.snapshot())assert(x==0);
    for(unsigned i=0;i<4096;i++)input[i]=i%2?2.f:-2.f;
    std::atomic<bool> done{false};
    std::thread writer([&]{for(unsigned n=0;n<1000;n++)scope.push(input,4096);done.store(true);});
    do{for(float x:scope.snapshot())assert(std::isfinite(x)&&std::abs(x)<=1);}while(!done.load());
    writer.join();
    std::fill_n(input,4096,0);scope.push(input,4096);for(float x:scope.snapshot())assert(x==0);
    std::cout<<"PASS: scope silence, trigger, block invariance, invalid input, bounds and concurrent snapshots\n";
}
