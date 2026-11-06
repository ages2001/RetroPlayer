/* Exercise the rebuilt C++ runtime inside a normal MinGW DLL. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <sstream>
#include <map>

static unsigned checks,failures,first;
static std::atomic<unsigned> destroyed(0);
extern "C" int __gthr_win32_key_create(unsigned *,void (*)(void *));
extern "C" int __gthr_win32_key_delete(unsigned);
extern "C" int __gthr_win32_setspecific(unsigned,const void *);
static unsigned key_a,key_b,key_c;
static std::atomic<unsigned> dtor_a(0),dtor_b(0),dtor_c(0),key_errors(0);
static void destroy_c(void *p) { if (p!=(void *)333) ++key_errors; ++dtor_c; }
static void destroy_b(void *) { ++dtor_b; }
static void destroy_a(void *p)
{
    if (p!=(void *)111 || __gthr_win32_key_delete(key_b) ||
        __gthr_win32_key_create(&key_c,destroy_c) ||
        __gthr_win32_setspecific(key_c,(void *)333)) ++key_errors;
    ++dtor_a;
}
#define CHECK(e) do { ++checks; if (!(e)) { ++failures; if (!first) first=checks; } } while (0)
struct Local {
    unsigned value=0;
    ~Local() { destroyed.fetch_add(1); }
};
static thread_local Local local;
struct Base { virtual ~Base() {} };
struct Derived : Base { int value=43; };

extern "C" __declspec(dllexport) unsigned Run(unsigned *count,unsigned *failed_at)
{
    try {
        std::string text="RetroPlayer";
        std::wstring wide=L"Legacy playback";
        CHECK(text.substr(5)=="Player" && wide.size()==15);
        std::vector<int> numbers;
        for (int i=0;i<1000;i++) numbers.push_back(i);
        CHECK(numbers.at(999)==999);
        try { (void)numbers.at(1000); CHECK(false); }
        catch (const std::out_of_range&) { CHECK(true); }
        std::stringstream stream;
        stream << "value=" << 12345;
        CHECK(stream.str()=="value=12345");
        std::map<std::string,int> map;
        map["first"]=12; map["second"]=19;
        CHECK(map.at("first")==12 && map.size()==2);
        std::unique_ptr<Base> base(new Derived);
        CHECK(dynamic_cast<Derived *>(base.get())->value==43);
        std::shared_ptr<int> shared=std::make_shared<int>(57);
        std::mutex mutex;
        std::condition_variable cv;
        std::once_flag once;
        std::atomic<unsigned> once_count(0),errors(0);
        bool go=false;
        unsigned ready=0,total=0;
        std::thread workers[8];
        for (unsigned i=0;i<8;i++) {
            workers[i]=std::thread([&,i,shared] {
                local.value=i+100;
                {
                    std::unique_lock<std::mutex> guard(mutex);
                    ++ready; cv.notify_all();
                    cv.wait(guard,[&] { return go; });
                }
                std::call_once(once,[&] { ++once_count; });
                for (unsigned j=0;j<1000;j++) {
                    std::lock_guard<std::mutex> guard(mutex);
                    ++total;
                    if (local.value!=i+100 || *shared!=57) ++errors;
                }
            });
        }
        {
            std::unique_lock<std::mutex> guard(mutex);
            CHECK(cv.wait_for(guard,std::chrono::seconds(20),[&] { return ready==8; }));
            go=true; cv.notify_all();
        }
        for (auto& worker:workers) worker.join();
        CHECK(total==8000 && errors==0);
        CHECK(once_count==1);
        CHECK(destroyed==8);
        CHECK(shared.use_count()==1);
        {
            std::unique_lock<std::mutex> guard(mutex);
            auto before=std::chrono::steady_clock::now();
            CHECK(!cv.wait_for(guard,std::chrono::milliseconds(40),[] { return false; }));
            CHECK(std::chrono::steady_clock::now()-before>=std::chrono::milliseconds(30));
        }
        std::once_flag retry;
        unsigned attempts=0;
        try { std::call_once(retry,[&] { ++attempts; throw std::runtime_error("retry"); }); CHECK(false); }
        catch (const std::runtime_error&) { CHECK(true); }
        std::call_once(retry,[&] { ++attempts; });
        CHECK(attempts==2);
        std::atomic<bool> detached(false);
        std::thread([&] { local.value=999; detached=true; }).detach();
        auto deadline=GetTickCount()+20000;
        while ((!detached || destroyed!=9) && (LONG)(deadline-GetTickCount())>0) Sleep(1);
        CHECK(detached && destroyed==9);
        // A destructor deletes another key and reuses its index. The old
        // snapshot must not dispatch the old destructor on the new value.
        CHECK(__gthr_win32_key_create(&key_b,destroy_b)==0);
        CHECK(__gthr_win32_key_create(&key_a,destroy_a)==0);
        std::thread([] {
            if (__gthr_win32_setspecific(key_a,(void *)111) ||
                __gthr_win32_setspecific(key_b,(void *)222)) ++key_errors;
        }).join();
        CHECK(dtor_a==1 && dtor_b==0 && dtor_c==1 && key_errors==0);
        CHECK(__gthr_win32_key_delete(key_a)==0 && __gthr_win32_key_delete(key_c)==0);
    } catch (...) { ++checks; ++failures; if (!first) first=checks; }
    *count=checks; *failed_at=first;
    return failures;
}
