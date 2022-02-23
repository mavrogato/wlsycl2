#ifndef INCLUDE_WAYLAND_CLIENT_HELPER_HPP_4A10701A_4A55_406C_8525_E20926D97E1C
#define INCLUDE_WAYLAND_CLIENT_HELPER_HPP_4A10701A_4A55_406C_8525_E20926D97E1C

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
INTERN_WL_INTERFACE(wl_output);

template <class T>
concept wl_client_t = std::same_as<decltype (wl_interface_ref<T>), wl_interface const&>;

template <wl_client_t T, class Ch>
decltype (auto) operator << (std::basic_ostream<Ch>& output, T const* ptr) {
    return output << static_cast<void const*>(ptr)
                  << '['
                  << wl_interface_ref<T>.name
                  << ']';
}

template <wl_client_t T>
[[nodiscard]] auto attach_unique(T* ptr) noexcept {
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
            if (!global) {
                global.reset(reinterpret_cast<type*>(wl_registry_bind(registry,
                                                                      name,
                                                                      &interface_ref,
                                                                      version)));
                return ;
            }
        }
        register_global_callback<N+1, Args...>(data, registry, name, interface, version);
    }
}

template <wl_client_t... Args>
[[nodiscard]] auto register_global(wl_display* display) noexcept {
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

#endif/*INCLUDE_WAYLAND_CLIENT_HELPER_HPP_4A10701A_4A55_406C_8525_E20926D97E1C*/
