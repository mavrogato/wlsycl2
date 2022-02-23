#ifndef INCLUDE_EXPERIMENTAL_GENERATOR_HPP_4C885F69_96B6_4C47_8ACE_C560BA14D5B3
#define INCLUDE_EXPERIMENTAL_GENERATOR_HPP_4C885F69_96B6_4C47_8ACE_C560BA14D5B3

#include <type_traits>
#include <memory>
#include <cassert>

namespace std::experimental
{

template <class T, class = std::void_t<>>
struct coroutine_traits_sfinae { };

template <class T>
struct coroutine_traits_sfinae<T, typename std::void_t<typename T::promise_type>> {
    using promise_type = typename T::promise_type;
};

template <typename Ret, typename... Args>
struct coroutine_traits : public coroutine_traits_sfinae<Ret> { };

template <typename Promise = void>
class coroutine_handle;

template <>
class coroutine_handle<void> {
public:
    constexpr coroutine_handle() noexcept
    : handle_(nullptr)
    {
    }
    constexpr coroutine_handle(nullptr_t) noexcept
        : handle_(nullptr)
    {
    }

    auto& operator = (nullptr_t) noexcept {
        this->handle_ = nullptr;
        return *this;
    }

    constexpr void* address() const noexcept { return this->handle_; }
    constexpr explicit operator bool() const noexcept { return this->handle_; }

    void operator() () { resume(); }

    void resume() {
        assert(is_suspended());
        assert(!done());
        __builtin_coro_resume(this->handle_);
    }
    void destroy() {
        assert(is_suspended());
        __builtin_coro_destroy(this->handle_);
    }
    bool done() const {
        assert(is_suspended());
        return __builtin_coro_done(this->handle_);
    }

public:
    static coroutine_handle from_address(void* addr) noexcept {
        coroutine_handle tmp;
        tmp.handle_ = addr;
        return tmp;
    }
    static coroutine_handle from_address(nullptr_t) noexcept {
        // Should from address(nullptr) be allowed?
        return coroutine_handle(nullptr);
    }
    template <class T, bool CALL_IS_VALID = false>
    static coroutine_handle from_address(T*) {
        static_assert(CALL_IS_VALID);
    }

public:
    friend bool operator == (coroutine_handle lhs, coroutine_handle rhs) noexcept {
        return lhs.address() == rhs.address();
    }
    friend bool operator < (coroutine_handle lhs, coroutine_handle rhs) noexcept {
        return less<void*>()(lhs.address(), rhs.address());
    }

private:
    bool is_suspended() const noexcept {
        // actually implement a check for if the coro is suspended.
        return this->handle_;
    }

private:
    void* handle_;

    template <class Promise> friend class coroutine_handle;
};

template <typename Promise>
class coroutine_handle : public coroutine_handle<> {
    using Base = coroutine_handle<>;

public:
    coroutine_handle() noexcept
        : Base()
    {
    }
    coroutine_handle(nullptr_t) noexcept
        : Base(nullptr)
    {
    }

    coroutine_handle& operator = (nullptr_t) noexcept {
        Base::operator = (nullptr);
        return *this;
    }

