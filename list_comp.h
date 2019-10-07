#ifndef LISTCOMP_LIST_COMPREHENSION_H
#define LISTCOMP_LIST_COMPREHENSION_H

#include<type_traits>
#include<vector>
#include<functional>
#include<algorithm>
#include<initializer_list>
#include<iterator>

#ifndef LISTCOMP_DISABLE_STD_CONTAINERS
#include<deque>
#include<list>
#include<forward_list>
#endif

#ifndef LISTCOMP_DISABLE_OR_AND
#define _or ||
#define _and &&
#define _not !
#endif

namespace listcomp{

class placeholder;
template<auto> class trans;

template<auto P>
class pred{
    public:
        pred(placeholder &_ph) {};
        pred(pred &) = delete;
        pred(pred &&other) = default;
        pred() = delete;
        pred &operator=(pred &) = delete;
        pred &operator=(pred &&) = delete;
};

namespace impl{

template<typename T>
using PredFunctor = std::function<bool(const T&)>;

template<typename InT, typename OutT>
using ElseFunctor = std::function<OutT(InT)>;

template <typename InT, typename OutT>
using TransFunctor = std::function<OutT(InT)>;

#define ADD_LIST_COMP_OPERATOR(TemplateClass,Typetag)\
operator TemplateClass<Typetag> () {\
    return TemplateClass<Typetag>(begin(), end());\
}\
\
template<typename TT, typename=std::void_t<decltype(TT(std::declval<Typetag>()))>>\
operator TemplateClass<TT> () {\
    return TemplateClass<TT>(begin(), end());\
}\

template<template<typename...> typename Cont, typename T, typename=void>
struct is_cont_impl : std::false_type{
};

template<template<typename...> typename Cont, typename T>
struct is_cont_impl<Cont, T, std::void_t<decltype(*(std::declval<Cont<T>>().begin())), decltype(*(std::declval<Cont<T>>().end()))>> :
    std::true_type{
};

template <template <typename...> typename Cont, typename T>
constexpr bool is_cont_v = is_cont_impl<Cont, T>::value;

template <typename InT, typename OutT>
class implicit_convertable;

template<typename InT, typename OutT>
class iterator : public std::iterator<std::forward_iterator_tag, OutT> {
    using Iter_t = typename std::vector<InT>::iterator;
    private:
        Iter_t iter;
        Iter_t end;
        implicit_convertable<InT,OutT> *enclosing;

        void get_next(){
            if(enclosing->hasPred && !enclosing->hasElse){
                auto predFunctor = enclosing->hasPred.value();
                while (iter != end && !predFunctor(*iter)){
                    iter++;
                }
            }
        }

        OutT get_val(){
            if(!(enclosing->hasTrans) && !(enclosing->hasElse)){
                return *iter;
            }
            else if (enclosing->hasTrans && !(enclosing->hasElse)) {
                auto transFunctor = enclosing->hasTrans.value();
                return transFunctor(*iter);
            } 
            else if(enclosing->hasTrans && enclosing->hasElse){
                auto transFunctor = enclosing->hasTrans.value();
                auto predFunctor = enclosing->hasPred.value();
                auto elseFunctor = enclosing->hasElse.value();
                if(predFunctor(*iter)){
                    return transFunctor(*iter);
                } 
                else {
                    return elseFunctor(*iter);
                }
            } 
            else if(!(enclosing->hasTrans) && enclosing->hasElse){
                auto predFunctor = enclosing->hasPred.value();
                auto elseFunctor = enclosing->hasElse.value();
                if(predFunctor(*iter)){
                    return *iter;
                } 
                else {
                    return elseFunctor(*iter);
                }
            }
            return *iter;
        }

    public:
        iterator &operator=(const iterator &other) = default;
        iterator(const iterator &other) = default;
        iterator(const Iter_t &_iter, const Iter_t &_end, implicit_convertable<InT,OutT>* _enclosing) : 
        iter{_iter}, end{_end}, enclosing{_enclosing} {
            get_next();
        };

        OutT operator*(){
            return get_val();
        }

        iterator &operator++(){
            (*this) = iterator(++iter, end, enclosing);
            return *this;
        }

        iterator operator++(int){
            iterator res(*this);
            ++(*this);
            return res;
        }
        iterator &operator--(){
            (*this) = iterator(--iter, end, enclosing);
            return *this;
        }

        iterator operator--(int){
            iterator res(*this);
            --(*this);
            return res;
        }

        bool operator==(const iterator& other){
            return iter == other.iter;
        }

