
#include <iostream>
#include <filesystem>
#include <complex>
#include <numbers>
#include <string_view>
#include <cstdlib>
#include <cstring>

#include <CL/sycl.hpp>

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "coroutines-ts.hpp"
#include "wayland-client-helper.hpp"
#include "versor.hpp"

inline namespace tuple_pretty_print {

template<std::size_t...> struct seq{};
template<std::size_t N, std::size_t... Is>
struct gen_seq : gen_seq<N-1, N-1, Is...>{};
template<std::size_t... Is>
struct gen_seq<0, Is...> : seq<Is...>{};


template<class Ch, class Tuple, std::size_t... Is>
void print(std::basic_ostream<Ch>& output, Tuple const& t, seq<Is...>) noexcept {
    using swallow = int[];
    (void)swallow{0, (void(output << (Is == 0? "" : ", ") << std::get<Is>(t)), 0)...};
}
template<class Ch, class... Args>
decltype (auto) operator<<(std::basic_ostream<Ch>& output, std::tuple<Args...> const& t) noexcept {
    output << '(';
    print(output, t, gen_seq<sizeof...(Args)>());
    output << ')';
    return output;
}

} // end of namespace tuple_prettry_print

inline namespace wayland_client_helper_candidate {

template <class> struct function;
template <class T, class... Args> struct function<T(*)(Args...)> {
    using result_type = T;
    using arguments_tuple = std::tuple<Args...>;
};
template <class T, class... Args> struct function<T(Args...)> {
    using result_type = T;
    using arguments_tuple = std::tuple<Args...>;
};
template <class F> using arguments_of = typename function<F>::arguments_tuple;
#define ARGUMENTS_OF(T,m) arguments_of<decltype (std::declval<T>().m)>
int main() {
    std::cerr << typeid (ARGUMENTS_OF(wl_pointer_listener, motion)).name() << std::endl;
    return 0;
}

} // end of namespace wayland_client_helper_candidate

[[nodiscard]] inline auto register_globals(wl_display* display) noexcept {
    std::tuple<unique_ptr_t<wl_compositor>,
               unique_ptr_t<wl_shell>,
               unique_ptr_t<wl_shm>,
               unique_ptr_t<wl_seat>,
               unique_ptr_t<wl_output>,
               unique_ptr_t<wl_output>> nil{};
    // Bind required global objects.
    auto globals = register_global<wl_compositor,
                                   wl_shell,
                                   wl_shm,
                                   wl_seat,
                                   wl_output,
                                   wl_output>(display);
    auto& [compositor, shell, shm, seat, output, output_sub] = globals;
    if (!compositor || !shell || !shm || !seat || !output) {
        std::cerr << "Some required wayland global objects are missing..." << std::endl;
        return nil;
    }
    // Add the listener for checking the shared memory format.
    bool format_supported = false;
    static wl_shm_listener shm_listener {
        .format = [](auto data, auto, uint32_t format) noexcept {
            if (format == WL_SHM_FORMAT_ARGB8888) {
                *reinterpret_cast<bool*>(data) = true;
            }
        },
    };
    if (wl_shm_add_listener(shm.get(), &shm_listener, &format_supported)) {
        std::cerr << "wl_shm_add_listener failed..." << std::endl;
        return nil;
    }
    // Add the listener for checking the seat capabilities.
    uint32_t seat_capabilities = 0;
    static wl_seat_listener seat_listener {
        .capabilities = [](auto data, auto, uint32_t caps) noexcept {
            *reinterpret_cast<uint32_t*>(data) = caps;
        },
        .name = [](auto...) noexcept { },
    };
    if (wl_seat_add_listener(seat.get(), &seat_listener, &seat_capabilities)) {
        std::cerr << "wl_seat_add_listener failed..." << std::endl;
        return nil;
    }
    // Add the listener for output/output_sub
    using output_mode_args = ARGUMENTS_OF(wl_output_listener, mode);
    static output_mode_args mode_args;
    static wl_output_listener output_listener {
        .geometry  = [](auto... args) noexcept {
            std::cerr << "output geometry: " << std::tuple(args...) << std::endl;
        },
        .mode = [](void* data, auto... args) noexcept {
            *reinterpret_cast<decltype (&mode_args)>(data) = std::tuple(data, args...);
            std::cerr << "output mode: " << std::tuple(args...) << std::endl;
        },
        .done = [](auto... args) noexcept {
            std::cerr << "output done: " << std::tuple(args...) << std::endl;
        },
        .scale = [](auto... args) noexcept {
            std::cerr << "output scale: " << std::tuple(args...) << std::endl;
        },
    };
    if (wl_output_add_listener(output.get(), &output_listener, &mode_args)) {
        std::cerr << "wl_output_add_listener failed..." << std::endl;
        return nil;
    }
    // Check the nil of the listeners above.
    wl_display_roundtrip(display);
    if (format_supported == false) {
        std::cerr << "Required wl_shm format not supported..." << std::endl;
        return nil;
    }
    if (!(seat_capabilities & WL_SEAT_CAPABILITY_POINTER)) {
        std::cerr << "No pointer found..." << std::endl;
        return nil;
    }
    if (!(seat_capabilities & WL_SEAT_CAPABILITY_KEYBOARD)) {
        std::cerr << "No keyboard found..." << std::endl;
        return nil;
    }
    if (!(seat_capabilities & WL_SEAT_CAPABILITY_TOUCH)) {
        std::cerr << "(Warning) No touch device found..." << std::endl;
    }
    std::cout << mode_args << std::endl;
    return globals;
}

