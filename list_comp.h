#ifndef LISTCOMP_LIST_COMPREHENSION_H
#define LISTCOMP_LIST_COMPREHENSION_H

#include<iostream>
#include<type_traits>
#include<cassert>
#include<vector>
#include<functional>
#include<algorithm>
#include<initializer_list>
#include<variant>
#include<iterator>

#ifndef LISTCOMP_DISABLE_STD_CONTAINERS
#include<deque>
#include<list>
#include<forward_list>
#endif

#ifndef LISTCOMP_DISABLE_OR_AND
#define _or ||
#define _and &&
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

template<typename T>
using ElseFunctor = std::function<T(T)>;

template <typename RT, typename T>
using TransFunctor = std::function<RT(T)>;

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

template <typename OutT>
class implicit_convertable;

template<typename OutT>
class iterator : public std::iterator<std::forward_iterator_tag, OutT> {
    using Iter_t = typename std::vector<OutT>::iterator;
    private:
        Iter_t iter;
        Iter_t end;
        implicit_convertable<OutT> *enclosing;

        void get_next(){
            if(enclosing->hasPred && !enclosing->hasElse){
                auto predFunctor = enclosing->hasPred.value();
                while (iter != end && !predFunctor(*iter)){
                    iter++;
                }
            }
        }

        void update_val(){
            if(enclosing->hasPred && enclosing->hasElse){
                auto elseFunctor = enclosing->hasElse.value();
                auto predFunctor = enclosing->hasPred.value();
                if(!predFunctor(*iter)){
                    *iter = elseFunctor(*iter);
                }
            }
        }

    public:
        iterator &operator=(const iterator &other) = default;
        iterator(const iterator &other) = default;
        iterator(const Iter_t &_iter, const Iter_t &_end, implicit_convertable<OutT>* _enclosing) : 
        iter{_iter}, end{_end}, enclosing{_enclosing} {
            get_next();
            update_val();
        };