        bool operator!=(const iterator& other){
            return iter != other.iter;
        }
};

struct PredFlag{
} pred_flag;

struct ElseFlag{
} else_flag;

struct TransFlag{
} trans_flag;

#ifdef LISTCOMP_CONVERTABLES
template<typename InT, typename OutT, template<typename...> typename... Ts>
struct impl_oper : public impl_oper<InT,OutT,Ts>... {

};

template<typename InT, typename OutT, template<typename...> typename T>
struct impl_oper<InT,OutT,T> {
    virtual iterator<InT,OutT> begin() = 0;
    virtual iterator<InT,OutT> end() = 0;

    ADD_LIST_COMP_OPERATOR(T, OutT);
};

template<typename InT, typename OutT>
class implicit_convertable : public impl_oper<InT, OutT, LISTCOMP_CONVERTABLES>{
#else
template<typename InT, typename OutT>
class implicit_convertable{
#endif
    private:
        std::vector<InT> vect;
        std::optional<PredFunctor<OutT>> hasPred = std::nullopt;
        std::optional<ElseFunctor<InT,OutT>> hasElse = std::nullopt;
        std::optional<TransFunctor<InT,OutT>> hasTrans = std::nullopt;

        implicit_convertable() : vect{} {};

        template<typename,typename> friend class iterator;

    protected:
        auto &vec(){
            return vect;
        }

    public:
        implicit_convertable(implicit_convertable<InT,OutT>&& other, PredFunctor<OutT>&& predFunc, PredFlag&) : 
            vect(other.vect), hasTrans{other.hasTrans}, hasPred{predFunc} {};
        
        implicit_convertable(implicit_convertable<InT,OutT>&& other, ElseFunctor<InT,OutT>&& elseFunc, ElseFlag&) : 
            vect(other.vect), hasTrans{other.hasTrans}, hasPred{other.hasPred}, hasElse{elseFunc} {};

        template <typename TT>
        implicit_convertable(const TT &begin, const TT &end) : vect(begin, end) {};

        implicit_convertable(std::vector<InT> &&_vec) : vect(std::move(_vec)) {};

        template <typename TT>
        implicit_convertable(const TT &begin, const TT &end, TransFunctor<InT,OutT>&& trans) : vect(begin, end) , hasTrans{trans} {};

        implicit_convertable(std::vector<InT> &&_vec, TransFunctor<InT,OutT>&& trans) : vect(std::move(_vec)), hasTrans{trans} {};

        iterator<InT,OutT> begin() {
            return iterator<InT,OutT>(vec().begin(),vec().end(),this);
        }

        iterator<InT,OutT> end() {
            return iterator<InT,OutT>(vec().end(),vec().end(),this);
        }

        ADD_LIST_COMP_OPERATOR(std::vector, OutT);

#ifndef LISTCOMP_DISABLE_STD_CONTAINERS
        ADD_LIST_COMP_OPERATOR(std::list, OutT);

        ADD_LIST_COMP_OPERATOR(std::deque, OutT);

        ADD_LIST_COMP_OPERATOR(std::forward_list, OutT);
#endif
};

template<typename InT, typename OutT>
class else_impl : public implicit_convertable<InT,OutT>{
    public:
        using implicit_convertable<InT,OutT>::implicit_convertable;
};

enum class oper_flag{
    mult,div,add,sub,mod,rmult,rdiv,radd,rsub,rmod
};

template<typename InT>
class proxy_trans{
    private:
        ElseFunctor<InT,InT> elseFunc;

        template<typename TT> friend class proxy_trans;

    public:
        auto get_else() const {
            return elseFunc;
        }

        proxy_trans(oper_flag flag, const InT& value){
            switch(flag){
                case oper_flag::mult:
                    elseFunc = [&](auto arg) { return arg * value; };
                    break;
                case oper_flag::div:
                    elseFunc = [&](const auto& arg) { return arg / value; };
                    break;
                case oper_flag::add:
                    elseFunc = [&](const auto& arg) { return arg + value; };
                    break;
                case oper_flag::sub:
                    elseFunc = [&](const auto& arg) { return arg - value; };
                    break;
                case oper_flag::mod:
                    elseFunc = [&](const auto& arg) { return arg % value; };
                    break;
                case oper_flag::rmult:
                    elseFunc = [&](const auto& arg) { return value * arg; };
                    break;
                case oper_flag::rdiv:
                    elseFunc = [&](const auto& arg) { return value / arg; };
                    break;
                case oper_flag::radd:
                    elseFunc = [&](const auto& arg) { return value + arg; };
                    break;
                case oper_flag::rsub:
                    elseFunc = [&](const auto& arg) { return value - arg; };
                    break;
                case oper_flag::rmod:
                    elseFunc = [&](const auto& arg) { return value % arg; };
                    break;
            }
        } 
};

template<typename InT, typename OutT>
class if_impl : public implicit_convertable<InT,OutT>{
    public:
        using implicit_convertable<InT,OutT>::implicit_convertable;

