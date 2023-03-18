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
        template<typename Callable, typename T, typename... Ts>
        struct has_same_return_type { 
            using return_type = std::invoke_result_t<Callable, T>;

            static constexpr auto value = (std::is_same_v<std::invoke_result_t<Callable, std::add_lvalue_reference_t<T>>, std::invoke_result_t<Callable, std::add_lvalue_reference_t<Ts>>> && ...);
        };

        template<typename Callable, typename... Ts>
        inline constexpr bool has_same_return_type_v = has_same_return_type<Callable, Ts...>::value;

        template<typename Callable, typename... Ts>
        using common_return_type_t = typename has_same_return_type<Callable, Ts...>::return_type;
        /****************************************************************/

        template<typename Callable, typename... Ts>
        concept SameReturnTypeCallables = has_same_return_type_v<Callable, Ts...>;
 
        /* Internal function to find the correct type contained in the variant */
        /****************************************************************/
        /*
        template<std::size_t I, typename F, typename... VarTypes>
        static decltype(auto) call_impl(F f, std::variant<VarTypes...>& v) noexcept(std::is_nothrow_invocable_v<F, std::add_lvalue_reference_t<std::variant_alternative_t<I, std::variant<VarTypes...>>>>) {
            return f(std::get<I>(v));
        } 
        */       

        template<typename Callable, typename... VarTypes, std::size_t... Is>
        static constexpr decltype(auto) visit(Callable&& f, std::variant<VarTypes...>& v, std::index_sequence<Is...>)
            noexcept((std::is_nothrow_invocable_v<Callable, std::add_lvalue_reference_t<VarTypes>> && ...))
        {
            using ForwardedType = decltype(std::forward<Callable>(f));
            using LambdaType = common_return_type_t<Callable, VarTypes...>(*)(ForwardedType, std::variant<VarTypes...>&);

            constexpr std::array<LambdaType, sizeof...(VarTypes)> funcs = { 
                +[](ForwardedType f, std::variant<VarTypes...>& var) {
                    return f(std::get<Is>(var));
                }...
            };

            return funcs[v.index()](std::forward<Callable>(f), v);
        }
        /****************************************************************/

   }

    /* The actual visit function */
    template<typename Callable, typename... VarTypes>
    requires detail::SameReturnTypeCallables<Callable, VarTypes...>
    constexpr decltype(auto) visit(Callable&& func, std::variant<VarTypes...>& v) noexcept((std::is_nothrow_invocable_v<Callable, std::add_lvalue_reference_t<VarTypes>> && ...)) {
         return detail::visit(std::forward<Callable>(func), v, std::make_index_sequence<sizeof...(VarTypes)>{});
    }
}

template<typename... Ts>
struct visitor : Ts... { using Ts::operator()...; };
template<typename... Ts>
visitor(Ts...) -> visitor<Ts...>;

int main() {
    std::cout << "ice::visit\n\n";

    std::variant<int, float, double, std::string> v{};

    auto vis1 = visitor {
        [](int const& s) {
            std::cout << "Int: " << s << "\n";
            return std::string{ "int" };
        }, 
        [](float const& s) {
            std::cout << "Float: " << s << "\n";
            return std::string{ "float" };
        },
        [](double& s) {
            std::cout << "Double: " << s << "\n";
            s *= 2;
            return std::string{ "double" };
        },
        [](const std::string& s) {
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

    return 0;
}