[[nodiscard]] inline auto create_shm_buffer(wl_shm* shm, size_t cx, size_t cy) noexcept {
    std::tuple<unique_ptr_t<wl_buffer>, color*> nil;
    // Check the environment
    std::string_view xdg_runtime_dir = std::getenv("XDG_RUNTIME_DIR");
    if (xdg_runtime_dir.empty() || !std::filesystem::exists(xdg_runtime_dir)) {
        std::cerr << "This program requires XDG_RUNTIME_DIR setting..." << std::endl;
        return nil;
    }
    std::string_view tmp_file_title = "/weston-shared-XXXXXX";
    if (1024 <= xdg_runtime_dir.size() + tmp_file_title.size()) {
        std::cerr << "The path of XDG_RUNTIME_DIR is too long..." << std::endl;
        return nil;
    }
    char tmp_path[1024] = { };
    auto p = std::strcat(tmp_path, xdg_runtime_dir.data());
    std::strcat(p, tmp_file_title.data());
    int fd = mkostemp(tmp_path, O_CLOEXEC);
    if (fd >= 0) {
        unlink(tmp_path);
    }
    else {
        std::cerr << "Failed to mkostemp..." << std::endl;
        return nil;
    }
    if (ftruncate(fd, 4*cx*cy) < 0) {
        std::cerr << "Failed to ftruncate..." << std::endl;
        close(fd);
        return nil;
    }
    auto data = mmap(nullptr, 4*cx*cy, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        std::cerr << "Failed to mmap..." << std::endl;
        close(fd);
        return nil;
    }
    return std::tuple(
        attach_unique(
            wl_shm_pool_create_buffer(
                attach_unique(wl_shm_create_pool(shm, fd, 4*cx*cy)).get(),
                0,
                cx, cy,
                cx * 4,
                WL_SHM_FORMAT_ARGB8888)),
        reinterpret_cast<color*>(data));
}

