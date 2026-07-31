// Linked without a C++ runtime; imports bind to the local game's own runtime.
static volatile int destroyed;

typedef int (*personality_fn)(unsigned, void*, void*);
static personality_fn game_personality;

extern "C" void fixture_set_personality(personality_fn value)
{
    game_personality = value;
}

// The game's personality is hidden, so the probe derives it from the game's
// own generic EHABI descriptors and checks its code bytes before binding here.
extern "C" __attribute__((visibility("hidden"))) int __gxx_personality_v0(
    unsigned state, void* exception, void* context)
{
    return game_personality(state, exception, context);
}

struct __attribute__((visibility("hidden"))) sample_error {
    int value;
};

struct guard {
    ~guard()
    {
        ++destroyed;
    }
};

extern "C" __attribute__((noinline)) void fixture_raise(int value)
{
    throw sample_error{value};
}

extern "C" int fixture_throw_catch(void)
{
    destroyed = 0;
    try {
        guard cleanup;
        fixture_raise(42);
    } catch(...) {
        // Catch-all isolates ARM unwind/RAII from typed cross-toolchain matching,
        // which a separate experiment did not establish with this game runtime.
        return 42 + destroyed;
    }

    return -1;
}