        template<auto F>
        else_impl<InT,OutT> _else(trans<F>&&){
            ElseFunctor<InT,OutT> elseFunctor = F;
            return else_impl<InT,OutT>(std::move(*this), std::move(elseFunctor),else_flag);
        }

        else_impl<InT,OutT> _else(placeholder&){
            ElseFunctor<InT,OutT> elseFunctor = [&](const auto &arg) { return arg; };
            return else_impl<InT,OutT>(std::move(*this), std::move(elseFunctor),else_flag);
        }

        else_impl<InT,OutT> _else(const InT& val){
            ElseFunctor<InT,OutT> elseFunctor = [&] (const auto& arg) { return val; };
            return else_impl<InT,OutT>(std::move(*this), std::move(elseFunctor),else_flag);
        }

        else_impl<InT,OutT> _else(proxy_trans<InT>&& proxy){
            return else_impl<InT,OutT>(std::move(*this), proxy.get_else(),else_flag);
        }
};

enum class bool_flag{
    equals, nequals, lthan, gthan, lthaneq, gthaneq, requals, rnequals, rlthan, rgthan, rlthaneq, rgthaneq
};

template<typename T>
class proxy_bool{
    private:
        proxy_bool() = delete;
        PredFunctor<T> predFunc;

        template<typename TT> friend class proxy_bool;

    public:
        auto get_pred() const {
            return predFunc;
        }

        proxy_bool(bool_flag flag, const T& value){
            switch(flag){
                case bool_flag::equals:
                    predFunc = [&](auto arg) { return arg == value; };
                    break;
                case bool_flag::nequals:
                    predFunc = [&](auto arg) { return arg != value; };
                    break;
                case bool_flag::lthan:
                    predFunc = [&](auto arg) { return arg < value; };
                    break;
                case bool_flag::gthan:
                    predFunc = [&](auto arg) { return arg > value; };
                    break;
                case bool_flag::lthaneq:
                    predFunc = [&](auto arg) { return arg <= value; };
                    break;
                case bool_flag::gthaneq:
                    predFunc = [&](auto arg) { return arg >= value; };
                    break;
                case bool_flag::requals:
                    predFunc = [&](auto arg) { return value == arg; };
                    break;
                case bool_flag::rnequals:
                    predFunc = [&](auto arg) { return value != arg; };
                    break;
                case bool_flag::rlthan:
                    predFunc = [&](auto arg) { return value < arg; };
                    break;
                case bool_flag::rgthan:
                    predFunc = [&](auto arg) { return value > arg; };
                    break;
                case bool_flag::rlthaneq:
                    predFunc = [&](auto arg) { return value <= arg; };
                    break;
                case bool_flag::rgthaneq:
                    predFunc = [&](auto arg) { return value >= arg; };
                    break;
            }
        }

        proxy_bool(const PredFunctor<T> &_predFunc) : predFunc{_predFunc} {};

        template<typename TT>
        proxy_bool operator&&(const proxy_bool<TT>& other){
            return PredFunctor<T>{ [=](const auto& arg) { return predFunc(arg) && other.predFunc(arg); } };
        }

        template<auto P>
        proxy_bool operator&&(const pred<P>&){
            return PredFunctor<T>{[=](const auto &arg) { return P(arg) && predFunc(arg); }};
        }

        template<typename TT>
        proxy_bool operator||(const proxy_bool<TT>& other){
            return PredFunctor<T>{ [=](const auto& arg) { return predFunc(arg) || other.predFunc(arg); } };
        }

        template<auto P>
        proxy_bool operator||(const pred<P>&){
            return PredFunctor<T>{[=](const auto &arg) { return predFunc(arg) || P(arg); }};
        }

        proxy_bool operator!(){
            return PredFunctor<T>{[=](const auto &arg) { return !predFunc(arg); }};
        }
};

class not_proxy_bool_flag{
};

template<typename InT, typename OutT>
class in_impl : public implicit_convertable<InT,OutT>{
    public:
        using implicit_convertable<InT,OutT>::implicit_convertable;

        template<auto P>
        if_impl<InT,OutT> _if(pred<P>&&){
            PredFunctor<InT> predFunctor = P;
            return if_impl<InT,OutT>(std::move(*this), std::move(predFunctor),pred_flag);
        }

        if_impl<InT,OutT> _if(placeholder&) {
            PredFunctor<InT> predFunctor = [] (const auto& val) ->bool { return val; };
            return if_impl<InT,OutT>(std::move(*this), std::move(predFunctor),pred_flag);
        }