[[nodiscard]]
auto mainloop(size_t cx, size_t cy) -> std::generator<int> {
    try {
        auto display = attach_unique(wl_display_connect(nullptr));
        if (!display) {
            std::cerr << "Cannot connect to the display server..." << std::endl;
            co_return ;
        }
        auto globals = register_globals(display.get());
        auto& [compositor, shell, shm, seat, output, output_sub] = globals;
        if (!compositor || !shell || !shm || !seat || !output) {
            co_return ;
        }
        std::cout << "globals: " << globals << std::endl;
        /////////////////////////////////////////////////////////////////////////////
        // Keyboard
        auto keyboard = attach_unique(wl_seat_get_keyboard(seat.get()));
        if (!keyboard) {
            std::cerr << "wl_seat_get_keyboard failed..." << std::endl;
            co_return ;
        }
        uint32_t key_input = 0;
        wl_keyboard_listener keyboard_listener{
            .keymap = [](auto...) noexcept { },
            .enter = [](auto...) noexcept { std::cerr << "key enter." << std::endl; },
            .leave = [](auto...) noexcept { std::cerr << "key leave." << std::endl; },
            .key = [](void *data,
                      wl_keyboard* keyboard_raw,
                      uint32_t serial,
                      uint32_t time,
                      uint32_t key,
                      uint32_t state) noexcept
            {
                *reinterpret_cast<uint32_t*>(data) = key;
                std::cerr << "key: " << key << '\t';
                std::cerr << "  state: " << state << std::endl;
            },
            .modifiers = [](auto...) noexcept { },
            .repeat_info = [](auto...) noexcept { std::cerr << "key repeat." << std::endl; },
        };
        if (wl_keyboard_add_listener(keyboard.get(), &keyboard_listener, &key_input)) {
            std::cerr << "wl_keyboard_add_listener failed..." << std::endl;
            co_return ;
        }
        /////////////////////////////////////////////////////////////////////////////
        // Pointer
        auto pointer = attach_unique(wl_seat_get_pointer(seat.get()));
        if (!pointer) {
            std::cerr << "wl_seat_get_pointer failed..." << std::endl;
            co_return ;
        }
        std::complex<float> pt(0, 0);
        wl_pointer_listener pointer_listener{
            .enter = [](auto...) noexcept { },
            .leave = [](auto...) noexcept { },
            .motion = [](auto data, auto, auto, wl_fixed_t x, wl_fixed_t y) noexcept {
                *reinterpret_cast<decltype (&pt)>(data) = {
                    static_cast<float>(wl_fixed_to_int(x)),
                    static_cast<float>(wl_fixed_to_int(y)),
                };
            },
            .button = [](auto...) noexcept { std::cerr << "button" << std::endl; },
            .axis = [](auto...) noexcept { },
            .frame = [](auto...) noexcept { },
            .axis_source = [](auto...) noexcept { },
            .axis_stop = [](auto...) noexcept { },
            .axis_discrete = [](auto...) noexcept { },
        };
        if (wl_pointer_add_listener(pointer.get(), &pointer_listener, &pt)) {
            std::cerr << "wl_pointer_add_listener failed..." << std::endl;
            co_return ;
        }
        /////////////////////////////////////////////////////////////////////////////
        // Buffer
        auto [buffer, pixels] = create_shm_buffer(shm.get(), cx, cy);
        if (!buffer || !pixels) {
            std::cerr << "Cannot create buffers..." << std::endl;
            co_return ;
        }
        wl_buffer_listener buffer_listener{
            .release = [](auto...) noexcept {
                std::cerr << "*** buffer released." << std::endl;
            },
        };
        if (wl_buffer_add_listener(buffer.get(), &buffer_listener, nullptr)) {
            std::cerr << "wl_buffer_add_listener failed..." << std::endl;
            co_return ;
        }
        /////////////////////////////////////////////////////////////////////////////
        // Create the surface.
        auto surface = attach_unique(wl_compositor_create_surface(compositor.get()));
        if (!surface) {
            std::cerr << "Cannot create the surface..." << std::endl;
            co_return ;
        }
        /////////////////////////////////////////////////////////////////////////////
        // Create the shell surface and add the listener
        auto shell_surface = attach_unique(wl_shell_get_shell_surface(shell.get(), surface.get()));
        if (!shell_surface) {
            std::cerr << "Cannot create the shell surface..." << std::endl;
            co_return ;
        }
        wl_shell_surface_listener shell_surface_listener = {
            .ping = [](auto, auto shell_surface, auto serial) noexcept {
                wl_shell_surface_pong(shell_surface, serial);
                std::cout << "Pinged and ponged." << std::endl;
            },
            .configure = [](auto, auto, auto, auto width, auto height) noexcept {
                std::cout << "Configuring... (not supported yet)" << std::endl;
                std::cout << "  width:  " << width  << std::endl;
                std::cout << "  hieght: " << height << std::endl;
            },
            .popup_done = [](auto...) noexcept {
                std::cout << "Popup done." << std::endl;
            },
        };
        if (wl_shell_surface_add_listener(shell_surface.get(), &shell_surface_listener, nullptr)) {
            std::cerr << "wl_shell_surface_add_listener failed..." << std::endl;
            co_return ;
        }
        //wl_shell_surface_set_toplevel(shell_surface.get());
        wl_shell_surface_set_fullscreen(shell_surface.get(), 0, 60, output.get());
        /////////////////////////////////////////////////////////////////////////////
        // Main loop
        while (wl_display_dispatch(display.get()) != -1) {
            co_yield 0;
            if (1 == key_input) {
                break;
            }
            /////////////////////////////////////////////////////////////////////////////
            auto dim_pixels = sycl::range<2>{cy, cx};
            auto dev_pixels = sycl::buffer{pixels, dim_pixels};
            sycl::queue().submit([&](sycl::handler& h) {
                auto a = dev_pixels.get_access<sycl::access::mode::write>(h);
                h.parallel_for(dim_pixels, [=](auto idx) {
                    a[idx] = color(0xC0, 0x00);
                });
            });
            auto resolution = sycl::range<1>{16384};
            sycl::queue().submit([&](sycl::handler& h) {
                auto a = dev_pixels.get_access<sycl::access::mode::read_write>(h);
                h.parallel_for(resolution, [=](auto idx) {
                    static constexpr float pi = std::numbers::pi_v<float>;
                    static constexpr float phi = std::numbers::phi_v<float>;
                    float i = (1 + idx);
                    std::complex<float> c = pt + std::polar<float>(std::sqrt(i), i*2*pi/phi);
                    if (0.0f <= c.real() && c.real() < cx) {
                        size_t y = std::round(c.imag());
                        size_t x = std::round(c.real());
                        a[{y, x}] = color(0xC0,
                                          0xC0 - 0xC0*idx[0]/resolution[0]);
                    }
                });
            });
            /////////////////////////////////////////////////////////////////////////////
            wl_surface_damage(surface.get(), 0, 0, cx, cy);
            wl_surface_attach(surface.get(), buffer.get(), 0, 0);
            wl_surface_commit(surface.get());
            wl_display_flush(display.get());
        }
        co_return ;
    }
    catch (std::exception& ex) {
        std::cerr << "Exception occured: " << ex.what() << std::endl;
    }
    catch (...) {
        std::cerr << "Unknown exception occured." << std::endl;
    }
    co_return ;
}

int main() {
    for ([[maybe_unused]] auto item : mainloop(640, 480)) {
    }
    return 0;
}
