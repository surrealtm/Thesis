#if FOUNDATION_WIN32 && CORE_LIB
# define EXPORT __declspec(dllexport)
#else
# define EXPORT 
#endif

extern "C" {
    EXPORT void do_simple_test();
}