        template<typename TT>
        if_impl<InT,OutT> _if(proxy_bool<TT> &&proxy){
            PredFunctor<InT> pred = [&](auto arg) { return proxy.get_pred()(arg); };
            return if_impl<InT,OutT>(std::move(*this), std::move(pred),pred_flag);
        }

        if_impl<InT,OutT> _if(proxy_bool<InT> &&proxy){
            return if_impl<InT,OutT>(std::move(*this), proxy.get_pred(),pred_flag);
        }

        if_impl<InT,OutT> _if(not_proxy_bool_flag&&){
            PredFunctor<InT> pred = [&](const auto &arg) ->bool { return !arg; };
            return if_impl<InT,OutT>(std::move(*this), std::move(pred),pred_flag);
        }
};

template<auto F>
class for_impl{
    public:
        for_impl() = default;

        template <template<typename> typename Cont, typename T>
        auto _in(const Cont<T> &container){
            static_assert(is_cont_v<Cont,T>, "argument to _in is not a container type");
            using OutT = decltype(F(std::declval<T>()));
            TransFunctor<T, OutT> trans = F;
            return in_impl<T, OutT>(container.begin(), container.end(), std::move(trans));
        }

        template <typename T>
        auto _in(std::initializer_list<T> &&container){
            using OutT = decltype(F(std::declval<T>()));
            TransFunctor<T, OutT> trans = F;
            return in_impl<T,OutT>(std::vector(container), std::move(trans));
        }
};

template<>
class for_impl<0>{
    public:
        for_impl() = default;

        template <template<typename> typename Cont, typename T>
        auto _in(const Cont<T> &container){
            static_assert(is_cont_v<Cont,T>, "argument to _in is not a container type");
            return in_impl<T,T>(container.begin(), container.end());
        }

        template <typename T>
        auto _in(std::initializer_list<T> &&container){
            return in_impl<T,T>(std::vector<T>(container));
        }
};

template<typename T>
struct _range {
    T limit;
    T init;
    T jump;

    _range() = delete;
    _range(T val) : init{0}, limit{val}, jump{1} {};
    _range(T in, T end) : init{in}, limit{end}, jump{1} {};
    _range(T in, T end, T j) : init{in}, limit{end}, jump{j} {};

    struct iter : public std::iterator<std::forward_iterator_tag,T> {
        T value;
        T jump;

        iter(T val, T j) : value{val}, jump{j} {};

        T operator*() { return value; }

        iter &operator++() { value+=jump;
            return *this;
        }

        iter operator++(int) { value+=jump;
            return iter{value - jump, jump};
        }

        iter &operator--() { value-=jump;
            return *this;
        }

        iter operator--(int) { value-=jump;
            return iter{value + jump, jump};
        }

        bool operator==(const iter& other) const {
            if(jump>0){
                return value >= other.value;
            }
            return value <= other.value;
        }

        bool operator!=(const iter& other) const {
            if(jump>0){
                return value < other.value;
            }
                return value > other.value;
        }
    };

    auto begin() const { return iter{init, jump}; }
    auto end() const { return iter{limit, jump}; }
};

} //namespace impl

class placeholder{
    private:
        const int id;

        inline static char inst_cnt = 0;

        placeholder(int i) : id{i} { inst_cnt++; }
        placeholder(placeholder&& other) : id{other.id} {};

        template <auto F> friend class trans;

    public:
        placeholder() : id{inst_cnt} { inst_cnt++; }
        placeholder(placeholder &) : placeholder{} {};
        placeholder &operator=(placeholder &) = delete;
        placeholder &operator=(placeholder &&) = delete;

        const int get_id() const {
            return id;
        }

        placeholder operator,(const placeholder &other){
            return std::move(placeholder{id+other.id});
        }

        impl::for_impl<0> _for(placeholder&){
            return impl::for_impl<0>{};
        }

        template<typename T>
        impl::proxy_bool<T> operator==(T&& value){
            return impl::proxy_bool<T>(impl::bool_flag::equals, std::forward<T>(value));
        }

        template<typename T>
        impl::proxy_bool<T> operator!=(T&& value){
            return impl::proxy_bool<T>(impl::bool_flag::nequals, std::forward<T>(value));
        }

        template<typename T>
        impl::proxy_bool<T> operator<(T&& value){
            return impl::proxy_bool<T>(impl::bool_flag::lthan, std::forward<T>(value));
        }

        template<typename T>
        impl::proxy_bool<T> operator>(T&& value){
            return impl::proxy_bool<T>(impl::bool_flag::gthan, std::forward<T>(value));
        }