    Promise& promise() const {
        return *static_cast<Promise*>(
            __builtin_coro_promise(this->handle_, alignof (Promise), false));
    }

public:
    static coroutine_handle from_address(void* addr) noexcept {
        coroutine_handle tmp;
        tmp.handle_ = addr;
        return tmp;
    }
    static coroutine_handle from_address(nullptr_t) noexcept {
        // NOTE: this overload isn't required by the standard but is needed so
        // the deleted Promise* overload doesn't make from_address(nullptr)
        // ambiguous.
        //  should from address work with nullptr?
        return coroutine_handle(nullptr);
    }
    template <class T, bool CALL_IS_VALID = false>
    static coroutine_handle from_address(T*) noexcept {
        static_assert(CALL_IS_VALID);
    }
    template <bool CALL_IS_VALID = false>
    static coroutine_handle from_address(Promise*) noexcept {
        static_assert(CALL_IS_VALID);
    }
    static coroutine_handle from_promise(Promise& promise) noexcept {
        using RawPromise = typename std::remove_cv<Promise>::type;
        coroutine_handle tmp;
        tmp.handle_ = __builtin_coro_promise(
            std::addressof(const_cast<RawPromise&>(promise)),
            alignof (Promise), true);
        return tmp;
    }
};

#if __has_builtin(__builtin_coro_null)
struct noop_coroutine_promise { };
template <>
class coroutine_handle<noop_coroutine_promise>
    : public coroutine_handle<>
{
    using Base = coroutine_handle<>;
    using Promise = noop_coroutine_promise;

public:
    Promise& promise() const {
        return *static_cast<Promise*>(
            __builtin_coro_promise(this->handle_, alignof (Promise), false));
    }

    constexpr explicit operator bool() const noexcept { return true; }
    constexpr bool done() const noexcept { return false; }

    constexpr void operator()() const noexcept { }
    constexpr void resume() const noexcept { }
    constexpr void destroy() const noexcept { }

private:
    friend coroutine_handle<noop_coroutine_promise> noop_coroutine() noexcept;

    coroutine_handle() noexcept {
        this->handle_ = __builtin_coro_noop();
    }
};
using noop_coroutine_handle = coroutine_handle<noop_coroutine\promise>;
inline noop_coroutine_handle noop_coroutine() noexcept {
    return noop_coroutine_handle();
}
#endif //__has_builtin(__builtin_coro_noop)

struct suspend_never {
    bool await_ready() const noexcept { return true; }
    void await_suspend(coroutine_handle<>) const noexcept { }
    void await_resume() const noexcept { }
};

struct suspend_always {
    bool await_ready() const noexcept { return false; }
    void await_suspend(coroutine_handle<>) const noexcept { }
    void await_resume() const noexcept { }
};

}
/////////////////////////////////////////////////////////////////////////////

namespace std
{

template <class T>
struct hash<experimental::coroutine_handle<T>> {
    using arg_type = experimental::coroutine_handle<T>;
    size_t operator() (arg_type const& v) const noexcept {
        return hash<void*>()(v.address());
    }
};

using suspend_always = experimental::suspend_always;

template <class T>
using coroutine_handle = experimental::coroutine_handle<T>;

template <class T>
struct generator {
    struct promise_type {
        T value_;

        generator get_return_object() noexcept { return generator{*this}; }
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend()   noexcept { return {}; }
        void unhandled_exception() { throw; }
        std::suspend_always yield_value(T const& value) noexcept {
            this->value_ = value;
            return {};
        }
        void return_void() noexcept { }
    };
    struct iterator {
        using iterator_category = std::input_iterator_tag;
        using size_type         = std::size_t;
        using differnce_type    = std::ptrdiff_t;
        using value_type        = std::remove_cvref_t<T>;
        using reference         = value_type&;
        using const_reference   = value_type const&;
        using pointer           = value_type*;
        using const_pointer     = value_type const*;

        std::coroutine_handle<promise_type> coro_ = nullptr;

        iterator() = default;
        explicit iterator(std::coroutine_handle<promise_type> coro) noexcept : coro_(coro) { }
        iterator& operator++() {
            if (this->coro_.done()) {
                this->coro_ = nullptr;
            }
            else {
                this->coro_.resume();
            }
            return *this;
        }
        iterator& operator++(int) {
            auto tmp = *this;
            ++*this;
            return tmp;
        }
        [[nodiscard]]
        friend bool operator==(iterator const& lhs, std::default_sentinel_t) noexcept {
            return lhs.coro_.done();
        }
        [[nodiscard]]
        friend bool operator!=(std::default_sentinel_t, iterator const& rhs) noexcept {
            return rhs.coro_.done();
        }
        [[nodiscard]] const_reference operator*() const noexcept {
            return this->coro_.promise().value_;
        }
        [[nodiscard]] reference operator*() noexcept {
            return this->coro_.promise().value_;
        }
        [[nodiscard]] const_pointer operator->() const noexcept {
            return std::addressof(this->coro_.promise().value_);
        }
        [[nodiscard]] pointer operator->() noexcept {
            return std::addressof(this->coro_.promise().value_);
        }
    };
    [[nodiscard]] iterator begin() {
        if (this->coro_) {
            if (this->coro_.done()) {
                return {};
            }
            else {
                this->coro_.resume();
            }
        }
        return iterator{this->coro_};
    }
    [[nodiscard]] std::default_sentinel_t end() noexcept { return std::default_sentinel; }

    [[nodiscard]] bool empty() noexcept { return this->coro_.done(); }