        OutT &operator*(){
            return *iter;
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

enum class func_flag{
    npred
};

#ifdef LISTCOMP_CONVERTABLES
template<typename UT, template<typename...> typename... Ts>
struct impl_oper : public impl_oper<UT,Ts>... {

};

template<typename UT, template<typename...> typename T>
struct impl_oper<UT,T> {
    virtual iterator<UT> begin() = 0;
    virtual iterator<UT> end() = 0;

    ADD_LIST_COMP_OPERATOR(T, UT);
};

template<typename OutT>
class implicit_convertable : public impl_oper<OutT, LISTCOMP_CONVERTABLES>{
#else
template<typename OutT>
class implicit_convertable{
#endif
    private:
        std::vector<OutT> vect;
        std::optional<PredFunctor<OutT>> hasPred = std::nullopt;
        std::optional<ElseFunctor<OutT>> hasElse = std::nullopt;
        std::optional<TransFunctor<OutT, OutT>> hasTrans = std::nullopt;
        bool filterDone = false;

        implicit_convertable() : vect{} {};

        template<typename> friend class iterator;

    protected:
        auto &vec(){
            return vect;
        }

    public:
        implicit_convertable(implicit_convertable<OutT>&& other, func_flag spec) : vect(other.vect) {
            switch(spec){
                case func_flag::npred:
                    hasPred=std::nullopt;
            }
        }

        implicit_convertable(implicit_convertable<OutT>&& other, PredFunctor<OutT>&& predFunc) : 
            vect(other.vect), hasPred{predFunc} {};
        
        implicit_convertable(implicit_convertable<OutT>&& other, ElseFunctor<OutT>&& elseFunc) : 
            vect(other.vect), hasPred{other.hasPred}, hasElse{elseFunc} {};

        template <typename TT>
        implicit_convertable(const TT &begin, const TT &end) : vect(begin, end){};

        implicit_convertable(std::vector<OutT> &&_vec) : vect(std::move(_vec)){};

        iterator<OutT> begin() {
            return iterator<OutT>(vec().begin(),vec().end(),this);
        }

        iterator<OutT> end() {
            return iterator<OutT>(vec().end(),vec().end(),this);
        }

        operator OutT*(){
            return &vec()[0];
        }

        template<typename TT, typename=std::void_t<decltype(TT(std::declval<OutT>()))>>
        operator TT*() {
            return &vec()[0];
        }

        ADD_LIST_COMP_OPERATOR(std::vector, OutT);

#ifndef LISTCOMP_DISABLE_STD_CONTAINERS
        ADD_LIST_COMP_OPERATOR(std::list, OutT);

        ADD_LIST_COMP_OPERATOR(std::deque, OutT);

        ADD_LIST_COMP_OPERATOR(std::forward_list, OutT);
#endif
};

template<typename T>
class else_impl : public implicit_convertable<T>{
    public:
        using implicit_convertable<T>::implicit_convertable;
};

template<typename T>
class if_impl : public implicit_convertable<T>{
    public:
        using implicit_convertable<T>::implicit_convertable;

        template<auto F>
        else_impl<T> _else(trans<F>&& transFunc){
            ElseFunctor<T> elseFunctor = F;
            return else_impl<T>(std::move(*this), std::move(elseFunctor));
        }

        else_impl<T> _else(placeholder&){
            return else_impl<T>(std::move(*this), func_flag::npred);
        }

        else_impl<T> _else(const T& val){
            ElseFunctor<T> elseFunctor = [&] (const auto& arg) { return val; };
            return else_impl<T>(std::move(*this), std::move(elseFunctor));
        }
};

enum class bool_flag{
    equals, nequals, lthan, gthan, lthaneq, gthaneq
};

enum class oper_flag{
    mult,div,add,sub,mod
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
            switch(spec){
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
            }
        }

        proxy_bool(const PredFunctor<T> &_predFunc) : predFunc{_predFunc} {};

        template <typename TT>
        proxy_bool(const proxy_bool<T> &&other) : predFunc{other.predFunc} {};

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
};

template<typename T>
class in_impl : public implicit_convertable<T>{
    public:
        using implicit_convertable<T>::implicit_convertable;

        template<auto P>
        if_impl<T> _if(pred<P>&& predicate){
            PredFunctor<T> predFunctor = P;
            return if_impl<T>(std::move(*this), std::move(predFunctor));
        }

        if_impl<T> _if(placeholder&) {
            PredFunctor<T> predFunctor = [](const auto& val) { return true; };
            return if_impl<T>(std::move(*this), std::move(predFunctor));
        }

        template<typename TT>
        if_impl<T> _if(proxy_bool<TT> &&proxy){
            PredFunctor<T> pred = [&](auto arg) { return proxy.get_pred()(arg); };
            return if_impl<T>(std::move(*this), std::move(pred));
        }

        if_impl<T> _if(proxy_bool<T> &&proxy){
            return if_impl<T>(std::move(*this), proxy.get_pred());
        }
};

template<auto F>
class for_impl{
    public:
        for_impl() = default;

        template <template<typename> typename Cont, typename T>
        auto _in(const Cont<T> &container){
            static_assert(is_cont_v<Cont,T>, "argument to _in is not a container type");
            using TT = decltype(F(std::declval<T>()));
            std::vector<TT> vec;
            for(const auto &i:container){
                vec.push_back(F(i));
            }
            return in_impl<TT>(std::move(vec));
        }

        template <typename T>
        auto _in(std::initializer_list<T> &&container){
            using TT = decltype(F(std::declval<T>()));
            std::vector<TT> vec;
            for(const auto &i:container){
                vec.push_back(F(i));
            }
            return in_impl<TT>(std::move(vec));
        }
};

template<>
class for_impl<0>{
    public:
        for_impl() = default;

        template <template<typename> typename Cont, typename T>
        auto _in(const Cont<T> &container){
            static_assert(is_cont_v<Cont,T>, "argument to _in is not a container type");
            return in_impl<T>(container.begin(), container.end());
        }

        template <typename T>
        auto _in(std::initializer_list<T> &&container){
            return in_impl<T>(std::vector<T>(container));
        }
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

placeholder new_placeholder(){
    return placeholder{};
}

} //namespace listcomp

#endif