        template<typename T>
        impl::proxy_bool<T> operator<=(T&& value){
            return impl::proxy_bool<T>(impl::bool_flag::lthaneq, std::forward<T>(value));
        }

        template<typename T>
        impl::proxy_bool<T> operator>=(T&& value){
            return impl::proxy_bool<T>(impl::bool_flag::gthaneq, std::forward<T>(value));
        }

        template<typename T>
        friend impl::proxy_bool<T> operator==(T&& value, placeholder&){
            return impl::proxy_bool<T>(impl::bool_flag::requals, std::forward<T>(value));
        }

        template<typename T>
        friend impl::proxy_bool<T> operator!=(T&& value, placeholder&){
            return impl::proxy_bool<T>(impl::bool_flag::rnequals, std::forward<T>(value));
        }

        template<typename T>
        friend impl::proxy_bool<T> operator<(T&& value, placeholder&){
            return impl::proxy_bool<T>(impl::bool_flag::rlthan, std::forward<T>(value));
        }

        template<typename T>
        friend impl::proxy_bool<T> operator>(T&& value, placeholder&){
            return impl::proxy_bool<T>(impl::bool_flag::rgthan, std::forward<T>(value));
        }

        template<typename T>
        friend impl::proxy_bool<T> operator<=(T&& value, placeholder&){
            return impl::proxy_bool<T>(impl::bool_flag::rlthaneq, std::forward<T>(value));
        }

        template<typename T>
        friend impl::proxy_bool<T> operator>=(T&& value, placeholder&){
            return impl::proxy_bool<T>(impl::bool_flag::rgthaneq, std::forward<T>(value));
        }

        impl::not_proxy_bool_flag operator!(){
            return impl::not_proxy_bool_flag{};
        }

        template<typename T>
        impl::proxy_trans<T> operator*(T&& value){
            return impl::proxy_trans<T>(impl::oper_flag::mult, std::forward<T>(value));
        }

        template<typename T>
        impl::proxy_trans<T> operator/(T&& value){
            return impl::proxy_trans<T>(impl::oper_flag::div, std::forward<T>(value));
        }

        template<typename T>
        impl::proxy_trans<T> operator+(T&& value){
            return impl::proxy_trans<T>(impl::oper_flag::add, std::forward<T>(value));
        }
        
        template<typename T>
        impl::proxy_trans<T> operator-(T&& value){
            return impl::proxy_trans<T>(impl::oper_flag::sub, std::forward<T>(value));
        }

        template<typename T>
        impl::proxy_trans<T> operator%(T&& value){
            return impl::proxy_trans<T>(impl::oper_flag::mod, std::forward<T>(value));
        }

        template<typename T>
        friend impl::proxy_trans<T> operator*(T&& value, placeholder&){
            return impl::proxy_trans<T>(impl::oper_flag::rmult, std::forward<T>(value));
        }

        template<typename T>
        friend impl::proxy_trans<T> operator/(T&& value, placeholder&){
            return impl::proxy_trans<T>(impl::oper_flag::rdiv, std::forward<T>(value));
        }

        template<typename T>
        friend impl::proxy_trans<T> operator+(T&& value, placeholder&){
            return impl::proxy_trans<T>(impl::oper_flag::radd, std::forward<T>(value));
        }
        
        template<typename T>
        friend impl::proxy_trans<T> operator-(T&& value, placeholder&){
            return impl::proxy_trans<T>(impl::oper_flag::rsub, std::forward<T>(value));
        }

        template<typename T>
        friend impl::proxy_trans<T> operator%(T&& value, placeholder&){
            return impl::proxy_trans<T>(impl::oper_flag::rmod, std::forward<T>(value));
        }

}_i,_j,_k;

template<auto F>
class trans{
    using Func=decltype(F);
    private:
        placeholder &ph;
        Func functor{F};

    public:
        trans(placeholder &_ph) : ph{_ph} {};
        trans() = delete;
        trans(trans &) = delete;
        trans(trans &&) = delete;
        trans &operator=(trans &) = delete;
        trans &operator=(trans &&) = delete;

        impl::for_impl<F> _for(placeholder&) {
            return impl::for_impl<F>{};
        }
};

template<typename T, typename=std::enable_if_t<std::is_arithmetic_v<T>>>
impl::_range<T> _range(T start, T end, T jump=1){
    return impl::_range<T>{start, end, jump};
}

template<typename T, typename=std::enable_if_t<std::is_arithmetic_v<T>>>
impl::_range<T> _range(T end){
    return impl::_range<T>{end};
}

} //namespace listcomp

#endif