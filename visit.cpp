#include <iostream>
#include <variant>
#include <string>
#include <tuple>
#include <array>

namespace ice {
    namespace detail {

        /****************************************************************/
        /* Type trait to check if callables have the same return type and */
        /* to get that return type                                        */
        /****************************************************************/
        template<typename Callable, typename T>
        struct has_same_return_type;

        template<typename Callable, typename... Ts>
        struct has_same_return_type<Callable, std::variant<Ts...>> { 
            using return_type = std::invoke_result_t<Callable, std::tuple_element_t<0, std::tuple<Ts...>>>;

            static constexpr auto value = (std::is_same_v<std::invoke_result_t<Callable, std::add_lvalue_reference_t<return_type>>, std::invoke_result_t<Callable, std::add_lvalue_reference_t<Ts>>> && ...);
        };

        template<typename Callable, typename T>
        static inline constexpr auto has_same_return_type_v = has_same_return_type<Callable, T>::value;

        template<typename Callable, typename T>
        using common_return_type_t = typename has_same_return_type<Callable, T>::return_type;
        /****************************************************************/

        template<typename Callable, typename T>
        concept SameReturnTypeCallables = has_same_return_type_v<Callable, T>;

        template<typename T>
        struct param_counter;

        template<typename... Args>
        struct param_counter<std::variant<Args...>> {
            static constexpr auto value = sizeof...(Args);
        };

        template<typename T>
        static inline constexpr auto param_counter_v = param_counter<T>::value;


        template<typename Callable, typename T>
        struct are_all_nothrow_invocable;

        template<typename Callable, typename... Ts>
        struct are_all_nothrow_invocable<Callable, std::variant<Ts...>> { 
            static constexpr auto value = (std::is_nothrow_invocable_v<Callable, std::add_lvalue_reference_t<Ts>> && ...);
        };

        template<typename Callable, typename T>
        static inline constexpr auto are_all_nothrow_invocable_v = are_all_nothrow_invocable<Callable, T>::value;

        /* Internal function to find the correct type contained in the variant */
        /****************************************************************/
        /*
        template<std::size_t I, typename F, typename... VarTypes>
        static decltype(auto) call_impl(F f, std::variant<VarTypes...>& v) noexcept(std::is_nothrow_invocable_v<F, std::add_lvalue_reference_t<std::variant_alternative_t<I, std::variant<VarTypes...>>>>) {
            return f(std::get<I>(v));
        } 
        */       

        template<typename Callable, typename Variant, std::size_t... Is, typename PureVariant = std::remove_cvref_t<Variant>>
        static constexpr decltype(auto) visit(Callable&& f, Variant&& v, std::index_sequence<Is...>)
            noexcept(are_all_nothrow_invocable_v<Callable, PureVariant>)
        {
            using ForwardedType = decltype(std::forward<Callable>(f));
            using ForwardVariantType = decltype(std::forward<Variant>(v));
            using LambdaType = common_return_type_t<Callable, PureVariant>(*)(ForwardedType, ForwardVariantType);

            constexpr std::array<LambdaType, param_counter_v<PureVariant>> funcs = { 
                +[](ForwardedType f, ForwardVariantType var) {
                    return std::forward<ForwardedType>(f)(std::get<Is>(std::forward<ForwardVariantType>(var)));
                }...
            };

            return funcs[v.index()](std::forward<Callable>(f), std::forward<ForwardVariantType>(v));
        }
        /****************************************************************/

   }

    /* The actual visit function */
    template<typename Callable, typename Variant, typename PureVariant = std::remove_cvref_t<Variant>>
    requires detail::SameReturnTypeCallables<Callable, PureVariant>
    constexpr decltype(auto) visit(Callable&& func, Variant&& v) 
        noexcept(detail::are_all_nothrow_invocable_v<Callable, PureVariant>)
    {

        return detail::visit(std::forward<Callable>(func), std::forward<Variant>(v), std::make_index_sequence<detail::param_counter_v<PureVariant>>{});
    }

}

template<typename... Ts>
struct visitor : Ts... { using Ts::operator()...; };
template<typename... Ts>
visitor(Ts...) -> visitor<Ts...>;

int main() {
    std::cout << "ice::visit\n\n";

    std::variant<int, float, double, std::string> v{};

    constexpr auto vis1 = visitor {
        [](int s) noexcept {
            std::cout << "Int: " << s << "\n";
            return std::string{ "int" };
        }, 
        [](float s) noexcept {
            std::cout << "Float: " << s << "\n";
            return std::string{ "float" };
        },
        [](double& s) noexcept {
            std::cout << "Double: " << s << "\n";
            s *= 2;
            return std::string{ "double" };
        },
        [](const std::string& s) noexcept {
            std::cout << "String: " << s << "\n";
            return std::string{ "std::string" };
        }
    };

     for (int i = 0; i < 4; ++i) {
 
        switch (i) {
            case 0:
                v = 12;
                break;
            case 1:
                v = 3.2f;
                break;
            case 2:
                v = 10.0;
                break;
            case 3:
                v = "Aloha";
                break;
        }

        auto ret = ice::visit(vis1, v);
        if (i == 2)
            std::cout << "Double is " << std::get<double>(v) << " now\n";
        
        std::cout << "Return: " << ret << '\n';
    }

    constexpr std::variant<int, double> var{1};

    constexpr auto val = ice::visit(visitor{ [](int i) { return i*2; }}, var);

    return 0;
}