    explicit generator(promise_type& prom) noexcept
        : coro_(std::coroutine_handle<promise_type>::from_promise(prom))
        {
        }
    generator() = default;
    generator(generator&& rhs) noexcept
        : coro_(std::exchange(rhs.coro_, nullptr))
        {
        }
    ~generator() noexcept {
        if (this->coro_) {
            this->coro_.destroy();
        }
    }
    generator& operator=(generator const&) = delete;
    generator& operator=(generator&& rhs) {
        if (this != &rhs) {
            this->coro_ = std::exchange(rhs.coro_, nullptr);
        }
        return *this;
    }

private:
    std::coroutine_handle<promise_type> coro_ = nullptr;
};

} // end of namespace std

#endif/*INCLUDE_EXPERIMENTAL_GENERATOR_HPP_4C885F69_96B6_4C47_8ACE_C560BA14D5B3*/

#ifndef INCLUDE_WAYLAND_HELPER_HPP_4A10701A_4A55_406C_8525_E20926D97E1C
#define INCLUDE_WAYLAND_HELPER_HPP_4A10701A_4A55_406C_8525_E20926D97E1C

#include <concepts>
#include <iosfwd>
#include <memory>
#include <string_view>

#include <wayland-client.h>

inline namespace wayland_client_helper
{

template <class> constexpr std::nullptr_t wl_interface_ref = nullptr;
#define INTERN_WL_INTERFACE(wl_client)                                  \
    template <> constexpr wl_interface const& wl_interface_ref<wl_client> = wl_client##_interface;
INTERN_WL_INTERFACE(wl_display);
INTERN_WL_INTERFACE(wl_registry);
INTERN_WL_INTERFACE(wl_compositor);
INTERN_WL_INTERFACE(wl_shell);
INTERN_WL_INTERFACE(wl_seat);
INTERN_WL_INTERFACE(wl_keyboard);
INTERN_WL_INTERFACE(wl_pointer);
INTERN_WL_INTERFACE(wl_touch);
INTERN_WL_INTERFACE(wl_shm);
INTERN_WL_INTERFACE(wl_surface);
INTERN_WL_INTERFACE(wl_shell_surface);
INTERN_WL_INTERFACE(wl_buffer);
INTERN_WL_INTERFACE(wl_shm_pool);
INTERN_WL_INTERFACE(wl_callback);

template <class T>
concept wl_client_t = std::same_as<decltype (wl_interface_ref<T>), wl_interface const&>;

template <wl_client_t T, class Ch>
auto& operator << (std::basic_ostream<Ch>& output, T const* ptr) {
    return output << static_cast<void const*>(ptr)
                  << '['
                  << wl_interface_ref<T>.name
                  << ']';
}

template <wl_client_t T>
auto attach_unique(T* ptr) noexcept {
    static constexpr auto deleter = [](T* ptr) noexcept -> void {
        static constexpr auto interface_addr = std::addressof(wl_interface_ref<T>);
        if      constexpr (interface_addr == std::addressof(wl_display_interface)) {
            wl_display_disconnect(ptr);
        }
        else if constexpr (interface_addr == std::addressof(wl_keyboard_interface)) {
            wl_keyboard_release(ptr);
        }
        else if constexpr (interface_addr == std::addressof(wl_pointer_interface)) {
            wl_pointer_release(ptr);
        }
        else if constexpr (interface_addr == std::addressof(wl_touch_interface)) {
            wl_touch_release(ptr);
        }
        else {
            wl_proxy_destroy(reinterpret_cast<wl_proxy*>(ptr));
        }
    };
    return std::unique_ptr<T, decltype (deleter)>(ptr, deleter);
}

template <wl_client_t T>
using unique_ptr_t = decltype (attach_unique(std::declval<T*>()));

template <size_t N, wl_client_t... Args>
void register_global_callback(void* data,
                              wl_registry* registry,
                              uint32_t name,
                              char const* interface,
                              uint32_t version) noexcept
{
    if constexpr (N != sizeof... (Args)) {
        using type = typename std::tuple_element<N, std::tuple<Args...>>::type;
        static constexpr auto const& interface_ref = wl_interface_ref<type>;
        if (std::string_view(interface) == interface_ref.name) {
            auto& globals = *reinterpret_cast<std::tuple<unique_ptr_t<Args>...>*>(data);
            auto& global = std::get<N>(globals);
            global.reset(reinterpret_cast<type*>(wl_registry_bind(registry,
                                                                  name,
                                                                  &interface_ref,
                                                                  version)));
        }
        else {
            register_global_callback<N+1, Args...>(data, registry, name, interface, version);
        }
    }
}

template <wl_client_t... Args>
auto register_global(wl_display* display) noexcept {
    static std::tuple<unique_ptr_t<Args>...> globals;
    if (auto registry = attach_unique(wl_display_get_registry(display))) {
        static constexpr wl_registry_listener listener {
            .global = register_global_callback<0, Args...>,
            .global_remove = [](auto...) noexcept { }
        };
        if (0 == wl_registry_add_listener(registry.get(), &listener, &globals)) {
            wl_display_roundtrip(display);
        }
    }
    return std::move(globals);
}

} // end of namesace wayland_client_helper

#endif/*INCLUDE_WAYLAND_HELPER_HPP_4A10701A_4A55_406C_8525_E20926D97E1C*/

/////////////////////////////////////////////////////////////////////////////
#include <iosfwd>
#include <iomanip>
#include <algorithm>
#include <cstdint>

inline namespace versor
{

template <class T> constexpr T min = std::numeric_limits<T>::min();
template <class T> constexpr T max = std::numeric_limits<T>::max();

constexpr auto clamp(auto x, auto a, auto b) {
    return std::min<decltype (x)>(std::max<decltype (x)>(x, a), b);
}
template <class T>
constexpr T clamp(auto x) {
    return clamp(x, min<T>, max<T>);
}

template <class T, size_t N>
struct versor : versor<T, N-1> {
public:
    using CAR = T;
    using CDR = versor<T, N-1>;

public:
    constexpr versor() noexcept : CDR{CAR{}}, car{CAR{}}
    {
    }
    constexpr versor(CAR car) noexcept : CDR{car}, car{car}
    {
    }
    constexpr versor(CAR car, auto... cdr) noexcept : CDR{cdr...}, car{car}
    {
    }
    constexpr versor(auto car) noexcept : CDR{car}, car{clamp<CAR>(car)}
    {
    }
    constexpr versor(auto car, auto... cdr) noexcept : CDR{cdr...}, car{clamp<CAR>(car)}
    {
    }

public:
    auto& rest() noexcept { return *static_cast<CDR*>(this); }

public:
    constexpr auto& operator += (versor rhs) noexcept {
        this->rest() += rhs.rest();
        // NOTE: Assumes a temporary promotion to a signed integer.
        this->car = clamp<CAR>(this->car + rhs.car);
        return *this;
    }
    constexpr auto& operator -= (versor rhs) noexcept {
        this->rest() -= rhs.rest();
        // NOTE: Assumes a temporary promotion to a signed integer.
        this->car = clamp<CAR>(this->car - rhs.car);
        return *this;
    }
    friend constexpr auto operator + (versor lhs, versor rhs) noexcept {
        return versor(lhs) += rhs;
    }
    friend constexpr auto operator - (versor lhs, versor rhs) noexcept {
        return versor(lhs) -= rhs;
    }

private:
    CAR car;
};
template <class T>
struct versor<T, 0> {
    constexpr versor(auto) noexcept { }
    constexpr auto& operator += (versor) noexcept { return *this; }
    constexpr auto& operator -= (versor) noexcept { return *this; }
};

using color = versor<uint8_t, 4>;

template <class Ch, class T, size_t N>
auto& operator << (std::basic_ostream<Ch>& output, versor<T, N> v) {
    return output;
}
template <class Ch>
auto& operator << (std::basic_ostream<Ch>& output, color v) {
    auto prevfill = output.fill('0');
    auto prevflag = output.setf(std::ios_base::hex, std::ios_base::basefield);
    output << std::setw(8) << reinterpret_cast<uint32_t&>(v);
    output.setf(prevflag);
    output.fill(prevfill);
    return output;
}

} // end of namespace versor

/////////////////////////////////////////////////////////////////////////////

#include <iostream>
#include <filesystem>
#include <chrono>
#include <variant>
#include <complex>
#include <numbers>
#include <ranges>
#include <cstdlib>

#include <CL/sycl.hpp>

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

inline auto static_lambda(auto&& lambda) {
    static auto bridge = lambda;
    static auto instance = [](auto... args) {
        return bridge(args...);
    };
    return instance;
}

struct pointer_motion {
    void *data;
    struct wl_pointer *wl_pointer;
    uint32_t time;
    wl_fixed_t x;
    wl_fixed_t y;
};
struct keyboard_key {
    void *data;
    struct wl_keyboard *wl_keyboard;
    uint32_t serial;
    uint32_t time;
    uint32_t key;
    uint32_t state;
};

using event = std::variant<pointer_motion,
                           keyboard_key>;

int main() {
    try {
        /////////////////////////////////////////////////////////////////////////////
        // Check the environment
        static auto const XDG_RUNTIME_DIR = std::getenv("XDG_RUNTIME_DIR");
        if (!XDG_RUNTIME_DIR || !std::filesystem::exists(XDG_RUNTIME_DIR)) {
            std::cerr << "This program requires XDG_RUNTIME_DIR setting..." << std::endl;
            return 0xff;
        }
        /////////////////////////////////////////////////////////////////////////////
        // Connect to the display servern
        auto display = attach_unique(wl_display_connect(nullptr));
        if (!display) {
            std::cerr << "Cannot connect to the wayland server..." << std::endl;
            return 0xff;
        }
        /////////////////////////////////////////////////////////////////////////////
        // Bind required global objects.
        auto globals = register_global<wl_compositor, wl_shell, wl_shm, wl_seat>(display.get());
        auto& [compositor, shell, shm, seat] = globals;
        if (!compositor || !shell || !shm || !seat) {
            std::cerr << "Some required wayland global objects are missing..." << std::endl;
            return 0xff;
        }
        /////////////////////////////////////////////////////////////////////////////
        // Add the listener for checking the shared memory format.
        bool required_shm_format_supported = false;
        wl_shm_listener shm_listener {
            .format = static_lambda([&](auto, auto, uint32_t format) noexcept {
                if (format == WL_SHM_FORMAT_ARGB8888) {
                    required_shm_format_supported = true;
                }
            }),
        };
        if (wl_shm_add_listener(shm.get(), &shm_listener, nullptr)) {
            std::cerr << "wl_shm_add_listener failed..." << std::endl;
            return 0xff;
        }
        /////////////////////////////////////////////////////////////////////////////
        // Add the listener for checking the seat capabilities.
        uint32_t seat_capabilities = 0;
        wl_seat_listener seat_listener {
            .capabilities = static_lambda([&](auto, auto, uint32_t caps) noexcept {
                seat_capabilities = caps;
            }),
            .name = [](auto...) noexcept { },
        };
        if (wl_seat_add_listener(seat.get(), &seat_listener, nullptr)) {
            std::cerr << "wl_seat_add_listener failed..." << std::endl;
            return 0xff;
        }
        /////////////////////////////////////////////////////////////////////////////
        // Check the result of the listeners above.
        wl_display_roundtrip(display.get());
        if (required_shm_format_supported == false) {
            std::cerr << "Required wl_shm format not supported..." << std::endl;
            return 0xff;
        }
        if (!(seat_capabilities & WL_SEAT_CAPABILITY_POINTER)) {
            std::cerr << "No pointer found..." << std::endl;
            return 0xff;
        }
        if (!(seat_capabilities & WL_SEAT_CAPABILITY_KEYBOARD)) {
            std::cerr << "No keyboard found..." << std::endl;
            return 0xff;
        }
        if (!(seat_capabilities & WL_SEAT_CAPABILITY_TOUCH)) {
            std::cerr << "(Warning) No touch device found..." << std::endl;
        }
        /////////////////////////////////////////////////////////////////////////////
        // Create the surface.
        auto surface = attach_unique(wl_compositor_create_surface(compositor.get()));
        if (!surface) {
            std::cerr << "Cannot create the surface..." << std::endl;
            return 0xff;
        }
        /////////////////////////////////////////////////////////////////////////////
        // Create the shell surface and add the listener
        auto shell_surface = attach_unique(wl_shell_get_shell_surface(shell.get(), surface.get()));
        if (!shell_surface) {
            std::cerr << "Cannot create the shell surface..." << std::endl;
            return 0xff;
        }
        wl_shell_surface_listener shell_surface_listener = {
            .ping = [](auto, auto shell_surface, auto serial) {
                wl_shell_surface_pong(shell_surface, serial);
                std::cout << "Pinged and ponged." << std::endl;
            },
            .configure = [](auto...) noexcept {
                std::cout << "Configuring... (not supported yet)" << std::endl;
            },
            .popup_done = [](auto...) noexcept {
                std::cout << "Popup done." << std::endl;
            },
        };
        if (wl_shell_surface_add_listener(shell_surface.get(), &shell_surface_listener, nullptr)) {
            std::cerr << "wl_shell_surface_add_listener failed..." << std::endl;
            return 0xff;
        }
        wl_shell_surface_set_toplevel(shell_surface.get());
        /////////////////////////////////////////////////////////////////////////////
        // Keyboard
        auto keyboard = attach_unique(wl_seat_get_keyboard(seat.get()));
        if (!keyboard) {
            std::cerr << "wl_seat_get_keyboard failed..." << std::endl;
            return 0xff;
        }
        uint32_t key_input = 0;
        wl_keyboard_listener keyboard_listener{
            .keymap = [](auto...) noexcept { },
            .enter = [](auto...) noexcept { },
            .leave = [](auto...) noexcept { },
            .key = static_lambda([&](void *data,
                                     wl_keyboard* keyboard_raw,
                                     uint32_t serial,
                                     uint32_t time,
                                     uint32_t key,
                                     uint32_t state) noexcept
            {
                key_input = key;
            }),
            .modifiers = [](auto...) noexcept { },
            .repeat_info = [](auto...) noexcept { },
        };
        if (wl_keyboard_add_listener(keyboard.get(), &keyboard_listener, nullptr)) {
            std::cerr << "wl_keyboard_add_listener failed..." << std::endl;
            return 0xff;
        }
        /////////////////////////////////////////////////////////////////////////////
        // Pointer
        auto pointer = attach_unique(wl_seat_get_pointer(seat.get()));
        if (!pointer) {
            std::cerr << "wl_seat_get_pointer failed..." << std::endl;
            return 0xff;
        }
        std::complex<float> pt(0, 0);
        wl_pointer_listener pointer_listener{
            .enter = [](auto...) noexcept { },
            .leave = [](auto...) noexcept { },
            .motion = static_lambda([&](auto, auto, auto, wl_fixed_t x, wl_fixed_t y) noexcept {
                pt = {
                    static_cast<float>(wl_fixed_to_int(x)),
                    static_cast<float>(wl_fixed_to_int(y)),
                };
            }),
            .button = [](auto...) noexcept { std::cerr << "button" << std::endl; },
            .axis = [](auto...) noexcept { },
            .frame = [](auto...) noexcept { },
            .axis_source = [](auto...) noexcept { },
            .axis_stop = [](auto...) noexcept { },
            .axis_discrete = [](auto...) noexcept { },
        };
        if (wl_pointer_add_listener(pointer.get(), &pointer_listener, nullptr)) {
            std::cerr << "wl_pointer_add_listener failed..." << std::endl;
            return 0xff;
        }
        /////////////////////////////////////////////////////////////////////////////
        // Buffer initialization (tentative)
        color* pixels = nullptr;
        auto create_buffer = [&pixels](wl_shm* shm, size_t cx, size_t cy) -> unique_ptr_t<wl_buffer>
        {
            static constexpr std::string_view tmp_name = "/weston-shared-XXXXXX";
            std::string tmp_path(XDG_RUNTIME_DIR);
            tmp_path += tmp_name;
            int fd = mkostemp(tmp_path.data(), O_CLOEXEC);
            if (fd >= 0) {
                unlink(tmp_path.c_str());
            }
            else {
                return nullptr;
            }
            if (ftruncate(fd, 4*cx*cy) < 0) {
                close(fd);
                return nullptr;
            }
            auto data = mmap(nullptr, 4*cx*cy, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if (data == MAP_FAILED) {
                close(fd);
                return nullptr;
            }
            pixels = reinterpret_cast<color*>(data);
            return attach_unique(
                wl_shm_pool_create_buffer(
                    attach_unique(wl_shm_create_pool(shm, fd, 4*cx*cy)).get(),
                    0,
                    cx, cy,
                    cx * 4,
                    WL_SHM_FORMAT_XRGB8888));
        };
        /////////////////////////////////////////////////////////////////////////////
        // Buffer
        size_t cx = 1920;
        size_t cy = 1080;
        auto buffer = create_buffer(shm.get(), cx, cy);
        if (!buffer || !pixels) {
            std::cerr << "Cannot create buffers..." << std::endl;
            return 0xff;
        }
        wl_buffer_listener buffer_listener{
            .release = [](auto...) noexcept {
                std::cerr << "*** buffer released." << std::endl;
            },
        };
        if (wl_buffer_add_listener(buffer.get(), &buffer_listener, nullptr)) {
            std::cerr << "wl_buffer_add_listener failed..." << std::endl;
            return 0xff;
        }
        /////////////////////////////////////////////////////////////////////////////
        // static const auto spiral = []
        // {
        //     using namespace std::numbers;
        //     std::array<std::complex<float>, 65536> ret;
        //     for (int i = 0; i < std::size(ret); ++i) {
        //         ret[i] = std::polar<float>(std::sqrt(i+1), (i)*(pi/(1-phi)));
        //     }
        //     return ret;
        // }();
        /////////////////////////////////////////////////////////////////////////////
        {
            auto dim_pixels = sycl::range<2>{cy, cx};
            auto dev_pixels = sycl::buffer{pixels, dim_pixels};
            sycl::queue().submit([&](sycl::handler& h) {
                auto a = dev_pixels.get_access<sycl::access::mode::write>(h);
                h.parallel_for(dim_pixels, [=](auto idx) {
                    a[idx] = color(0x00);
                });
            });
            wl_surface_damage(surface.get(), 0, 0, cx, cy);
            wl_surface_attach(surface.get(), buffer.get(), 0, 0);
            wl_surface_commit(surface.get());
            wl_display_flush(display.get());
        }
        /////////////////////////////////////////////////////////////////////////////
        // Main loop
        while (wl_display_dispatch(display.get()) != -1) {
            if (1 == key_input) {
                break;
            }
            /////////////////////////////////////////////////////////////////////////////
            auto dim_pixels = sycl::range<2>{cy, cx};
            auto dev_pixels = sycl::buffer{pixels, dim_pixels};
            sycl::queue().submit([&](sycl::handler& h) {
                auto a = dev_pixels.get_access<sycl::access::mode::write>(h);
                h.parallel_for(dim_pixels, [=](auto idx) {
                    a[idx] = color(0x00);
                });
            });
            wl_surface_damage(surface.get(), 0, 0, cx, cy);
            wl_surface_attach(surface.get(), buffer.get(), 0, 0);
            wl_surface_commit(surface.get());
            wl_display_flush(display.get());
            auto dim_spiral = sycl::range<1>{16384*256};
            // auto dev_spiral = sycl::buffer{spiral.data(), dim_spiral};
            sycl::queue().submit([&](sycl::handler& h) {
                auto a = dev_pixels.get_access<sycl::access::mode::read_write>(h);
                //auto s = dev_spiral.get_access<sycl::access::mode::write>(h);
                h.parallel_for(dim_spiral, [=](auto idx) {
                    static constexpr float pi = std::numbers::pi_v<float>;
                    static constexpr float phi = std::numbers::phi_v<float>;
                    float i = (1 + idx);
                    std::complex<float> c = pt + std::polar<float>(std::sqrt(i), i*2*pi/phi);
                    if (0.0f <= c.real() && c.real() < cx) {
                        // auto at = sycl::range{
                        //     static_cast<size_t>(std::round(c.imag())),
                        //     static_cast<size_t>(std::round(c.real())),
                        // };
                        size_t y = std::round(c.imag());
                        size_t x = std::round(c.real());
                        a[{y, x}] = color(0xff,
                                          0xff - 0xff*idx/dim_spiral[0]);
                    }
                });
            });
            /////////////////////////////////////////////////////////////////////////////
            wl_surface_damage(surface.get(), 0, 0, cx, cy);
            wl_surface_attach(surface.get(), buffer.get(), 0, 0);
            wl_surface_commit(surface.get());
            wl_display_flush(display.get());
        }
        return 0;
    }
    catch (std::exception& ex) {
        std::cerr << "Exception occured: " << ex.what() << std::endl;
    }
    catch (...) {
        std::cerr << "Unknown exception occured." << std::endl;
    }
    return 0xff;
}
